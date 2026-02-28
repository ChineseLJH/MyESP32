#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "protocol_examples_common.h"

static const char *TAG = "Lab06_Main";

void app_main(void)
{
    /* 1. 初始化非易失性存储 (NVS)
     * 底层机理：Wi-Fi MAC/PHY 驱动在启动时，必须从 NVS 读取射频 (RF) 校准参数。
     * 若遇到分区空间不足或版本不匹配，需强制擦除后重新初始化。
     */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* 2. 初始化网络接口 (Netif)
     * 底层机理：拉起 LwIP 协议栈的核心数据结构，为后续创建网络接口（如 STA/AP）分配内存。
     */
    ESP_ERROR_CHECK(esp_netif_init());

    /* 3. 创建系统默认事件循环
     * 底层机理：启动一个专用的后台 Task，用于监听和分发由 Wi-Fi 驱动底层抛出的异步中断事件
     * （例如 WIFI_EVENT_STA_START，IP_EVENT_STA_GOT_IP 等）。
     */
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* 4. 挂载网络并获取 IP
     * 底层机理：调用封装库，内部执行 L2 层（802.11 协议）的关联/认证状态机，
     * 随后启动 L3 层 DHCP 客户端状态机获取 IP 地址。该函数默认阻塞，直到获取有效 IP。
     */
    ESP_LOGI(TAG, "Starting network connection...");
    ESP_ERROR_CHECK(example_connect());
    ESP_LOGI(TAG, "Network connected successfully.");

    /* 5. 协议栈启动入口 (预留)
     * TODO: 在这里调用 raw_netstack 组件中的 tcp_server_start() 
     */
}