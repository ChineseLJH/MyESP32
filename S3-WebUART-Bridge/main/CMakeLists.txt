idf_component_register(SRCS "ws_echo_server.c" "web_uart_helper.c"
                    PRIV_REQUIRES esp_http_server esp_wifi nvs_flash esp_eth esp_driver_uart esp_driver_gpio
                    INCLUDE_DIRS ".")
