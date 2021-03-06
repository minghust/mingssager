#pragma comment(lib, "ws2_32.lib")

#include <stdio.h>
#include <winsock2.h>
#include <list>
#include <algorithm>
#include <string.h>
#include <windows.h>
#include <map>
#include <sstream>
#include "login.h"

using namespace std;
const int MAXCONN = 10;
const int BUFLEN = 500;
const string LOGIN = "login";
const string REGISTER = "register";
const string GETPASSWDBACK = "getpasswdback";
const string GETUSERLIST = "getuserlist";
const string OFFLINE = "offline";
const string UPDATE = "update";
const string MESSAGE = "message";

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
	ListCONN conList;		//保存所有有效的会话SOCKET
	ListCONN::iterator itor;
	ListConErr conErrList;	//保存所有失效的会话SOCKET
	ListConErr::iterator itor1;
	FD_SET rfds;
	u_long uNonBlock;

	vector<UserList>userList;
	vector<OfflineMessage>offlineBuf;
	//初始化 winsock
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

	//创建 TCP socket
	srvSock = socket(AF_INET, SOCK_STREAM, 0);
	if (srvSock == INVALID_SOCKET)
	{
		printf("Server create socket error!\n");
		WSACleanup();
		return;
	}
	printf("Server TCP socket create OK!\n");

	//绑定 socket to Server's IP and port 60000
	srvAddr.sin_family = AF_INET;
	srvAddr.sin_port = htons(60000);
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

	//开始监听过程，等待客户的连接
	nRC = listen(srvSock, MAXCONN);
	if (nRC == SOCKET_ERROR)
	{
		printf("Server socket listen error!\n");
		closesocket(srvSock);
		WSACleanup();
		return;
	}

	//将srvSock设为非阻塞模式以监听客户连接请求
	uNonBlock = 1;
	ioctlsocket(srvSock, FIONBIO, &uNonBlock);

	while (1)
	{
		//从conList中删除已经产生错误的会话SOCKET
		for (itor1 = conErrList.begin(); itor1 != conErrList.end(); itor1++)
		{
			itor = find(conList.begin(), conList.end(), *itor1);
			if (itor != conList.end()) conList.erase(itor);
		}

		//清空read,write套接字集合
		FD_ZERO(&rfds);

		//设置等待客户连接请求
		FD_SET(srvSock, &rfds);

		for (itor = conList.begin(); itor != conList.end(); itor++)
		{
			//把所有会话SOCKET设为非阻塞模式
			uNonBlock = 1;
			ioctlsocket(*itor, FIONBIO, &uNonBlock);
			//设置等待会话SOKCET可接受数据或可发送数据
			FD_SET(*itor, &rfds);
		}
		//开始等待
		int nTotal = select(0, &rfds, NULL, NULL, NULL);

		//如果srvSock收到连接请求，接受客户连接请求, 用于测试srvSock是否在rfds中
		if (FD_ISSET(srvSock, &rfds))
		{
			nTotal--;
			//产生会话SOCKET
			SOCKET connSock = accept(srvSock, (LPSOCKADDR)&clientAddr, &nAddrLen);
			if (connSock == INVALID_SOCKET)
			{
				printf("Server accept connection request error!\n");
				closesocket(srvSock);
				WSACleanup();
				return;
			}
			sprintf(sendBuf, "%s", inet_ntoa(clientAddr.sin_addr));
			printf("%s: ", sendBuf);
			printf("%u has connected to server\n", clientAddr.sin_port);

			//将产生的会话SOCKET保存在conList中
			conList.insert(conList.end(), connSock);
		}
		if (nTotal > 0)
		{
			//检查所有有效的会话SOCKET是否有数据到来
			//或是否可以发送数据
			for (itor = conList.begin(); itor != conList.end();)
			{
				//如果会话SOCKET有数据到来，则接受客户的数据
				if (FD_ISSET(*itor, &rfds))
				{
					nRC = recv(*itor, recvBuf, BUFLEN, 0);
					if (nRC == SOCKET_ERROR)
					{
						//接受数据错误，
						//记录下产生错误的会话SOCKET
						conErrList.insert(conErrList.end(), *itor);
					}
					else
					{
						vector<string>op;
						string str(recvBuf);
						SplitString(str, op, "!");
						// 接收数据成功，保存在发送缓冲区中，
						// 以发送到所有客户去
						if (op[0] == LOGIN)
						{
							cout << "recvBuf: " << op[1] << endl;
							vector<string> v;
							SplitString(op[1], v, "+");
							memset(sendBuf, '\0', BUFLEN);
							int re = VerifyUserLogin(v[0], v[1]);
							if (re == LOGIN_SUCCESS)
							{
								sprintf(sendBuf, "%s", "LOGIN_SUCCESS");
								send(*itor, sendBuf, strlen(sendBuf), 0);
								// send to client userList
								UpdateUserList(userList);
								string s = "";
								for (int i = 0; i < userList.size(); i++)
								{
									if (userList[i].name == v[0])
									{
										userList[i].isOnline = true;
										userList[i].ip = v[2];
										userList[i].port = v[3];
									}
									s += userList[i].name + "+" + userList[i].ip + "+" + userList[i].port + "+";
									s += userList[i].isOnline ? "ON!" : "OFF!";
								}
								s += "%";
								string t = s;
								for (auto it = offlineBuf.begin(); it != offlineBuf.end();)
								{
									if (it->toName == v[0])
									{
										s += it->fromName;
										s = s + "+" + it->msg + "!";
										it = offlineBuf.erase(it);
									}
									else
									{
										it++;
									}
								}
								if (s.length() == t.length())
								{
									s += "none";
								}
								memcpy(sendBuf, s.c_str(), s.length() + 1);
								//sprintf(sendBuf, "%s", s.c_str());
								// sendBuf: name+xxxx+false!name+xxxxx+false!%fromName+msg!fromName+msg
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
							itor = conList.erase(itor);
							if (itor == conList.end()) break;
						}
						else if (op[0] == REGISTER)
						{
							vector<string> v;
							cout << "recvBuf:" << op[1] << endl;
							SplitString(op[1], v, "+");
							memset(sendBuf, '\0', BUFLEN);
							// name + email + passwd
							bool re = SaveInfo(v[0], v[1], v[2]);
							if (re)
							{
								sprintf(sendBuf, "%s", "REGISTER_SUCCESS");
								send(*itor, sendBuf, strlen(sendBuf), 0);
							}
							else
							{
								sprintf(sendBuf, "%s", "REGISTER_FAILURE");
								send(*itor, sendBuf, strlen(sendBuf), 0);
							}
							itor = conList.erase(itor);
							if (itor == conList.end()) break;
						}
						else if (op[0] == GETPASSWDBACK)
						{
							cout << "recvBuf" << op[0] << endl;
							vector<string> v;
							SplitString(op[1], v, "+");
							memset(sendBuf, '\0', BUFLEN);
							bool re = GetbackPasswd(v[0], v[1]);
							if (re)
							{
								sprintf(sendBuf, "%s", "GETPASSWDBACK_SUCCESS");
								send(*itor, sendBuf, strlen(sendBuf), 0);
							}
							else
							{
								sprintf(sendBuf, "%s", "GETPASSWDBACK_FAILURE");
								send(*itor, sendBuf, strlen(sendBuf), 0);
							}
							itor = conList.erase(itor);
							if (itor == conList.end()) break;
						}
						else if (op[0] == OFFLINE)
						{
							for (int i = 0; i < userList.size(); i++)
							{
								if (userList[i].name == op[1])
								{
									userList[i].isOnline = false;
									userList[i].ip = "-.-.-.-";
									userList[i].port = "xxxxx";
								}
							}
							itor = conList.erase(itor);
							if (itor == conList.end()) break;
						}
						else if (op[0] == UPDATE)
						{
							UpdateUserList(userList);
							string s = "";
							vector<string>v;
							SplitString(op[1], v, "+");
							memset(sendBuf, '\0', BUFLEN);
							for (int i = 0; i < userList.size(); i++)
							{
								if (userList[i].name == v[0])
								{
									userList[i].isOnline = true;
									userList[i].ip = v[1];
									userList[i].port = v[2];
								}
								s += userList[i].name + "+" + userList[i].ip + "+" + userList[i].port + "+";
								s += userList[i].isOnline ? "ON!" : "OFF!";
							}
							sprintf(sendBuf, "%s", s.c_str());
							send(*itor, sendBuf, strlen(sendBuf), 0);
							itor = conList.erase(itor);
							if (itor == conList.end()) break;
						}
						else if (op[0] == MESSAGE)
						{
							vector<string>v;
							SplitString(op[1], v, "+");
							OfflineMessage tmp = { v[0], v[1], v[2] };
							offlineBuf.push_back(tmp);
							itor = conList.erase(itor);
							if (itor == conList.end()) break;
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