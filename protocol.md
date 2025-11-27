# 期货云端预警系统 - 通信协议规范 V3.0

## 1. 基础通信层 (Transport Layer)

本系统采用基于 TCP 的长连接通信模式。为了解决 TCP 粘包和拆包问题，应用层协议采用 **Length-Prefix (长度前缀)** 格式。

*   **传输协议**: TCP (Transmission Control Protocol)
*   **数据包结构**:
    ```text
    +-------------------+--------------------------------+
    | Header (4 Bytes)  |          Body (N Bytes)        |
    +-------------------+--------------------------------+
    | ASCII Integer     |       UTF-8 JSON String        |
    +-------------------+--------------------------------+
    ```
    *   **Header (包头)**: 固定 4 字节。存储一个 ASCII 格式的十进制整数，表示 Body 的字节长度。不足 4 位时左侧补空格或零（例如 `" 120"` 或 `"0120"`）。
    *   **Body (包体)**: 长度由 Header 指定。内容为标准的 UTF-8 编码 JSON 字符串。
*   **交互模式**: 
    *   **请求-响应 (Request-Response)**: 客户端发送请求，服务器必须回复。
    *   **异步推送 (Server Push)**: 服务器可在任意时刻主动向客户端推送消息（如预警触发）。
    *   **心跳保活**: (可选) 建议客户端每 30 秒发送一次心跳包以维持连接。

## 2. 业务数据定义 (Business Logic)

所有 JSON 消息必须包含 `type` 字段，用于标识消息类型。

### A. 用户账户管理 (User Account)

#### 1. 用户注册 (Register)
*   **方向**: Client -> Server
*   **描述**: 新用户注册账户。
```json
{
    "type": "register",
    "request_id": "req_001",     // [必填] 请求唯一标识，用于匹配响应
    "username": "client001",     // [必填] 用户名
    "password": "hashed_password_here" // [必填] 密码 (建议传输哈希值)
}
```

#### 2. 用户登录 (Login)
*   **方向**: Client -> Server
*   **描述**: 验证用户身份，建立会话。
```json
{
    "type": "login",
    "request_id": "req_002",
    "username": "client001",
    "password": "hashed_password_here"
}
```

#### 3. 设置接收邮箱 (Set Email)
*   **方向**: Client -> Server
*   **描述**: 设置用于接收邮件通知的邮箱地址。
```json
{
    "type": "set_email",
    "request_id": "req_003",
    "email": "alert@example.com"
}
```

### B. 预警单管理 (Warning Orders)

#### 4. 添加预警单 (Add Warning)

**场景一：价格预警 (Price Warning)**
*   **描述**: 监控特定合约的价格，当价格超出设定区间时触发。
```json
{
    "type": "add_warning",
    "request_id": "req_004",
    "warning_type": "price",     // [必填] 预警类型：价格
    "account": "88001234",       // [必填] 关联的期货账号
    "symbol": "rb2310",          // [必填] 合约代码
    "max_price": 3600.0,         // [必填] 触发上限 (大于此值触发)
    "min_price": 3400.0          // [必填] 触发下限 (小于此值触发)
}
```

**场景二：时间预警 (Time Warning)**
*   **描述**: 在特定时间点触发提醒。
```json
{
    "type": "add_warning",
    "request_id": "req_005",
    "warning_type": "time",      // [必填] 预警类型：时间
    "account": "88001234",
    "symbol": "rb2310",
    "trigger_time": "2023-11-25 14:55:00" // [必填] 触发时间 (格式: YYYY-MM-DD HH:MM:SS)
}
```

#### 5. 删除预警单 (Delete Warning)
*   **方向**: Client -> Server
*   **描述**: 移除一个不再需要的预警单。
```json
{
    "type": "delete_warning",
    "request_id": "req_006",
    "order_id": "1001"           // [必填] 待删除的预警单 ID
}
```

#### 6. 修改预警单 (Modify Warning)
*   **方向**: Client -> Server
*   **描述**: 修改现有预警单的触发条件。

**场景一：修改价格预警**
```json
{
    "type": "modify_warning",
    "request_id": "req_007",
    "order_id": "1001",          // [必填] 目标预警单 ID
    "warning_type": "price",     // [必填] 明确修改类型
    "max_price": 3650.0,         // [可选] 新的上限
    "min_price": 3450.0          // [可选] 新的下限
}
```

**场景二：修改时间预警**
```json
{
    "type": "modify_warning",
    "request_id": "req_008",
    "order_id": "1002",
    "warning_type": "time",
    "trigger_time": "2023-11-25 16:00:00" // [可选] 新的触发时间
}
```

### C. 查询与推送 (Query & Push)

#### 7. 查询预警单 (Query Warnings)
*   **方向**: Client -> Server
*   **描述**: 获取当前用户的所有预警单列表。
```json
{
    "type": "query_warnings",
    "request_id": "req_008",
    "status_filter": "active"    // [可选] 过滤状态: "active"(生效中), "triggered"(已触发), "all"(全部)
}
```

#### 8. 通用响应 (Server Response)
*   **方向**: Server -> Client
*   **描述**: 服务器对上述所有请求的回复。
```json
{
    "type": "response",
    "request_id": "req_004",     // [重要] 回传请求中的 request_id，用于客户端匹配回调
    "request_type": "add_warning", // [可选] 提示原请求类型
    "success": true,             // [必填] 操作是否成功
    "message": "添加成功",       // [可选] 状态描述或错误信息
    "data": {                    // [可选] 附加数据
        "order_id": "1002"       // 例如：添加成功后返回新生成的 ID
    }
}
```

#### 9. 预警触发通知 (Alert Triggered)
*   **方向**: Server -> Client
*   **描述**: 当预警条件满足时，服务器主动推送此消息。
```json
{
    "type": "alert_triggered",
    "alert_id": "msg_9999",      // [必填] 通知的唯一 ID，用于 ACK
    "order_id": "1001",          // [必填] 触发的预警单 ID
    "symbol": "rb2310",          // 合约代码
    "trigger_value": 3601.0,     // 触发时的实际数值 (价格)
    "trigger_time": "2023-11-25 15:00:01", // 触发时间
    "message": "您的螺纹钢合约已突破上限 3600.0，当前价格 3601.0！" // 显示给用户的文本
}
```

#### 10. 预警通知确认 (Alert ACK)
*   **方向**: Client -> Server
*   **描述**: 客户端收到 `alert_triggered` 后必须回复此消息，确保通知已送达。
```json
{
    "type": "alert_ack",
    "alert_id": "msg_9999"       // [必填] 对应 alert_triggered 中的 alert_id
}
```
