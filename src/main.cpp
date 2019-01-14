//============================================================================
// Name        : socket_server.cpp
// Author      : gyd
// Version     :
// Copyright   : Copyright cnki.
// Description : socket server in C++, Ansi-style
//============================================================================
#include <thread>
using namespace std;

#include "MySocket_server.h"
extern void init_daemon();
extern int fileTransfer(int argc, char **argv);
int main(int argc, char **argv) {
	if(argc == 1 || 0 != strcmp(argv[1],"-d"))
	{
		printf("Enter silent mode\n");
//		init_daemon();
	}

	queue<MSGBODY> msgQueueFromClient;
	queue<MSGBODY> msgQueueToServer;

 	MySocket_server myServer;


 	myServer.init( &msgQueueFromClient, &msgQueueToServer);
 	std::thread th1{ &MySocket_server::serv, &myServer};
 	//

 	myServer.myconnect( );

 	th1.join();
	return 0;
}
