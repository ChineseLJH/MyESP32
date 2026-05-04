#include <string.h>
#include <sys/param.h>
#include "esp_log.h"
#include "lwip/sockets.h"

static const char *TAG = "TCP_SERVER";

void do_retransmit(const int sock)
{
    int len;
    char rx_buffer[128];
    char peer_ip[128] = "Unknown";
    int peer_port = 0;

    struct sockaddr_storage peer_addr;
    socklen_t peer_addr_len = sizeof(peer_addr);

    if (getpeername(sock, (struct sockaddr *)&peer_addr, &peer_addr_len) == 0) {
        if (peer_addr.ss_family == AF_INET) { // 确认是 IPv4
            struct sockaddr_in *peer_ipv4 = (struct sockaddr_in *)&peer_addr;
            // 提取 IP 并转成字符串
            inet_ntoa_r(peer_ipv4->sin_addr, peer_ip, sizeof(peer_ip) - 1);
            // 提取端口（注意：必须用 ntohs 从网络大端序转换回主机的正常小端序！）
            peer_port = ntohs(peer_ipv4->sin_port); 
        }
    }

    do {
        len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
        if (len < 0) {
            ESP_LOGE(TAG, "Error occurred during receiving: errno %d", errno);
        } else if (len == 0) {
            ESP_LOGW(TAG, "Connection closed");
        } else {
            rx_buffer[len] = 0; // Null-terminate whatever is received and treat it like a string
            // ESP_LOGI(TAG, "Received %d bytes: %s", len, rx_buffer);
            ESP_LOGI(TAG, "[%s:%d] Received %d bytes: %s", peer_ip, peer_port, len, rx_buffer);

            // send() can return less bytes than supplied length.
            // Walk-around for robust implementation.
            int to_write = len;
            while (to_write > 0) {
                int written = send(sock, rx_buffer + (len - to_write), to_write, 0);
                if (written < 0) {
                    ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
                    // Failed to retransmit, giving up
                    return;
                }
                to_write -= written;
            }
        }
    } while (len > 0);
}