#pragma once
#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct{
    uint32_t magic_id;
    uint16_t config_ver;
    uint8_t flag;
} sys_config_t;

esp_err_t sys_storage_init(void);
esp_err_t sys_storage_save(const sys_config_t *cfg);
esp_err_t sys_storage_load(sys_config_t *cfg);

#ifdef __cplusplus
}
#endif