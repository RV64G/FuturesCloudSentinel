#pragma once
#include "tradeapi/ThostFtdcMdApi.h"
#include "EmailNotifier.h"
#include <Windows.h>
#include <stdio.h>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <atomic>
#include <thread>
#include <functional>
#include <algorithm>


#include <mysql/jdbc.h>
using namespace std;



// ------------------------- Notifier -------------------------
class INotifier {
public:
    virtual void Notify(const std::string& account, const std::string& instrument, double price, const std::string& message) = 0;
    virtual ~INotifier() = default;
};

class ConsoleNotifier : public INotifier {
public:
    void Notify(const std::string& account, const std::string& instrument, double price, const std::string& message) override {
        printf("[ALERT] 用户=%s 合约=%s 价格=%.2f 触发原因=%s\n",
            account.c_str(), instrument.c_str(), price, message.c_str());
        fflush(stdout);
    }
};

class EmailNotifierWrapper : public INotifier {
private:
    std::shared_ptr<EmailNotifier> email_notifier;

public:
    EmailNotifierWrapper(std::shared_ptr<EmailNotifier> email) : email_notifier(email) {}

    void Notify(const std::string& account, const std::string& instrument, double price, const std::string& message) override {
        // 控制台输出
        printf("[ALERT] 用户=%s 合约=%s 价格=%.2f 触发原因=%s\n",
            account.c_str(), instrument.c_str(), price, message.c_str());
        fflush(stdout);

        // 发送邮件通知到用户邮箱
        email_notifier->SendAlertEmail(account, instrument, price, message);
    }
};

// ------------------------- DB 连接 -------------------------
static sql::Connection* GetConn()
{
    try {
        sql::Driver* driver = get_driver_instance();
        sql::Connection* conn = driver->connect("tcp://127.0.0.1:3306", "root", "123456");
        conn->setSchema("cpptestmysql");
        printf("[DB] 数据库连接成功\n");
        fflush(stdout);
        return conn;
    }
    catch (sql::SQLException& e) {
        printf("[DB ERROR] 连接失败: %s (错误码: %d, SQLState: %s)\n", 
               e.what(), e.getErrorCode(), e.getSQLState().c_str());
        fflush(stdout);
        throw;
    }
}

// ------------------------- 预警结构体 -------------------------
struct AlertOrder
{
    long orderId;
    string account;     // 添加 account 字段
    string symbol;
    double max_price;
    double min_price;
    string trigger_time;
    int state;
};

// =========================================================
// =============      CMduserHandler 主体       =============
// =========================================================

class CMduserHandler : public CThostFtdcMdSpi {
private:
    CThostFtdcMdApi* m_mdApi{ nullptr };

    vector<string> m_instruments;
    vector<char*>  m_instrumentCStrs;

    std::shared_ptr<INotifier> m_notifier;

    // 最新行情缓存
    unordered_map<string, double> m_lastPrices;
    mutex m_priceMutex;

    // 从数据库加载的预警缓存
    unordered_map<string, vector<AlertOrder>> m_alertMap;
    mutex m_alertMutex;

    // 线程控制
    atomic<bool> m_runAlertReload{ false };
    thread m_reloadThread;

    // 连接/登录 状态与请求 id
    atomic<bool> m_isConnected{ false };
    atomic<bool> m_isLoggedIn{ false };
    int m_reqId{ 0 };

    // 私有构造函数，用于单例模式
    CMduserHandler()
    {
        m_notifier = make_shared<ConsoleNotifier>();
    }

public:
    // 禁止拷贝和移动
    CMduserHandler(const CMduserHandler&) = delete;
    CMduserHandler& operator=(const CMduserHandler&) = delete;

    // 单例获取方法
    static CMduserHandler& GetHandler() {
        static CMduserHandler instance;
        return instance;
    }

    ~CMduserHandler()
    {
        StopAlertReloadThread();
        if (m_mdApi) {
            m_mdApi->Release();
            m_mdApi = nullptr;
        }
    }

    // 获取最新价格缓存（线程安全）
    unordered_map<string, double> getLastPrices() {
        lock_guard<mutex> lk(m_priceMutex);
        return m_lastPrices;
    }

    void SetNotifier(shared_ptr<INotifier> n)
    {
        m_notifier = n;
    }

    // =====================================================
    // =============== 1. 启动/停止 DB 预警加载线程 ============
    // =====================================================
    void StartAlertReloadThread()
    {
        m_runAlertReload = true;
        m_reloadThread = thread([this]() {
            while (m_runAlertReload.load())
            {
                ReloadAlertsFromDB();
                this_thread::sleep_for(chrono::seconds(3));
            }
            });
    }

    void StopAlertReloadThread()
    {
        m_runAlertReload = false;
        if (m_reloadThread.joinable())
            m_reloadThread.join();
    }

    // ===================== 从数据库读取预警单 =====================
    void ReloadAlertsFromDB()
    {
        try {
            unique_ptr<sql::Connection> conn(GetConn());
            unique_ptr<sql::PreparedStatement> stmt(
                conn->prepareStatement(
                    "SELECT orderId, account, symbol, max_price, min_price, trigger_time, state "
                    "FROM alert_order WHERE state=0"
                )
            );
            unique_ptr<sql::ResultSet> res(stmt->executeQuery());

            unordered_map<string, vector<AlertOrder>> tmp;

            while (res->next())
            {
                AlertOrder a;
                a.orderId = res->getInt("orderId");
                a.account = res->getString("account");
                a.symbol = res->getString("symbol");
                a.max_price = res->getDouble("max_price");
                a.min_price = res->getDouble("min_price");
                a.trigger_time = res->getString("trigger_time");  // 加载时间字段
                a.state = res->getInt("state");

                tmp[a.symbol].push_back(a);
            }

            lock_guard<mutex> lk(m_alertMutex);
            m_alertMap.swap(tmp);
        }
        catch (sql::SQLException& e) {
            printf("[DB ERROR] ReloadAlerts: %s\n", e.what());
            fflush(stdout);
        }
    }

    // ===================== 更新数据库状态（触发预警） =====================
    void MarkAlertTriggered(long orderId)
    {
        try {
            unique_ptr<sql::Connection> conn(GetConn());
            unique_ptr<sql::PreparedStatement> stmt(
                conn->prepareStatement("UPDATE alert_order SET state=1 WHERE orderId=?")
            );
            stmt->setInt(1, orderId);
            stmt->execute();
        }
        catch (...) {
            printf("[DB ERROR] 更新预警状态失败\n");
            fflush(stdout);
        }
    }

    // =====================================================
    // =============== 2. 行情 API 相关（你原来就有） ==========
    // =====================================================
    void connect()
    {
        if (m_mdApi) return;
        m_mdApi = CThostFtdcMdApi::CreateFtdcMdApi();
        m_mdApi->RegisterSpi(this);

        char addr[] = "tcp://182.254.243.31:30011";
        printf("Connecting to market data server: %s\n", addr);
        fflush(stdout);

        m_mdApi->RegisterFront(addr);
        m_mdApi->Init();
        printf("Market data API initialized\n");
        fflush(stdout);

        // 不在这里直接调用 ReqUserLogin，改在 OnFrontConnected 中处理。
    }

    // 仍保留 login 接口：如果外部调用，会等待登录成功（最长等待若干秒）
    void login(int timeoutSeconds = 10)
    {
        int waited = 0;
        while (!m_isLoggedIn.load() && waited < timeoutSeconds * 10)
        {
            Sleep(100);
            waited++;
        }

        if (m_isLoggedIn.load()) {
            printf("Login successful (confirmed)\n");
            fflush(stdout);
        }
        else {
            printf("Login not confirmed within timeout\n");
            fflush(stdout);
        }
    }

    void subscribe(const vector<string>& contracts)
    {
        // 等待登录确认（简单等待，避免在未登录前订阅）
        int waited = 0;
        while (!m_isLoggedIn.load() && waited < 50) // 5 秒
        {
            Sleep(100);
            waited++;
        }

        if (!m_isLoggedIn.load()) {
            printf("Warning: subscribe called before login confirmed\n");
            fflush(stdout);
        }

        m_instruments = contracts;
        m_instrumentCStrs.clear();

        // 添加输出，显示即将订阅的合约
        printf("Subscribing to %zu instruments:\n", contracts.size());
        for (const auto& contract : contracts) {
            printf("  - %s\n", contract.c_str());
        }
        fflush(stdout);

        for (auto& s : m_instruments)
            m_instrumentCStrs.push_back(const_cast<char*>(s.c_str()));

        int result = 0;
        while ((result = m_mdApi->SubscribeMarketData(m_instrumentCStrs.data(),
            (int)m_instrumentCStrs.size())) != 0)
        {
            printf("SubscribeMarketData failed with code: %d, retrying...\n", result);
            fflush(stdout);
            Sleep(1000);
        }

        printf("Successfully subscribed to market data\n");
        fflush(stdout);
    }

    void unsubscribe()
    {
        if (m_mdApi)
            m_mdApi->UnSubscribeMarketData(m_instrumentCStrs.data(),
                (int)m_instrumentCStrs.size());
    }

    // =====================================================
    // =============== 3. 行情回调处理 ========================
    // =====================================================

    // 确认与前置机建立连接后触发（在这里发送登录请求）
    void OnFrontConnected() override
    {
        m_isConnected = true;
        printf("OnFrontConnected: connected to front\n");
        fflush(stdout);

        // 发起登录请求（请按实际需求填充 BrokerID/UserID/Password）
        CThostFtdcReqUserLoginField req = { 0 };
        // 示例中保持空，如果你需要登录凭证请在此处赋值：
        // strcpy_s(req.BrokerID, "你的BrokerID");
        // strcpy_s(req.UserID, "你的UserID");
        // strcpy_s(req.Password, "你的Password");

        m_reqId++;
        int rt = m_mdApi->ReqUserLogin(&req, m_reqId);
        printf("ReqUserLogin returned: %d\n", rt);
        fflush(stdout);
    }

    void OnFrontDisconnected(int nReason) override
    {
        m_isConnected = false;
        m_isLoggedIn = false;
        printf("OnFrontDisconnected: reason=%d\n", nReason);
        fflush(stdout);
    }

    // 登录响应
    void OnRspUserLogin(CThostFtdcRspUserLoginField* pRspUserLogin,
        CThostFtdcRspInfoField* pRspInfo,
        int nRequestID, bool bIsLast) override
    {
        if (pRspInfo && pRspInfo->ErrorID != 0) {
            printf("OnRspUserLogin failed: %d %s\n", pRspInfo->ErrorID,
                pRspInfo->ErrorMsg ? pRspInfo->ErrorMsg : "");
            fflush(stdout);
            m_isLoggedIn = false;
            return;
        }

        printf("OnRspUserLogin success. TradingDay=%s, LoginTime=%s\n",
            pRspUserLogin && pRspUserLogin->TradingDay ? pRspUserLogin->TradingDay : "",
            pRspUserLogin && pRspUserLogin->LoginTime ? pRspUserLogin->LoginTime : "");
        fflush(stdout);
        m_isLoggedIn = true;
    }

    // 订阅/退订的响应（只是打印确认）
    void OnRspSubMarketData(CThostFtdcSpecificInstrumentField* pSpecificInstrument,
        CThostFtdcRspInfoField* pRspInfo,
        int nRequestID, bool bIsLast) override
    {
        if (pRspInfo && pRspInfo->ErrorID != 0) {
            printf("OnRspSubMarketData failed: %d %s\n", pRspInfo->ErrorID,
                pRspInfo->ErrorMsg ? pRspInfo->ErrorMsg : "");
        }
        else if (pSpecificInstrument) {
            printf("OnRspSubMarketData success for %s\n", pSpecificInstrument->InstrumentID);
        }
        else {
            printf("OnRspSubMarketData called (no instrument info)\n");
        }
        fflush(stdout);
    }

    // 行情下发回调
    void OnRtnDepthMarketData(CThostFtdcDepthMarketDataField* d) override
    {
        if (!d) return;

        printf("Received market data for %s: LastPrice=%.2f\n",
            d->InstrumentID, d->LastPrice);
        fflush(stdout);

        string symbol = d->InstrumentID;
        double price = d->LastPrice;
        // 改为带换行并立即 flush，避免缓冲导致看不到输出
        //printf("成功启动预警程序-缓存\n");
        //fflush(stdout);
        // 更新行情缓存
        {
            lock_guard<mutex> lk(m_priceMutex);
            m_lastPrices[symbol] = price;
        }

        // 执行预警判断
        CheckAlert(symbol, price);
    }

    // 根据 symbol 和 price 判断预警

    // 根据 symbol 和 price 判断预警
    void CheckAlert(const string& symbol, double price)
    {
        vector<AlertOrder> alerts;

        {
            lock_guard<mutex> lk(m_alertMutex);
            auto it = m_alertMap.find(symbol);
            if (it == m_alertMap.end())
                return;
            alerts = it->second; // 拷贝，避免长时间持锁
        }

        // 记录已触发的 orderId，循环结束后在内存中删除它们
        vector<long> triggeredIds;
        triggeredIds.reserve(4);

        // 获取当前时间 - 使用安全的 localtime_s
        time_t now = time(0);
        tm local_tm = { 0 };
        localtime_s(&local_tm, &now);  // 使用 localtime_s 替代 localtime
        char time_buffer[20];
        strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", &local_tm);
        string current_time_str = string(time_buffer);

        for (auto& a : alerts)
        {
            bool triggered = false;
            string reason;

            // 价格预警判断
            if (a.max_price > 0 && price >= a.max_price) {
                triggered = true;
                reason = ">= 上限 " + to_string(a.max_price);
            }
            if (a.min_price > 0 && price <= a.min_price) {
                triggered = true;
                reason = "<= 下限 " + to_string(a.min_price);
            }

            if (!a.trigger_time.empty()) {
                // 解析预警时间
                tm trigger_tm = { 0 };

                // 使用 sscanf_s 替代 sscanf
                int result = sscanf_s(a.trigger_time.c_str(), "%d-%d-%d %d:%d:%d",
                    &trigger_tm.tm_year, &trigger_tm.tm_mon, &trigger_tm.tm_mday,
                    &trigger_tm.tm_hour, &trigger_tm.tm_min, &trigger_tm.tm_sec);

                if (result == 6) {
                    trigger_tm.tm_year -= 1900;
                    trigger_tm.tm_mon -= 1;

                    time_t trigger_time_t = mktime(&trigger_tm);
                    time_t current_time_t = time(0);

                    if (current_time_t >= trigger_time_t) {
                        triggered = true;
                        reason = "到达预定时间 " + a.trigger_time;
                    }
                }
            }

            if (triggered)
            {
                // 先通知并在 DB 标记
                m_notifier->Notify(a.account, symbol, price, reason);
                MarkAlertTriggered(a.orderId);

                // 立即记录，需要在内存中移除，避免短时间重复触发
                triggeredIds.push_back(a.orderId);
            }
        }

        // 如果有触发项，移除内存缓存中的对应条目（线程安全）
        if (!triggeredIds.empty())
        {
            lock_guard<mutex> lk(m_alertMutex);
            auto it = m_alertMap.find(symbol);
            if (it != m_alertMap.end())
            {
                auto& vec = it->second;
                vec.erase(std::remove_if(vec.begin(), vec.end(),
                    [&](const AlertOrder& x) {
                        return std::find(triggeredIds.begin(), triggeredIds.end(), x.orderId) != triggeredIds.end();
                    }), vec.end());

                if (vec.empty())
                    m_alertMap.erase(it);
            }
        }
    }

    // 获取最新价（用于心跳打印）
    bool GetLastPrice(const string& ins, double& out)
    {
        lock_guard<mutex> lk(m_priceMutex);
        auto it = m_lastPrices.find(ins);
        if (it == m_lastPrices.end()) return false;
        out = it->second;
        return true;
    }
};

class MduserHandler
{
};
