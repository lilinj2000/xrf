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

//判断文件是否存在
bool fileExists(const char *fileName);
//备份日志文件
int backUpFile(const char *fileName);
//输出到文件
void outToFile(FILE *fp, const char *format, ...);

//创建日志
void CreateLog();
//关闭日志
void DestroyLog();

FILE * g_fp = NULL;//全局日志文件指针
FILE * g_fpRecv = NULL;//接收行情日志文件指针

//程序退出标志
bool quit = false;
//接收指定个数行情日志
#define RECEIVE_LOG "socketrecv.log"
//打印日志线程绑定的核心
int print_log_core = -1;

struct timespec time1 = {0, 0};

#ifndef WIN32
//信号处理函数  
void signal_handler(int signo)  
{  
	//输出信号值  
	printf("recevive a signal: %d\n", signo);  
	if(SIGINT == signo||SIGQUIT == signo||SIGKILL == signo)
	{
		quit = true;
		DestroyLog();
		exit(0);
	}
	
}
#endif


//回调函数中存入队列的结构体
typedef struct logStruct
{
    struct timeval localTVTime;		//本地时间戳
    int sec;					//秒时间戳
    int nsec;					//纳秒时间戳
    char Market[3];					//市场说明
    char Status;					//字段状态
    char Instrument[7];				//合约代码
    char UpdateTime[9];				//更新时间
    int UpdateMillisec;				//最后更新时间毫秒
    double LastPrice;				//最新价
    int	Volume;					//成交量
    double Turnover;				//成交金额
    double OpenInterest;				//持仓量
    double BidPrice;				//买一价
    int BidVolume;					//买一量
    double AskPrice;				//卖一价
    int AskVolume;					//卖一量
}logStruct;

typedef ArrayLockFreeQueue<logStruct,	MULTI_THREAD_FALSE, MULTI_THREAD_FALSE>	ArrayDataQueue;
ArrayDataQueue queue;			// the queue for recved data from socket
pthread_t pid;

//行情的double类型有可能出现double值为FF FF FF FF FF FF EF 7F
//将其视为0
int check_large_double(double n)
{
	return (n > 999999999999.0)?1:0; 
}

//线程绑定cpu
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
	//行情回调
	void OnMarketData(struct DFITCMarketDataFieldXQN * pMarketDataField) 
	{
		logStruct ls = {0};
		clock_gettime(CLOCK_REALTIME, &time1);
		ls.sec = time1.tv_sec;
		ls.nsec = time1.tv_nsec;

		strcpy(ls.Instrument,pMarketDataField->Instrument);//合约代码
		strcpy(ls.UpdateTime,pMarketDataField->UpdateTime);//交易所时间
		strcpy(ls.Market,pMarketDataField->Market);//市场说明
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
			sprintf(buf, "[市场说明]:%s, [合约代码]:%s, [更新时间]:%s, [毫秒]:%03d, [最新价]:%lf, [成交量]:%d, [成交金额]:%lf, [持仓量]:%lf, [买一价]:%lf, [买一量]:%d, [卖一价]:%lf, [卖一量]:%d, [行情写入时间]%d-%d\n",
					ls.Market,//市场说明
					ls.Instrument,//合约代码
					ls.UpdateTime,//更新时间
					ls.UpdateMillisec,//毫秒
					ls.LastPrice,//最新价
					ls.Volume,//成交量
					ls.Turnover,//成交金额
					ls.OpenInterest,//持仓量
					ls.BidPrice,//买一价
					ls.BidVolume,//买一量
					ls.AskPrice,//卖一价
					ls.AskVolume,//卖一量
					ls.sec,//本地时间戳秒
					ls.nsec//本地时间戳纳秒
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

	//输出当前进程号  
	printf("process id is %d\n", getpid());  
	//为SIGINT信号设置信号处理函数  
	signal(SIGINT, signal_handler);  
	//为SIGHUP信号设置信号处理函数  
	signal(SIGHUP, signal_handler);  
	//为SIGQUIT信号设置信号处理函数  
	signal(SIGQUIT, signal_handler);

	CreateLog();//创建日志

	//创建打印线程
	int err = pthread_create(&pid, NULL, printQueue, NULL);
	if (0 != err)
		printf("can't create thread: %s\n", strerror(err));

	DFITCMDAPIXQN::DFITCMdApi* pMdApi = DFITCMDAPIXQN::DFITCMdApi::CreateDFITCMdApi();
	CMdSpi MySpi(pMdApi);
	pMdApi->Init(ip, port, local_ip, cpu_core, &MySpi);
	printf("demo Init over\n");
	pMdApi->Run();
	DestroyLog();//关闭日志
	return 0;
}

//创建日志
void CreateLog()
{

	backUpFile(RECEIVE_LOG);
	g_fpRecv = fopen(RECEIVE_LOG,"w");
	if(g_fpRecv == NULL)
	{
		perror("创建日志文件失败");
	}
}
//关闭日志
void DestroyLog()
{
	if(g_fpRecv)//行情接收日志
	{
		fclose(g_fpRecv);
		g_fpRecv = NULL;
	}
}
//判断文件是否存在
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
//若日志已经存在则备份日志文件
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
//输出到文件
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
