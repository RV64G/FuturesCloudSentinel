#pragma once
#ifndef BASE_H
#define BASE_H

#include <cstdlib>
#include <deque>
#include <string>
#include <boost/asio.hpp>
#include <nlohmann/json.hpp>

using boost::asio::ip::tcp;
using json = nlohmann::json;

// 聊天消息协议定义
class ChatMessage {
public:
    enum { header_length = 4 };         // 消息头部长度（用于存储消息体长度）
    enum { max_body_length = 4096 };    // 最大消息体长度（适应JSON数据）

    ChatMessage() : body_length_(0) {}

    // 获取消息数据的常量指针和普通指针
    const char* data() const { return data_; }
    char* data() { return data_; }

    // 获取消息总长度（头部+体部）
    std::size_t length() const { return header_length + body_length_; }

    // 获取消息体的常量指针和普通指针
    const char* body() const { return data_ + header_length; }
    char* body() { return data_ + header_length; }

    // 获取和设置消息体长度
    std::size_t body_length() const { return body_length_; }
#include "base.h"

    void body_length(std::size_t new_length) {
        body_length_ = new_length;
        if (body_length_ > max_body_length)
            body_length_ = max_body_length;
    }

    bool decode_header() {
        char header[header_length + 1] = "";
        //strncat_s(header, data_, header_length);
		strncat_s(header, sizeof(header), data_, header_length);
        body_length_ = std::atoi(header);
        if (body_length_ > max_body_length) {
            body_length_ = 0;
            return false;
        }
        return true;
    }

    void encode_header() {
        char header[header_length + 1] = "";
        //std::sprintf(header, "%4d", static_cast<int>(body_length_));
		sprintf_s(header, sizeof(header), "%4d", static_cast<int>(body_length_));
        std::memcpy(data_, header, header_length);
    }

private:
    char data_[header_length + max_body_length];  // 存储完整消息（头部+体部）
    std::size_t body_length_;                     // 消息体实际长度
};

#endif // BASE_H