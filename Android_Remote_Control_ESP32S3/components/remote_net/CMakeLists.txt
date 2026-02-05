idf_component_register(
    SRC_DIRS "src"  # 添加新.c需要 idf.py reconfigure
    INCLUDE_DIRS "include"
    PRIV_REQUIRES nvs_flash esp_wifi esp_event lwip
)