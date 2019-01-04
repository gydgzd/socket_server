/*
 * MySocket_server.cpp
 *
 *  Created on: Jun 15, 2017
 *      Author: gyd
 */
#include "MySocket_server.h"
std::mutex g_recvMutex;
std::mutex g_sendMutex;
std::mutex g_clientNumMutex;
MySocket_server::MySocket_server()
{
	mn_socket_fd = 0;

	mn_clientCounts = 0;
	mn_clientSend = 0;
	mn_isFlushed = 1;
	mp_msgQueueRecv = NULL;
	mp_msgQueueSend = NULL;

	mylog.setMaxFileSize(200*1024*1024); // 200MB
}

MySocket_server::~MySocket_server()
{
	close(mn_socket_fd);
}

int MySocket_server::init(int listenPort, queue<MSGBODY> * msgQToRecv, queue<MSGBODY> * msgQToSend)
{
	char logmsg[512] = "";
    if(listenPort <= 0 || listenPort >=65536)
    {
		mylog.logException("ERROR: listen port error, should between 1-65535!");
		return -1;
	}
	mp_msgQueueRecv = msgQToRecv;
	mp_msgQueueSend = msgQToSend;
	// initialize
	mylog.logException("****************************BEGIN****************************");
	if( (mn_socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1 )
	{
		sprintf(logmsg, "ERROR: Create socket error: %s (errno: %d)\n", strerror(errno), errno);
		mylog.logException(logmsg);
		return -1;
	}

	memset(&m_serverAddr, 0, sizeof(m_serverAddr));
	m_serverAddr.sin_family = AF_INET;
	m_serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);  //
	m_serverAddr.sin_port = htons(listenPort);       //
	int optval = 1;
	if(-1 == setsockopt(mn_socket_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)))
	{
		sprintf(logmsg, "ERROR: Reuse addr error: %s (errno: %d)\n", strerror(errno), errno);
		mylog.logException(logmsg);
		return -1;
	}

	// bind
	if(-1 == bind(mn_socket_fd, (struct sockaddr*)&m_serverAddr, sizeof(m_serverAddr)) )
	{
		sprintf(logmsg, "ERROR: Bind error: %s (errno: %d)\n", strerror(errno), errno);
		mylog.logException(logmsg);
		return -1;
	}
	sprintf(logmsg, "INFO: bind succeed to port %d", listenPort);
	mylog.logException(logmsg);
	if(-1 == listen(mn_socket_fd, 10)  )
	{
		sprintf(logmsg, "ERROR: Listen error: %s (errno: %d)\n", strerror(errno), errno);
		mylog.logException(logmsg);
		return -1;
	}
	mylog.logException("INFO: listen succeed.");
	return 0;
}
/*
 * connect to a server+ by IP
 */
int MySocket_server::connectTo(char* server_IP)
{
	if( inet_pton(AF_INET, server_IP, &m_serverAddr.sin_addr) <= 0)
	{
		printf("inet_pton error: %s\n", server_IP);
		return -1;
	}
	if( connect(mn_socket_fd, (struct sockaddr*)&m_serverAddr, sizeof(m_serverAddr)) < 0)
	{
		printf("connect error: %s(errno: %d)\n", strerror(errno), errno);
		return -1;
	}
	return 0;
}
/*
 * include accept (block mode)
 * and send and receive (non block mode)
 */
int MySocket_server::serv()
{
	//
	struct sockaddr_in client_addr;
	struct sockaddr_in server_addr;
	socklen_t server_len = sizeof(server_addr);
	socklen_t client_len = sizeof(client_addr);
	memset(&client_addr, 0, client_len);
	memset(&client_addr, 0, server_len);

	queue<int> connect_fdQueue;      // for storage of all the fd that accepted, to be served

	char logmsg[512] = "";
	while(true)
	{
		CONNECTION client;
		if( -1 == (client.socket_fd = accept(mn_socket_fd, (struct sockaddr*)&client_addr, &client_len)))
		{
			sprintf(logmsg, "Accept error: %s (errno: %d)\n", strerror(errno), errno);
			sleep(10);
			continue;
		}
		// client count +1 when accept one new client
		safeAddClientCounts();

		// get server address
		getsockname(client.socket_fd, (struct sockaddr *)&server_addr, &server_len);
		inet_ntop(AF_INET,(void *)&server_addr.sin_addr, client.serverIP, 64 );
		client.serverPort = ntohs(server_addr.sin_port);

		// get client address
		memset(client.clientIP, 0, 64);
		inet_ntop(AF_INET,(void *)&client_addr.sin_addr, client.clientIP, 64 );
		client.clientPort = ntohs(client_addr.sin_port);

		sprintf(logmsg, "INFO: %s:%d --> %s:%d connected, there are %d clients online!", client.clientIP, client.clientPort, client.serverIP, client.serverPort, mn_clientCounts);
		mylog.logException(logmsg);
		// keepalive
		int ret = setKeepalive(client.socket_fd, 10, 5, 3);
		if( ret == -1)
		{
		    mylog.logException("ERROR: set keepalive failed!");
		}
		else
		    mylog.logException("INFO: set keepalive succeed!");
		//
	//	myrecv(client);
	//	mysend(client);
		std::thread th_recv{&MySocket_server::myrecv, this, client};
		std::thread th_send{&MySocket_server::mysend, this, client};
	//	std::thread th1{&MySocket_server::recvAndSend, this, client};
	//	th1.join();
	//	th1.detach();
	}
	return 0;
}
/*
 *  default send buffer 87040, recv buffer 369280
 */
int MySocket_server::recvAndSend(const CONNECTION client)
{
	int isSend = 0;             // whether sended to this client: 1 is send, 0 not

	// get buffer
/*	int s_length, r_length;
	socklen_t optl = sizeof(s_length);
	getsockopt(client.socket_fd,SOL_SOCKET,SO_SNDBUF,&s_length,&optl);     //获得连接套接字发送端缓冲区的信息
	getsockopt(client.socket_fd,SOL_SOCKET,SO_RCVBUF,&r_length,&optl);     //获得连接套接字的接收端的缓冲区信息
	printf("send buffer = %d, recv buffer = %d\n",s_length, r_length);
  */

	// set buffer
/*	int nRecvBufSize = 64*1024;//设置为64K
	setsockopt(client.socket_fd,SOL_SOCKET,SO_RCVBUF,(const char*)&nRecvBufSize,sizeof(int));
	int nSendBufSize = 64*1024;//设置为64K
	setsockopt(client.socket_fd,SOL_SOCKET,SO_SNDBUF,(const char*)&nSendBufSize,sizeof(int));
*/
	MSGBODY recvBuf;
	char logmsg[512] = "";
	char logHead[64] = "";
	sprintf(logHead, "%s:%d --> %s:%d ", client.clientIP, client.clientPort, client.serverIP, client.serverPort);
	char * p_hexLog = NULL;
	//使用非阻塞io
	int flags;
	if( (flags = fcntl(client.socket_fd, F_GETFL, 0)) < 0)
	{
	    printf("fcntl error: %s", strerror(errno));
	    sprintf(logmsg, "ERROR: %s: fcntl error: %d--%s",logHead, errno, strerror(errno) );
	    mylog.logException(logmsg);
	    return -1;
	}
	fcntl(client.socket_fd, F_SETFL, flags | O_NONBLOCK);

	while(true)
	{
		recvBuf.length = 0;
		memset(recvBuf.msg, 0, sizeof(recvBuf.msg));

		// recv ,  recv() return 0 when connection is closed
		recvBuf.length = recv(client.socket_fd, recvBuf.msg, MAXLENGTH, 0);
		if(recvBuf.length == -1)     // recv
		{
			// data isnot ready when errno = 11
			if(errno != 11)
			{
				sprintf(logmsg, "ERROR: %s: recv error: %d--%s",logHead, errno, strerror(errno) );
				mylog.logException(logmsg);
			}
			//sleep(1);
			usleep(10000);  // 10ms
			recvBuf.length = 0;  // set it back to 0
		}
		else                     // recv success
		{
			//	printf("%s %s recved: ", getLocalTime("%Y-%m-%d %H:%M:%S").c_str(), logHead);
			//	for(int i=0; i<recvBuf.length; i++)
			//		printf("%c", (unsigned char)recvBuf.msg[i]);
			//	printf("\n");             //it will cause an error in daemon(broken pipe)
			// log the hex
			try
			{
				p_hexLog = new char[recvBuf.length*3 + 128];    // include the logHead
				memset(p_hexLog, 0, recvBuf.length*3 + 128);
				sprintf(p_hexLog, "INFO: %s recved: ", logHead);
				int len = strlen(p_hexLog);
				for(int i=0; i<recvBuf.length; i++)
					sprintf(p_hexLog+len+3*i, "%02x ", (unsigned char)recvBuf.msg[i]);
				mylog.logException(p_hexLog);
				delete[] p_hexLog;
			}catch(bad_alloc& bad)
			{
				sprintf(logmsg,"ERROR: Failed to alloc mem when log hex: %s", bad.what());
				mylog.logException(logmsg);
			}
			int ret = msgCheck(&recvBuf);
			if(strcmp((char *)recvBuf.msg,"exit\n")==0 || recvBuf.length == 0)
			{
				close(client.socket_fd);
				sprintf(logmsg, "INFO: %s: The child process is exit.\n", logHead);
				mylog.logException(logmsg);
				// client count -1 when a client exit
				safeDecClientCounts();
				return 0;
			}
			else if(ret == 1)  // heart beat
			{
				if(send(client.socket_fd, recvBuf.msg, 31, 0) == -1)
				{
					sprintf(logmsg, "ERROR: %s: heart error: %d--%s",logHead, errno, strerror(errno) );
					mylog.logException(logmsg);
				}
				mylog.logException("INFO: Get a heartbeat.");
			}
			else if(ret == 0)  // valid msg
			{
				// add a lock
				{
					std::lock_guard<std::mutex> guard(g_recvMutex);
					if(mp_msgQueueRecv->size() == MAXQUEUELENGTH )
						mp_msgQueueRecv->pop();
					mp_msgQueueRecv->push(recvBuf);  // msg push back to the queue
					sprintf(logmsg, "INFO: %s: recved 1 valid message, add to queue. Total: %u in recv queue, %u in send queue.",logHead, (unsigned int)mp_msgQueueRecv->size(),(unsigned int)mp_msgQueueSend->size());
					mylog.logException(logmsg);
				}
				// do something handle the msg that received
				// ....
			}
			else
			{
				mylog.logException("INFO: msg invalid.");
			}
		}// end if,  recv finished

		// send
		{
			std::lock_guard<std::mutex> guard(g_sendMutex);
			if(isSend == 1 && mn_isFlushed == 1)    // update send status when flushed
				isSend = 0;
		}
		if(isSend == 1 && mn_isFlushed == 0)        // already send
		{
			sprintf(logmsg, "INFO: %s: recved %d bytes, message was sent lasttime, %d/%d.", logHead, recvBuf.length ,mn_clientSend, mn_clientCounts);
			mylog.logException(logmsg);
			continue;
		}
		if(mp_msgQueueSend->empty())
		{
			if(0 != recvBuf.length)
			{
				sprintf(logmsg, "INFO: %s: recved %d bytes, send %d bytes", logHead, recvBuf.length, 0 );
				mylog.logException(logmsg);
			}
			continue;
		}
		if(send(client.socket_fd, mp_msgQueueSend->front().msg, mp_msgQueueSend->front().length, 0) == -1)
		{
			sprintf(logmsg, "ERROR: %s: send error: %d--%s\n", logHead, errno, strerror(errno) );
			mylog.logException(logmsg);
		}
		//
		// printf("%s %s sent: ", getLocalTime("%Y-%m-%d %H:%M:%S").c_str(), logHead);
		// for(int i=0; i<mqstr_msgQueueSend->front().length; i++)
		//      printf("%2x", mqstr_msgQueueSend->front().msg[i]);
		// printf("\n");

		sprintf(logmsg, "INFO: %s: recved %d bytes, send %d bytes", logHead, recvBuf.length, mp_msgQueueSend->front().length );
		mylog.logException(logmsg);
		// flush the msg if send
		{
			std::lock_guard<std::mutex> guard(g_sendMutex);
			mn_clientSend++;
			if(mn_clientSend == mn_clientCounts)
			{
				mp_msgQueueSend->pop();
				mn_clientSend = 0;
				mn_isFlushed = 1;
				isSend = 0;
			}
		}
	//	printf("In the child process, pid is %d.\n", getpid());
	}// end of while
}

/*
 * recv thread function
 *
 */
int MySocket_server::myrecv(const CONNECTION client)
{
    int isSend = 0;             // whether send to this client: 1 is send, 0 not

    // get buffer
/*  int r_length;
    socklen_t optl = sizeof(s_length);
    getsockopt(client.socket_fd,SOL_SOCKET,SO_RCVBUF,&r_length,&optl);   //获得连接套接字的接收端的缓冲区信息
    printf("recv buffer = %d\n", r_length);

    // set buffer
    int nRecvBufSize = 64*1024;              //设置为64K
    setsockopt(client.socket_fd,SOL_SOCKET,SO_RCVBUF,(const char*)&nRecvBufSize,sizeof(int));
*/
    MSGBODY recvBuf;
    char logmsg[512] = "";
    char logHead[64] = "";
    sprintf(logHead, "%s:%d --> %s:%d ", client.clientIP, client.clientPort, client.serverIP, client.serverPort);
    char * p_hexLog = NULL;
    //使用非阻塞io
    int flags;
    if( (flags = fcntl(client.socket_fd, F_GETFL, 0)) < 0)
    {
        printf("fcntl error: %s", strerror(errno));
        sprintf(logmsg, "ERROR: %s: fcntl error: %d--%s",logHead, errno, strerror(errno) );
        mylog.logException(logmsg);
        return -1;
    }
    fcntl(client.socket_fd, F_SETFL, flags | O_NONBLOCK);

    while(true)
    {
        recvBuf.length = 0;
        memset(recvBuf.msg, 0, sizeof(recvBuf.msg));

        // recv ,  recv() return 0 when connection is closed
        recvBuf.length = recv(client.socket_fd, recvBuf.msg, MAXLENGTH, 0);
        if(recvBuf.length == -1)     // recv
        {
            // data isnot ready when errno = 11
            if(errno != 11)
            {
                sprintf(logmsg, "ERROR: %s: recv error: %d--%s",logHead, errno, strerror(errno) );
                mylog.logException(logmsg);
            }
            //sleep(1);
            usleep(10000);  // 10ms
            recvBuf.length = 0;  // set it back to 0
        }
        else                     // recv success
        {
            //  printf("%s %s recved: ", getLocalTime("%Y-%m-%d %H:%M:%S").c_str(), logHead);
            //  for(int i=0; i<recvBuf.length; i++)
            //      printf("%c", (unsigned char)recvBuf.msg[i]);
            //  printf("\n");             //it will cause an error in daemon(broken pipe)
            // log the hex
            try
            {
                p_hexLog = new char[recvBuf.length*3 + 128];    // include the logHead
                memset(p_hexLog, 0, recvBuf.length*3 + 128);
                sprintf(p_hexLog, "INFO: %s recved: ", logHead);
                int len = strlen(p_hexLog);
                for(int i=0; i<recvBuf.length; i++)
                    sprintf(p_hexLog+len+3*i, "%02x ", (unsigned char)recvBuf.msg[i]);
                mylog.logException(p_hexLog);
                delete[] p_hexLog;
            }catch(bad_alloc& bad)
            {
                sprintf(logmsg,"ERROR: Failed to alloc mem when log hex: %s", bad.what());
                mylog.logException(logmsg);
            }
            //
            int ret = 0;
      //      ret = msgCheck(&recvBuf);
            if(strcmp((char *)recvBuf.msg,"exit\n")==0 || recvBuf.length == 0)
            {
                close(client.socket_fd);
                sprintf(logmsg, "INFO: %s: The child process is exit.\n", logHead);
                mylog.logException(logmsg);
                // client count -1 when a client exit
                safeDecClientCounts();
                return 0;
            }
            else if(ret == 1)  // heart beat
            {
                if(send(client.socket_fd, recvBuf.msg, 31, 0) == -1)
                {
                    sprintf(logmsg, "ERROR: %s: heart error: %d--%s",logHead, errno, strerror(errno) );
                    mylog.logException(logmsg);
                }
                mylog.logException("INFO: Get a heartbeat.");
            }
            else if(ret == 0)  // valid msg
            {
                // add a lock
                {
                    std::lock_guard<std::mutex> guard(g_recvMutex);
                    if(mp_msgQueueRecv->size() == MAXQUEUELENGTH )
                        mp_msgQueueRecv->pop();
                    mp_msgQueueRecv->push(recvBuf);  // msg push back to the queue
                    sprintf(logmsg, "INFO: %s: recved 1 valid message, add to queue. Total: %u in recv queue, %u in send queue.",logHead, (unsigned int)mp_msgQueueRecv->size(),(unsigned int)mp_msgQueueSend->size());
                    mylog.logException(logmsg);
                }
                // do something handle the msg that received
                // ....
            }
            else
            {
                mylog.logException("INFO: msg invalid.");
            }
        }// end if,  recv finished
    }
    return 0;
}
/*
 * send thread function
 */
int MySocket_server::mysend(const CONNECTION client)
{
    int isSend = 0;             // whether sended to this client: 1 is send, 0 not

    // get buffer
/*  int s_length;
    socklen_t optl = sizeof(s_length);
    getsockopt(client.socket_fd,SOL_SOCKET,SO_SNDBUF,&s_length,&optl);     //获得连接套接字发送端缓冲区的信息
    printf("send buffer = %d\n",s_length);

    // set buffer
    int nSendBufSize = 64*1024;//设置为64K
    setsockopt(client.socket_fd,SOL_SOCKET,SO_SNDBUF,(const char*)&nSendBufSize,sizeof(int));
*/
    MSGBODY recvBuf;
    char logmsg[512] = "";
    char logHead[64] = "";
    sprintf(logHead, "%s:%d --> %s:%d ", client.clientIP, client.clientPort, client.serverIP, client.serverPort);
    char * p_hexLog = NULL;
    //使用非阻塞io
    int flags;
    if( (flags = fcntl(client.socket_fd, F_GETFL, 0)) < 0)
    {
        printf("fcntl error: %s", strerror(errno));
        sprintf(logmsg, "ERROR: %s: fcntl error: %d--%s",logHead, errno, strerror(errno) );
        mylog.logException(logmsg);
        return -1;
    }
    fcntl(client.socket_fd, F_SETFL, flags | O_NONBLOCK);

    while(true)
    {
        // send
        {
            std::lock_guard<std::mutex> guard(g_sendMutex);
            if(isSend == 1 && mn_isFlushed == 1)    // update send status when flushed
                isSend = 0;
        }
        if(isSend == 1 && mn_isFlushed == 0)        // already send
        {
            sprintf(logmsg, "INFO: %s: recved %d bytes, message was sent lasttime, %d/%d.", logHead, recvBuf.length ,mn_clientSend, mn_clientCounts);
            mylog.logException(logmsg);
            continue;
        }
        if(mp_msgQueueSend->empty())
        {
            if(0 != recvBuf.length)
            {
                sprintf(logmsg, "INFO: %s: recved %d bytes, send %d bytes", logHead, recvBuf.length, 0 );
                mylog.logException(logmsg);
            }
            continue;
        }
        if(send(client.socket_fd, mp_msgQueueSend->front().msg, mp_msgQueueSend->front().length, 0) == -1)
        {
            sprintf(logmsg, "ERROR: %s: send error: %d--%s\n", logHead, errno, strerror(errno) );
            mylog.logException(logmsg);
        }
        //
        //  printf("%s %s sent: ", getLocalTime("%Y-%m-%d %H:%M:%S").c_str(), logHead);
        //  for(int i=0; i<mqstr_msgQueueSend->front().length; i++)
        //      printf("%2x", mqstr_msgQueueSend->front().msg[i]);
        //  printf("\n");

        sprintf(logmsg, "INFO: %s: recved %d bytes, send %d bytes", logHead, recvBuf.length, mp_msgQueueSend->front().length );
        mylog.logException(logmsg);
        // flush the msg if send
        {
            std::lock_guard<std::mutex> guard(g_sendMutex);
            mn_clientSend++;
            if(mn_clientSend == mn_clientCounts)
            {
                mp_msgQueueSend->pop();
                mn_clientSend = 0;
                mn_isFlushed = 1;
                isSend = 0;
            }
        }
    }// end of while
    return 0;
}

/*
 * get msg from keyboard
 */
int MySocket_server::getMsg()
{
   // fgets((char *)m_sendBuf.msg, sizeof(m_sendBuf.msg), stdin);
	return 0;
}
/*
 * send msg
 */
int MySocket_server::sendMsg()
{
	MSGBODY recvBuffer;
	MSGBODY sendBuffer;
	recvBuffer.length = 0;
	memset(recvBuffer.msg, 0, sizeof(recvBuffer.msg));
	sendBuffer.length = 0;
	memset(sendBuffer.msg, 0, sizeof(sendBuffer.msg));

	char logmsg[512] = "";
	if( send(mn_socket_fd, sendBuffer.msg, sizeof(sendBuffer.msg), 0) < 0)
	{
		sprintf(logmsg, "send msg error: %s(errno: %d)", strerror(errno), errno);
		mylog.logException(logmsg);
		close(mn_socket_fd);
		return -1;
	}
	printf("send msg to server: %s\n", sendBuffer.msg);

	if(strcmp((char *)sendBuffer.msg, "exit\n")==0)
	{
		mylog.logException("INFO: exit.");
		close(mn_socket_fd);
		return -1;
	}
	if((recvBuffer.length = recv(mn_socket_fd, recvBuffer.msg, MAXLENGTH, 0)) == -1)
	{
		sprintf(logmsg, "ERROR: receive error: %s(errno: %d)", strerror(errno), errno);
		mylog.logException(logmsg);
		close(mn_socket_fd);
		return -1;
	}

	printf("Received: %s", recvBuffer.msg);
	return 0;
}
int MySocket_server::safeAddClientCounts()
{
	std::lock_guard<std::mutex> guard(g_sendMutex);
	mn_clientCounts++;
	return 0;
}
int MySocket_server::safeDecClientCounts()
{
	std::lock_guard<std::mutex> guard(g_sendMutex);
	mn_clientCounts--;
	return 0;
}
/*
 * to check if a msg is valid,legal
 * should be begin with FFFFFFFF, end with EEEEEEEE, 31 byte at least
 * if the 15th byte is 0x00, regard it as a heartbeat, return 1;
 * if the msg is invalid, return -1; else return 0;
 */
int MySocket_server::msgCheck(const MSGBODY *msg)
{
	char hexff = 0xff;
	char hexee = 0xee;
	char hex00 = 0x00;
	if(msg->length < 31)
		return -1;
	// check head and tail
	for(int i = 0; i < 4; i++)
		if( 0 != memcmp(&(msg->msg[i]) , &hexff, 1))
			return -1;
	for(int i = msg->length - 1 ; i > msg->length - 5; i--)
		if( 0 != memcmp(&(msg->msg[i]), &hexee, 1))
			return -1;
	// check heartbeat
	if( (0 == memcmp(&(msg->msg[14]), &hex00, 1)) && msg->length == 31)
		return 1;
	return 0;
}
/*
 * setsockopt of keepalive
 * fd is the socket file descriptor
 * idle is the idle time to start check heartbeat
 * interval is the time between heartbeat
 * probe is the time to send heartbeat
 */
int MySocket_server::setKeepalive(int fd, int idle, int interval, int probe )
{
	char logmsg[512] = "";
	// keepalive
	int optval = 1;
	if(setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval)) < 0)
	{
		mylog.logException("ERROR: set keepalive failed!");
		return -1;
	}
	 /* idle秒钟无数据，触发保活机制，发送保活包 */
	if (setsockopt(fd, SOL_TCP, TCP_KEEPIDLE, &idle, sizeof(idle)) != 0 )
	{
	  sprintf(logmsg, "ERROR: Set keepalive idle error: %s.", strerror(errno));
	  mylog.logException(logmsg);
	  return -1;
	}
	/* 如果没有收到回应，则interval秒钟后重发保活包 */
	if (setsockopt(fd, SOL_TCP, TCP_KEEPINTVL, &interval, sizeof(interval)) != 0 )
	{
		sprintf(logmsg, "ERROR: Set keepalive intv error: %s.\n", strerror(errno));
		return -1;
	}
	/* 连续probe次没收到保活包，视为连接失效 */
	if (setsockopt (fd, SOL_TCP, TCP_KEEPCNT, &probe, sizeof(probe)) != 0)
	{
		sprintf(logmsg, "Set keepalive cnt error: %s.\n", strerror (errno));
		return -1;
	}
	return 0;
}
