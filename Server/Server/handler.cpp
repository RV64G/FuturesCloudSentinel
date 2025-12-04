//用于路由请求到不同函数

#include "handler.h"

/*

#include <iostream>
#include <unordered_map>
#include <string>
#include <functional>
#include "nlohmann/json.hpp"  // 使用nlohmann/json库处理JSON
#include <mysql/jdbc.h>

#define WIN32_LEAN_AND_MEAN
using json = nlohmann::json;

// 前置声明处理函数
class FuturesAlertServer;
using RequestHandler = std::function<json(FuturesAlertServer&, const json&)>;

class FuturesAlertServer {
private:
    // 路由映射表：请求类型 -> 处理函数
    std::unordered_map<std::string, RequestHandler> requestHandlers;

    // 通用错误响应生成函数
    json createErrorResponse(const std::string& requestId,
        const std::string& requestType,
        const std::string& errorMsg) {
        return {
            {"type", "response"},
            {"request_id", requestId},
            {"request_type", requestType},
            {"success", false},
            {"message", errorMsg},
            {"data", json::object()}
        };
    }

    // 通用成功响应生成函数
    json createSuccessResponse(const std::string& requestId,
        const std::string& requestType,
        const json& data = json::object(),
        const std::string& msg = "操作成功") {
        return {
            {"type", "response"},
            {"request_id", requestId},
            {"request_type", requestType},
            {"success", true},
            {"message", msg},
            {"data", data}
        };
    }

public:
    FuturesAlertServer() {
        // 初始化路由映射
        requestHandlers = {
            {"register", &FuturesAlertServer::handleRegister},
            {"login", &FuturesAlertServer::handleLogin},
            {"set_email", &FuturesAlertServer::handleSetEmail},
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

    // 处理函数实现（仅为示例框架）
    static json handleRegister(FuturesAlertServer& server, const json& request) {
        // 实际实现注册逻辑
        std::string reqId = request["request_id"];
        // 解析username和password：request["username"], request["password"]
        return server.createSuccessResponse(reqId, "register", { {"message", "注册成功"} });
    }

    static json handleLogin(FuturesAlertServer& server, const json& request) {
        std::string reqId = request["request_id"];
        // 实现登录逻辑
        return server.createSuccessResponse(reqId, "login", { {"token", "sample_token"} });
    }

    static json handleSetEmail(FuturesAlertServer& server, const json& request) {
        std::string reqId = request["request_id"];
        // 实现设置邮箱逻辑
        return server.createSuccessResponse(reqId, "set_email");
    }

    static json handleAddWarning(FuturesAlertServer& server, const json& request) {
        std::string reqId = request["request_id"];
        // 实现添加预警单逻辑
        return server.createSuccessResponse(reqId, "add_warning", { {"order_id", "1001"} });
    }

    static json handleDeleteWarning(FuturesAlertServer& server, const json& request) {
        std::string reqId = request["request_id"];
        // 实现删除预警单逻辑
        return server.createSuccessResponse(reqId, "delete_warning");
    }

    static json handleModifyWarning(FuturesAlertServer& server, const json& request) {
        std::string reqId = request["request_id"];
        // 实现修改预警单逻辑
        return server.createSuccessResponse(reqId, "modify_warning");
    }

    static json handleQueryWarnings(FuturesAlertServer& server, const json& request) {
        std::string reqId = request["request_id"];
        // 实现查询预警单逻辑
        json data = { {"warnings", json::array()} }; // 示例返回空数组
        return server.createSuccessResponse(reqId, "query_warnings", data);
    }

    static json handleAlertAck(FuturesAlertServer& server, const json& request) {
        // alert_ack可能没有request_id，根据文档处理
        std::string reqId = request.contains("request_id") ? request["request_id"] : "";
        // 实现预警确认逻辑
        return server.createSuccessResponse(reqId, "alert_ack");
    }
};

//// 使用示例
//int main() {
//    FuturesAlertServer server;a
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
*/