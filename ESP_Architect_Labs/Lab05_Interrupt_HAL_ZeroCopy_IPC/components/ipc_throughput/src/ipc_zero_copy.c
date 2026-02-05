#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "ipc_throughput.h"

static const char *TAG = "IPC_ZERO";

// ----------------------------------------------------------------------
// 1. 内存池定义 (The Memory Pool)
// ----------------------------------------------------------------------
// 定义缓冲池的数量。这决定了我们能容忍多大的突发流量 (Burst)。
#define BUFFER_POOL_COUNT   16 

// 静态分配 N 个 4KB 的块。这块内存在 .bss 段，系统启动时分配。
static ipc_packet_t g_memory_pool[BUFFER_POOL_COUNT];

// 两个队列句柄
static QueueHandle_t g_free_queue = NULL; // 存空闲块的指针
static QueueHandle_t g_data_queue = NULL; // 存有数据块的指针

static esp_timer_handle_t g_timer_handle = NULL;
static volatile uint32_t g_packets_sent = 0;
static volatile uint32_t g_packets_lost = 0; // 因无空闲块导致的丢包

// ----------------------------------------------------------------------
// 2. 消费者任务 (Core 1)
// ----------------------------------------------------------------------
static void task_consumer_zero_copy(void *arg)
{
    ipc_packet_t *p_packet = NULL; // 这是一个指针！不是巨大的结构体

    while (1) {
        // [A] 从数据队列获取指针 (只搬运 4 字节)
        if (xQueueReceive(g_data_queue, &p_packet, portMAX_DELAY) == pdTRUE) {
            
            // [B] 原地处理数据 (Zero Copy Access)
            // 直接通过指针访问内存，没有任何 memcpy 发生
            if (p_packet->data[0] != 0xAA) {
                 ESP_LOGE(TAG, "Data Verify Failed!");
            }

            // 模拟负载 (和 Phase A 保持一致，甚至可以更重)
            // esp_rom_delay_us(100); 

            // 打印统计 (每 10000 包打一次，因为现在太快了)
            if (p_packet->seq_num % 10000 == 0) {
                int64_t now = esp_timer_get_time();
                int32_t latency = (int32_t)(now - p_packet->timestamp);
                ESP_LOGI(TAG, "Seq: %lu, Latency: %ld us, Lost: %lu", 
                         p_packet->seq_num, latency, g_packets_lost);
            }

            // [C] 归还资源：把指针扔回空闲队列
            xQueueSend(g_free_queue, &p_packet, portMAX_DELAY);
        }
    }
}

// ----------------------------------------------------------------------
// 3. 生产者中断 (Core 0)
// ----------------------------------------------------------------------
static void IRAM_ATTR isr_timer_callback(void *arg)
{
    ipc_packet_t *p_packet = NULL;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    // [A] 尝试获取空闲块 (申请资源)
    // 如果 Free 队列空了，说明 Consumer 处理太慢，所有 Buffer 都在忙
    if (xQueueReceiveFromISR(g_free_queue, &p_packet, &xHigherPriorityTaskWoken) == pdTRUE) {
        
        // [B] 写入数据 (直接写内存)
        p_packet->seq_num = g_packets_sent;
        p_packet->timestamp = esp_timer_get_time();
        p_packet->data[0] = 0xAA;
        
        // [C] 发送数据 (发送指针)
        // 仅仅发送 4 字节的地址给消费者
        xQueueSendFromISR(g_data_queue, &p_packet, &xHigherPriorityTaskWoken);
        g_packets_sent++;

    } else {
        // [D] 无空闲块 (Resource Starvation)
        // 这就是零拷贝模式下的丢包：不是队列满，而是内存池空了
        g_packets_lost++;
    }

    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

// ----------------------------------------------------------------------
// 4. 初始化
// ----------------------------------------------------------------------
esp_err_t ipc_zero_copy_init(void)
{
    ESP_LOGW(TAG, "Initializing Phase B: Zero-Copy Mode (Pointer Passing)...");

    // [1] 创建指针队列
    // 关键点：Item Size 是 sizeof(ipc_packet_t*)，也就是 4 字节！
    // 哪怕载荷有 100MB，这里也只传 4 字节。
    g_free_queue = xQueueCreate(BUFFER_POOL_COUNT, sizeof(ipc_packet_t*));
    g_data_queue = xQueueCreate(BUFFER_POOL_COUNT, sizeof(ipc_packet_t*));

    if (g_free_queue == NULL || g_data_queue == NULL) return ESP_ERR_NO_MEM;

    // [2] 填充空闲队列 (Seed the pool)
    // 把静态数组里每个块的地址，都塞进 Free 队列
    for (int i = 0; i < BUFFER_POOL_COUNT; i++) {
        ipc_packet_t *ptr = &g_memory_pool[i];
        if (xQueueSend(g_free_queue, &ptr, 0) != pdPASS) {
            ESP_LOGE(TAG, "Failed to fill buffer pool");
            return ESP_FAIL;
        }
    }

    // [3] 创建任务 (Core 1)
    // Stack 可以给小一点了，因为我们不在栈上放 4KB 数据了，只有指针
    xTaskCreatePinnedToCore(task_consumer_zero_copy, "ZeroConsumer", 4096, NULL, 5, NULL, 1);

    // [4] 启动定时器
    const esp_timer_create_args_t timer_args = {
        .callback = &isr_timer_callback,
        .name = "ipc_producer_zero"
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &g_timer_handle));

    return ESP_OK;
}

void ipc_zero_copy_start(void)
{
    int interval_us = CONFIG_IPC_TIMER_INTERVAL_US;
    ESP_LOGW(TAG, "Starting Zero-Copy Timer at %d us...", interval_us);
    ESP_ERROR_CHECK(esp_timer_start_periodic(g_timer_handle, interval_us));
}