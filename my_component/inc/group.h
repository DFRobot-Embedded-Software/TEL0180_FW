#ifndef _GROUP_H_
#define _GROUP_H_
#include <stdio.h>
#include "esp_log.h"
#include "driver/i2c.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdlib.h>
#include "driver/gpio.h"
#include "esp_spiffs.h"
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_wps.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "mqtt_client.h"

#define I2C_NUM  I2C_NUM_0

#define I2C_SLAVE_SCL_IO 38               /*!< gpio number for i2c slave clock */
#define I2C_SLAVE_SDA_IO 34               /*!< gpio number for i2c slave data */
#define DATA_LENGTH 512                  /*!< Data buffer length of test buffer */
#define I2C_TX_BUF_LEN (2 * DATA_LENGTH)              /*!< I2C slave tx buffer size */
#define I2C_RX_BUF_LEN (2 * DATA_LENGTH)              /*!< I2C slave rx buffer size */
#define RW_LENGTH 128               /*!< Data length for r/w test, [0,DATA_LENGTH] */

#define MAX_NODES  20
#define I2C_MASTER_HZ   100000///<100K

//定义链表结构体
typedef struct Topic
{
    char* topic;//topic数据
    struct Topic* next;//指向下一个节点指针
}Topic;

typedef struct{
    char* wifiName;//WiFi名字
    char* wifiPWD;//WiFi 密码
    char* iot;//平台链接
    char* QOS;//SIOTV2 需要使用
    char* MQTTID;//MQTT id
    char* MQTTPWD;//MQTT 密码
    Topic* head;   // 指向链表头节点的指针
    Topic* tail;   // 指向链表尾节点的指针
    Topic topic[MAX_NODES];  // 节点对象池
    int nodeCount;      // 节点计数器

}sConfigData;
//初始化链表
void initLinkedList(sConfigData* myStruct);

Topic* allocateNode(sConfigData* myStruct);
void insertNode(sConfigData* myStruct, char* newData);
void checkAndInsert(sConfigData* myStruct, char* newData);
void freeLinkedList(sConfigData* myStruct);

#endif
