#define _WINSOCK_DEPRECATED_NO_WARNINGS   // ���ⱨ��
#define _CRT_SECURE_NO_WARNINGS

#include <WinSock2.h>
#include <stdio.h>
#include <stdlib.h>
#include<iostream>
#include<string.h>
#include<Windows.h>
#include<list>
#include <chrono>
#include <iomanip>
#include <sstream>
using namespace std;

// 1�����߳����ڳ��������ͻ�������
///2�����߳����ڴ������ӳɹ��Ŀͻ��ˣ�
// ��Ϊÿ�����ӳɹ��Ŀͻ��˽����̣߳�һ���յ��ÿͻ��˵���Ϣ����㲥�������ͻ���


#define MAX_MESSAGE_LEN  128               // ��Ϣ��󳤶�
#define MAX_NAME_LEN     10                // �û�����󳤶�
#define MAX_TIME_LEN     20                // ��ʱ��Ԥ��20���ַ�����
#pragma comment(lib,"ws2_32.lib")

struct ClientInfo {
    SOCKET S_Client;                      // �ͻ����׽���
    char Name[MAX_NAME_LEN] = { 0 };      // �ͻ����û���          
};
list<ClientInfo> mylist;                  // ����ά�����������ӵĿͻ���

const char* GetTimeNow() {                      // ��ȡ��ǰʱ��
    auto now = chrono::system_clock::now();
    time_t now_time = chrono::system_clock::to_time_t(now);
    tm* tm_now = localtime(&now_time);
    stringstream ss;
    ss << put_time(tm_now, "%Y-%m-%d %H:%M:%S"); // �Զ���ʱ���ʽ
    string time = ss.str();
    const char* temp = time.c_str();
    char* ti = new char[strlen(temp) + 1];
    strcpy(ti, temp);
    return ti;
}

DWORD WINAPI AcceptThread(LPVOID param) {// LPVOID param�����������̴߳������̵߳Ĳ������˴�ΪClientSocket
    /*���ոÿͻ��˷��͵���Ϣ��
      ���յ�����Ϣ�㲥�����������ӵĿͻ��ˡ�
      ��⵽�ͻ��˶Ͽ�����ʱ��������Դ�������̡߳�*/

    char Message[MAX_MESSAGE_LEN] = { 0 };
    SOCKET ClientSocket = (SOCKET)param;  // ����ת�����õ����̴߳��ݵĲ���,ClientSocketΪ�������ӵ�SOCKET
    while (true) {
        if (SOCKET_ERROR == recv(ClientSocket, Message, sizeof(Message), 0)) {
            cout << "recv�����������󣬾��������Ϣ���£�" << endl;
            cout << WSAGetLastError() << endl;
            break;
        }
        if (strcmp(Message, "q") == 0) {
            // ֪ͨ�ͻ����ӽ��̣�����receive
            if (SOCKET_ERROR == send(ClientSocket, Message, sizeof(Message), 0)) {
                cout << "send�����������󣬾��������Ϣ���£�" << endl;
                cout << WSAGetLastError() << endl;
            }
            // ��������ɾ���ÿͻ���
            char ExitInfo[MAX_NAME_LEN+14] = { 0 };
            strcpy(ExitInfo, "�û�");
            for (list<ClientInfo>::iterator curr = mylist.begin(); curr != mylist.end(); curr++) {
                if (curr->S_Client == ClientSocket) {
                    cout << "�û�" << curr->Name << "�ѶϿ����ӣ�" << endl;
                    strcat(ExitInfo,curr->Name);
                    mylist.erase(curr);
                    break;
                }
            }
            strcat(ExitInfo, "�˳����죡");
            // �㲥�������û����û����˳�����
            for (list<ClientInfo>::iterator curr = mylist.begin(); curr != mylist.end(); curr++) {
                if (SOCKET_ERROR == send(curr->S_Client, ExitInfo, sizeof(ExitInfo), 0)) {
                    cout << "send�����������󣬾��������Ϣ���£�" << endl;
                    cout << WSAGetLastError() << endl;
                    WSACleanup();
                    return 0;
                }
            }
            return 0; // ���û����˳��������ý���
        }
        // Message��recv������Ϣ
        string str(Message);
        string pre = str.substr(0, 7);
        if (pre == "$n$ame:") {     // ����������û�����
            strcpy(mylist.back().Name, Message + 7);// �������д�������
            cout << "�û�" << mylist.back().Name << "�ɹ��������ӣ�" << endl;

            char to_other[MAX_NAME_LEN+14] = { 0 };                // ���¼�����û���Ϣ�㲥�������û�
            char to_self[100] = { 0 };                             // ���ڴ洢�����ߵ��û������ظ��¼�����û�
            
            strcpy(to_other, "�û�");
            strcat(to_other, mylist.back().Name);
            strcat(to_other, "�������죡");             // ���û�xxx�������족���͸������û�
            strcpy(to_self, "��ǰ�����û���");

            list<ClientInfo>::iterator self = mylist.begin();
            for (list<ClientInfo>::iterator curr = mylist.begin(); curr != mylist.end(); curr++) {
                if (curr->S_Client != ClientSocket) { // ����������Ϣ����
                    if (SOCKET_ERROR == send(curr->S_Client, to_other, sizeof(to_other), 0)) {
                        cout << "send�����������󣬾��������Ϣ���£�" << endl;
                        cout << WSAGetLastError() << endl;
                        WSACleanup();
                        return 0;
                    }
                }
                else {
                    self = curr;
                } 
                strcat(to_self, curr->Name);   // �����û�������
                strcat(to_self, "  ");
            }
            // �����¼�����û�to_self
            if (SOCKET_ERROR == send(self->S_Client, to_self, sizeof(to_self), 0)) {
                cout << "send�����������󣬾��������Ϣ���£�" << endl;
                cout << WSAGetLastError() << endl;
                WSACleanup();
                return 0;
            }
            continue;
        }

        char MessageInfo[MAX_MESSAGE_LEN + MAX_NAME_LEN + MAX_TIME_LEN + 2] = { 0 }; // MessageInfo = time  name:Message
        strcpy(MessageInfo, GetTimeNow());
        strcat(MessageInfo, "\n");
        // ������������SOCKET�ҵ�����������
        for (list<ClientInfo>::iterator curr = mylist.begin(); curr != mylist.end(); curr++) {
            if (curr->S_Client == ClientSocket) {
                strcat(MessageInfo, curr->Name);
                break;
            }
        }
        strcat(MessageInfo, ":");
        strcat(MessageInfo, Message);
        cout << MessageInfo << endl;
        // ����Ϣ���й㲥
        for (list<ClientInfo>::iterator curr = mylist.begin(); curr != mylist.end(); curr++) {
            if (curr->S_Client != ClientSocket) { // ����������Ϣ����
                if (SOCKET_ERROR == send(curr->S_Client, MessageInfo, sizeof(MessageInfo), 0)) {
                    cout << "send�����������󣬾��������Ϣ���£�" << endl;
                    cout << WSAGetLastError() << endl;
                    WSACleanup();
                    return 0;
                }
            }
        }
    }
    exit(0);// �˳��ý���
}

int main() {
    cout << "��ӭ���������ҷ���˽��棡" << endl;
    WSADATA wsaData = { 0 };                          // ����׽�����Ϣ
    SOCKET ServerSocket = INVALID_SOCKET;             // �׽��ֳ�ʼ��Ϊ��Ч���׽��־��
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
    ServerSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ServerSocket == INVALID_SOCKET) {                       // ������
        cout << "socket�����������󣬾��������Ϣ���£�" << endl;
        cout << WSAGetLastError() << endl;
        WSACleanup();
        return 0;
    }

    SOCKADDR_IN Server_addr;
    memset(&Server_addr, 0x0, sizeof(Server_addr));             // ����
    USHORT Port = 8080;

    Server_addr.sin_family = AF_INET;                           // ����IPv4Э��
    Server_addr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);       // INADDR_ANY������������IP
    Server_addr.sin_port = htons(Port);                         // ���ö˿ں�     


    // ��һ�� IP ��ַ��˿ں���һ���׽��ֽ��а�
    if (SOCKET_ERROR == bind(ServerSocket, (SOCKADDR*)&Server_addr, sizeof(Server_addr))) {
        cout << "bind�����������󣬾��������Ϣ���£�" << endl;
        cout << WSAGetLastError() << endl;
        WSACleanup();
        return 0;
    }
    // ����ɺ�ʼlisten���������õ�����ȴ����ӵ�������Ϊ6
    // ʹsocket�������״̬������Զ�������Ƿ���
    if (SOCKET_ERROR == listen(ServerSocket, 6)) {
        cout << "listen�����������󣬾��������Ϣ���£�" << endl;
        cout << WSAGetLastError() << endl;
        WSACleanup();
        return 0;
    }
    cout << "���ڵȴ��ͻ������ӡ���" << endl;
    cout << endl;

    while (true) {// ���̳߳���������accept���µĿͻ���������Ϊ���Ǵ����µ��߳�
        SOCKADDR_IN Client_addr;                            // ���ڷ��ط�������Ŀͻ��˵ĵ�ַ
        memset(&Client_addr, 0x0, sizeof(Client_addr));     // ��ʼ������
        int ClientAddrLen = sizeof(Client_addr);            // ���ڷ��ص�ַ���� 

        // ��ȡ�ͻ��˵��������󲢽������ӣ������µ�socket�׽���(��Ȳ͹���İ���)
        SOCKET ClientSocket = accept(ServerSocket, (SOCKADDR*)&Client_addr, &ClientAddrLen);
        if (INVALID_SOCKET == ClientSocket) {
            cout << "accept�����������󣬾��������Ϣ���£�" << endl;
            cout << WSAGetLastError() << endl;
        }
        else {// �ɹ����Ӻ����µ��߳�
            ClientInfo c;
            c.S_Client = ClientSocket;
            mylist.push_back(c);      //���뵽������
            // ���µ��׽��ִ����߳�
            HANDLE acceptthread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)AcceptThread, LPVOID(ClientSocket), 0, 0);
        }
    }
    // ����Winsock��Դ
    WSACleanup();
    return 0;
}