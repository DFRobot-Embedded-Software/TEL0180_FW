#include <stdio.h>
#include "rgb.h"
#include "i2c_slave.h"
#include "spi_ffs.h"
#include "group.h"
#include "http.h"
static const char *TAG = "main";

extern sConfigData* pConfig;
//extern QueueHandle_t xQueue;

// static void delete_task(void *delete){
//     uint8_t cmdData = 0;
//     while(true){
//         if(xQueueReceive(xQueue, &cmdData, portMAX_DELAY)){
//             switch(cmdData){
//                 case 1://删除任务
//                 case 2:
//                     ESP_LOGI(TAG,"close i2c slave");
//                     init_i2c_master();
//                 break;
//                 case 3:
//                     ESP_LOGI(TAG,"close begin sci");
//                     i2c_read_data();
//                 break;
//                 default:
//                 break;
//             }
//         }
//         vTaskDelay(1);
//     }
//     vTaskDelete(NULL); 
// }

void app_main(void)
{
    //xQueue = xQueueCreate(5, sizeof(uint8_t));
    initLinkedList(pConfig);
    rgb_init();//初始化RGB灯
    init_spi_ffs();//初始化FFS文件系统
    parsingFile();//解析文件
    setLed(1,1,1);//默认为白灯，进入从机模式
    init_i2c_slave();//初始化I2C从机模式
    //xTaskCreate(delete_task, "delete_task", 1024*2, NULL, 10, NULL);//将FIFO中数据读取到用户空间

}
