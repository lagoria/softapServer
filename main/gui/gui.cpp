#include "gui.h"
#include "wifi_wrapper.h"
#include "tcp_server.h"
#include "lcd_st7735.h"
#include "app_config.h"
#include "key.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include <memory.h>

namespace gui {

constexpr static char TAG[] = "gui";

/**
 * clear a rectangle window
*/
static void lcd_clear_row(lcd_data_frame_t *image, uint8_t row)
{
    if (row <= 6 && row > 0) {
        image->x = 3;
        image->y = 24 + 16 * (row - 1);
        image->x_end = 124;
        image->y_end = image->y + 16;
    } else {
        image->x = 3;
        image->y = 24;
        image->x_end = 124;
        image->y_end = 124;
    }
    image->type = LCD_REC;
    image->color = COLOR_BLACK;
    lcd_frame_display_data(image);
}

/**
 * LCD快捷显示字符串
*/
static void lcd_printf_string(lcd_data_frame_t *image,
                            uint8_t row, char *string)
{
    if (string != NULL) {
        uint8_t len = strlen(string);
        strcpy((char *)image->data, string);
        image->data[len] = '\0';
        image->len = len;
    } else {
        image->len = strlen((char *)image->data);
    }
    lcd_clear_row(image, row);
    image->type = LCD_STRING;
    image->color = COLOR_GREEN;
    image->back_color = COLOR_BLACK;
    image->x = 3;
    image->y = 24 + 16 * (row - 1);
    lcd_frame_display_data(image);
}

void refresh_client_info(lcd_data_frame_t *image, TcpServer::ClientInfo *client, uint8_t index)
{
    sprintf((char *) image->data, "    Page:%d", index + 1);
    lcd_printf_string(image, 1, NULL);

    sprintf((char *) image->data, "Name:");
    strcat((char *)image->data, client->name);
    lcd_printf_string(image, 2, NULL);

    sprintf((char *) image->data, "IP:");
    strcat((char *)image->data, client->ip);
    lcd_printf_string(image, 3, NULL);

    sprintf((char *)image->data, "Socket: %d",client->socket);
    lcd_printf_string(image, 4, NULL);

    sprintf((char *)image->data, "Port:   %d", client->port);
    lcd_printf_string(image, 5, NULL);

    sprintf((char *)image->data, "Mark:   %d", client->id);
    lcd_printf_string(image, 6, NULL);
}

static void lcd_draw_task(void *arg)
{
    static int32_t key = 0;
    uint8_t client_index = 0;
    lcd_data_frame_t image;
    memset(&image, 0, sizeof(image));
    
    image.data = (uint8_t *)heap_caps_malloc(LCD_MAX_SIZE, MALLOC_CAP_DMA);
    if (image.data == NULL) {
        ESP_LOGE(TAG, "malloc the image data failed");
        goto over;
    }
    image.type = LCD_CLEAR;
    image.color = COLOR_CYAN;
    lcd_frame_display_data(&image);
    
    image.type = LCD_STRING;
    image.color = COLOR_VIOLET;
    image.back_color = COLOR_CYAN;
    image.x = 12;
    image.y = 0;
    sprintf((char *)image.data, "SOFTAP_SERVER");
    image.len = strlen((char *)image.data);
    lcd_frame_display_data(&image);

    lcd_clear_row(&image, 0);

    key = KEY_CONFIRM_PIN;
    while (1) {

        switch(key) {
            case KEY_UP_PIN: {
                if (client_index > 0) {
                    client_index --;
                }
                TcpServer::ClientInfo *current = TcpServer::getClientsInfo();
                if (current == NULL) {
                    lcd_clear_row(&image, 0);
                    lcd_printf_string(&image, 3, "   No client\n  connection");
                    client_index = 0;
                    break;
                }
                for (int i = 0; i < client_index; i ++) {
                    if (current->next == NULL) {
                        client_index = i;
                        break;
                    }
                    current = current->next;
                }
                refresh_client_info(&image, current, client_index);

                break;
            }
            case KEY_DOWN_PIN: {
                client_index++;
                TcpServer::ClientInfo *current = TcpServer::getClientsInfo();
                if (current == NULL) {
                    lcd_clear_row(&image, 0);
                    lcd_printf_string(&image, 3, "   No client\n  connection");
                    client_index = 0;
                    break;
                }
                for (int i = 0; i < client_index; i ++) {
                    if (current->next == NULL) {
                        client_index = i;
                        break;
                    }
                    current = current->next;
                }
                refresh_client_info(&image, current, client_index);

                break;
            }
            case KEY_CONFIRM_PIN: {
                lcd_clear_row(&image, 2);
                lcd_printf_string(&image, 1, "  Device Info");

                sprintf((char *)image.data, "SID:%s",AppCfg::SOFTAP_SSID);
                lcd_printf_string(&image, 3, NULL);
                sprintf((char *)image.data, "PWD:%s",AppCfg::SOFTAP_PAWD);
                lcd_printf_string(&image, 4, NULL);

                sprintf((char *)image.data, "IP:192.168.4.1");
                lcd_printf_string(&image, 5, NULL);
                sprintf((char *)image.data, "TCP PORT:%d", AppCfg::SERVER_PORT);
                lcd_printf_string(&image, 6, NULL);
                break;
            }
            case KEY_CANCEL_PIN: {
                lcd_clear_row(&image, 0);
                uint8_t sta_count = 0;

                for (TcpServer::ClientInfo* list = TcpServer::getClientsInfo(); list; list = list->next) {
                    sta_count++;
                }
                
                sprintf((char *)image.data, "  sta count:%.1d", sta_count);
                lcd_printf_string(&image, 1, NULL);

                sprintf((char *)image.data, "Router SSID:\n%s", Wrapper::WiFi::Store::read_ssid().data());
                lcd_printf_string(&image, 3, NULL);

                if (Wrapper::WiFi::state() == Wrapper::WiFi::State::CONNECTED) {
                    sprintf((char *)image.data, "Station IP:\n%s", Wrapper::WiFi::get_ip().data());
                } else {
                    sprintf((char *)image.data, "     Router\n    %s", Wrapper::WiFi::stateString(Wrapper::WiFi::state()));
                }
                lcd_printf_string(&image, 5, NULL);
                
                break;
            }
            default : break;
        }
        

        vTaskDelay(pdMS_TO_TICKS(100));

        button_get_key_value(&key);
    }
over:
    vTaskDelete(NULL);
}


void init() {
    my_key_init();
    lcd_st7735_init();
    xTaskCreate(lcd_draw_task, "lcd_draw_task", 5 * 1024, NULL, 5, NULL);
}

}
