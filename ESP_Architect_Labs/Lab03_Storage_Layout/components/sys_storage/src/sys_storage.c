#include "sys_storage.h"
#include "esp_log.h"

static const char *TAG="SYS_STORE";

esp_err_t sys_storage_init(void)
{
    ESP_LOGI(TAG,"Init function called (Skeleton)");
    
    return ESP_OK;
}

esp_err_t sys_storage_save(const sys_config_t *cfg)
{
    ESP_LOGI(TAG,"Save function called. Magic: 0x%08lX",cfg->magic_id);

    return ESP_OK;
}

esp_err_t sys_storage_load(sys_config_t *cfg)
{
    ESP_LOGI(TAG,"Load function called (Skeleton)");

    cfg->magic_id=0x12345678;
    return ESP_OK;
}