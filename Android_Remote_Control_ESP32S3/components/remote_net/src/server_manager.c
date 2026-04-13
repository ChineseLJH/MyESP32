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

#include "driver/uart.h"
#include "esp_timer.h"

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
    static int64_t last_packet_time = 0;
    int64_t current_time = esp_timer_get_time(); // 获取当前微秒数

    char data_part[100];
    snprintf(data_part, sizeof(data_part), "[%s]", content);

    uint16_t crc = calculate_crc16(data_part, strlen(data_part));
    uint8_t hi = (crc >> 8) & 0xFF;
    uint8_t lo = crc & 0xFF;
    
    // 物理输出
    printf("%s%02X%02X\n", data_part, hi, lo);

    // 计算并记录间隔 (Interval)
    int64_t interval = 0;
    if (last_packet_time != 0) {
        interval = current_time - last_packet_time;
    }
    last_packet_time = current_time;

    // 打印到 monitor
    // ESP_LOGI(TAG, "Interval: %lld us", interval);
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
        
        // 【最小改动】：加了一个 strlen(end_ptr) >= 5 的判断，确保 ']' 后面至少有 4位CRC 
        // 因为 end_ptr 指向 ']'，所以如果后面有 4 个字符，它的长度至少是 5
        if (end_ptr != NULL && end_ptr > start_ptr && strlen(end_ptr) >= 5) {
            
            // 1. 提取收到的 4 位 CRC (16进制字符串转数字)
            char recv_crc_str[5];
            memcpy(recv_crc_str, end_ptr + 1, 4);
            recv_crc_str[4] = 0;
            uint16_t recv_crc = (uint16_t)strtol(recv_crc_str, NULL, 16);

            // 2. 计算本地 CRC（对包含头尾的 [...] 进行计算）
            int packet_len = end_ptr - start_ptr + 1; 
            uint16_t calc_crc = calculate_crc16(start_ptr, packet_len);

            // 3. 拦截网关：只有 CRC 匹配，才允许透传
            if (recv_crc == calc_crc) {
                int content_len = end_ptr - start_ptr - 1;
                
                if (content_len > 0 && content_len < 100) {
                    char content[100];
                    memcpy(content, start_ptr + 1, content_len);
                    content[content_len] = 0; 

                    // 转发给 STM32（原封不动，它里面会重新拼装 [...] 和 CRC）
                    forward_packet_to_uart(content);
                }
            } else {
                // 如果需要调试，可以解开下面的注释
                // ESP_LOGW(TAG, "CRC Error! Recv: %04X, Calc: %04X", recv_crc, calc_crc);
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
    
    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    if (bind(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0) {
        ESP_LOGE(TAG, "UDP bind failed");
        close(sock);
        vTaskDelete(NULL);
        return;
    }

    // 设置接收超时，用于判定客户端离线
    struct timeval timeout = { .tv_sec = 2, .tv_usec = 0 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    ESP_LOGI(TAG, "UDP Server listening on port %d", RNET_UDP_CTRL_PORT);

    while (1) {
        struct sockaddr_in source_addr;
        socklen_t addr_len = sizeof(source_addr);

        // 1. 阻塞接收：捕获网络冻结恢复后的第一个包
        int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0,
                           (struct sockaddr *)&source_addr, &addr_len);

        if (len > 0) {
            g_udp_active = true;

            // // 【核心：突发吸收器】挂起 10ms，让空中积压的后续历史包全部进入 LwIP 邮箱
            // vTaskDelay(1); 

            char latest_packet[128];
            int latest_len = len;
            memcpy(latest_packet, rx_buffer, len);

            // 2. 抽空机制：循环提取所有积压包，仅保留最后一帧（头部丢弃）
            while (1) {
                int n = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, MSG_DONTWAIT,
                                 (struct sockaddr *)&source_addr, &addr_len);
                if (n > 0) {
                    memcpy(latest_packet, rx_buffer, n);
                    latest_len = n;
                } else {
                    break; // 邮箱已空，此时 latest_packet 存储的是绝对最新帧
                }
            }

            // 3. 解析并透传给 STM32 (内部包含 921600 波特率的 printf 和间隔打印)
            latest_packet[latest_len] = 0;
            process_line(latest_packet, latest_len);

            // 4. ACK 逻辑：每处理 10 个有效包回复一次
            static uint32_t local_rx_count = 0;
            if (++local_rx_count >= 10) {
                local_rx_count = 0;
                sendto(sock, "ACK", 3, 0, (struct sockaddr *)&source_addr, addr_len);
            }
        } else {
            if (g_udp_active) {
                ESP_LOGI(TAG, "UDP timeout, client deemed disconnected.");
                g_udp_active = false;
            }
        }
    }
    close(sock);
    vTaskDelete(NULL);
}

void remote_net_start(void)
{
    uart_set_baudrate(UART_NUM_0, 921600);

    rnet_internal_wifi_init();
    xTaskCreate(udp_broadcast_task, "udp_bc", 4096, NULL, 3, NULL);
    // 控制通道优先级高一点，保证不丢包
    xTaskCreate(udp_server_task, "udp_sv", 4096, NULL, 10, NULL);
}