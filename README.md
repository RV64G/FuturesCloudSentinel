# 期货云哨兵 (Futures Cloud Sentinel) ————期货云端预警系统

这是一个高性能的期货行情监控与预警系统，包含 **C++ Qt 客户端**、**高性能 C++ 服务器** 以及 **MySQL 数据库**。系统支持实时行情订阅、自定义价格/时间预警，并通过邮件发送实时通知。

---

## 📚 目录

- [期货云哨兵 (Futures Cloud Sentinel) ————期货云端预警系统](#期货云哨兵-futures-cloud-sentinel-期货云端预警系统)
  - [📚 目录](#-目录)
  - [🏗 系统架构](#-系统架构)
  - [💻 客户端 (Client)](#-客户端-client)
    - [技术栈](#技术栈)
    - [功能特性](#功能特性)
    - [项目结构](#项目结构)
  - [🖥 服务器 (Server)](#-服务器-server)
    - [技术栈](#技术栈-1)
    - [核心模块](#核心模块)
    - [项目结构](#项目结构-1)
  - [🗄 数据库 (Database)](#-数据库-database)
  - [📡 通信协议](#-通信协议)
  - [🛠 环境依赖与构建](#-环境依赖与构建)
    - [开发环境](#开发环境)
    - [依赖库](#依赖库)
    - [构建步骤](#构建步骤)

---

## 🏗 系统架构

本系统采用典型的 **C/S (Client-Server)** 架构：

1.  **客户端**: 基于 Qt/QML 开发，提供用户友好的图形界面。负责展示行情、管理预警任务，并通过 TCP 长连接与服务器通信。
2.  **服务器**: 基于 Windows IOCP/多线程模型开发。负责维护客户端连接、接入 CTP 行情接口、监控市场数据、触发预警逻辑，并调用 SMTP 服务发送邮件。
3.  **数据库**: 使用 MySQL 存储用户信息、预警条目和历史记录。

---

## 💻 客户端 (Client)

客户端位于 `AlarmingClient/` 目录下，是一个现代化的桌面应用程序。

### 技术栈
*   **语言**: C++17
*   **UI 框架**: Qt 6 (QML) - 实现流畅的现代化界面
*   **网络库**: Boost.Asio - 实现高性能异步 TCP 通信
*   **数据交换**: JSON (nlohmann/json)
*   **行情接口**: CTP (Comprehensive Transaction Platform) API

### 功能特性
*   **用户系统**: 支持用户注册、登录及凭证保存。
*   **实时行情**: 订阅并展示期货合约的实时价格（支持合约代码过滤）。
*   **多维预警**:
    *   **价格预警**: 设置价格上限和下限，触价即发。
    *   **时间预警**: 设置特定时间点提醒。
*   **通知管理**: 支持绑定接收邮箱，预警触发时自动发送邮件。
*   **调试模式**:
    *   **UI 调试**: 内置模拟数据生成器，无需连接真实服务器即可测试界面。
    *   **模拟服务器**: 支持本地模拟服务器响应，方便开发调试。

### 项目结构
```text
AlarmingClient/
├── base/
│   ├── FuturesClient.h/cpp    # 网络通信核心类 (Boost.Asio)
│   └── ...
├── ClientUI/
│   ├── Main.qml               # 主界面入口
│   ├── Backend.h/cpp          # QML 与 C++ 的交互桥梁 (ViewModel)
│   ├── QuoteClient.h/cpp      # 行情数据处理
│   ├── AuthManager.h/cpp      # 认证管理
│   ├── WarningManager.h/cpp   # 预警逻辑管理
│   └── ...
└── ...
```

---

## 🖥 服务器 (Server)

服务器位于 `Project2/` 目录下，是一个高并发的后台服务程序。

### 技术栈
*   **语言**: C++
*   **网络模型**: Windows Socket API + 线程池 (ThreadPool)
*   **数据库驱动**: MySQL Connector/C++
*   **邮件服务**: SMTP 协议实现 (EmailNotifier)

### 核心模块
1.  **连接管理 (ThreadPool & Router)**:
    *   使用线程池处理并发客户端连接。
    *   `RequestRouter` 负责分发请求到对应的处理器 (Handler)。
2.  **行情服务 (MarketService)**:
    *   负责连接期货交易所 CTP 接口，接收实时 Tick 数据。
    *   维护全市场的合约快照。
3.  **预警引擎**:
    *   实时比对最新行情与用户设置的预警条件。
    *   触发条件后，通过 `EmailNotifier` 发送邮件，并通过 TCP 推送通知给客户端。
4.  **数据库管理 (DBManager)**:
    *   单例模式封装 MySQL 连接。
    *   提供线程安全的数据库访问接口。

### 项目结构
```text
Project2/
├── Project2/
│   ├── main.cpp               # 服务器入口，初始化 Socket 和线程池
│   ├── MarketSeverce.h/cpp    # 行情服务模块
│   ├── db_manager.h/cpp       # 数据库连接池/单例
│   ├── EmailNotifier.h/cpp    # 邮件发送模块 (SMTP)
│   ├── threadpool.h/cpp       # 线程池实现
│   ├── router.h               # 请求路由分发
│   └── ...
└── ...
```

---

## 🗄 数据库 (Database)

系统使用 **MySQL** 作为持久化存储。主要包含以下数据表（推测结构）：

*   **users**: 存储用户账户信息 (ID, 用户名, 密码哈希, 绑定邮箱)。
*   **warnings**: 存储预警任务 (ID, 用户ID, 合约代码, 触发条件, 状态)。

`DBManager` 类负责管理数据库连接，确保多线程环境下的数据一致性。

---

## 📡 通信协议

客户端与服务器之间采用自定义的 **TCP 应用层协议**，解决了粘包/拆包问题。

*   **协议格式**: `Length-Prefix` + `JSON Body`
    *   **Header (4 Bytes)**: ASCII 整数，表示 Body 的长度。
    *   **Body (N Bytes)**: UTF-8 编码的 JSON 字符串。
*   **交互模式**:
    *   **Request-Response**: 客户端主动请求（如登录、添加预警）。
    *   **Server Push**: 服务器主动推送（如预警触发通知）。

详细协议定义请参考 [protocol.md](./protocol.md)。

---

## 🛠 环境依赖与构建

### 开发环境
*   **操作系统**: Windows 10/11 (推荐)
*   **IDE**: Visual Studio 2022 或 VS Code
*   **包管理器**: vcpkg

### 依赖库
*   **Boost**: 用于网络通信 (Asio) 和系统工具。
*   **Qt 6**: 用于客户端界面开发。
*   **MySQL Connector/C++**: 用于服务器连接数据库。
*   **nlohmann-json**: 用于 JSON 序列化/反序列化。
*   **OpenSSL**: 用于加密通信 (可选)。

### 构建步骤
1.  **安装 vcpkg** 并配置环境变量。
2.  **安装依赖**:
    ```bash
    vcpkg install boost:x64-windows qtbase:x64-windows nlohmann-json:x64-windows mysql-connector-cpp:x64-windows
    ```
3.  **客户端构建**:
    *   打开 `AlarmingClient/AlarmingClient.slnx` 或使用 CMake 构建。
4.  **服务器构建**:
    *   打开 `Project2/Project2.slnx` 或使用 CMake 构建。
    *   确保 MySQL 服务已启动，并配置 `DBManager` 中的连接参数。
