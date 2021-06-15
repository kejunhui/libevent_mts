#pragma once
#include<openssl/ssl.h>
#include<openssl/err.h>
#include "eventserver.h"


class LibeventServer_ssl;
class LibeventWorker_ssl :public LibeventWorker
{
public:
	friend class LibeventServer;
	explicit LibeventWorker_ssl(LibeventServer *pSever, SSL_CTX *pCtx);
	~LibeventWorker_ssl();
	void appendBufferevent()override;

private:
	SSL_CTX					* m_pCtx;
};

class LibeventServer_ssl :public LibeventServer
{
public:
	explicit LibeventServer_ssl(TcpServerListener* pListener, int nNum = 4);
	~LibeventServer_ssl();
	friend LibeventWorker_ssl;

	int start(const char* pHost, int nPort = 8080, int nMaxConnections = 100000)override;
	int stop()override;
	int startThread()override;
	int disconnect(const CSocket &socket)override;
	bool init_SSL();

private:
	SSL_CTX					* m_pCtx;
};


