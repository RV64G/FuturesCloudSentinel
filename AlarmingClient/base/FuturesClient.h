#pragma once

#include <cstdlib>
#include <deque>
#include <iostream>
#include <string>
#include <functional>
#include <boost/asio.hpp>
#include <nlohmann/json.hpp>

using boost::asio::ip::
tcp;
using json = nlohmann::json;

// ============================================================================
// 编译选项 (Build Configuration)
// ============================================================================
// 
// DEBUG_UI: 启用调试 UI 功能
//   - 显示服务器 IP/端口输入框
//   - 显示"模拟触发"按钮
//   - IP/端口设置会被持久化
//
// SIMULATE_SERVER: 启用模拟服务器响应
//   - 不发起真实 TCP 连接
//   - 所有请求由本地模拟器处理
//   - 用于在无服务器环境下测试 UI 流程
//
// 可以单独或同时启用：
//   - 只启用 DEBUG_UI：连接真实服务器，但有调试界面
//   - 只启用 SIMULATE_SERVER：使用模拟数据，但不显示调试界面
//   - 两者都启用：模拟数据 + 调试界面（完整调试模式）
//   - 两者都禁用：生产模式，连接真实服务器
// ============================================================================

// #define DEBUG_UI
// #define SIMULATE_SERVER

// Legacy macro support (保持向后兼容)
//#define GLOBAL_DEBUG_MODE
//#define DEBUG_UI
//#define SIMULATE_SERVER
#ifdef GLOBAL_DEBUG_MODE
    #define DEBUG_UI
    #define SIMULATE_SERVER
#endif

// --- 协议定义 ---

/**
 * @brief 消息封装类 (ChatMessage)
 * 
 * 负责处理 TCP 粘包/拆包问题。
 * 协议格式: [Header (4 bytes)] + [Body (N bytes)]
 * Header 是一个 ASCII 字符串，表示 Body 的长度。
 */
class ChatMessage {
public:
    enum { header_length = 4 };          ///< 包头长度 (固定 4 字节)
    enum { max_body_length = 4096 };     ///< 包体最大长度 (4KB)

    ChatMessage();

    // --- 数据访问接口 ---
    const char* data() const;            ///< 获取完整数据包指针 (Header + Body)
    char* data();                        ///< 获取完整数据包指针 (可写)
    std::size_t length() const;          ///< 获取完整数据包总长度

    const char* body() const;            ///< 获取包体数据指针
    char* body();                        ///< 获取包体数据指针 (可写)
    std::size_t body_length() const;     ///< 获取包体长度

    void body_length(std::size_t new_length); ///< 设置包体长度

    // --- 编解码接口 ---
    
    /**
     * @brief 解析包头
     * 从 data_ 的前 4 个字节解析出 body_length_。
     * @return true 解析成功, false 解析失败 (长度非法)
     */
    bool decode_header();

    /**
     * @brief 编码包头
     * 将 body_length_ 格式化为 4 字节的 ASCII 字符串并写入 data_ 头部。
     */
    void encode_header();

private:
    char data_[header_length + max_body_length]; ///< 数据缓冲区
    std::size_t body_length_;                    ///< 当前包体长度
};

typedef std::deque<ChatMessage> chat_message_queue;
typedef std::function<void(const json&)> MessageCallback;

/**
 * @brief 期货预警客户端核心类 (FuturesClient)
 * 
 * 负责与服务器建立 TCP 连接，发送业务请求，并接收服务器推送的消息。
 * 支持异步 IO，不会阻塞主线程。
 */
class FuturesClient {
public:
    /**
     * @brief 构造函数
     * @param io_context Boost.Asio 的 IO 上下文
     * @param endpoints 服务器地址解析结果
     */
    FuturesClient(boost::asio::io_context& io_context,
               const tcp::resolver::results_type& endpoints);

    /**
     * @brief 设置消息回调函数
     * UI 层通过此接口注册回调，当收到服务器消息时会被调用。
     * @param callback 回调函数，参数为解析后的 JSON 对象
     */
    void set_message_callback(MessageCallback callback);

    // --- 业务接口 (Business Logic API) ---

    /**
     * @brief 用户注册
     * @param username 用户名
     * @param password 密码 (建议哈希后传入)
     */
    void register_user(const std::string& username, const std::string& password);

    /**
     * @brief 用户登录
     * @param username 用户名
     * @param password 密码
     */
    void login(const std::string& username, const std::string& password);

    /**
     * @brief 设置接收邮箱
     * @param email 邮箱地址
     */
    void set_email(const std::string& email);

    /**
     * @brief 添加价格预警
     * @param account 期货账号
     * @param symbol 合约代码 (如 rb2310)
     * @param max_price 价格上限
     * @param min_price 价格下限
     */
    void add_price_warning(const std::string& account, const std::string& symbol, double max_price, double min_price);

    /**
     * @brief 添加时间预警
     * @param account 期货账号
     * @param symbol 合约代码
     * @param trigger_time 触发时间 (YYYY-MM-DD HH:MM:SS)
     */
    void add_time_warning(const std::string& account, const std::string& symbol, const std::string& trigger_time);

    /**
     * @brief 删除预警单
     * @param order_id 预警单 ID
     */
    void delete_warning(const std::string& order_id);

    /**
     * @brief 修改价格预警
     * @param order_id 预警单 ID
     * @param max_price 新的上限
     * @param min_price 新的下限
     */
    void modify_price_warning(const std::string& order_id, double max_price, double min_price);

    /**
     * @brief 修改时间预警
     * @param order_id 预警单 ID
     * @param trigger_time 新的触发时间
     */
    void modify_time_warning(const std::string& order_id, const std::string& trigger_time);

    /**
     * @brief 查询预警单列表
     * @param account 用户名/账号
     * @param status_filter 状态过滤 ("active", "triggered", "all")
     */
    void query_warnings(const std::string& account, const std::string& status_filter);

    /**
     * @brief 关闭连接
     * 线程安全地关闭 Socket 连接。
     */
    void close();

#if defined(DEBUG_UI) || defined(SIMULATE_SERVER)
    /**
     * @brief 模拟触发预警 (调试用)
     * 手动触发一条预警消息推送给客户端
     */
    void simulate_alert_trigger(const std::string& symbol, double price, const std::string& message);
#endif

private:
    // --- 内部辅助方法 ---
    long long current_timestamp();              ///< 获取当前时间戳 (毫秒)
    std::string generate_request_id();          ///< 生成唯一的请求 ID
    void send_json(const json& j);              ///< 序列化 JSON 并发送
    void write(const ChatMessage& msg);         ///< 将消息推入发送队列

    // --- Asio 异步操作 ---
    void do_connect(const tcp::resolver::results_type& endpoints); ///< 发起连接
    void do_read_header();                                         ///< 读取包头
    void do_read_body();                                           ///< 读取包体
    void handle_message(const json& j);                            ///< 处理完整的 JSON 消息
    void do_write();                                               ///< 执行实际的 Socket 写入

#ifdef SIMULATE_SERVER
    /**
     * @brief 模拟服务器响应 (调试模式)
     * 在不连接真实服务器的情况下，构造虚假的响应数据并触发回调。
     */
    void simulate_response(const json& request);

    std::vector<json> mock_warnings_; // 模拟的内存数据库
#endif

    boost::asio::io_context& io_context_;
    tcp::socket socket_;
    ChatMessage read_msg_;
    chat_message_queue write_msgs_;
    int request_id_counter_;
    MessageCallback message_callback_;
};
