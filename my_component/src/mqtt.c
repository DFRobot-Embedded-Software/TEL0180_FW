#include "mqtt.h"
#include "rgb.h"
static const char *TAG = "MQTT_event";
#define MQTT_DATA_LEN      100
char mqtt_host[MQTT_DATA_LEN]   = {0x00};               // mqtt 主机网址
char mqtt_iotID[MQTT_DATA_LEN]  = {0x00};               // mqtt iot id
char mqtt_iotPWD[MQTT_DATA_LEN] = {0x00};               // mqtt iot password
char mqtt_clientID[MQTT_DATA_LEN] = {0x00};             // mqtt client id len
uint32_t mqtt_port = 1883;                              // mqtt 端口
esp_mqtt_client_config_t mqtt_cfg;
esp_mqtt_client_handle_t client;

extern sConfigData* pConfig;
extern uint8_t mqttConnected;

static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            setLed(1,0,1);//紫色
            vTaskDelay(100/portTICK_RATE_MS);
            setLed(0,0,0);
            vTaskDelay(100/portTICK_RATE_MS);

            setLed(1,0,1);//青色
            vTaskDelay(100/portTICK_RATE_MS);
            setLed(0,0,0);
            vTaskDelay(100/portTICK_RATE_MS);

            setLed(1,0,1);//青色
            vTaskDelay(100/portTICK_RATE_MS);
            setLed(0,0,0);
            vTaskDelay(100/portTICK_RATE_MS);
            setLed(0,0,0);
            mqttConnected = 1;
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            break;
        case MQTT_EVENT_DISCONNECTED:
            mqttConnected = 0;
            setLed(0,0,0);
            vTaskDelay(100/portTICK_RATE_MS);
            setLed(1,1,0);
            esp_mqtt_client_reconnect(client);
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            vTaskDelay(1000/portTICK_RATE_MS);
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            break;
        default:
            ESP_LOGI(TAG, "Other event id:%d", event->event_id);
            break;
    }
    return ESP_OK;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    mqtt_event_handler_cb((esp_mqtt_event_handle_t)event_data);
}

void init_mqtt(void){
    ESP_LOGI(TAG,"config mqtt");
    setLed(1,1,0);
    memset(mqtt_host, 0 ,MQTT_DATA_LEN);
    memcpy(mqtt_host, pConfig->iot, strlen(pConfig->iot));
    memset(mqtt_iotID, 0 ,MQTT_DATA_LEN);
    memcpy(mqtt_iotID, pConfig->MQTTID, strlen(pConfig->MQTTID));
    memset(mqtt_iotPWD, 0 ,MQTT_DATA_LEN);
    memcpy(mqtt_iotPWD, pConfig->MQTTPWD, strlen(pConfig->MQTTPWD));
    mqtt_cfg.host = mqtt_host;
    mqtt_cfg.port = mqtt_port;
    mqtt_cfg.username = mqtt_iotID;
    mqtt_cfg.password = mqtt_iotPWD;
    mqtt_cfg.disable_auto_reconnect = true;
    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    esp_mqtt_client_start(client);
}