/*
 * fileTransfer.cpp
 *
 *  Created on: Jun 9, 2017
 *      Author: gyd
 */

 /*
  * file_server.c -- socket文件传输服务器端示例代码
  */
#include<errno.h>
#include<netinet/in.h>
#include<sys/types.h>
#include<sys/socket.h>
#include <sys/stat.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include <unistd.h>       // close()
#define SERVER_PORT    6666
#define LENGTH_OF_LISTEN_QUEUE     20
#define BUFFER_SIZE                102400
#define FILE_NAME_MAX_SIZE         512

int fileTransfer(int argc, char **argv)
{
    // set socket's address information
    // 设置一个socket地址结构server_addr,代表服务器internet的地址和端口
    struct sockaddr_in   server_addr;
    memset(&server_addr,0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(SERVER_PORT);

    // create a stream socket
    // 创建用于internet的流协议(TCP)socket，用server_socket代表服务器向客户端提供服务的接口
    int server_socket = socket(PF_INET, SOCK_STREAM, 0);
    if (server_socket < 0)
    {
    	printf("Server create socket error: %s(errno: %d)\n", strerror(errno), errno);
        exit(-1);
    }
    // 把socket和socket地址结构绑定
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)))
    {
    	printf("Server port %d bind error: %s (errno: %d)\n",SERVER_PORT, strerror(errno), errno);
        exit(-1);
    }
    // server_socket用于监听
    if (listen(server_socket, LENGTH_OF_LISTEN_QUEUE))
    {
    	printf("Server listen error: %s (errno: %d)\n", strerror(errno), errno);
        exit(-1);
    }
    // 服务器端一直运行用以持续为客户端提供服务
    while(1)
    {
    	long fileSize = -1;
    	long sendSize = 0;
        // 定义客户端的socket地址结构client_addr，当收到来自客户端的请求后，调用accept
        // 接受此请求，同时将client端的地址和端口等信息写入client_addr中
        struct sockaddr_in client_addr;
        socklen_t length = sizeof(client_addr);

        // accpet返回一个新的socket,这个socket用来与此次连接到server的client进行通信
        // 这里的conn_socket代表了这个通信通道
        int conn_socket = accept(server_socket, (struct sockaddr*)&client_addr, &length);
        if (conn_socket < 0)
        {
            printf("Server Accept Failed!");
            break;
        }
        // 从客户端获取要传输的文件名
        char buffer[BUFFER_SIZE] = "";
        length = recv(conn_socket, buffer, sizeof(buffer), 0);
        if (length < 0)
        {
            printf("Server Recieve Data Failed!");
            break;
        }

        char fileName[FILE_NAME_MAX_SIZE + 1];
        memset(fileName, 0, sizeof(fileName));
        strncpy(fileName, buffer, strlen(buffer) > FILE_NAME_MAX_SIZE ? FILE_NAME_MAX_SIZE : strlen(buffer));
        // 准备打开文件
        struct stat buf;
        if((stat(fileName,&buf)!=-1))
		{
			fileSize = buf.st_size;
			printf("The size of the file is: %ld",fileSize);
		}
		else
		{
			perror(fileName);
			exit(EXIT_FAILURE);
		}
        char size[32] = "";
        sprintf(size,"%ld",fileSize);
        printf("客户端请求打开文件： %s, 大小%ld字节。\n",fileName, fileSize);
        if (send(conn_socket, size, strlen(size), 0) < 0)
		{
			printf("Send size of file:  %s Failed!\n", fileName);
			break;
		}
        FILE *fp = fopen(fileName, "r");
        if (fp == NULL)
        {
            printf("ERR: Open file %s failed: %s\n", fileName, strerror(errno));
        }
        else
        {
        	memset(buffer, 0, BUFFER_SIZE);
            int file_block_length = 0;
            while( (file_block_length = fread(buffer, sizeof(char), BUFFER_SIZE, fp)) > 0)
            {
            	sendSize += file_block_length;
                printf("Sending %3.2f%%\r", 100.0*sendSize/fileSize);
                fflush(stdout);
                // 发送buffer中的字符串到new_server_socket,实际上就是发送给客户端
                if (send(conn_socket, buffer, file_block_length, 0) < 0)
                {
                    printf("Send File:  %s Failed!", fileName);
                    break;
                }
                memset(buffer, 0, sizeof(buffer));
            }
            fclose(fp);
            printf("\nFile: %s Transfer Finished!\n", fileName);
        }
        close(conn_socket);
    }
    close(server_socket);
    return 0;
}


