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

//���ջ�������С
#define MAXLINE 2048
//���鱨�ĳ���
#define PACK_LEN 100

int sockfd;//�������׽���
struct sockaddr_in servaddr;//��������ַ
struct sockaddr_in cliaddr;//�ͻ��˵�ַ
socklen_t len = sizeof(cliaddr);//�ͻ��˵�ַ����
bool g_initOK = false;


DFITCMDAPIXQN::DFITCMdSpi *g_spi = NULL;

//�̰߳�cpu
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
	DFITCMarketDataFieldXQN recvMarketData;//����ṹ������/	char* addr_reg;
	int n = 0;
	char* addr_reg;
	//char buffer[MAXLINE];//�洢���յ�����������
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

	//��ʼ��
	//
	virtual int Init(const char *multicast_addr, long port, const char *local_addr, int cpu_core, DFITCMDAPIXQN::DFITCMdSpi *pSpi)
	{
		if (!multicast_addr || !pSpi)
			return -1;

		printf("port = %ld, ip_addr = %s\n",port, multicast_addr);
		if (cpu_core >= 0)
			pthread_bind_cpu(cpu_core);

		sockfd = socket(AF_INET, SOCK_DGRAM, 0); //����UDP�׽���
	
		//��ʼ����������ַ
		bzero(&servaddr, sizeof(servaddr));
		servaddr.sin_family = AF_INET;
		servaddr.sin_addr.s_addr = htonl(INADDR_ANY);//���ձ����κ���������
		servaddr.sin_port = htons(port);//�����������ݶ˿�
	
		//�趨SO_REUSEADDR��������Ӧ�ð�ͬһ�����ض˿ڽ������ݰ�
		int reuse = 1;
		if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse)) < 0)
		{
			perror("set SO_REUSEADDR error");
			//close(sockfd);
			//exit(-3);
		}

		//����������ַ�ͷ������׽��ְ�
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

		//���ûػ����
		int loop = 1;
		if(setsockopt(sockfd,IPPROTO_IP, IP_MULTICAST_LOOP,&loop, sizeof(loop)) < 0)
		{
			perror("setsockopt():IP_MULTICAST_LOOP");
			//exit(-5);
		}
		//����Ҫ�����鲥�ĵ�ַ
		struct ip_mreq mreq;
		bzero(&mreq, sizeof (struct ip_mreq)); 
		mreq.imr_multiaddr.s_addr = inet_addr(multicast_addr); //�㲥��ַ
		//���÷����鲥��Ϣ��Դ�����ĵ�ַ��Ϣ
		//mreq.imr_interface.s_addr = htonl (INADDR_ANY);
		mreq.imr_interface.s_addr = addr.s_addr;
		//�ѱ��������鲥��ַ��������������Ϊ�鲥��Ա��ֻ�м���������յ��鲥��Ϣ

		if (setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq,sizeof (struct ip_mreq)) == -1)
		{
			perror ("setsockopt");
			//exit(-6);
		}

		//����TTL
		unsigned char ttl = 255;
		if(setsockopt(sockfd,IPPROTO_IP,IP_MULTICAST_TTL,&ttl,sizeof(ttl)) < 0)
		{
			perror("setsockopt():IP_MULTICAST_TTL\n");
		}

		//���÷�����recv
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
