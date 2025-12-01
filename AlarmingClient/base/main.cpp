#include "FuturesClient.h"
#include <thread>

/**
 * @brief 主程序入口
 * 
 * 演示如何实例化 FuturesClient，注册 UI 回调，并调用业务接口。
 * 在实际的 UI 项目中 (如 Qt/MFC)，main 函数通常是 UI 框架的事件循环，
 * 而 FuturesClient 会作为一个成员变量集成在 MainWindow 或 Controller 中。
 */
int main(int argc, char* argv[]) {
    try {
        // 服务器配置
        std::string host = "127.0.0.1";
        std::string port = "8888";

        // 创建 IO 上下文 (Boost.Asio 的核心)
        boost::asio::io_context io_context;

        // 解析服务器地址
        tcp::resolver resolver(io_context);
        auto endpoints = resolver.resolve(host, port);

        // 实例化客户端
        // 注意：FuturesClient.h 中定义了 CLIENT_DEBUG_SIMULATION 宏
        // 如果该宏开启，客户端将不会连接真实服务器，而是进入模拟模式
        FuturesClient c(io_context, endpoints);

        // ==========================================
        // 1. UI 层集成：注册消息回调
        // ==========================================
        // 当客户端收到服务器消息（或模拟消息）时，会调用此 Lambda 函数。
        // 在实际 UI 开发中，这里通常会发射一个信号 (Signal) 通知 UI 线程更新界面。
        c.set_message_callback([](const json& j) {
            std::string type = j.value("type", "unknown");
            
            if (type == "alert_triggered") {
                // [场景]：收到预警推送
                // 动作：弹出红色报警窗口，播放声音
                std::cout << "[UI] >>> 收到报警！刷新界面，弹窗提示: " << j.value("message", "") << std::endl;
            } 
            else if (type == "response") {
                // [场景]：收到请求的操作结果 (如登录成功、添加成功)
                // 动作：关闭加载动画，提示成功或失败
                bool success = j.value("success", false);
                std::cout << "[UI] >>> 操作结果: " << (success ? "成功" : "失败") << " - " << j.value("message", "") << std::endl;
            }
            else {
                std::cout << "[UI] >>> 收到其他消息: " << j.dump() << std::endl;
            }
        });

        // ==========================================
        // 2. 启动网络线程
        // ==========================================
        // io_context.run() 是一个阻塞调用，它会处理所有的异步网络事件。
        // 为了不阻塞主线程 (UI 线程)，我们通常在一个独立的线程中运行它。
        std::thread t([&io_context]() { 
            io_context.run(); 
        });

        // ==========================================
        // 3. 模拟用户操作 (仅在调试模式下演示)
        // ==========================================
#ifdef CLIENT_DEBUG_SIMULATION
        // 模拟用户点击 "登录" 按钮
        std::cout << "[Main] 模拟用户登录..." << std::endl;
        c.login("test_user", "123456");
        
        // 模拟等待 1 秒
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        // 模拟用户点击 "添加预警" 按钮
        std::cout << "[Main] 模拟添加预警..." << std::endl;
        c.add_price_warning("acc1", "BTCUSDT", 60000, 50000);

        std::this_thread::sleep_for(std::chrono::seconds(1));

        // 模拟用户点击 "查询预警" 按钮
        std::cout << "[Main] 模拟查询预警..." << std::endl;
        c.query_warnings("all");
#else
        std::cout << "[Main] 正在连接真实服务器 " << host << ":" << port << "..." << std::endl;
        std::cout << "[Main] 请按 Ctrl+C 退出程序。" << std::endl;
#endif

        // ==========================================
        // 4. 保持主线程运行
        // ==========================================
        // 在实际 UI 程序中，这里是 UI 框架的主事件循环 (如 QApplication::exec())
        t.join();

    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}
