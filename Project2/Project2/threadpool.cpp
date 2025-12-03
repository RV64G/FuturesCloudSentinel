#include "threadpool.h"
bool g_shouldQuit = false;
SOCKET g_listenSocket = INVALID_SOCKET;
ThreadPool* g_pThreadPool = nullptr;


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
/*
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

    // 修改这里的 listenIp：
    // - "0.0.0.0" 或 INADDR_ANY: 监听所有网卡
    // - "127.0.0.1": 仅本地回环
    // - "192.168.1.100": 仅该网卡
    const char* listenIp = "127.0.0.1";

    sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(LISTEN_PORT);

    if (strcmp(listenIp, "0.0.0.0") == 0) {
        serverAddr.sin_addr.s_addr = INADDR_ANY; // 监听所有接口
    }
    else {
        // 使用 inet_pton 将字符串地址转换为二进制 IPv4 地址
        int rc = inet_pton(AF_INET, listenIp, &serverAddr.sin_addr);
        if (rc != 1) {
            std::cerr << "[解析监听IP失败] listenIp=" << listenIp << std::endl;
            closesocket(listenSocket);
            return INVALID_SOCKET;
        }
    }

    /*if (bind(listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "[端口绑定失败] 错误码: " << WSAGetLastError() << std::endl;
        closesocket(listenSocket);
        return INVALID_SOCKET;
    }*/

    if (listen(listenSocket, 5) == SOCKET_ERROR) {
        std::cerr << "[监听失败] 错误码: " << WSAGetLastError() << std::endl;
        closesocket(listenSocket);
        return INVALID_SOCKET;
    }

    std::cout << "[监听启动成功] IP: " << listenIp << " 端口: " << LISTEN_PORT << " (等待客户端连接...)" << std::endl;
    return listenSocket;
}