#ifndef DFITCAPISTRUCTXQN_H_
#define DFITCAPISTRUCTXQN_H_

#define APISTRUCT

///����ṹ��
#pragma pack(1)
struct APISTRUCT DFITCMarketDataFieldXQN
{
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
};
#pragma pack()

#endif//DFITCAPISTRUCTXQN_H_
