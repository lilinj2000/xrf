#include "DFITCMdApiXQN.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdarg.h>
#include <sys/time.h>
#include <time.h>

#include <pthread.h>
#include "LockFreeQueue2.h"
using namespace LOCK_FREE;

//�ж��ļ��Ƿ����
bool fileExists(const char *fileName);
//������־�ļ�
int backUpFile(const char *fileName);
//������ļ�
void outToFile(FILE *fp, const char *format, ...);

//������־
void CreateLog();
//�ر���־
void DestroyLog();

FILE * g_fp = NULL;//ȫ����־�ļ�ָ��
FILE * g_fpRecv = NULL;//����������־�ļ�ָ��

//�����˳���־
bool quit = false;
//����ָ������������־
#define RECEIVE_LOG "socketrecv.log"
//��ӡ��־�̰߳󶨵ĺ���
int print_log_core = -1;

struct timespec time1 = {0, 0};

#ifndef WIN32
//�źŴ�����  
void signal_handler(int signo)  
{  
	//����ź�ֵ  
	printf("recevive a signal: %d\n", signo);  
	if(SIGINT == signo||SIGQUIT == signo||SIGKILL == signo)
	{
		quit = true;
		DestroyLog();
		exit(0);
	}
	
}
#endif


//�ص������д�����еĽṹ��
typedef struct logStruct
{
    struct timeval localTVTime;		//����ʱ���
    int sec;					//��ʱ���
    int nsec;					//����ʱ���
    char Market[3];					//�г�˵��
    char Status;					//�ֶ�״̬
    char Instrument[7];				//��Լ����
    char UpdateTime[9];				//����ʱ��
    int UpdateMillisec;				//������ʱ�����
    double LastPrice;				//���¼�
    int	Volume;					//�ɽ���
    double Turnover;				//�ɽ����
    double OpenInterest;				//�ֲ���
    double BidPrice;				//��һ��
    int BidVolume;					//��һ��
    double AskPrice;				//��һ��
    int AskVolume;					//��һ��
}logStruct;

typedef ArrayLockFreeQueue<logStruct,	MULTI_THREAD_FALSE, MULTI_THREAD_FALSE>	ArrayDataQueue;
ArrayDataQueue queue;			// the queue for recved data from socket
pthread_t pid;

//�����double�����п��ܳ���doubleֵΪFF FF FF FF FF FF EF 7F
//������Ϊ0
int check_large_double(double n)
{
	return (n > 999999999999.0)?1:0; 
}

//�̰߳�cpu
void pthread_bind_cpu(int cpu_index)
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
		printf("thread %lu bind cpu %d\n",pthread_self(), cpu_index);
	}
}

class CMdSpi : public DFITCMDAPIXQN::DFITCMdSpi
{
public:
	~CMdSpi()
	{
	
	}
	CMdSpi(DFITCMDAPIXQN::DFITCMdApi* pMdApi) : m_pMdApi(pMdApi)
	{

	}
	//����ص�
	void OnMarketData(struct DFITCMarketDataFieldXQN * pMarketDataField) 
	{
		logStruct ls = {0};
		clock_gettime(CLOCK_REALTIME, &time1);
		ls.sec = time1.tv_sec;
		ls.nsec = time1.tv_nsec;

		strcpy(ls.Instrument,pMarketDataField->Instrument);//��Լ����
		strcpy(ls.UpdateTime,pMarketDataField->UpdateTime);//������ʱ��
		strcpy(ls.Market,pMarketDataField->Market);//�г�˵��
		ls.Status = pMarketDataField->Status;
		ls.UpdateMillisec = pMarketDataField->UpdateMillisec;
		if (check_large_double(pMarketDataField->LastPrice))
			ls.LastPrice = 0.0;
		else
			ls.LastPrice = pMarketDataField->LastPrice;
		ls.Volume = pMarketDataField->Volume;
		if (check_large_double(pMarketDataField->Turnover))
			ls.Turnover = 0.0;
		else
			ls.Turnover = pMarketDataField->Turnover;
		if (check_large_double(pMarketDataField->OpenInterest))
			ls.OpenInterest = 0.0;
		else
			ls.OpenInterest = pMarketDataField->OpenInterest;
		if (check_large_double(pMarketDataField->BidPrice))
			ls.BidPrice = 0.0;
		else
			ls.BidPrice = pMarketDataField->BidPrice;
		ls.BidVolume = pMarketDataField->BidVolume;
		if (check_large_double(pMarketDataField->AskPrice))
			ls.AskPrice = 0.0;
		else
			ls.AskPrice = pMarketDataField->AskPrice;
		ls.AskVolume = pMarketDataField->AskVolume;
		queue.push(ls);
	}

protected:
	DFITCMDAPIXQN::DFITCMdApi *m_pMdApi;
};

void *printQueue(void *)
{
	logStruct ls = {0};
	if (print_log_core >= 0)
		pthread_bind_cpu(print_log_core);
	char buf[100] = {0};
	struct tm p;
	while (1)
	{
		if (queue.pop(ls))
		{
			sprintf(buf, "[�г�˵��]:%s, [��Լ����]:%s, [����ʱ��]:%s, [����]:%03d, [���¼�]:%lf, [�ɽ���]:%d, [�ɽ����]:%lf, [�ֲ���]:%lf, [��һ��]:%lf, [��һ��]:%d, [��һ��]:%lf, [��һ��]:%d, [����д��ʱ��]%d-%d\n",
					ls.Market,//�г�˵��
					ls.Instrument,//��Լ����
					ls.UpdateTime,//����ʱ��
					ls.UpdateMillisec,//����
					ls.LastPrice,//���¼�
					ls.Volume,//�ɽ���
					ls.Turnover,//�ɽ����
					ls.OpenInterest,//�ֲ���
					ls.BidPrice,//��һ��
					ls.BidVolume,//��һ��
					ls.AskPrice,//��һ��
					ls.AskVolume,//��һ��
					ls.sec,//����ʱ�����
					ls.nsec//����ʱ�������
					);
			fputs(buf,g_fpRecv);
			fflush(g_fpRecv);
			memset(buf, 0, 100);
		}
		else
			//usleep(0);
			sched_yield();
	}
}

void printHelp()
{
	printf("Usage: ./demo -ip 226.100.100.100 -port 11003\n");
}
int main(int argc, char *argv[])
{

	char ip[20] = "226.100.100.100";
	char local_ip[20] = "172.16.26.46";
	int port = 10008;
	int cpu_core = 6;
	print_log_core = 7;
	
	if(argc > 1)
	{
		for(int i=0;i<argc;++i)
		{
			if(!strcmp(argv[i],"-ip"))
			{
					strcpy(ip,argv[++i]);
			}
			else if(!strcmp(argv[i],"-port"))
			{
					port = atoi(argv[++i]);
			}
			else if(!strcmp(argv[i],"-local"))
			{
				strcpy(local_ip,argv[++i]);
			}
			else if(!strcmp(argv[i],"-cpu_core"))
			{
					cpu_core = atoi(argv[++i]);
			}
			else if(!strcmp(argv[i],"-log_core"))
			{
					print_log_core = atoi(argv[++i]);
			}
			else if(!strcmp(argv[i],"-h"))
			{
				printHelp();
			}
		}
	}
	printf("ip=%s,port=%d,cpu_core=%d, local_ip:%s\n",ip,port,cpu_core, local_ip);

	//�����ǰ���̺�  
	printf("process id is %d\n", getpid());  
	//ΪSIGINT�ź������źŴ�����  
	signal(SIGINT, signal_handler);  
	//ΪSIGHUP�ź������źŴ�����  
	signal(SIGHUP, signal_handler);  
	//ΪSIGQUIT�ź������źŴ�����  
	signal(SIGQUIT, signal_handler);

	CreateLog();//������־

	//������ӡ�߳�
	int err = pthread_create(&pid, NULL, printQueue, NULL);
	if (0 != err)
		printf("can't create thread: %s\n", strerror(err));

	DFITCMDAPIXQN::DFITCMdApi* pMdApi = DFITCMDAPIXQN::DFITCMdApi::CreateDFITCMdApi();
	CMdSpi MySpi(pMdApi);
	pMdApi->Init(ip, port, local_ip, cpu_core, &MySpi);
	printf("demo Init over\n");
	pMdApi->Run();
	DestroyLog();//�ر���־
	return 0;
}

//������־
void CreateLog()
{

	backUpFile(RECEIVE_LOG);
	g_fpRecv = fopen(RECEIVE_LOG,"w");
	if(g_fpRecv == NULL)
	{
		perror("������־�ļ�ʧ��");
	}
}
//�ر���־
void DestroyLog()
{
	if(g_fpRecv)//���������־
	{
		fclose(g_fpRecv);
		g_fpRecv = NULL;
	}
}
//�ж��ļ��Ƿ����
bool fileExists(const char *fileName)
{
	//return (access(fileName,0) == 0);
	FILE *fp = fopen(fileName,"r");
	if(fp)
	{
		fclose(fp);
		return true;
	}
	return false;
}
//����־�Ѿ������򱸷���־�ļ�
int backUpFile(const char *fileName)
{
	if(fileExists(fileName))
	{
		char newfileName[260] = {0};
		strncpy(newfileName,fileName,sizeof(newfileName));
		struct timeval tv;
		struct tm      p;
		gettimeofday(&tv,NULL);
		localtime_r(&tv.tv_sec,&p);
		int i = 1;
		do{
			sprintf(newfileName,"%s_%04d%02d%02d_test%d.log",fileName,p.tm_year+1900,p.tm_mon+1,p.tm_mday,i++);
		}while(fileExists(newfileName));
		return rename(fileName,newfileName);
	}
	return 0;
}
//������ļ�
void outToFile(FILE *fp,const char *format,...)
{
	if (NULL==format||0==format[0]) return;
	
	char buf[4096] = {0};
	va_list args;
	va_start(args, format);
	vsnprintf(buf, sizeof(buf), format, args);
	va_end(args);
	
	char strTime[30] = {0};
	struct timeval tv;
	struct tm      p;
	gettimeofday(&tv,NULL);
	localtime_r(&tv.tv_sec,&p);  
	sprintf(strTime,"%04d-%02d-%02d %02d:%02d:%02d.%06ld ",1900+p.tm_year, 1+p.tm_mon, p.tm_mday, p.tm_hour, p.tm_min, p.tm_sec, tv.tv_usec);
	
	if(fp){
		fputs(strTime, fp);
		fputs(buf, fp);
		fflush(fp);
	}
	//	printf("[%s]%s\n", strTime, buf);
}
