idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES freertos esp_wifi nvs_flash esp_event esp_netif esp_common raw_netstack
)