#include "sys_storage.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>

// static const char *TAG="SYS_STORE";

#define PARTITION_NAME "storage"     // 对应的 CSV 分区名字
#define NAMESPACE_NAME "storage_ns"  // 分区中的一个 namespace 名字
#define KEY_NAME "sys_cfg"           // namespace 中的一个 key 的名字

// 结构体拆解成字符数组
static void serialize_internal(const sys_config_t *src,uint8_t *buffer)
{
    buffer[0]=(uint8_t)(src->magic_id & 0xFF);
    buffer[1]=(uint8_t)((src->magic_id>>8) & 0xFF);
    buffer[2]=(uint8_t)((src->magic_id>>16) & 0xFF);
    buffer[3]=(uint8_t)((src->magic_id>>24) & 0xFF);

    buffer[4]=(uint8_t)(src->config_ver & 0xFF);
    buffer[5]=(uint8_t)((src->config_ver>>8) & 0xFF);

    buffer[6]=src->flag;
}

// 字符数组合并成结构体
static void deserialize_internal(const uint8_t *buffer,sys_config_t *dest)
{
    dest->magic_id=(uint32_t)buffer[0]|
                   (uint32_t)buffer[1]<<8|
                   (uint32_t)buffer[2]<<16|
                   (uint32_t)buffer[3]<<24;

    dest->config_ver=(uint16_t)buffer[4]|
                     (uint16_t)buffer[5];

    dest->flag=buffer[6];

}

esp_err_t sys_storage_init(void)
{
    esp_err_t err=nvs_flash_init_partition(PARTITION_NAME);

    if(err==ESP_ERR_NVS_NO_FREE_PAGES||err==ESP_ERR_NVS_NEW_VERSION_FOUND){
        ESP_ERROR_CHECK(nvs_flash_erase_partition(PARTITION_NAME));
        err=nvs_flash_init_partition(PARTITION_NAME);
    }

    // ESP_LOGI(TAG,"Init function called (Skeleton)");
    
    return err;
}

esp_err_t sys_storage_save(const sys_config_t *cfg)
{
    nvs_handle_t my_handle;
    esp_err_t err;
    uint8_t buffer[7];

    serialize_internal(cfg,buffer);

    err=nvs_open_from_partition(PARTITION_NAME,NAMESPACE_NAME,NVS_READWRITE,&my_handle);
    if(err!=ESP_OK) return err;

    err=nvs_set_blob(my_handle,KEY_NAME,buffer,sizeof(buffer));

    if(err==ESP_OK) err=nvs_commit(my_handle);

    nvs_close(my_handle);

    // ESP_LOGI(TAG,"Save function called. Magic: 0x%08lX",cfg->magic_id);

    return err;
}

esp_err_t sys_storage_load(sys_config_t *cfg)
{
    // ESP_LOGI(TAG,"Load function called (Skeleton)");
    // cfg->magic_id=0x12345678;

    nvs_handle_t my_handle;
    esp_err_t err;
    uint8_t buffer[7];
    size_t len=sizeof(buffer);

    err=nvs_open_from_partition(PARTITION_NAME,NAMESPACE_NAME,NVS_READONLY,&my_handle);
    if(err!=ESP_OK) return err;

    err=nvs_get_blob(my_handle,KEY_NAME,buffer,&len);

    if(err==ESP_OK) deserialize_internal(buffer,cfg);

    nvs_close(my_handle);

    return err;
}