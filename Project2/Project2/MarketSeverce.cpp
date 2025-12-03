#include "MduserHandler.h"
#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <atomic>
#include <thread>
#include <chrono>
#include <windows.h>

std::atomic<bool> g_running{ true };

// 控制台信号处理函数
BOOL WINAPI ConsoleHandler(DWORD signal)
{
    if (signal == CTRL_C_EVENT || signal == CTRL_BREAK_EVENT || signal == CTRL_CLOSE_EVENT)
    {
        g_running.store(false);
        return TRUE;
    }
    return FALSE;
}

// 自定义通知器实现
class ServiceNotifier : public INotifier {
public:
    void Notify(const std::string& account, const std::string& instrument, double price, const std::string& message) override {
        printf("[SERVICE ALERT] 用户=%s 合约=%s 价格=%.2f 触发原因=%s\n",
            account.c_str(), instrument.c_str(), price, message.c_str());
        fflush(stdout);
    }
};


// 从数据库获取需要订阅的合约列表
std::vector<std::string> LoadContractsFromDB()
{
    std::vector<std::string> contracts;

    try {
        sql::Connection* conn = GetConn();
        std::unique_ptr<sql::Connection> connPtr(conn);
        std::unique_ptr<sql::PreparedStatement> stmt(
            conn->prepareStatement("SELECT DISTINCT symbol FROM alert_order WHERE state=0")
        );
        std::unique_ptr<sql::ResultSet> res(stmt->executeQuery());

        while (res->next()) {
            contracts.push_back(res->getString("symbol"));
        }

        printf("从数据库加载了 %zu 个合约\n", contracts.size());
        for (const auto& contract : contracts) {
            printf("  - %s\n", contract.c_str());
        }
        fflush(stdout);
    }
    catch (sql::SQLException& e) {
        printf("[DB ERROR] 加载合约列表失败: %s\n", e.what());
        fflush(stdout);
    }

    return contracts;
}

// 将 Test.cpp 中的 main 函数内容提取为独立函数
int StartMarketService() {
    // 重置运行标志
    g_running.store(true);

    // 原 main 函数的核心逻辑
    try {
        // 创建行情处理器实例
        CMduserHandler& handler = CMduserHandler::GetHandler();
        // 创建邮件通知器
        std::shared_ptr<EmailNotifier> emailNotifier = std::make_shared<EmailNotifier>(
            "smtp.163.com", 25,
            "grmtest132@163.com",    // 发件人邮箱
            "MTQ5UJyvsJ85eiG2",      // 授权码
            ""     // 收件人邮箱
        );

        // 设置邮件通知器
        std::shared_ptr<EmailNotifierWrapper> emailWrapper =
            std::make_shared<EmailNotifierWrapper>(emailNotifier);
        handler.SetNotifier(emailWrapper);

        // 从数据库加载需要订阅的合约列表
        std::vector<std::string> contracts = LoadContractsFromDB();

        // 如果数据库中没有合约，则使用默认合约列表
        if (contracts.empty()) {
            printf("数据库中未找到合约，使用默认合约列表\n");
            contracts = {
                "IF2512", "IH2512", "IC2512", "IM2512",
                "TS2603", "TF2603", "T2603"
            };
        }

        // 连接并登录行情服务器
        handler.connect();
        handler.login();

        // 订阅合约
        handler.subscribe(contracts);

        // 启动预警数据重载线程
        handler.StartAlertReloadThread();

        // 在独立线程中运行监控逻辑
        std::thread monitorThread([&handler]() {
            while (g_running.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }

            // 退出清理
            handler.unsubscribe();
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            });

        // 分离线程，使其独立运行
        monitorThread.detach();

        return 0; // 成功启动
    }
    catch (...) {
        return -1; // 启动失败
    }
}

void StopMarketService() {
    g_running.store(false);
}