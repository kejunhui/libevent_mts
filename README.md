# libevent_TMT
libevent tcp mulit thread server
libevent 多线程服务器
使用方法
1、加载libevent 库
2、继承TcpServerListener， 实现虚类
3、实现回调函数

#pragma once
#include <atomic>
#include <unordered_map>
#include "sockets.h"
#include "singleton.h"

class LibeventServer;
class NtripCenter:public TcpServerListener
{
	SINGLETON(NtripCenter);
	int send(const CSocket &socket, const unsigned char* buffer, const unsigned int &buffer_size);
	int onAccept(const CSocket &socket, const char* remote_host, const int &remote_port, const int &local_port);
	int onReceive(const CSocket &socket, const unsigned char* pData, const unsigned int &nLen);
	int onSend(const CSocket &socket, const unsigned char* pData, const unsigned int &nLen);
	int onError(const CSocket &socket, const unsigned long &nErrorCode);
	int onClose(const CSocket &socket);
private:

	std::unique_ptr<LibeventServer>								         m_pVrsServers;		// vrs servers											 		
};
