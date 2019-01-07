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
#include <set>
#include <queue>
#include <thread>
#include <mutex>
#include <new>       // to catch bad alloc
#include "Mylog.h"
using namespace std;
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
/*
 * to be added: msg type, is compressed and so on
 */
struct MSGBODY
{
    int type;              // 0:int, 1:string, 2: byte(hex)
    int length;
    BYTE msg[MAXLENGTH];
};
/*
 * use to form a string clientIP:clientPort--> serverIP:serverPort
 */
struct CONNECTION
{
    int  socket_fd = 0;       //
    int  clientPort = 0;      // test if ok
    int  serverPort = 0;
    char clientIP[64] = "";
    char serverIP[64] = "";
};

class MySocket_server
{
public:
    int mn_socket_fd;                     // for listen use
//	int mn_connect_fd;                    // for connection use
    MySocket_server();
    ~MySocket_server();

    int init(int listenPort, queue<MSGBODY> * msgQToRecv, queue<MSGBODY> * msgQToSend); // socket(),get ready to communicate.

    int connectTo(char* server_IP);       // connect
    int serv();

    int recvAndSend(const CONNECTION client);
    int myrecv(const CONNECTION client);  // recv thread function
    int mysend(const CONNECTION client);  // send thread function
    int getMsg();
    int sendMsg();                        // transfer message
    int fileSend();                       // transfer file

private:

    int mn_clientCounts;        // if send counts equal to client counts,
    int mn_clientSend;          // remove one msg from the send queue
    int mn_isFlushed;           // if the send buffer flushed(flushed is 1). When flushed, each client can recv new msg
    struct sockaddr_in m_serverAddr;
    vector<string> mv_clients;
    set<string>    mset_clients;      // storage of all client ip that connected to server

    static queue<MSGBODY>  m_msgQueueRecv;  // a queue to storage the msg
    static queue<MSGBODY>  m_msgQueueSend;

    queue<MSGBODY> * mp_msgQueueRecv; // pointer to queue
    queue<MSGBODY> * mp_msgQueueSend;

    Mylog mylog;

    int safeAddClientCounts();          // safely add a client count
    int safeDecClientCounts();          // safely desc a client count
    int msgCheck(const MSGBODY *msg);
    int setKeepalive(int fd, int idle = 10, int interval = 5, int probe = 3);
    int getMyIP();
	int myclose();                      // close socket
};
#endif /* MYSOCKET_SERVER_H_ */
