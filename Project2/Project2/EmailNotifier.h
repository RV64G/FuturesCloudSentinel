// EmailNotifier.h
#pragma once
#include <string>
#include <WinSock2.h>
#include <iostream>
#include <mysql/jdbc.h>

class EmailNotifier {
private:
    std::string smtp_server;
    int smtp_port;
    std::string from_email;
    std::string from_password;
    std::string to_email;

    std::string base64_encode(const std::string& input);
    bool send_email(const std::string& subject, const std::string& body);
    std::string GetUserEmail(const std::string& account);
    std::string GetFormattedTime();
public:
    EmailNotifier(const std::string& server, int port, const std::string& from,
        const std::string& password, const std::string& to);
    //bool SendAlertEmail(const std::string& instrument, double price, const std::string& reason);
    bool SendAlertEmail(const std::string& account, const std::string& instrument, double price, const std::string& reason);
};