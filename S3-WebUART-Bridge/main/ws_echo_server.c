#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <esp_http_server.h>
#include "driver/gpio.h"
#include "driver/uart.h" 
#include "web_uart_helper.h" 

static const char *TAG = "MAIN";

#define TXD_PIN (GPIO_NUM_1)
#define RXD_PIN (GPIO_NUM_2)
#define UART_PORT_NUM (UART_NUM_1)
#define UART_BAUD_RATE (115200)
#define BUF_SIZE (1024)

// 全局的服务器句柄，用于后台任务向网页主动发消息
static httpd_handle_t global_server = NULL;

void uart_init(void) {
    const uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_driver_install(UART_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_PORT_NUM, &uart_config);
    uart_set_pin(UART_PORT_NUM, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

// ================== 新增：主动推送数据到所有网页客户端 ==================
static void send_uart_data_to_websockets(const uint8_t *data, size_t len) {
    if (!global_server) return;
    
    // 获取当前连上的所有客户端
    size_t clients = 4;
    int client_fds[4];
    if (httpd_get_client_list(global_server, &clients, client_fds) == ESP_OK) {
        for (int i = 0; i < clients; i++) {
            int fd = client_fds[i];
            // 确认这个客户端是 WebSocket 握手过的
            if (httpd_ws_get_fd_info(global_server, fd) == HTTPD_WS_CLIENT_WEBSOCKET) {
                httpd_ws_frame_t ws_pkt;
                memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
                ws_pkt.payload = (uint8_t*)data;
                ws_pkt.len = len;
                ws_pkt.type = HTTPD_WS_TYPE_TEXT;
                
                // 异步发送：从其他线程安全地向 HTTPD 任务投递数据
                httpd_ws_send_frame_async(global_server, fd, &ws_pkt);
            }
        }
    }
}

// ================== 新增：读取物理串口的后台任务 ==================
static void uart_rx_task(void *arg) {
    uint8_t* data = (uint8_t*) malloc(BUF_SIZE + 1);
    while (1) {
        // 阻塞等待串口 2(RX) 接收外部设备发来的数据
        int rxBytes = uart_read_bytes(UART_PORT_NUM, data, BUF_SIZE, 100 / portTICK_PERIOD_MS);
        if (rxBytes > 0) {
            data[rxBytes] = '\0'; // 加上字符串结尾
            ESP_LOGI(TAG, "引脚2收到 -> 网页: %s", data);
            
            // 把读到的串口数据群发给所有连上的网页
            send_uart_data_to_websockets(data, rxBytes);
        }
    }
    free(data);
}

// ================== WebSocket 收到网页数据 ==================
static esp_err_t ws_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "WebSocket 客户端已连接");
        return ESP_OK;
    }

    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) return ret;

    if (ws_pkt.len) {
        uint8_t *buf = calloc(1, ws_pkt.len + 1);
        if (buf == NULL) return ESP_ERR_NO_MEM;
        ws_pkt.payload = buf;

        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "网页发来 -> 引脚1: %s", ws_pkt.payload);
            // 真实写入物理引脚 1 (TX)
            uart_write_bytes(UART_PORT_NUM, (const char*)ws_pkt.payload, ws_pkt.len);
            
            // 注意：这里我们删掉了以前的 httpd_ws_send_frame (Echo)
            // 现在只有真正从引脚2收到数据，网页才会显示 [RX]
        }
        free(buf);
    }
    return ret;
}

static const httpd_uri_t ws_uri = {
    .uri        = "/ws",
    .method     = HTTP_GET,
    .handler    = ws_handler,
    .user_ctx   = NULL,
    .is_websocket = true
};

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 1. 初始化串口
    uart_init();

    // 2. 启动热点
    wifi_init_softap();

    // 3. 启动 Web 服务器，并赋值给全局变量
    global_server = start_webserver_with_html();

    if (global_server) {
        httpd_register_uri_handler(global_server, &ws_uri);
        
        // 4. 创建后台多线程任务，专门盯着串口接收
        xTaskCreate(uart_rx_task, "uart_rx_task", 4096, NULL, 5, NULL);
        
        ESP_LOGI(TAG, "Web-to-UART 桥接器双向透传已就绪！");
    }
}