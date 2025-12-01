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
    
    Q_PROPERTY(QStringList contractCodes READ contractCodes NOTIFY contractCodesChanged)
    QStringList contractCodes() const { return contractCodes_; }

    // 预警管理接口
    Q_INVOKABLE void addPriceWarning(const QString &symbolText, double maxPrice, double minPrice);

signals:
    void loginSuccess();
    void loginFailed(const QString &message);
    void registerSuccess();
    void registerFailed(const QString &message);
    void showMessage(const QString &message);
    void contractCodesChanged();

private:
    void onMessageReceived(const nlohmann::json& j);

    QStringList contractCodes_;

    boost::asio::io_context io_context_;
    std::unique_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> work_guard_;
    std::unique_ptr<FuturesClient> client_;
    std::thread io_thread_;

    // 记录当前正在进行的请求类型 (假设同一时间只有一个请求)
    std::string current_request_type_;
    QString currentUsername_; // 记录当前登录用户，用于后续请求

    // 辅助函数：从 "中文名 (代码)" 中提取 "代码"
    std::string extractContractCode(const QString &text);

    // 存储 "中文名 (代码)" -> "代码" 的映射
    std::map<std::string, std::string> contractMap_;
};
