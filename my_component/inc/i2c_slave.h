#ifndef _I2C_SLAVE_H_
#define _I2C_SLAVE_H_
#include "group.h"
#include "rgb.h"
#include "spi_ffs.h"
#include "i2c_master.h"

#define WIFICONFIG      0x00///<wifi配置
#define IOTCONFIG       0x01///<iot平台配置
#define MQTTCONFIG      0x02///<mqtt配置
#define TOPICCONFIG     0x03///<主题配置
#define SENDEND         0x04///<传输完成
#define RMOVEFILD       0x05 ///<清楚全部配置


/**
 * @fn init_i2c_slave
 * @brief 初始化I2C从模式
*/
esp_err_t init_i2c_slave(void);

void updateStringMember(char** dest, const char* src);


#endif
