//============================================================================
// Name        : socket_server.cpp
// Author      : gyd
// Version     :
// Copyright   : gyd
// Description : Hello World in C++, Ansi-style
//============================================================================
#include "socket_server.h"

int socket_server()
{
	int socket_fd, connect_fd;
	int n;
	struct sockaddr_in servaddr;
	char buff[4096];
	// initialize
	if( (socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1 )
	{
		printf("Create socket error: %s (errno: %d)\n", strerror(errno), errno);
		exit(0);
	}
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);  //
	servaddr.sin_port = htons(DEFAULT_PORT);       //
	int optval = 1;
    if(-1 == setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)))
    {
        printf("ERROR: Reuse addr error: %s (errno: %d). Program exit.", strerror(errno), errno);
        exit(-1);
        //return -1;
    }
	// bind
	if( bind(socket_fd, (struct sockaddr*)&servaddr, sizeof(servaddr)) == -1)
	{
		printf("Bind error: %s (errno: %d)\n", strerror(errno), errno);
		exit(0);
	}
	if( listen(socket_fd, 10) == -1)
	{
		printf("Listen error: %s (errno: %d)\n", strerror(errno), errno);
		exit(0);
	}
	//
	while(true)
	{
		if( (connect_fd = accept(socket_fd, (struct sockaddr*)NULL, NULL)) == -1)
		{
			printf("Accept error: %s (errno: %d)\n", strerror(errno), errno);
			continue;
		}

	//	if(!fork())
	//	{
			while(true)
			{
				std::string msg = "##**01470000json{\"funcName\":\"acqState\",\"param\":{\"uuid\":\"servers_3a6a305c-4b07-4aed-8f24-dedd4a24f18d\",\"state\":\"start\",\"userId\":\"99f371f98c5342c3a5477bddae1b45ad\"}}**##";
				std::string heart = "##**00840000json{\"funcName\":\"HETP\",\"uuid\":\"servers_624854a7-91ca-491b-8eeb-779d2e4cf9bd\",\"param\":\"\"}**##";
			//	std::string prestart = "##**01550000json{\"funcName\":\"ProcessRestart\",\"uuid\":\"servers_624854a7-91ca-491b-8eeb-779d2e4cf9bd\",\"param\":{\"type\":\"SLWLMT\",\"processName\":\"mysqld\",\"userId\":\"32bitstring\"}}**##";
				//	if(send(connect_fd, heart.c_str(), heart.length(), 0) == -1)
			//	    perror("send error");
				int ret = send(connect_fd, heart.c_str(), heart.length(), 0);
				if( ret == -1)
					perror("send error");
				else
				    printf("send msg: %s\n", heart.c_str());
				sleep(1);
				ret = send(connect_fd, msg.c_str(), msg.length(), 0);
                if( ret == -1)
                    perror("send error");
                else
                    printf("send msg: %s\n", msg.c_str());
				n = recv(connect_fd, buff, MAXLINE, 0);
				buff[n] = '\0';
				printf("recv msg from client: %s\n", buff);
				if(strcmp(buff,"exit\n")==0)
				{
					close(connect_fd);
					close(socket_fd);
					printf("INFO: The child process is exit.\n");
					exit(0);
				}
				memset(buff, 0, sizeof(buff));
				printf("In the child process, pid is %d.\n", getpid());
			}
	//	}
	}
	close(socket_fd);
	return 0;
}
