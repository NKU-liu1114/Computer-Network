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
long long NextSeq = 2;// 初始序列号从2开始
const int ReceiverPort = 7000;
const int RouterPort = 6000;
FILE* p = 0;// 文件指针
SOCKET ReceiverSocket = { 0 };
SOCKADDR_IN Receiver_addr = { 0 };
SOCKADDR_IN Sender_addr = { 0 };
const int MaxBufferSize = 4096;
struct Datagram {
    bool ack, syn, fin;
    uint16_t checksum;//16位校验和
    long long seqnum, acknum;
    int DataLen;// DataLen<=MaxBufferSize
    char data[MaxBufferSize] = { 0 };
}SendData, ReceiveData;
int DatagramLen = sizeof(Datagram);// 4116


uint16_t Checksum(Datagram d, int flag) {
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
    if (SOCKET_ERROR == sendto(ReceiverSocket, (char*)&SendData, DatagramLen, 0,
        (struct sockaddr*)&Sender_addr, sizeof(Sender_addr))) {
        cout << "发送失败！" << endl;
    }
}
bool Receive() {
    int len = sizeof(Receiver_addr);
    if (SOCKET_ERROR == recvfrom(ReceiverSocket, (char*)&ReceiveData, DatagramLen, 0,
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
    assert(ReceiveData.syn==true);
    SendData = { 0 };
    SendData.syn = true;
    SendData.ack = true;
    SendData.acknum = ReceiveData.seqnum+1;
    Send();
    ReceiveData = { 0 };
    Receive();
    if (ReceiveData.ack == true && ReceiveData.acknum == 1 && ReceiveData.seqnum == 1) {
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
    // 等待传输文件名
    ReceiveData = { 0 };
    while (true) {
        if (Receive() && ReceiveData.seqnum == -1) {
            SendData = { 0 };
            SendData.ack = true;
            Send();
            break;
        }
    }
    char filename[40];
    strcpy(filename, ReceiveData.data);
    cout << "待传输的文件名："<<ReceiveData.data << endl;
    ofstream OutputFile(filename, ios::binary);
    int temp = sizeof(Receiver_addr);
    ReceiveData = { 0 };
    if (!(p = fopen(filename, "wb+"))) {
        cout<<"打开文件失败!"<<endl;
    }
    cout << "开始接收！" << endl;
    int LastSeq = 0;//用于记录上一次接收到的ReceiveData.seqnum
    while (true) {
        // 情形一：发送方发的数据包丢失或有误，发送方会直接重传
        // 情形二：数据包成功到达，但不是期望的序列号，直接丢弃该包
        // 情形三：数据包成功到达，但接收方发的ACK包丢失或超时，发送方重传
        // 对于情形三，接收方拿到了数据包，对于重传的数据包只需回ACK,不需要fwrite
       
        bool flag=Receive();
        if (flag) {//数据无误
            if (ReceiveData.fin == true && ReceiveData.ack == true) {// fin+ack
                cout << "文件传输结束！" << endl;
                break;
            }
            if (ReceiveData.seqnum != NextSeq) {//拿到的是乱序的包，直接丢弃，继续Receive
                continue;
            }
            
            if (LastSeq != ReceiveData.seqnum) {// 拿到的不是重复数据包
                fwrite(ReceiveData.data, ReceiveData.DataLen, 1, p);//写回文件
                cout << "已收到序号为" << ReceiveData.seqnum << "的数据包" << endl;
                NextSeq += ReceiveData.DataLen;// 期望序列号增加
            }
            // 不管重没重复都需要发ACK包，可能是之前发回的ACK包超时或者丢失导致发送方重传（case3)
            SendData = { 0 };
            SendData.ack = true;
            SendData.acknum = ReceiveData.seqnum + ReceiveData.DataLen;// 代表acknum-2个字节流都已成功收到
            
            Send();
            LastSeq = ReceiveData.seqnum;
            ReceiveData = { 0 };
        }
        // 如果数据有误则循环继续等待Receive()
    }
    
    // 第二次挥手
    SendData = { 0 };
    SendData.ack = true;
    SendData.acknum = ReceiveData.seqnum + 1;
    Send();

    SendData.fin = true;
    Send();

    ReceiveData = { 0 };
    Receive();
    assert(ReceiveData.ack == true&&ReceiveData.acknum==1);
    OutputFile.close();
    closesocket(ReceiverSocket);
    WSACleanup();
    cout << "成功关闭连接" << endl;
    return 0;
}