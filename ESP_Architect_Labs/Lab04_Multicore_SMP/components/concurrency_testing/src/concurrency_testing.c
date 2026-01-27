#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "sdkconfig.h"
#include "concurrency_testing.h"

volatile int g_shared_counter=0;
volatile bool test_start_signal = false;

#if CONFIG_SMP_RACE_CONDITION_SPINLOCK
    static portMUX_TYPE my_spinlock=portMUX_INITIALIZER_UNLOCKED;
#endif

#if CONFIG_SMP_RACE_CONDITION_MUTEX
    static SemaphoreHandle_t my_mutex=NULL;
#endif

void worker_task(void *arg){
    const int loop_count=100000;
    int i;
    int temp;

    while (test_start_signal == false) {
        // 空转等待，什么都不做
        vTaskDelay(1);
    }

    int64_t start_time=esp_timer_get_time();

    for(i=0;i<loop_count;i++){
        #if CONFIG_SMP_RACE_CONDITION_SPINLOCK
            portENTER_CRITICAL(&my_spinlock);
        #endif

        #if CONFIG_SMP_RACE_CONDITION_MUTEX
            xSemaphoreTake(my_mutex,portMAX_DELAY);
        #endif


        temp = g_shared_counter;

        // 2. 故意延时 (Delay) - 把这个“窗口”撑大！
        // 让 CPU 在拿到旧值后，发呆一会儿，给另一个核可乘之机
        // "nop" 是汇编指令 No Operation (空操作)
        for(int j=0; j<50; j++) {
            __asm__ __volatile__("nop");
        }

        // 3. 修改 (Modify)
        temp = temp + 1;

        // 4. 写回 (Write)
        g_shared_counter = temp;

        #if CONFIG_SMP_RACE_CONDITION_SPINLOCK
            portEXIT_CRITICAL(&my_spinlock);
        #endif

        #if CONFIG_SMP_RACE_CONDITION_MUTEX
            xSemaphoreGive(my_mutex);
        #endif

    }

    int64_t end_time=esp_timer_get_time();
    int time_ms=(int)(end_time-start_time)/1000;
    printf("Core %d: Done! Added %d times. Cost: %d ms\n", xPortGetCoreID(), loop_count, time_ms);
    vTaskDelete(NULL);
}

void start_smp_test(void) {
    g_shared_counter = 0;

    test_start_signal = false;
    
    printf("-------------------------------------------------\n");
    printf("Starting SMP Race Condition Test...\n");

    #if CONFIG_SMP_RACE_CONDITION_NONE
        printf("Mode: NO PROTECTION (Expect Error)\n");
    #elif CONFIG_SMP_RACE_CONDITION_SPINLOCK
        printf("Mode: SPINLOCK (Expect Correct 200,000)\n");
    #elif CONFIG_SMP_RACE_CONDITION_MUTEX
        printf("Mode: MUTEX (Expect Correct 200,000)\n");
        
        if (my_mutex == NULL) {
            my_mutex = xSemaphoreCreateMutex();
        }
    #endif
    
    printf("-------------------------------------------------\n");

    xTaskCreatePinnedToCore(worker_task, "Worker_Core0", 2048, NULL, 5, NULL, 0);

    xTaskCreatePinnedToCore(worker_task, "Worker_Core1", 2048, NULL, 5, NULL, 1);

    for(int k=0; k<10000; k++) { __asm__ __volatile__("nop"); }

    printf("Start!!!\n");
    
    // === 核心修改：鸣枪！===
    test_start_signal = true;

    vTaskDelay(pdMS_TO_TICKS(5000));

    printf("-------------------------------------------------\n");
    printf("Final Result: g_shared_counter = %d\n", g_shared_counter);
    
    if (g_shared_counter == 200000) {
        printf("Status: SUCCESS (Thread Safe)\n");
    } else {
        printf("Status: FAILURE (Race Condition Detected!)\n");
        printf("Lost Counts: %d\n", 200000 - g_shared_counter);
    }
    printf("-------------------------------------------------\n");
}