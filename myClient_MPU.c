#include "myClient_MPU.h"

//自定义协议控制信息结构体初始化
void my_conn_init(struct my_conn *connHandle,int32_t socket)
{
    time_t curtime = time(NULL);
    connHandle->m_socket = socket;
    connHandle->m_topic = MYTOPIC;
    connHandle->m_sub_topic = MYSUBTOPIC;
    connHandle->live_time_out = 10;	//至少10s发送一次报文
    connHandle->live_time_in = 15;		//超过15s未接收则关闭链接
    connHandle->m_timeout_in = curtime + connHandle->live_time_in;
    connHandle->m_timeout_out = curtime + connHandle->live_time_out;
    connHandle->m_read_idx = 0;
}

//写一个字节
static void writeChar(unsigned char** pptr, char c)
{
	**pptr = c;
	(*pptr)++;
}
//写二个字节
static void writeInt16(unsigned char** pptr, uint16_t anInt)
{
	**pptr = (unsigned char)(anInt / 256);
	(*pptr)++;
	**pptr = (unsigned char)(anInt % 256);
	(*pptr)++;
}
//写字符串
static void writeCString(unsigned char** pptr, const char* string)
{
	int len = strlen(string);
	writeChar(pptr, len);
	memcpy(*pptr, string, len);
	*pptr += len;
}

//发送连接报文
uint8_t send_connect_packet(struct my_conn *connHandle)
{
    uint8_t buf[200];
    unsigned char *ptr = buf;

    int len = 0;
    //组装消息
    uint8_t header = CONNECT<<4;
    writeChar(&ptr, (char)header);
    //写剩余长度可最后再写，先把坑占了
    writeInt16(&ptr, 0);
    //topic长度 + topic
    writeCString(&ptr, connHandle->m_topic);
    //发送消息
    len = ptr - buf;
    //把剩余长度写回去
    ptr = buf + 1;
    writeInt16(&ptr, len - 3);
    int32_t ret;
    ret = write(connHandle->m_socket, buf, len);
    //发送成功判断
    if(ret < 0)
    {
        PRINTF("SEND CONNECT PACKET ERROR");
        return 0;
    }
    return 1;
}
//发送订阅报文
static uint8_t send_subscribe_packet(struct my_conn *connHandle)
{
    uint8_t buf[200];
    unsigned char *ptr = buf;

    int len = 0;
    //组装消息
    uint8_t header = SUBSCRIBE<<4;
    writeChar(&ptr, (char)header);
    //写剩余长度可最后再写，先把坑占了
    writeInt16(&ptr, 0);
    //topic长度 + topic
    writeCString(&ptr, connHandle->m_sub_topic);
    //发送消息
    len = ptr - buf;
    //把剩余长度写回去
    ptr = buf + 1;
    writeInt16(&ptr, len - 3);
    int32_t ret;
    ret = write(connHandle->m_socket, buf, len);
    //发送成功判断
    if(ret < 0)
    {
        PRINTF("SEND SUBSCRIBE PACKET ERROR");
        return 0;
    }
    return 1;
}
//发送发布报文
static uint8_t send_publish_packet(struct my_conn *connHandle)
{
    uint8_t buf[200];
    unsigned char *ptr = buf;

    int len = 0;
    //组装消息
    uint8_t header = SUBSCRIBE<<4;
    writeChar(&ptr, (char)header);
    //写剩余长度可最后再写，先把坑占了
    writeInt16(&ptr, 0);
    //topic长度 + topic
    writeCString(&ptr, connHandle->m_sub_topic);
    //发送消息，随机发送灯控制信息
    writeChar(&ptr, time(NULL)%4);
    len = ptr - buf;
    //把剩余长度写回去
    ptr = buf + 1;
    writeInt16(&ptr, len - 3);
    int32_t ret;
    ret = write(connHandle->m_socket, buf, len);
    //发送成功判断
    if(ret < 0)
    {
        PRINTF("SEND PUBLISH PACKET ERROR");
        return 0;
    }
    return 1;
}

//发送心跳报文
static uint8_t send_ping_packet(struct my_conn *connHandle)
{
    uint8_t buf[200];
    unsigned char *ptr = buf;
    int len = 0;
    //组装消息
    uint8_t header = PINGREQ<<4;
    writeChar(&ptr, (char)header);
    //写剩余长度可最后再写，先把坑占了
    writeInt16(&ptr, 0);
    //topic长度 + topic
    writeCString(&ptr, connHandle->m_topic);
    //发送消息
    len = ptr - buf;
    //把剩余长度写回去
    ptr = buf + 1;
    writeInt16(&ptr, len - 3);
    int32_t ret;
    ret = write(connHandle->m_socket, buf, len);
    //发送成功判断
    if(ret < 0)
    {
        PRINTF("SEND PING PACKET ERROR");
        return 0;
    }
    return 1;
}
//接收发布报文
static void recv_publish_packet(struct my_conn *connHandle)
{
    uint16_t remain_len = (uint16_t)*(connHandle->m_read_buf + 1);
    uint8_t topic_len = (uint8_t)*(connHandle->m_read_buf + 3);
    //对比topic 后续可用链表，不同的topic进入不同的处理函数
    //STM32H7_func(connHandle->m_read_buf + 4);
    return ;
}
//接收心跳报文
static void recv_ping_packet(struct my_conn *connHandle)
{
    uint16_t remain_len = (uint16_t)*(connHandle->m_read_buf + 1);
    uint8_t topic_len = (uint8_t)*(connHandle->m_read_buf + 3);
    //对比topic
    int i = 0;
    char *msg_topic = connHandle->m_read_buf + 4;
    const char *my_topic = connHandle->m_topic;
    for(i = 0; i < topic_len; i ++)
    {
        if(*(msg_topic + i) != *(my_topic + i))
        {
            PRINTF("PING RECV NO MATCH");
            return;
        }
    }
    //更新读超时时间
    time_t curtime = time(NULL);
    connHandle->live_time_in = curtime + connHandle->live_time_in;
    return ;
}


//读线程
static void *my_conn_recv(void *arg)
{
    struct my_conn *connHandle = (struct my_conn *)arg;
    int num;
    uint8_t packet_type = 0;
	time_t curtime;
    for(;;)
    {   
        //阻塞等待
        num = read(connHandle->m_socket, connHandle->m_read_buf, READ_BUFFER_SIZE);
        if(num <= 3)
		{
		    PRINTF("RECV TOO SHOTE!\n");
            continue;
		}
        //更新读超时时间
        curtime = time(NULL);
        connHandle->m_timeout_in = curtime + connHandle->live_time_in;
        //解包 收到亮灯指令 亮灯颜色
        connHandle->m_read_idx = num;
        packet_type = (*(connHandle->m_read_buf) & 0xF0) >> 4;
        switch(packet_type)
		{
			case PUBLISH:
				recv_publish_packet(connHandle);
				break;
            case PINGRESP:
                recv_ping_packet(connHandle);
				break;
            default:
                PRINTF("RECV NO MATCH");
                break;
		}
    }
	
}
//写线程
static void *my_conn_send(void *arg)
{
    struct my_conn *connHandle = (struct my_conn *)arg;
	time_t curtime;
    //发送订阅消息
    int ret = send_subscribe_packet(connHandle);
    if(ret == 0)
    {
        PRINTF("SEND SUBSCRIBE_PACKET ERROR");
    }
    else
    {
        //更新写超时时间
        curtime = time(NULL);
        connHandle->m_timeout_out = curtime + connHandle->live_time_out;
    }
    //推送消息 
    for(;;)
    {
		ret = send_publish_packet(connHandle);
		if(ret == 0)
    	{
        	PRINTF("SEND SUBSCRIBE_PACKET ERROR");
    	}
	    else
	    {
	        //更新写超时时间
	        curtime = time(NULL);
	        connHandle->m_timeout_out = curtime + connHandle->live_time_out;
	    }
        sleep(8);
    }
}
//心跳报文检查线程
static void *my_ping_check(void *arg)
{
    struct my_conn *connHandle = (struct my_conn *)arg;
	time_t curtime;
    for(;;)
    {
        sleep(2);
        curtime = time(NULL);
        if(connHandle->m_timeout_in < curtime)
        {
            //接收超时，应断开连接
            PRINTF("RECV PING ERROR");
        }
        if(connHandle->m_timeout_out < curtime)
        {
            send_ping_packet(connHandle);
            //更新写超时时间
            curtime = time(NULL);
            connHandle->m_timeout_out = curtime + connHandle->live_time_out;
        }
    }

}


int main(int argc, char *argv[])
{

    char buf[BUFSIZ];
    int cfd;
    struct sockaddr_in serv_addr;          //服务器地址结构

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERV_PORT);
    serv_addr.sin_addr.s_addr = 184920256;//192.168.5.11
   

    cfd = socket(AF_INET, SOCK_STREAM, 0);
    if (cfd == -1)
        sys_err("socket error");

    int ret = connect(cfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    if (ret != 0)
        sys_err("connect err");

	
	struct my_conn *my_connHandler;
    //记得释放
    my_connHandler = (struct my_conn *)malloc(sizeof(struct my_conn));
    //自定义协议初始化init
    my_conn_init(my_connHandler, cfd);
    //发送连接报文
	send_connect_packet(my_connHandler);
    sleep(1);
    //创建自定义协议读写线程
	pthread_t recv_id, send_id,ping_id;
	ret = pthread_create(&recv_id, NULL, my_conn_recv, (void *)my_connHandler);
	if (ret)
		sys_err("pthread_create err!\n");
	ret = pthread_create(&send_id, NULL, my_conn_send, (void *)my_connHandler);
	if (ret)
		sys_err("pthread_create err!\n");
	ret = pthread_create(&ping_id, NULL, my_ping_check, (void *)my_connHandler);
	if (ret)
		sys_err("pthread_create err!\n");

	
	while(1)
	{
		sleep(100);
	}

    close(cfd);

	return 0;
}

