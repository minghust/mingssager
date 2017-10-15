#pragma comment(lib, "ws2_32.lib")

#include <stdio.h>
#include <winsock2.h>
#include <list>
#include <algorithm>
#include <string.h>
#include <map>
#include "login.h"

const int MAXCONN = 5;
const int BUFLEN = 255;
const char USER_LOGIN[] = "USER_LOGIN";
const char USER_REGISTER[] = "USER_REGISTER";
const char USER_GETPASSWDBACK[] = "USER_GETPASSWDBACK";
const char end[] = "END";

void SplitString(const string& s, vector<string>& v, const string& c);

using namespace std;
typedef list<SOCKET> ListCONN;
typedef list<SOCKET> ListConErr;


void main(int argc, char* argv[])
{
	WSADATA wsaData;
	int nRC;
	sockaddr_in srvAddr, clientAddr;
	SOCKET srvSock;
	int nAddrLen = sizeof(sockaddr);
	char sendBuf[BUFLEN], recvBuf[BUFLEN];
	ListCONN conList;		//����������Ч�ĻỰSOCKET
	ListCONN::iterator itor;
	ListConErr conErrList;	//��������ʧЧ�ĻỰSOCKET
	ListConErr::iterator itor1;
	FD_SET rfds, wfds;
	u_long uNonBlock;

	map<string, string>offlineBuf; // offline message

	//��ʼ�� winsock
	nRC = WSAStartup(0x0101, &wsaData);
	if (nRC)
	{
		printf("Server initialize winsock error!\n");
		return;
	}
	if (wsaData.wVersion != 0x0101)
	{
		printf("Server's winsock version error!\n");
		WSACleanup();
		return;
	}
	printf("Server's winsock initialized !\n");

	//���� TCP socket
	srvSock = socket(AF_INET, SOCK_STREAM, 0);
	if (srvSock == INVALID_SOCKET)
	{
		printf("Server create socket error!\n");
		WSACleanup();
		return;
	}
	printf("Server TCP socket create OK!\n");

	//�� socket to Server's IP and port 5050
	srvAddr.sin_family = AF_INET;
	srvAddr.sin_port = htons(5050);
	srvAddr.sin_addr.S_un.S_addr = INADDR_ANY;
	nRC = bind(srvSock, (LPSOCKADDR)&srvAddr, sizeof(srvAddr));
	if (nRC == SOCKET_ERROR)
	{
		printf("Server socket bind error!\n");
		closesocket(srvSock);
		WSACleanup();
		return;
	}
	printf("Server socket bind OK!\n");

	//��ʼ�������̣��ȴ��ͻ�������
	nRC = listen(srvSock, MAXCONN);
	if (nRC == SOCKET_ERROR)
	{
		printf("Server socket listen error!\n");
		closesocket(srvSock);
		WSACleanup();
		return;
	}

	//��srvSock��Ϊ������ģʽ�Լ����ͻ���������
	uNonBlock = 1;
	ioctlsocket(srvSock, FIONBIO, &uNonBlock);

	while (1)
	{
		//��conList��ɾ���Ѿ���������ĻỰSOCKET
		for (itor1 = conErrList.begin(); itor1 != conErrList.end(); itor1++)
		{
			itor = find(conList.begin(), conList.end(), *itor1);
			if (itor != conList.end()) conList.erase(itor);
		}

		//���read,write�׽��ּ���
		FD_ZERO(&rfds);
		FD_ZERO(&wfds);

		//���õȴ��ͻ���������
		FD_SET(srvSock, &rfds);

		for (itor = conList.begin(); itor != conList.end(); itor++)
		{
			//�����лỰSOCKET��Ϊ������ģʽ
			uNonBlock = 1;
			ioctlsocket(*itor, FIONBIO, &uNonBlock);
			//���õȴ��ỰSOKCET�ɽ������ݻ�ɷ�������
			FD_SET(*itor, &rfds);
			FD_SET(*itor, &wfds);
		}
		//��ʼ�ȴ�
		int nTotal = select(0, &rfds, &wfds, NULL, NULL);

		//���srvSock�յ��������󣬽��ܿͻ���������
		if (FD_ISSET(srvSock, &rfds))
		{
			nTotal--;
			//�����ỰSOCKET
			SOCKET connSock = accept(srvSock, (LPSOCKADDR)&clientAddr, &nAddrLen);
			if (connSock == INVALID_SOCKET)
			{
				printf("Server accept connection request error!\n");
				closesocket(srvSock);
				WSACleanup();
				return;
			}
			sprintf(sendBuf, "%s",inet_ntoa(clientAddr.sin_addr));
			printf("%s: ", sendBuf);
			printf("%u\n", clientAddr.sin_port);

			//�������ĻỰSOCKET������conList��
			conList.insert(conList.end(), connSock);
		}
		if (nTotal > 0)
		{
			//���������Ч�ĻỰSOCKET�Ƿ������ݵ���
			//���Ƿ���Է�������
			for (itor = conList.begin(); itor != conList.end(); itor++)
			{
				//����ỰSOCKET���Է������ݣ�
				//����ͻ�������Ϣ
				//if (FD_ISSET(*itor, &wfds))
				//{
				//	//������ͻ����������ݣ�����
				//	if (strlen(sendBuf) > 0)
				//	{
				//		nRC = send(*itor, sendBuf, strlen(sendBuf), 0);
				//		if (nRC == SOCKET_ERROR)
				//		{
				//			//�������ݴ���
				//			//��¼�²�������ĻỰSOCKET
				//			conErrList.insert(conErrList.end(), *itor);
				//		}
				//		else//�������ݳɹ�����շ��ͻ�����
				//			memset(sendBuf, '\0', BUFLEN);
				//	}
				//}

				//����ỰSOCKET�����ݵ���������ܿͻ�������
				if (FD_ISSET(*itor, &rfds))
				{
					nRC = recv(*itor, recvBuf, BUFLEN, 0);
					if (nRC == SOCKET_ERROR)
					{
						//�������ݴ���
						//��¼�²�������ĻỰSOCKET
						conErrList.insert(conErrList.end(), *itor);
					}
					else
					{
						//�������ݳɹ��������ڷ��ͻ������У�
						//�Է��͵����пͻ�ȥ
						if (strcmp(recvBuf, USER_LOGIN) == 0)
						{
							sprintf(sendBuf, "%s", "PREPARE_LOGIN");
							send(*itor, sendBuf, strlen(sendBuf), 0);
							recv(*itor, recvBuf, BUFLEN, 0);
							string str(recvBuf);
							vector<string> v;
							SplitString(str, v, "+");
							int re = VerifyUserLogin(v[0], v[1]);
							if (re == LOGIN_SUCCESS)
							{
								sprintf(sendBuf, "%s", "LOGIN_SUCCESS");
								send(*itor, sendBuf, strlen(sendBuf), 0);
							}
							else if (re == PSW_ERROR)
							{
								sprintf(sendBuf, "%s", "PSW_ERROR");
								send(*itor, sendBuf, strlen(sendBuf), 0);
							}
							else if (re == INVALID_USERNAME)
							{
								sprintf(sendBuf, "%s", "INVALID_USERNAME");
								send(*itor, sendBuf, strlen(sendBuf), 0);
							}
							else
							{
								sprintf(sendBuf, "%s", "FILE_ERROR");
								send(*itor, sendBuf, strlen(sendBuf), 0);
							}
						}
						else if (strcmp(recvBuf, USER_REGISTER) == 0)
						{

						}
						else if (strcmp(recvBuf, USER_GETPASSWDBACK) == 0)
						{

						}
					}
				}
			}
		}
	}
	closesocket(srvSock);
	WSACleanup();
}

void SplitString(const string& s, vector<string>& v, const string& c)
{
	string::size_type pos1, pos2;
	pos2 = s.find(c);
	pos1 = 0;
	while (string::npos != pos2)
	{
		v.push_back(s.substr(pos1, pos2 - pos1));

		pos1 = pos2 + c.size();
		pos2 = s.find(c, pos1);
	}
	if (pos1 != s.length())
		v.push_back(s.substr(pos1));
}