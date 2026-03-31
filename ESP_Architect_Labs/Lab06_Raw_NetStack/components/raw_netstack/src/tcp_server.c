#include "tcp_server.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// ====== 跨平台标准的 POSIX 网络头文件 ======
// 以后你在 Linux SBC 上写 C 语言网络程序，用的也是这几个头文件！
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>     // 提供 close() 函数
#include <errno.h>      // 提供全局错误码 errno
#include <string.h>     // 提供 strerror() 将错误码转为字符串

static const char *TAG = "Linux_Socket";
#define PORT 3333

// 这是实际干活的线程函数（FreeRTOS Task 对应 Linux 的 pthread）
static void tcp_server_task(void *pvParameters)
{
    char addr_str[128];
    int addr_family = AF_INET;
    int ip_protocol = IPPROTO_IP;

    while (1) {
        // ---------------------------------------------------------
        // 1. 创建文件描述符 (socket)
        // AF_INET: IPv4, SOCK_STREAM: TCP流式套接字
        // ---------------------------------------------------------
        int listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
        if (listen_sock < 0) {
            // 【严谨性体现】：在 Linux 中，系统调用失败会设置 errno
            ESP_LOGE(TAG, "Unable to create socket: errno %d (%s)", errno, strerror(errno));
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue; // 创建失败则重试，千万不要直接退出线程
        }
        ESP_LOGI(TAG, "Socket created, fd = %d", listen_sock);

        // ---------------------------------------------------------
        // 2. 绑定 IP 地址与端口 (bind)
        // ---------------------------------------------------------
        struct sockaddr_in dest_addr;
        // bzero() 或 memset() 清零是一个好习惯，防止内存里的脏数据导致绑定失败
        memset(&dest_addr, 0, sizeof(dest_addr)); 
        
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_addr.s_addr = htonl(INADDR_ANY); // 绑定所有网卡 (0.0.0.0)
        dest_addr.sin_port = htons(PORT);              // 主机字节序转网络字节序 (大端序)

        int err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err != 0) {
            ESP_LOGE(TAG, "Socket unable to bind: errno %d (%s)", errno, strerror(errno));
            // 【严谨性体现】：绑定失败必须先 close 掉上面创建的 fd，否则会造成文件描述符泄漏 (fd leak)！
            close(listen_sock);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue; 
        }
        ESP_LOGI(TAG, "Socket bound to port %d", PORT);

        // ---------------------------------------------------------
        // 3. 将套接字转化为被动监听状态 (listen)
        // ---------------------------------------------------------
        // 参数 5 表示内核全连接队列 (backlog) 的最大长度
        err = listen(listen_sock, 5);
        if (err != 0) {
            ESP_LOGE(TAG, "Error occurred during listen: errno %d (%s)", errno, strerror(errno));
            close(listen_sock);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG, "Socket listening... waiting for connection");

        // TODO: 第四步 accept() 等待客户端接入... 我们先停在这里！
        
        // 为了防止死循环导致看门狗复位，在还没有写 accept 的时候先 delay 一下
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        
        // 目前仅仅测试创建过程，测试完后我们主动关闭再循环
        close(listen_sock);
    }
    
    // Linux 标准中，线程退出前需要清理资源 (FreeRTOS 中是 vTaskDelete(NULL))
    vTaskDelete(NULL);
}

void tcp_server_start(void)
{
    // 在 Linux 中这里会用 pthread_create()，在 ESP32 中我们用 xTaskCreate()
    // 分配 4KB 的栈空间，优先级设为 5
    xTaskCreate(tcp_server_task, "tcp_server", 4096, NULL, 5, NULL);
}