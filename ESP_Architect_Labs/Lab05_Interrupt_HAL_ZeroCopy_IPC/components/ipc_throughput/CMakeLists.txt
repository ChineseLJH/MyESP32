idf_component_register(
    SRCS
        "src/ipc_naive.c"
        "src/ipc_zero_copy.c"
        "src/ipc_throughput.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES esp_timer freertos
)