#include "Backend.h"
#include "ContractData.h"
#include <QDebug>
#include <QCryptographicHash>
#include <QCoreApplication>

Backend::Backend(QObject *parent) : QObject(parent) {
    // Load contract codes from hardcoded data
    contractCodes_.clear();
    contractMap_.clear();
    
    for (const auto& entry : RAW_CONTRACT_DATA) {
        // 构造显示文本: "中文名 (代码)"
        QString displayText = QString("%1 (%2)").arg(entry.name, entry.code);
        contractCodes_.append(displayText);
        
        // 存入映射表: "中文名 (代码)" -> "代码"
        // 同时也可以存入 "代码" -> "代码" 以支持直接输入代码的情况
        contractMap_[displayText.toStdString()] = entry.code;
        contractMap_[entry.code] = entry.code; 
    }
    emit contractCodesChanged();

    work_guard_ = std::make_unique<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>(io_context_.get_executor());
    
    tcp::resolver resolver(io_context_);
    // Assuming server is on localhost 8888 for now. 
    // In a real app, this should be configurable.
    try {
        // Note: resolve is blocking here, but it's in constructor so it blocks main thread briefly.
        // Ideally should be async or in the thread.
        auto endpoints = resolver.resolve("127.0.0.1", "8888");
        client_ = std::make_unique<FuturesClient>(io_context_, endpoints);
        
        client_->set_message_callback([this](const nlohmann::json& j) {
            this->onMessageReceived(j);
        });

        io_thread_ = std::thread([this]() {
            io_context_.run();
        });
    } catch (std::exception& e) {
        qCritical() << "Failed to initialize client:" << e.what();
    }
}

Backend::~Backend() {
    if (client_) {
        client_->close();
    }
    if (work_guard_) {
        work_guard_->reset();
    }
    io_context_.stop();
    if (io_thread_.joinable()) {
        io_thread_.join();
    }
}

void Backend::login(const QString &username, const QString &password, const QString &brokerId, const QString &frontAddr) {
    if (client_) {
        current_request_type_ = "login";
        currentUsername_ = username; // 暂存用户名
        QString hashedPassword = QString(QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256).toHex());
        // TODO: Pass brokerId and frontAddr when base library supports it
        client_->login(username.toStdString(), hashedPassword.toStdString());
    } else {
        emit loginFailed("Not connected to server");
    }
}

void Backend::registerUser(const QString &username, const QString &password) {
    if (client_) {
        current_request_type_ = "register";
        currentUsername_ = username; // 注册成功后通常自动登录或需要记录
        QString hashedPassword = QString(QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256).toHex());
        client_->register_user(username.toStdString(), hashedPassword.toStdString());
    } else {
        emit registerFailed("Not connected to server");
    }
}

void Backend::addPriceWarning(const QString &symbolText, double maxPrice, double minPrice) {
    if (!client_) {
        emit showMessage("Not connected to server");
        return;
    }
    if (currentUsername_.isEmpty()) {
        emit showMessage("Please login first");
        return;
    }

    std::string symbol = extractContractCode(symbolText);
    current_request_type_ = "add_warning";
    // 注意：这里假设了 account 就是 username。实际业务中可能不同。
    client_->add_price_warning(currentUsername_.toStdString(), symbol, maxPrice, minPrice);
}

std::string Backend::extractContractCode(const QString &text) {
    std::string key = text.trimmed().toStdString();
    
    // 1. 尝试直接从映射表中查找
    auto it = contractMap_.find(key);
    if (it != contractMap_.end()) {
        return it->second;
    }
    
    // 2. 如果没找到 (可能是用户手动输入的未知代码)，尝试解析括号
    // 兼容旧逻辑，防止用户输入了不在列表中的格式
    int start = text.lastIndexOf('(');
    int end = text.lastIndexOf(')');
    
    if (start != -1 && end != -1 && end > start) {
        return text.mid(start + 1, end - start - 1).trimmed().toStdString();
    }
    
    // 3. 默认返回原文本
    return key;
}

void Backend::onMessageReceived(const nlohmann::json& j) {
    std::string type = j.value("type", "");
    
    if (type == "response") {
        bool success = j.value("success", false);
        std::string message = j.value("message", "");
        
        // 使用本地记录的请求类型
        std::string requestType = current_request_type_;
        current_request_type_ = ""; // 处理完后重置

        if (requestType == "login") {
            if (success) {
                emit loginSuccess();
            } else {
                emit loginFailed(QString::fromStdString(message));
            }
        } else if (requestType == "register") {
            if (success) {
                emit registerSuccess();
            } else {
                emit registerFailed(QString::fromStdString(message));
            }
        } else {
            if (!success || !message.empty()) {
                 emit showMessage(QString::fromStdString(message));
            }
        }
    } else if (type == "alert_triggered") {
        // 处理服务器推送的预警消息
        std::string msg = j.value("message", "Alert Triggered!");
        // 通过信号通知 UI 层显示弹窗
        emit showMessage(QString::fromStdString(msg));
    } else if (type == "login_response") {
        // 兼容旧代码逻辑 (如果服务器改回去了)
        bool success = j.value("success", false);
        if (success) {
            emit loginSuccess();
        } else {
            std::string msg = j.value("message", "Login failed");
            emit loginFailed(QString::fromStdString(msg));
        }
    } else if (type == "register_response") {
        bool success = j.value("success", false);
        if (success) {
            emit registerSuccess();
        } else {
            std::string msg = j.value("message", "Registration failed");
            emit registerFailed(QString::fromStdString(msg));
        }
    }
}
