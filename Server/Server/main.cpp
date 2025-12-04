#include <jdbc/cppconn/statement.h>
#include <jdbc/cppconn/resultset.h>
#include <jdbc/cppconn/exception.h>
#include "jdbc/mysql_driver.h"
//#include<windows.h>
#include "thread_local.h"
#include "threadpool.h"
#define WIN32_LEAN_AND_MEAN

#include "MarketSeverce.h"
#include "threadpool.h"
#include "router.h"

//bool g_shouldQuit = false;
//SOCKET g_listenSocket = INVALID_SOCKET;
//ThreadPool* g_pThreadPool = nullptr;


int main() {
    ////������̨�̣߳���������
    //HANDLE hThread = CreateThread(
    //    NULL,               // Ĭ�ϰ�ȫ����
    //    0,                  // Ĭ��ջ��С
    //    MyThreadProc,       // �̺߳���ָ��
    //    NULL,               // �޲������ݣ�lpParam = NULL��
    //    0,                  // ���������߳�
    //    NULL                // ����Ҫ�߳� ID
    //);

    StartMarketService();


    // ��ʼ��Winsock
    if (!InitWinsock()) {
        return 1;
    }

    // ���ÿ���̨���� handler
    if (!SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE)) {
        std::cerr << "[���ÿ���̨����ʧ��] ������: " << GetLastError() << std::endl;
        WSACleanup();
        return 1;
    }

    // ����·��ʵ��
    RequestRouter router;

    // �����������̳߳�
    ThreadPool threadPool(&router);
    g_pThreadPool = &threadPool;
    if (!threadPool.Start()) {
        WSACleanup();
        return 1;
    }

    // ��������Socket
    g_listenSocket = CreateListenSocket();
    if (g_listenSocket == INVALID_SOCKET) {
        threadPool.Stop();
        WSACleanup();
        return 1;
    }

    // ���ܿͻ�������
    while (!g_shouldQuit) {
        sockaddr_in clientAddr;
        int clientAddrLen = sizeof(clientAddr);
        SOCKET clientSocket = accept(g_listenSocket, (sockaddr*)&clientAddr, &clientAddrLen);

        if (clientSocket == INVALID_SOCKET) {
            if (g_shouldQuit) break;
            std::cerr << "[接受连接失败] 错误码: " << WSAGetLastError() << std::endl;
            continue;
        }

        // 注意：使用阻塞模式，与 HandleClient 中的同步 recv 配合
        // 如果需要非阻塞，请修改 HandleClient 使用 select/poll 或异步 IO
        // u_long mode = 1;
        // ioctlsocket(clientSocket, FIONBIO, &mode);

        // 添加任务到线程池
        ClientContext* client = new ClientContext(clientSocket, clientAddr);
        if (!threadPool.AddTask(client)) {
            delete client;
            closesocket(clientSocket);
            std::cerr << "[��������ʧ��] �޷�����������" << std::endl;
        }
    }

    // ������Դ
    threadPool.Stop();
    if (g_listenSocket != INVALID_SOCKET) {
        closesocket(g_listenSocket);
    }
    WSACleanup();

    return 0;
}


/*
int main() {
    
    int user_id = 100;
    ThreadLocalUserID::SetUserID(100);

    std::cout<< ThreadLocalUserID::GetUserID();



    ////1���������ݿ����ӣ�������������
    //std::string sql1 = "select * from user where id = 1";//sql1���ڴ���testmysql���ݿ�
    //SetConsoleOutputCP(CP_UTF8);
    //try
    //{
    //    // ע��MySQL��������
    //    sql::mysql::MySQL_Driver* driver = sql::mysql::get_mysql_driver_instance();
    //    sql::Connection* con;
    //    sql::Statement* stmt;
    //    sql::ResultSet* res;
    //    //��ȡ���ݿ����Ӷ���
    //    con = driver->connect("tcp://localhost:3306", "root", "123456");
    //    //��ȡִ��������
    //    stmt = con->createStatement();
    //    stmt->execute(sql1);
    //    delete stmt;
    //    delete con;
    //}
    //catch (sql::SQLException& sqle)
    //{
    //    std::cout << "���ݿ����ӳ����������ǲ�����������û���д����?����������ݿ����ƻ��߱�����д����?" << std::endl;
    //}

    //2���������������ͻ�������Ϊÿ���������һ���̣߳��̳߳أ�
    
    while (1) {
        //��ѭ�������ͻ�������


        //Ϊ�������ӵĿͻ��˷���һ���̣߳������봦���ͻ���������߼�

    }



    
    if (!SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE)) {
        std::cerr << "[�����źŴ���ʧ��] ������: " << GetLastError() << std::endl;
        return 1;
    }

    if (!InitWinsock()) {
        return 1;
    }

    ThreadPool threadPool;
    g_pThreadPool = &threadPool;
    if (!threadPool.Start()) {
        CleanupWinsock();
        return 1;
    }

    g_listenSocket = CreateListenSocket();
    if (g_listenSocket == INVALID_SOCKET) {
        threadPool.Stop();
        CleanupWinsock();
        return 1;
    }

    while (!g_shouldQuit) {
        sockaddr_in clientAddr;
        int clientAddrLen = sizeof(clientAddr);

        SOCKET clientSocket = accept(g_listenSocket, (sockaddr*)&clientAddr, &clientAddrLen);
        if (clientSocket == INVALID_SOCKET) {
            if (g_shouldQuit) break;
            std::cerr << "[��������ʧ��] ������: " << WSAGetLastError() << std::endl;
            continue;
        }

        u_long mode = 0;
        ioctlsocket(clientSocket, FIONBIO, &mode);

        ClientContext* client = new ClientContext(clientSocket, clientAddr);
        if (!threadPool.AddTask(client)) {
            closesocket(clientSocket);
            delete client;
            std::cerr << "[��������ʧ��] �ͻ������ӱ��ܾ�" << std::endl;
        }
    }

    closesocket(g_listenSocket);
    threadPool.Stop();
    CleanupWinsock();

    std::cout << "[�����������˳�]" << std::endl;
    return 0;
}
*/