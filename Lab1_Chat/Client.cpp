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

#define MAX_MESSAGE_LEN 128               // ��Ϣ��󳤶�
#define MAX_NAME_LEN    10                // �û�����󳤶�
#define MAX_TIME_LEN    20

// �ͻ������߳����ڷ�����Ϣ
// ���߳����ڽ��շ���˵���Ϣ
// �˳�Ⱥ��ʱ���߳�����q���͸�����ˣ�������ٷ��͸������̣߳������̵߳õ��󼴽��������߳�


DWORD WINAPI ReceiveThread(LPVOID param) {                                               // �������շ��������͵���Ϣ
    char ReceiveMessage[MAX_MESSAGE_LEN + MAX_NAME_LEN + MAX_TIME_LEN + 3] = { 0 };      // ������󳤶Ⱥ���Ϣ��󳤶Ⱥ�ð��
    SOCKET ClientSocket = (SOCKET)param;                                                 // ����ת�����õ����̴߳��ݵĲ���
    while (true) {
        if (SOCKET_ERROR == recv(ClientSocket, ReceiveMessage, sizeof(ReceiveMessage), 0)) {
            cout << "receive����ʧ��" << endl;
            cout << WSAGetLastError() << endl;
            break;
        }
        if (strcmp(ReceiveMessage, "q") == 0) {                              // �ͻ�����q�˳�ʱ������˻ᷢ��q�����ս��̣��رոý���
            return 0;
        }
        cout << ReceiveMessage << endl << endl;
    }
    return 0;// �����˳�
}


int main() {
    cout << "��ӭ���������ҿͻ��˽��棡" << endl;
    WSADATA wsaData = { 0 };                          // ����׽�����Ϣ
    SOCKET ClientSocket = INVALID_SOCKET;             // �׽��ֳ�ʼ��Ϊ��Ч���׽��־��
    SOCKADDR_IN Server_addr;
    memset(&Server_addr, 0x0, sizeof(Server_addr)); // ����
    USHORT Port = 8080;//����˶˿�

    // ÿһ������Ҫ��ɴ�����

    // ��ʼ��Winsock������汾2.2
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("Winsock��ʼ��ʧ�ܣ�\n");
        WSACleanup();
        return 0;
    }

    // socket()���ڴ���һ������ͨ�Ŷ˵�
    // AF_INET������IPv4��ѡ��AF_INET����
    // SOCK_STREAM��������ʽ����SOCK����STREAM
    // 0��ϵͳĬ��ѡ��Э��
    ClientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (ClientSocket == INVALID_SOCKET) {              // ������
        cout << "socket�����������󣬾��������Ϣ���£�" << endl;
        cout << WSAGetLastError() << endl;
        WSACleanup();
        return 0;
    }

    // �ͻ��˲���Ҫbind������ϵͳ���Զ�Ϊ�ͻ���ѡ��һ���ʵ���IP��һ���������ʱ�˿ںš�

    Server_addr.sin_family = AF_INET;                            // ����IPv4Э��
    Server_addr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");   // ����IP��ַ
    Server_addr.sin_port = htons(Port);                          // ���ö˿ں� 

    char name[MAX_NAME_LEN] = { 0 }, nameinfo[MAX_NAME_LEN + 7] = { 0 };                 // nameinfo = "$n$ame:" + name
    cout << "�����������û�����������10���֣���";
    cin.getline(name, 10);

    // ���ӷ�����
    if (SOCKET_ERROR == connect(ClientSocket, (SOCKADDR*)&Server_addr, sizeof(Server_addr))) {
        cout << "connect�����������󣬾��������Ϣ���£�" << endl;
        cout << WSAGetLastError() << endl;
        WSACleanup();
        return 0;
    }
    // ��ʾ��Ϣ
    cout << "���ѽ���Ⱥ�ģ��������������ݣ�" << endl;
    cout << "��Ҫ�˳���������q" << endl;
    strcpy(nameinfo, "$n$ame:");
    strcat(nameinfo, name);
    if (SOCKET_ERROR == send(ClientSocket, nameinfo, sizeof(nameinfo), 0)) {// �Ȱ����ִ���ȥ
        cout << "send�����������󣬾��������Ϣ����" << endl;
        cout << WSAGetLastError() << endl;
        WSACleanup();
        return 0;
    }

    // ��������
    // ReceiveThread�����̺߳���
    // ClientSocket�����ݸ��̺߳����Ĳ���
    HANDLE recvthread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ReceiveThread, LPVOID(ClientSocket), 0, 0);
    CloseHandle(recvthread);   // �رվ�����Ա�����Դй¶,�����̼���ִ��
    char chat[MAX_MESSAGE_LEN] = { 0 };
    while (true) {
        cin.getline(chat, 128);
        cout << endl;
        if (SOCKET_ERROR == send(ClientSocket, chat, sizeof(chat), 0)) {
            cout << "send�����������󣬾��������Ϣ����" << endl;
            cout << WSAGetLastError() << endl;
            WSACleanup();
            return 0;
        }
        if (strcmp(chat, "q") == 0) {
            cout << "���������" << endl;
            closesocket(ClientSocket);
            // ����Winsock��Դ
            WSACleanup();
            break;
        }
    }
    return 0;
}