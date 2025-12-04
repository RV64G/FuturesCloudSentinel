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
    ////开启后台线程，监听行情
    //HANDLE hThread = CreateThread(
    //    NULL,               // 默认安全属性
    //    0,                  // 默认栈大小
    //    MyThreadProc,       // 线程函数指针
    //    NULL,               // 无参数传递（lpParam = NULL）
    //    0,                  // 立即启动线程
    //    NULL                // 不需要线程 ID
    //);

    StartMarketService();


    // 初始化Winsock
    if (!InitWinsock()) {
        return 1;
    }

    // 设置控制台控制 handler
    if (!SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE)) {
        std::cerr << "[设置控制台控制失败] 错误码: " << GetLastError() << std::endl;
        WSACleanup();
        return 1;
    }

    // 创建路由实例
    RequestRouter router;

    // 创建并启动线程池
    ThreadPool threadPool(&router);
    g_pThreadPool = &threadPool;
    if (!threadPool.Start()) {
        WSACleanup();
        return 1;
    }

    // 创建监听Socket
    g_listenSocket = CreateListenSocket();
    if (g_listenSocket == INVALID_SOCKET) {
        threadPool.Stop();
        WSACleanup();
        return 1;
    }

    // 接受客户端连接
    while (!g_shouldQuit) {
        sockaddr_in clientAddr;
        int clientAddrLen = sizeof(clientAddr);
        SOCKET clientSocket = accept(g_listenSocket, (sockaddr*)&clientAddr, &clientAddrLen);

        if (clientSocket == INVALID_SOCKET) {
            if (g_shouldQuit) break;
            std::cerr << "[接受连接失败] 错误码: " << WSAGetLastError() << std::endl;
            continue;
        }

        // 设置非阻塞模式
        u_long mode = 1;
        ioctlsocket(clientSocket, FIONBIO, &mode);

        // 添加任务到线程池
        ClientContext* client = new ClientContext(clientSocket, clientAddr);
        if (!threadPool.AddTask(client)) {
            delete client;
            closesocket(clientSocket);
            std::cerr << "[添加任务失败] 无法处理新连接" << std::endl;
        }
    }

    // 清理资源
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



    ////1、建立数据库连接，并创建服务器
    //std::string sql1 = "select * from user where id = 1";//sql1用于创建testmysql数据库
    //SetConsoleOutputCP(CP_UTF8);
    //try
    //{
    //    // 注册MySQL驱动程序
    //    sql::mysql::MySQL_Driver* driver = sql::mysql::get_mysql_driver_instance();
    //    sql::Connection* con;
    //    sql::Statement* stmt;
    //    sql::ResultSet* res;
    //    //获取数据库连接对象
    //    con = driver->connect("tcp://localhost:3306", "root", "123456");
    //    //获取执行语句对象
    //    stmt = con->createStatement();
    //    stmt->execute(sql1);
    //    delete stmt;
    //    delete con;
    //}
    //catch (sql::SQLException& sqle)
    //{
    //    std::cout << "数据库连接出错啦，你是不是密码或者用户名写错了?或者你的数据库名称或者表名称写错了?" << std::endl;
    //}

    //2、创建服务器，客户端请求，为每个请求分配一个线程（线程池）
    
    while (1) {
        //死循环监听客户端请求，


        //为建立连接的客户端分配一个线程，并进入处理客户端请求的逻辑

    }



    
    if (!SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE)) {
        std::cerr << "[设置信号处理失败] 错误码: " << GetLastError() << std::endl;
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
            std::cerr << "[接受连接失败] 错误码: " << WSAGetLastError() << std::endl;
            continue;
        }

        u_long mode = 0;
        ioctlsocket(clientSocket, FIONBIO, &mode);

        ClientContext* client = new ClientContext(clientSocket, clientAddr);
        if (!threadPool.AddTask(client)) {
            closesocket(clientSocket);
            delete client;
            std::cerr << "[任务添加失败] 客户端连接被拒绝" << std::endl;
        }
    }

    closesocket(g_listenSocket);
    threadPool.Stop();
    CleanupWinsock();

    std::cout << "[服务器正常退出]" << std::endl;
    return 0;
}
*/