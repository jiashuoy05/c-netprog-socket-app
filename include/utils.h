// 這叫做 Header Guard，防止這個檔案被重複引入兩次造成編譯錯誤
#ifndef UTILS_H
#define UTILS_H

// 引入標準定義（為了使用 size_t 這種型別）
#include <stddef.h>

// 宣告一個全域變數（extern 代表這個變數的實體在別的地方，這裡是 utils.c）
extern int debug_level;

// 宣告功能函式：設定 Debug 等級
void set_debug_level(int level);

// 宣告功能函式：印出 Debug 訊息 (類似 printf，... 代表可以傳入不固定數量的參數)
void log_debug(const char *format, ...);

// 宣告功能函式：抓取系統資訊，並存到 buffer 裡面
void get_system_info(char *buffer, size_t size);

#endif
