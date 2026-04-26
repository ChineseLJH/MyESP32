#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化底层的 NVS 和 Wi-Fi，阻塞直到获取到 IP 地址
 */
void bsp_network_init(void);

#ifdef __cplusplus
}
#endif