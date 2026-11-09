# 設定編譯器為 gcc (GNU C Compiler)
CC = gcc

# CFLAGS: 告訴 gcc 編譯時要加上的參數
# -Wall -Wextra: 開啟所有的警告，幫你抓錯
# -I./include: 告訴編譯器去找標頭檔 (.h) 的資料夾
# -DDEBUG: 定義一個 DEBUG 巨集，用來開啟「編譯期」的 Debug 功能
CFLAGS = -Wall -Wextra -I./include -DDEBUG

# LDFLAGS: 告訴 gcc 連結（Link）動態函式庫時的參數
# -L./lib: 告訴系統去 ./lib 資料夾找函式庫
# -lutils: 連結名為 libutils.so 的檔案
# -Wl,-rpath=./lib: 告訴編譯出來的執行檔，執行時要去 ./lib 找這個 .so 檔，不然會報錯找不到
LDFLAGS = -L./lib -lutils -Wl,-rpath,./lib

# 預設動作，打 make 時會依序執行 setup -> lib -> server -> client
all: setup lib server client

# 建立需要的資料夾（-p 代表如果存在就不報錯）
setup:
	mkdir -p bin lib

# 編譯動態函式庫 (.so)
# -fPIC -shared: 這是產出 Linux 動態函式庫（類似 Windows 的 .dll）的必備參數
lib: src/utils.c
	$(CC) $(CFLAGS) -fPIC -shared src/utils.c -o lib/libutils.so

# 編譯伺服器程式，依賴 src/server.c 和剛編譯好的 lib
server: src/server.c lib
	$(CC) $(CFLAGS) src/server.c -o bin/server $(LDFLAGS)

# 編譯客戶端程式，依賴 src/client.c 和剛編譯好的 lib
client: src/client.c lib
	$(CC) $(CFLAGS) src/client.c -o bin/client $(LDFLAGS)

# 清除編譯產物，打 make clean 時執行
clean:
	rm -rf bin/ lib/
