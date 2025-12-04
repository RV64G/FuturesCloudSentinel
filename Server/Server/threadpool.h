#pragma once
#ifndef THREADPOOL_H
#define WIN32_LEAN_AND_MEAN
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <iostream>
#include <vector>
#include <queue>
#include <string>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "base.h"
#include "handler.h"
#include "router.h"
#include "thread_local.h"

#include <thread>
#include <memory>
#include <atomic>

#pragma comment(lib, "ws2_32.lib")

extern bool g_shouldQuit;
// 线程池配置
const int THREAD_POOL_SIZE = 4;
const int MAX_CLIENT_COUNT = 100;
const int LISTEN_PORT = 8888;

// 前置声明
class RequestRouter;
/*
// 客户端上下文结构
struct ClientContext {
    SOCKET clientSocket;
    sockaddr_in clientAddr;
    ChatMessage readMsg;
    ChatMessage writeMsg;

    ClientContext(SOCKET sock, const sockaddr_in& addr)
        : clientSocket(sock), clientAddr(addr) {
    }
};
*/
// 线程池类
class ThreadPool {
private:
    std::vector<HANDLE> workerThreads;
    std::queue<ClientContext*> taskQueue;
    CRITICAL_SECTION cs;
    HANDLE hTaskEvent;
    bool isRunning;
    int clientCount;
    RequestRouter* router ;  // 路由实例指针

    static DWORD WINAPI WorkerThreadProc(LPVOID lpParam) {
        ThreadPool* pPool = static_cast<ThreadPool*>(lpParam);
        pPool->ProcessTasks();
        return 0;
    }

    void ProcessTasks() {
        while (isRunning && !g_shouldQuit) {
            DWORD waitResult = WaitForSingleObject(hTaskEvent, 100);
            if (waitResult == WAIT_OBJECT_0) {
                while (isRunning && !g_shouldQuit) {
                    ClientContext* client = nullptr;
                    EnterCriticalSection(&cs);
                    if (!taskQueue.empty()) {
                        client = taskQueue.front();
                        taskQueue.pop();
                    }
                    else {
                        ResetEvent(hTaskEvent);
                    }
                    LeaveCriticalSection(&cs);

                    if (client == nullptr) break;

                    HandleClient(client);
                    delete client;

                    EnterCriticalSection(&cs);
                    clientCount--;
                    LeaveCriticalSection(&cs);
                }
            }
            else if (waitResult == WAIT_TIMEOUT) {
                continue;
            }
            else {
                break;
            }
        }
    }

    // 处理客户端连接
    void HandleClient(ClientContext* client) {
        char clientIP[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client->clientAddr.sin_addr), clientIP, INET_ADDRSTRLEN);
        uint16_t clientPort = ntohs(client->clientAddr.sin_port);

        std::cout << "[客户端连接] IP: " << clientIP << ", 端口: " << clientPort << std::endl;
		ThreadLocalUser::SetClient(client);
        while (isRunning && !g_shouldQuit) {
            // 读取消息头
            int bytesRead = recv(client->clientSocket,
                client->readMsg.data(),
                client->readMsg.header_length,
                0);

            if (bytesRead <= 0) {
                HandleSocketError(clientIP, clientPort, bytesRead);
                break;
            }

            // 解析消息头获取消息体长度
            if (!client->readMsg.decode_header()) {
                std::cerr << "[协议错误] 无效的消息头" << std::endl;
                break;
            }

            // 读取消息体
            bytesRead = recv(client->clientSocket,
                client->readMsg.body(),
                client->readMsg.body_length(),
                0);

            if (bytesRead <= 0) {
                HandleSocketError(clientIP, clientPort, bytesRead);
                break;
            }

            // 处理请求
            std::string requestData(client->readMsg.body(), client->readMsg.body_length());
            std::cout << "[接收数据] " << clientIP << ":" << clientPort << ": " << requestData << std::endl;

            // 路由请求并获取响应
			
            std::string responseData = router->RouteRequest(requestData);

            // 发送响应
            if (!SendResponse(client, responseData)) {
                std::cerr << "[发送失败] " << clientIP << ":" << clientPort << std::endl;
                break;
            }
        }
        FuturesAlertServer::stopUserWatcher(ThreadLocalUser::GetUserToken() );
        closesocket(client->clientSocket);

        std::cout << "[连接关闭] IP: " << clientIP << ", 端口: " << clientPort << std::endl;
    }

    // 处理Socket错误
    void HandleSocketError(const std::string& ip, uint16_t port, int bytesRead) {
        if (bytesRead == 0) {
            std::cout << "[客户端断开] IP: " << ip << ", 端口: " << port << std::endl;
        }
        else {
            int err = WSAGetLastError();
            if (err != WSAEWOULDBLOCK) {
                std::cerr << "[接收错误] 客户端 " << ip << ":" << port
                    << " 错误码: " << err << std::endl;
            }
        }
    }

    // 发送响应给客户端
    bool SendResponse(ClientContext* client, const std::string& responseData) {
        // 检查响应长度
        if (responseData.size() > client->writeMsg.max_body_length) {
            std::cerr << "[响应过长] 超过最大长度: " << client->writeMsg.max_body_length << std::endl;
            return false;
        }

        // 填充响应数据
        client->writeMsg.body_length(responseData.size());
        std::memcpy(client->writeMsg.body(), responseData.data(), responseData.size());
        client->writeMsg.encode_header();

        // 发送数据
        int totalSent = 0;
        int dataLength = client->writeMsg.length();
        const char* data = client->writeMsg.data();

        while (totalSent < dataLength) {
            int sent = send(client->clientSocket, data + totalSent, dataLength - totalSent, 0);
            if (sent == SOCKET_ERROR) {
                return false;
            }
            totalSent += sent;
        }

        return true;
    }

public:
    ThreadPool(RequestRouter* routerInstance)
        : isRunning(false), clientCount(0), router(routerInstance) {
        InitializeCriticalSection(&cs);
        hTaskEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    }

    ~ThreadPool() {
        Stop();
        DeleteCriticalSection(&cs);
        CloseHandle(hTaskEvent);
    }

    bool Start() {
        if (isRunning) return true;
        isRunning = true;
        clientCount = 0;

        for (int i = 0; i < THREAD_POOL_SIZE; ++i) {
            HANDLE hThread = CreateThread(
                NULL,
                0,
                WorkerThreadProc,
                this,
                0,
                NULL
            );
            if (hThread == NULL) {
                std::cerr << "[线程创建失败] 错误码: " << GetLastError() << std::endl;
                Stop();
                return false;
            }
            workerThreads.push_back(hThread);
        }

        std::cout << "[线程池启动成功] 线程数量: " << THREAD_POOL_SIZE << std::endl;
        return true;
    }

    void Stop() {
        if (!isRunning) return;
        isRunning = false;

        SetEvent(hTaskEvent);

        WaitForMultipleObjects(
            static_cast<DWORD>(workerThreads.size()),
            workerThreads.data(),
            TRUE,
            INFINITE
        );

        for (HANDLE hThread : workerThreads) {
            CloseHandle(hThread);
        }
        workerThreads.clear();

        EnterCriticalSection(&cs);
        while (!taskQueue.empty()) {
            ClientContext* client = taskQueue.front();
            taskQueue.pop();
            closesocket(client->clientSocket);
            delete client;
        }
        LeaveCriticalSection(&cs);

        std::cout << "[线程池已停止]" << std::endl;
    }

    bool AddTask(ClientContext* client) {
        if (!isRunning || client == nullptr || g_shouldQuit) return false;

        EnterCriticalSection(&cs);
        if (clientCount >= MAX_CLIENT_COUNT) {
            LeaveCriticalSection(&cs);
            std::cerr << "[连接已满] 客户端数量已达上限 (" << MAX_CLIENT_COUNT << ")" << std::endl;
            return false;
        }
        taskQueue.push(client);
        clientCount++;
        SetEvent(hTaskEvent);
        LeaveCriticalSection(&cs);

        return true;
    }
};

// 全局变量

extern SOCKET g_listenSocket ;
extern ThreadPool* g_pThreadPool;



// 将原来头文件中的函数实现移到 threadpool.cpp 中以避免重复定义。
// 在头文件中只声明这些全局函数：
BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType);
bool InitWinsock();
SOCKET CreateListenSocket();
/*
// 控制台控制处理函数
BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType) {
    if (dwCtrlType == CTRL_C_EVENT) {
        std::cout << "\n[收到退出信号] 正在释放资源..." << std::endl;
        g_shouldQuit = true;

        if (g_listenSocket != INVALID_SOCKET) {
            closesocket(g_listenSocket);
            g_listenSocket = INVALID_SOCKET;
        }

        if (g_pThreadPool != nullptr) {
            g_pThreadPool->Stop();
        }

        WSACleanup();
        return TRUE;
    }
    return FALSE;
}

// 初始化Winsock
bool InitWinsock() {
    WSADATA wsaData;
    int ret = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (ret != 0) {
        std::cerr << "[Winsock初始化失败] 错误码: " << ret << std::endl;
        return false;
    }
    if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
        std::cerr << "[Winsock版本不支持] 需要 2.2 版本" << std::endl;
        WSACleanup();
        return false;
    }
    std::cout << "[Winsock初始化成功] 版本: 2.2" << std::endl;
    return true;
}

// 创建监听Socket
SOCKET CreateListenSocket() {
    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET) {
        std::cerr << "[Socket创建失败] 错误码: " << WSAGetLastError() << std::endl;
        return INVALID_SOCKET;
    }

    int reuse = 1;
    if (setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) == SOCKET_ERROR) {
        std::cerr << "[设置端口复用失败] 错误码: " << WSAGetLastError() << std::endl;
        closesocket(listenSocket);
        return INVALID_SOCKET;
    }

    sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(LISTEN_PORT);

    if (bind(listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "[端口绑定失败] 错误码: " << WSAGetLastError() << std::endl;
        closesocket(listenSocket);
        return INVALID_SOCKET;
    }

    if (listen(listenSocket, 5) == SOCKET_ERROR) {
        std::cerr << "[监听失败] 错误码: " << WSAGetLastError() << std::endl;
        closesocket(listenSocket);
        return INVALID_SOCKET;
    }

    std::cout << "[监听启动成功] 端口: " << LISTEN_PORT << " (等待客户端连接...)" << std::endl;
    return listenSocket;
}
*/
#endif
