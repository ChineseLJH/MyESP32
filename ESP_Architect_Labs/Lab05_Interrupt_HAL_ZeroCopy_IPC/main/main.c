#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "ipc_throughput.h" // 引用我们的组件

void app_main(void)
{
    // 1. 初始化 IPC 测试组件
    ESP_ERROR_CHECK(ipc_test_init());

    // 2. 稍微停顿一下，让 Log 打印完，看清楚配置
    vTaskDelay(pdMS_TO_TICKS(1000));

    // 3. 启动测试 (开启定时器中断)
    ipc_test_start();

    // 4. 主任务可以退场了，或者做个简单的监控
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        // 这里只是为了让 main task 不退出，虽然 app_main 退出也没事
        // 实际工作中通常由 watchdog 监控
    }
}