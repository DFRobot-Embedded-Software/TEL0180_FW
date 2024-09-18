#include "http.h"
#include "esp_http_server.h"
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include "esp_netif.h"
#include "esp_eth.h"
#include <string.h>
#include "stdio.h"
#include "i2c_slave.h"
#include "i2c_master.h"
#include "rgb.h"

#define EXAMPLE_ESP_WIFI_SSID      "ESP32_Hotspot"
#define EXAMPLE_ESP_WIFI_PASS      "12345678"
#define EXAMPLE_ESP_WIFI_CHANNEL   1
#define EXAMPLE_MAX_STA_CONN       4

static const char *TAG = "http";
#define MIN(a, b) ((a) < (b) ? (a) : (b))

static int pre_start_mem, post_stop_mem;
static unsigned visitors = 0;
static httpd_handle_t server = NULL;
esp_netif_t *ap_netif = NULL;

extern sConfigData* pConfig;
extern SemaphoreHandle_t xSemaphore;

// 定义 topicline 的前缀
char *topicPrefix[10] = {"1:", "2:", "3:", "4:", "5:", "6:", "7:", "8:", "9:", "10:"};

/* This handler gets the present value of the accumulator */
static esp_err_t adder_get_handler(httpd_req_t *req)
{
    const char *form_html =
        "<form action=\"/config\" method=\"post\">"
        "WiFi SSID: <input type=\"text\" name=\"wifissid\"><br>"
        "WiFi Password: <input type=\"text\" name=\"wifipassword\"><br>"
        "MQTT Server: <input type=\"text\" name=\"mqttServer\"><br>"
        "MQTT Username: <input type=\"text\" name=\"mqttUsername\"><br>"
        "MQTT Password: <input type=\"text\" name=\"mqttPassword\"><br>"
        "Save: <input type=\"text\" name=\"save\"><br>"
        "Topic Line 1: <input type=\"text\" name=\"topicline1\"><br>"
        "Topic Line 2: <input type=\"text\" name=\"topicline2\"><br>"
        "Topic Line 3: <input type=\"text\" name=\"topicline3\"><br>"
        "Topic Line 4: <input type=\"text\" name=\"topicline4\"><br>"
        "Topic Line 5: <input type=\"text\" name=\"topicline5\"><br>"
        "Topic Line 6: <input type=\"text\" name=\"topicline6\"><br>"
        "Topic Line 7: <input type=\"text\" name=\"topicline7\"><br>"
        "Topic Line 8: <input type=\"text\" name=\"topicline8\"><br>"
        "Topic Line 9: <input type=\"text\" name=\"topicline9\"><br>"
        "Topic Line 10: <input type=\"text\" name=\"topicline10\"><br>"
        "<input type=\"submit\" value=\"Submit\">"
        "</form>";
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, form_html, strlen(form_html));
    return ESP_OK;

}

static const httpd_uri_t adder_get = {
    .uri      = "/",
    .method   = HTTP_GET,
    .handler  = adder_get_handler,
    .user_ctx = NULL
};

void print_buf_data(const char *tag, const char *buf, size_t len) {
    // 打印缓冲区数据
    for (int i = 0; i < len; i++) {
        // 按十六进制格式打印每个字节
        printf("%c",buf[i]);
        //ESP_LOGI(tag, "%d ", buf[i]);
    }
    ESP_LOGI(tag,"end");
}

// // 辅助函数，截取指定长度的字符串
// char *extract_value(const char *start, const char *end) {
//     size_t len = end ? (size_t)(end - start) : strlen(start);
//     char *value = (char *)malloc(len + 1);
//     if (value) {
//         strncpy(value, start, len);
//         value[len] = '\0';
//     }
//     return value;
// }

// // 解析参数函数
// char *get_param_value(const char *buf, const char *param_name) {
//     char *param_start = strstr(buf, param_name);
//     if (!param_start) {
//         return "";
//     }
//     param_start += strlen(param_name);  // 跳过参数名称部分
//     char *param_end = strstr(param_start, "&");  // 找到下一个 "&"
//     return extract_value(param_start, param_end);
// }


// URL解码函数，将%XX转换为对应的字符
char *url_decode(const char *str) {
    char *decoded = malloc(strlen(str) + 1);
    char *pout = decoded;
    while (*str) {
        if (*str == '%' && *(str + 1) && *(str + 2)) {
            int hex;
            sscanf(str + 1, "%2x", &hex);
            *pout++ = (char)hex;
            str += 3;
        } else {
            *pout++ = *str++;
        }
    }
    *pout = '\0';
    return decoded;
}

// 提取参数值
char *extract_value(const char *param_start, const char *param_end) {
    size_t length = param_end ? (size_t)(param_end - param_start) : strlen(param_start);
    char *value = malloc(length + 1);
    strncpy(value, param_start, length);
    value[length] = '\0';
    return value;
}

// 获取参数值的函数
char *get_param_value(const char *buf, const char *param_name) {
    char *param_start = strstr(buf, param_name);
    if (!param_start) {
        return " ";
    }
    param_start += strlen(param_name);  // 跳过参数名称部分
    char *param_end = strstr(param_start, "&");  // 找到下一个 "&"
    
    char *encoded_value = extract_value(param_start, param_end);
    char *decoded_value = url_decode(encoded_value);
    
    free(encoded_value);  // 释放提取的原始字符串
    return decoded_value;  // 返回解码后的值
}


static esp_err_t adder_post_handler(httpd_req_t *req)
{
    char buf[1000];
    int ret;
    int remaining = req->content_len;
    
    // 初始化缓冲区
    memset(buf, 0, sizeof(buf));

    // 从请求中读取数据
    while (remaining > 0) {
        ret = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf) - 1));
        if (ret < 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            ESP_LOGE(TAG, "Socket read failed!");
            return ESP_FAIL;
        } else if (ret == 0) {
            break;
        }
        remaining -= ret;
        buf[ret] = '\0'; // Null-terminate the received data
    }

    // 检查是否接收到数据
    if (strlen(buf) == 0) {
        ESP_LOGW(TAG, "Received empty data");
        const char *resp_str = "No data received!";
        httpd_resp_send(req, resp_str, strlen(resp_str));
        return ESP_OK;
    }
    freeLinkedList(pConfig);//清除链表
    //print_buf_data(TAG, buf, sizeof(buf));//打印原始数据

    // 解析表单数据
    #if 0
    char *wifissid = strstr(buf, "wifissid=") ? strtok(strstr(buf, "wifissid=") + 9, "&") : "";
    char *wifipassword = strstr(buf, "wifipassword=") ? strtok(strstr(buf, "wifipassword=") + 13, "&") : "";
    char *mqttServer = strstr(buf, "mqttServer=") ? strtok(strstr(buf, "mqttServer=") + 11, "&") : "";
    char *mqttUsername = strstr(buf, "mqttUsername=") ? strtok(strstr(buf, "mqttUsername=") + 13, "&") : "";
    char *mqttPassword = strstr(buf, "mqttPassword=") ? strtok(strstr(buf, "mqttPassword=") + 13, "&") : "";
    char *save = strstr(buf, "save=") ? strtok(strstr(buf, "save=") + 5, "&") : "";
    char *topicline1 = strstr(buf, "topicline1=") ? strtok(strstr(buf, "topicline1=") + 11, "&") : "";
    char *topicline2 = strstr(buf, "topicline2=") ? strtok(strstr(buf, "topicline2=") + 11, "&") : "";
    char *topicline3 = strstr(buf, "topicline3=") ? strtok(strstr(buf, "topicline3=") + 11, "&") : "";
    char *topicline4 = strstr(buf, "topicline4=") ? strtok(strstr(buf, "topicline4=") + 11, "&") : "";
    char *topicline5 = strstr(buf, "topicline5=") ? strtok(strstr(buf, "topicline5=") + 11, "&") : "";
    char *topicline6 = strstr(buf, "topicline6=") ? strtok(strstr(buf, "topicline6=") + 11, "&") : "";
    char *topicline7 = strstr(buf, "topicline7=") ? strtok(strstr(buf, "topicline7=") + 11, "&") : "";
    char *topicline8 = strstr(buf, "topicline8=") ? strtok(strstr(buf, "topicline8=") + 11, "&") : "";
    char *topicline9 = strstr(buf, "topicline9=") ? strtok(strstr(buf, "topicline9=") + 11, "&") : "";
    char *topicline10 = strstr(buf, "topicline10=") ? strtok(strstr(buf, "topicline10=") + 12, "&") : "";
    // 处理其余的 topiclineX ...
    #else
    char *wifissid = get_param_value(buf, "wifissid=");
    char *wifipassword = get_param_value(buf, "wifipassword=");
    char *mqttServer = get_param_value(buf, "mqttServer=");
    char *mqttUsername = get_param_value(buf, "mqttUsername=");
    char *mqttPassword = get_param_value(buf, "mqttPassword=");
    char *save = get_param_value(buf, "save=");

    // 拼接 topicline
    char *topicLines[10];
    for (int i = 0; i < 10; i++) {
        // 获取 topic 参数
        char paramName[30];
        snprintf(paramName, sizeof(paramName), "topicline%d=", i + 1);
        char *paramValue = get_param_value(buf, paramName);

        // 动态分配内存并拼接
        topicLines[i] = (char *)malloc(strlen(topicPrefix[i]) + strlen(paramValue) + 1);
        if (topicLines[i] != NULL) {
            strcpy(topicLines[i], topicPrefix[i]);
            strcat(topicLines[i], paramValue);
        }
    }

    #endif
    #if 1
    // 检查指针是否为空并进行适当处理
    if (strlen(wifissid) != 0){
        updateStringMember(&pConfig->wifiName,wifissid);
    }

    if (strlen(wifipassword) != 0){
        updateStringMember(&pConfig->wifiPWD,wifipassword);
    }
    if (strlen(mqttServer) != 0){
        updateStringMember(&pConfig->iot,mqttServer);
    } 
    if (strlen(mqttUsername) != 0){
        updateStringMember(&pConfig->MQTTID,mqttUsername);
    }
    if (strlen(mqttPassword) != 0){
        updateStringMember(&pConfig->MQTTPWD,mqttPassword);
    }
    if (strlen(save) != 0){
        updateStringMember(&pConfig->QOS,save);
    }

    // 处理 topicLines
    for (int i = 0; i < 10; i++) {
        if (strlen(topicLines[i]) > 3) {
            if (pConfig->head != NULL) {
                checkAndInsert(pConfig, topicLines[i]);
            } else {
                insertNode(pConfig, topicLines[i]);
            }
        }
        free(topicLines[i]); // 释放动态分配的内存
    }
    #endif
    // 处理其余的 topiclineX ...
    #if 1
    ESP_LOGI(TAG, "Config values received:");
    ESP_LOGI(TAG, "WiFi SSID: %s", pConfig->wifiName);
    ESP_LOGI(TAG, "WiFi Password: %s", pConfig->wifiPWD);
    ESP_LOGI(TAG, "MQTT Server: %s", pConfig->iot);
    ESP_LOGI(TAG, "MQTT Username: %s", pConfig->MQTTID);
    ESP_LOGI(TAG, "MQTT Password: %s", pConfig->MQTTPWD);
    ESP_LOGI(TAG, "Save: %s", pConfig->QOS);
    ESP_LOGI(TAG, "Topic Line 1: %s", pConfig->QOS);
     ESP_LOGI(TAG, "Topic Line 2: %s", pConfig->QOS);
     // 处理其余的 topiclineX ...
    #endif
   

    // 发送响应
    const char *resp_str = "Configuration received!";
    httpd_resp_send(req, resp_str, strlen(resp_str));
    writeFile();
    vTaskDelay(100);
    xSemaphoreGive(xSemaphore);
    return ESP_OK;

}

static const httpd_uri_t adder_post = {
    .uri      = "/config",
    .method   = HTTP_POST,
    .handler  = adder_post_handler,
    .user_ctx = NULL
};


void stop_httpd(void){
    esp_wifi_stop();
    esp_netif_destroy_default_wifi(ap_netif);
    esp_event_loop_delete_default();
    esp_wifi_deinit();
    esp_netif_deinit();
    httpd_stop(server);
    init_i2c_master(2);
}

httpd_handle_t start_server(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    httpd_handle_t server;

    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &adder_get);
        httpd_register_uri_handler(server, &adder_post);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}




void initHTTPServer(void){
    #if 0
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
            .channel = EXAMPLE_ESP_WIFI_CHANNEL,
            .password = EXAMPLE_ESP_WIFI_PASS,
            .max_connection = EXAMPLE_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
    if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    vTaskDelay(100);
    #else
    // 初始化 NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 初始化网络接口
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // 创建 AP 网络接口
    ap_netif = esp_netif_create_default_wifi_ap();
    if (ap_netif == NULL) {
        ESP_LOGE(TAG, "Failed to create default Wi-Fi AP netif");
        return;
    }

    // 配置静态 IP 地址
    esp_netif_ip_info_t ip_info;
    IP4_ADDR(&ip_info.ip, 192, 168, 32, 32);       // 修改为你希望的静态 IP 地址
    IP4_ADDR(&ip_info.gw, 192, 168, 32, 32);        // 修改为你希望的网关地址
    IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0); // 修改为你希望的子网掩码
    
    ESP_ERROR_CHECK(esp_netif_dhcps_stop(ap_netif)); // 停止 DHCP 服务
    ESP_ERROR_CHECK(esp_netif_set_ip_info(ap_netif, &ip_info)); // 设置静态 IP
    ESP_ERROR_CHECK(esp_netif_dhcps_start(ap_netif)); // 启动 DHCP 服务（可选）

    // 初始化 Wi-Fi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 配置 Wi-Fi AP 参数
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
            .channel = EXAMPLE_ESP_WIFI_CHANNEL,
            .password = EXAMPLE_ESP_WIFI_PASS,
            .max_connection = EXAMPLE_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
    if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    #endif
    server = start_server();

}