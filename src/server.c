#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>      // 提供 fork(), close(), read(), write() 等系統呼叫
#include <arpa/inet.h>   // 提供 sockaddr_in 這種網路位址結構
#include <sys/wait.h>    // 提供 waitpid()，用來收屍 (防止殭屍行程)
#include <signal.h>      // 提供訊號處理 signal(), sigaction()
#include <errno.h>       // 提供錯誤代碼 errno
#include "../include/utils.h"

#define PORT 8080        // 伺服器要開在哪個 Port 
#define BUFFER_SIZE 1024 // 讀取資料的緩衝區大小

// [強健性機制 1]：處理子行程死亡的訊號 (SIGCHLD)，防止變殭屍
void sigchld_handler(int s) {
    (void)s; // 避免編譯器警告「未使用的參數」
    int saved_errno = errno; // 備份目前的錯誤碼，以免打斷主程式
    // WNOHANG 代表「非阻塞」。把所有已經結束的子行程收屍，沒人結束就直接離開，不等待。
    while(waitpid(-1, NULL, WNOHANG) > 0);
    errno = saved_errno; // 恢復錯誤碼
}

// 這是 Child Process 專屬的工作內容
void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE] = {0}; // 準備一個乾淨的陣列來收資料
    
    // 從 client_socket 讀取資料
    int valread = read(client_socket, buffer, BUFFER_SIZE);

    if (valread > 0) {
        log_debug("Received request: %s", buffer);
        
        // 檢查客戶端是不是傳來 "GET_SYS_INFO" 這個密語
        if (strncmp(buffer, "GET_SYS_INFO", 12) == 0) {
            char sys_info[BUFFER_SIZE];
            
            // 呼叫 .so 裡的函式抓系統資訊
            get_system_info(sys_info, sizeof(sys_info));
            
            // 把抓到的資訊透過 socket 傳回給客戶端
            send(client_socket, sys_info, strlen(sys_info), 0);
            log_debug("System info sent to client.");
        }
    }
    // 服務完畢，關掉這個客戶端的連線
    close(client_socket);
    // 子行程任務結束，必須呼叫 exit(0) 自殺，否則他會跑出這個函式變成另一個 Server 去搶客人
    exit(0); 
}

int main(int argc, char *argv[]) {
    // 檢查啟動程式時有沒有加 -v (例如：./server -v)
    if (argc > 1 && strcmp(argv[1], "-v") == 0) {
        set_debug_level(1); // 開啟 runtime debug
    }

    int server_fd, new_socket;
    struct sockaddr_in address; // 用來設定 IP 和 Port 的結構
    int opt = 1;
    int addrlen = sizeof(address);

    // [強健性機制 2]：忽略 SIGPIPE 訊號。
    // 如果 Server 傳送資料到一半，Client 突然斷網，作業系統會發送 SIGPIPE 給 Server。
    // 預設行為是「直接崩潰」，我們改成 SIG_IGN (忽略)，讓 Server 繼續活著。
    signal(SIGPIPE, SIG_IGN);

    // [強健性機制 1 的設定]：告訴系統，收到子行程死亡訊號 (SIGCHLD) 時，去執行 sigchld_handler
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    // 步驟一：向系統申請建立一扇門 (Socket)
    // AF_INET 代表 IPv4，SOCK_STREAM 代表 TCP 協定
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // [強健性機制 3]：設定 SO_REUSEADDR。
    // 當 Server 當掉重開時，原本的 Port 通常會被系統鎖定一段時間 (TIME_WAIT)。
    // 加上這行就能馬上重新綁定同一個 Port，不會發生 "Address already in use" 錯誤。
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // 設定要綁定的位址資訊：IPv4, 任意 IP (INADDR_ANY), Port 8080
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT); // htons: 把數字轉成網路標準的位元組順序 (Big-Endian)

    // 步驟二：綁定門牌 (Bind)
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // 步驟三：坐在門口等 (Listen)，參數 20 代表最多允許 20 個人在門外排隊等候連線
    if (listen(server_fd, 20) < 0) { 
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", PORT);

    // 無窮迴圈，Server 永遠不關機
    while (1) {
        // 步驟四：接待客人 (Accept)。程式會卡在這裡睡覺，直到有人連線才會醒來往下走。
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept");
            continue; // 如果接待失敗，就跳過這次，繼續等下一個
        }

        log_debug("New client connected.");

        // 步驟五：影分身之術 (Fork)。呼叫的瞬間，會完整複製出另一個行程。
        pid_t pid = fork();
        
        if (pid < 0) {
            // 複製失敗
            perror("fork failed");
            close(new_socket);
        } else if (pid == 0) {
            // 這裡是「子行程 (Child)」會走的路 (fork 回傳 0 給子行程)
            close(server_fd); // 子行程不需要在門口接客，關掉他手上的 Listen Socket
            handle_client(new_socket); // 專心服務這個新客人
        } else {
            // 這裡是「父行程 (Parent)」會走的路 (fork 回傳子行程的 PID 給父行程)
            close(new_socket); // 父行程把客人交給子行程了，所以關掉他手上的 Client Socket
            // 父行程會順著迴圈回到 while(1) 開頭，繼續卡在 accept() 等下一個客人
        }
    }
    return 0;
}
