#ifndef MY_CLIENT_H
#define MY_CLIENT_H

#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>

#define  PRINTF printf

#define SERV_PORT 8080
#define READ_BUFFER_SIZE 1024

const char *MYTOPIC = "STM32MP157";
const char *MYSUBTOPIC = "STM32H7";

void sys_err(const char *str)
{
	perror(str);
	exit(1);
}

//报文类型
enum msgTypes
{
	CONNECT = 1, CONNACK, PUBLISH, PUBACK, PUBREC, PUBREL,
	PUBCOMP, SUBSCRIBE, SUBACK, UNSUBSCRIBE, UNSUBACK,
	PINGREQ, PINGRESP, DISCONNECT
};
//灯控制
enum msgCtl
{
	LEDOFF = 0, LEDRED, LEDGREEN, LEDBLUE
};
	
//自定义协议客户端结构体
struct my_conn
{
    int32_t m_socket;                       //客户端套接字
    const char *m_topic;                    //客户端主题
    const char *m_sub_topic;                //订阅主题
    time_t live_time_out;                 //10s发送一次心跳报文
    time_t live_time_in;                  //读报文超时时间最多为15s
    time_t m_timeout_in;                  //读报文超时时间
    time_t m_timeout_out;                 //写报文超时时间
    char m_read_buf[READ_BUFFER_SIZE];      //读缓冲区
    int m_read_idx;                         //读到的长度
};

#endif


