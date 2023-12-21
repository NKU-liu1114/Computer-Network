#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#include <WinSock2.h>
#include <stdio.h>
#include<string.h>
#include <stdlib.h>
#include<iostream>
#include<string.h>
#include<Windows.h>
#include<assert.h>
#include<cstdint>
#include<fstream>
#include<ctime>
#pragma comment(lib,"ws2_32.lib")
using namespace std;


#define SEND_CHECK 1
#define RECEIVE_CHECK 0
#define MAX_ATTEMPT 5
long long ExpectedNum = 0;
const int ReceiverPort = 7000;
const int RouterPort = 6000;
char filename[40];
FILE* p = 0;// 文件指针
SOCKET ReceiverSocket = { 0 };
SOCKADDR_IN Receiver_addr = { 0 };
SOCKADDR_IN Sender_addr = { 0 };
const int MaxBufferSize = 4096;

struct Receive_Datagram {
    bool ack, syn, fin, sack;
    uint16_t checksum;//16位校验和
    long long seqnum, acknum, sacknum;
    int DataLen;// DataLen<=MaxBufferSize
    char data[MaxBufferSize] = { 0 };
}ReceiveData;
int Receive_DatagramLen = sizeof(Receive_Datagram);


struct Send_Datagram {
    bool ack, syn, fin, sack;
    uint16_t checksum;//16位校验和
    long long seqnum, acknum, sacknum;
    // 不需要数据缓冲区
}SendData;
int Send_DatagramLen = sizeof(Send_Datagram);

int SeqNumber;// 记录一共有多少个数据包
Receive_Datagram* Buffer;// 用来缓存数据，最后一次性写入文件中
bool* flag_cache;

uint16_t Checksum(Send_Datagram d, int flag) {
    unsigned short* check = (unsigned short*)&d;//两字节为一组来处理
    int count = Send_DatagramLen / sizeof(short);// 一个数据报有 count 组
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
    else {
        return sum & 0xffff;
    }
}

// 利用函数重载
uint16_t Checksum(Receive_Datagram d, int flag) {
    unsigned short* check = (unsigned short*)&d;//两字节为一组来处理
    int count = Receive_DatagramLen / sizeof(short);// 一个数据报有 count 组
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
    else {
        return sum & 0xffff;
    }
}

void init() {
    WSADATA wsaData = { 0 };
    int err = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (err == SOCKET_ERROR) {
        cout << "打开WSA失败！" << endl;
    }
    ReceiverSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (ReceiverSocket == -1) {
        cout << "创建套接字失败！" << endl;
    }

    Receiver_addr.sin_family = AF_INET; //地址类型
    Receiver_addr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1"); //地址
    Receiver_addr.sin_port = htons(ReceiverPort); //端口号

    Sender_addr.sin_family = AF_INET; //地址类型
    Sender_addr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1"); //地址
    Sender_addr.sin_port = htons(RouterPort); //端口号

    if (SOCKET_ERROR == bind(ReceiverSocket, (SOCKADDR*)&Receiver_addr, sizeof(Receiver_addr))) {
        cout << "bind函数产生错误！" << endl;
        cout << WSAGetLastError() << endl;
        WSACleanup();
    }
    cout << "接收方初始化成功！" << endl;
}
void Send() {
    SendData.checksum = Checksum(SendData, SEND_CHECK);// 计算校验和
    if (SOCKET_ERROR == sendto(ReceiverSocket, (char*)&SendData, Send_DatagramLen, 0,
        (struct sockaddr*)&Sender_addr, sizeof(Sender_addr))) {
        cout << "发送失败！" << endl;
    }
}
bool Receive() {
    int len = sizeof(Receiver_addr);
    if (SOCKET_ERROR == recvfrom(ReceiverSocket, (char*)&ReceiveData, Receive_DatagramLen, 0,
        (struct sockaddr*)&Sender_addr, &len)) {
        cout << "接收失败！" << endl;
        return false;
    }
    int check = Checksum(ReceiveData, RECEIVE_CHECK);
    if ((check ^ ReceiveData.checksum) == 0xffff) {// 数据无误
        return true;
    }
    else return false;
}
bool Shakehand() {
    ReceiveData = { 0 };
    Receive();
    assert(ReceiveData.syn == true);
    // 接收私货
    memcpy(&SeqNumber, ReceiveData.data, sizeof(int));


    SendData = { 0 };
    SendData.syn = true;
    SendData.ack = true;
    SendData.acknum = ReceiveData.seqnum + 1;
    Send();
    ReceiveData = { 0 };
    Receive();
    if (ReceiveData.ack == true && ReceiveData.acknum == 1 && ReceiveData.seqnum == 1) {
        // 第三次握手传文件名
        strcpy(filename, ReceiveData.data);
        return true;
    }
    else return false;
}
int main() {
    init();
    cout << "正在等待建立连接……" << endl;
    if (Shakehand()) {
        cout << "建立连接成功！" << endl;
    }
    else {
        cout << "建立连接失败，请重启程序继续传输！" << endl;
        return 0;
    }
    Buffer = new Receive_Datagram[SeqNumber];
    flag_cache = new bool[SeqNumber];
    for (int i = 0; i < SeqNumber; i++) {
        Buffer[i] = { 0 };
        flag_cache[i] = 0;
    }
    cout << "待接受的数据包数：" << SeqNumber << endl;
    cout << "待传输的文件名：" << filename << endl;
    ofstream OutputFile(filename, ios::binary);
    int temp = sizeof(Receiver_addr);
    ReceiveData = { 0 };
    if (!(p = fopen(filename, "wb+"))) {
        cout << "打开文件失败!" << endl;
    }
    cout << "开始接收！" << endl;
    while (true) {
        ReceiveData = { 0 };
        bool recv = Receive();
        if (recv && ReceiveData.fin == true && ReceiveData.ack == true) {// 第一次挥手
            break;
        }
        if (recv) {//数据无误
            if (ReceiveData.seqnum == ExpectedNum) {
                Buffer[ReceiveData.seqnum] = ReceiveData;
                cout << "成功接收第" << ExpectedNum << "个数据包" << endl;
                SendData = { 0 };
                SendData.ack = true;
                for (int i = ++ExpectedNum; flag_cache[i] && i < SeqNumber; i++) {// 有可能刚好填补上空缺的一块，这里需要连续增加ExpectedNum
                    ExpectedNum++;
                }
                SendData.acknum = ExpectedNum - 1;//期望值-1
                Send();
            }
            else if (ReceiveData.seqnum > ExpectedNum) {// 收到的不是期望的数据包，缓存并回复ACK
                Buffer[ReceiveData.seqnum] = ReceiveData;//这里即使重复收到，也不需要判断，直接覆盖即可
                cout << "第" << ReceiveData.seqnum << "个数据包失序缓存" << endl;
                flag_cache[ReceiveData.seqnum] = 1;
                SendData = { 0 };
                SendData.sack = true;//通知发送端这是失序回复报文
                SendData.sacknum = ReceiveData.seqnum;
                Send();
            }
            else {
                cout << "收到重复数据包，seqnum为：" << ReceiveData.seqnum << endl;
                SendData = { 0 };
                SendData.ack = true;
                SendData.acknum = ExpectedNum - 1;
                Send();
            }
        }

    }
    // 第二次挥手
    SendData = { 0 };
    SendData.ack = true;
    SendData.acknum = 1;
    Send();

    SendData.fin = true;
    SendData.ack = true;
    Send();

    ReceiveData = { 0 };
    Receive();
    assert(ReceiveData.ack == true && ReceiveData.acknum == 1);
    for (int i = 0; i < SeqNumber; i++) {
        fwrite(Buffer[i].data, Buffer[i].DataLen, 1, p);//写回文件
    }
    cout << "文件接收完毕！" << endl;
    OutputFile.close();
    closesocket(ReceiverSocket);
    WSACleanup();
    cout << "成功关闭连接" << endl;
    delete[]Buffer;
    delete[]flag_cache;
    return 0;
}