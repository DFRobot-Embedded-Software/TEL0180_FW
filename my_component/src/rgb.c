#include "rgb.h"

#define GPIO_RED    10
#define GPIO_GREEN  9
#define GPIO_BLUE   11

/**
 * @brief Set the Led object
 * 
 * @param red  0，关闭 1 开启
 * @param green  0，关闭 1 开启
 * @param blue  0，关闭 1 开启
 */
void setLed(int red, int green, int blue)
{
    gpio_set_level(GPIO_RED, red);
    gpio_set_level(GPIO_GREEN, green);
    gpio_set_level(GPIO_BLUE, blue);   
}

/**
 * @brief rgb_init 初始rgb灯
 * 
 */
void rgb_init(void)
{
    gpio_config_t io_conf;
    //disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = (1ULL<<GPIO_RED) | (1ULL<<GPIO_GREEN) | (1ULL<<GPIO_BLUE);
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    gpio_config(&io_conf);
    
}

