#pragma once
#include <QObject>
#include <QString>
#include <thread>
#include <memory>
#include <boost/asio.hpp>
#include <nlohmann/json.hpp>
#include "../../base/FuturesClient.h"

class Backend : public QObject {
    Q_OBJECT
public:
    explicit Backend(QObject *parent = nullptr);
    ~Backend();

    Q_INVOKABLE void login(const QString &username, const QString &password, const QString &brokerId, const QString &frontAddr);
    Q_INVOKABLE void registerUser(const QString &username, const QString &password);

signals:
    void loginSuccess();
    void loginFailed(const QString &message);
    void registerSuccess();
    void registerFailed(const QString &message);
    void showMessage(const QString &message);

private:
    void onMessageReceived(const nlohmann::json& j);

    boost::asio::io_context io_context_;
    std::unique_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> work_guard_;
    std::unique_ptr<FuturesClient> client_;
    std::thread io_thread_;
};
