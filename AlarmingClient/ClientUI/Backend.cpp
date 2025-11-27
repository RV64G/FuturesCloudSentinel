#include "Backend.h"
#include <QDebug>
#include <QCryptographicHash>

Backend::Backend(QObject *parent) : QObject(parent) {
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
        QString hashedPassword = QString(QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256).toHex());
        // TODO: Pass brokerId and frontAddr when base library supports it
        client_->login(username.toStdString(), hashedPassword.toStdString());
    } else {
        emit loginFailed("Not connected to server");
    }
}

void Backend::registerUser(const QString &username, const QString &password) {
    if (client_) {
        QString hashedPassword = QString(QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256).toHex());
        client_->register_user(username.toStdString(), hashedPassword.toStdString());
    } else {
        emit registerFailed("Not connected to server");
    }
}

void Backend::onMessageReceived(const nlohmann::json& j) {
    std::string type = j.value("type", "");
    
    if (type == "login_response") {
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
