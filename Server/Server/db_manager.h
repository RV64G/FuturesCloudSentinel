#pragma once
#ifndef DB_MANAGER_H
#define DB_MANAGER_H
#define WIN32_LEAN_AND_MEAN
#include <string>
#include <mutex>
// MySQL Connector/C++ header
#include <mysql/jdbc.h>
using namespace sql;

// ���ݿ����ӹ����ࣨ����ģʽ��
class DBManager {
private:
    // ����ʵ������̬��
    static DBManager* instance;
    // �̰߳�ȫ������֤������ʼ����ȫ��
    static std::mutex instance_mutex;

    // ���ݿ����ã�˽�У����ڲ�ʹ�ã�
    std::string db_host;
    std::string db_name;
    std::string db_user;
    std::string db_pass;
    int db_timeout;

    // MySQL ������ȫ��Ψһ��
    mysql::MySQL_Driver* driver;

    // ˽�й��캯������ֹ�ⲿʵ������
    DBManager();
    // ˽��������������ֹ�ⲿ���٣�
    ~DBManager();
    // ��ֹ��������͸�ֵ
    DBManager(const DBManager&) = delete;
    DBManager& operator=(const DBManager&) = delete;

public:
    // ȫ�ֻ�ȡ����ʵ��
    static DBManager* GetInstance();

    // ��ȡ���ݿ����ӣ����ض������ӣ����������ֶ��ͷţ�
    Connection* GetConnection();

    // �ͷ����ݿ����ӣ���ѡ����ʹ�����ӳأ��˴���Ϊ�黹���ӣ�
    void ReleaseConnection(Connection* conn);

    // ���������Ƿ���Ч
    bool IsConnectionValid(Connection* conn);
};

#endif // DB_MANAGER_H