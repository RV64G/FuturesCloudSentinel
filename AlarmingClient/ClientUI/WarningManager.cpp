#include "WarningManager.h"
#include "ContractData.h"
#include <QDebug>

WarningManager::WarningManager(QObject *parent) : QObject(parent) {}

void WarningManager::setClient(FuturesClient* client) {
    client_ = client;
}

void WarningManager::setCurrentUsername(const QString& username) {
    currentUsername_ = username;
}

void WarningManager::addPriceWarning(const QString &symbol, double maxPrice, double minPrice) {
    if (!client_) {
        emit operationResult(false, "Not connected to server");
        return;
    }
    if (currentUsername_.isEmpty()) {
        emit operationResult(false, "Please login first");
        return;
    }
    // Note: symbol here is expected to be the code, not "Name (Code)"
    // The caller (Backend) should have already extracted the code.
    client_->add_price_warning(currentUsername_.toStdString(), symbol.toStdString(), maxPrice, minPrice);
}

void WarningManager::addTimeWarning(const QString &symbol, const QString &timeStr) {
    try {
        qDebug() << "[WarningManager] addTimeWarning called with symbol:" << symbol << "time:" << timeStr;
        if (!client_) {
            emit operationResult(false, "Not connected to server");
            return;
        }
        if (currentUsername_.isEmpty()) {
            emit operationResult(false, "Please login first");
            return;
        }
        qDebug() << "[WarningManager] Calling client add_time_warning...";
        client_->add_time_warning(currentUsername_.toStdString(), symbol.toStdString(), timeStr.toStdString());
        qDebug() << "[WarningManager] add_time_warning call completed";
    } catch (const std::exception& e) {
        qCritical() << "[WarningManager] addTimeWarning exception:" << e.what();
        emit operationResult(false, QString("异常: %1").arg(e.what()));
    } catch (...) {
        qCritical() << "[WarningManager] addTimeWarning unknown exception";
        emit operationResult(false, "未知异常");
    }
}

void WarningManager::modifyPriceWarning(const QString &orderId, double maxPrice, double minPrice) {
    if (client_) {
        client_->modify_price_warning(orderId.toStdString(), maxPrice, minPrice);
    }
}

void WarningManager::modifyTimeWarning(const QString &orderId, const QString &timeStr) {
    if (client_) {
        client_->modify_time_warning(orderId.toStdString(), timeStr.toStdString());
    }
}

void WarningManager::deleteWarning(const QString &orderId) {
    if (client_) {
        client_->delete_warning(orderId.toStdString());
    }
}

void WarningManager::queryWarnings(const QString &statusFilter) {
    if (client_ && !currentUsername_.isEmpty()) {
        client_->query_warnings(currentUsername_.toStdString(), statusFilter.toStdString());
    }
}

void WarningManager::handleResponse(const std::string& type, bool success, const std::string& message, const nlohmann::json& j) {
    qDebug() << "[WarningManager] handleResponse:" << QString::fromStdString(type) << "success:" << success;
    
    if (type == "query_warnings") {
        if (success) {
            // 按照 protocol.md: data.warnings 是数组，data.total 是总数
            nlohmann::json warnings;
            if (j.contains("data")) {
                if (j["data"].contains("warnings") && j["data"]["warnings"].is_array()) {
                    warnings = j["data"]["warnings"];
                } else if (j["data"].is_array()) {
                    // 兼容旧格式
                    warnings = j["data"];
                }
            }
            
            warningList_.clear();
            QStringList symbolsToSubscribe;
            for (const auto& item : warnings) {
                QVariantMap map;
                
                // 安全解析 order_id（可能是字符串或整数）
                QString orderId;
                if (item.contains("order_id")) {
                    if (item["order_id"].is_string()) {
                        orderId = QString::fromStdString(item["order_id"].get<std::string>());
                    } else if (item["order_id"].is_number()) {
                        orderId = QString::number(item["order_id"].get<long>());
                    }
                }
                map.insert("order_id", orderId);
                map.insert("symbol", QString::fromStdString(item.value("symbol", "")));
                
                std::string wType = item.value("warning_type", "");
                if (wType.empty()) wType = item.value("type", "");
                map.insert("type", QString::fromStdString(wType));

                // Add Contract Name
                std::string symbol = item.value("symbol", "");
                QString qSymbol = QString::fromStdString(symbol);
                if (!symbolsToSubscribe.contains(qSymbol) && !qSymbol.isEmpty()) {
                    symbolsToSubscribe.append(qSymbol);
                }
                QString contractName = qSymbol; // Default to code
                
                for (const auto& entry : RAW_CONTRACT_DATA) {
                    if (entry.code == symbol) {
                        contractName = QString::fromStdString(entry.name);
                        break;
                    }
                }
                map.insert("contract_name", contractName);

                // 安全解析数值字段（可能为 null）
                if (item.contains("max_price") && !item["max_price"].is_null()) {
                    if (item["max_price"].is_number()) {
                        map.insert("max_price", item["max_price"].get<double>());
                    }
                }
                if (item.contains("min_price") && !item["min_price"].is_null()) {
                    if (item["min_price"].is_number()) {
                        map.insert("min_price", item["min_price"].get<double>());
                    }
                }
                if (item.contains("trigger_time") && !item["trigger_time"].is_null()) {
                    if (item["trigger_time"].is_string()) {
                        map.insert("trigger_time", QString::fromStdString(item["trigger_time"].get<std::string>()));
                    }
                }
                
                // 解析触发状态
                std::string status = item.value("status", "active");
                map.insert("status", QString::fromStdString(status));
                
                warningList_.append(QVariant(map));
            }
            qDebug() << "[WarningManager] Loaded" << warningList_.size() << "warnings";
            emit warningListChanged();
            // 始终发送订阅请求（即使列表为空，也需要退订不再需要的合约）
            emit subscribeRequest(symbolsToSubscribe);
        } else {
            emit operationResult(false, QString::fromStdString(message));
        }
    } else if (type == "add_warning") {
        if (success) {
            emit operationResult(true, "预警添加成功");
            queryWarnings(); 
        } else {
            emit operationResult(false, QString::fromStdString(message));
        }
    } else if (type == "modify_warning") {
        if (success) {
            emit operationResult(true, "预警修改成功");
            queryWarnings();
        } else {
            emit operationResult(false, QString::fromStdString(message));
        }
    } else if (type == "delete_warning") {
        if (success) {
            emit operationResult(true, "预警删除成功");
            queryWarnings();
        } else {
            emit operationResult(false, QString::fromStdString(message));
        }
    }
}
