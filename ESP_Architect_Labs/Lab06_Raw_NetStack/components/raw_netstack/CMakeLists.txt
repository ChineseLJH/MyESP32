idf_component_register(
    SRCS
        "src/protocol_parser.c"
        "src/tcp_server.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES esp_wifi nvs_flash
)