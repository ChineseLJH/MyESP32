#include "web_uart_helper.h"
#include <string.h>
#include <esp_wifi.h>
#include <esp_log.h>
#include <esp_mac.h>

static const char *TAG = "WEB_HELPER";

#define ESP_WIFI_SSID      "ESP32_S3_WebUART"
#define ESP_WIFI_PASS      "12345678"
#define MAX_STA_CONN       4

// ================== 1. 网页前端代码 ==================
static const char index_html[] = 
"<!DOCTYPE html>"
"<html>"
"<head>"
"<meta charset=\"utf-8\">"
"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
"<title>ESP32 WebUART</title>"
"<style>"
"  body { font-family: Arial, sans-serif; margin: 0; padding: 10px; background: #f0f0f0; }"
"  h2 { text-align: center; color: #333; margin-top: 0; }"
"  #terminal { width: 100%; height: 60vh; background: #1e1e1e; color: #0f0; padding: 10px; box-sizing: border-box; overflow-y: auto; font-family: monospace; border-radius: 5px; margin-bottom: 10px; }"
"  .input-area { display: flex; gap: 10px; }"
"  input[type=\"text\"] { flex: 1; padding: 10px; font-size: 16px; border: 1px solid #ccc; border-radius: 4px; }"
"  button { padding: 10px 20px; font-size: 16px; background: #007bff; color: white; border: none; border-radius: 4px; cursor: pointer; }"
"  button:active { background: #0056b3; }"
"</style>"
"</head>"
"<body>"
"  <h2>ESP32-S3 局域网串口助手</h2>"
"  <div id=\"terminal\"></div>"
"  <div class=\"input-area\">"
"    <input type=\"text\" id=\"cmd\" placeholder=\"输入要发送的指令...\">"
"    <button onclick=\"sendCmd()\">发送</button>"
"  </div>"
"  <script>"
"    var ws = new WebSocket('ws://' + location.host + '/ws');"
"    var term = document.getElementById('terminal');"
"    function log(msg, color) {"
"      term.innerHTML += '<div style=\"color:' + color + '\">' + msg + '</div>';"
"      term.scrollTop = term.scrollHeight;"
"    }"
"    ws.onopen = function() { log('[系统] WebSocket 连接成功!', '#0ff'); };"
"    ws.onclose = function() { log('[系统] WebSocket 连接已断开!', '#f00'); };"
"    ws.onerror = function() { log('[系统] WebSocket 发生错误!', '#f00'); };"
"    ws.onmessage = function(evt) { log('[RX] ' + evt.data, '#0f0'); };"
"    function sendCmd() {"
"      var input = document.getElementById('cmd');"
"      var cmd = input.value;"
"      if(cmd && ws.readyState === WebSocket.OPEN) {"
"        ws.send(cmd);"
"        log('[TX] ' + cmd, '#fff');"
"        input.value = '';"
"      }"
"    }"
"    document.getElementById('cmd').addEventListener('keypress', function(e) {"
"      if (e.key === 'Enter') sendCmd();"
"    });"
"  </script>"
"</body>"
"</html>";

static esp_err_t index_html_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, index_html, HTTPD_RESP_USE_STRLEN);
}

static const httpd_uri_t index_uri = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = index_html_handler,
    .user_ctx  = NULL
};

// ================== 2. 热点初始化逻辑 ==================
void wifi_init_softap(void) {
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = ESP_WIFI_SSID,
            .ssid_len = strlen(ESP_WIFI_SSID),
            .channel = 1,
            .password = ESP_WIFI_PASS,
            .max_connection = MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    if (strlen(ESP_WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "SoftAP 启动成功！");
}

// ================== 3. 启动服务器并挂载网页 ==================
httpd_handle_t start_webserver_with_html(void) {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &index_uri);
        return server;
    }
    return NULL;
}