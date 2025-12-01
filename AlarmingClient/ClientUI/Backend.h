#pragma once
#include <QObject>
#include <QString>
#include <QVariant>
#include <thread>
#include <memory>
#include <boost/asio.hpp>
#include <nlohmann/json.hpp>
#include "../base/FuturesClient.h"

class Backend : public QObject {
    Q_OBJECT
public:
    explicit Backend(QObject *parent = nullptr);
    ~Backend();

    Q_INVOKABLE void login(const QString &username, const QString &password, const QString &brokerId, const QString &frontAddr);
    Q_INVOKABLE void registerUser(const QString &username, const QString &password);
    
    Q_PROPERTY(QStringList contractCodes READ contractCodes NOTIFY contractCodesChanged)
    QStringList contractCodes() const { return filteredContractCodes_; }

    Q_INVOKABLE void filterContractCodes(const QString &text);

    // 预警管理接口
    Q_INVOKABLE void addPriceWarning(const QString &symbolText, double maxPrice, double minPrice);
    Q_INVOKABLE void addTimeWarning(const QString &symbolText, const QString &timeStr);
    Q_INVOKABLE void modifyPriceWarning(const QString &orderId, double maxPrice, double minPrice);
    Q_INVOKABLE void modifyTimeWarning(const QString &orderId, const QString &timeStr);
    Q_INVOKABLE void deleteWarning(const QString &orderId);

    // 测试接口
    Q_INVOKABLE void testTriggerAlert(const QString &symbol = "IF2310");

    Q_INVOKABLE void queryWarnings(const QString &statusFilter = "all");
    Q_INVOKABLE void setEmail(const QString &email);

    // Credentials Management
    Q_INVOKABLE void saveCredentials(const QString &username, const QString &password, bool rememberUser, bool autoLogin);
    Q_INVOKABLE QVariantMap loadCredentials(); // returns map with username, password, rememberUser, autoLogin
    Q_INVOKABLE void clearCredentials();

    Q_PROPERTY(QVariantList warningList READ warningList NOTIFY warningListChanged)
    Q_PROPERTY(QString username READ username NOTIFY usernameChanged)
    
    QString username() const { return currentUsername_; }
    QVariantList warningList() const { return warningList_; }

signals:
    void loginSuccess();
    void loginFailed(const QString &message);
    void registerSuccess();
    void registerFailed(const QString &message);
    void showMessage(const QString &message);
    void contractCodesChanged();
    void warningListChanged();
    void logReceived(const QString &time, const QString &level, const QString &message);
    void usernameChanged();

private:
    void onMessageReceived(const nlohmann::json& j);

    QStringList allContractCodes_;
    QStringList filteredContractCodes_;
    QVariantList warningList_;

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
