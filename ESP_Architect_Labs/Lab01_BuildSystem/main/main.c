#include <stdio.h>
#include "my_math.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
void app_main()
{
    printf("--- Rehab Day 1 Success! ---\n");
    printf("1+2=%d\n",My_add(1,2));
    while(1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
        printf("hello\n");
    }
}