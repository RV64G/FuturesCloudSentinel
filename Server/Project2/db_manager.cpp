#include "db_manager.h"
#include <iostream>
#include <string>
// MySQL Connector/C++ 头文件
#include "jdbc/mysql_driver.h"
#include <jdbc/cppconn/statement.h>

#define WIN32_LEAN_AND_MEAN
using namespace std;
using namespace sql;

// 初始化静态成员
DBManager* DBManager::instance = nullptr;
std::mutex DBManager::instance_mutex;

// 私有构造函数：初始化驱动和配置
DBManager::DBManager() {
    // 1. 数据库配置（可改为从配置文件读取，此处硬编码示例）
    db_host = "tcp://127.0.0.1:3306";
    db_name = "cpptestmysql";
    db_user = "root";
    db_pass = "123456";
    db_timeout = 30;

    // 2. 初始化 MySQL 驱动（全局唯一）
    try {
        driver = mysql::get_mysql_driver_instance();
        if (driver == nullptr) {
            throw runtime_error("获取 MySQL 驱动失败");
        }
        cout << "[DBManager] MySQL 驱动初始化成功" << endl;
    }
    catch (const sql::SQLException& e) {
        cerr << "[DBManager] 驱动初始化异常：" << e.what() << endl;
        throw; // 向上抛出，确保初始化失败时程序退出
    }
}

// 析构函数：释放驱动（驱动由库管理，无需手动删除，此处仅做清理标记）
DBManager::~DBManager() {
    cout << "[DBManager] 连接管理器已销毁" << endl;
}

// 全局获取单例实例（线程安全）
DBManager* DBManager::GetInstance() {
    if (instance == nullptr) { // 双重检查锁定（DCLP），提高效率
        std::lock_guard<std::mutex> lock(instance_mutex); // 加锁
        if (instance == nullptr) {
            instance = new DBManager();
        }
    }
    return instance;
}

// 获取数据库连接（返回独立连接，供单个线程使用）
Connection* DBManager::GetConnection() {
    try {
        if (driver == nullptr) {
            throw runtime_error("MySQL 驱动未初始化");
        }

        // 创建新连接（每个线程调用时返回独立连接）
        Connection* conn = driver->connect(db_host, db_user, db_pass);
        if (conn == nullptr) {
            throw runtime_error("创建数据库连接失败");
        }

        // 配置连接
        conn->setSchema(db_name); // 选择数据库
        conn->setClientOption("connectTimeout", to_string(db_timeout)); // 超时
        conn->setClientOption("charset", "utf8mb4"); // 编码

        // 测试连接有效性
        if (!IsConnectionValid(conn)) {
            delete conn;
            throw runtime_error("连接创建成功但无效");
        }

        cout << "[DBManager] 数据库连接获取成功" << endl;
        return conn;
    }
    catch (const sql::SQLException& e) {
        cerr << "[DBManager] 获取连接异常：" << e.what()
            << " (错误码：" << e.getErrorCode() << ")" << endl;
        return nullptr;
    }
    catch (const runtime_error& e) {
        cerr << "[DBManager] 获取连接失败：" << e.what() << endl;
        return nullptr;
    }
}

// 释放数据库连接（调用者使用完后调用）
void DBManager::ReleaseConnection(Connection* conn) {
    if (conn != nullptr) {
        try {
            conn->close(); // 关闭连接
            delete conn;   // 释放内存
            cout << "[DBManager] 数据库连接已释放" << endl;
        }
        catch (const sql::SQLException& e) {
            cerr << "[DBManager] 释放连接异常：" << e.what() << endl;
        }
    }
}

// 测试连接是否有效（执行简单SQL验证）
bool DBManager::IsConnectionValid(Connection* conn) {
    if (conn == nullptr) return false;
    try {
        Statement* stmt = conn->createStatement();
        stmt->executeQuery("SELECT 1"); // 执行简单查询
        delete stmt;
        return true;
    }
    catch (const sql::SQLException& e) {
        cerr << "[DBManager] 连接无效：" << e.what() << endl;
        return false;
    }
}