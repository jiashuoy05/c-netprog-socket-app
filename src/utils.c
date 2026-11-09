#include <stdio.h>       // 提供 printf, snprintf 等輸入輸出功能
#include <stdarg.h>      // 提供處理可變參數 (va_list) 的功能
#include <string.h>      // 提供字串處理功能 (strncpy)
#include <sys/utsname.h> // 提供 uname() 函式來抓取 Linux 系統資訊
#include "../include/utils.h" // 載入我們自己寫的菜單

// 實體化全域變數，預設為 0 (不印出 runtime debug 訊息)
int debug_level = 0; 

// 給外部呼叫，用來改變 debug_level 的值
void set_debug_level(int level) {
    debug_level = level;
}

// 我們自己包裝的 Debug 列印函式
void log_debug(const char *format, ...) {
// #ifdef DEBUG 是「編譯期控制」。如果在 Makefile 裡拿掉 -DDEBUG，這裡面的程式碼會被編譯器直接刪掉，完全不佔容量。
#ifdef DEBUG
    // if (debug_level > 0) 是「執行期控制」。程式有啟動（傳入 -v）才會大於 0。
    if (debug_level > 0) {
        va_list args;          // 宣告一個變數來存放所有傳進來的參數
        va_start(args, format); // 開始解析參數
        printf("[DEBUG] ");     // 每行前面加上標籤
        vprintf(format, args);  // 把文字印出來
        printf("\n");           // 換行
        va_end(args);          // 結束解析
    }
#endif
}

// 實作抓取系統資訊的功能
void get_system_info(char *buffer, size_t size) {
    struct utsname sysinfo; // 宣告一個結構體來裝系統資料
    
    // uname() 是一個 System Call，會跟作業系統要硬體和系統資料
    if (uname(&sysinfo) == 0) { // 回傳 0 代表成功
        // 將系統名稱、版本、架構組合成字串，安全的塞進 buffer 裡
        snprintf(buffer, size, "OS: %s %s, Arch: %s", 
                 sysinfo.sysname, sysinfo.release, sysinfo.machine);
    } else {
        // 如果失敗，就把錯誤訊息塞進去
        strncpy(buffer, "Error getting system info", size);
    }
}
