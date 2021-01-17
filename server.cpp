#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <iostream>
#include <arpa/inet.h>
#include <pthread.h>
#include <queue>
#include <vector>
#include <string>
#include <errno.h>
#include <resolv.h>
#include "openssl/ssl.h"
#include "openssl/err.h"
#define FAIL    -1

using namespace std;

const int MAX_LEN = 1000;
const int listen_number = 10;
const int thread_number = 10;
class Client;
struct User;
struct UserList;

SSL_CTX* InitServerCTX(void)
{
    const SSL_METHOD *method;
    SSL_CTX *ctx;
    OpenSSL_add_all_algorithms();  /* load & register all cryptos, etc. */
    SSL_load_error_strings();   /* load all error messages */
    method = TLS_server_method();  /* create new server-method instance */
    ctx = SSL_CTX_new(method);   /* create new context from method */
    if ( ctx == NULL )
    {
        ERR_print_errors_fp(stderr);
        abort();
    }
    return ctx;
}
void LoadCertificates(SSL_CTX* ctx, char* CertFile, char* KeyFile)
{
    /* set the local certificate from CertFile */
    if ( SSL_CTX_use_certificate_file(ctx, CertFile, SSL_FILETYPE_PEM) <= 0 )
    {
        ERR_print_errors_fp(stderr);
        abort();
    }
    /* set the private key from KeyFile (may be the same as CertFile) */
    if ( SSL_CTX_use_PrivateKey_file(ctx, KeyFile, SSL_FILETYPE_PEM) <= 0 )
    {
        ERR_print_errors_fp(stderr);
        abort();
    }
    /* verify private key */
    if ( !SSL_CTX_check_private_key(ctx) )
    {
        fprintf(stderr, "Private key does not match the public certificate\n");
        abort();
    }
}
void ShowCerts(SSL* ssl)
{
    X509 *cert;
    char *line;
    cert = SSL_get_peer_certificate(ssl); /* Get certificates (if available) */
    if ( cert != NULL )
    {
        printf("Server certificates:\n");
        line = X509_NAME_oneline(X509_get_subject_name(cert), 0, 0);
        printf("Subject: %s\n", line);
        free(line);
        line = X509_NAME_oneline(X509_get_issuer_name(cert), 0, 0);
        printf("Issuer: %s\n", line);
        free(line);
        X509_free(cert);
    }
    else
        printf("No certificates.\n");
}

// 判別user輸入的指令類別為何(有Register/Login/List/Exit共四種)
char* Command_type(char* command) {
    char* p1 = strchr(command, '#');
    char* Type = new char[MAX_LEN]; // 動態記憶體配置(不可為local variable，因為要做為此函數回傳值)
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
            strcpy(Type, "Register");
        }
    }
    // cout << "Command: " << command << "\n";
    // cout << "Command Type: " << Type << "\n";
    
    return Type;
}

class Client
{
public:
    char *c_IP;
    SSL* c_ssl;
    Client();
    Client(char *IP, SSL* ssl);
};

Client::Client()
{
    this->c_IP = new char[MAX_LEN];
    // this->c_sockfd = 0;
}

Client::Client(char *IP, SSL* ssl)
{
    this->c_IP = IP;
    this->c_ssl = ssl;
}

struct User
{
    string money;
    string name;
    string IP;
    string port_number;
    bool online;
    User();
    User(string money, string name, string IP, string port_number, bool online);
    string getMoney();
    string getName();
    string getPort();
    string getIP();
    void setMoney(string money);
    void setIP(string IP);
    void setPort(string port_number);
    void setName(string name);
    void setOnline();
    void setOffline();
    friend class UserList;
};

User::User()
{
    this->money = "";
    this->name = "";
    this->IP = "";
    this->port_number = "";
    this->online = false;
}

User::User(string money, string name, string IP, string port_number, bool online)
{
    this->money = money;
    this->name = name;
    this->IP = IP;
    this->port_number = port_number;
    this->online = online;
}

string User::getIP()
{
    return this->IP;
}
string User::getPort()
{
    return this->port_number;
}
string User::getName()
{
    return this->name;
}
string User::getMoney()
{
    return this->money;
}
void User::setMoney(string money)
{
    this->money = money;
}
void User::setIP(string IP)
{
    this->IP = IP;
}
void User::setPort(string port_number)
{
    this->port_number = port_number;
}
void User::setName(string name)
{
    this->name = name;
}
void User::setOnline()
{
    this->online = true;
}
void User::setOffline()
{
    this->online = false;
}

struct UserList
{
    vector<User *> usersList_ptr;
    int getOnlineCnt();
    User *findUser(string name);
    char* onlineList();
    bool nameExisted(string name);
};

User *UserList::findUser(string userName)
{
    int which_user = -1;
    for (int i = 0; i < this->usersList_ptr.size(); i++)
    {
        if (this->usersList_ptr[i]->name.compare(userName) == 0){
            which_user = i;
            return this->usersList_ptr[which_user];
        }
    }
    return nullptr;
}
int UserList::getOnlineCnt()
{
    int cnt = 0;
    for (int i = 0; i < this->usersList_ptr.size(); i++)
    {
        if (this->usersList_ptr[i]->online)
            cnt++;
    }
    return cnt;
}
bool UserList::nameExisted(string name)
{
    for (int i = 0; i < this->usersList_ptr.size(); i++)
    {
        if (this->usersList_ptr[i]->name == name)
            return true;
    }
    return false;
}
char* UserList::onlineList() {
    char* online_list = new char[MAX_LEN];
    for (int i = 0; i < this->usersList_ptr.size(); i++) {
        if(usersList_ptr[i]->online) {
            strcat(online_list, usersList_ptr[i]->name.c_str());
            // todo: debug
            strcat(online_list, "#");
            strcat(online_list, usersList_ptr[i]->IP.c_str());
            strcat(online_list, "#");
            strcat(online_list, usersList_ptr[i]->port_number.c_str());
            strcat(online_list, "\n");
        }
    }
    return online_list;
}

UserList UL;
queue<Client> qc;
pthread_mutex_t mutex1;
void* work(void *x) {
    while (true)
    {
        usleep(1000000);
        pthread_mutex_lock(&(mutex1));
        if(qc.empty()){
            pthread_mutex_unlock(&(mutex1));
            continue;
        }
        Client aClient = qc.front();
        qc.pop();
        pthread_mutex_unlock(&(mutex1));
        cout << "Successfully assign a thread to serve the client!\n";
        bool havelogin = false;
        string myName = "";
        // ssh
        SSL* ssl = aClient.c_ssl;
        int sd, bytes;

        while (true) {
            char command[MAX_LEN] = {};
            memset(command, '\0', sizeof(command));
            // recv(aClient.c_sockfd,command,sizeof(command),0);
            bytes = SSL_read(ssl, command, sizeof(command)); /* get request */
            command[bytes] = '\0';            
            cout << "client message: " << command << "\n";
            char* CommandType = Command_type(command);
            char s_msg[MAX_LEN] = {};
            memset(s_msg, '\0', sizeof(s_msg));
            if(strcmp(CommandType, "Register") == 0){
                char* p1 = strchr(command, '#');
                char* p2 = strchr(p1+1, '#'); //Account Name
                *p2 = '\0';
                char* p3 = strchr(p2+1, '\n'); //Account Balance
                if(p3 != nullptr)
                    *p3 = '\0';

                string name(p1+1);
                string money(p2+1);
                bool DoubleRegisterd = UL.nameExisted(name);
                if(!DoubleRegisterd) {
                    User* aUser = new User(money, name, aClient.c_IP, "", false); //要login後才有port number，故先設""。
                    UL.usersList_ptr.push_back(aUser);
                    sprintf(s_msg, "%s", "100 OK\n");
                    // send(aClient.c_sockfd, s_msg, strlen(s_msg), 0);
                    SSL_write(ssl, s_msg, strlen(s_msg)); /* send reply */
                }
                else {
                    sprintf(s_msg, "%s", "Please not register twice!\n");
                    // send(aClient.c_sockfd, s_msg, strlen(s_msg), 0);
                    SSL_write(ssl, s_msg, strlen(s_msg)); /* send reply */
                }
            }

            else if(strcmp(CommandType, "Login") == 0){
                if(havelogin) {
                    strcpy(s_msg, "Please not log in twice!\n");
                    // send(aClient.c_sockfd, s_msg, strlen(s_msg), 0);
                    SSL_write(ssl, s_msg, strlen(s_msg)); /* send reply */
                    cout << "server message: " << s_msg << "\n";                   
                    continue;
                }
                char* p1 = strchr(command, '#'); //Account Name
                *p1 = '\0';
                char* p2 = strchr(p1+1, '\n'); //Port Number
                if(p2 != nullptr)
                    *p2 = '\0';
                myName = command;
                User *userPtr = UL.findUser(myName);
                if(userPtr == nullptr) {
                    sprintf(s_msg, "%s", "Please register first!\n");
                    // send(aClient.c_sockfd, s_msg, strlen(s_msg), 0);
                    SSL_write(ssl, s_msg, strlen(s_msg)); /* send reply */
                }
                else if(userPtr->online) {
                    sprintf(s_msg, "%s", "You have already logged in other place!\n");
                    // send(aClient.c_sockfd, s_msg, strlen(s_msg), 0);
                    SSL_write(ssl, s_msg, strlen(s_msg)); /* send reply */
                }
                else {
                    havelogin = true;
                    userPtr->setPort(p1+1);
                    userPtr->setOnline();
                    strcat(s_msg, "Account Balance: ");
                    char temp[MAX_LEN] = {};
                    sprintf(temp, "%s", userPtr->getMoney().c_str());
                    strcat(s_msg, temp);
                    strcat(s_msg, "\nNumber of Account Online: ");
                    sprintf(temp, "%i", UL.getOnlineCnt());
                    strcat(s_msg, temp);
                    strcat(s_msg, "\n");
                    sprintf(temp, "%s", UL.onlineList());
                    strcat(s_msg, temp);
                    // send(aClient.c_sockfd, s_msg, strlen(s_msg), 0);
                    SSL_write(ssl, s_msg, strlen(s_msg)); /* send reply */
                }
            }

            else if(strcmp(CommandType, "List") == 0){
                User *userPtr = UL.findUser(myName);
                strcat(s_msg, "Account Balance: ");
                char temp[MAX_LEN] = {};
                sprintf(temp, "%s", userPtr->getMoney().c_str());
                strcat(s_msg, temp);
                strcat(s_msg, "\nNumber of Account Online: ");
                sprintf(temp, "%i", UL.getOnlineCnt());
                strcat(s_msg, temp);
                strcat(s_msg, "\n");
                sprintf(temp, "%s", UL.onlineList());
                strcat(s_msg, temp);
                // send(aClient.c_sockfd, s_msg, strlen(s_msg), 0);
                SSL_write(ssl, s_msg, strlen(s_msg)); /* send reply */
            }

            else if(strcmp(CommandType, "Exit") == 0){
                User *userPtr = UL.findUser(myName);
                userPtr->setOffline();
                userPtr->setPort("");
                userPtr->setIP("");
                strcpy(s_msg, "Bye!\n");
                // send(aClient.c_sockfd, s_msg, strlen(s_msg), 0);
                SSL_write(ssl, s_msg, strlen(s_msg)); /* send reply */
                cout << "server message: " << s_msg << "\n";
                break;
            }
            else if(strcmp(CommandType, "Unknown Command") == 0) {
                strcpy(s_msg, "Try Again!\n");
                // send(aClient.c_sockfd, s_msg, strlen(s_msg), 0);
                SSL_write(ssl, s_msg, strlen(s_msg)); /* send reply */
            }
            cout << "server message: " << s_msg << "\n";     
        }
        sd = SSL_get_fd(ssl);       /* get socket connection */
        SSL_free(ssl);         /* release SSL state */
        close(sd);          /* close connection */   
    }
}



int main(int argc, char *argv[]) {
    int sockfd = 0;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1){
         printf("Fail to create a socket.");
    }

    struct sockaddr_in serverInfo;
    bzero(&serverInfo,sizeof(serverInfo));
    serverInfo.sin_family = PF_INET;
    serverInfo.sin_addr.s_addr = INADDR_ANY;
    serverInfo.sin_port = htons(atoi(argv[1]));

    bind(sockfd,(struct sockaddr *)&serverInfo,sizeof(serverInfo));
    listen(sockfd,listen_number);
    
    int mutexInit = pthread_mutex_init(&(mutex1), NULL);
    if (mutexInit != 0)
        cout << "error on locking!\n";

    pthread_t *threads = new pthread_t[thread_number];
    for (int i = 0; i < thread_number; i++)
    {
        pthread_create(&(threads[i]), NULL, work, NULL);
    }
    //ssh
    SSL_CTX *ctx;
    SSL_library_init();
    ctx = InitServerCTX();        /* initialize SSL */
    char path[200] = {};
    strcpy(path, "mycert.pem");
    LoadCertificates(ctx, path, path); /* load certs */    

    int forClientSockfd = 0;
    struct sockaddr_in clientInfo;
    socklen_t addrlen = sizeof(clientInfo); // 注意：變數類別是為socklen_t，而非int
    cout << "Waiting for connection!\n";
    while(1){
        forClientSockfd = accept(sockfd,(struct sockaddr*) &clientInfo, &addrlen);
        // ssh
        SSL *ssl;
        ssl = SSL_new(ctx); 
        SSL_set_fd(ssl, forClientSockfd); 
        if ( SSL_accept(ssl) == FAIL )     /* do SSL-protocol accept */
            ERR_print_errors_fp(stderr);
        else
            ShowCerts(ssl);

        if(forClientSockfd == -1) {
            printf("error on accepting\n");
        }
        else {
            char message[] = {"Connection accepted.\n"};
            SSL_write(ssl, message, strlen(message)); /* send reply */
        }
        struct sockaddr_in *pV4Addr = (struct sockaddr_in *)&forClientSockfd;
        //以下四行用以取得該client的IP address
        struct in_addr ipAddr = pV4Addr->sin_addr;
        char client_ip_addr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &ipAddr, client_ip_addr, INET_ADDRSTRLEN);
        Client client(client_ip_addr, ssl);
        qc.push(client);
    }
    return 0;
}

