#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#define SEND_CHECK 1  
#define RECEIVE_CHECK 0
#define MaxBufferSize 4096

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
#include<vector>
#include <thread>
#include<mutex>// 用于互斥锁
#include<iomanip>
#pragma comment(lib,"ws2_32.lib")
using namespace std;



struct Send_Datagram {
    bool ack, syn, fin, sack;
    uint16_t checksum;//16位校验和
    long long seqnum, acknum, sacknum;
    int DataLen;// DataLen<=MaxBufferSize
    char data[MaxBufferSize] = { 0 };
}SendData;
int Send_DatagramLen = sizeof(Send_Datagram);

struct Receive_Datagram {
    bool ack, syn, fin, sack;
    uint16_t checksum;//16位校验和
    long long seqnum, acknum, sacknum;
    // 不需要数据缓冲区
}ReceiveData;
int Receive_DatagramLen = sizeof(Receive_Datagram);

FILE* p = nullptr;
char filepath[40] = { 0 };
char filename[40] = { 0 };
const int SenderPort = 5000;
const int RouterPort = 6000;

int WindowSize;// 窗口大小
int timeout = 1500;// 超时时间
int BackCounter = 0;//记录回退N步的次数
SOCKET SenderSocket = { 0 };
SOCKADDR_IN Receiver_addr;
SOCKADDR_IN Sender_addr;
vector<Send_Datagram> v;//用来保存数据包序列
bool* flag_cache;// 指示某个包是否被缓存

// 多线程共享数据,写数据的时候用互斥锁
int Flag_Resend = 0;// 表示recvfrom的数据是否正确或者超时
int Flag_Timer = 0;// 表示是否需要启用定时器服务
int SeqNumber = 0;// 记录一共有多少个数据包,seqnum:0--SeqNumber-1
long long Base = 0;// 初始序列号从0开始
long long NextSeq = 0;

// 互斥锁
std::mutex mtx;

void GetFileName(char* FilePath, char* FileName) {
    int last = 0, j = 0;
    for (int i = 0; FilePath[i] != '\0'; i++) {
        if (FilePath[i] == '/') {
            last = i;
        }
    }
    last++;
    for (; FilePath[j + last] != '\0'; j++) {
        FileName[j] = FilePath[j + last];
    }
    FileName[j] = '\0';
}

void init() {
    WSADATA wsaData = { 0 };
    int err;
    err = WSAStartup(MAKEWORD(2, 2), &wsaData);
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
    cout << "请输入滑动窗口大小:";
    cin >> WindowSize;
}

uint16_t Checksum(Send_Datagram d, int flag) {
    unsigned short* check = (unsigned short*)&d;//两字节为一组来处理
    int count = Send_DatagramLen / sizeof(short);// 一个数据报有 count 组
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

// 利用函数重载
uint16_t Checksum(Receive_Datagram d, int flag) {
    unsigned short* check = (unsigned short*)&d;//两字节为一组来处理
    int count = Receive_DatagramLen / sizeof(short);// 一个数据报有 count 组
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

void Send() {
    SendData.checksum = Checksum(SendData, SEND_CHECK);// 计算校验和
    if (SOCKET_ERROR == sendto(SenderSocket, (char*)&SendData, Send_DatagramLen, 0,
        (struct sockaddr*)&Receiver_addr, sizeof(Receiver_addr))) {
        cout << "发送失败！" << endl;
    }
}



bool Shakehand() {
    SendData = { 0 };
    SendData.syn = true;
    // 夹带私货，把一共有多少个数据包传过去
    memcpy(SendData.data, &SeqNumber, sizeof(SeqNumber));
    Send();//第一次握手

    // 等待回应
    ReceiveData = { 0 };
    int len = sizeof(Receiver_addr);
    bool flag = recvfrom(SenderSocket, (char*)&ReceiveData, Receive_DatagramLen, 0, (struct sockaddr*)&Receiver_addr, &len);
    if (flag == false || ReceiveData.ack == false || ReceiveData.syn == false || ReceiveData.acknum != 1) {
        return false;// 未收到正确的SYN+ACK包
    }

    // 配置发送数据
    SendData = { 0 };
    SendData.ack = true;
    SendData.seqnum = 1;
    SendData.acknum = 1;
    strcpy(SendData.data, filename);
    SendData.DataLen = strlen(filename);
    Send();//第三次握手,同时夹带点私货，把文件名传过去
    // 将套接字设置为非阻塞
    u_long mode = 1;  // 1 = 非阻塞模式, 0 = 阻塞模式
    if (ioctlsocket(SenderSocket, FIONBIO, &mode) == SOCKET_ERROR) {
        cout << "设置接收非阻塞失败！" << endl;
    }
    return true;
}

bool Wavehand() {
    // 先回到阻塞状态
    u_long mode = 0; // 0表示阻塞模式
    if (SOCKET_ERROR == ioctlsocket(SenderSocket, FIONBIO, &mode)) {
        cout << "设置接收阻塞失败！" << endl;
        return false;
    }

    SendData = { 0 };
    SendData.fin = true;
    SendData.ack = true;
    Send();//第一次挥手
    ReceiveData = { 0 };
    int len = sizeof(Receiver_addr);
    bool flag = recvfrom(SenderSocket, (char*)&ReceiveData, Receive_DatagramLen, 0, (struct sockaddr*)&Receiver_addr, &len);
    if (flag == false || ReceiveData.ack == false || ReceiveData.acknum != 1) {
        return false;
    }
    ReceiveData = { 0 };
    flag = recvfrom(SenderSocket, (char*)&ReceiveData, Receive_DatagramLen, 0, (struct sockaddr*)&Receiver_addr, &len);
    if (flag == false || ReceiveData.ack == false || ReceiveData.fin == false || ReceiveData.acknum != 1) {
        return false;
    }
    SendData = { 0 };
    SendData.ack = true;
    SendData.acknum = 1;
    Send();
    return true;
}


void Timer() {// 持续监测Flag_Timer,只要Flag_Timer=1就对Base包启动定时器
    while (true) {
        if (Base == SeqNumber) {//传输完毕
            cout << "[提示]：Timer线程正确结束" << endl;
            return;// 结束进程
        }
        clock_t start, end;
        if (Flag_Timer == 1) {
            int len = sizeof(Receiver_addr);
            start = clock();
            while (true) {
                int recv = recvfrom(SenderSocket, (char*)&ReceiveData, Receive_DatagramLen, 0, (struct sockaddr*)&Receiver_addr, &len);
                int check = Checksum(ReceiveData, RECEIVE_CHECK);
                // recvfrom非阻塞，如果接收到的数据无误且ack大于Base,则移动窗口左边界，用队列来实现
                // 同时主线程监测到NextSeq<Base+WindowSize，传输新增的右边界窗口，从而实现滑动窗口的功能
                if (recv != -1 && ReceiveData.acknum >= Base && (check ^ ReceiveData.checksum) == 0xffff && ReceiveData.ack == true) {
                    cout << "收到acknum为" << ReceiveData.acknum << "的回复" << endl;
                    lock_guard<mutex> lock(mtx);
                    Base = ReceiveData.acknum + 1;
                    break;//跳出内层循环后，因为Flag_Timer仍然为1，会为新的Base设置定时器
                }
                if (recv != -1 && (check ^ ReceiveData.checksum) == 0xffff && ReceiveData.sack == true) {
                    // 接收端将已经缓存的失序数据包通知给发送端
                    flag_cache[ReceiveData.sacknum] = 1;
                }
                end = clock();
                if (end - start >= timeout) {// 超时
                    lock_guard<mutex> lock(mtx);
                    Flag_Resend = 1;// 写操作使用互斥锁,通知Resend重传整个窗口
                    Flag_Timer = 0;// 重传的时候再设置为1
                    break;//跳出内层循环,此时因为Flag_Timer为0，不需要再监测，等待Resend重发Base包的时候设置Flag_Timer
                }
            }
        }
    }
}

void Resend() {// 只要检测到Flag_Resend变为1就重发Base--(NextSeq-1)
    while (true) {
        if (Flag_Resend == 1) {
            BackCounter++;
            int curr = 0;
            for (int i = Base; i <= NextSeq - 1; i++) {
                // 首先根据失序数据包序列判断是否需要重发
                if (flag_cache[i]) continue;
                SendData = v[i];
                Send();
                if (i == Base) {//Base包
                    lock_guard<mutex> lock(mtx);
                    Flag_Timer = 1;//启动定时，通知Receive线程
                }
            }
            {
                lock_guard<mutex> lock(mtx);
                Flag_Resend = 0;
            }
        }
        if (Base == SeqNumber) {//传输完毕
            cout << "[提示]：ReSend线程正确结束" << endl;
            return;// 结束进程
        }
    }
}

int main() {
    clock_t Send_Start, Send_End;
    init();
    cout << "请输入待传输文件的路径：";
    cin >> filepath;
    GetFileName(filepath, filename);
    // 打开文件
    if (!(p = fopen(filepath, "rb+"))) {
        cout << "打开文件失败！" << endl;
        return 0;
    }

    // 移动文件指针到文件末尾
    fseek(p, 0, SEEK_END);
    long long FileLen = ftell(p);
    fseek(p, 0, SEEK_SET);
    long long temp = FileLen;
    // 数据以4096字节为单位放入vector序列

    while (temp > 0) {
        SendData = { 0 };
        fread(SendData.data, MaxBufferSize, 1, p);
        SendData.DataLen = temp < MaxBufferSize ? temp : MaxBufferSize;
        SendData.seqnum = SeqNumber++;
        v.push_back(SendData);
        temp -= SendData.DataLen;
    }
    if (Shakehand()) {
        cout << "建立连接成功！" << endl;
    }
    else {
        cout << "建立连接失败，请重启程序继续传输！" << endl;
        return 0;
    }
    flag_cache = new bool[SeqNumber];
    for (int i = 0; i < SeqNumber; i++) {
        flag_cache[i] = 0;
    }
    cout << "开始传输！" << endl;
    thread resend(Resend);//用来重发Base--(NextSeq-1)
    thread wait(Timer);
    Send_Start = clock();
    while (true) {// 主进程不需要考虑SACK的问题
        // 主线程main负责调整窗口右边界的变化，即持续判断NextSeq是否小于Base + WindowSize
        // 子线程Resend负责持续检测Flag_Resend标志是否为0（是否“超时”），“超时”则重传
        // 这里的“超时”指的是在指定时间内没有收到正确的acknum（即acknum>=Base)
        // 子线程Timer负责为Base包开启定时器，一旦超时即设置Flag_Resend标志为1
        if (NextSeq == SeqNumber) {// 发送完毕
            break;
        }
        if (NextSeq < Base + WindowSize) {
            SendData = v[NextSeq];
            Send();
            if (NextSeq == Base) {// 第一个包
                lock_guard<mutex> lock(mtx);
                Flag_Timer = 1;
            }
            {
                lock_guard<mutex> lock(mtx);
                NextSeq++;
            }
            Sleep(100);//每次发送睡眠100毫秒，避免短时间内发送大量数据包给接收方
        }
    }
    resend.join();//阻塞等待
    wait.join();//阻塞等待
    Send_End = clock();
    cout << "文件传输完毕！" << endl;
    cout << "--------------------—————————----—————" << endl;
    cout << "文件总大小为：" << FileLen << " Bytes" << endl;
    cout << "传输性能指标如下：" << endl;
    float DuringTime = (Send_End - Send_Start) / 1000.0;
    cout << "传输总用时：" << DuringTime << "s" << endl;
    cout << fixed << setprecision(3);
    cout << "传输吞吐率(包含协议开销）：" << float(Send_DatagramLen * SeqNumber * 8 / (1000000 * DuringTime)) << "Mbps" << endl;
    cout << "其中回退N步的次数为：" << BackCounter << endl;
    if (Wavehand()) {
        cout << "成功关闭连接！" << endl;
    }
    else {
        cout << "关闭连接失败！" << endl;
    }
    fclose(p);// 关闭文件
    closesocket(SenderSocket);
    WSACleanup();
    delete[]flag_cache;
    return 0;
}


