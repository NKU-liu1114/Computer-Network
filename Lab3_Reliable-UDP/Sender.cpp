#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#include <WinSock2.h>
#include <stdio.h>
#include <stdlib.h>
#include<iostream>
#include<string.h>
#include<Windows.h>
#include<assert.h>
#include<cstdint>
#include<fstream>
#include<string.h>
#pragma comment(lib,"ws2_32.lib")
using namespace std;

#define SEND_CHECK 1
#define RECEIVE_CHECK 0
#define MAX_ATTEMPT 5 // 最多重传五次
FILE* p=nullptr;
const int SenderPort = 5000;
const int RouterPort = 6000;
SOCKET SenderSocket = { 0 };
SOCKADDR_IN Receiver_addr;
SOCKADDR_IN Sender_addr;
const int MaxBufferSize = 4096;
DWORD timeout = 2000;// 超时时间/阻塞时间
long long NextSeq = 2 ;// 初始序列号从2开始
struct Datagram {
    bool ack, syn, fin;
    uint16_t checksum;//16位校验和
    long long seqnum, acknum;
    int DataLen;// DataLen<=MaxBufferSize
    char data[MaxBufferSize] = { 0 };
}SendData, ReceiveData;
int DatagramLen = sizeof(Datagram);

void GetFileName(char* FilePath,char* FileName) {
    int last = 0, j = 0;
    for (int i = 0; FilePath[i]!='\0'; i++) {
        if (FilePath[i] == '/') {
            last = i;
        }
    }
    last++;
    for (; FilePath[j+last] != '\0'; j++) {
        FileName[j] = FilePath[j + last];
    }
    FileName[j] = '\0';
}

void init() {
    WSADATA wsaData = { 0 };
    int err;
    err = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (err == SOCKET_ERROR) {
        cout << "打开WSA失败！" << endl;
    }
    SenderSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (SenderSocket == SOCKET_ERROR) {
        cout << "打开套接字失败！" << endl;
    }
    
    Receiver_addr.sin_family = AF_INET; //地址类型
    Receiver_addr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1"); //地址
    Receiver_addr.sin_port = htons(RouterPort); //端口号

    Sender_addr.sin_family = AF_INET; //地址类型
    Sender_addr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1"); //地址
    Sender_addr.sin_port = htons(SenderPort); //端口号
    bind(SenderSocket, (LPSOCKADDR)&SenderSocket, sizeof(Sender_addr));

    cout << "发送方初始化成功！" << endl;
}

uint16_t Checksum(Datagram d,int flag) {
    unsigned short* check = (unsigned short*)&d;//两字节为一组来处理
    int count = DatagramLen / sizeof(short);// 一个数据报有 count组
    // 一共4128字节，能够被16比特(2字节)整除，故无需填充0
    d.checksum = 0;
    unsigned long sum = 0;
    while (count--) {
        sum += *check;
        check++;
        if (sum & 0xffff0000) {// 最高位有溢出
            sum = sum & 0xffff;
            sum++;
        }
    }
    if (flag == SEND_CHECK) {
        return (~sum) & 0xffff;//取反
    }
    else{
        return sum & 0xffff;
    }
}

void Send() {
    SendData.checksum = Checksum(SendData, SEND_CHECK);// 计算校验和
    if (SOCKET_ERROR == sendto(SenderSocket, (char*)&SendData, DatagramLen, 0,
        (struct sockaddr*)&Receiver_addr, sizeof(Receiver_addr))) {
        cout << "发送失败！" << endl;
    }
}

bool Receive() {// 超时或者数据有误时返回false
    int len = sizeof(Receiver_addr);
    if (SOCKET_ERROR == recvfrom(SenderSocket, (char*)&ReceiveData, DatagramLen, 0,
        (struct sockaddr*)&Receiver_addr, &len)) {
        if (WSAGetLastError()== WSAETIMEDOUT) {
            cout << "接收超时！" << endl;
        }
        else {
            cout << "接收失败！" << endl;
        }
        return false;
    }
    int check = Checksum(ReceiveData, RECEIVE_CHECK);
    if ((check ^ ReceiveData.checksum) == 0xffff) {// 数据无误
        return true;
    }
    else return false;
}

bool Shakehand() {
    SendData = { 0 };
    SendData.syn = true;
    Send();//第一次握手
    
    // 等待回应
    ReceiveData = { 0 };
    bool flag=Receive();
    if (flag == false || ReceiveData.ack == false || ReceiveData.syn == false || ReceiveData.acknum != 1) {
        return false;// 未收到正确的SYN+ACK包
    }

    // 配置发送数据
    SendData = { 0 };
    SendData.ack = true;
    SendData.seqnum = 1;
    SendData.acknum = 1;
    Send();//第三次握手
    // 设置为非阻塞，为数据传输做准备
    if (setsockopt(SenderSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout)) == SOCKET_ERROR) {
        cout << "设置接收非阻塞失败！" << endl;
    }
    return true;
}

bool Wavehand() {
    // 先回到阻塞状态
    u_long mode = 0; // 0表示阻塞模式
    if (SOCKET_ERROR==ioctlsocket(SenderSocket, FIONBIO, &mode)) {
        cout << "设置接收阻塞失败！" << endl;
        return false;
    }

    SendData = { 0 };
    SendData.fin = true;
    SendData.ack = true;
    Send();//第一次挥手

    ReceiveData = { 0 };
    int flag=Receive();
    if (flag == false || ReceiveData.ack == false||ReceiveData.acknum!=1) {
        return false;
    }
    
    ReceiveData = { 0 };
    flag = Receive();
    if (flag == false || ReceiveData.ack == false|| ReceiveData.fin==false|| ReceiveData.acknum != 1) {
        return false;
    }

    SendData = { 0 };
    SendData.ack = true;
    SendData.acknum = ReceiveData.seqnum+1;
    Send();
    return true;
}

int main()
{
    init();
    cout << "请输入待传输文件的路径：";
    char filepath[40];
    char filename[40] = { 0 };
    cin >> filepath;
    GetFileName(filepath, filename);
    // 打开文件
    if (!(p = fopen(filepath, "rb+"))) {
        cout << "打开文件失败！" << endl;
        return 0;
    }
    
    if (Shakehand()) {
        cout << "建立连接成功！" << endl;
    }
    else {
        cout << "建立连接失败，请重启程序继续传输！" << endl;
        return 0;
    }

    // 先把文件名传过去
    SendData = { 0 };
    SendData.seqnum = -1;//表示传的文件名
    SendData.DataLen = strlen(filename);
    strcpy(SendData.data, filename);
    Send();
    int attempt = 0;
    while (attempt < MAX_ATTEMPT) {
        bool flag = Receive();
        if (flag == true && ReceiveData.ack == true) {
            break;
        }
        Send();
        cout << "文件名第" << attempt + 1 << "次重传" << endl;
        attempt++;
    }
    if (attempt == MAX_ATTEMPT) {
        cout << "已达到重传次数上限！" << endl;
        cout << "传输终止！" << endl;
        return 0;
    }

    // 移动文件指针到文件末尾
    fseek(p, 0, SEEK_END);
    long long FileLen = ftell(p);
    long long t = FileLen;
    fseek(p, 0, SEEK_SET);
    cout << "开始传输！" << endl;
    while (t > 0) {
        SendData = { 0 };
        // 从文件指针p开始的地址读取1个MaxBufferSize大小的数据到SendData中
        fread(&SendData.data, MaxBufferSize, 1, p);
        if (t >= MaxBufferSize) {
            SendData.DataLen = MaxBufferSize;
        }
        else {
            SendData.DataLen = t;
        }
        SendData.seqnum = NextSeq;
        Send();
        NextSeq += SendData.DataLen;
        // 等待 ACK 包
        ReceiveData = { 0 };
        int attempt = 0;
        while (attempt < MAX_ATTEMPT) {// 最多重传五次
            bool flag = Receive();
            if (flag == true && ReceiveData.ack == true && ReceiveData.acknum == NextSeq) {// 直到成功接收到有序的ACK包
                break;
            }
            Send();
            cout << "序号为"<<SendData.seqnum << "的数据包第" << attempt + 1 << "次重传" << endl;
            attempt++;
        }
        if (attempt == MAX_ATTEMPT) {
            cout << "已达到重传次数上限！" << endl;
            cout << "传输终止！" << endl;
            fclose(p);
            closesocket(SenderSocket);
            WSACleanup();
            return 0;
        }
        t -= SendData.DataLen;
    }
    cout << "文件传输完毕！" << endl;
    if (Wavehand()) {
        cout << "成功关闭连接！" << endl;
    }
    else {
        cout << "关闭连接失败！" << endl;
    }
    fclose(p);// 关闭文件
    closesocket(SenderSocket);
    WSACleanup();
    return 0;
}


