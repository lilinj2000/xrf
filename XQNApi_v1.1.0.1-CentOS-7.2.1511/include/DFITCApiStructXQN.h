#ifndef DFITCAPISTRUCTXQN_H_
#define DFITCAPISTRUCTXQN_H_

#define APISTRUCT

///行情结构体
#pragma pack(1)
struct APISTRUCT DFITCMarketDataFieldXQN
{
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
};
#pragma pack()

#endif//DFITCAPISTRUCTXQN_H_
