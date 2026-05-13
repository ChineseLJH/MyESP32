#include "esp_heap_trace.h"

#define NUM_RECORDS 100
static heap_trace_record_t trace_record[NUM_RECORDS];

void app_main(void)
{
    int *ptr = NULL;
    *ptr = 42; // 往物理地址 0 写数据，必定触发 LoadStoreError
}