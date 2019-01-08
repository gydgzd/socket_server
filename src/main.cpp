//============================================================================
// Name        : socket_server.cpp
// Author      : gyd
// Version     :
// Copyright   : Copyright cnki.
// Description : Hello World in C++, Ansi-style
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

	queue<MSGBODY> msgQueueFromUp;
	queue<MSGBODY> msgQueueFromDown;

 	MySocket_server serverToUp;
 	MySocket_server serverToDown;
 	serverToUp.init(3401, &msgQueueFromUp, &msgQueueFromDown);
 	std::thread th1{ &MySocket_server::serv, &serverToUp};

 	serverToDown.init(3402, &msgQueueFromDown, &msgQueueFromDown);
 	std::thread th2{ &MySocket_server::serv, &serverToDown};

 	th1.join();
 	th2.join();

	return 0;
}
