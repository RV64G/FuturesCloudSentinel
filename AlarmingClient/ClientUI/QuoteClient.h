#pragma once
#include <QObject>
#include <QMap>
#include <QMutex>
#include <QSet>
#include <QStringList>
#include "CTP/include/ThostFtdcMdApi.h"

class QuoteClient : public QObject, public CThostFtdcMdSpi
{
    Q_OBJECT
public:
    explicit QuoteClient(QObject *parent = nullptr);
    ~QuoteClient();

    void connectToCtp(const QString& frontAddr);
    void subscribe(const QStringList& instruments);
    void unsubscribe(const QStringList& instruments);
    void updateSubscriptions(const QStringList& newInstruments);  // 智能更新订阅列表
    bool isConnected() const { return m_isConnected; }
    QStringList subscribedInstruments() const { return m_subscribedInstruments.values(); }

signals:
    void priceUpdated(const QString& symbol, double price);
    void connectionStatusChanged(bool connected);

private:
    // CThostFtdcMdSpi interface
    void OnFrontConnected() override;
    void OnFrontDisconnected(int nReason) override;
    void OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) override;
    void OnRspSubMarketData(CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) override;
    void OnRspUnSubMarketData(CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) override;
    void OnRtnDepthMarketData(CThostFtdcDepthMarketDataField *pDepthMarketData) override;

    CThostFtdcMdApi* m_api;
    int m_requestId;
    bool m_isConnected;
    QStringList m_pendingSubscriptions;
    std::string m_frontAddr;  // 保存前置地址，防止被释放
    QSet<QString> m_subscribedInstruments;  // 已订阅的合约
};
