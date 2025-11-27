#pragma once

#include <cstdlib>
#include <deque>
#include <iostream>
#include <string>
#include <functional>
#include <boost/asio.hpp>
#include <nlohmann/json.hpp>

using boost::asio::ip::tcp;
using json = nlohmann::json;

// Uncomment the following line to enable debug simulation mode
#define CLIENT_DEBUG_SIMULATION

// --- 协议定义 ---
class ChatMessage {
public:
    enum { header_length = 4 };
    enum { max_body_length = 4096 };

    ChatMessage();

    const char* data() const;
    char* data();
    std::size_t length() const;
    const char* body() const;
    char* body();
    std::size_t body_length() const;
    void body_length(std::size_t new_length);
    bool decode_header();
    void encode_header();

private:
    char data_[header_length + max_body_length];
    std::size_t body_length_;
};

typedef std::deque<ChatMessage> chat_message_queue;
typedef std::function<void(const json&)> MessageCallback;

class FuturesClient {
public:
    FuturesClient(boost::asio::io_context& io_context,
               const tcp::resolver::results_type& endpoints);

    void set_message_callback(MessageCallback callback);

    void register_user(const std::string& username, const std::string& password);
    void login(const std::string& username, const std::string& password);
    void set_email(const std::string& email);
    void add_price_warning(const std::string& account, const std::string& symbol, double max_price, double min_price);
    void add_time_warning(const std::string& account, const std::string& symbol, const std::string& trigger_time);
    void delete_warning(const std::string& order_id);
    void modify_price_warning(const std::string& order_id, double max_price, double min_price);
    void modify_time_warning(const std::string& order_id, const std::string& trigger_time);
    void query_warnings(const std::string& status_filter);
    void close();

private:
    std::string generate_request_id();
    void send_json(const json& j);
    void write(const ChatMessage& msg);
    void do_connect(const tcp::resolver::results_type& endpoints);
    void do_read_header();
    void do_read_body();
    void handle_message(const json& j);
    void do_write();

#ifdef CLIENT_DEBUG_SIMULATION
    void simulate_response(const json& request);
#endif

    boost::asio::io_context& io_context_;
    tcp::socket socket_;
    ChatMessage read_msg_;
    chat_message_queue write_msgs_;
    int request_id_counter_;
    MessageCallback message_callback_;
};
