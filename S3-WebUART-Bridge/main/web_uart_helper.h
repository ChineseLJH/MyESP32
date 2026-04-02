#ifndef WEB_UART_HELPER_H
#define WEB_UART_HELPER_H

#include <esp_http_server.h>

// 启动 SoftAP 热点
void wifi_init_softap(void);

// 启动 Web 服务器并注册 HTML 主页，返回服务器句柄
httpd_handle_t start_webserver_with_html(void);

#endif