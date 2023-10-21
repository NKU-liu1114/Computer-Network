#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#include <WinSock2.h>
#include <stdio.h>
#include <stdlib.h>
#include<iostream>
#include<string.h>
#include<Windows.h>
#pragma comment(lib,"ws2_32.lib")
using namespace std;

#define MAX_MESSAGE_LEN 128               // 消息最大长度
#define MAX_NAME_LEN    10                // 用户名最大长度
#define MAX_TIME_LEN    20

// 客户端主线程用于发送消息
// 子线程用于接收服务端的消息
// 退出群聊时主线程输入q发送给服务端，服务端再发送给接收线程，接收线程得到后即结束接收线程


DWORD WINAPI ReceiveThread(LPVOID param) {                                               // 用来接收服务器发送的消息
    char ReceiveMessage[MAX_MESSAGE_LEN + MAX_NAME_LEN + MAX_TIME_LEN + 3] = { 0 };      // 名字最大长度和消息最大长度和冒号
    SOCKET ClientSocket = (SOCKET)param;                                                 // 类型转换，拿到主线程传递的参数
    while (true) {
        if (SOCKET_ERROR == recv(ClientSocket, ReceiveMessage, sizeof(ReceiveMessage), 0)) {
            cout << "receive接收失败" << endl;
            cout << WSAGetLastError() << endl;
            break;
        }
        if (strcmp(ReceiveMessage, "q") == 0) {                              // 客户输入q退出时，服务端会发送q到接收进程，关闭该进程
            return 0;
        }
        cout << ReceiveMessage << endl << endl;
    }
    return 0;// 正常退出
}


int main() {
    cout << "欢迎来到聊天室客户端界面！" << endl;
    WSADATA wsaData = { 0 };                          // 存放套接字信息
    SOCKET ClientSocket = INVALID_SOCKET;             // 套接字初始化为无效的套接字句柄
    SOCKADDR_IN Server_addr;
    memset(&Server_addr, 0x0, sizeof(Server_addr)); // 清零
    USHORT Port = 8080;//服务端端口

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
    // 0：系统默认选择协议
    ClientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (ClientSocket == INVALID_SOCKET) {              // 错误处理
        cout << "socket函数产生错误，具体错误信息见下：" << endl;
        cout << WSAGetLastError() << endl;
        WSACleanup();
        return 0;
    }

    // 客户端不需要bind，操作系统会自动为客户端选择一个适当的IP和一个随机的临时端口号。

    Server_addr.sin_family = AF_INET;                            // 采用IPv4协议
    Server_addr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");   // 设置IP地址
    Server_addr.sin_port = htons(Port);                          // 设置端口号 

    char name[MAX_NAME_LEN] = { 0 }, nameinfo[MAX_NAME_LEN + 7] = { 0 };                 // nameinfo = "$n$ame:" + name
    cout << "请输入您的用户名（不超过10个字）：";
    cin.getline(name, 10);

    // 连接服务器
    if (SOCKET_ERROR == connect(ClientSocket, (SOCKADDR*)&Server_addr, sizeof(Server_addr))) {
        cout << "connect函数产生错误，具体错误信息见下：" << endl;
        cout << WSAGetLastError() << endl;
        WSACleanup();
        return 0;
    }
    // 提示信息
    cout << "您已进入群聊，请输入聊天内容：" << endl;
    cout << "如要退出，请输入q" << endl;
    strcpy(nameinfo, "$n$ame:");
    strcat(nameinfo, name);
    if (SOCKET_ERROR == send(ClientSocket, nameinfo, sizeof(nameinfo), 0)) {// 先把名字传过去
        cout << "send函数产生错误，具体错误信息见下" << endl;
        cout << WSAGetLastError() << endl;
        WSACleanup();
        return 0;
    }

    // 开启进程
    // ReceiveThread：新线程函数
    // ClientSocket：传递给线程函数的参数
    HANDLE recvthread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ReceiveThread, LPVOID(ClientSocket), 0, 0);
    CloseHandle(recvthread);   // 关闭句柄，以避免资源泄露,但进程继续执行
    char chat[MAX_MESSAGE_LEN] = { 0 };
    while (true) {
        cin.getline(chat, 128);
        cout << endl;
        if (SOCKET_ERROR == send(ClientSocket, chat, sizeof(chat), 0)) {
            cout << "send函数产生错误，具体错误信息见下" << endl;
            cout << WSAGetLastError() << endl;
            WSACleanup();
            return 0;
        }
        if (strcmp(chat, "q") == 0) {
            cout << "聊天结束！" << endl;
            closesocket(ClientSocket);
            // 清理Winsock资源
            WSACleanup();
            break;
        }
    }
    return 0;
}