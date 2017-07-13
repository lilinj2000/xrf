#ifndef DFITCMDAPIXQN_H_
#define DFITCMDAPIXQN_H_

#include "DFITCApiStructXQN.h"


#ifdef WIN32
    #ifdef DFITCAPI_EXPORTS
        #define MDAPI_API __declspec(dllexport)
    #else
        #define MDAPI_API __declspec(dllimport)
    #endif//DFITCAPI_EXPORTS
#else
    #define MDAPI_API 
#endif//WIN32

namespace DFITCMDAPIXQN
{
    class DFITCMdSpi
    {
    public:
        DFITCMdSpi(){}
        /**
         * 行情消息应答:有行情返回时，该方法会被调用。
         * @param pMarketDataField:指向行情信息结构的指针，结构体中包含具体的行情信息。
         */
        virtual void OnMarketData(struct DFITCMarketDataFieldXQN * pMarketDataField) {}
    };//end of DFITCMdSpi

    class MDAPI_API DFITCMdApi
    {
    public:
        /**
         * 创建行情API实例
         * @return 创建出的UserApi
         */
        static DFITCMdApi * CreateDFITCMdApi();

        /**
         * 进行一系列初始化工作:注册回调函数接口,连接行情前置。
         * @param multicast_addr:组播地址 例如"226.100.100.100"
         * @param port:端口号 例如"11003"
         * @param local_addr:本地收取行情的网卡ip 例如"172.16.26.46"
         * @param cpu_core:绑定cpu核心号，例如该参数赋值3，则会将该线程绑定在cpu第3个核心中
         * @param pSpi:类DFITCMdSpi对象实例
         * @return 0 - 成功; 非0 - 失败。
         */
        virtual int Init(const char *multicast_addr, long port, const char *local_addr, int cpu_core, DFITCMDAPIXQN::DFITCMdSpi *pSpi) = 0;
        /**
         * @return 0 - 成功; 非0 - 失败，因为Init函数执行未成功
        */	
       	virtual int Run() = 0;
    protected:
        virtual ~DFITCMdApi() = 0;

    };//end of MDAPI_API
} //end of namespace

#endif//DFITCMDAPIXQN_H_
