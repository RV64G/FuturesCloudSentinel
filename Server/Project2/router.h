#pragma once
#ifndef ROUTER_H
#define ROUTER_H
#include <string>
#include "handler.h"
#include "nlohmann/json.hpp"

using json = nlohmann::json;
/*

// 简单的本地编码（CP_ACP）到 UTF-8 的转换器
static inline std::string to_utf8(const std::string& s) {
    if (s.empty()) return {};
    int wlen = MultiByteToWideChar(CP_ACP, 0, s.c_str(), -1, nullptr, 0);
    if (wlen == 0) return {};
    std::wstring w(wlen, L'\0');
    MultiByteToWideChar(CP_ACP, 0, s.c_str(), -1, &w[0], wlen);
    if (!w.empty() && w.back() == L'\0') w.pop_back();

    int len = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len == 0) return {};
    std::string out(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &out[0], len, nullptr, nullptr);
    if (!out.empty() && out.back() == '\0') out.pop_back();
    return out;
}
*/

// 路由类 - 负责请求分发
class RequestRouter {
private:
    FuturesAlertServer server;  // 实际处理请求的服务器实例

public:
    RequestRouter() = default;

    // 路由请求并返回响应
    std::string RouteRequest(const std::string& requestData) {
        try {
            // 解析JSON请求
            json request = json::parse(requestData);

            // 路由到对应的处理函数
            json response = server.routeRequest(request);

            // 转换为字符串返回（容错：如果字符串中包含非 UTF-8，使用 replace 策略）
            return response.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
        }
        catch (const json::parse_error& e) {
            // 处理JSON解析错误
            json errorResp = {
                {"type", "response"},
                {"request_id", ""},
                {"request_type", ""},
                {"success", false},
                {"message", "JSON解析错误: " + std::string(e.what())},
                {"data", json::object()}
            };
            return errorResp.dump();
        }
        catch (const std::exception& e) {
            // 处理其他异常
            json errorResp = {
                {"type", "response"},
                {"request_id", ""},
                {"request_type", ""},
                {"success", false},
                {"message", "处理请求时出错: " + std::string(e.what())},
                {"data", json::object()}
            };
            return errorResp.dump();
        }
    }
};
#endif