#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <iostream>
#include <string>
#include <errno.h>
#include <resolv.h>
#include <netdb.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#define FAIL    -1
using namespace std;
    
const int LEN = 200;

SSL_CTX* InitClientCTX(void)
{
    const SSL_METHOD *method;
    SSL_CTX *ctx;
    OpenSSL_add_all_algorithms();  /* Load cryptos, et.al. */
    SSL_load_error_strings();   /* Bring in and register error messages */
    method = TLS_client_method();  /* Create new client-method instance */
    ctx = SSL_CTX_new(method);   /* Create new context */
    if ( ctx == NULL )
    {
        ERR_print_errors_fp(stderr);
        abort();
    }
    return ctx;
}

void ShowCerts(SSL* ssl)
{
    X509 *cert;
    char *line;
    cert = SSL_get_peer_certificate(ssl); /* get the server's certificate */
    if ( cert != NULL )
    {
        printf("Server certificates:\n");
        line = X509_NAME_oneline(X509_get_subject_name(cert), 0, 0);
        printf("Subject: %s\n", line);
        free(line);       /* free the malloc'ed string */
        line = X509_NAME_oneline(X509_get_issuer_name(cert), 0, 0);
        printf("Issuer: %s\n", line);
        free(line);       /* free the malloc'ed string */
        X509_free(cert);     /* free the malloc'ed certificate copy */
    }
    else
        printf("Info: No client certificates configured.\n");
}

// 判別user輸入的指令類別為何(有Register/Login/List/Transaction/Exit共五種)
char* Command_type(char* command) {
    char* p1 = strchr(command, '#');
    char* Type = new char[LEN]; // 動態記憶體配置(不可為local variable，因為要做為此函數回傳值)
    if(p1 == nullptr) { // 若指令中沒出現#字號
        if(strcmp(command, "Exit") == 0)
            strcpy(Type, "Exit");
        else if(strcmp(command, "List") == 0)
            strcpy(Type, "List");
        else
            strcpy(Type, "Unknown Command");
    }
    else if (p1 != nullptr){
        char* p2 = strchr(p1+1, '#');
        if(p2 == nullptr) // 若指令中只出現一次#字號
            strcpy(Type, "Login");
        else if(p2 != nullptr) { // 若指令中出現兩次#字號
            if(isdigit(p1[1]) != 0)
                strcpy(Type, "Transaction");
            else
                strcpy(Type, "Register");
        }
    }

    //cout << "Command Type: " << Type << "\n";
    
    return Type;
}
// 從Server回傳的User list中找到payee的port number
int payee_port(char* command, char* user_list) {
    // 傳入變數範例如下
    // command : UserA#11111#UserB
    // user_list: 9999\n3\nUserA#127.0.0.1#9090\nUserB#127.0.0.1#9091\nUserC#127.0.0.1#9092
    char* p1 = strchr(command, '#');
    char* p2 = strchr(p1+1, '#');
    char* p3 = strstr(user_list, p2+1); // p2+1為payee的name(UserB)
    if(p3 != nullptr) {
        char* p4 = strchr(p3, '#');
        char* p5 = strchr(p4+1, '#'); // p5+1為payee的port number(9091)
        char* p6 = strchr(p5, '\n'); // todo
        if(p6 != nullptr)
            *p6 = '\0';
        cout << "The port number of " << p2+1 << " is " << p5+1 << "\n";
        return atoi(p5+1);
    }
    cout << p2+1 << " is not online!" << "\n";
    return 0; //todo
}

//  建立單次使用的socket，傳送訊息後即關閉
void send_message(char* command, int host_port, int size) {
    int sockfd = 0;
    sockfd = socket(AF_INET , SOCK_STREAM , 0);
    
    if (sockfd == -1){
        printf("Fail to create a socket.");
    }
    
    string ip_addr = "127.0.0.1";
    
    struct sockaddr_in info;
    bzero(&info,sizeof(info));
    info.sin_family = PF_INET;
    info.sin_addr.s_addr = inet_addr(ip_addr.c_str());
    info.sin_port = htons(host_port);
    
    int err = connect(sockfd,(struct sockaddr *)&info,sizeof(info));
    if(err==-1){
        printf("Connection error in send_message");
    }
    //cout << "command: " << command << "\n";
    send(sockfd,command,size,0);
    close(sockfd);
}

int main(int argc , char *argv[])
{
    // printf("%s\n","version15");
    //建立client的socket
    int sockfd = 0;
    sockfd = socket(AF_INET , SOCK_STREAM , 0);

    if (sockfd == -1){
        printf("Fail to create a socket.");
    }
    // 從argv參數中傳入server的socket的IP address跟port number
    string ip_addr = argv[1];
    if(ip_addr.compare("localhost") == 0)
    {
        ip_addr = "127.0.0.1";
    }
    int host_port = atoi(argv[2]);

    // 儲存上述參數至sockaddr_in物件
    struct sockaddr_in info;
    bzero(&info,sizeof(info));
    info.sin_family = PF_INET;
    info.sin_addr.s_addr = inet_addr(ip_addr.c_str());
    info.sin_port = htons(host_port);

    // 讓client的socket與server的socket建立TCP連線
    int err = connect(sockfd,(struct sockaddr *)&info,sizeof(info));
    if(err==-1){
        printf("Connection error");
        return 0;
    }

    // SSL/TLS安全通訊
    SSL_CTX *ctx;
    SSL *ssl;
    int bytes;
    SSL_library_init();
    ctx = InitClientCTX();
    ssl = SSL_new(ctx);      /* create new SSL connection state */
    SSL_set_fd(ssl, sockfd);    /* attach the socket descriptor */
    if ( SSL_connect(ssl) == FAIL )   /* perform the connection */
        ERR_print_errors_fp(stderr);
    else {
        printf("\n\nConnected with %s encryption\n", SSL_get_cipher(ssl));
        ShowCerts(ssl);        /* get any certs */        
    }

    // 接收server回傳的第一筆訊息(Connection accepted!)
    char receiveConnect[LEN] = {};
    bytes = SSL_read(ssl, receiveConnect, sizeof(receiveConnect)); /* get reply & decrypt */
    receiveConnect[bytes] = 0;
    printf("Received: %s\n", receiveConnect);
    
    // client與server進行資料的傳輸(雙向)
    int my_port = -1;
    while(true)
    {
        // 使用者可用鍵盤輸入要傳給server的訊息
        char command[LEN] = {};
        char receiveMessage[LEN] = {};
        cin.getline(command, sizeof(char)*LEN);
        
        // 判別使用者輸入的訊息屬於何種指令類別
        char* CommandType = Command_type(command);

        // 若為Transaction，則payer的socket與payee's child process的socket建立TCP連線
        if(strcmp(CommandType, "Transaction") == 0) {
            // 跟server要使用者清單(內有payee的port)
            char listReq[LEN] = {};
            strcpy(listReq, "List");
            SSL_write(ssl,listReq, strlen(listReq));   /* encrypt & send message */
            bytes = SSL_read(ssl, receiveMessage, sizeof(receiveMessage)); /* get reply & decrypt */
            receiveMessage[bytes] = 0;

            // 找到payee的port
            int payeePort = payee_port(command, receiveMessage);
            if(payeePort == 0)
            	continue;
            // 與payee's child process的socket建立TCP連線，並傳送訊息(單次，傳送完即關閉socket)
            send_message(command, payeePort, sizeof(char)*LEN);
        }
        // 若為其他四種指令類別
        else {
            // 都要先將該指令傳送給server
            SSL_write(ssl,command, strlen(command));   /* encrypt & send message */
            bytes = SSL_read(ssl, receiveMessage, sizeof(receiveMessage)); /* get reply & decrypt */
            receiveMessage[bytes] = 0;
            printf("Received: %s\n", receiveMessage);
            
            // 若為Login，則用fork建立該client's child process，然後建立socket實作server(做為payee時可供其他client連線)
            if(strcmp(CommandType, "Login") == 0) {
                char* p1 = strchr(command, '#');
                my_port = atoi(p1+1); // 從Login command中擷取自己的port
                //cout << "my_port" << my_port << endl;
                pid_t pid;
                pid = fork();
                if(pid == 0) {
                    //****** 在該client's child process中建立server ******(start)
                    
                    int sockfd2 = 0;
                    sockfd2 = socket(AF_INET, SOCK_STREAM ,0);
                    if (sockfd2 == -1){
                         printf("Fail to create a socket.");
                    }

                    struct sockaddr_in serverInfo;
                    bzero(&serverInfo,sizeof(serverInfo));
                    serverInfo.sin_family = PF_INET;
                    serverInfo.sin_addr.s_addr = INADDR_ANY;
                    serverInfo.sin_port = htons(my_port);

                    bind(sockfd2,(struct sockaddr *)&serverInfo,sizeof(serverInfo));
                    listen(sockfd2,5);

                    char inputBuffer[LEN] = {};
                    int forClientSockfd2 = 0;
                    struct sockaddr_in clientInfo;
                    socklen_t addrlen = sizeof(clientInfo); // 注意：變數類別是為socklen_t，而非int                    
                    while(1){
                        forClientSockfd2 = accept(sockfd2,(struct sockaddr*) &clientInfo, &addrlen);
                        recv(forClientSockfd2,inputBuffer,sizeof(inputBuffer),0);
                        printf("Received Messages: %s\n",inputBuffer);
                        if(strcmp(inputBuffer, "Exit") == 0) {
                        	close(forClientSockfd2);
                            break;
                        }
                    }
                    close(sockfd2);
                    return 0;
                    //****** 在該client的child process中建立server ******(end)
                }
            }
            // 若為Exit，則終止程式
            else if(strcmp(CommandType, "Exit") == 0) {
                char message[LEN] = {};
                strcpy(message, "Exit");
                send_message(message, my_port,sizeof(char)*LEN);
                wait(NULL);
                break;
            }
        }
    }
    SSL_free(ssl);
    close(sockfd);
    SSL_CTX_free(ctx);
    
    return 0;
}
