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
	std::unique_ptr<LibeventServer>								m_pServers;		// servers							
};