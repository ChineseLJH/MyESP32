#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* --------------------------------------------------------------------------
 * Configuration & Constants
 * -------------------------------------------------------------------------- */
// 保持 4096 (4KB) 以维持压力测试的高负载
// Phase A: 每次搬运 4096 字节 -> 崩
// Phase B: 每次搬运 4 字节指针 -> 稳
#define IPC_PAYLOAD_SIZE    4096

/* --------------------------------------------------------------------------
 * Data Structures
 * -------------------------------------------------------------------------- */
/* * 核心数据包结构
 * 使用 __attribute__((packed)) 防止编译器插入 padding
 */
typedef struct __attribute__((packed)) {
    uint32_t seq_num;               // 包序号
    int64_t  timestamp;             // 发送时间戳
    uint8_t  data[IPC_PAYLOAD_SIZE]; // 4KB 载荷
} ipc_packet_t;

/* --------------------------------------------------------------------------
 * Public API (对外暴露给 main.c 使用)
 * -------------------------------------------------------------------------- */
/**
 * @brief 根据 Kconfig 配置，初始化具体的模式 (Copy vs Zero-Copy)
 */
esp_err_t ipc_test_init(void);

/**
 * @brief 启动定时器中断
 */
void ipc_test_start(void);

/* --------------------------------------------------------------------------
 * Internal Implementations (供 ipc_throughput.c 调度)
 * -------------------------------------------------------------------------- */

/* Phase A: Naive Copy Mode (实现见 src/ipc_naive.c) */
esp_err_t ipc_naive_init(void);
void ipc_naive_start(void);

/* Phase B: Zero Copy Mode (实现见 src/ipc_zero_copy.c) */
esp_err_t ipc_zero_copy_init(void);
void ipc_zero_copy_start(void);

#ifdef __cplusplus
}
#endif