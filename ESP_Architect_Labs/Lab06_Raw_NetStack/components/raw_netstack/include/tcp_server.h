#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 启动 TCP 服务器任务
 * 映射 Linux 概念：类似于在 main() 函数中拉起一个专门负责网络的后台线程 (pthread)
 */
void tcp_server_start(void);

#ifdef __cplusplus
}
#endif