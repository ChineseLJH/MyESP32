#include "remote_net.h"
#include "internal_defs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "lwip/sockets.h"
#include "esp_netif.h"
#include "sdkconfig.h"
#include <string.h>
#include <sys/param.h>
#include <stdio.h> 

static const char *TAG = "RNET_SERVER";
static volatile bool g_udp_active = false;

#ifdef CONFIG_RNET_UDP_CONTROL_PORT
#define RNET_UDP_CTRL_PORT CONFIG_RNET_UDP_CONTROL_PORT
#else
#define RNET_UDP_CTRL_PORT CONFIG_RNET_UDP_PORT
#endif

/* --- CRC16 算法 (与 Qt 上位机一致) --- */
static uint16_t calculate_crc16(const char *data, int len)
{
    uint16_t crc = 0xFFFF;
    for (int i = 0; i < len; i++) {
        crc ^= (uint8_t)data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) crc = (crc >> 1) ^ 0xA001;
            else crc >>= 1;
        }
    }
    return crc;
}

/**
 * @brief 重组包并透传给下位机 (串口输出)
 */
static void forward_packet_to_uart(const char *content)
{
    char data_part[100];
    
    // 1. 重建带括号的数据体: [content]
    snprintf(data_part, sizeof(data_part), "[%s]", content);

    // 2. 计算 CRC16
    uint16_t crc = calculate_crc16(data_part, strlen(data_part));

    // 3. 拼接最终包: [数据]CRC
    uint8_t hi = (crc >> 8) & 0xFF;
    uint8_t lo = crc & 0xFF;
    
    // 4. 串口透传 (printf 默认输出到 UART0)
    // 注意：波特率建议设为 921600 或更高，否则 10ms 一包的打印会阻塞 CPU
    printf("%s%02X%02X\n", data_part, hi, lo);
}

/**
 * @brief 解析一行数据
 */
static void process_line(char *line, int length)
{
    // Trim
    while (length > 0 && (line[length - 1] == '\r' || line[length - 1] == '\n')) {
        line[--length] = 0;
    }

    // 寻找包头 [
    char *start_ptr = strchr(line, '['); 
    if (start_ptr != NULL) {
        // 寻找包尾 ]
        char *end_ptr = strchr(start_ptr, ']'); 
        
        if (end_ptr != NULL && end_ptr > start_ptr) {
            int content_len = end_ptr - start_ptr - 1;
            
            if (content_len > 0 && content_len < 100) {
                char content[100];
                memcpy(content, start_ptr + 1, content_len);
                content[content_len] = 0; 

                // 转发给 STM32
                forward_packet_to_uart(content);
            }
        }
    }
}

/* --- UDP 广播任务 --- */
static void udp_broadcast_task(void *pvParameters)
{
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    int broadcast = 1;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));

    struct sockaddr_in broadcast_addr;
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(CONFIG_RNET_UDP_PORT);
    broadcast_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);

    while (1) {
        if (!g_udp_active) {
            esp_netif_ip_info_t ip_info;
            esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
            if (netif && esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
                if (ip_info.ip.addr != 0) {
                    char msg[32];
                    sprintf(msg, IPSTR, IP2STR(&ip_info.ip));
                    sendto(sock, msg, strlen(msg), 0, (struct sockaddr *)&broadcast_addr, sizeof(broadcast_addr));
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
    vTaskDelete(NULL);
}

/* --- UDP 服务端任务 --- */
static void udp_server_task(void *pvParameters)
{
    char rx_buffer[128]; 

    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(RNET_UDP_CTRL_PORT);
    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Unable to create UDP socket");
        vTaskDelete(NULL);
        return;
    }
    
    // 允许地址复用
    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    if (bind(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0) {
        ESP_LOGE(TAG, "UDP bind failed");
        close(sock);
        vTaskDelete(NULL);
        return;
    }

    // UDP 无连接事件，使用接收超时做在线状态判断
    struct timeval timeout;
    timeout.tv_sec = 2; // 2秒超时
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    ESP_LOGI(TAG, "UDP Server listening on port %d", RNET_UDP_CTRL_PORT);

    while (1) {
        struct sockaddr_in source_addr;
        socklen_t addr_len = sizeof(source_addr);

        int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0,
                           (struct sockaddr *)&source_addr, &addr_len);

        if (len > 0) {
            g_udp_active = true;
            rx_buffer[len] = 0;

            process_line(rx_buffer, len);

            // 引入一个仅用于触发回传的局部静态或任务级变量
            static uint32_t local_rx_count = 0;
            local_rx_count++;
            
            // 严格每接收 10 个数据包，回复一次固定的 "ACK" 报文，不携带递增数字
            if (local_rx_count == 10) {
                local_rx_count = 0; // 重置计数器
                // 回发 3 字节的 "ACK" 代替原有的字符串化数字
                sendto(sock, "ACK", 3, 0, (struct sockaddr *)&source_addr, addr_len);
            }
        } else {
            if (g_udp_active) {
                ESP_LOGI(TAG, "UDP timeout, client deemed disconnected. Resuming broadcast...");
                g_udp_active = false;
                // num = 0;
            }
        }
    }
    vTaskDelete(NULL);
}

void remote_net_start(void)
{
    rnet_internal_wifi_init();
    xTaskCreate(udp_broadcast_task, "udp_bc", 4096, NULL, 3, NULL);
    // 控制通道优先级高一点，保证不丢包
    xTaskCreate(udp_server_task, "udp_sv", 4096, NULL, 10, NULL);
}