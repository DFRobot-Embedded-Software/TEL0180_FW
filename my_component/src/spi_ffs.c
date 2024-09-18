#include "spi_ffs.h"

static const char *TAG = "spiffs";

extern sConfigData* pConfig;

esp_err_t init_spi_ffs(void){

    esp_vfs_spiffs_conf_t conf = {
      .base_path = "/wifi",
      .partition_label = NULL,
      .max_files = 5,
      .format_if_mount_failed = true
    };

    // Use settings defined above to initialize and mount SPIFFS filesystem.
    // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGI(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGI(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGI(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return ret;
    }
    
    size_t total = 0, used = 0;
    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGI(TAG, "Failed to get SPIFFS partition information (%s). Formatting...", esp_err_to_name(ret));
        esp_spiffs_format(conf.partition_label);
        return ret;
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }
return ret;
}

void parsingFile(void){
    struct stat st;
    char line[64];
    uint8_t count= 1;
    if (stat("/wifi/wifi.txt", &st) == 0) {
        ESP_LOGI(TAG,"FILE");
        FILE* file = fopen("/wifi/wifi.txt", "r");
        if (file == NULL) {
            ESP_LOGE(TAG, "Failed to open file for writing");
        }
        while(fgets(line, sizeof(line), file) != NULL){//按行解析数据
            
            char* pos = strchr(line, '\n');
            if (pos) {
                *pos = '\0';
            }
            switch(count){
                case 1:
                    pConfig->wifiName = strdup(line);
                    count++;
                break;
                case 2:
                    pConfig->wifiPWD = strdup(line);
                    count++;
                break;
                case 3:
                    pConfig->iot = strdup(line);
                    count++;
                break;
                case 4:
                    pConfig->QOS = strdup(line);
                    count++;
                break;
                case 5:
                    pConfig->MQTTID = strdup(line);
                    count++;
                break;
                case 6:
                    pConfig->MQTTPWD = strdup(line);
                    count++;
                break;
                case 7:
                    insertNode(pConfig,line);
                break;
                default:
                break;
            }
            ESP_LOGE(TAG, "%s,count:%d",line,count);
        }
        
    }else{
        ESP_LOGI(TAG,"NO FILE");
    }
}

void writeFile(void){
    ESP_LOGI(TAG,"write file");
    remove("/wifi/wifi.txt");
    vTaskDelay(10/portTICK_RATE_MS);
    ESP_LOGI(TAG, "first write file");
    FILE* file = fopen("/wifi/wifi.txt", "w");
    if (file == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
    }
    if(pConfig->wifiName != NULL){//检测数据是否为空，防止第一次写入异常
        char* wifiNameBuf = addNewline(pConfig->wifiName);
        if(wifiNameBuf == NULL){
            ESP_LOGE(TAG, "add 0D 0A error");
            return;
        }
        
        fprintf(file,wifiNameBuf);
        free(wifiNameBuf);
    }
    if(pConfig->wifiPWD != NULL){
        char* wifiPWDBuf = addNewline(pConfig->wifiPWD);
        if(wifiPWDBuf == NULL){
            ESP_LOGE(TAG, "add 0D 0A error");
            return;
        }
        fprintf(file,wifiPWDBuf);
        free(wifiPWDBuf);
    }
    if(pConfig->iot != NULL){
       char* iotBuf = addNewline(pConfig->iot);
        if(iotBuf == NULL){
            ESP_LOGE(TAG, "add 0D 0A error");
            return;
        }
        fprintf(file,iotBuf);
        free(iotBuf);
    }
    if(pConfig->QOS != NULL){
        char* QOSBuf = addNewline(pConfig->QOS);
        if(QOSBuf == NULL){
            ESP_LOGE(TAG, "add 0D 0A error");
            return;
        }
        fprintf(file,QOSBuf);
        free(QOSBuf);
    }
    if(pConfig->MQTTID != NULL){
       char* MQTTIDBuf = addNewline(pConfig->MQTTID);
        if(MQTTIDBuf == NULL){
            ESP_LOGE(TAG, "add 0D 0A error");
            return;
        }
        fprintf(file,MQTTIDBuf);
        free(MQTTIDBuf);
    }
    if(pConfig->MQTTPWD != NULL){
        char* MQTTPWDBuf = addNewline(pConfig->MQTTPWD);
        if(MQTTPWDBuf == NULL){
            ESP_LOGE(TAG, "add 0D 0A error");
            return;
        }
        fprintf(file,MQTTPWDBuf);
        free(MQTTPWDBuf);
    }
    
    Topic* current = pConfig->head;
    while(current != NULL){
        char* topicBuf = addNewline(current->topic);
        if(topicBuf == NULL)
            return;
        fprintf(file,topicBuf);
        free(topicBuf);
        current = current->next;
    }
    fclose(file);
}

char* addNewline(char* str) {
    if (str == NULL) {
        printf("Error: Passed string is NULL\n");
        return NULL;
    }
    size_t len = strlen(str);
    char* result = (char*)malloc(len + 2); // 预留空间给 '\n' 和 '\0'
    if (result == NULL) {
        return NULL; // 内存分配失败
    }
    strcpy(result, str);
    result[len] = '\n'; // 在末尾添加换行符
    result[len + 1] = '\0'; // 确保字符串以 null 结尾
    return result;
}