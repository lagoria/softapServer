#ifndef APP_GPIO_H
#define APP_GPIO_H
#include "driver/gpio.h"

#define RGB_LED_PIN         18      //板载LED

#define KEY_UP_PIN          12
#define KEY_DOWN_PIN        13
#define KEY_CONFIRM_PIN     14
#define KEY_CANCEL_PIN      15


/**
 * @brief gpio button configuration
 * 
 */
typedef struct {
    int32_t gpio_num;              /**< num of gpio */
    uint8_t active_level;          /**< gpio level when press down */
} button_gpio_config_t;


// 获取按键值
void button_get_key_value(int32_t *num);
// 初始化gpio创建按键
void my_key_init();

#endif

