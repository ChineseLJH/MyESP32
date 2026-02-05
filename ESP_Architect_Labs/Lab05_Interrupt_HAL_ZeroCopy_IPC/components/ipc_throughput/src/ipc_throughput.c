#include "sdkconfig.h" // 必须包含！否则读不到 CONFIG_ 宏
#include "esp_log.h"
#include "ipc_throughput.h"

static const char *TAG = "IPC_MGR";

esp_err_t ipc_test_init(void)
{
    ESP_LOGI(TAG, "Initializing IPC Throughput Lab...");

    // ----------------------------------------------------------------
    // 分支逻辑：根据 Kconfig 定义的宏来决定运行哪个模式
    // ----------------------------------------------------------------
    
    #if defined(CONFIG_IPC_MODE_COPY)
        // 模式 A: 笨拙拷贝
        ESP_LOGW(TAG, "Mode Selected: Phase A (Naive Copy)");
        ESP_LOGW(TAG, "WARNING: High CPU usage expected due to memcpy(%d bytes)", IPC_PAYLOAD_SIZE);
        return ipc_naive_init();

    #elif defined(CONFIG_IPC_MODE_ZERO_COPY)
        // 模式 B: 零拷贝 (指针传递)
        ESP_LOGW(TAG, "Mode Selected: Phase B (Zero Copy)");
        ESP_LOGI(TAG, "Optimized: Passing 4-byte pointers instead of %d-byte data", IPC_PAYLOAD_SIZE);
        return ipc_zero_copy_init();

    #else
        // 此时 menuconfig 里可能什么都没选 (很少见)
        ESP_LOGE(TAG, "No IPC mode selected in Kconfig!");
        return ESP_ERR_NOT_SUPPORTED;
    #endif
}

void ipc_test_start(void)
{
    #if defined(CONFIG_IPC_MODE_COPY)
        ipc_naive_start();
    #elif defined(CONFIG_IPC_MODE_ZERO_COPY)
        ipc_zero_copy_start();
    #endif
}