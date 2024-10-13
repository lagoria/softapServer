#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_heap_caps.h"

#include "wifi_wrapper.h"
#include "socket_wrapper.h"
#include "app_cjson.h"
#include "lcd_st7735.h"
#include "app_config.h"
#include "app_gpio.h"


static const char *TAG = "app_main";

static QueueHandle_t handle_queue = NULL;
// 套接字数据缓存指针
static uint8_t *tx_rx_buffer = NULL;

enum {
    TCP_SERVER_MARK,
};

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

void refresh_client_info(lcd_data_frame_t *image, tcp_client_info_t *client, uint8_t index)
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

    sprintf((char *)image->data, "Id:     %d", client->id);
    lcd_printf_string(image, 6, NULL);
}

static void lcd_draw_task(void *arg)
{
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

    static int32_t key = KEY_CONFIRM_PIN;
    uint8_t client_index = 0;
    while (1) {

        switch(key) {
            case KEY_UP_PIN: {
                if (client_index > 0) {
                    client_index --;
                }
                tcp_client_info_t *current = get_clients_info_list();
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
                tcp_client_info_t *current = get_clients_info_list();
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

                sprintf((char *)image.data, "SID:%s",CONFIG_SOFTAP_SSID);
                lcd_printf_string(&image, 3, NULL);
                sprintf((char *)image.data, "PWD:%s",CONFIG_SOFTAP_PAS);
                lcd_printf_string(&image, 4, NULL);

                sprintf((char *)image.data, "IP:192.168.4.1");
                lcd_printf_string(&image, 5, NULL);
                sprintf((char *)image.data, "TCP PORT:%d",TCP_SERVER_PORT);
                lcd_printf_string(&image, 6, NULL);
                break;
            }
            case KEY_CANCEL_PIN: {
                lcd_clear_row(&image, 0);
                uint8_t sta_count = 0;

                for (wifi_wrapper_sta_info_t *list = wifi_wrapper_get_sta_info(); list; list = list->next) {
                    sta_count++;
                }
                
                sprintf((char *)image.data, "  sta count:%.1d", sta_count);
                lcd_printf_string(&image, 1, NULL);

                wifi_config_t router_config;
                esp_wifi_get_config(WIFI_IF_STA, &router_config);
                sprintf((char *)image.data, "Router SSID:\n%s", (char *)router_config.sta.ssid);
                lcd_printf_string(&image, 3, NULL);

                if (wifi_wrapper_wait_connected(0)) {
                    sprintf((char *)image.data, "Station IP:\n%s", wifi_wrapper_get_local_ip());
                } else {
                    sprintf((char *)image.data, "     Router\n  disconnect");
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


static void tcp_recv_callback(tcp_socket_info_t info)
{
    tcp_socket_info_t handle = {0};
    handle.data = tx_rx_buffer;     // point to data buffer
    memcpy(handle.data, info.data, info.len);
    handle.data[info.len] = '\0';
    handle.len = info.len;
    handle.socket = info.socket;
    // 将事件处理送入数据队列
    xQueueSend(handle_queue, &handle, 10 / portTICK_PERIOD_MS);
}


void data_frame_send(int sock, uint8_t target_id,
                    uint32_t len, char *data, client_protocol_t protocol)
{
    if (len + FRAME_HEADER_LEN > MAX_BUFFER_SIZE) {
        ESP_LOGE(TAG, "data frame length overflow");
        return ;
    }
    if (protocol == PROTOCOL_STRING) {
        socket_send(sock, (uint8_t *)data, len);
    } else {
        tx_rx_buffer[FRAME_HEAD_BIT] = FRAME_HEAD;
        tx_rx_buffer[FRAME_TYPE_BIT] = FRAME_TYPE_RESPOND;
        tx_rx_buffer[FRAME_TARGET_BIT] = target_id;
        tx_rx_buffer[FRAME_LOCAL_BIT] = FRAME_SERVER_ID;
        *(uint32_t *)&tx_rx_buffer[FRAME_LENGTH_BIT] = len;
        memcpy(&tx_rx_buffer[FRAME_DATA_BIT], data, len);
        socket_send(sock, tx_rx_buffer, len + FRAME_HEADER_LEN);
    }
}

void client_cmd_handle(tcp_socket_info_t *handle, char *recv_data, client_protocol_t protocol)
{
    char *respond = NULL;
    client_evt_t handle_evt = CLIENT_UNKNOWN_EVT;
    tcp_client_info_t *current_client = get_clients_info_list();
    /* 进行身份注册检测 */
    for ( ; current_client; current_client = current_client->next) {
        if (current_client->socket == handle->socket) {
            break;
        }
    }
    if (current_client == NULL) return;

    cJSON *root = cJSON_Parse((char *)recv_data);  // 解析JSON字符串
    cJSON *cmd = cJSON_GetObjectItem(root, CMD_KEY_COMMAND);
    cJSON *transmit = cJSON_GetObjectItem(root, CMD_KEY_TRANSMIT);

    /* 数据转发判断 */
    if (transmit != NULL && transmit->type == cJSON_String) {
        handle_evt = CLIENT_TRANSMIT_EVT;
    } else if (cmd != NULL && cmd->type == cJSON_String) {
        if (strcmp(cmd->valuestring, CMD_STRING_REGISTER) == 0) {
            /* 注册信息 */
            handle_evt = CMD_REGISTER_EVT;
        } else if (strcmp(cmd->valuestring, CMD_STRING_LIST) == 0) {
            /* 客户端信息列表指令 */
            handle_evt = CMD_LIST_EVT;
        } else if (strcmp(cmd->valuestring, CMD_STRING_ROUTER) == 0) {
            /* 修改路由器帐号信息指令 */
            handle_evt = CMD_ROUTER_EVT;
        } else if (strcmp(cmd->valuestring, CMD_STRING_UUID) == 0) {
            /* 获取目标客户端ID */
            handle_evt = CMD_UUID_EVT;
        }
    }

    if (current_client->name[0] == 0 && handle_evt != CMD_REGISTER_EVT) {
        // 未注册事件
        handle_evt = CMD_UNREGISTER_EVT;
    }
    
    switch (handle_evt) {
    case CMD_UNREGISTER_EVT : {
        /* 未注册回应 */
        respond = create_json_string(1, CMD_KEY_STATUS, "unregister");
        break;
    }
    case CMD_REGISTER_EVT : {
        ESP_LOGI(TAG, "device info register");
        cJSON *name = cJSON_GetObjectItem(root, CMD_KEY_NAME);
        if (name != NULL && name->type == cJSON_String) {
            // 注册记录客户端设备名
            strncpy(current_client->name, name->valuestring, 32);

            respond = create_json_string(1, CMD_KEY_STATUS, "succeed");
        }
        break;
    }
    case CMD_LIST_EVT : {
        // 回应客户端信息列表
        char sock_str[8] = {0};
        char port_str[8] = {0};
        char id_str[8] = {0};
        for (tcp_client_info_t *list = get_clients_info_list(); list; list = list->next) {
            sprintf(sock_str, "%d", list->socket);
            sprintf(id_str, "%d", list->id);
            sprintf(port_str, "%d", list->port);
            respond = create_json_string(5, "name", list->name,
                                                "ip", list->ip, 
                                                "port", port_str, 
                                                "sock", sock_str,
                                                "id", id_str);
            data_frame_send(handle->socket, current_client->id, strlen(respond) + 1, respond, protocol);
            free(respond);
        }
        respond = NULL;
        
        break;
    }
    case CMD_UUID_EVT : {
        /* 获取目标设备ID */
        tcp_client_info_t *list = NULL;
        cJSON *name = cJSON_GetObjectItem(root, CMD_KEY_NAME);
        if (name != NULL && name->type == cJSON_String) {
            for (list = get_clients_info_list(); list; list = list->next) {
                if(strcmp (list->name, name->valuestring) == 0) {
                    break;
                }
            }
        }
        if (list == NULL) {
            respond = create_json_string(1, CMD_KEY_STATUS, "failed");
        } else {
            char id_str[8];
            snprintf(id_str, 8, "%d", list->id);
            respond = create_json_string(1, CMD_KEY_ID, id_str);
        }

        break;
    }
    case CMD_ROUTER_EVT: {
        /* 设置wifi STA的路由器帐号 */
        cJSON *ssid = cJSON_GetObjectItem(root, CMD_KEY_SSID);
        cJSON *pwd = cJSON_GetObjectItem(root, CMD_KEY_PWD);
        if (ssid != NULL && pwd != NULL) {
            wifi_wrapper_account_cfg_t wifi_account;
            strncpy((char *)wifi_account.ssid, ssid->valuestring, sizeof(wifi_account.ssid));
            strncpy((char *)wifi_account.password, pwd->valuestring, sizeof(wifi_account.password));
            if (wifi_wrapper_nvs_store_account(&wifi_account)) {
                ESP_LOGE(TAG, "nvs store failed!");
                respond = create_json_string(1, CMD_KEY_STATUS, "failed");
            } else {
                respond = create_json_string(1, CMD_KEY_STATUS, "succeed");
            }
        }
        break;
    }
    case CLIENT_TRANSMIT_EVT : {
        /* 数据转发事件 */
        ESP_LOGD(TAG, "transmit len %d", handle->len);
        uint8_t target = (uint8_t )atoi(transmit->valuestring);
        tcp_client_info_t *list;
        for (list = get_clients_info_list(); list; list = list->next) {
            if (list->id == target) {
                /* transmit */
                socket_send(list->socket, handle->data, handle->len);
                break;
            }
        }

        if (list != NULL) {
            respond = create_json_string(1, CMD_KEY_STATUS, "succeed");
        } else {
            respond = create_json_string(1, CMD_KEY_STATUS, "failed");
        }
        break;
    }
    case CLIENT_NOT_FOUND_EVT : {
        ESP_LOGW(TAG, "client target not found");
        respond = create_json_string(1, CMD_KEY_STATUS, "failed");
        break;
    }
    case CLIENT_FRAME_INVALID_EVT : {
        ESP_LOGW(TAG, "[sock=%d]: data frame invalid", handle->socket);
        respond = create_json_string(1, CMD_KEY_STATUS, "invalid");
        break;
    }
    case CLIENT_UNKNOWN_EVT : {
        ESP_LOGW(TAG, "unknown event");
        respond = create_json_string(1, CMD_KEY_STATUS, "unknown");
        break;
    }
    
    default : break;
    }

    cJSON_Delete(root);

    if (respond == NULL) return;
    data_frame_send(handle->socket, current_client->id, strlen(respond) + 1, respond, protocol);
    free(respond);
}

static void sock_evt_handle_task(void *pvParameter)
{
    tcp_socket_info_t handle;
    frame_header_info_t frame;
    char *recv_data = NULL;
    int recv_len = 0;               // 接收数据长度

    while (1) {
        /* 堵塞 等待事件触发 */
        xQueueReceive(handle_queue, &handle, portMAX_DELAY);

        client_protocol_t protocol = PROTOCOL_STRING;       // 客户端通信方式
        uint8_t frame_type = FRAME_TYPE_INVALID;            // 数据桢类型
        /* data frame parse*/
        frame = *(frame_header_info_t *)handle.data;
        if (frame.head == FRAME_HEAD && frame.length == handle.len - FRAME_HEADER_LEN) {
            if (frame.type == FRAME_TYPE_DIRECT || frame.type == FRAME_TYPE_TRANSMIT) {
                // 加桢头通信方式
                recv_len = frame.length;
                recv_data = (char *)&handle.data[FRAME_DATA_BIT];
                protocol = PROTOCOL_FRAME;
                frame_type = frame.type;
            }
        } else if (handle.data[0] == '{') {
            // json字符串通信方式
            recv_len = handle.len;
            recv_data = (char *)handle.data;
            protocol = PROTOCOL_STRING;
            frame_type = FRAME_TYPE_DIRECT;
        }

        switch (frame_type) {
        case FRAME_TYPE_INVALID: {
            break;
        }
        case FRAME_TYPE_DIRECT : {
            ESP_LOGI(TAG, "[sock=%d]: %d Byte Received %s", handle.socket, recv_len, (char *)recv_data);
            /* 命令处理 */
            client_cmd_handle(&handle, recv_data, protocol);
            break;
        }
        case FRAME_TYPE_TRANSMIT : {
            /* 桢数据转发 */
            for (tcp_client_info_t *list = get_clients_info_list(); list; list = list->next) {
                if (list->id == frame.goal) {
                    /* transmit */
                    socket_send(list->socket, handle.data, handle.len);
                    break;
                }
            }
            break;
        }
        default : break;
            
        } /* switch */

    } /* while (1) */
}



void app_main(void)
{
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_wrapper_account_cfg_t wifi_account;
    if (wifi_wrapper_nvs_load_account(&wifi_account)) {
        strncpy((char *)wifi_account.ssid, DEFAULT_ROUTER_SSID, sizeof(wifi_account.ssid));
        strncpy((char *)wifi_account.password, DEFAULT_ROUTER_PWD, sizeof(wifi_account.password));
        if (wifi_wrapper_nvs_store_account(&wifi_account)) {
            ESP_LOGE(TAG, "nvs store failed!");
            return;
        }
    }

    wifi_wrapper_apsta_cfg_t account = {
        .softap_ssid = CONFIG_SOFTAP_SSID,
        .softap_pwd = CONFIG_SOFTAP_PAS,
        .enable_nat = true,
    };
    wifi_wrapper_apsta_init(&account);
    wifi_wrapper_connect(CONN_5X);

    my_key_init();

    lcd_st7735_init();

    handle_queue = xQueueCreate(1, sizeof(tcp_socket_info_t));
    if (handle_queue == NULL) {
        ESP_LOGE(TAG, "failed to create handle queue");
        return;
    }
    /* allocation sock date buffer */
    tx_rx_buffer = (uint8_t *)heap_caps_malloc(MAX_BUFFER_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (tx_rx_buffer == NULL) {
        ESP_LOGE(TAG, "tx rx buffer malloc failed");
        return ;
    }

    // 创建TCP服务器
    socket_server_config_t tcp_config = {
        .listen_port = TCP_SERVER_PORT,
        .maxcon_num = MAX_CLIENT_NUM,
        .way = WAY_TCP,
        .mark = TCP_SERVER_MARK,
    };
    create_socket_wrapper_server(&tcp_config);
    tcp_server_register_callback(tcp_recv_callback);


    xTaskCreatePinnedToCore(sock_evt_handle_task, "sock_evt_handle_task", 5 * 1024, NULL, 10, NULL, PRO_CPU_NUM);
    xTaskCreatePinnedToCore(lcd_draw_task, "lcd_draw_task", 5 * 1024, NULL, 5, NULL, PRO_CPU_NUM);

}


