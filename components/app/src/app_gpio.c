#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "app_gpio.h"

#define TAG         "app_gpio"

#define KEY_SCAN_INTERVAL_MS        60          // 按键扫描间隔(消抖时长)

static QueueHandle_t  key_queue = NULL;

typedef struct key_dev{
    int32_t key_pin;
    uint8_t active_level;
    struct key_dev *next;
} btn_dev_t;

static btn_dev_t *btn_head_handle = NULL;

esp_err_t button_create(button_gpio_config_t *config)
{
    /** config gpio */
    gpio_config_t gpio_conf;
    gpio_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_conf.mode = GPIO_MODE_INPUT;
    gpio_conf.pin_bit_mask = (1ULL << config->gpio_num);
    if (config->active_level) {
        gpio_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
        gpio_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    } else {
        gpio_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        gpio_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    }
    gpio_config(&gpio_conf);

    /** config button handle node*/
    btn_dev_t *button = (btn_dev_t *)malloc (sizeof(btn_dev_t));
    if  (button == NULL) {
        ESP_LOGE(TAG, "Button memory alloc failed");
        return ESP_FAIL;
    }
    button->key_pin = config->gpio_num;
    button->active_level = config->active_level;

    /** Add handle to list */
    button->next = btn_head_handle;
    btn_head_handle = button;

    return ESP_OK;

}

/**
 * get key value
*/
void button_get_key_value(int32_t *num)
{
    xQueueReceive(key_queue, num, portMAX_DELAY);
}

static void key_scan_task(void *pvParameter)
{
    ESP_LOGD(TAG, "key scan start");
    btn_dev_t *target = NULL;
    int32_t key_state_mask = 0;
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(KEY_SCAN_INTERVAL_MS));

        /** ergod all button device*/
        for (target = btn_head_handle; target; target = target->next) {
            int status = gpio_get_level(target->key_pin);
            if (status == target->active_level) {
                int32_t pin_bit_mask = (1ULL << target->key_pin);
                if (key_state_mask & pin_bit_mask) {
                    int32_t key_pin = target->key_pin;
                    key_state_mask &= ~pin_bit_mask;
                    xQueueSend(key_queue, &key_pin, 10 / portTICK_PERIOD_MS);
                } else {
                    /** wait next callback debounce*/
                    key_state_mask |= pin_bit_mask;
                }
                
            }
        }
        
    }

}

void my_key_init()
{
    // 熄灭RGB灯，防止闪烁
    gpio_reset_pin(RGB_LED_PIN);
    gpio_set_direction(RGB_LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(RGB_LED_PIN, 0);

    button_gpio_config_t btn_config;

    btn_config.gpio_num = KEY_UP_PIN;
    btn_config.active_level = 0;
    button_create(&btn_config);

    btn_config.gpio_num = KEY_DOWN_PIN;
    button_create(&btn_config);

    btn_config.gpio_num = KEY_CONFIRM_PIN;
    button_create(&btn_config);

    btn_config.gpio_num = KEY_CANCEL_PIN;
    button_create(&btn_config);

    /** create key queue*/
    key_queue = xQueueCreate(1, sizeof(int32_t));
    /** create key scan task*/
    xTaskCreate(key_scan_task, "key_scan_task", 1024, NULL, 2, NULL);
}

