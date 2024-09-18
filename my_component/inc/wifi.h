#ifndef _WIFI_H_
#define _WIFI_H_
#include "group.h"
#include "rgb.h"
/**
 * @fn init_wifi
 * @brief 初始化WiFi 并连接
*/
void wifi_init(void);

/**
 * @fn wifi_connect
 * @brief 链接WiFi
*/
void wifi_connect(void);
#endif
