idf_component_register(
    SRCS "src/bsp_network.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES esp_wifi nvs_flash
)