# OpenSSL_Program＿說明文件

### 【如何編譯】
1. 開啟terminal移動到檔案所在的資料夾
2. 使用「make」指令即可成功編譯

### 【程式需求執行環境】
可在macOS中運行。

### 【套件版本】
OpenSSL@1.1.1i 

### 【執行server/client程式(編譯後的執行檔)】
輸入指令: ./server <port of the server>
輸入指令: ./client <IP address of the server> <port of the server>

### 【程式架構】
Server透過multi-thread實作Thread Pool，與到達的Client建立SSL/TLS連線，並進行一問一答(雙向傳輸)。
除此之外，client也會透過fork()建立child process，與其他client建立socket會談，並接受訊息（單向傳輸）。
![entrance_UI](./Architecture.png)

