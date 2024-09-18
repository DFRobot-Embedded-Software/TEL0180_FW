#ifndef _I2C_MASTER_H_
#define _I2C_MASTER_H_
#include "group.h"
#include "rgb.h"
#include "wifi.h"
/**
 * @fn initI2CMaster
 * @brief 初始化I2C主机
*/
esp_err_t init_i2c_master(uint8_t state);

void i2c_read_data(void);


#endif
