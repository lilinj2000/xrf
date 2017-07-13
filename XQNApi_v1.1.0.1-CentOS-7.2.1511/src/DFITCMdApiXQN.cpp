#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <sched.h>
#include <sys/socket.h>
#include <sys/timeb.h>
#include <string.h>
#include <sys/types.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <fcntl.h>
#include "DFITCMdApiXQN.h"

//接收缓冲区大小
#define MAXLINE 2048
//行情报文长度
#define PACK_LEN 100

int sockfd;//服务器套接字
struct sockaddr_in servaddr;//服务器地址
struct sockaddr_in cliaddr;//客户端地址
socklen_t len = sizeof(cliaddr);//客户端地址长度
bool g_initOK = false;


DFITCMDAPIXQN::DFITCMdSpi *g_spi = NULL;

//线程绑定cpu
static void pthread_bind_cpu(int cpu_index)
{
	cpu_set_t mask;
	CPU_ZERO(&mask);
	CPU_SET(cpu_index, &mask);
	printf("cpu num: %ld\n", sysconf(_SC_NPROCESSORS_CONF));
	if (cpu_index >= sysconf(_SC_NPROCESSORS_CONF))
	{
		printf("bind cpu failed\n");
		return;
	}
	if (pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask) < 0)
	{
		printf("setaffinity_error\n");
	}
	else
	{
		printf("thread bind cpu %d\n",cpu_index);
	}
}

static void socketRecv()
{
	DFITCMarketDataFieldXQN recvMarketData;//行情结构体数据/	char* addr_reg;
	int n = 0;
	char* addr_reg;
	//char buffer[MAXLINE];//存储接收到的行情数据
	memset(&recvMarketData,0,sizeof(recvMarketData));
	while(1)
	{
		//n = recvfrom(sockfd, &recvMarketData, MAXLINE, 0, (struct sockaddr *)&cliaddr, &len);
		n = read(sockfd, &recvMarketData, PACK_LEN);
		if(n < 0)
			continue;
		
		if (g_spi)
			g_spi->OnMarketData(&recvMarketData);
		memset(&recvMarketData,0,sizeof(recvMarketData));
	}
	return;
}


class DFITCMdAPIEx : public DFITCMDAPIXQN::DFITCMdApi
{
public:
	DFITCMdAPIEx();
	~DFITCMdAPIEx();

	//初始化
	//
	virtual int Init(const char *multicast_addr, long port, const char *local_addr, int cpu_core, DFITCMDAPIXQN::DFITCMdSpi *pSpi)
	{
		if (!multicast_addr || !pSpi)
			return -1;

		printf("port = %ld, ip_addr = %s\n",port, multicast_addr);
		if (cpu_core >= 0)
			pthread_bind_cpu(cpu_core);

		sockfd = socket(AF_INET, SOCK_DGRAM, 0); //创建UDP套接字
	
		//初始化服务器地址
		bzero(&servaddr, sizeof(servaddr));
		servaddr.sin_family = AF_INET;
		servaddr.sin_addr.s_addr = htonl(INADDR_ANY);//接收本机任何网卡数据
		servaddr.sin_port = htons(port);//本机接收数据端口
	
		//设定SO_REUSEADDR，允许多个应用绑定同一个本地端口接收数据包
		int reuse = 1;
		if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse)) < 0)
		{
			perror("set SO_REUSEADDR error");
			//close(sockfd);
			//exit(-3);
		}

		//将服务器地址和服务器套接字绑定
		if(bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1)
		{
			perror("bind socket error");
			//exit(-4);
		}

		struct in_addr addr = {0};
		addr.s_addr = inet_addr(local_addr);

		if(-1 == setsockopt(sockfd, IPPROTO_IP, IP_MULTICAST_IF, (char *)&addr, sizeof(addr)))
		{
			printf("set error IP_MULTICAST_IF\n");
		}

		//设置回环许可
		int loop = 1;
		if(setsockopt(sockfd,IPPROTO_IP, IP_MULTICAST_LOOP,&loop, sizeof(loop)) < 0)
		{
			perror("setsockopt():IP_MULTICAST_LOOP");
			//exit(-5);
		}
		//设置要加入组播的地址
		struct ip_mreq mreq;
		bzero(&mreq, sizeof (struct ip_mreq)); 
		mreq.imr_multiaddr.s_addr = inet_addr(multicast_addr); //广播地址
		//设置发送组播消息的源主机的地址信息
		//mreq.imr_interface.s_addr = htonl (INADDR_ANY);
		mreq.imr_interface.s_addr = addr.s_addr;
		//把本机加入组播地址，即本机网卡作为组播成员，只有加入组才能收到组播消息

		if (setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq,sizeof (struct ip_mreq)) == -1)
		{
			perror ("setsockopt");
			//exit(-6);
		}

		//设置TTL
		unsigned char ttl = 255;
		if(setsockopt(sockfd,IPPROTO_IP,IP_MULTICAST_TTL,&ttl,sizeof(ttl)) < 0)
		{
			perror("setsockopt():IP_MULTICAST_TTL\n");
		}

		//设置非阻塞recv
		int flag;
		if (flag = fcntl(sockfd, F_GETFL, 0) < 0)
			perror("get flag error");
		flag |= O_NONBLOCK;
		if (fcntl(sockfd, F_SETFL, flag) < 0)
			perror("set flag error");
	
		g_spi = pSpi;
		g_initOK = true;	
		printf("Init over\n");
		return 0;
	}
	
	virtual int Run()
	{
		if (!g_initOK)
		{
			printf("Init error\n");
			return -1;
		}
		socketRecv();
		return 0;
	}
};

//////////////////////////////////////////////////////////////////////////
DFITCMDAPIXQN::DFITCMdApi * DFITCMDAPIXQN::DFITCMdApi::CreateDFITCMdApi()
{
	return new DFITCMdAPIEx();
}

DFITCMdAPIEx::DFITCMdAPIEx()
{

}

DFITCMDAPIXQN::DFITCMdApi::~DFITCMdApi()
{
	
}

DFITCMdAPIEx::~DFITCMdAPIEx()
{

}
