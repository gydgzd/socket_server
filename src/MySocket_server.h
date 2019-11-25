/*
 * MySocket_server.h
 * Defined my socket server class:
 * How to use:
 * init->recv or send
 *
 *  Created on: Jun 15, 2017
 *      Author: gyd
 */

#ifndef MYSOCKET_SERVER_H_
#define MYSOCKET_SERVER_H_

#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>      // for close , usleep
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>   // inet_pton
#include <fcntl.h>       // fcntl
#include <netinet/tcp.h> // TCP
#include <iostream>
#include <vector>
#include <list>
#include <set>
#include <map>
#include <queue>
#include <thread>
#include <mutex>
#include <new>       // to catch bad alloc
#include <signal.h>
#include "Mylog.h"
using namespace std;
#define CONFIGFILE "config.conf"
#define TOUP_PORT 39401
#define TODOWN_PORT 39402
#define MAXLENGTH 1024*64
#define MAXQUEUELENGTH 10    // max 10 elements in queue
#ifndef BYTE
typedef unsigned char BYTE;
#endif

extern std::mutex g_recvMutex;
extern std::mutex g_sendMutex;
extern std::mutex g_clientNumMutex;

#define MSGHEAD_LENGTH 8
struct MSGBODY
{
    int type;              // 0:int, 1:string, 2: byte(hex)
    int length;            // length of msg
    BYTE msg[MAXLENGTH];
    MSGBODY()
    {
        memset(this, 0, sizeof(MSGBODY));
    }
    MSGBODY(const MSGBODY & msgbody)
    {
        memset(this, 0, sizeof(MSGBODY));
        type = msgbody.type;
        length = msgbody.length;
        memcpy(msg,msgbody.msg,length);
    }

};
/*
 * use to form a string clientIP:clientPort--> serverIP:serverPort
 */
struct CONNECTION
{
    int  socket_fd ;       //
    int  status ;          // 0: closed; 1:connected
    int  clientPort ;      // test if ok
    int  serverPort ;
    char clientIP[64] ;
    char serverIP[64] ;
    CONNECTION()
    {
        memset(this, 0, sizeof(CONNECTION));
    }
};

class MySocket_server
{
public:

    MySocket_server();
    ~MySocket_server();

    int loadConfig();
    // as a server
    int init( queue<MSGBODY> * msgQToRecv, queue<MSGBODY> * msgQToSend); // socket(),get ready to communicate.
    int serv();
    int recvAndSend(const CONNECTION client);
    int myrecv( std::list<CONNECTION>::reverse_iterator conn);      // recv thread function
    int mysend( std::list<CONNECTION>::reverse_iterator conn);      // send thread function
    // as a client
    int myconnect();// connect

    int getMsg();
    int fileSend();                         // transfer file

private:
    int mn_socketToLocal;                   // for listen use (in init)
    int mn_socketToServer;                  // for connect use

    int mn_clientCounts;                    // if send counts equal to client counts,
//    int mn_clientSend;                    // remove one msg from the send queue

    struct sockaddr_in m_localAddr;         // local address, for bind as a server
    struct sockaddr_in m_serverAddr;        // server address,for connect

    list<CONNECTION> ml_conns;
    set<string>    mset_clients;            // storage of all client ip that connected to server
    map<string,string> mmap_config;         // map for config

    static queue<MSGBODY>  m_msgQueueRecv;  // a queue to storage the msg
    static queue<MSGBODY>  m_msgQueueSend;

    queue<MSGBODY> * mp_msgQueueRecv;       // pointer to recv queue
    queue<MSGBODY> * mp_msgQueueSend;

    Mylog mylog;

    int safeAddClientCounts();              // safely add a client count
    int safeDecClientCounts();              // safely desc a client count
    int msgCheck(const MSGBODY *msg);
    int setKeepalive(int fd, int idle = 10, int interval = 5, int probe = 3);
    int logMsg(const MSGBODY *pMsg, const char *logHead, int isRecv);
    int reconnect(int& socketfd, struct sockaddr_in& addr);

    int getMyIP();
	int myclose();                      // close socket
};
#endif /* MYSOCKET_SERVER_H_ */
