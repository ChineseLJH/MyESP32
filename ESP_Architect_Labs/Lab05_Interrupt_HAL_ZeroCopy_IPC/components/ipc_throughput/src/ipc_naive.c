#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "ipc_throughput.h"

static const char *TAG = "IPC_NAIVE";

// 全局句柄
static QueueHandle_t g_naive_queue_handle = NULL;
static esp_timer_handle_t g_timer_handle = NULL;

// 统计信息 (放在 IRAM 中以提高存取速度，非必需但符合嵌入式习惯)
static volatile uint32_t g_packets_sent = 0;
static volatile uint32_t g_packets_lost = 0; // 队列满导致发送失败

/*
 * --------------------------------------------------------------------------
 * Consumer Task (消费者任务) - 运行在 Core 1
 * --------------------------------------------------------------------------
 * 它的工作就是不断从队列里 "Memcpy" 数据出来。
 */
static void task_consumer_naive(void *arg)
{
    // 在栈上分配接收缓存。
    // 警告：这个结构体很大 (1036字节)，必须确保创建 Task 时分配了足够的栈空间！
    ipc_packet_t recv_packet; 

    while (1) {
        // 1. 阻塞等待数据
        // portMAX_DELAY 表示死等。
        // 当数据到达时，FreeRTOS 会把数据从队列内部存储区 memcpy 到 &recv_packet
        if (xQueueReceive(g_naive_queue_handle, &recv_packet, portMAX_DELAY) == pdTRUE) {

            esp_rom_delay_us(100);
            
            // 2. 模拟业务处理 (校验)
            // 这里的目的是产生一点点计算负载，防止编译器把代码优化没了
            if (recv_packet.data[0] != 0xAA) {
                 ESP_LOGE(TAG, "Data Corruption!");
            }

            // 3. (可选) 打印调试信息，为了不刷屏，每 1000 个包打一次
            if (recv_packet.seq_num % 1000 == 0) {
                int64_t now = esp_timer_get_time();
                // 计算延迟：当前时间 - 发送时间
                int32_t latency = (int32_t)(now - recv_packet.timestamp);
                ESP_LOGI(TAG, "Seq: %lu, Latency: %ld us, Lost: %lu", 
                         recv_packet.seq_num, latency, g_packets_lost);
            }
        }
    }
}

/*
 * --------------------------------------------------------------------------
 * Producer ISR (生产者中断) - 运行在 Core 0 (通常 timer 中断在 Core 0)
 * --------------------------------------------------------------------------
 * 模拟硬件产生数据。
 * IRAM_ATTR: 告诉链接器把这个函数放在内部 RAM，防止 Flash Cache Miss 导致中断延迟抖动。
 */
static void IRAM_ATTR isr_timer_callback(void *arg)
{
    // 1. 准备数据
    // 使用 static 避免炸掉 ISR 栈 (1KB 太大了)
    // 这一步模拟“硬件寄存器”里的数据准备好了
    static ipc_packet_t tx_packet; 
    
    tx_packet.seq_num = g_packets_sent;
    tx_packet.timestamp = esp_timer_get_time();
    // 简单填充一点数据
    tx_packet.data[0] = 0xAA;
    tx_packet.data[IPC_PAYLOAD_SIZE - 1] = 0x55;

    // 2. 高优先级唤醒标志
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    // 3. 发送数据 (The Bottleneck!)
    // 这里的 xQueueSendFromISR 会执行 memcpy(&queue_storage, &tx_packet, 1036);
    // 这是我们在 Phase A 故意制造的 CPU 杀手。
    if (xQueueSendFromISR(g_naive_queue_handle, &tx_packet, &xHigherPriorityTaskWoken) == pdTRUE) {
        g_packets_sent++;
    } else {
        // 队列满了，说明 Consumer 没来得及取走，发生丢包
        g_packets_lost++;
    }

    // 4. 如果唤醒了更高优先级任务，请求上下文切换
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

/*
 * --------------------------------------------------------------------------
 * 对外接口实现
 * --------------------------------------------------------------------------
 */

// 这些函数将在 ipc_throughput.c 的逻辑中被调用
// 但为了简单，我们先在这里实现一个具体的初始化，之后再整合

esp_err_t ipc_naive_init(void)
{
    ESP_LOGW(TAG, "Initializing Phase A: Naive Copy Mode...");

    // 1. 创建队列
    // 深度: 10 (缓冲区能存10个包)
    // Item Size: 1036 字节 (直接存结构体) -> 内存占用 ~10KB
    g_naive_queue_handle = xQueueCreate(10, sizeof(ipc_packet_t));
    if (g_naive_queue_handle == NULL) {
        ESP_LOGE(TAG, "Failed to create queue! Out of memory?");
        return ESP_ERR_NO_MEM;
    }

    // 2. 创建消费者任务
    // 绑定到 Core 1，与 Timer 中断 (Core 0) 分离，制造跨核通信场景
    // Stack Depth: 4096 字节 (因为我们在栈上放了 1KB 的变量，栈必须大)
    BaseType_t ret = xTaskCreatePinnedToCore(
        task_consumer_naive,
        "NaiveConsumer",
        8192,           // Stack size
        NULL,           // Arg
        5,              // Priority (High)
        NULL,           // Handle
        1               // Core ID (1)
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create task!");
        return ESP_FAIL;
    }

    // 3. 配置定时器 (模拟硬件中断)
    const esp_timer_create_args_t timer_args = {
        .callback = &isr_timer_callback,
        .name = "ipc_producer"
    };
    
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &g_timer_handle));

    return ESP_OK;
}

void ipc_naive_start(void)
{
    // 读取 Kconfig 中的频率配置
    int interval_us = CONFIG_IPC_TIMER_INTERVAL_US;
    ESP_LOGW(TAG, "Starting Timer at %d us interval...", interval_us);
    
    // 启动周期性定时器
    ESP_ERROR_CHECK(esp_timer_start_periodic(g_timer_handle, interval_us));
}