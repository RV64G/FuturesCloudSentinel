#include "thread_local.h"
#define WIN32_LEAN_AND_MEAN
// 初始化线程局部变量（必须在 .cpp 文件中定义，否则会报链接错误）
thread_local std::string ThreadLocalUser::s_user_id = "";
thread_local std::string ThreadLocalUser::s_user_token = "";
ClientContext* ThreadLocalUser::s_client = NULL;