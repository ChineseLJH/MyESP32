#include <stdio.h>
#include "remote_net.h"
#include "esp_log.h"

void app_main(void)
{
    remote_net_start();

    ESP_LOGI("MAIN", "Remote Net Started");
}
