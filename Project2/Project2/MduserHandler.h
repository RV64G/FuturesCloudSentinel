#pragma once
#ifndef MDUESR_HANDLER_H



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
    sql::Driver* driver = get_driver_instance();
    sql::Connection* conn = driver->connect("tcp://127.0.0.1:3306", "root", "123456");
    conn->setSchema("cpptestmysql");
    return conn;
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

    mutex m_priceMutex;

    // 从数据库加载的预警缓存
    unordered_map<string, vector<AlertOrder>> m_alertMap;
    mutex m_alertMutex;

    // 线程控制
    atomic<bool> m_runAlertReload{ false };
    thread m_reloadThread;

    static CMduserHandler& handler;

public:

    // 最新行情缓存
    unordered_map<string, double> m_lastPrices;
    

    static CMduserHandler& GetHandler() {
		static CMduserHandler handler;
		return handler;
    }

    CMduserHandler()
    {
        m_notifier = make_shared<ConsoleNotifier>();
    }

    ~CMduserHandler()
    {
        StopAlertReloadThread();
        if (m_mdApi) {
            m_mdApi->Release();
            m_mdApi = nullptr;
        }
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

    }

    void login()
    {
        CThostFtdcReqUserLoginField t = { 0 };

        while (m_mdApi->ReqUserLogin(&t, 1) != 0)
            Sleep(1000);


        printf("Login successful\n");
        fflush(stdout);
    }

    void subscribe(const vector<string>& contracts)
    {
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
    void OnRtnDepthMarketData(CThostFtdcDepthMarketDataField* d) override
    {
        if (!d) return;

        printf("Received market data for %s: LastPrice=%.2f\n",
            d->InstrumentID, d->LastPrice);
        fflush(stdout);

        string symbol = d->InstrumentID;
        double price = d->LastPrice;

        // 更新行情缓存
        {
            lock_guard<mutex> lk(m_priceMutex);
            m_lastPrices[symbol] = price;
        }

        // 执行预警判断
        CheckAlert(symbol, price);
    }

    // 根据 symbol 和 price 判断预警
    void CheckAlert(const string& symbol, double price)
    {
        vector<AlertOrder> alerts;

        {
            lock_guard<mutex> lk(m_alertMutex);
            if (m_alertMap.count(symbol) == 0)
                return;
            alerts = m_alertMap[symbol];
        }

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

                // 检查解析是否成功
                if (result == 6) {  // 成功解析了6个字段
                    // 转换为标准 tm 格式
                    trigger_tm.tm_year -= 1900;  // tm_year 从1900年开始计数
                    trigger_tm.tm_mon -= 1;      // tm_mon 从0开始计数

                    // 计算预警时间的前一天
                    time_t trigger_time_t = mktime(&trigger_tm);
                    time_t one_day_before = trigger_time_t - 24 * 60 * 60;  // 减去一天的秒数

                    // 获取当前时间
                    time_t current_time_t = time(0);

                    // 判断是否在前一天范围内
                    if (current_time_t >= one_day_before && current_time_t < trigger_time_t) {
                        triggered = true;
                        reason = "到达预定时间前一天 " + a.trigger_time;
                    }
                }
            }

            if (triggered)
            {
                m_notifier->Notify(a.account, symbol, price, reason);
                MarkAlertTriggered(a.orderId);
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

#endif // !MDUESR_HANDLER_H