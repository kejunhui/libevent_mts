#pragma once

struct CSocket
{
	CSocket() { dwConnID = -1; socket = nullptr; }
	CSocket(int fd,void *svr, void * sct) :dwConnID(fd), server(svr), socket(sct) {}
	CSocket(const CSocket& other) { dwConnID = other.dwConnID; server = other.server; socket = other.socket; }
	~CSocket() {}
	bool operator==(const CSocket& other) { return other.dwConnID == dwConnID; }
	bool operator!=(const CSocket& other) { return other.dwConnID != dwConnID; }
	CSocket & operator=(const CSocket& other) { dwConnID = other.dwConnID; server = other.server; socket = other.socket; return *this; }
	int dwConnID;
	void * socket;
	void * server;
};

struct message
{
	message() {}
	message(const message& other ) { pBuff = other.pBuff; nLen = other.nLen;}
	message(unsigned char * p, const unsigned int &len):pBuff(p),nLen(len) {}
	message & operator=(const message& other) { pBuff = other.pBuff; nLen = other.nLen; return *this; }
	unsigned char *pBuff = nullptr;
	unsigned int nLen = 0;
};

class TcpServerListener
{
public:
	virtual int onAccept(const CSocket &socket, const char* remote_host, const int &remote_port, const int &local_port) = 0;
	virtual int onReceive(const CSocket &socket, const unsigned char* pData,const unsigned int &nLen) = 0;
	virtual int onSend(const CSocket &socket, const unsigned char* pData, const unsigned int &nLen) = 0;
	virtual int onError(const CSocket &socket, const unsigned long &nErrorCode) = 0;
	virtual int onClose(const CSocket &socket) = 0;
};

class EventServerSocketInterface
{
public:
	virtual ~EventServerSocketInterface() {}

	// interface methods
	virtual int start(const char* pHost, int nPort = 8080, int nMaxConnections = 100000) = 0;
	virtual int stop() = 0;
	virtual int send(const CSocket &socket, const unsigned char* buffer, const unsigned int &buffer_size) = 0;
	virtual int disconnect(const CSocket &socket) = 0;
};

class TcpClientListener
{
public:
	virtual int onSend(const CSocket &socket, const unsigned char* pData, unsigned int &nLen) = 0;
	virtual int onReceive(const CSocket &socket, const unsigned char* pData, unsigned int &nLen) = 0;
	virtual int onClose(const CSocket &socket) = 0;
	virtual int onError(const CSocket &socket, unsigned long &nErrorCode) = 0;
	virtual int onConnect(const CSocket &socket, const char* remote_host, const int &remote_port, const int &local_port) = 0;
};

class EventClientSocketInterface
{
public:
	virtual ~EventClientSocketInterface() {}

	// interface methods
	virtual int connectServer(const char* host, unsigned short port) = 0;
	virtual int disconnect(CSocket &socket) = 0;
	virtual int send(const CSocket &socket, unsigned char* buffer, unsigned int &buffer_size) = 0;
};

