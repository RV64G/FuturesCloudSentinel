#pragma once
#include <QObject>
#include <QMap>
#include <QMutex>
#include <QStringList>
#include "ThostFtdcMdApi.h"

class QuoteClient : public QObject, public CThostFtdcMdSpi
{
    Q_OBJECT
public:
    explicit QuoteClient(QObject *parent = nullptr);
    ~QuoteClient();

    void connectToCtp(const QString& frontAddr);
    void subscribe(const QStringList& instruments);

signals:
    void priceUpdated(const QString& symbol, double price);

private:
    // CThostFtdcMdSpi interface
    void OnFrontConnected() override;
    void OnFrontDisconnected(int nReason) override;
    void OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) override;
    void OnRspSubMarketData(CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) override;
    void OnRtnDepthMarketData(CThostFtdcDepthMarketDataField *pDepthMarketData) override;

    CThostFtdcMdApi* m_api;
    int m_requestId;
    bool m_isConnected;
    QStringList m_pendingSubscriptions;
};
