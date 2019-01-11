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
	mn_socketToLocal = 0;

	mn_clientCounts = 0;
//	mn_clientSend = 0;

	mp_msgQueueRecv = NULL;
	mp_msgQueueSend = NULL;

	mylog.setMaxFileSize(200*1024*1024); // 200MB
}

MySocket_server::~MySocket_server()
{
	close(mn_socketToLocal);
}
int MySocket_server::loadConfig()
{
    // read from file
    ifstream infile;
    infile.open(CONFIGFILE, ios_base::in);
    char logmsg[256] = "";
    if (!infile)
    {
        sprintf(logmsg, "ERR: Load config from %s failed:(%d) %s", CONFIGFILE, errno, strerror(errno));
        mylog.logException(logmsg);
        return -1;
    }
    string linebuf;
    string key, value;
    while (getline(infile, linebuf))   // !infile.eof()
    {
        stringstream ss(linebuf);
        getline(ss, key, '=');
        ss >> value;
        if(key.substr(0, 1) == "#" || key.substr(0, 1) == "\r" || key.substr(0, 1) == "\0")
            continue;
        mmap_config[key] = value;

    }
    infile.close();
    mylog.logException("INFO: Load config from file succeed.");

    return 0;
}

int MySocket_server::init(int listenPort, queue<MSGBODY> * msgQToRecv = &m_msgQueueRecv, queue<MSGBODY> * msgQToSend = &m_msgQueueSend)
{
	char logmsg[512] = "";
	mylog.logException("****************************BEGIN****************************");
    if(listenPort <= 0 || listenPort >=65536)
    {
		mylog.logException("ERROR: listen port error, should between 1-65535! Program exit.");
		exit(-1);
		//return -1;
	}

	// initialize
	mp_msgQueueRecv = msgQToRecv;
	mp_msgQueueSend = msgQToSend;
	if( (mn_socketToLocal = socket(AF_INET, SOCK_STREAM, 0)) == -1 )
	{
		sprintf(logmsg, "ERROR: Create socket error: %s (errno: %d). Program exit.", strerror(errno), errno);
		mylog.logException(logmsg);
		exit(-1);
		//return -1;
	}
	if( (mn_socketToServer = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        sprintf(logmsg, "ERROR: Create socket error: %s(errno: %d). Program exit.", strerror(errno), errno);
        mylog.logException(logmsg);
        exit(-1);
    }
	memset(&m_localAddr, 0, sizeof(m_localAddr));
	m_localAddr.sin_family = AF_INET;
	m_localAddr.sin_addr.s_addr = htonl(INADDR_ANY);  //
	m_localAddr.sin_port = htons(listenPort);       //
	int optval = 1;
	if(-1 == setsockopt(mn_socketToLocal, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)))
	{
		sprintf(logmsg, "ERROR: Reuse addr error: %s (errno: %d). Program exit.", strerror(errno), errno);
		mylog.logException(logmsg);
		exit(-1);
		//return -1;
	}

	// bind
	if(-1 == bind(mn_socketToLocal, (struct sockaddr*)&m_localAddr, sizeof(m_localAddr)) )
	{
		sprintf(logmsg, "ERROR: Bind error: %s (errno: %d). Program exit.", strerror(errno), errno);
		mylog.logException(logmsg);
		exit(-1);
        //return -1;
	}
	sprintf(logmsg, "INFO: bind succeed to port %d", listenPort);
	mylog.logException(logmsg);
	// listen
	if(-1 == listen(mn_socketToLocal, 10) )
	{
		sprintf(logmsg, "ERROR: Listen error: %s (errno: %d). Program exit.", strerror(errno), errno);
		mylog.logException(logmsg);
		exit(-1);
        //return -1;
	}
	mylog.logException("INFO: listen succeed.");
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
		if( -1 == (client.socket_fd = accept(mn_socketToLocal, (struct sockaddr*)&client_addr, &client_len)))
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
		client.status = 1;
		sprintf(logmsg, "INFO: %s:%d --> %s:%d connected, there are %d clients online!", client.clientIP, client.clientPort, client.serverIP, client.serverPort, mn_clientCounts);
		mylog.logException(logmsg);
		//set nonlocking mode
        int flags;
        if( (flags = fcntl(client.socket_fd, F_GETFL, 0)) < 0)
        {
            sprintf(logmsg, "ERROR: %s:%d --> %s:%d: fcntl error: %d--%s. Give up the connection.",client.clientIP, client.clientPort, client.serverIP, client.serverPort, errno, strerror(errno) );
            mylog.logException(logmsg);
            continue;
        }
        fcntl(client.socket_fd, F_SETFL, flags | O_NONBLOCK);
		// keepalive
		int ret = setKeepalive(client.socket_fd, 10, 5, 3);
		if( -1 == ret)
		{
		    mylog.logException("ERROR: set keepalive failed!");
		}
		else
		    mylog.logException("INFO: set keepalive succeed!");
		// get buffer
        int s_length, r_length;
        socklen_t optl = sizeof(s_length);
        getsockopt(client.socket_fd,SOL_SOCKET,SO_SNDBUF,&s_length,&optl);     //获得连接套接字发送端缓冲区的信息
        getsockopt(client.socket_fd,SOL_SOCKET,SO_RCVBUF,&r_length,&optl);     //获得连接套接字的接收端的缓冲区信息
        sprintf(logmsg, "INFO: default send buffer = %d, recv buffer = %d\n",s_length, r_length);
        mylog.logException(logmsg);
        // set buffer
    /*  int nRecvBufSize = 64*1024;//设置为64K
        setsockopt(client.socket_fd,SOL_SOCKET,SO_RCVBUF,(const char*)&nRecvBufSize,sizeof(int));
        int nSendBufSize = 64*1024;//设置为64K
        setsockopt(client.socket_fd,SOL_SOCKET,SO_SNDBUF,(const char*)&nSendBufSize,sizeof(int));
        sprintf(logmsg, "INFO: Set send buffer = %d, recv buffer = %d\n",nSendBufSize, nRecvBufSize);
        mylog.logException(logmsg);
        */

	//	myrecv(client);
	//	mysend(client);
		std::thread th_recv{&MySocket_server::myrecv, this, &client};
		std::thread th_send{&MySocket_server::mysend, this, &client};
	//	std::thread th1{&MySocket_server::recvAndSend, this, client};
		th_recv.join();
		th_send.join();
	//	th1.join();
	//	th1.detach();
	}
	return 0;
}
/*
 *  default send buffer 87040, recv buffer 369280
 *  obsolete
 */
int MySocket_server::recvAndSend(const CONNECTION client)
{
	MSGBODY recvBuf;
	char logmsg[512] = "";
	char logHead[64] = "";
	sprintf(logHead, "%s:%d --> %s:%d ", client.clientIP, client.clientPort, client.serverIP, client.serverPort);

	while(true)
	{
		recvBuf.length = 0;
		memset(recvBuf.msg, 0, sizeof(recvBuf.msg));

		// recv ,  recv() return 0 when connection is closed
		recvBuf.length = recv(client.socket_fd, recvBuf.msg, MAXLENGTH, 0);
		if(-1 == recvBuf.length)     // recv
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
			logMsg(&recvBuf, logHead);
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
					sprintf(logmsg, "INFO: %s recved 1 valid message, add to queue. Total: %u in recv queue, %u in send queue.",logHead, (unsigned int)mp_msgQueueRecv->size(),(unsigned int)mp_msgQueueSend->size());
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
		if(mp_msgQueueSend->empty())
		{
			if(0 != recvBuf.length)
			{
				sprintf(logmsg, "INFO: %s recved %d bytes, send %d bytes", logHead, recvBuf.length, 0 );
				mylog.logException(logmsg);
			}
			continue;
		}
		if(send(client.socket_fd, mp_msgQueueSend->front().msg, mp_msgQueueSend->front().length, 0) == -1)
		{
			sprintf(logmsg, "ERROR: %s: send error: %d--%s\n", logHead, errno, strerror(errno) );
			mylog.logException(logmsg);
		}

		sprintf(logmsg, "INFO: %s recved %d bytes, send %d bytes", logHead, recvBuf.length, mp_msgQueueSend->front().length );
		mylog.logException(logmsg);
		// flush the msg if send
		{
			std::lock_guard<std::mutex> guard(g_sendMutex);
			mp_msgQueueSend->pop();
		}
	//	printf("In the child process, pid is %d.\n", getpid());
	}// end of while
}

/*
 * recv thread function
 *
 */
int MySocket_server::myrecv( CONNECTION * client)
{
    char logmsg[512] = "";
    char logHead[64] = "";
    sprintf(logHead, "%s:%d --> %s:%d ", client->clientIP, client->clientPort, client->serverIP, client->serverPort);

    int length = 0;
    MSGBODY recvMsg;
    while(true)
    {
        recvMsg.length = 0;
        memset(&recvMsg, 0, sizeof(recvMsg));
        // recv head, to get the length of msg
        length = recv(client->socket_fd, &recvMsg, MSGHEAD_LENGTH, 0);
        if(length == -1)     // recv
        {
            if(errno != 11) // data isnot ready when errno = 11, log other error
            {

                sprintf(logmsg, "ERROR: %s: recv error: %d--%s",logHead, errno, strerror(errno) );
                mylog.logException(logmsg);
                if(errno == 9)
                {
                   mylog.logException("ERROR: exit.");
                   return 0;
                }
            }
            //sleep(1);
            usleep(10000);  // 10ms
            length = 0;  // set it back to 0
            continue;
        }
        else                     // recv success
        {
            if( length == 0 )
            {
                close(client->socket_fd);
                // client count -1 when a client exit
                safeDecClientCounts();
                sprintf(logmsg, "INFO: %s: The child process is exit.Stop recving. There are %d clients online.", logHead, mn_clientCounts);
                mylog.logException(logmsg);
                client->status = 0;
                return 0;
            }
        }
        // recv msg, sometimes because of recvMsg.length is 0,it will return 0
        // so it will confirm that recvMsg.length isnot 0
        if(0 != recvMsg.length)
            length = recv(client->socket_fd, recvMsg.msg, recvMsg.length, 0);
        if(length == -1)     // recv
        {
            if(errno != 11) // data isnot ready when errno = 11, log other error
            {
                sprintf(logmsg, "ERROR: %s: recv error: %d--%s",logHead, errno, strerror(errno) );
                mylog.logException(logmsg);
                if(errno == 9)
                {
                   mylog.logException("ERROR: exit.");
                   return 0;
                }
            }
            //sleep(1);
            usleep(10000);  // 10ms
            length = 0;  // set it back to 0
            continue;
        }
        else                     // recv success
        {
            logMsg(&recvMsg, logHead);
            int ret = 0;
            //      ret = msgCheck(&recvBuf);
            if( length == 0 )
            {
                close(client->socket_fd);
                // client count -1 when a client exit
                safeDecClientCounts();
                sprintf(logmsg, "INFO: %s: The child process is exit. Recv thread exit. There are %d clients online.", logHead, mn_clientCounts);
                client->status = 0;
                mylog.logException(logmsg);

                return 0;
            }else if(ret == 1)  // heart beat
            {
                if(send(client->socket_fd, recvMsg.msg, 31, 0) == -1)
                {
                    sprintf(logmsg, "ERROR: %s: heart error: %d--%s",logHead, errno, strerror(errno) );
                    mylog.logException(logmsg);
                }
                mylog.logException("INFO: Get a heartbeat.");
            }
            else if(ret == 0)  // valid msg
            {
                {                // add a lock
                    std::lock_guard<std::mutex> guard(g_recvMutex);
                    if(mp_msgQueueRecv->size() == MAXQUEUELENGTH )
                        mp_msgQueueRecv->pop();
                    mp_msgQueueRecv->push(recvMsg);  // msg push back to the queue
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
int MySocket_server::mysend( CONNECTION * client)
{
    char logmsg[512] = "";
    char logHead[64] = "";
    sprintf(logHead, "%s:%d --> %s:%d ", client->clientIP, client->clientPort, client->serverIP, client->serverPort);
    while(true)
    {
        // send
        if(client->status == 0)
        {
            mylog.logException("INFO: Noticed that connection is closed, send thread exit.");
            return 0;
        }
        if(mp_msgQueueSend->empty())        // nothing to send
        {
         //   mylog.logException("INFO: Nothing to send.");
            sleep(1);
            continue;
        }
        int sendLen = sizeof(mp_msgQueueSend->front().length) + sizeof(mp_msgQueueSend->front().type) + mp_msgQueueSend->front().length;
        if(send(client->socket_fd, &mp_msgQueueSend->front(), sendLen, 0) == -1)
        {
            int err = errno;
            sprintf(logmsg, "ERROR: %s: send error: %d--%s\n", logHead, errno, strerror(errno) );
            mylog.logException(logmsg);
            if(err == EBADF)
            {
                mylog.logException("INFO: Send error, send thread exit.");
                return 0;
            }
        }

        sprintf(logmsg, "INFO: %s send %d bytes", logHead, sendLen );
        mylog.logException(logmsg);
        // flush the msg if send
        {
            std::lock_guard<std::mutex> guard(g_sendMutex);
            mp_msgQueueSend->pop();
        }
    }// end of while
    return 0;
}
int MySocket_server::myconnect(const char* server_IP, int server_port)
{
    char logmsg[512] = "";
    if( inet_pton(AF_INET, server_IP, &m_serverAddr.sin_addr) <= 0)
    {
        sprintf(logmsg, "ERROR: connectTo %s error, inet_pton error: %s\n", server_IP, strerror(errno));
        mylog.logException(logmsg);
        return -1;
    }
    m_serverAddr.sin_port = htons(server_port);
    if( connect(mn_socketToServer, (struct sockaddr*)&m_serverAddr, sizeof(m_serverAddr)) < 0)
    {
        sprintf(logmsg, "ERROR: connectTo %s:%d error: %s(errno: %d)\n", server_IP, server_port, strerror(errno), errno);
        mylog.logException(logmsg);
        return -1;
    }
    // connect success
    // get server address
    CONNECTION myconn;
    memset(&myconn, 0, sizeof(myconn));
    memcpy(myconn.serverIP, server_IP, strlen(server_IP));
    myconn.serverPort = server_port;

    // get client address (namely local address)
    myconn.socket_fd = mn_socketToServer;
    struct sockaddr_in local_addr;
    socklen_t local_len = sizeof(local_addr);
    memset(&local_addr, 0, local_len);
    getsockname(mn_socketToServer, (struct sockaddr *)&local_addr, &local_len);
    inet_ntop(AF_INET,(void *)&local_addr.sin_addr, myconn.clientIP, 64 );
    myconn.clientPort = ntohs(local_addr.sin_port);

    sprintf(logmsg, "INFO: %s:%d --> %s:%d connected", myconn.clientIP, myconn.clientPort, myconn.serverIP, myconn.serverPort);
    mylog.logException(logmsg);
    // set nonblocking mode
    int flags;
    if( (flags = fcntl(mn_socketToServer, F_GETFL, 0)) < 0)
    {
        sprintf(logmsg, "ERROR: fcntl error: %d--%s", errno, strerror(errno) );
        mylog.logException(logmsg);
        return -1;
    }
    fcntl(mn_socketToServer, F_SETFL, flags | O_NONBLOCK);
    return 0;
}
/*
 * get msg from somewhere
 */
int MySocket_server::getMsg()
{
   // fgets((char *)m_sendBuf.msg, sizeof(m_sendBuf.msg), stdin);
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
 * to check if a msg is valid and legal
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
/*
 * log Msg
 * when it is hex,    log the lex
 *      it is string, log the string
 */
int MySocket_server::logMsg(const MSGBODY *recvMsg, const char *logHead)
{
    char logmsg[256] = "";
    if(2 == recvMsg->type)             //  hex
    {
        try
        {
            char *p_hexLog = new char[recvMsg->length*3 + 128];    // include the logHead
            memset(p_hexLog, 0, recvMsg->length*3 + 128);
            sprintf(p_hexLog, "INFO: %s recved: ", logHead);
            int len = strlen(p_hexLog);
            for(int i=0; i<recvMsg->length; i++)
                sprintf(p_hexLog+len+3*i, "%02x ", (unsigned char)recvMsg->msg[i]);
            mylog.logException(p_hexLog);
            delete[] p_hexLog;
        }catch(bad_alloc& bad)
        {
            sprintf(logmsg,"ERROR: Failed to alloc mem when log hex: %s", bad.what());
            mylog.logException(logmsg);
        }
    }
    else if(1 == recvMsg->type)
    {
        char logmsg[recvMsg->length + 128];
        memset(logmsg, 0, recvMsg->length + 128);
        sprintf(logmsg, "INFO: %s recved: %s", logHead, recvMsg->msg);
        mylog.logException(logmsg);
    }
    return 0;
}
