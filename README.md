# 期货云端预警系统客户端

这是一个基于 C++17 和 Boost.Asio 开发的高性能 TCP 客户端，专为期货云端预警系统设计。它实现了完整的通信协议，支持异步消息收发、JSON 数据解析、业务逻辑封装以及 UI 回调集成。

## 📋 功能特性

*   **高性能网络通信**: 基于 `Boost.Asio` 的异步 TCP 模型，确保 UI 线程不卡顿。
*   **标准通信协议**: 实现了基于 Length-Prefix (4字节头) + JSON Body 的应用层协议。
*   **完整业务逻辑**:
    *   用户注册与登录
    *   设置接收邮箱
    *   **价格预警**: 支持设置价格上限和下限。
    *   **时间预警**: 支持设置特定时间触发。
    *   预警单的增删改查 (CRUD)。
*   **UI 集成友好**:
    *   提供 `std::function` 回调接口，轻松对接 Qt, MFC, ImGui 等 UI 框架。
    *   业务逻辑与网络层解耦。
*   **调试模式 (Simulation Mode)**:
    *   内置模拟器，无需连接真实服务器即可测试 UI 交互和业务流程。
    *   通过宏定义一键切换。
*   **跨平台构建**: 使用 CMake (vcpkg) 或 VS Code Tasks (MSVC) 进行构建。

## 📂 项目结构

```text
client/
├── base/
│   ├── FuturesClient.h      # [核心] 客户端类声明、协议定义、回调接口
│   ├── FuturesClient.cpp    # [核心] 客户端实现、网络逻辑、模拟器逻辑
│   ├── main.cpp             # [入口] 示例程序，展示如何调用接口和处理回调
│   └── .vscode/             # VS Code 构建与调试配置
│       ├── tasks.json       # MSVC 编译任务
│       └── launch.json      # 调试启动配置
├── vcpkg/                   # 依赖包管理 (Boost, nlohmann-json)
├── protocol.md              # 通信协议规范文档
└── README.md                # 项目说明文档
```

## 🛠️ 环境依赖

*   **操作系统**: Windows 10/11 (推荐), Linux/macOS (需调整构建脚本)
*   **编译器**: 支持 C++17 的编译器 (MSVC v143+, GCC 9+, Clang 10+)
    *   本项目默认配置为 **MSVC (Visual Studio 2022)**。
*   **依赖库**:
    *   `boost` (system, asio, thread)
    *   `nlohmann-json`
    *   *依赖库已通过 vcpkg 管理*

## 🚀 快速开始 (VS Code)

本项目已配置好 VS Code 的开发环境，开箱即用。

### 1. 准备工作
确保已安装：
1.  **Visual Studio 2022 Community** (勾选 "使用 C++ 的桌面开发")。
2.  **Visual Studio Code** (安装 C/C++ 插件)。
3.  项目目录下的 `vcpkg` 已经 bootstrap 并安装了相关包 (如果尚未安装，请参考 vcpkg 文档)。
    *   *注意：本项目配置假设 vcpkg 位于项目根目录的 `vcpkg/` 文件夹下，且已安装 `x64-windows` triplet 的包。*

### 2. 编译与运行
1.  用 VS Code 打开 `client` 文件夹。
2.  打开 `base/main.cpp`。
3.  按下 **`F5`** 键。
    *   VS Code 会自动调用 `cl.exe` 进行编译。
    *   编译成功后会自动启动调试终端运行程序。

### 3. 切换调试/真实模式
默认情况下，客户端处于 **调试模拟模式 (Simulation Mode)**，不会尝试连接真实服务器。

*   **关闭调试模式 (连接服务器)**:
    打开 `base/FuturesClient.h`，注释掉第一行：
    ```cpp
    // #define CLIENT_DEBUG_SIMULATION  <-- 注释掉此行
    ```
    再次按下 `F5`，客户端将尝试连接 `127.0.0.1:8888`。

## 📖 使用指南

### 1. 初始化客户端
```cpp
boost::asio::io_context io_context;
tcp::resolver resolver(io_context);
auto endpoints = resolver.resolve("127.0.0.1", "8888");

// 实例化客户端
FuturesClient client(io_context, endpoints);
```

### 2. 注册 UI 回调
在 UI 初始化代码中设置回调，处理服务器推送的消息：
```cpp
client.set_message_callback([](const json& j) {
    std::string type = j.value("type", "unknown");
    
    if (type == "alert_triggered") {
        // TODO: 弹出报警窗口
        std::cout << "收到报警: " << j["message"] << std::endl;
    } 
    else if (type == "response") {
        // TODO: 更新操作状态
        bool success = j.value("success", false);
    }
});
```

### 3. 调用业务接口
```cpp
// 登录
client.login("user1", "password123");

// 添加价格预警 (账号, 合约, 上限, 下限)
client.add_price_warning("88001", "rb2310", 4000.0, 3500.0);

// 添加时间预警
client.add_time_warning("88001", "rb2310", "2023-12-01 14:55:00");

// 查询所有预警
client.query_warnings("all");
```

### 4. 运行网络循环
网络 IO 需要在 `io_context` 上运行。建议在单独的线程中运行它，以免阻塞 UI 主线程：
```cpp
std::thread t([&io_context]() { 
    io_context.run(); 
});

// ... UI 主循环 ...

t.join();
```

## 📡 协议摘要

通信采用 **Length-Prefix + JSON** 格式。
*   **Header**: 4字节 ASCII 数字，表示 Body 长度 (例如 " 120")。
*   **Body**: JSON 字符串。

**示例报文**:
```
 105{"type":"login","request_id":"req_1","username":"test","password":"123"}
```

详细协议定义请参考 [protocol.md](./protocol.md)。

## ⚠️ 常见问题

1.  **编译错误: 找不到头文件**
    *   请检查 `.vscode/tasks.json` 中的 `/I` 和 `/LIBPATH` 路径是否指向了正确的 vcpkg 安装目录。
    *   确保已运行 `vcpkg install boost:x64-windows nlohmann-json:x64-windows`。

2.  **链接错误**
    *   确保使用了 `x64 Native Tools Command Prompt` 或者在 `tasks.json` 中正确调用了 `vcvars64.bat`。

3.  **中文乱码**
    *   编译参数已添加 `/utf-8`，请确保源文件保存为 UTF-8 编码。


# FuturesAlarmingClient
WebStock Futures Alarming Client

## 架构设计 (Architecture)

本项目采用 **Qt 6** + **QML** 开发，遵循 **Model-View** 架构模式。

### 页面结构 (Page Structure)

1.  **LoginPage**: 启动页。负责 CTP 连接配置与登录。
2.  **ShellPage**: 主骨架。包含左侧抽屉式导航栏 (`Drawer`)，负责页面路由。
3.  **AlarmPage**: 核心业务页。采用 Master-Detail 布局 (`SplitView`)，左侧为预警单列表，右侧为编辑面板。
4.  **LogPage**: 日志监控页。展示系统运行日志与交易回报。
5.  **SettingsPage**: 应用偏好设置页。

### 文件结构 (File Structure)

*   **ClientUI/**
    *   `main.cpp`: 应用程序入口，负责加载 QML 引擎和注册 C++ 类型。
    *   `main.qml`: 应用主窗口，负责初始路由（登录页/主页）。
    *   **Views/** (QML):
        *   `LoginPage.qml`: 登录界面。
        *   `ShellPage.qml`: 主界面骨架。
        *   `AlarmPage.qml`: 预警监控界面。
        *   `LogPage.qml`: 日志界面。
        *   `SettingsPage.qml`: 设置界面。
    *   **Models/** (C++):
        *   (待实现) `AlarmModel`: 预警单列表模型 (`QAbstractListModel`)。
        *   (待实现) `LogModel`: 日志列表模型 (`QAbstractListModel`)。

### 依赖管理 (Dependency Management)

本项目建议使用 **vcpkg** 进行第三方库管理。

#### 推荐依赖包 (Recommended Packages)

*   **JSON 处理**: `nlohmann-json` (现代 C++ JSON 库，语法类似 Python)
*   **数据库**: `sqlite3` (轻量级本地存储)
*   **网络通信**: `cpprestsdk` (微软出品的 C++ REST SDK) 或 `curl`
*   **MVVM 辅助**: `Microsoft.Toolkit.Mvvm` (如果支持 C++) 或手动实现 `INotifyPropertyChanged`。

#### 如何使用 vcpkg

1.  **安装 vcpkg**:
    ```powershell
    git clone https://github.com/microsoft/vcpkg
    .\vcpkg\bootstrap-vcpkg.bat
    ```
2.  **集成到 Visual Studio**:
    ```powershell
    .\vcpkg\vcpkg integrate install
    ```
3.  **安装包**:
    ```powershell
    .\vcpkg\vcpkg install nlohmann-json sqlite3 cpprestsdk:x64-windows
    ```

### TODO: 待讨论的接口定义 (Interfaces to be Discussed)

以下 Model 结构目前仅为 UI 绑定的骨架，具体业务字段需后续讨论确定：

#### 1. AlarmItem (预警单)
*   目前包含: `InstrumentId`, `LastPrice`, `ConditionDesc`, `Status`
*   **待补充**:
    *   具体的触发条件结构体 (如 `ConditionType`, `Threshold`)
    *   具体的下单参数结构体 (如 `Direction`, `Offset`, `Volume`)
    *   与 CTP `CThostFtdcInputOrderField` 的映射关系

#### 2. LogItem (日志)
*   目前包含: `Time`, `Level`, `Message`
*   **待补充**:
    *   日志持久化方案 (SQLite 表结构)
    *   日志级别枚举 (Info, Warning, Error, Debug)

#### 3. CTP 接口封装
*   需要定义一个单例 `CtpService` 类，负责：
    *   管理 `CThostFtdcMdApi` 和 `CThostFtdcTraderApi` 实例
    *   处理线程调度 (从 CTP 线程 -> UI 线程)
    *   提供 `Connect`, `Login`, `Subscribe` 等高层接口
