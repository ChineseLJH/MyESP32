#include <stdio.h>
#include "concurrency_testing.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void app_main(void)
{

    vTaskDelay(pdMS_TO_TICKS(1000));

    start_smp_test();
}