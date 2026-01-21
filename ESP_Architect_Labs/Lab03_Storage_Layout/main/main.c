#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "sys_storage.h"

static const char *TAG="MAIN";

void app_main(void)
{
    ESP_LOGI(TAG,"Syetem Starting...");

    ESP_ERROR_CHECK(sys_storage_init());

    sys_config_t tx_data={
        .magic_id=0xAABBCCDD,
        .config_ver=1,
        .flag=0xFF
    };

    ESP_LOGI(TAG,"Attempting to save config...");
    if(sys_storage_save(&tx_data)==ESP_OK)
    {
        ESP_LOGI(TAG,"Success");
    }
    else
    {
        ESP_LOGE(TAG,"Failed");
    }

    sys_config_t rx_data={0};
    ESP_LOGI(TAG,"Attempting to load config...");

    if(sys_storage_load(&rx_data)==ESP_OK)
    {
        ESP_LOGI(TAG,"Load API returned Success.");
        ESP_LOGI(TAG,"Read Data->Magic:0x%08lX",rx_data.magic_id);
    }
    else
    {
        ESP_LOGE(TAG,"Load API failed!");
    }

    while(1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}