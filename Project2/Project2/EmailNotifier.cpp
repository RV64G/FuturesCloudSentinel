// EmailNotifier.cpp
#include "EmailNotifier.h"
#include "MduserHandler.h"  // 包含定义 GetConn 的头文件
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>

#pragma comment(lib, "ws2_32.lib")

EmailNotifier::EmailNotifier(const std::string& server, int port, const std::string& from,
    const std::string& password, const std::string& to)
    : smtp_server(server), smtp_port(port), from_email(from),
    from_password(password), to_email(to) {
}

std::string EmailNotifier::base64_encode(const std::string& input) {
    const char* base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string encoded;
    int i = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];
    int length = input.length();
    const char* bytes_to_encode = input.c_str();

    while (length--) {
        char_array_3[i++] = *(bytes_to_encode++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for (i = 0; i < 4; i++)
                encoded += base64_chars[char_array_4[i]];
            i = 0;
        }
    }

    if (i) {
        for (int j = i; j < 3; j++)
            char_array_3[j] = '\0';

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;

        for (int j = 0; j < i + 1; j++)
            encoded += base64_chars[char_array_4[j]];

        while (i++ < 3)
            encoded += '=';
    }

    return encoded;
}

std::string receive_response(SOCKET sock) {
    char buffer[1024];
    std::string response;
    int bytes_received;

    do {
        bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';
            response += buffer;
        }
    } while (bytes_received > 0 && response.find("\r\n") == std::string::npos);

    return response;
}

std::string EmailNotifier::GetFormattedTime() {
    std::time_t now = std::time(nullptr);
    std::tm local_tm = { 0 };
    localtime_s(&local_tm, &now);
    char time_buffer[100];
    std::strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", &local_tm);
    return std::string(time_buffer);
}

std::string EmailNotifier::GetUserEmail(const std::string& account) {
    try {
        std::unique_ptr<sql::Connection> conn(GetConn());
        std::unique_ptr<sql::PreparedStatement> stmt(
            conn->prepareStatement("SELECT email FROM user WHERE account = ?")
        );
        stmt->setString(1, account);
        std::unique_ptr<sql::ResultSet> res(stmt->executeQuery());
        if (res->next()) {
            return res->getString("email");
        }
    }
    catch (sql::SQLException& e) {
        printf("[DB ERROR] GetUserEmail: %s\n", e.what());
    }
    return std::string();
}


bool send_command(SOCKET sock, const std::string& command, const std::string& expected_code) {
    if (send(sock, command.c_str(), command.length(), 0) == SOCKET_ERROR) {
        return false;
    }

    std::string response = receive_response(sock);
    if (response.find(expected_code) != 0) {
        return false;
    }

    return true;
}

bool EmailNotifier::send_email(const std::string& subject, const std::string& body) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return false;
    }

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        WSACleanup();
        return false;
    }

    // 使用 getaddrinfo 替代 gethostbyname
    struct addrinfo hints, * result = nullptr;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    if (getaddrinfo(smtp_server.c_str(), std::to_string(smtp_port).c_str(), &hints, &result) != 0) {
        closesocket(sock);
        WSACleanup();
        return false;
    }

    // 连接服务器
    if (connect(sock, result->ai_addr, (int)result->ai_addrlen) == SOCKET_ERROR) {
        freeaddrinfo(result);
        closesocket(sock);
        WSACleanup();
        return false;
    }

    freeaddrinfo(result);

    std::string response = receive_response(sock);
    if (response.find("220") != 0) {
        closesocket(sock);
        WSACleanup();
        return false;
    }

    bool success = true;

    // SMTP会话
    if (!send_command(sock, "EHLO localhost\r\n", "250")) success = false;
    if (success && !send_command(sock, "AUTH LOGIN\r\n", "334")) success = false;

    if (success) {
        std::string encoded_username = base64_encode(from_email) + "\r\n";
        if (!send_command(sock, encoded_username, "334")) success = false;
    }

    if (success) {
        std::string encoded_password = base64_encode(from_password) + "\r\n";
        if (!send_command(sock, encoded_password, "235")) success = false;
    }

    if (success && !send_command(sock, "MAIL FROM: <" + from_email + ">\r\n", "250")) success = false;
    if (success && !send_command(sock, "RCPT TO: <" + to_email + ">\r\n", "250")) success = false;
    if (success && !send_command(sock, "DATA\r\n", "354")) success = false;

    if (success) {
        std::string email_data =
            "From: " + from_email + "\r\n" +
            "To: " + to_email + "\r\n" +
            "Subject: " + subject + "\r\n" +
            "\r\n" +
            body + "\r\n" +
            ".\r\n";

        if (!send_command(sock, email_data, "250")) success = false;
    }

    if (success) {
        send_command(sock, "QUIT\r\n", "221");
    }

    closesocket(sock);
    WSACleanup();

    return success;
}

bool EmailNotifier::SendAlertEmail(const std::string& account, const std::string& instrument, double price, const std::string& reason) {
    // 根据 account 查询用户邮箱
    std::string user_email = GetUserEmail(account);
    if (user_email.empty()) {
        printf("未找到用户 %s 的邮箱地址\n", account.c_str());
        return false;
    }

    // 使用查询到的邮箱地址发送邮件
    std::string subject = "期货交易预警通知 - " + instrument;
    std::string body = "用户: " + account + "\r\n" +
        "合约: " + instrument + "\r\n" +
        "当前价格: " + std::to_string(price) + "\r\n" +
        "触发原因: " + reason + "\r\n" +
        "时间: " + GetFormattedTime() + "\r\n";

    // 临时设置收件人为用户邮箱（或者修改 send_email 方法支持指定收件人）
    std::string original_to_email = to_email;
    to_email = user_email;
    bool result = send_email(subject, body);
    to_email = original_to_email;  // 恢复原始设置

    return result;
}