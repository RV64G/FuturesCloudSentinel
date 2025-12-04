#include <iostream>
#include <string>
#include "db_manager.h" // 包含连接管理类头文件
#include <jdbc/cppconn/prepared_statement.h>
#include <jdbc/cppconn/resultset.h>

#define WIN32_LEAN_AND_MEAN
using namespace std;
using namespace sql;

// 示例1：用户注册（插入数据）
bool UserRegister(const string& username, const string& password, const string& email) {
    // 1. 获取连接管理器实例
    DBManager* db_manager = DBManager::GetInstance();
    if (db_manager == nullptr) {
        cerr << "获取数据库管理器失败" << endl;
        return false;
    }

    // 2. 获取独立连接
    Connection* conn = db_manager->GetConnection();
    if (conn == nullptr) {
        cerr << "获取数据库连接失败" << endl;
        return false;
    }

    PreparedStatement* pstmt = nullptr;
    try {
        // 3. 执行插入SQL（预处理语句，防注入）
        const string insert_sql = "INSERT INTO `user` (username, password, email) VALUES (?, ?, ?)";
        pstmt = conn->prepareStatement(insert_sql);
        pstmt->setString(1, username);
        pstmt->setString(2, password); // 实际项目中需加密（如bcrypt）
        pstmt->setString(3, email);

        int affected_rows = pstmt->executeUpdate();
        if (affected_rows > 0) {
            cout << "用户注册成功：" << username << endl;
            return true;
        }
        else {
            cout << "用户注册失败：无数据插入" << endl;
            return false;
        }
    }
    catch (const sql::SQLException& e) {
        cerr << "注册SQL异常：" << e.what()
            << " (错误码：" << e.getErrorCode() << ")" << endl;
        return false;
    }
    catch (const exception& e) {
        cerr << "注册失败：" << e.what() << endl;
        return false;
    }
    //finally{
    //    // 4. 释放资源（按顺序：结果集→语句→连接）
    //    if (pstmt != nullptr) delete pstmt;
    //    db_manager->ReleaseConnection(conn); // 归还/释放连接
    //}
}

// 示例2：根据用户名查询用户（查询数据）
bool QueryUserByUsername(const string& username) {
    DBManager* db_manager = DBManager::GetInstance();
    Connection* conn = db_manager->GetConnection();
    if (conn == nullptr) return false;

    PreparedStatement* pstmt = nullptr;
    ResultSet* res = nullptr;
    try {
        const string select_sql = "SELECT id, username, email, create_time FROM `user` WHERE username = ?";
        pstmt = conn->prepareStatement(select_sql);
        pstmt->setString(1, username);
        res = pstmt->executeQuery();

        if (res->next()) {
            cout << "\n查询到用户：" << endl;
            cout << "ID: " << res->getInt("id") << endl;
            cout << "用户名: " << res->getString("username") << endl;
            cout << "邮箱: " << res->getString("email") << endl;
            cout << "创建时间: " << res->getString("create_time") << endl;
            return true;
        }
        else {
            cout << "未查询到用户：" << username << endl;
            return false;
        }
    }
    catch (const sql::SQLException& e) {
        cerr << "查询SQL异常：" << e.what() << endl;
        return false;
    }
   /* finally {
        if (res != nullptr) delete res;
        if (pstmt != nullptr) delete pstmt;
        db_manager->ReleaseConnection(conn);
    }*/
}
