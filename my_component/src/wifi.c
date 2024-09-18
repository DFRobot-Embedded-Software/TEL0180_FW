#include "wifi.h"
#include "mqtt.h"

static const char *TAG = "wifi";

extern sConfigData* pConfig;
wifi_config_t wifi_config;

extern uint8_t mqttConnected;
extern uint8_t mqttState;

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {//开始链接WiFi
        setLed(1,0,0);
        esp_wifi_connect();
        printf("%s",wifi_config.sta.ssid);
        printf("%s",wifi_config.sta.password);
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 wifi_config.sta.ssid, wifi_config.sta.password);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        mqttConnected = 0;
        setLed(0,0,0);
        vTaskDelay(100/portTICK_RATE_MS);
        setLed(1,0,0);
        esp_wifi_connect();
        ESP_LOGI(TAG, "connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        setLed(0,0,0);
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        if(mqttState ==0){
            init_mqtt();
        }
    }

}
void wifi_init(void){
    ESP_LOGI(TAG,"config wifi");
    /* Initialize NVS — it is used to store PHY calibration data */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
         ESP_LOGI(TAG,"erase nvs");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(esp_event_loop_delete_default());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());


    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, NULL));

     // Initialize default station as network interface instance (esp-netif)
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    
}

void wifi_connect(void){
    wifi_config = (wifi_config_t){
        .sta = {
            .ssid = "123456789",
            .password = "123456789",
            /* Setting a password implies station will connect to all security modes including WEP/WPA.
             * However these modes are deprecated and not advisable to be used. Incase your Access point
             * doesn't support WPA2, these mode can be enabled by commenting below line */
	     .threshold.authmode = WIFI_AUTH_OPEN,              // 为了兼容低安全性的wifi ，将安全性降到最低

            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    memset(wifi_config.sta.ssid, 0, 32);
    memset(wifi_config.sta.password, 0, 64);
    memcpy(wifi_config.sta.ssid, pConfig->wifiName, strlen(pConfig->wifiName));
    memcpy(wifi_config.sta.password, pConfig->wifiPWD, strlen(pConfig->wifiPWD));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    //ESP_ERROR_CHECK(esp_wifi_disconnect());
    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_set_max_tx_power(8);           // 解决发射频率不够的问题，（可能会导致瞬时功率过大）
    esp_wifi_set_ps(WIFI_PS_MAX_MODEM);

}