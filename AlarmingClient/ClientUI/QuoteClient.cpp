#include "QuoteClient.h"
#include <QDebug>
#include <QDir>
#include <QCoreApplication>
#include <cstring>

QuoteClient::QuoteClient(QObject *parent) : QObject(parent), m_api(nullptr), m_requestId(0), m_isConnected(false)
{
    qDebug() << "[QuoteClient] Created";
}

QuoteClient::~QuoteClient()
{
    qDebug() << "[QuoteClient] Destroying...";
    if (m_api) {
        m_api->RegisterSpi(nullptr);
        m_api->Release();
        m_api = nullptr;
    }
}

void QuoteClient::connectToCtp(const QString& frontAddr)
{
    if (m_api) {
        qDebug() << "[QuoteClient] Already initialized, skipping";
        return;
    }

    qDebug() << "[QuoteClient] Connecting to CTP:" << frontAddr;

    // 使用应用程序目录下的 flow 文件夹
    QString appDir = QCoreApplication::applicationDirPath();
    QString flowPath = appDir + "/flow/";
    QDir dir(flowPath);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            qCritical() << "[QuoteClient] Failed to create flow directory:" << flowPath;
            return;
        }
    }
    
    qDebug() << "[QuoteClient] Flow path:" << flowPath;

    // Create API instance
    m_api = CThostFtdcMdApi::CreateFtdcMdApi(flowPath.toStdString().c_str());
    
    if (!m_api) {
        qCritical() << "[QuoteClient] Failed to create CTP API instance!";
        return;
    }

    qDebug() << "[QuoteClient] CTP API created, registering SPI...";
    m_api->RegisterSpi(this);
    
    // Register front - 保存地址字符串
    m_frontAddr = frontAddr.toStdString();
    m_api->RegisterFront(const_cast<char*>(m_frontAddr.c_str()));
    
    qDebug() << "[QuoteClient] Calling Init()...";
    m_api->Init();
    qDebug() << "[QuoteClient] Init() called, waiting for connection callback...";
}

void QuoteClient::subscribe(const QStringList& instruments)
{
    if (instruments.isEmpty()) {
        qDebug() << "[QuoteClient] Subscribe called with empty list";
        return;
    }

    qDebug() << "[QuoteClient] Subscribe request:" << instruments << "connected:" << m_isConnected;

    if (!m_isConnected) {
        qDebug() << "[QuoteClient] Not connected yet, queuing subscriptions";
        m_pendingSubscriptions.append(instruments);
        return;
    }

    int count = instruments.size();
    char** ppInstrumentID = new char*[count];
    for (int i = 0; i < count; ++i) {
        ppInstrumentID[i] = new char[31];
        std::string s = instruments[i].toStdString();
        strncpy(ppInstrumentID[i], s.c_str(), 30);
        ppInstrumentID[i][30] = '\0';
        qDebug() << "[QuoteClient] Subscribing to:" << ppInstrumentID[i];
    }

    int ret = m_api->SubscribeMarketData(ppInstrumentID, count);
    qDebug() << "[QuoteClient] SubscribeMarketData returned:" << ret;

    for (int i = 0; i < count; ++i) {
        delete[] ppInstrumentID[i];
    }
    delete[] ppInstrumentID;
}

void QuoteClient::OnFrontConnected()
{
    qDebug() << "[QuoteClient] *** OnFrontConnected callback ***";
    emit connectionStatusChanged(true);
    // Login immediately
    CThostFtdcReqUserLoginField req = {0};
    int ret = m_api->ReqUserLogin(&req, ++m_requestId);
    qDebug() << "[QuoteClient] ReqUserLogin returned:" << ret;
}

void QuoteClient::OnFrontDisconnected(int nReason)
{
    qDebug() << "[QuoteClient] *** OnFrontDisconnected, reason:" << nReason << "***";
    // 常见断开原因:
    // 0x1001 - 网络读失败
    // 0x1002 - 网络写失败  
    // 0x2001 - 心跳超时
    // 0x2002 - 发送心跳失败
    // 0x2003 - 收到错误报文
    m_isConnected = false;
    emit connectionStatusChanged(false);
}

void QuoteClient::OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    if (pRspInfo && pRspInfo->ErrorID != 0) {
        qDebug() << "[QuoteClient] Login FAILED! ErrorID:" << pRspInfo->ErrorID << "Msg:" << pRspInfo->ErrorMsg;
        return;
    }
    qDebug() << "[QuoteClient] *** Login SUCCESS! Trading Day:" << (pRspUserLogin ? pRspUserLogin->TradingDay : "null") << "***";
    m_isConnected = true;
    emit connectionStatusChanged(true);

    if (!m_pendingSubscriptions.isEmpty()) {
        qDebug() << "[QuoteClient] Processing pending subscriptions:" << m_pendingSubscriptions.size();
        subscribe(m_pendingSubscriptions);
        m_pendingSubscriptions.clear();
    }
}

void QuoteClient::OnRspSubMarketData(CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    if (pRspInfo && pRspInfo->ErrorID != 0) {
        qDebug() << "[QuoteClient] Subscribe FAILED:" << pRspInfo->ErrorMsg;
    } else {
        qDebug() << "[QuoteClient] Subscribe OK:" << (pSpecificInstrument ? pSpecificInstrument->InstrumentID : "null");
    }
}

void QuoteClient::OnRtnDepthMarketData(CThostFtdcDepthMarketDataField *pDepthMarketData)
{
    if (!pDepthMarketData) return;
    
    QString symbol = pDepthMarketData->InstrumentID;
    double price = pDepthMarketData->LastPrice;
    
    // 减少日志频率 - 只在价格变化时输出
    static QMap<QString, double> lastPrices;
    if (lastPrices.value(symbol, -1) != price) {
        qDebug() << "[QuoteClient] Quote:" << symbol << "Price:" << price 
                 << "Vol:" << pDepthMarketData->Volume << "Time:" << pDepthMarketData->UpdateTime;
        lastPrices[symbol] = price;
    }
    
    // 收到行情数据也作为心跳信号
    emit connectionStatusChanged(true);

    // Emit signal to update UI
    emit priceUpdated(symbol, price);
}
