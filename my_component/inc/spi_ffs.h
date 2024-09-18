#ifndef _SPIFFS_H_
#define _SPIFFS_H_
#include "group.h"

/**
 * @fn init_spi_ffs
 *@brief 初始化ffs文件系统 
 *
*/
esp_err_t init_spi_ffs(void);
/**
 * @fn parsingFile
 * @brief 解析文件
*/
void parsingFile(void);
/**
 * @fn writeFile
 * @brief 将配置信息写入文件
*/
void writeFile(void);

char* addNewline(char* str);

#endif
