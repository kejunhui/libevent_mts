#include "eventserver.h"
#include <event2/event_compat.h>
#include "log.h"
#ifndef WIN32
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#endif

/**************************** LibeventWorker **********************************/
LibeventWorker::LibeventWorker(LibeventServer *pServer):m_pServer(pServer), m_pBase(nullptr)
{

}

LibeventWorker::~LibeventWorker()
{
	stop();
}

void  LibeventWorker::appendBufferevent()
{
	evutil_socket_t confd;
	read(m_notifyReceFd, &confd, sizeof(evutil_socket_t));
	if (confd == -1) return;
	struct bufferevent *bev = bufferevent_socket_new(m_pBase, confd, BEV_OPT_CLOSE_ON_FREE);
	if (!bev)
	{
		LERROR("Error constructing bufferevent{}!", confd);
		return;
	}
	struct timeval t = { 60,0 };
	bufferevent_set_timeouts(bev, &t, NULL);
	evutil_make_socket_nonblocking(confd);
	bufferevent_setcb(bev, onReceive, NULL, onLasterror, m_pServer);
	bufferevent_enable(bev, EV_TIMEOUT | EV_PERSIST);
	bufferevent_enable(bev, EV_READ | EV_PERSIST);
	CSocket socket(confd, m_pServer, bev);
	m_pServer->m_pCallback->onAccept(socket, m_pServer->m_strIp.c_str(), 0, m_pServer->m_nPort);
}

void LibeventWorker::threadProcess(int fd, short which, void *arg)
{
	LibeventWorker * worker = (LibeventWorker*)arg;
	worker->appendBufferevent();
}

void LibeventWorker::onReceive(bufferevent *bev, void *arg)
{
	LibeventServer * pServer = static_cast<LibeventServer *>(arg);
	unsigned char request[4096] = { 0 };
	size_t len = bufferevent_read(bev, request, sizeof(request) - 1);
	request[len] = '\0';
	CSocket socket(bufferevent_getfd(bev), pServer, bev);
	pServer->m_pCallback->onReceive(socket, request, len);
}

void LibeventWorker::onLasterror(bufferevent * bev, short events, void *arg)
{
	LibeventServer * pServer = static_cast<LibeventServer *>(arg);
	CSocket socket(bufferevent_getfd(bev), pServer, bev);
	if(events & BEV_EVENT_TIMEOUT)
	{
		LERROR("bufferevent timeout {}", socket.dwConnID);
	}
	pServer->m_pCallback->onError(socket, events);
	bufferevent_free(bev);
}

int  LibeventWorker::start()
{
	if (m_pBase == nullptr)
	{
		m_pBase = event_base_new();
		evthread_make_base_notifiable(m_pBase);
	}	
	int fds[2];
	int res = Pipe(fds);
	assert(res == 0);
	m_notifyReceFd = fds[0];
	m_notifySendFd = fds[1];
	
	event_set(&m_notifyEvent, m_notifyReceFd, EV_READ | EV_PERSIST, threadProcess, this);
	event_base_set(m_pBase, &m_notifyEvent);
	res = event_add(&m_notifyEvent, 0);
	assert(res == 0);
	std::thread t = std::thread([](event_base *base) {event_base_loop(base, EVLOOP_NO_EXIT_ON_EMPTY); }, m_pBase);
	t.detach();
	return 0;
}

int LibeventWorker::stop()
{
	if (m_pBase)
	{
		event_base_loopbreak(m_pBase);
		event_base_free(m_pBase);
	}
	m_pBase = nullptr;
	return 0;
}

/******************************* LibeventServer ************************************/
LibeventServer::LibeventServer(TcpServerListener* pListener,int nNum):m_pCallback(pListener)
{
#ifdef WIN32
	WSAData wsaData;
	WSAStartup(MAKEWORD(2, 0), &wsaData);
	evthread_use_windows_threads();
	struct event_config* cfg = event_config_new();
	event_config_set_flag(cfg, EVENT_BASE_FLAG_STARTUP_IOCP);
#else
	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
		return ;
	evthread_use_pthreads();    //unix上设置
#endif
	int capacity = 1;
	while (capacity < nNum)
		capacity <<= 1;
	m_workNum = capacity;
	m_pThread = nullptr;
	m_pListener = nullptr;
	m_pListenBase = nullptr;
}

LibeventServer::~LibeventServer()
{
	stop();
}

int LibeventServer::start(const char* pHost, int nPort, int nMaxConnections)
{
	m_strIp = pHost;
	m_nPort = nPort;
	m_pListenBase = event_base_new();

	if (m_pListenBase == NULL)
	{
		LERROR("Could not initialize libevent");
		return - 1;
	}
	
	struct sockaddr_in sin;
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	#if defined(WIN32)
	sin.sin_addr.S_un.S_addr = inet_addr(pHost);
	#else
	sin.sin_addr.s_addr = inet_addr(pHost);
	#endif
	sin.sin_port = htons(m_nPort);

	struct sockaddr_in6 sin6;
	memset(&sin6, 0, sizeof(struct sockaddr_in6));
	sin6.sin6_family = AF_INET6;
	sin6.sin6_port = htons(m_nPort);

	m_pListener = evconnlistener_new_bind(m_pListenBase, onAccept, this, LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE /*| LEV_OPT_THREADSAFE*/, -1, (struct sockaddr*)&sin, sizeof(sin));
	// 绑定并监听IPV6端口
	//evconnlistener *listener6 = evconnlistener_new_bind(m_pListenBase, onAccept, this, LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE | LEV_OPT_THREADSAFE,-1, (struct sockaddr*)&sin6, sizeof(sin6));
	if (m_pListener == NULL/* && listener6 == NULL*/)
	{
		LERROR("Listener bind failed");
		return -1;
	}
	startThread();
	return 0;
}

int LibeventServer::startThread()
{
	// 先启动worker
	for (int i = 0; i < m_workNum; i++)
	{
		LibeventWorker * pWorker = new LibeventWorker(this);
		pWorker->start();
		m_vecWorker.push_back(pWorker);
	}

	std::thread t = std::thread([](event_base *base) {event_base_loop(base, EVLOOP_NO_EXIT_ON_EMPTY); }, m_pListenBase);
	t.detach();
	return 0;
}

void LibeventServer::onAccept(struct evconnlistener *listenner_cb, evutil_socket_t fd, struct sockaddr *sock, int socklen, void *arg)
{
	LibeventServer* pServer = static_cast<LibeventServer *>(arg);
	pServer->appendBufferevent(fd);
}

void LibeventServer::appendBufferevent(const evutil_socket_t dwConnID)
{
	int sfd = m_vecWorker[dwConnID & (m_workNum - 1)]->m_notifySendFd;
	write(sfd, &dwConnID, sizeof(evutil_socket_t));
}

int LibeventServer::stop()
{
	while (m_vecWorker.size()>0)
	{
		LibeventWorker * pWorker = m_vecWorker.back();
		pWorker->stop();
		delete pWorker;
		m_vecWorker.pop_back();
	}
	if (m_pListener)
	{
		evconnlistener_free(m_pListener);
	}
	if (m_pListenBase)
	{
		event_base_loopbreak(m_pListenBase);
		event_base_free(m_pListenBase);
	}
	m_pListenBase = nullptr;
	m_pListener = nullptr;
	return 0;
}

int LibeventServer::send(const CSocket &socket, const unsigned char* buffer, const unsigned int &buffer_size)
{
	int result = 1;
	bufferevent* bev = static_cast<bufferevent*>(socket.socket);
	if (bev)
	{
		evbuffer * output = bufferevent_get_output(bev);
		if (evbuffer_get_length(output) < 1024 * 4 * 8) // maybe connection lost or client socket don't receive from buf
		{
			result = bufferevent_write(bev, buffer, buffer_size);
		}
		else LWARN("port:{}, fd:{},bufferevent out of buf!",m_nPort, socket.dwConnID);
	}
	return result;
}

int LibeventServer::disconnect(const CSocket &socket)
{
	bufferevent* bev = static_cast<bufferevent*>(socket.socket);
	if (bev)
	{
		evbuffer * outbuf = bufferevent_get_output(bev);
		evbuffer * intbuf = bufferevent_get_input(bev);
		evbuffer_unfreeze(outbuf, 1);
		evbuffer_unfreeze(intbuf, 1);
		evbuffer_drain(intbuf, evbuffer_get_length(intbuf));
		evbuffer_drain(outbuf, evbuffer_get_length(outbuf));
		bufferevent_free(bev);
	}
	return 0;
}

void LibeventServer::getServerInfo(std::string &ip, int &port)
{
	ip = m_strIp;
	port = m_nPort;
}
