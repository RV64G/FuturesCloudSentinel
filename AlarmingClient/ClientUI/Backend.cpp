#include "Backend.h"
#include "ContractData.h"
#include <QDebug>
#include <QCryptographicHash>
#include <QCoreApplication>
#include <QTime>
#include <QSettings>

Backend::Backend(QObject *parent) : QObject(parent) {
    // Load contract codes from hardcoded data
    allContractCodes_.clear();
    contractMap_.clear();
    
    for (const auto& entry : RAW_CONTRACT_DATA) {
        // 构造显示文本: "中文名 (代码)"
        QString displayText = QString("%1 (%2)").arg(entry.name, entry.code);
        allContractCodes_.append(displayText);
        
        // 存入映射表: "中文名 (代码)" -> "代码"
        // 同时也可以存入 "代码" -> "代码" 以支持直接输入代码的情况
        contractMap_[displayText.toStdString()] = entry.code;
        contractMap_[entry.code] = entry.code; 
    }
    filteredContractCodes_ = allContractCodes_;
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
        emit usernameChanged(); // Notify UI
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

void Backend::addTimeWarning(const QString &symbolText, const QString &timeStr) {
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
    client_->add_time_warning(currentUsername_.toStdString(), symbol, timeStr.toStdString());
}

void Backend::modifyPriceWarning(const QString &orderId, double maxPrice, double minPrice) {
    if (client_) {
        current_request_type_ = "modify_warning";
        client_->modify_price_warning(orderId.toStdString(), maxPrice, minPrice);
    }
}

void Backend::modifyTimeWarning(const QString &orderId, const QString &timeStr) {
    if (client_) {
        current_request_type_ = "modify_warning";
        client_->modify_time_warning(orderId.toStdString(), timeStr.toStdString());
    }
}

void Backend::deleteWarning(const QString &orderId) {
    if (client_) {
        current_request_type_ = "delete_warning";
        client_->delete_warning(orderId.toStdString());
    }
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

void Backend::queryWarnings(const QString &statusFilter) {
    if (client_) {
        current_request_type_ = "query_warnings";
        client_->query_warnings(statusFilter.toStdString());
    }
}

void Backend::setEmail(const QString &email) {
    if (client_) {
        current_request_type_ = "set_email";
        client_->set_email(email.toStdString());
    }
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
        } else if (requestType == "query_warnings") {
            if (success) {
                if (j.contains("data") && j["data"].is_array()) {
                    warningList_.clear();
                    for (const auto& item : j["data"]) {
                        QVariantMap map;
                        map.insert("order_id", QString::fromStdString(item.value("order_id", "")));
                        map.insert("symbol", QString::fromStdString(item.value("symbol", "")));
                        
                        // Fix: Check both "type" and "warning_type"
                        std::string wType = item.value("type", "");
                        if (wType.empty()) wType = item.value("warning_type", "");
                        map.insert("type", QString::fromStdString(wType));

                        // Add Contract Name
                        std::string symbol = item.value("symbol", "");
                        QString qSymbol = QString::fromStdString(symbol);
                        QString contractName = qSymbol; // Default to code
                        
                        // Reverse lookup or iterate to find name
                        // Since contractMap_ is "Name (Code)" -> "Code", we need to find the key for this value
                        // Or better, just iterate RAW_CONTRACT_DATA again or store a reverse map.
                        // For simplicity, let's iterate RAW_CONTRACT_DATA as it's small.
                        for (const auto& entry : RAW_CONTRACT_DATA) {
                            if (entry.code == symbol) {
                                contractName = QString::fromStdString(entry.name);
                                break;
                            }
                        }
                        map.insert("contract_name", contractName);

                        // map.insert("status", QString::fromStdString(item.value("status", "active"))); 
                        if (item.contains("max_price")) map.insert("max_price", item["max_price"].get<double>());
                        if (item.contains("min_price")) map.insert("min_price", item["min_price"].get<double>());
                        if (item.contains("trigger_time")) map.insert("trigger_time", QString::fromStdString(item["trigger_time"].get<std::string>()));
                        warningList_.append(QVariant(map));
                    }
                    emit warningListChanged();
                }
            } else {
                emit showMessage("Query failed: " + QString::fromStdString(message));
            }
        } else if (requestType == "set_email") {
            if (success) {
                emit showMessage("Email set successfully");
            } else {
                emit showMessage("Failed to set email: " + QString::fromStdString(message));
            }
        } else if (requestType == "modify_warning") {
            if (success) {
                emit showMessage("Warning modified successfully");
                queryWarnings(); // Refresh list
            } else {
                emit showMessage("Failed to modify warning: " + QString::fromStdString(message));
            }
        } else if (requestType == "delete_warning") {
            if (success) {
                emit showMessage("Warning deleted successfully");
                queryWarnings(); // Refresh list
            } else {
                emit showMessage("Failed to delete warning: " + QString::fromStdString(message));
            }
        } else if (requestType == "add_warning") {
            if (success) {
                emit showMessage("Warning added successfully");
                queryWarnings(); // Refresh list
            } else {
                emit showMessage("Failed to add warning: " + QString::fromStdString(message));
            }
        } else {
            if (!success || !message.empty()) {
                 emit showMessage(QString::fromStdString(message));
            }
        }
    } else if (type == "alert_triggered") {
        // 处理服务器推送的预警消息
        std::string msg = j.value("message", "Alert Triggered!");
        QString qMsg = QString::fromStdString(msg);
        
        // 尝试解析更多信息用于日志
        std::string symbol = j.value("symbol", "Unknown");
        double price = j.value("current_price", 0.0);
        
        QString logDetail = QString("[%1] %2 (Price: %3)").arg("ALERT", qMsg).arg(price);

        // 通过信号通知 UI 层显示弹窗
        emit showMessage(qMsg);
        // 记录日志
        emit logReceived(QTime::currentTime().toString("HH:mm:ss"), "ALERT", logDetail);
    } else if (type == "login_response") {
        // 兼容旧代码逻辑 (如果服务器改回去了)
        bool success = j.value("success", false);
        if (success) {
            currentUsername_ = currentUsername_; // Already set in login() call? No, we need to track it.
            // Actually, login() sets currentUsername_ before sending request? No.
            // We should update currentUsername_ here if successful, but we don't have the username in response usually.
            // So we rely on the username passed to login().
            // Let's assume login() stores it temporarily or we just trust the UI state.
            // Better: login() stores it in currentUsername_ tentatively, or we pass it in response.
            // For now, let's assume the UI handles the username display via the input field, 
            // but we need to update the property for ShellPage.
            // Let's update it in the login() method.
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

void Backend::saveCredentials(const QString &username, const QString &password, bool rememberUser, bool autoLogin) {
    QSettings settings("FuturesCloudSentinel", "AlarmingClient");
    settings.setValue("rememberUser", rememberUser);
    settings.setValue("autoLogin", autoLogin);
    
    if (rememberUser) {
        settings.setValue("username", username);
        if (autoLogin) {
            // Simple obfuscation (NOT SECURE, just to avoid plain text)
            QByteArray pwdBytes = password.toUtf8().toBase64();
            settings.setValue("password", pwdBytes);
        } else {
            settings.remove("password");
        }
    } else {
        settings.remove("username");
        settings.remove("password");
    }
}

QVariantMap Backend::loadCredentials() {
    QSettings settings("FuturesCloudSentinel", "AlarmingClient");
    QVariantMap map;
    bool rememberUser = settings.value("rememberUser", false).toBool();
    bool autoLogin = settings.value("autoLogin", false).toBool();
    
    map.insert("rememberUser", rememberUser);
    map.insert("autoLogin", autoLogin);
    
    if (rememberUser) {
        map.insert("username", settings.value("username").toString());
        if (autoLogin) {
            QByteArray pwdBytes = settings.value("password").toByteArray();
            QString password = QString::fromUtf8(QByteArray::fromBase64(pwdBytes));
            map.insert("password", password);
        }
    }
    return map;
}

void Backend::clearCredentials() {
    QSettings settings("FuturesCloudSentinel", "AlarmingClient");
    settings.remove("username");
    settings.remove("password");
    settings.setValue("rememberUser", false);
    settings.setValue("autoLogin", false);
}

void Backend::testTriggerAlert(const QString &symbol) {
#ifdef CLIENT_DEBUG_SIMULATION
    if (client_) {
        // 模拟触发一个预警
        // 使用传入的合约代码，或者默认 IF2310
        std::string code = symbol.isEmpty() ? "IF2310" : symbol.toStdString();
        
        // 构造更详细的预警消息
        QString detailMsg = QString("Alert Triggered: %1 Price >= 9999.0").arg(QString::fromStdString(code));
        
        // 模拟价格，这里随便给一个值，或者根据合约给一个合理值
        // 为了简单，我们假设价格是 9999.0，这样容易触发 ">= X" 的报警
        client_->simulate_alert_trigger(code, 9999.0, detailMsg.toStdString());
    }
#endif
}
