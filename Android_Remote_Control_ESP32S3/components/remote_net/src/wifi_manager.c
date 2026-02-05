#include "internal_defs.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "lwip/ip_addr.h"

static const char *TAG = "RNET_WIFI";

static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*) event_data;
        ESP_LOGW(TAG, "WiFi Disconnected (Reason: %d). Retrying...", event->reason);
        // 延时 1 秒重连，避免频繁撞墙导致热点拉黑
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
    }
}

void rnet_internal_wifi_init(void)
{
    // 1. NVS 初始化
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. 网络接口初始化
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    // 3. WiFi 初始化
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 4. 注册事件
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, NULL));

    // 5. 配置参数 (兼容性优化版)
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_RNET_WIFI_SSID,
            .password = CONFIG_RNET_WIFI_PASSWORD,
            
            // 【关键兼容性设置 1】
            // 明确指定 WPA2，避免 ESP32 尝试升级到 WPA3 导致握手失败
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            
            // 【关键兼容性设置 2】
            // 彻底禁用 PMF (受保护管理帧)。部分手机热点会因为 PMF 协商失败而拒连。
            .pmf_cfg = {
                .capable = false,
                .required = false
            },
            
            // 使用全信道扫描 + 信号强度排序
            .scan_method = WIFI_ALL_CHANNEL_SCAN,
            .sort_method = WIFI_CONNECT_AP_BY_SIGNAL,
        },
    };
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    
    // 【关键兼容性设置 3】
    // 强制使用 B/G/N 协议，屏蔽 WiFi 6 (AX)，防止物理层协商失败
    ESP_ERROR_CHECK(esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N));

    ESP_ERROR_CHECK(esp_wifi_start());

    // 【性能优化】
    // 关闭省电模式，防止 Ping 值波动和丢包。必须在 start 之后调用。
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    
    ESP_LOGI(TAG, "WiFi Init Done (Low Latency Mode)");
}