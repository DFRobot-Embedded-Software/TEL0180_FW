#include "group.h"

sConfigData myConfig;
sConfigData* pConfig = &myConfig;
static const char *TAG = "group";

uint8_t mqttConnected = 0;
uint8_t mqttState = 0;

// 声明队列句柄
//QueueHandle_t xQueue;

void initLinkedList(sConfigData* myStruct){
    myStruct->wifiPWD=NULL;
    myStruct->wifiName = NULL;
    myStruct->iot = NULL;
    myStruct->QOS = NULL;
    myStruct->MQTTID = NULL;
    myStruct->MQTTPWD = NULL;
    myStruct->head = NULL;
    myStruct->tail = NULL;
    myStruct->nodeCount = 0;
    for (int i = 0; i < MAX_NODES; ++i) {
        myStruct->topic[i].topic = NULL; // 将节点对象池中的每个节点的数据指针初始化为 NULL
        myStruct->topic[i].next = NULL;
    }
}

// 从对象池中获取新节点
Topic* allocateNode(sConfigData* myStruct) {
    if (myStruct->nodeCount < MAX_NODES) {
        return &(myStruct->topic[myStruct->nodeCount++]);
    } else {
        ESP_LOGE(TAG,"allocateNode error\n");
        return NULL;
    }
}

// 插入新节点到链表头部
void insertNode(sConfigData* myStruct, char* newData) {
    Topic* newNode = allocateNode(myStruct);
    if (newNode == NULL) {
        ESP_LOGE(TAG," allocateNode error\n");
        return;
    }

    // 分配足够的内存来存储字符串，并复制数据
    newNode->topic= strdup(newData);
    if (newNode->topic == NULL) {
        ESP_LOGE(TAG,"memory allocation failure\n");
        return;
    }
    newNode->next = NULL;

    if (myStruct->tail == NULL) {
        // 如果链表为空，则新节点既是头节点又是尾节点
        myStruct->head = newNode;
        myStruct->tail = newNode;
    } else {
        // 将新节点连接到链表的尾部，并更新尾节点指针
        myStruct->tail->next = newNode;
        myStruct->tail = newNode;
    }
}

// 检查链表中是否存在特定的数据，如果不存在，则插入新节点
void checkAndInsert(sConfigData* myStruct, char* newData) {
    Topic* current = myStruct->head;
    while (current != NULL) {
        if (strncmp(current->topic, newData, 2) == 0) {
            // 数据前两个字符相同，替换当前节点的数据
            free(current->topic); // 释放当前节点的字符串内存
            current->topic = strdup(newData); // 分配新内存并复制数据
            if (current->topic == NULL) {
                printf("memory allocation failure\n");
                return;
            }
            return; // 替换完成，返回
        }
        current = current->next;
    }
    // 数据不存在，插入新节点
    insertNode(myStruct, newData);
}

// 释放链表节点中的字符串内存
void freeLinkedList(sConfigData* myStruct) {
    Topic* current = myStruct->head;
    while (current != NULL) {
        Topic* next = current->next;
        free(current->topic); // 释放字符串内存
        current->topic = NULL;
        current->next = NULL;
        current = next;
    }
    myStruct->head = NULL;
    myStruct->tail = NULL;
    myStruct->nodeCount = 0; // 重置节点计数器
}