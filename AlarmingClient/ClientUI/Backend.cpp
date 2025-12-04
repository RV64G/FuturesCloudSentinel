#include "Backend.h"
#include "ContractData.h"
#include <QDebug>
#include <QCoreApplication>
#include <QTime>
#include <QDateTime>

Backend::Backend(QObject *parent) : QObject(parent) {
    // Initialize Managers
    authManager_ = new AuthManager(this);
    warningManager_ = new WarningManager(this);

    // Connect AuthManager signals
    connect(authManager_, &AuthManager::loginSuccess, this, &Backend::loginSuccess);
    connect(authManager_, &AuthManager::loginFailed, this, &Backend::loginFailed);
    connect(authManager_, &AuthManager::registerSuccess, this, &Backend::registerSuccess);
    connect(authManager_, &AuthManager::registerFailed, this, &Backend::registerFailed);
    connect(authManager_, &AuthManager::usernameChanged, this, &Backend::usernameChanged);
    connect(authManager_, &AuthManager::usernameChanged, [this](){
        warningManager_->setCurrentUsername(authManager_->currentUsername());
    });

    // Connect WarningManager signals
    connect(warningManager_, &WarningManager::warningListChanged, this, &Backend::warningListChanged);
    connect(warningManager_, &WarningManager::operationResult, [this](bool success, const QString& message){
        emit showMessage(message);
    });
    connect(warningManager_, &WarningManager::subscribeRequest, [this](const QStringList& symbols){
        if (quoteClient_) {
            // 使用智能更新：自动订阅新的、退订不需要的
            quoteClient_->updateSubscriptions(symbols);
        }
    });

    // Load contract codes from hardcoded data
    allContractCodes_.clear();
    contractMap_.clear();
    
    for (const auto& entry : RAW_CONTRACT_DATA) {
        // 构造显示文本: "中文名 (代码)"
        QString displayText = QString("%1 (%2)").arg(entry.name, entry.code);
        allContractCodes_.append(displayText);
        
        // 存入映射表: "中文名 (代码)" -> "代码"
        contractMap_[displayText.toStdString()] = entry.code;
        contractMap_[entry.code] = entry.code; 
    }
    filteredContractCodes_ = allContractCodes_;
    emit contractCodesChanged();

    // Load debug settings if in debug UI mode
    loadDebugSettings();

    // Initialize QuoteClient
    quoteClient_ = new QuoteClient(this);
    // 使用 Qt::QueuedConnection 确保 CTP 回调线程的信号在主线程处理
    connect(quoteClient_, &QuoteClient::priceUpdated, this, &Backend::onPriceUpdated, Qt::QueuedConnection);
    connect(quoteClient_, &QuoteClient::connectionStatusChanged, this, [this](bool connected) {
        lastCtpHeartbeat_ = QDateTime::currentMSecsSinceEpoch();
        if (ctpConnected_ != connected) {
            ctpConnected_ = connected;
            emit ctpConnectedChanged();
        }
    }, Qt::QueuedConnection);
    
    // Connect to CTP (24h demo server for quotes)
    // 注意: CTP 连接独立于 SIMULATE_SERVER，因为行情数据对测试有帮助
    // SimNow 7x24 行情前置 (与 Project2 使用相同地址)
    // 延迟 1 秒启动，等待 Qt 事件循环完全初始化
    QTimer::singleShot(1000, this, [this]() {
        qDebug() << "[Backend] Starting delayed CTP connection...";
        quoteClient_->connectToCtp("tcp://182.254.243.31:30011");
    });
    qDebug() << "[Backend] CTP connection scheduled (delayed 1s)";

    // CTP 连接看门狗定时器 (每500ms检查一次)
    ctpWatchdog_ = new QTimer(this);
    connect(ctpWatchdog_, &QTimer::timeout, this, &Backend::updateCtpConnectionStatus);
    ctpWatchdog_->start(500);

    work_guard_ = std::make_unique<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>(io_context_.get_executor());
    
    connectToServer();
}

void Backend::connectToServer() {
    if (client_) {
        client_->close();
        client_.reset();
    }

    tcp::resolver resolver(io_context_);
    try {
        auto endpoints = resolver.resolve(serverAddress_.toStdString(), std::to_string(serverPort_));
        client_ = std::make_unique<FuturesClient>(io_context_, endpoints);
        
        client_->set_message_callback([this](const nlohmann::json& j) {
            this->onMessageReceived(j);
        });

        // Update managers with new client
        authManager_->setClient(client_.get());
        warningManager_->setClient(client_.get());

        if (!io_thread_.joinable()) {
            io_thread_ = std::thread([this]() {
                io_context_.run();
            });
        }
    } catch (std::exception& e) {
        qCritical() << "Failed to initialize client:" << e.what();
    }
}

void Backend::setServerAddress(const QString &addr) {
    if (serverAddress_ != addr) {
        serverAddress_ = addr;
        emit serverAddressChanged();
        saveDebugSettings();
        // 不立即连接，等用户点击登录时再连接
    }
}

void Backend::setServerPort(int port) {
    if (serverPort_ != port) {
        serverPort_ = port;
        emit serverPortChanged();
        saveDebugSettings();
        // 不立即连接，等用户点击登录时再连接
    }
}

void Backend::loadDebugSettings() {
#ifdef DEBUG_UI
    QSettings settings("FuturesCloudSentinel", "AlarmingClient");
    serverAddress_ = settings.value("debug_server_address", "127.0.0.1").toString();
    serverPort_ = settings.value("debug_server_port", 8888).toInt();
#else
    // 非调试模式：使用生产服务器地址
    serverAddress_ = PRODUCTION_SERVER_ADDRESS;
    serverPort_ = PRODUCTION_SERVER_PORT;
#endif
}

void Backend::saveDebugSettings() {
#ifdef DEBUG_UI
    QSettings settings("FuturesCloudSentinel", "AlarmingClient");
    settings.setValue("debug_server_address", serverAddress_);
    settings.setValue("debug_server_port", serverPort_);
#endif
}

void Backend::updateCtpConnectionStatus() {
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    // 如果超过 1 秒没有收到心跳，认为断开
    bool shouldBeConnected = (now - lastCtpHeartbeat_) < 1000;
    
    if (ctpConnected_ != shouldBeConnected) {
        ctpConnected_ = shouldBeConnected;
        emit ctpConnectedChanged();
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

// --- Delegated Methods ---

void Backend::login(const QString &username, const QString &password, const QString &brokerId, const QString &frontAddr) {
    current_request_type_ = "login";
    authManager_->login(username, password);
}

void Backend::registerUser(const QString &username, const QString &password) {
    current_request_type_ = "register";
    authManager_->registerUser(username, password);
}

void Backend::saveCredentials(const QString &username, const QString &password, bool rememberUser, bool autoLogin) {
    authManager_->saveCredentials(username, password, rememberUser, autoLogin);
}

QVariantMap Backend::loadCredentials() {
    return authManager_->loadCredentials();
}

void Backend::clearCredentials() {
    authManager_->clearCredentials();
}

QString Backend::username() const {
    return authManager_->currentUsername();
}

void Backend::addPriceWarning(const QString &symbolText, double maxPrice, double minPrice) {
    current_request_type_ = "add_warning";
    std::string symbol = extractContractCode(symbolText);
    warningManager_->addPriceWarning(QString::fromStdString(symbol), maxPrice, minPrice);
}

void Backend::addTimeWarning(const QString &symbolText, const QString &timeStr) {
    current_request_type_ = "add_warning";
    std::string symbol = extractContractCode(symbolText);
    warningManager_->addTimeWarning(QString::fromStdString(symbol), timeStr);
}

void Backend::modifyPriceWarning(const QString &orderId, double maxPrice, double minPrice) {
    current_request_type_ = "modify_warning";
    warningManager_->modifyPriceWarning(orderId, maxPrice, minPrice);
}

void Backend::modifyTimeWarning(const QString &orderId, const QString &timeStr) {
    current_request_type_ = "modify_warning";
    warningManager_->modifyTimeWarning(orderId, timeStr);
}

void Backend::deleteWarning(const QString &orderId) {
    current_request_type_ = "delete_warning";
    warningManager_->deleteWarning(orderId);
}

void Backend::queryWarnings(const QString &statusFilter) {
    current_request_type_ = "query_warnings";
    warningManager_->queryWarnings(statusFilter);
}

QVariantList Backend::warningList() const {
    return warningManager_->warningList();
}

// --- Other Methods ---

void Backend::filterContractCodes(const QString &text) {
    if (text.isEmpty()) {
        filteredContractCodes_ = allContractCodes_;
    } else {
        filteredContractCodes_.clear();
        for (const QString &code : allContractCodes_) {
            if (code.contains(text, Qt::CaseInsensitive)) {
                filteredContractCodes_.append(code);
            }
        }
    }
    emit contractCodesChanged();
}

void Backend::onPriceUpdated(const QString& symbol, double price) {
    prices_[symbol] = price;
    emit pricesChanged();
}

void Backend::subscribe(const QString &symbol) {
    if (quoteClient_ && !symbol.isEmpty()) {
        quoteClient_->subscribe(QStringList() << symbol);
    }
}

void Backend::setEmail(const QString &email) {
    if (client_) {
        current_request_type_ = "set_email";
        client_->set_email(email.toStdString());
    }
    // Save locally
    QSettings settings("FuturesCloudSentinel", "AlarmingClient");
    settings.setValue("saved_email", email);
}

QString Backend::getSavedEmail() {
    QSettings settings("FuturesCloudSentinel", "AlarmingClient");
    return settings.value("saved_email", "").toString();
}

std::string Backend::extractContractCode(const QString &text) {
    std::string key = text.trimmed().toStdString();
    auto it = contractMap_.find(key);
    if (it != contractMap_.end()) {
        return it->second;
    }
    int start = text.lastIndexOf('(');
    int end = text.lastIndexOf(')');
    if (start != -1 && end != -1 && end > start) {
        return text.mid(start + 1, end - start - 1).trimmed().toStdString();
    }
    return key;
}

QString Backend::getContractName(const QString &code) {
    for (const auto& entry : RAW_CONTRACT_DATA) {
        if (entry.code == code.toStdString()) {
            return QString::fromStdString(entry.name);
        }
    }
    return code;
}

QString Backend::constructAlertMessage(const nlohmann::json& j) {
    // 按照 protocol.md: alert_triggered 包含 order_id, symbol, trigger_value, trigger_time
    std::string symbol = j.value("symbol", "");
    double triggerValue = j.value("trigger_value", 0.0);
    std::string triggerTime = j.value("trigger_time", "");
    std::string orderId = j.value("order_id", "");
    
    QString contractName = getContractName(QString::fromStdString(symbol));
    
    QString msg = QString("预警触发！\n合约: %1 (%2)\n触发值: %3\n触发时间: %4")
            .arg(contractName)
            .arg(QString::fromStdString(symbol))
            .arg(triggerValue)
            .arg(QString::fromStdString(triggerTime));
    
    return msg;
}

void Backend::onMessageReceived(const nlohmann::json& j) {
    std::string type = j.value("type", "");
    qDebug() << "[Backend] Received message type:" << QString::fromStdString(type);
    
    if (type == "response") {
        // 按照 protocol.md 解析: status (int), error_code (int)
        int status = j.value("status", 0);
        int error_code = j.value("error_code", 0);
        bool success = (status == 0 && error_code == 0);
        
        // 从 request_type 获取原始请求类型，或使用保存的 current_request_type_
        std::string requestType = j.value("request_type", current_request_type_);
        current_request_type_ = ""; 
        
        // 根据 error_code 生成友好消息
        std::string message = getErrorMessage(error_code);

        qDebug() << "[Backend] Response for:" << QString::fromStdString(requestType) 
                 << "status:" << status << "error_code:" << error_code;

        if (requestType == "login" || requestType == "register") {
            authManager_->handleResponse(requestType, success, message, j);
        } else if (requestType == "query_warnings" || requestType == "add_warning" || 
                   requestType == "modify_warning" || requestType == "delete_warning") {
            warningManager_->handleResponse(requestType, success, message, j);
        } else if (requestType == "set_email") {
            if (success) {
                emit showMessage("Email set successfully");
            } else {
                emit showMessage("Failed to set email: " + QString::fromStdString(message));
            }
        } else {
            if (!success) {
                emit showMessage(QString::fromStdString(message));
            }
        }
    } else if (type == "alert_triggered") {
        // 按照 protocol.md: alert_id, order_id, symbol, trigger_value, trigger_time
        QString qMsg = constructAlertMessage(j);
        
        std::string symbol = j.value("symbol", "Unknown");
        double triggerValue = j.value("trigger_value", 0.0);
        
        QString logDetail = QString("[ALERT] %1 (Value: %2)").arg(qMsg).arg(triggerValue);

        emit showMessage(qMsg);
        emit logReceived(QTime::currentTime().toString("HH:mm:ss"), "ALERT", logDetail);
    }
}

std::string Backend::getErrorMessage(int error_code) {
    // 根据 protocol.md 的错误码定义返回友好消息
    switch (error_code) {
        case 0:    return "操作成功";
        case 1001: return "系统繁忙，请稍后重试";
        case 1002: return "数据格式错误";
        case 1003: return "缺少必填参数";
        case 1004: return "参数值不合法";
        case 1005: return "服务器开小差了";
        case 1006: return "数据库操作失败，请稍后重试";
        case 1007: return "处理超时，请稍后重试";
        case 2001: return "用户不存在，请检查用户名";
        case 2002: return "密码错误，请重新输入";
        case 2003: return "用户名已存在，请更换用户名";
        case 2004: return "未登录或会话无效";
        case 2005: return "会话已过期，请重新登录";
        case 2006: return "账号被锁定，请联系管理员";
        case 2007: return "无权访问";
        case 3001: return "预警单不存在";
        case 3002: return "合约代码不存在或不支持";
        case 3006: return "休市期间无法操作，请在交易时间再试";
        default:   return "操作失败 (错误码: " + std::to_string(error_code) + ")";
    }
}

void Backend::testTriggerAlert(const QString &symbol) {
#ifdef SIMULATE_SERVER
    if (client_) {
        std::string code = symbol.isEmpty() ? "IF2310" : symbol.toStdString();
        QString detailMsg = QString("Alert Triggered: %1 Price >= 9999.0").arg(QString::fromStdString(code));
        client_->simulate_alert_trigger(code, 9999.0, detailMsg.toStdString());
    }
#else
    Q_UNUSED(symbol)
#endif
}
