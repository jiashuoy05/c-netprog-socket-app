#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h> // 提供 inet_pton (把字串 IP 轉換成二進位)
#include "../include/utils.h"

#define PORT 8080
#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    // 檢查啟動時有沒有加 -v
    if (argc > 1 && strcmp(argv[1], "-v") == 0) {
        set_debug_level(1);
    }

    int sock = 0;
    struct sockaddr_in serv_addr;
    char *request = "GET_SYS_INFO"; // 要對 Server 說的密語
    char buffer[BUFFER_SIZE] = {0}; // 用來裝 Server 回覆的陣列

    // 步驟一：建立 Socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return -1;
    }

    // 設定要連線的目標位址 (IPv4, Port 8080)
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // 把 "127.0.0.1" (本機測試用的 Loopback IP) 轉換成網路格式寫入 serv_addr
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("\nInvalid address\n");
        return -1;
    }

    // 步驟二：發起連線 (Connect)
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed \n");
        return -1;
    }

    log_debug("Connected to server.");
    
    // 步驟三：發送請求字串
    send(sock, request, strlen(request), 0);
    log_debug("Request sent.");

    // 步驟四：等待並讀取 Server 的回傳結果
    int valread = read(sock, buffer, BUFFER_SIZE);
    
    if (valread > 0) {
        // 印出抓到的系統資訊
        printf("Server reply: %s\n", buffer);
    }

    // 步驟五：結束連線
    close(sock);
    return 0;
}
