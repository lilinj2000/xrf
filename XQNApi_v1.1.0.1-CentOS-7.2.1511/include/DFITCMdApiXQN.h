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
         * ������ϢӦ��:�����鷵��ʱ���÷����ᱻ���á�
         * @param pMarketDataField:ָ��������Ϣ�ṹ��ָ�룬�ṹ���а��������������Ϣ��
         */
        virtual void OnMarketData(struct DFITCMarketDataFieldXQN * pMarketDataField) {}
    };//end of DFITCMdSpi

    class MDAPI_API DFITCMdApi
    {
    public:
        /**
         * ��������APIʵ��
         * @return ��������UserApi
         */
        static DFITCMdApi * CreateDFITCMdApi();

        /**
         * ����һϵ�г�ʼ������:ע��ص������ӿ�,��������ǰ�á�
         * @param multicast_addr:�鲥��ַ ����"226.100.100.100"
         * @param port:�˿ں� ����"11003"
         * @param local_addr:������ȡ���������ip ����"172.16.26.46"
         * @param cpu_core:��cpu���ĺţ�����ò�����ֵ3����Ὣ���̰߳���cpu��3��������
         * @param pSpi:��DFITCMdSpi����ʵ��
         * @return 0 - �ɹ�; ��0 - ʧ�ܡ�
         */
        virtual int Init(const char *multicast_addr, long port, const char *local_addr, int cpu_core, DFITCMDAPIXQN::DFITCMdSpi *pSpi) = 0;
        /**
         * @return 0 - �ɹ�; ��0 - ʧ�ܣ���ΪInit����ִ��δ�ɹ�
        */	
       	virtual int Run() = 0;
    protected:
        virtual ~DFITCMdApi() = 0;

    };//end of MDAPI_API
} //end of namespace

#endif//DFITCMDAPIXQN_H_
