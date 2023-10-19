#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#include <WinSock2.h>
#include <stdio.h>
#include <stdlib.h>
#include<iostream>
#include<string.h>
#include<Windows.h>
#include<list>
using namespace std;

// 1、主线程用于持续监听客户端连接
///2、子线程用于处理连接成功的客户端，
// 即为每个连接成功的客户端建立线程，一旦收到该客户端的消息，则广播给其他客户端


#define MAX_MESSAGE_LEN 128               // 消息最大长度
#define MAX_NAME_LEN  10                  // 用户名最大长度
#pragma comment(lib,"ws2_32.lib")

struct ClientInfo {
    SOCKET S_Client;                      // 客户端套接字
    char Name[MAX_NAME_LEN] = { 0 };      // 客户端用户名          
};
list<ClientInfo> mylist;                  // 用于维护所有已连接的客户端

DWORD WINAPI AcceptThread(LPVOID param) {// LPVOID param用来传递主线程传给新线程的参数，此处为ClientSocket
        /*接收该客户端发送的消息。
          将收到的消息广播给所有已连接的客户端。
          检测到客户端断开连接时，清理资源并结束线程。*/

    char Message[MAX_MESSAGE_LEN] = { 0 };
    SOCKET ClientSocket = (SOCKET)param;  // 类型转换，拿到主线程传递的参数,ClientSocket为请求连接的SOCKET
    while (true) {
        if (SOCKET_ERROR == recv(ClientSocket, Message, sizeof(Message), 0)) {
            cout << "recv函数产生错误，具体错误信息见下：" << endl;
            cout << WSAGetLastError() << endl;
            break;
        }
        if (strcmp(Message, "q") == 0) {
            // 通知客户端子进程，结束receive
            if (SOCKET_ERROR == send(ClientSocket, Message, sizeof(Message), 0)) {
                cout << "send函数产生错误，具体错误信息见下：" << endl;
                cout << WSAGetLastError() << endl;
            }
            // 从链表上删除该客户端
            for (list<ClientInfo>::iterator curr = mylist.begin(); curr != mylist.end(); curr++) {
                if (curr->S_Client == ClientSocket){
                    cout << "用户" << curr->Name << "已退出群聊！" << endl;
                    mylist.erase(curr);
                    break;
                }
            }
            return 0;
        }
        // Message是recv到的消息
        string str(Message);
        string pre = str.substr(0, 7);
        if (pre == "$n$ame:") {     // 这条消息不进行广播，用于服务端设置客户名字
            strcpy(mylist.back().Name, Message + 7);// 在链表中存下名字
            cout << "用户"<< mylist.back().Name<<"成功建立连接！" << endl;
            continue;
        }
        
        char MessageInfo[MAX_MESSAGE_LEN + MAX_NAME_LEN + 1] = { 0 }; // MessageInfo = name:Message
        // 遍历链表，根据SOCKET找到发送者名字
        for (list<ClientInfo>::iterator curr = mylist.begin(); curr != mylist.end(); curr++) {
            if (curr->S_Client == ClientSocket) {
                strcpy(MessageInfo, curr->Name);
                break;
            }
        }
        strcat(MessageInfo, ":");
        strcat(MessageInfo, Message);
        // 对消息进行广播
        for (list<ClientInfo>::iterator curr = mylist.begin(); curr != mylist.end(); curr++) {
            if (curr->S_Client != ClientSocket) { // 跳过发送消息的人
                if (SOCKET_ERROR == send(curr->S_Client, MessageInfo, sizeof(MessageInfo), 0)) {
                    cout << "send函数产生错误，具体错误信息见下：" << endl;
                    cout << WSAGetLastError() << endl;
                    WSACleanup();
                    return 0;
                }
            }
        }
    }
    exit(0);// 退出该进程
}

int main() {
    cout << "欢迎来到聊天室服务端界面！" << endl;
    WSADATA wsaData = { 0 };                          // 存放套接字信息
    SOCKET ServerSocket = INVALID_SOCKET;             // 套接字初始化为无效的套接字句柄
    // 每一步都需要完成错误检查
    
    // 初始化Winsock，请求版本2.2
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("Winsock初始化失败！\n");
        WSACleanup();
        return 0;
    }

    // socket()用于创建一个网络通信端点
    // AF_INET：对于IPv4，选择AF_INET即可
    // SOCK_STREAM：采用流式传输SOCK——STREAM
    ServerSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ServerSocket == INVALID_SOCKET) {                       // 错误处理
        cout << "socket函数产生错误，具体错误信息见下：" << endl;
        cout << WSAGetLastError() << endl;
        WSACleanup();
        return 0;
    }

    SOCKADDR_IN Server_addr;
    memset(&Server_addr, 0x0, sizeof(Server_addr));             // 清零
    USHORT Port = 8080;

    Server_addr.sin_family = AF_INET;                           // 采用IPv4协议
    Server_addr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);       // INADDR_ANY，即本机所有IP
    Server_addr.sin_port = htons(Port);                         // 设置端口号     


    // 将一个 IP 地址或端口号与一个套接字进行绑定
    if (SOCKET_ERROR == bind(ServerSocket, (SOCKADDR*)&Server_addr, sizeof(Server_addr))) {
        cout << "bind函数产生错误，具体错误信息见下：" << endl;
        cout << WSAGetLastError() << endl;
        WSACleanup();
        return 0;
    }

    /*HANDLE CreateThread(
        LPSECURITY_ATTRIBUTES lpThreadAttributes,//线程安全属性
        DWORD dwStackSize,                       //堆栈大小
        LPTHREAD_START_ROUTINE lpStartAddress,   //线程函数
        LPVOID lpParameter,                      //线程参数
        DWORD dwCreationFlags,                   //线程创建属性
        LPDWORD lpThreadId                       //线程ID
    );*/

    // 绑定完成后开始listen，这里设置的允许等待连接的最大队列为6
    // 使socket进入监听状态，监听远程连接是否到来
    if (SOCKET_ERROR == listen(ServerSocket, 6)) {
        cout << "listen函数产生错误，具体错误信息见下：" << endl;
        cout << WSAGetLastError() << endl;
        WSACleanup();
        return 0;
    }
    cout << "正在等待客户端连接……" << endl;

    while (true) {// 主线程持续监听，accept到新的客户端请求后就为他们创建新的线程
        SOCKADDR_IN Client_addr;                            // 用于返回发起请求的客户端的地址
        memset(&Client_addr, 0x0, sizeof(Client_addr));     // 初始化清零
        int ClientAddrLen = sizeof(Client_addr);            // 用于返回地址长度 

        // 获取客户端的连接请求并建立连接，返回新的socket套接字(类比餐馆里的包间)
        SOCKET ClientSocket = accept(ServerSocket, (SOCKADDR*)&Client_addr, &ClientAddrLen);
        if (INVALID_SOCKET == ClientSocket) {
            cout << "accept函数产生错误，具体错误信息见下：" << endl;
            cout << WSAGetLastError() << endl;
        }
        else {// 成功连接后建立新的线程
            ClientInfo c;
            c.S_Client = ClientSocket;
            mylist.push_back(c);      //插入到链表中
            // 将新的套接字传给线程
            HANDLE acceptthread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)AcceptThread, LPVOID(ClientSocket), 0, 0);
        }
    }
    // 清理Winsock资源
    WSACleanup();
    return 0;
}