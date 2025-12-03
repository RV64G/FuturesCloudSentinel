#include "FuturesClient.h"
#include <cstring>
#include <chrono>

// 日志输出宏 - 在 Qt 环境下使用 qDebug，否则使用 std::cout
#ifdef QT_CORE_LIB
#include <QDebug>
#define LOG_DEBUG(msg) qDebug() << msg
#define LOG_WARN(msg) qWarning() << msg
#define LOG_ERROR(msg) qCritical() << msg
#else
#include <iostream>
#define LOG_DEBUG(msg) std::cout << "[DEBUG] " << msg << std::endl
#define LOG_WARN(msg) std::cerr << "[WARN] " << msg << std::endl
#define LOG_ERROR(msg) std::cerr << "[ERROR] " << msg << std::endl
#endif

// ==========================================
// ChatMessage 类实现
// ==========================================

ChatMessage::ChatMessage() : body_length_(0) {
}

const char* ChatMessage::data() const { return data_; }
char* ChatMessage::data() { return data_; }

std::size_t ChatMessage::length() const { return header_length + body_length_; }

const char* ChatMessage::body() const { return data_ + header_length; }
char* ChatMessage::body() { return data_ + header_length; }

std::size_t ChatMessage::body_length() const { return body_length_; }

void ChatMessage::body_length(std::size_t new_length) {
    body_length_ = new_length;
    if (body_length_ > max_body_length)
        body_length_ = max_body_length;
}

bool ChatMessage::decode_header() {
    char header[header_length + 1] = "";
    std::strncat(header, data_, header_length);
    body_length_ = std::atoi(header);
    if (body_length_ > max_body_length) {
        body_length_ = 0;
        return false;
    }
    return true;
}

// 编码 Header: 将整数长度格式化为4字节 ASCII 字符串 (例如 " 123")
void ChatMessage::encode_header() {
    char header[header_length + 1] = "";
    std::sprintf(header, "%4d", static_cast<int>(body_length_));
    std::memcpy(data_, header, header_length);
}

// ==========================================
// FuturesClient 类实现
// ==========================================

FuturesClient::FuturesClient(boost::asio::io_context& io_context,
           const tcp::resolver::results_type& endpoints)
    : io_context_(io_context),
      socket_(io_context),
      request_id_counter_(0) {
#ifndef SIMULATE_SERVER
    // 正常模式：发起 TCP 连接
    LOG_DEBUG("[FuturesClient] Connecting to server...");
    do_connect(endpoints);
#else
    // 模拟模式：跳过连接，直接打印日志
    LOG_DEBUG("[FuturesClient] Simulation Mode Enabled. Network disabled.");
#endif
}

void FuturesClient::set_message_callback(MessageCallback callback) {
    message_callback_ = callback;
}

// --- 业务方法实现 ---

void FuturesClient::register_user(const std::string& username, const std::string& password) {
    json j;
    j["type"] = "register";
    j["request_id"] = generate_request_id();
    j["username"] = username;
    j["password"] = password;
    send_json(j);
}

void FuturesClient::login(const std::string& username, const std::string& password) {
    json j;
    j["type"] = "login";
    j["request_id"] = generate_request_id();
    j["username"] = username;
    j["password"] = password; 
    send_json(j);
}

void FuturesClient::set_email(const std::string& email) {
    json j;
    j["type"] = "set_email";
    j["request_id"] = generate_request_id();
    j["email"] = email;
    send_json(j);
}

void FuturesClient::add_price_warning(const std::string& account, const std::string& symbol, double max_price, double min_price) {
    json j;
    j["type"] = "add_warning";
    j["request_id"] = generate_request_id();
    j["warning_type"] = "price";
    j["account"] = account;
    j["symbol"] = symbol;
    j["max_price"] = max_price;
    j["min_price"] = min_price;
    send_json(j);
}

void FuturesClient::add_time_warning(const std::string& account, const std::string& symbol, const std::string& trigger_time) {
    json j;
    j["type"] = "add_warning";
    j["request_id"] = generate_request_id();
    j["warning_type"] = "time";
    j["account"] = account;
    j["symbol"] = symbol;
    j["trigger_time"] = trigger_time;
    send_json(j);
}

void FuturesClient::delete_warning(const std::string& order_id) {
    json j;
    j["type"] = "delete_warning";
    j["request_id"] = generate_request_id();
    j["order_id"] = order_id;
    send_json(j);
}

void FuturesClient::modify_price_warning(const std::string& order_id, double max_price, double min_price) {
    json j;
    j["type"] = "modify_warning";
    j["request_id"] = generate_request_id();
    j["order_id"] = order_id;
    j["warning_type"] = "price";
    j["max_price"] = max_price;
    j["min_price"] = min_price;
    send_json(j);
}

void FuturesClient::modify_time_warning(const std::string& order_id, const std::string& trigger_time) {
    json j;
    j["type"] = "modify_warning";
    j["request_id"] = generate_request_id();
    j["order_id"] = order_id;
    j["warning_type"] = "time";
    j["trigger_time"] = trigger_time;
    send_json(j);
}

long long FuturesClient::current_timestamp() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

void FuturesClient::query_warnings(const std::string& account, const std::string& status_filter) {
    json j;
    j["type"] = "query_warnings";
    j["request_id"] = generate_request_id();
    j["account"] = account;
    j["status_filter"] = status_filter;
    send_json(j);
}

void FuturesClient::close() {
    boost::asio::post(io_context_, [this]() { socket_.close(); });
}

// --- 内部逻辑实现 ---

std::string FuturesClient::generate_request_id() {
    return "req_" + std::to_string(++request_id_counter_);
}

void FuturesClient::send_json(const json& j_in) {
    json j = j_in;
    // Add common fields
    j["ver"] = "3.1";
    j["ts"] = current_timestamp();

#ifdef SIMULATE_SERVER
    // 模拟模式：拦截发送，直接调用模拟响应
    LOG_DEBUG("[FuturesClient] Simulate sending: " << j.dump());
    simulate_response(j);
    return;
#endif

    // 正常模式：序列化并发送
    LOG_DEBUG("[FuturesClient] Sending: " << j.dump());
    std::string s = j.dump();
    ChatMessage msg;
    msg.body_length(s.size());
    std::memcpy(msg.body(), s.data(), msg.body_length());
    msg.encode_header();
    write(msg);
}

void FuturesClient::write(const ChatMessage& msg) {
    boost::asio::post(io_context_,
        [this, msg]() {
            bool write_in_progress = !write_msgs_.empty();
            write_msgs_.push_back(msg);
            if (!write_in_progress) {
                do_write();
            }
        });
}

void FuturesClient::do_connect(const tcp::resolver::results_type& endpoints) {
    boost::asio::async_connect(socket_, endpoints,
        [this](boost::system::error_code ec, tcp::endpoint ep) {
            if (!ec) {
                LOG_DEBUG("[FuturesClient] Connected to server: " << ep.address().to_string() << ":" << ep.port());
                do_read_header();
            } else {
                LOG_ERROR("[FuturesClient] Connect FAILED: " << ec.message());
            }
        });
}

void FuturesClient::do_read_header() {
    boost::asio::async_read(socket_,
        boost::asio::buffer(read_msg_.data(), ChatMessage::header_length),
        [this](boost::system::error_code ec, std::size_t /*length*/) {
            if (!ec && read_msg_.decode_header()) {
                do_read_body();
            } else {
                LOG_WARN("[FuturesClient] Read header failed, closing socket");
                socket_.close();
            }
        });
}

void FuturesClient::do_read_body() {
    boost::asio::async_read(socket_,
        boost::asio::buffer(read_msg_.body(), read_msg_.body_length()),
        [this](boost::system::error_code ec, std::size_t /*length*/) {
            if (!ec) {
                // 收到完整消息，进行 JSON 解析
                std::string received_data(read_msg_.body(), read_msg_.body_length());
                LOG_DEBUG("[FuturesClient] Received: " << received_data);
                try {
                    json j = json::parse(received_data);
                    handle_message(j);
                } catch (json::parse_error& e) {
                    LOG_ERROR("[FuturesClient] JSON parse error: " << e.what());
                }

                // 继续读取下一条消息的 Header
                do_read_header();
            } else {
                LOG_WARN("[FuturesClient] Read body failed, closing socket");
                socket_.close();
            }
        });
}

void FuturesClient::handle_message(const json& j) {
    // 1. 协议层逻辑：自动回复 ACK
    // 如果收到预警触发消息，必须回复 ACK 告知服务器已收到
    if (j.value("type", "") == "alert_triggered" && j.contains("alert_id")) {
        LOG_DEBUG("[FuturesClient] Alert triggered, sending ACK");
        json ack;
        ack["type"] = "alert_ack";
        ack["alert_id"] = j["alert_id"];
        send_json(ack);
        // 注意：这里不 return，继续向下传递给 UI，因为 UI 也需要弹窗
    }

    // 2. 通知 UI 层
    if (message_callback_) {
        message_callback_(j);
    } else {
        // 如果没有设置回调，默认打印到控制台方便调试
        LOG_DEBUG("[FuturesClient] Unhandled Message: " << j.dump(2));
    }
}

void FuturesClient::do_write() {
    boost::asio::async_write(socket_,
        boost::asio::buffer(write_msgs_.front().data(),
          write_msgs_.front().length()),
        [this](boost::system::error_code ec, std::size_t /*length*/) {
            if (!ec) {
                write_msgs_.pop_front();
                if (!write_msgs_.empty()) {
                    do_write();
                }
            } else {
                socket_.close();
            }
        });
}

#ifdef SIMULATE_SERVER
void FuturesClient::simulate_alert_trigger(const std::string& symbol, double price, const std::string& message) {
    json j;
    j["type"] = "alert_triggered";
    j["alert_id"] = "alert_" + std::to_string(std::rand());
    j["order_id"] = "mock_ord_1";
    j["symbol"] = symbol;
    j["trigger_value"] = price;
    j["trigger_time"] = "2023-10-27 10:00:00";

    LOG_DEBUG("[FuturesClient] Simulating alert trigger for " << symbol);

    // 模拟服务器推送
    boost::asio::post(io_context_, [this, j]() {
        handle_message(j);
    });
}

void FuturesClient::simulate_response(const json& request) {
    // 按照 protocol.md 构造响应
    // 响应格式: type, request_id, request_type, status, error_code, data
    
    json response;
    response["type"] = "response";
    if (request.contains("request_id")) {
        response["request_id"] = request["request_id"];
    }
    
    std::string req_type = request.value("type", "unknown");
    response["request_type"] = req_type;
    
    int status = 0;      // 0 = 成功
    int error_code = 0;  // 0 = SUCCESS

    // --- 模拟逻辑 ---

    if (req_type == "login") {
        std::string user = request.value("username", "");
        if (user == "error_user") {
            status = 1;
            error_code = 2001;  // USER_NOT_FOUND
        } else if (user == "wrong_pass") {
            status = 1;
            error_code = 2002;  // PASSWORD_INCORRECT
        }
        // 成功时 status=0, error_code=0
    } 
    else if (req_type == "register") {
        std::string user = request.value("username", "");
        if (user == "existing_user") {
            status = 1;
            error_code = 2003;  // USER_ALREADY_EXISTS
        }
    }
    else if (req_type == "add_warning") {
        json new_warning = request;
        new_warning.erase("type");
        new_warning.erase("request_id");
        new_warning.erase("ver");
        new_warning.erase("ts");
        
        // 生成模拟 ID
        std::string order_id = "mock_ord_" + std::to_string(mock_warnings_.size() + 1);
        new_warning["order_id"] = order_id;
        new_warning["status"] = "active";
        new_warning["created_at"] = "2025-12-03T12:00:00Z";
        
        mock_warnings_.push_back(new_warning);
        
        response["data"]["order_id"] = order_id;
    }
    else if (req_type == "modify_warning") {
        std::string order_id = request.value("order_id", "");
        bool found = false;
        for (auto& w : mock_warnings_) {
            if (w.value("order_id", "") == order_id) {
                if (request.contains("max_price")) w["max_price"] = request["max_price"];
                if (request.contains("min_price")) w["min_price"] = request["min_price"];
                if (request.contains("trigger_time")) w["trigger_time"] = request["trigger_time"];
                found = true;
                break;
            }
        }
        if (!found) {
            status = 1;
            error_code = 3001;  // WARNING_NOT_FOUND
        }
    }
    else if (req_type == "delete_warning") {
        std::string order_id = request.value("order_id", "");
        auto it = std::remove_if(mock_warnings_.begin(), mock_warnings_.end(),
            [&](const json& w) { return w.value("order_id", "") == order_id; });
        
        if (it != mock_warnings_.end()) {
            mock_warnings_.erase(it, mock_warnings_.end());
        } else {
            status = 1;
            error_code = 3001;  // WARNING_NOT_FOUND
        }
    }
    else if (req_type == "query_warnings") {
        json data;
        data["warnings"] = mock_warnings_;
        data["total"] = mock_warnings_.size();
        response["data"] = data;
    }
    else if (req_type == "set_email") {
        // 成功，不需要额外处理
    }

    response["status"] = status;
    response["error_code"] = error_code;

    LOG_DEBUG("[FuturesClient] Simulate response: " << response.dump());

    // 使用 post 将回调放入 io_context 队列，模拟异步接收
    boost::asio::post(io_context_, [this, response]() {
        handle_message(response);
    });
}
#endif

