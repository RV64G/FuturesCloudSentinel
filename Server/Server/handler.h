#pragma once
//用于路由请求到不同函数
#ifndef HANDLER_H
#define HANDLER_H

#include <iostream>
#include <unordered_map>
#include <string>
#include <functional>
#include "nlohmann/json.hpp"  // 使用nlohmann/json库处理JSON
#include <mysql/jdbc.h>
#include "thread_local.h"
#include "MduserHandler.h"
#define WIN32_LEAN_AND_MEAN
using json = nlohmann::json;

#include <thread>
#include <memory>
#include <atomic>
#include <mutex>
#include <chrono>

// 前置声明处理函数
class FuturesAlertServer;
using RequestHandler = std::function<json(FuturesAlertServer&, const json&)>;

class FuturesAlertServer {
private:
    // 路由映射表：请求类型 -> 处理函数
    std::unordered_map<std::string, RequestHandler> requestHandlers;

    // 用于每个已登录用户的守护线程控制（token -> stop flag）
    inline static std::unordered_map<std::string, std::shared_ptr<std::atomic<bool>>> userWatchers;
    inline static std::mutex userWatchersMutex;

    // 通用错误响应生成函数 (符合 protocol.md V3.0)
    json createErrorResponse(const std::string& requestId,
        const std::string& requestType,
        int errorCode,
        const std::string& hint = "") {
        json resp = {
            {"type", "response"},
            {"request_id", requestId},
            {"request_type", requestType},
            {"status", 1},
            {"error_code", errorCode}
        };
        if (!hint.empty()) {
            resp["data"] = { {"hint", hint} };
        }
        return resp;
    }

    // 旧版兼容：接收 message 参数自动转换为 error_code
    json createErrorResponse(const std::string& requestId,
        const std::string& requestType,
        const std::string& errorMsg) {
        // 根据消息推断错误码
        int code = 1001; // 默认未知错误
        if (errorMsg.find("用户名可能已存在") != std::string::npos) code = 2003;
        else if (errorMsg.find("用户名或密码错误") != std::string::npos) code = 2002;
        else if (errorMsg.find("数据库") != std::string::npos) code = 1006;
        else if (errorMsg.find("缺少") != std::string::npos) code = 1003;
        else if (errorMsg.find("不支持") != std::string::npos) code = 1004;
        return createErrorResponse(requestId, requestType, code, errorMsg);
    }

    // 通用成功响应生成函数 (符合 protocol.md V3.0)
    json createSuccessResponse(const std::string& requestId,
        const std::string& requestType,
        const json& data = json::object()) {
        json resp = {
            {"type", "response"},
            {"request_id", requestId},
            {"request_type", requestType},
            {"status", 0},
            {"error_code", 0}
        };
        if (!data.empty()) {
            resp["data"] = data;
        }
        return resp;
    }

    // 启动用户守护线程（token 唯一标识），线程生命周期由客户端连接控制：
    // 在客户端断开时应调用 stopUserWatcher(token) 来结束守护线程。
    static void startUserWatcher(const std::string& token, const std::string& username) {
        std::lock_guard<std::mutex> lk(userWatchersMutex);
        if (userWatchers.find(token) != userWatchers.end()) return; // 已有守护线程

        auto stopFlag = std::make_shared<std::atomic<bool>>(false);
        userWatchers[token] = stopFlag;

        // 启动后台线程轮询数据库
        std::thread([token, username, stopFlag]() {
            // 创建行情处理器实例
            CMduserHandler& handler = CMduserHandler::GetHandler();
            // 连接并登录行情服务器
            handler.connect();
            handler.login();
            while (!stopFlag->load()) {
				// 最新行情缓存,之后和数据库对比
                unordered_map<string, double> m_lastPrices = handler.m_lastPrices;
                
                try {
                    std::unique_ptr<sql::Connection> conn(getConn());
                    if (conn) {
                        conn->setSchema("cpptestmysql");
                        std::unique_ptr<sql::PreparedStatement> stmt(
                            conn->prepareStatement("SELECT orderId, symbol, max_price, min_price, trigger_time, state FROM alert_order WHERE account=? AND state=0")
                        );
                        stmt->setString(1, username);
                        //查询了未处理的预警单
                        std::unique_ptr<sql::ResultSet> res(stmt->executeQuery());
                        
                        // 关键修正：使用 std::move(res) 传递 unique_ptr 所有权
                        //std::vector<std::string> constra = LoadContracts1(std::move(res));
                        std::vector<AlertOrder> order = LoadAlertOrders(std::move(res));
                        //对比
                        if (order.size() > 0) {
                            for (auto it = m_lastPrices.begin(); it != m_lastPrices.end(); ++it) {
                            
                                CheckAlert(order[0].symbol, it->second, order);
                            }
                            
                        }
                            
                        
                    }
                }
                catch (sql::SQLException& e) {
                    std::cerr << "[守护线程] 数据库异常: " << e.what() << std::endl;
                }
                catch (...) {
                    // 忽略
                }

                std::this_thread::sleep_for(std::chrono::seconds(5));
            }

            // 线程退出前从映射中移除自身
            std::lock_guard<std::mutex> lk2(userWatchersMutex);
            auto it = userWatchers.find(token);
            if (it != userWatchers.end() && it->second == stopFlag) {
                userWatchers.erase(it);
            }
        }).detach();
    }

    static std::vector<AlertOrder> LoadAlertOrders(std::unique_ptr<sql::ResultSet> res)
    {
        std::vector<AlertOrder> orders;
        try {
            while (res && res->next()) {
                AlertOrder a;
                a.orderId = res->getInt("orderId");
                a.symbol = res->getString("symbol");
                // 如果数据库字段可能为 NULL，使用 isNull 检查
                if (res->isNull("max_price")) a.max_price = 0.0;
                else a.max_price = res->getDouble("max_price");
                if (res->isNull("min_price")) a.min_price = 0.0;
                else a.min_price = res->getDouble("min_price");
                if (res->isNull("trigger_time")) {
                    a.trigger_time = "";
                }
                else {
                    a.trigger_time = res->getString("trigger_time");
                }
                a.state = res->getInt("state");
                orders.push_back(std::move(a));
            }
        }
        catch (sql::SQLException& e) {
            // 可根据项目日志规范记录错误
            fflush(stdout);
        }
        return orders;
    }

    static void CheckAlert(const string& symbol, double price, vector<AlertOrder> alerts)
    {
        
        // 获取当前时间 - 使用安全的 localtime_s
        time_t now = time(0);
        tm local_tm = { 0 };
        localtime_s(&local_tm, &now);  // 使用 localtime_s 替代 localtime
        char time_buffer[20];
        strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", &local_tm);
        string current_time_str = string(time_buffer);
		string message;
        for (auto& a : alerts)
        {
            bool triggered = false;
            string reason;

            // 价格预警判断
            if (a.max_price > 0 && price >= a.max_price) {
                triggered = true;
                reason = ">= 上限 " + to_string(a.max_price);
            }
            else if (a.min_price > 0 && price <= a.min_price) {
                triggered = true;
                message = "您的" + a.symbol + "合约已跌破下限 " + to_string(a.min_price) + "，当前价格 " + to_string(price) + "！";
            }

            // 时间预警判断（保持原有逻辑）
            if (!triggered && !a.trigger_time.empty()) {
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

                    // 计算预警时间
                    time_t trigger_time_t = mktime(&trigger_tm);
                    time_t one_day_before = trigger_time_t ;  // 不减去一天的秒数

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
                //SendResponse(ThreadLocalUser::GetClient(), responseData)
                ClientContext* client = ThreadLocalUser::GetClient();
                if (!client) {
                    // 无有效客户端上下文则跳过发送
                    continue;
                }

                // 构造唯一 alert_id（可按需替换为更复杂的生成策略）
                string alertId = "msg_" + to_string(a.orderId) + "_" + to_string((long)now);

                // 构造 JSON 响应 (符合 protocol.md alert_triggered 格式)
                json j = {
                    {"type", "alert_triggered"},
                    {"alert_id", alertId},
                    {"order_id", to_string(a.orderId)},
                    {"symbol", a.symbol},
                    {"trigger_value", price},
                    {"trigger_time", current_time_str}
                };

                string responseData = j.dump();

                // 发送响应（忽略返回值或根据需要记录）
                SendResponse(client, responseData);
            }
        }
    }

    static bool SendResponse(ClientContext* client, const std::string& responseData) {
        // 检查响应长度
        if (responseData.size() > client->writeMsg.max_body_length) {
            std::cerr << "[响应过长] 超过最大长度: " << client->writeMsg.max_body_length << std::endl;
            return false;
        }

        // 填充响应数据
        client->writeMsg.body_length(responseData.size());
        std::memcpy(client->writeMsg.body(), responseData.data(), responseData.size());
        client->writeMsg.encode_header();

        // 发送数据
        int totalSent = 0;
        int dataLength = client->writeMsg.length();
        const char* data = client->writeMsg.data();

        while (totalSent < dataLength) {
            int sent = send(client->clientSocket, data + totalSent, dataLength - totalSent, 0);
            if (sent == SOCKET_ERROR) {
                return false;
            }
            totalSent += sent;
        }

        return true;
    }

    // 转化格式的合约列表
    static std::vector<std::string> LoadContracts1(std::unique_ptr<sql::ResultSet> res)
    {
        std::vector<std::string> contracts;

        try {
            
            while (res->next()) {
                contracts.push_back(res->getString("symbol"));
            }

            //printf("从数据库加载了 %zu 个合约\n", contracts.size());
            for (const auto& contract : contracts) {
                printf("  - %s\n", contract.c_str());
            }
            fflush(stdout);
        }
        catch (sql::SQLException& e) {
            //printf("[DB ERROR] 加载合约列表失败: %s\n", e.what());
            fflush(stdout);
        }

        return contracts;
    }

public:
    
    // 请求停止某个 token 对应的守护线程（通常在客户端断开时调用）
    static void stopUserWatcher(const std::string& token) {
        std::lock_guard<std::mutex> lk(userWatchersMutex);
        auto it = userWatchers.find(token);
        if (it != userWatchers.end()) {
            it->second->store(true);
            // 由守护线程最终从映射中移除自己的条目
        }
    }

    FuturesAlertServer() {
        // 初始化路由映射
        requestHandlers = {
            {"register", &FuturesAlertServer::handleRegister},
            {"login", &FuturesAlertServer::handleLogin},
            {"set_email", &FuturesAlertServer::handleSetEmail},
            // 预警相关（支持 price 与 time 两种 warning_type，通过字段区分）
            {"add_warning", &FuturesAlertServer::handleAddWarning},
            {"delete_warning", &FuturesAlertServer::handleDeleteWarning},
            {"modify_warning", &FuturesAlertServer::handleModifyWarning},
            {"query_warnings", &FuturesAlertServer::handleQueryWarnings},
            {"alert_ack", &FuturesAlertServer::handleAlertAck}
        };
    }

    // 路由主函数：分发请求到对应的处理函数
    json routeRequest(const json& requestData) {
        try {
            // 检查是否包含type字段
            if (!requestData.contains("type")) {
                return createErrorResponse("", "", "缺少type字段");
            }

            std::string reqType = requestData["type"];
            std::string reqId = requestData.contains("request_id") ? requestData["request_id"] : "";

            // 查找对应的处理函数
            auto it = requestHandlers.find(reqType);
            if (it == requestHandlers.end()) {
                return createErrorResponse(reqId, reqType, "不支持的请求类型: " + reqType);
            }

            // 调用处理函数
            return it->second(*this, requestData);
        }
        catch (const std::exception& e) {
            std::string reqId = requestData.contains("request_id") ? requestData["request_id"] : "";
            std::string reqType = requestData.contains("type") ? requestData["type"] : "";
            return createErrorResponse(reqId, reqType, "处理请求时发生错误: " + std::string(e.what()));
        }
        catch (...) {
            std::string reqId = requestData.contains("request_id") ? requestData["request_id"] : "";
            std::string reqType = requestData.contains("type") ? requestData["type"] : "";
            return createErrorResponse(reqId, reqType, "处理请求时发生未知错误");
        }
    }

    // ---------------------- 数据库配置（可根据需要改） ----------------------
    static sql::Connection* getConn() {
        sql::Driver* driver = get_driver_instance();
        return driver->connect("tcp://127.0.0.1:3306", "root", "123456");
    }

    // ---------------------- 注册 ----------------------
    static json handleRegister(FuturesAlertServer& server, const json& request) {
        std::string reqId = request["request_id"];
        std::string username = request["username"];
        std::string password = request["password"];

        try {
            std::unique_ptr<sql::Connection> conn(getConn());
            conn->setSchema("cpptestmysql");

            std::unique_ptr<sql::PreparedStatement> stmt(
                conn->prepareStatement("INSERT INTO user(account, password, state) VALUES(?, ?, 0)")
            );
            stmt->setString(1, username);
            stmt->setString(2, password);
            stmt->execute();

            return server.createSuccessResponse(reqId, "register");
        }
        catch (sql::SQLException& e) {
            // 用户名已存在 - 使用 error_code 2003
            return server.createErrorResponse(reqId, "register", 2003, "用户名已存在");
        }
    }

    // ---------------------- 登录 ----------------------
    static json handleLogin(FuturesAlertServer& server, const json& request) {
        std::string reqId = request["request_id"];
        std::string username = request["username"];
        std::string password = request["password"];

        try {
            std::unique_ptr<sql::Connection> conn(getConn());
            conn->setSchema("cpptestmysql");

            std::unique_ptr<sql::PreparedStatement> stmt(
                conn->prepareStatement("SELECT userId FROM user WHERE account=? AND password=? AND state=0")
            );
            stmt->setString(1, username);
            stmt->setString(2, password);
            std::unique_ptr<sql::ResultSet> res(stmt->executeQuery());

            if (res->next()) {
                std::string token = "token_" + username;

                // 登录成功后启动与该用户关联的守护线程（生命周期由客户端连接控制）
                startUserWatcher(token, username);
				ThreadLocalUser::SetUserID(username);
				ThreadLocalUser::SetUserToken(token);
                return server.createSuccessResponse(reqId, "login");
            }
            // 用户名或密码错误 - 使用 error_code 2002
            return server.createErrorResponse(reqId, "login", 2002, "用户名或密码错误");
        }
        catch (sql::SQLException& e) {
            // 数据库错误 - 使用 error_code 1006
            return server.createErrorResponse(reqId, "login", 1006, e.what());
        }
    }

    // ---------------------- 设置邮箱 ----------------------
    static json handleSetEmail(FuturesAlertServer& server, const json& request) {
        std::string reqId = request["request_id"];
        std::string username = request["username"];
        std::string email = request["email"];

        try {
            std::unique_ptr<sql::Connection> conn(getConn());
            conn->setSchema("cpptestmysql");

            std::unique_ptr<sql::PreparedStatement> stmt(
                conn->prepareStatement("UPDATE user SET email=? WHERE account=?")
            );
            stmt->setString(1, email);
            stmt->setString(2, username);
            stmt->execute();

            return server.createSuccessResponse(reqId, "set_email");
        }
        catch (...) {
            return server.createErrorResponse(reqId, "set_email", 1006, "设置邮箱失败");
        }
    }

    // ---------------------- 添加预警单 ----------------------
    // 支持两种 warning_type: "price" 和 "time"
    static json handleAddWarning(FuturesAlertServer& server, const json& request) {
        std::string reqId = request.contains("request_id") ? request["request_id"] : "";
        std::string username = request.contains("account") ? request["account"] : (request.contains("username") ? request["username"] : "");
        std::string symbol = request.contains("symbol") ? request["symbol"] : "";
        std::string warningType = request.contains("warning_type") ? request["warning_type"] : "price";

        if (username.empty() || symbol.empty()) {
            return server.createErrorResponse(reqId, "add_warning", 1003, "缺少 account 或 symbol 字段");
        }

        try {
            std::unique_ptr<sql::Connection> conn(getConn());
            conn->setSchema("cpptestmysql");

            if (warningType == "price") {
                if (!request.contains("max_price") || !request.contains("min_price")) {
                    return server.createErrorResponse(reqId, "add_warning", 1003, "价格预警缺少 max_price 或 min_price");
                }
                double maxPrice = request["max_price"];
                double minPrice = request["min_price"];

                std::unique_ptr<sql::PreparedStatement> stmt(
                    conn->prepareStatement(
                        "INSERT INTO alert_order(account, symbol, max_price, min_price, trigger_time, state) "
                        "VALUES (?, ?, ?, ?, NULL, 0)"
                    )
                );

                stmt->setString(1, username);
                stmt->setString(2, symbol);
                stmt->setDouble(3, maxPrice);
                stmt->setDouble(4, minPrice);
                stmt->execute();
            }
            else if (warningType == "time") {
                if (!request.contains("trigger_time")) {
                    return server.createErrorResponse(reqId, "add_warning", 1003, "时间预警缺少 trigger_time");
                }
                std::string triggerTime = request["trigger_time"];

                std::unique_ptr<sql::PreparedStatement> stmt(
                    conn->prepareStatement(
                        "INSERT INTO alert_order(account, symbol, max_price, min_price, trigger_time, state) "
                        "VALUES (?, ?, NULL, NULL, ?, 0)"
                    )
                );

                stmt->setString(1, username);
                stmt->setString(2, symbol);
                stmt->setString(3, triggerTime);
                stmt->execute();
            }
            else {
                return server.createErrorResponse(reqId, "add_warning", 1004, "未知的 warning_type: " + warningType);
            }

            // 获取刚插入的ID
            std::unique_ptr<sql::PreparedStatement> stmt2(
                conn->prepareStatement("SELECT LAST_INSERT_ID() AS id")
            );
            std::unique_ptr<sql::ResultSet> res(stmt2->executeQuery());
            res->next();
            long orderId = res->getInt("id");

            return server.createSuccessResponse(reqId, "add_warning", {
                {"order_id", orderId}
                });
        }
        catch (...) {
            return server.createErrorResponse(reqId, "add_warning", 1006, "添加预警单失败");
        }
    }

    // ---------------------- 删除预警单 ----------------------
    static json handleDeleteWarning(FuturesAlertServer& server, const json& request) {
        std::string reqId = request["request_id"];
        long orderId = request["order_id"];

        try {
            std::unique_ptr<sql::Connection> conn(getConn());
            conn->setSchema("cpptestmysql");

            std::unique_ptr<sql::PreparedStatement> stmt(
                conn->prepareStatement("DELETE FROM alert_order WHERE orderId=?")
            );
            stmt->setInt(1, orderId);
            stmt->execute();

            return server.createSuccessResponse(reqId, "delete_warning");
        }
        catch (...) {
            return server.createErrorResponse(reqId, "delete_warning", 3001, "删除失败");
        }
    }

    // ---------------------- 修改预警单 ----------------------
    // 支持修改 price 或 time 类型的字段（可选字段）
    static json handleModifyWarning(FuturesAlertServer& server, const json& request) {
        std::string reqId = request.contains("request_id") ? request["request_id"] : "";
        long orderId = request["order_id"];
        std::string warningType = request.contains("warning_type") ? request["warning_type"] : "price";

        try {
            std::unique_ptr<sql::Connection> conn(getConn());
            conn->setSchema("cpptestmysql");

            if (warningType == "price") {
                bool hasMax = request.contains("max_price");
                bool hasMin = request.contains("min_price");

                if (!hasMax && !hasMin) {
                    return server.createErrorResponse(reqId, "modify_warning", 1003, "价格预警未提供可修改的字段");
                }

                if (hasMax && hasMin) {
                    double maxPrice = request["max_price"];
                    double minPrice = request["min_price"];
                    std::unique_ptr<sql::PreparedStatement> stmt(
                        conn->prepareStatement("UPDATE alert_order SET max_price=?, min_price=? WHERE orderId=?")
                    );
                    stmt->setDouble(1, maxPrice);
                    stmt->setDouble(2, minPrice);
                    stmt->setInt(3, orderId);
                    stmt->execute();
                }
                else if (hasMax) {
                    double maxPrice = request["max_price"];
                    std::unique_ptr<sql::PreparedStatement> stmt(
                        conn->prepareStatement("UPDATE alert_order SET max_price=? WHERE orderId=?")
                    );
                    stmt->setDouble(1, maxPrice);
                    stmt->setInt(2, orderId);
                    stmt->execute();
                }
                else { // hasMin
                    double minPrice = request["min_price"];
                    std::unique_ptr<sql::PreparedStatement> stmt(
                        conn->prepareStatement("UPDATE alert_order SET min_price=? WHERE orderId=?")
                    );
                    stmt->setDouble(1, minPrice);
                    stmt->setInt(2, orderId);
                    stmt->execute();
                }
            }
            else if (warningType == "time") {
                if (!request.contains("trigger_time")) {
                    return server.createErrorResponse(reqId, "modify_warning", 1003, "时间预警未提供 trigger_time");
                }
                std::string triggerTime = request["trigger_time"];
                std::unique_ptr<sql::PreparedStatement> stmt(
                    conn->prepareStatement("UPDATE alert_order SET trigger_time=? WHERE orderId=?")
                );
                stmt->setString(1, triggerTime);
                stmt->setInt(2, orderId);
                stmt->execute();
            }
            else {
                return server.createErrorResponse(reqId, "modify_warning", 1004, "未知的 warning_type: " + warningType);
            }

            return server.createSuccessResponse(reqId, "modify_warning");
        }
        catch (...) {
            return server.createErrorResponse(reqId, "modify_warning", 1006, "修改失败");
        }
    }

    // ---------------------- 查询预警单 ----------------------
    static json handleQueryWarnings(FuturesAlertServer& server, const json& request) {
		static const string map1[3] = { "active", "triggered","all" };
        std::string reqId = request["request_id"];
        std::string username = request["username"];

        try {
            std::unique_ptr<sql::Connection> conn(getConn());
            conn->setSchema("cpptestmysql");

            std::unique_ptr<sql::PreparedStatement> stmt(
                conn->prepareStatement(
                    "SELECT orderId, symbol, max_price, min_price, trigger_time, state "
                    "FROM alert_order WHERE account=?"
                )
            );
            stmt->setString(1, username);
            std::unique_ptr<sql::ResultSet> res(stmt->executeQuery());

            json arr = json::array();
            while (res->next()) {
                arr.push_back({
                    {"order_id", res->getInt("orderId")},
                    {"symbol", res->getString("symbol")},
                    {"max_price", res->isNull("max_price") ? nullptr : json(res->getDouble("max_price"))},
                    {"min_price", res->isNull("min_price") ? nullptr : json(res->getDouble("min_price"))},
                    {"trigger_time", res->isNull("trigger_time") ? "" : res->getString("trigger_time")},
                    {"state", map1[res->getInt("state")]}
                    });
            }

            return server.createSuccessResponse(reqId, "query_warnings", {
                {"warnings", arr}
                });
        }
        catch (...) {
            return server.createErrorResponse(reqId, "query_warnings", 1006, "查询失败");
        }
    }

    // ---------------------- 预警确认 ----------------------
    static json handleAlertAck(FuturesAlertServer& server, const json& request) {
        std::string reqId = request.contains("request_id") ? request["request_id"] : "";
        long orderId = request["order_id"];

        try {
            std::unique_ptr<sql::Connection> conn(getConn());
            conn->setSchema("cpptestmysql");

            std::unique_ptr<sql::PreparedStatement> stmt(
                conn->prepareStatement("UPDATE alert_order SET state=1 WHERE orderId=?")
            );
            stmt->setInt(1, orderId);
            stmt->execute();

            return server.createSuccessResponse(reqId, "alert_ack");
        }
        catch (...) {
            return server.createErrorResponse(reqId, "alert_ack", 1006, "确认失败");
        }
    }

};

//// 使用示例
//int main() {
//    FuturesAlertServer server;
//
//    // 示例：处理一个登录请求
//    json loginRequest = {
//        {"type", "login"},
//        {"request_id", "req_002"},
//        {"username", "client001"},
//        {"password", "hashed_password_here"}
//    };
//
//    json response = server.routeRequest(loginRequest);
//    std::cout << response.dump(4) << std::endl;
//
//    return 0;
//}
#endif