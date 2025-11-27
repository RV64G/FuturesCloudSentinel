#include "FuturesClient.h"
#include <thread>

int main(int argc, char* argv[]) {
    try {
        std::string host = "127.0.0.1";
        std::string port = "8888";

        boost::asio::io_context io_context;

        tcp::resolver resolver(io_context);
        auto endpoints = resolver.resolve(host, port);

        // 实例化客户端
        FuturesClient c(io_context, endpoints);

        // --- 模拟 UI 层集成 ---
        // 在实际 UI 项目中，这里会是 UI 框架的初始化代码
        // UI 层注册回调函数，处理来自服务器的消息
        c.set_message_callback([](const json& j) {
            std::string type = j.value("type", "unknown");
            
            if (type == "alert_triggered") {
                // UI 动作：弹出红色报警窗口，播放声音
                std::cout << "[UI] >>> 收到报警！刷新界面，弹窗提示: " << j.value("message", "") << std::endl;
            } 
            else if (type == "response") {
                // UI 动作：关闭加载动画，提示成功或失败
                bool success = j.value("success", false);
                std::cout << "[UI] >>> 操作结果: " << (success ? "成功" : "失败") << " - " << j.value("message", "") << std::endl;
            }
            else {
                std::cout << "[UI] >>> 收到其他消息: " << j.dump() << std::endl;
            }
        });

        // 启动网络线程
        // 在 UI 程序中，通常会在单独的线程运行 io_context，或者集成到 UI 的事件循环中
        std::thread t([&io_context]() { io_context.run(); });

#ifdef CLIENT_DEBUG_SIMULATION
        // 模拟用户操作
        std::cout << "[Main] 模拟用户登录..." << std::endl;
        c.login("test_user", "123456");
        
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        std::cout << "[Main] 模拟添加预警..." << std::endl;
        c.add_price_warning("acc1", "BTCUSDT", 60000, 50000);

        std::this_thread::sleep_for(std::chrono::seconds(1));

        std::cout << "[Main] 模拟查询预警..." << std::endl;
        c.query_warnings("all");
#endif

        // 保持主线程运行 (在实际 UI 程序中，这里是 UI 的主事件循环)
        t.join();

    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}
