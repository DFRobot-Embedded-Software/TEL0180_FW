#include "i2c_slave.h"

static const char *TAG = "i2c_slave";
#define ESP_SLAVE_ADDR 0x16 /*!< ESP32 slave address, you can set any 7bit value */

static QueueHandle_t xQueueI2CBUF = NULL;//数据接收队列
static TaskHandle_t I2CReadTaskHandle = NULL;//I2C接收任务句柄
static TaskHandle_t timeTaskHandle = NULL;//定时任务句柄
static TaskHandle_t I2CManageTaskHandle = NULL;//I2C解析任务句柄

uint8_t receiveState = 0;//接收数据标志位
extern sConfigData* pConfig;
//extern QueueHandle_t xQueue;

typedef struct{
    uint8_t len;
    uint8_t buf[RW_LENGTH];
}sQueueData;

static void i2c_slave_read_task(void *xQueueI2CBUF)
{
    QueueHandle_t _xQueueI2CBUF = (QueueHandle_t)xQueueI2CBUF;
    sQueueData data;
    uint8_t readData[RW_LENGTH];
    uint16_t size;
    while(true){
        size = i2c_slave_read_buffer(I2C_NUM, readData, RW_LENGTH, 1);//将FIFO中数读取到用户端

        if(size != 0){
            data.len = size-1;
            memcpy(data.buf, readData, size);
            xQueueSend(_xQueueI2CBUF, &data, portMAX_DELAY);
        }
        vTaskDelay(1);
    }
    vTaskDelete(NULL);
}

// 定义函数，用于更新结构体中的字符串成员
void updateStringMember(char** dest, const char* src) {
    // 释放先前分配的内存
    if (*dest != NULL) {
        free(*dest);
    }
    // 复制数据并分配内存
    *dest = strdup(src);
}


static void i2c_slave_manage_task(void *xQueueI2CBUF)
{
    QueueHandle_t _xQueueI2CBUF = (QueueHandle_t)xQueueI2CBUF;
    sQueueData data;
    //uint8_t cmdData = 1;
    while(true){
        if(xQueueReceive(_xQueueI2CBUF, &data, portMAX_DELAY)){
            receiveState = 1;
            
            switch(data.buf[0]){
                case WIFICONFIG://修改WiFi配置
                    ESP_LOGI(TAG,"config WIFI");
                    char* wifiCacheBuf = (char*)malloc(sizeof(char)*(data.len +1));
                    if(wifiCacheBuf == NULL){
                        ESP_LOGE(TAG,"cacheBuf malloc error");
                        break;
                    }
                    memcpy(wifiCacheBuf,&data.buf[1],data.len);
                    wifiCacheBuf[data.len]='\0';
                    char* wifiRes = strtok(wifiCacheBuf, ":");
                    updateStringMember(&pConfig->wifiName,wifiRes);
                    // if(pConfig->wifiName != NULL){
                    //     free(pConfig->wifiName);
                    //     pConfig->wifiName = strdup(wifiRes);//复制数据并分配内存
                    // }else{
                    //     pConfig->wifiName = strdup(wifiRes);//复制数据并分配内存
                    // }
                    wifiRes = strtok(NULL, ":");
                    updateStringMember(&pConfig->wifiPWD,wifiRes);
                    // if(pConfig->wifiPWD != NULL){
                    //     free(pConfig->wifiPWD);
                    //     pConfig->wifiPWD =strdup(wifiRes);
                    // }else{
                    //     pConfig->wifiPWD =strdup(wifiRes);
                    // }
                    free(wifiCacheBuf);
                    ESP_LOGI(TAG,"wifi name: %s,wifi pwd:%s",pConfig->wifiName,pConfig->wifiPWD);
                    
                break;
                case IOTCONFIG://修改iot平台配置
                    ESP_LOGI(TAG,"config IOT");
                    char* iotCacheBuf = (char*)malloc(sizeof(char)*(data.len+1));
                    if(iotCacheBuf == NULL){
                        ESP_LOGE(TAG,"cacheBuf malloc error");
                    }
                    memcpy(iotCacheBuf,&data.buf[1],data.len);
                    iotCacheBuf[data.len]='\0';
                    char* iotRes = strtok(iotCacheBuf, ":");
                    updateStringMember(&pConfig->iot,iotRes);
                    iotRes = strtok(NULL, ":");
                    updateStringMember(&pConfig->QOS,iotRes);
                    //pConfig->QOS = strdup(iotRes);
                    free(iotCacheBuf);
                    ESP_LOGI(TAG,"iot: %s,QOS:%s",pConfig->iot,pConfig->QOS);
                break;
                case MQTTCONFIG://修改mqtt配置
                    ESP_LOGI(TAG,"config MQTT");
                    char* mqttCacheBuf = (char*)malloc(sizeof(char)*(data.len+1));
                    if(mqttCacheBuf == NULL){
                        ESP_LOGE(TAG,"cacheBuf malloc error");
                    }
                    memcpy(mqttCacheBuf,&data.buf[1],data.len);
                    mqttCacheBuf[data.len]='\0';
                    char* mqttRes = strtok(mqttCacheBuf, ":");
                    updateStringMember(&pConfig->MQTTID,mqttRes);
                    mqttRes = strtok(NULL, ":");
                    updateStringMember(&pConfig->MQTTPWD,mqttRes);
                    free(mqttCacheBuf);
                    ESP_LOGI(TAG,"MQTTID: %s,MQTTPWD:%s",pConfig->MQTTID,pConfig->MQTTPWD);
                break;
                case TOPICCONFIG://修改主题配置
                    ESP_LOGI(TAG,"config topic");
                    char* topCacheBuf = (char*)malloc(sizeof(char)*(data.len+1));
                    if(topCacheBuf == NULL){
                        ESP_LOGE(TAG,"cacheBuf malloc error");
                    }
                    memcpy(topCacheBuf,&data.buf[1],data.len);
                    topCacheBuf[data.len]='\0';
                    if(pConfig->head != NULL){
                        checkAndInsert(pConfig,topCacheBuf);
                    }else{
                        insertNode(pConfig,topCacheBuf);
                    }
                    free(topCacheBuf);
                    // for(uint8_t i = 1;i < data.len;i++){
                    //     printf("%c",data.buf[i]);
                    // }
                    // printf("\n");
                break;
                case SENDEND://配置传输完成
                    vTaskDelete(timeTaskHandle);//接收配置命令，结束计时
                    vTaskDelay(10/portTICK_RATE_MS);
                    vTaskDelete(I2CReadTaskHandle);//销毁I2C读取任务
                    vTaskDelay(10/portTICK_RATE_MS);
                    esp_err_t err = i2c_driver_delete(I2C_NUM);//销毁I2C从机模式
                    if(err != ESP_OK){//I2C 从机销毁失败
                        ESP_LOGE(TAG,"i2c driver delete error !!!");
                        // while(1){//红灯连续闪烁
                        //     setLed(1,0,0);
                        //     vTaskDelay(100/portTICK_RATE_MS);
                        //     setLed(0,0,0);
                        //     vTaskDelay(100/portTICK_RATE_MS);
                        // }
                    }
                    writeFile();//开始写入配置文件
                    //初始化主机模式
                    vTaskDelay(10/portTICK_RATE_MS);
                    init_i2c_master(0);
                    vTaskDelay(10/portTICK_RATE_MS);
                    vTaskDelete(NULL);
                break;
                case RMOVEFILD:
                    remove("/wifi/wifi.txt");
                    freeLinkedList(pConfig);
                break;
                default:
                break;
            }
            

        }
        vTaskDelay(1);
    }
    
}


static void timed_task(void *time){
    //struct stat st;
    //uint8_t cmdData = 2;
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = pdMS_TO_TICKS(5000); // 指定任务运行的周期为 5000ms

    // 初始化任务的运行时间
    xLastWakeTime = xTaskGetTickCount();
    while(true){
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        if(receiveState == 0){
            //if (stat("/wifi/wifi.txt", &st) == 0) {//检测是否存在文件
                ESP_LOGI(TAG,"TIME END FILE");
                vTaskDelete(I2CReadTaskHandle);//销毁I2C读取任务
                vTaskDelay(10/portTICK_RATE_MS);
                esp_err_t err = i2c_driver_delete(I2C_NUM);//销毁I2C从机模式
                if(err != ESP_OK){//I2C 从机销毁失败
                     ESP_LOGI(TAG,"i2c driver delete error !!!");
                }
                vTaskDelete(I2CManageTaskHandle);//销毁I2C解析任务
                vTaskDelay(10/portTICK_RATE_MS);
                init_i2c_master(1);
                vTaskDelay(10/portTICK_RATE_MS);
                vTaskDelete(NULL);
            //}else{
                //ESP_LOGI(TAG,"TIME END");
            //}
        }
        
    }
}

esp_err_t init_i2c_slave(void){
    ESP_LOGI(TAG,"init_i2c_slave");
    xQueueI2CBUF = xQueueCreate(10, sizeof(sQueueData));//创建存储画面的队列
    i2c_config_t conf_slave = {
        .sda_io_num = I2C_SLAVE_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_SLAVE_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .mode = I2C_MODE_SLAVE,
        .slave.addr_10bit_en = 0,
        .slave.slave_addr = ESP_SLAVE_ADDR,
    };
    esp_err_t err = i2c_param_config(I2C_NUM, &conf_slave);
    if (err != ESP_OK) {
        return err;
    }
    err = i2c_driver_install(I2C_NUM, conf_slave.mode, I2C_RX_BUF_LEN, I2C_TX_BUF_LEN, 0);
    xTaskCreate(i2c_slave_read_task, "i2c_slave_read_task", 1024 * 2, (void*)xQueueI2CBUF, 10, &I2CReadTaskHandle);//将FIFO中数据读取到用户空间
    xTaskCreate(i2c_slave_manage_task, "i2c_slave_manage_task", 1024 * 4, (void*)xQueueI2CBUF, 10, &I2CManageTaskHandle);//处理用户空间输就
    xTaskCreate(timed_task, "timed_task", 1024 * 2, NULL, 10, &timeTaskHandle);//定时检测任务（3秒），如果在3秒内没有接收到配置信息且之前以有配置文件则进入主机模式


    return err;
}