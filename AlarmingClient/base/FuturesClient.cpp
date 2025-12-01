#include "FuturesClient.h"
#include <cstring>

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
#ifndef CLIENT_DEBUG_SIMULATION
    // 正常模式：发起 TCP 连接
    do_connect(endpoints);
#else
    // 调试模式：跳过连接，直接打印日志
    std::cout << "[Client] Debug Mode (Simulation) Enabled. Network disabled." << std::endl;
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

void FuturesClient::query_warnings(const std::string& status_filter) {
    json j;
    j["type"] = "query_warnings";
    j["request_id"] = generate_request_id();
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

void FuturesClient::send_json(const json& j) {
#ifdef CLIENT_DEBUG_SIMULATION
    // 调试模式：拦截发送，直接调用模拟响应
    std::cout << "[Debug] Sending JSON: " << j.dump() << std::endl;
    simulate_response(j);
    return;
#endif

    // 正常模式：序列化并发送
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
        [this](boost::system::error_code ec, tcp::endpoint) {
            if (!ec) {
                std::cout << "[Client] Connected to server." << std::endl;
                do_read_header();
            } else {
                std::cerr << "[Client] Connect failed: " << ec.message() << std::endl;
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
                try {
                    json j = json::parse(received_data);
                    handle_message(j);
                } catch (json::parse_error& e) {
                    std::cerr << "[Client] JSON parse error: " << e.what() << std::endl;
                }

                // 继续读取下一条消息的 Header
                do_read_header();
            } else {
                socket_.close();
            }
        });
}

void FuturesClient::handle_message(const json& j) {
    // 1. 协议层逻辑：自动回复 ACK
    // 如果收到预警触发消息，必须回复 ACK 告知服务器已收到
    if (j.value("type", "") == "alert_triggered" && j.contains("alert_id")) {
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
        std::cout << "[Client] Unhandled Message: " << j.dump(4) << std::endl;
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

#ifdef CLIENT_DEBUG_SIMULATION
void FuturesClient::simulate_alert_trigger(const std::string& symbol, double price, const std::string& message) {
    json j;
    j["type"] = "alert_triggered";
    j["alert_id"] = "alert_" + std::to_string(std::rand());
    j["symbol"] = symbol;
    j["trigger_price"] = price;
    j["message"] = message;
    j["timestamp"] = "2023-10-27 10:00:00";

    // 模拟服务器推送
    boost::asio::post(io_context_, [this, j]() {
        handle_message(j);
    });
}

void FuturesClient::simulate_response(const json& request) {
    // 模拟服务器处理延迟
    // 在实际场景中，这里可以根据 request["type"] 构造不同的回复
    
    json response;
    response["type"] = "response";
    if (request.contains("request_id")) {
        response["request_id"] = request["request_id"];
    }
    
    std::string req_type = request.value("type", "unknown");
    bool success = true;
    std::string message = "Operation successful";

    // --- 模拟逻辑 ---

    if (req_type == "login") {
        std::string user = request.value("username", "");
        std::string pass = request.value("password", "");
        if (user == "error_user") {
            success = false;
            message = "User not found (Simulated)";
        } else if (pass == "wrong_pass") {
            success = false;
            message = "Invalid password (Simulated)";
        } else {
            message = "Login successful";
        }
    } 
    else if (req_type == "register") {
        std::string user = request.value("username", "");
        if (user == "existing_user") {
            success = false;
            message = "User already exists (Simulated)";
        } else {
            message = "Registration successful";
        }
    }
    else if (req_type == "add_warning") {
        json new_warning = request;
        // 移除请求特有的字段，保留数据字段
        new_warning.erase("type");
        new_warning.erase("request_id");
        
        // 生成模拟 ID
        new_warning["order_id"] = "mock_ord_" + std::to_string(mock_warnings_.size() + 1);
        new_warning["status"] = "active";
        
        mock_warnings_.push_back(new_warning);
        message = "Warning added";
    }
    else if (req_type == "modify_warning") {
        std::string order_id = request.value("order_id", "");
        bool found = false;
        for (auto& w : mock_warnings_) {
            if (w.value("order_id", "") == order_id) {
                // 更新字段
                if (request.contains("max_price")) w["max_price"] = request["max_price"];
                if (request.contains("min_price")) w["min_price"] = request["min_price"];
                if (request.contains("trigger_time")) w["trigger_time"] = request["trigger_time"];
                found = true;
                break;
            }
        }
        if (!found) {
            success = false;
            message = "Warning not found";
        } else {
            message = "Warning modified";
        }
    }
    else if (req_type == "delete_warning") {
        std::string order_id = request.value("order_id", "");
        auto it = std::remove_if(mock_warnings_.begin(), mock_warnings_.end(),
            [&](const json& w) { return w.value("order_id", "") == order_id; });
        
        if (it != mock_warnings_.end()) {
            mock_warnings_.erase(it, mock_warnings_.end());
            message = "Warning deleted";
        } else {
            success = false;
            message = "Warning not found";
        }
    }
    else if (req_type == "query_warnings") {
        response["data"] = mock_warnings_;
    }

    response["success"] = success;
    response["message"] = message;

    // 使用 post 将回调放入 io_context 队列，模拟异步接收
    // 这样可以确保回调在 io_context 线程中执行，与真实网络行为一致
    boost::asio::post(io_context_, [this, response]() {
        handle_message(response);
    });
}
#endif

