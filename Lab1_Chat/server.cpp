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

// 1�����߳����ڳ��������ͻ�������
///2�����߳����ڴ������ӳɹ��Ŀͻ��ˣ�
// ��Ϊÿ�����ӳɹ��Ŀͻ��˽����̣߳�һ���յ��ÿͻ��˵���Ϣ����㲥�������ͻ���


#define MAX_MESSAGE_LEN 128               // ��Ϣ��󳤶�
#define MAX_NAME_LEN  10                  // �û�����󳤶�
#pragma comment(lib,"ws2_32.lib")

struct ClientInfo {
    SOCKET S_Client;                      // �ͻ����׽���
    char Name[MAX_NAME_LEN] = { 0 };      // �ͻ����û���          
};
list<ClientInfo> mylist;                  // ����ά�����������ӵĿͻ���

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
            for (list<ClientInfo>::iterator curr = mylist.begin(); curr != mylist.end(); curr++) {
                if (curr->S_Client == ClientSocket){
                    cout << "�û�" << curr->Name << "���˳�Ⱥ�ģ�" << endl;
                    mylist.erase(curr);
                    break;
                }
            }
            return 0;
        }
        // Message��recv������Ϣ
        string str(Message);
        string pre = str.substr(0, 7);
        if (pre == "$n$ame:") {     // ������Ϣ�����й㲥�����ڷ�������ÿͻ�����
            strcpy(mylist.back().Name, Message + 7);// �������д�������
            cout << "�û�"<< mylist.back().Name<<"�ɹ��������ӣ�" << endl;
            continue;
        }
        
        char MessageInfo[MAX_MESSAGE_LEN + MAX_NAME_LEN + 1] = { 0 }; // MessageInfo = name:Message
        // ������������SOCKET�ҵ�����������
        for (list<ClientInfo>::iterator curr = mylist.begin(); curr != mylist.end(); curr++) {
            if (curr->S_Client == ClientSocket) {
                strcpy(MessageInfo, curr->Name);
                break;
            }
        }
        strcat(MessageInfo, ":");
        strcat(MessageInfo, Message);
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

    /*HANDLE CreateThread(
        LPSECURITY_ATTRIBUTES lpThreadAttributes,//�̰߳�ȫ����
        DWORD dwStackSize,                       //��ջ��С
        LPTHREAD_START_ROUTINE lpStartAddress,   //�̺߳���
        LPVOID lpParameter,                      //�̲߳���
        DWORD dwCreationFlags,                   //�̴߳�������
        LPDWORD lpThreadId                       //�߳�ID
    );*/

    // ����ɺ�ʼlisten���������õ�����ȴ����ӵ�������Ϊ6
    // ʹsocket�������״̬������Զ�������Ƿ���
    if (SOCKET_ERROR == listen(ServerSocket, 6)) {
        cout << "listen�����������󣬾��������Ϣ���£�" << endl;
        cout << WSAGetLastError() << endl;
        WSACleanup();
        return 0;
    }
    cout << "���ڵȴ��ͻ������ӡ���" << endl;

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