
#include "i2c_master.h"
#include "mqtt.h"
#include "esp_sleep.h"
#include "esp_pm.h"
#include "http.h"

static const char *TAG = "i2c_master";

#define RP2040_SCI_ADDR_0X21      0x21 ///< default I2C address
#define RP2040_SCI_ADDR_0X22      0x22
#define RP2040_SCI_ADDR_0X23      0x23
#define READ_BIT I2C_MASTER_READ                /*!< I2C master read */
#define WRITE_BIT I2C_MASTER_WRITE              /*!< I2C master write */
#define ACK_CHECK_EN 0x1                        /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS 0x0                       /*!< I2C master will not check ack from slave */
#define ACK_VAL 0x0                             /*!< I2C ack value */
#define NACK_VAL 0x1                            /*!< I2C nack value */
#define TIME_OUT         2500                   ///<ms

#define CMD_GET_REFRESH_TIME 0x20 //获取采样时间间隔     
#define CMD_GET_INFORMATION  0x0D //获取全部数据
#define CMD_RESET            0x14

#define  CONFIG_EXAMPLE_MAX_CPU_FREQ_MHZ 1000000*160

#define  CONFIG_EXAMPLE_MIN_CPU_FREQ_MHZ 1000000*40

static uint8_t addr = RP2040_SCI_ADDR_0X21;

// 声明信号量句柄
SemaphoreHandle_t xSemaphore = NULL;

extern uint8_t mqttConnected;
extern sConfigData* pConfig;

extern esp_mqtt_client_handle_t client;

//extern QueueHandle_t xQueue;

uint16_t len = 0;
static void __attribute__((unused)) i2c_master_write_reset(i2c_port_t i2c_num,uint8_t cmdData){
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        //写命令
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addr << 1) | WRITE_BIT, ACK_CHECK_EN);
        i2c_master_write_byte(cmd,CMD_RESET,ACK_CHECK_EN);
        i2c_master_write_byte(cmd,1,ACK_CHECK_EN);
        i2c_master_write_byte(cmd,0,ACK_CHECK_EN);
        i2c_master_write_byte(cmd,cmdData,ACK_CHECK_EN);
        i2c_master_stop(cmd);
        esp_err_t ret = i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_RATE_MS);
        if(ret != ESP_OK){
            setLed(1,0,1);//紫色色，SCI通信错误
            ESP_LOGE(TAG,"I2C write error!");
        }
        i2c_cmd_link_delete(cmd);
}
static uint8_t* __attribute__((unused)) i2c_master_read_refresh_rate(i2c_port_t i2c_num, uint8_t cmdData)
{
    uint8_t buf[4];
    
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if(cmdData == CMD_GET_INFORMATION){
        //写命令
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addr << 1) | WRITE_BIT, ACK_CHECK_EN);
        i2c_master_write_byte(cmd,cmdData,ACK_CHECK_EN);
        i2c_master_write_byte(cmd,2,ACK_CHECK_EN);
        i2c_master_write_byte(cmd,0,ACK_CHECK_EN);
        i2c_master_write_byte(cmd,7,ACK_CHECK_EN);
        i2c_master_write_byte(cmd,0,ACK_CHECK_EN);
        i2c_master_stop(cmd);
    }else{
        //写命令
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addr << 1) | WRITE_BIT, ACK_CHECK_EN);
        i2c_master_write_byte(cmd,cmdData,ACK_CHECK_EN);
        i2c_master_write_byte(cmd,0,ACK_CHECK_EN);
        i2c_master_write_byte(cmd,0,ACK_CHECK_EN);
        i2c_master_stop(cmd);

    }
    
    esp_err_t ret = i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_RATE_MS);
    if(ret != ESP_OK){
        ESP_LOGE(TAG,"I2C write error!");
        return NULL;
    }
    i2c_cmd_link_delete(cmd);

    vTaskDelay(300/portTICK_RATE_MS);

    //判断数据是否正常和获取返回数据整理长度
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | READ_BIT, ACK_CHECK_EN);
    i2c_master_read(cmd, buf, 3, ACK_VAL);
    i2c_master_read_byte(cmd, &buf[3], NACK_VAL);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_RATE_MS);
    if(ret != ESP_OK){
        ESP_LOGE(TAG,"I2C read error!");
        return NULL;
    }
    i2c_cmd_link_delete(cmd);

    if(buf[1] != cmdData){
        ESP_LOGE(TAG,"buf:%d",buf[0]);
        ESP_LOGE(TAG,"buf:%d",buf[1]);
        ESP_LOGE(TAG,"buf:%d",buf[2]);
        setLed(1,0,1);//紫色，SCI通信错误
        i2c_master_write_reset(i2c_num,cmdData);
        vTaskDelay(1000/portTICK_RATE_MS);
        ESP_LOGE(TAG,"read cmdData error!!");
        return NULL;
    }

    len = buf[3]<< 8 | buf[2];
    if(len == 0){
        return NULL;
    }
    ESP_LOGI(TAG,"data len:%d",len);
    uint8_t* data_rd = (uint8_t*)malloc(sizeof(uint8_t)*len);

    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | READ_BIT, ACK_CHECK_EN);
    if (len > 1) {
        i2c_master_read(cmd, data_rd, len - 1, ACK_VAL);
    }
    i2c_master_read_byte(cmd, data_rd + len - 1, NACK_VAL);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_RATE_MS);
    if(ret != ESP_OK){
        ESP_LOGE(TAG,"I2C read data error!");
        free(data_rd);
        return NULL;
    }
    i2c_cmd_link_delete(cmd);
    return data_rd;
}



static void i2c_master(void *master){
    TickType_t xLastWakeTime;
    uint8_t qos = 0;
    uint8_t stateWif = 0;
    uint8_t stateWhile = 0;
    TickType_t xFrequency = pdMS_TO_TICKS(1000*5);
    char token1[100];
    do{//检测同SCI通信
        uint8_t* data = i2c_master_read_refresh_rate(I2C_NUM,CMD_GET_REFRESH_TIME);
        
        if(data != NULL){
            ESP_LOGI(TAG,"data: %d",data[0]);
            switch(data[0]){
                case 6:
                    xFrequency = pdMS_TO_TICKS(1000*60); // 指定任务运行的周期为 1分钟
                    free(data);
                    stateWhile = 1;
                break;
                case 5:
                    xFrequency = pdMS_TO_TICKS(1000*30); // 指定任务运行的周期为 30S
                    free(data);
                    stateWhile = 1;
                break;
                case 4:
                    xFrequency = pdMS_TO_TICKS(1000*10); // 指定任务运行的周期为 10S
                    free(data);
                    stateWhile = 1;
                break;
                case 7://5分钟
                    xFrequency = pdMS_TO_TICKS(1000*300); // 指定任务运行的周期为 5分钟
                    free(data);
                    stateWhile = 2;
                break;
                case 8:
                    xFrequency = pdMS_TO_TICKS(1000*600); // 指定任务运行的周期为 10分钟
                    free(data);
                    stateWhile = 3;
                break;
                default:
                    xFrequency = pdMS_TO_TICKS(1000*5); // 指定任务运行的周期为 5S
                    free(data);
                    stateWhile = 1;
                break;
            }
        }
        ESP_LOGE(TAG,"SCI Communication failure");
        //setLed(1,0,1);//青色，SCI通信错误
    }while(stateWhile == 0);
     ESP_LOGI(TAG,"time: %d",xFrequency);

    if(strcmp(pConfig->QOS,"0") == 0){
        ESP_LOGI(TAG,"no qos");
        qos = 0;
    }else {
        ESP_LOGI(TAG,"qos"); 
        qos = 1;
    }
    esp_pm_config_esp32s2_t pm_config = {
                        .max_freq_mhz = CONFIG_EXAMPLE_MAX_CPU_FREQ_MHZ,
                        .min_freq_mhz = CONFIG_EXAMPLE_MIN_CPU_FREQ_MHZ,
                        .light_sleep_enable = true,
                     };
    esp_pm_configure(&pm_config);
    wifi_init();
    wifi_connect();
    ESP_LOGI(TAG,"start");
    // 初始化任务的运行时间
    //xLastWakeTime = xTaskGetTickCount();
    while(true){
        if(mqttConnected == 1){
            setLed(1,0,1);//紫色，SCI通信错误
            uint8_t* data = i2c_master_read_refresh_rate(I2C_NUM,CMD_GET_INFORMATION);
            if(data != NULL){
                char* data_ = (char*)malloc(len+1);
                data_[len] = '\0';
                memcpy(data_,data,len);
                free(data);
                char *token = strtok(data_, ",");
                Topic* current = pConfig->head;
                while(current != NULL){
                    if(token!= NULL){
                        //setLed(1,0,1);//紫色
                        strcpy(token1,token);
                        token1[strlen(token)] = '\0';
                        ESP_LOGI(TAG,"data:%s",token1);
                        char* token2 = strstr(token1, ":");
                        token2 = token2+1;
                        uint8_t len1 = strlen(token2);
                        ESP_LOGI(TAG,"len1:%d",len1);
                        ESP_LOGI(TAG,"data:%s",token2);
                        char* token3 = strstr(token2, " ");
                        uint8_t len2 = strlen(token3);
                        ESP_LOGI(TAG,"len2:%d",len2);
                        ESP_LOGI(TAG,"data:%s",token3);
                        memcpy(token1,token2,len1 - len2);
                        token1[len1 - len2] = '\0';
                        
                        char *topic = strstr(current->topic, ":");
                        topic = topic+1;
                        ESP_LOGI(TAG,"data:%s",token1);
                        ESP_LOGI(TAG,"topic:%s",topic);
                        if(qos != 1){
                            ESP_LOGI(TAG,"no qos");
                            esp_mqtt_client_publish(client, (const char*)topic, (const char*)token1, strlen(token1), 0, 0);
                        }else{
                             ESP_LOGI(TAG,"qos");
                            esp_mqtt_client_publish(client,  (const char*)topic, (const char*)token1, strlen(token1), 1, 1);
                        }
                    }
                    ESP_LOGI(TAG,"ff");
                    token = strtok(NULL, ",");
                    vTaskDelay(100/portTICK_RATE_MS);
                    current = current->next;
                }
                free(data_);
                setLed(0,0,0);//青色，SCI通信错误
                vTaskDelay(xFrequency);
                //vTaskDelayUntil(&xLastWakeTime, xFrequency);
            }else{
                //setLed(0,0,1);//青色，SCI通信错误
                free(data);
                vTaskDelay(100);
            }
            ESP_LOGI(TAG,"time");
            //ESP_LOGE(TAG,"%s",data);
        }
        

    }
    //配置wifi 和mqtt
    vTaskDelete(NULL);

}


static void begin_sci1(void *sci){
    ESP_LOGI(TAG,"begin_sci");
    //uint8_t cmdData = 3;
    while(true){
        if(addr > RP2040_SCI_ADDR_0X23){
            ESP_LOGI(TAG,"break");
            addr = RP2040_SCI_ADDR_0X21;
        }
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addr << 1) | READ_BIT, ACK_CHECK_EN);
        i2c_master_stop(cmd);
        esp_err_t ret = i2c_master_cmd_begin(I2C_NUM, cmd, 1000 / portTICK_RATE_MS);
        if(ret == ESP_OK){
            i2c_cmd_link_delete(cmd);
            ESP_LOGI(TAG,"addr:%d",addr);
            vTaskDelay(10/portTICK_RATE_MS);
            i2c_read_data();
            vTaskDelete(NULL);
        }
        i2c_cmd_link_delete(cmd);
        addr += 1;
        vTaskDelay(500/portTICK_RATE_MS);
    }
    vTaskDelete(NULL);
}

static void begin_sci(void *sci){
    ESP_LOGI(TAG,"begin_sci");
    struct stat st;
    // 创建信号量
    xSemaphore = xSemaphoreCreateBinary();
    //uint8_t cmdData = 3;
    while(true){
        if(addr > RP2040_SCI_ADDR_0X23){
            ESP_LOGI(TAG,"break");
            break;
            //addr = RP2040_SCI_ADDR_0X21;
        }
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addr << 1) | READ_BIT, ACK_CHECK_EN);
        i2c_master_stop(cmd);
        esp_err_t ret = i2c_master_cmd_begin(I2C_NUM, cmd, 1000 / portTICK_RATE_MS);
        if(ret == ESP_OK){
            i2c_cmd_link_delete(cmd);
            ESP_LOGI(TAG,"addr:%d",addr);
            if (stat("/wifi/wifi.txt", &st) == 0) {
            vTaskDelay(10/portTICK_RATE_MS);
            i2c_read_data();
            vTaskDelete(NULL);
            }else{
                ESP_LOGI(TAG,"NO fiel");
                break;
            }
        }
        i2c_cmd_link_delete(cmd);
        addr += 1;
        vTaskDelay(500/portTICK_RATE_MS);
    }
    initHTTPServer();//开始初始化HTTP
    vTaskDelay(10/portTICK_RATE_MS);
    while(true){
        if (xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE)
        {
            ESP_LOGI(TAG, "Task2: Semaphore received! Performing task...");
            // 执行需要同步的任务
            stop_httpd();
            vTaskDelete(NULL);
        }
        
    }
    vTaskDelete(NULL);
}

esp_err_t init_i2c_master(uint8_t state){
    ESP_LOGI(TAG,"init_i2c_master");
    
    if(state != 2){
        i2c_config_t conf = {
            .mode = I2C_MODE_MASTER,
            .sda_io_num = I2C_SLAVE_SDA_IO,
            .sda_pullup_en = GPIO_PULLUP_ENABLE,
            .scl_io_num = I2C_SLAVE_SCL_IO,
            .scl_pullup_en = GPIO_PULLUP_ENABLE,
            .master.clk_speed = I2C_MASTER_HZ,
            // .clk_flags = 0,          /*!< Optional, you can use I2C_SCLK_SRC_FLAG_* flags to choose i2c source clock here. */
        };
        esp_err_t err = i2c_param_config(I2C_NUM, &conf);
        if (err != ESP_OK) {
            return err;
        }
        i2c_driver_install(I2C_NUM, conf.mode, I2C_RX_BUF_LEN, I2C_TX_BUF_LEN, 0);
    }
    
    if(state == 1){
        setLed(0,0,1);//蓝色
        xTaskCreate(begin_sci,"begin_sci",1024*8,NULL,10,NULL);//检测SCI
    }else{
        setLed(0,1,0);//绿色
        xTaskCreate(begin_sci1,"begin_sci1",1024*8,NULL,10,NULL);//检测SCI
    }
    return ESP_OK;

}

void i2c_read_data(void){
    xTaskCreate(i2c_master, "i2c_master", 1024 * 4, NULL, 5, NULL);//检测到设别地址，开启I2C从机任务
}