#include <string.h>
#include "freertos/FreeRTOS.h"
#include "lwip/sockets.h"

#include "bsp_network.h"
#include "tcp_server.h"
#include "sdkconfig.h"

void app_main(void)
{
    bsp_network_init();

    xTaskCreate(tcp_server_task, "tcp_server", CONFIG_RAW_NETSTACK_TASK_STACK_SIZE, (void *)AF_INET, CONFIG_RAW_NETSTACK_TASK_PRIORITY, NULL);
}
