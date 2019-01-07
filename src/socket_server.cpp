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

		if(!fork())
		{
			while(true)
			{
				n = recv(connect_fd, buff, MAXLINE, 0);
				buff[n] = '\0';
				if(send(connect_fd, "Hello, this is server!\n", 23, 0) == -1)
					perror("send error");
				printf("recv msg from client: %s", buff);
				if(strcmp(buff,"exit\n")==0)
				{
					close(connect_fd);
					close(socket_fd);
					printf("INFO: The child process is exit.\n");
					exit(0);
				}
				printf("In the child process, pid is %d.\n", getpid());
			}
		}
	}
	close(socket_fd);
	return 0;
}
