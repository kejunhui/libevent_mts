#pragma once
#include <string>
#include <vector>
#include <event2/util.h>
#include <event2/thread.h>
#include <event2/bufferevent.h>
#include <event2/event_struct.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/event.h>
#include "sockets.h"

class bufferevent;
class event_base;
class evconnlistener;
class LibeventServer;
class LibeventWorker_ssl;
class ThreadWorker;

class LibeventWorker
{
public:
	explicit LibeventWorker(LibeventServer *pServer);
	~LibeventWorker();
	friend class LibeventServer;
	virtual void appendBufferevent();
	int  start();
	int  stop();

protected:
	static void onReceive(bufferevent *bev, void *arg);
	static void onLasterror(bufferevent * bev, short events, void *arg);
	static void threadProcess(int fd, short which, void *arg);
protected:
	int						m_notifyReceFd;         // 管道接收端
	int						m_notifySendFd;         // 管道发送端
	event					m_notifyEvent;			// 监听管理的事件机	
	event_base			  * m_pBase;
	LibeventServer		  * m_pServer;
};

class LibeventServer :  public EventServerSocketInterface
{
public:
	explicit LibeventServer(TcpServerListener* pListener,int nNum = 4);
	~LibeventServer();
	
	virtual int stop();
	virtual int start( const char* pHost, int nPort = 8080, int nMaxConnections = 100000);
	virtual int send(const CSocket &socket, const unsigned char* buffer, const unsigned int &buffer_size);
	virtual int disconnect(const CSocket &socket);
	virtual int startThread();
	friend LibeventWorker;
	friend LibeventWorker_ssl;
	void getServerInfo(std::string &ip, int &port);
protected:
	static void onAccept(struct evconnlistener *listenner_cb, evutil_socket_t fd, struct sockaddr *sock, int socklen, void *arg);
	void appendBufferevent(const evutil_socket_t dwConnID);
	
protected:
	short							m_nPort;
	std::string						m_strIp;
	int								m_workNum;			// 工作线程数，最好是2的n 次方幂
	ThreadWorker				  * m_pThread;			// 线程处理
	evconnlistener				  * m_pListener;
	event_base					  * m_pListenBase;
	TcpServerListener			  * m_pCallback;
	std::vector<LibeventWorker*>	m_vecWorker;	
};
