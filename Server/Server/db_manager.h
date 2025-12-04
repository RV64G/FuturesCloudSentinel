#pragma once
#ifndef DB_MANAGER_H
#define DB_MANAGER_H
#define WIN32_LEAN_AND_MEAN
#include <string>
#include <mutex>
// MySQL Connector/C++ 头文件
#include <jdbc/cppconn/connection.h>
#include <jdbc/cppconn/exception.h>
#include "jdbc/mysql_driver.h"
using namespace sql;

// 数据库连接管理类（单例模式）
class DBManager {
private:
    // 单例实例（静态）
    static DBManager* instance;
    // 线程安全锁（保证单例初始化安全）
    static std::mutex instance_mutex;

    // 数据库配置（私有，仅内部使用）
    std::string db_host;
    std::string db_name;
    std::string db_user;
    std::string db_pass;
    int db_timeout;

    // MySQL 驱动（全局唯一）
    mysql::MySQL_Driver* driver;

    // 私有构造函数（禁止外部实例化）
    DBManager();
    // 私有析构函数（禁止外部销毁）
    ~DBManager();
    // 禁止拷贝构造和赋值
    DBManager(const DBManager&) = delete;
    DBManager& operator=(const DBManager&) = delete;

public:
    // 全局获取单例实例
    static DBManager* GetInstance();

    // 获取数据库连接（返回独立连接，调用者需手动释放）
    Connection* GetConnection();

    // 释放数据库连接（可选：若使用连接池，此处改为归还连接）
    void ReleaseConnection(Connection* conn);

    // 测试连接是否有效
    bool IsConnectionValid(Connection* conn);
};

#endif // DB_MANAGER_H