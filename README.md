# c-netprog-socket-app

使用 C 與 POSIX TCP socket 實作的 client-server 期中作業。Server 使用
`fork()` 為每個 client connection 建立獨立的 child process，client 可向 server
請求作業系統資訊。

## 功能

- TCP server 綁定 `0.0.0.0:8080`，使用 `listen()` 等待連線。
- 每個連線由 `fork()` 建立的 child process 處理，非 thread。
- client 連線至 `127.0.0.1:8080`，送出 `GET_SYS_INFO`。
- server 使用 `uname()` 取得作業系統名稱、版本與 CPU architecture，回傳給 client。
- 共用函式封裝於動態函式庫 `lib/libutils.so`，並由 client 與 server 共同連結。
- 支援兩層 debug log 控制：
  - 編譯期：Makefile 的 `-DDEBUG`。
  - 執行期：啟動程式時加入 `-v`。
- Server 包含以下 robustness 機制：
  - `SIGCHLD` handler 使用 `waitpid(..., WNOHANG)` 清理 child process，避免 zombie process。
  - 忽略 `SIGPIPE`，避免 client 意外斷線造成 server 終止。
  - 使用 `SO_REUSEADDR`，讓 server 可較快重新綁定相同 port。

## 專案結構

```text
.
├── Makefile
├── include/
│   └── utils.h              # 共用函式宣告
├── src/
│   ├── server.c             # fork-based TCP server
│   ├── client.c             # TCP client
│   └── utils.c              # debug log 與系統資訊函式
├── tests/
│   └── stress_test.sh       # 15 個平行 client 測試
├── pcap/                    # tcpdump/Wireshark 封包檔案
└── README.md
```

## 系統需求

- macOS 或 Linux
- 支援 POSIX socket、`fork()` 與 shared library 的 C compiler
- `make`
- `tcpdump`（封包擷取時使用）

## 編譯

在專案根目錄執行：

```sh
make
```

Makefile 會依序：

1. 建立 `bin/` 與 `lib/`。
2. 使用 `-fPIC -shared` 將 `src/utils.c` 編譯成 `lib/libutils.so`。
3. 編譯 `bin/server` 與 `bin/client`，並連結 `libutils.so`。

清除編譯產物：

```sh
make clean
```

## 執行

### 啟動 server

```sh
./bin/server
```

啟用執行期 debug log：

```sh
./bin/server -v
```

預期輸出：

```text
Server listening on port 8080...
```

### 執行 client

另開一個終端機執行：

```sh
./bin/client
```

啟用執行期 debug log：

```sh
./bin/client -v
```

client 會顯示類似以下回應：

```text
Server reply: OS: Darwin 24.x.x, Arch: arm64
```

目前 client 的 server 位址與 port 是寫在 `src/client.c` 的常數中，並非由
命令列指定。Server 與 client 必須使用相同的 `8080` port。

## Debug log 控制

Makefile 的 `CFLAGS` 包含 `-DDEBUG`，因此 debug logging code 會被編譯進
程式。只有同時滿足以下條件時才會輸出 `[DEBUG]` 訊息：

```text
編譯時有 -DDEBUG
且
執行時加入 -v
```

例如：

```sh
./bin/server -v
./bin/client -v
```

若移除 Makefile 中的 `-DDEBUG` 後重新編譯，即使執行時加入 `-v`，debug log
也不會輸出。

## 10+ clients 併發測試

先在一個終端機啟動 server：

```sh
./bin/server -v
```

再於另一個終端機執行：

```sh
./tests/stress_test.sh
```

測試腳本會同時啟動 15 個 `bin/client`，等待全部 client 結束後輸出：

```text
Starting 15 concurrent clients...
All 15 clients finished successfully.
```

Server 的 `listen()` backlog 設為 20，且每個連線由個別 child process 處理。
可搭配 server 的 debug log、`ps` 或 `pgrep` 觀察併發處理情形。

## 封包擷取與分析

封包擷取前先啟動 server，然後在另一個終端機啟動 tcpdump：

### macOS

macOS 的 loopback 介面名稱是 `lo0`：

```sh
sudo tcpdump -i lo0 port 8080 -w pcap/loopback.pcap
```

### Linux

Linux 通常使用 `lo`：

```sh
sudo tcpdump -i lo port 8080 -w pcap/loopback.pcap
```

接著執行 client 或併發測試，完成後回到 tcpdump 終端機按 `Ctrl+C`。

讀取封包摘要：

```sh
tcpdump -r pcap/loopback.pcap
```

也可以將 `pcap/loopback.pcap` 載入 Wireshark，分析：

- TCP three-way handshake：`SYN`、`SYN/ACK`、`ACK`
- client request：`GET_SYS_INFO`
- server response：系統資訊字串
- TCP connection termination：`FIN/ACK`

`.pcap` 是二進位檔，不能用一般文字編輯器判斷內容；應使用 `tcpdump -r`
或 Wireshark 開啟。

## Robustness 示範

### 1. Zombie process prevention

`SIGCHLD` handler 會在 child 結束時呼叫 `waitpid(-1, NULL, WNOHANG)`，回收
已結束的 child process。可在壓力測試期間觀察 process 狀態，確認不會累積
`<defunct>` process。

### 2. Client 意外斷線

Server 使用：

```c
signal(SIGPIPE, SIG_IGN);
```

因此 client 連線中斷時，server 不會因 `SIGPIPE` 預設行為而整個終止，仍可
繼續接受其他 client。

### 3. Server 快速重啟

Server 在 `bind()` 前設定：

```c
setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, ...);
```

可降低 server 重啟時因 TCP socket 狀態造成 `Address already in use` 的機率。

## 作業要求對應

| 作業要求                           | 本專案實作                                     |
| ---------------------------------- | ---------------------------------------------- |
| Client-server application          | `src/client.c`、`src/server.c`             |
| `fork()` client handling         | Server 每次 `accept()` 後呼叫 `fork()`     |
| 至少 10 clients concurrently       | `tests/stress_test.sh` 同時啟動 15 個 client |
| Packet capture and analysis        | `pcap/`、`tcpdump`、Wireshark              |
| Compile-time/runtime debug control | `-DDEBUG` 與 `-v`                          |
| Dynamic shared library             | `lib/libutils.so`                            |
| System information feature         | `GET_SYS_INFO` 與 `uname()`                |
| Robustness mechanisms              | `SIGCHLD`、`SIGPIPE`、`SO_REUSEADDR`     |
| Makefile/CMake build               | `Makefile`                                   |

## 注意事項

- `tests/stress_test.sh` 不會自動啟動或停止 server；執行前必須先手動啟動
  `./bin/server`。
- Server 與 client 目前只支援固定的 port `8080`。
- `bin/`、`lib/` 與產生的 `.pcap` 檔案屬於建置或測試產物，不應將不必要的
  大型封包檔案提交至 GitHub。
