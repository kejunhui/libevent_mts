#define EVENT__HAVE_OPENSSL
#include <event2/bufferevent_ssl.h>
#include <openssl/rand.h>
#include "eventserverssl.h"
#include "log.h"

LibeventWorker_ssl::LibeventWorker_ssl(LibeventServer *pSever, SSL_CTX *pCtx):LibeventWorker(pSever),m_pCtx(pCtx)
{

}

LibeventWorker_ssl::~LibeventWorker_ssl()
{

}

void LibeventWorker_ssl::appendBufferevent()
{
	evutil_socket_t confd;
	read(m_notifyReceFd, &confd, sizeof(evutil_socket_t));
	if (confd == -1) return;
	SSL * pSSL = SSL_new(m_pCtx);
	if (!pSSL)
	{
		LERROR("SSL_new error {}", confd);
		return;
	}
	bufferevent *bev = bufferevent_openssl_socket_new(m_pBase, confd, pSSL, BUFFEREVENT_SSL_ACCEPTING, BEV_OPT_CLOSE_ON_FREE);
	struct timeval t = { 60,0 };
	bufferevent_set_timeouts(bev, &t, NULL);
	evutil_make_socket_nonblocking(confd);
	evbuffer_enable_locking(bufferevent_get_output(bev), NULL);
	evbuffer_enable_locking(bufferevent_get_input(bev), NULL);
	bufferevent_setcb(bev, onReceive, NULL, onLasterror, m_pServer);
	bufferevent_enable(bev, EV_READ | EV_PERSIST);
	bufferevent_enable(bev, EV_TIMEOUT | EV_PERSIST);
	CSocket socket(confd, m_pServer, bev);
	m_pServer->m_pCallback->onAccept(socket, m_pServer->m_strIp.c_str(), 0, m_pServer->m_nPort);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
LibeventServer_ssl::LibeventServer_ssl(TcpServerListener* pListener, int nNum) :LibeventServer(pListener, nNum)
{
	m_pCtx = nullptr;
}

LibeventServer_ssl::~LibeventServer_ssl() 
{

}

int LibeventServer_ssl::start(const char* pHost, int nPort, int nMaxConnections)
{
	if (!init_SSL()) return -1;
	return LibeventServer::start(pHost, nPort, nMaxConnections);
}

int LibeventServer_ssl::stop()
{
	if (m_pCtx)
	{
		SSL_CTX_free(m_pCtx);
		m_pCtx = nullptr;
	}
	return  LibeventServer::stop();
}

int LibeventServer_ssl::startThread()
{
	// 先启动worker
	for (int i = 0; i < m_workNum; i++)
	{
		LibeventWorker * pWorker = new LibeventWorker_ssl(this, m_pCtx);
		pWorker->start();
		m_vecWorker.push_back(pWorker);
	}
	std::thread t = std::thread([](event_base *base) {event_base_loop(base, EVLOOP_NO_EXIT_ON_EMPTY); }, m_pListenBase);
	t.detach();
	return 0;
}

int LibeventServer_ssl::disconnect(const CSocket &socket)
{
	bufferevent* bev = static_cast<bufferevent*>(socket.socket);
	SSL* ctx = bufferevent_openssl_get_ssl(bev);
	if (ctx)
	{
		SSL_set_shutdown(ctx, SSL_RECEIVED_SHUTDOWN);
		SSL_shutdown(ctx);
	}
	LibeventServer::disconnect(socket);
	return 0;
}

bool LibeventServer_ssl::init_SSL()
{
	SSL_load_error_strings();

	SSL_library_init();

	if (!RAND_poll()) return false;

	m_pCtx = SSL_CTX_new(TLSv1_server_method());
	if (!m_pCtx)
	{
		LERROR("SSL_CTX_new failed, SSL_CTX is null");
		return false;
	}
	char file[MAX_PATH_LOCAL] = { 0 };
	char path[MAX_PATH_LOCAL] = { 0 };
	getModulePath(path);
	// 服务端的证书文件
	sprintf(file, "%s/conf/server.crt", path);
	if (SSL_CTX_use_certificate_file(m_pCtx, file, SSL_FILETYPE_PEM) <= 0)
	{
		LERROR("init_SSL,check certificate file");
		return false;
	}
	// 服务端的私钥(建议加密存储)
	sprintf(file, "%s/conf/server.key", path);
	if (SSL_CTX_use_PrivateKey_file(m_pCtx, file, SSL_FILETYPE_PEM) <= 0)
	{
		LERROR("init_SSL,check PrivateKey file");
		return false;
	}
	return true;
}