#include "QuoteClient.h"
#include <QDebug>
#include <QDir>
#include <cstring>

QuoteClient::QuoteClient(QObject *parent) : QObject(parent), m_api(nullptr), m_requestId(0), m_isConnected(false)
{
}

QuoteClient::~QuoteClient()
{
    if (m_api) {
        m_api->RegisterSpi(nullptr);
        m_api->Release();
        m_api = nullptr;
    }
}

void QuoteClient::connectToCtp(const QString& frontAddr)
{
    if (m_api) return;

    // Ensure flow directory exists
    QDir dir("flow");
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    // Use absolute path for flow directory to avoid issues
    QString flowPath = dir.absolutePath() + "/";

    // Create API instance
    m_api = CThostFtdcMdApi::CreateFtdcMdApi(flowPath.toStdString().c_str());
    
    if (!m_api) {
        qCritical() << "Failed to create CTP API instance";
        return;
    }

    m_api->RegisterSpi(this);
    
    // Register front
    // Keep string alive during the call
    std::string addr = frontAddr.toStdString();
    m_api->RegisterFront(const_cast<char*>(addr.c_str()));
    
    m_api->Init();
}

void QuoteClient::subscribe(const QStringList& instruments)
{
    if (instruments.isEmpty()) return;

    if (!m_isConnected) {
        m_pendingSubscriptions.append(instruments);
        return;
    }

    int count = instruments.size();
    char** ppInstrumentID = new char*[count];
    for (int i = 0; i < count; ++i) {
        ppInstrumentID[i] = new char[31];
        // Use strncpy for safety, though CTP IDs are usually short
        std::string s = instruments[i].toStdString();
        strncpy(ppInstrumentID[i], s.c_str(), 30);
        ppInstrumentID[i][30] = '\0';
    }

    m_api->SubscribeMarketData(ppInstrumentID, count);

    for (int i = 0; i < count; ++i) {
        delete[] ppInstrumentID[i];
    }
    delete[] ppInstrumentID;
}

void QuoteClient::OnFrontConnected()
{
    qDebug() << "CTP Market Data Connected";
    // Login immediately
    CThostFtdcReqUserLoginField req = {0};
    m_api->ReqUserLogin(&req, ++m_requestId);
}

void QuoteClient::OnFrontDisconnected(int nReason)
{
    qDebug() << "CTP Market Data Disconnected, reason:" << nReason;
    m_isConnected = false;
}

void QuoteClient::OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    if (pRspInfo && pRspInfo->ErrorID != 0) {
        qDebug() << "CTP Login Failed:" << pRspInfo->ErrorMsg;
        return;
    }
    qDebug() << "CTP Login Success";
    m_isConnected = true;

    if (!m_pendingSubscriptions.isEmpty()) {
        subscribe(m_pendingSubscriptions);
        m_pendingSubscriptions.clear();
    }
}

void QuoteClient::OnRspSubMarketData(CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    if (pRspInfo && pRspInfo->ErrorID != 0) {
        qDebug() << "Subscribe Failed:" << pRspInfo->ErrorMsg;
    } else {
        qDebug() << "Subscribed:" << (pSpecificInstrument ? pSpecificInstrument->InstrumentID : "null");
    }
}

void QuoteClient::OnRtnDepthMarketData(CThostFtdcDepthMarketDataField *pDepthMarketData)
{
    if (!pDepthMarketData) return;
    
    QString symbol = pDepthMarketData->InstrumentID;
    double price = pDepthMarketData->LastPrice;
    
    // Debug output to verify data reception
    qDebug() << "Quote Received:" << symbol << price;

    // Emit signal to update UI
    emit priceUpdated(symbol, price);
}
