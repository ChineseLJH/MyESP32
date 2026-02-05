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
static volatile bool g_tcp_connected = false;

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
        if (!g_tcp_connected) {
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

/* --- TCP 服务端任务 --- */
static void tcp_server_task(void *pvParameters)
{
    char rx_buffer[128]; 
    static char line_buffer[256]; 
    static int line_pos = 0;
    double num = 0; // 计数器

    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(CONFIG_RNET_TCP_PORT);
    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    int listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    
    // 允许地址复用
    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    listen(listen_sock, 1);

    ESP_LOGI(TAG, "TCP Server listening on port %d", CONFIG_RNET_TCP_PORT);

    while (1) {
        struct sockaddr_in source_addr;
        socklen_t addr_len = sizeof(source_addr);
        
        int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
        if (sock < 0) continue;

        ESP_LOGI(TAG, "Client connected: %s", inet_ntoa(source_addr.sin_addr));
        
        // --- 优化 1: 禁用 Nagle 算法 (降低延迟) ---
        int nodelay = 1;
        setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));

        // --- 优化 2: 设置发送超时 (防止 send 卡死) ---
        // 如果手机卡顿不接收数据，ESP32 最多等 100ms，超时就丢弃，防止阻塞接收循环
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000; // 100ms
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

        g_tcp_connected = true;
        line_pos = 0; 
        // 每次新连接重置计数器 (可选)
        num = 0;

        while (1) {
            int len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);

            if (len <= 0) {
                ESP_LOGI(TAG, "Connection closed");
                break;
            } else {
                for (int i = 0; i < len; i++) {
                    char c = rx_buffer[i];
                    
                    if (c == '\n') {
                        // 1. 处理完整的一行
                        line_buffer[line_pos] = 0; 
                        process_line(line_buffer, line_pos);
                        
                        // 2. 【已恢复】回传计数给手机
                        // 这对 Qt 上位机判断连接心跳非常重要
                        num++;
                        char send_buf[32];
                        int slen = snprintf(send_buf, sizeof(send_buf), "%.0f\r\n", num);
                        
                        // 因为设置了 SO_SNDTIMEO，即使网络堵塞这里也不会死锁
                        send(sock, send_buf, slen, 0);

                        // 3. 准备下一行
                        line_pos = 0; 
                    } else {
                        if (line_pos < sizeof(line_buffer) - 1) {
                            line_buffer[line_pos++] = c;
                        } else {
                            line_pos = 0; // 溢出保护
                        }
                    }
                }
            }
        }

        close(sock);
        g_tcp_connected = false;
        ESP_LOGI(TAG, "Client disconnected");
    }
    vTaskDelete(NULL);
}

void remote_net_start(void)
{
    rnet_internal_wifi_init();
    xTaskCreate(udp_broadcast_task, "udp_bc", 4096, NULL, 3, NULL);
    // TCP 优先级高一点，保证不丢包
    xTaskCreate(tcp_server_task, "tcp_sv", 4096, NULL, 10, NULL);
}