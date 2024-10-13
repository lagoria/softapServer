#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_mac.h"
#include "esp_system.h"
#include "esp_event.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "dhcpserver/dhcpserver.h"

#include "wifi_wrapper.h"

#define DEFAULT_DNS             "223.5.5.5"


static const char *TAG = "wifi_wrapper";


static wifi_wrapper_sta_info_t *sta_list = NULL;      // 链表指针
static char local_ip_str[INET_ADDRSTRLEN] = {0};
static wifi_mode_t wifi_mode = WIFI_MODE_NULL;
static int16_t retry_conn_count;

static esp_netif_t *ap_netif = NULL;
static esp_netif_t *sta_netif = NULL;

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_START_BIT          BIT0
#define WIFI_CONNECTED_BIT      BIT1
#define WIFI_DISCONNECTED_BIT   BIT2



int wifi_wrapper_nvs_store_account(wifi_wrapper_account_cfg_t *data)
{
    nvs_handle_t nvs_handle;
    int err = nvs_open(WIFI_WRAPPER_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_set_blob(nvs_handle, WIFI_WRAPPER_NVS_KEY_SSID, data->ssid, sizeof(data->ssid));
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_blob(nvs_handle, WIFI_WRAPPER_NVS_KEY_PASS, data->password, sizeof(data->password));
    if (err != ESP_OK) {
        return err;
    }
    nvs_commit(nvs_handle);     // 更改写入物理存储
    nvs_close(nvs_handle);
    return ESP_OK;
}


int wifi_wrapper_nvs_load_account(wifi_wrapper_account_cfg_t *data)
{
    nvs_handle_t nvs_handle;
    size_t load_len = 0;
    int err = nvs_open(WIFI_WRAPPER_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        return err;
    }

    load_len = sizeof(data->ssid);
    err = nvs_get_blob(nvs_handle, WIFI_WRAPPER_NVS_KEY_SSID, data->ssid, &load_len);
    if (err != ESP_OK) {
        return err;
    }
    load_len = sizeof(data->password);
    err = nvs_get_blob(nvs_handle, WIFI_WRAPPER_NVS_KEY_PASS, data->password, &load_len);
    if (err != ESP_OK) {
        return err;
    }
    nvs_close(nvs_handle);
    return ESP_OK;
} 


static void add_wifi_sta_list_node(uint8_t *mac, uint8_t aid) 
{  
    wifi_wrapper_sta_info_t *new_sta = (wifi_wrapper_sta_info_t*)malloc(sizeof(wifi_wrapper_sta_info_t));  
    if (new_sta == NULL) return;
    if (sta_list == NULL) {
        sta_list = new_sta;
    } else {
        wifi_wrapper_sta_info_t *current;
        current = sta_list;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = new_sta;
    }

    // 复制节点信息
    new_sta->aid = aid;
    memcpy(new_sta->mac, mac, 6);
    new_sta->next = NULL;
}

static void delete_wifi_sta_list_node(uint8_t aid)
{
    if (sta_list == NULL) return;
    wifi_wrapper_sta_info_t *current, *prev;
    current = sta_list;
    // 判断是否为第一个节点
    if (current != NULL && current->aid == aid) {
        sta_list = current->next;
        free(current);
        return;
    }
    while (current->next != NULL) {
        prev = current;
        current = current->next;
        if (current != NULL && current->aid == aid) {
            prev->next = current->next;
            free(current);
            break;
        }
    }
}

char* wifi_wrapper_get_local_ip(void)
{
    return local_ip_str;
}

wifi_wrapper_sta_info_t * wifi_wrapper_get_sta_info(void)
{
    return sta_list;
}

wifi_mode_t wifi_wrapper_get_mode()
{
    return wifi_mode;
}

void wifi_wrapper_connect(wifi_wrapper_retry_t count)
{
    retry_conn_count = count;
    esp_wifi_connect();
    xEventGroupSetBits(wifi_event_group, WIFI_START_BIT);
}

bool wifi_wrapper_wait_connected(uint32_t wait_time)
{
    if (wifi_mode == WIFI_MODE_AP) return true;

    EventBits_t bits = xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, wait_time);
    if (bits & WIFI_CONNECTED_BIT) {
        return true;
    } else {
        return false;
    }
}

bool wifi_wrapper_wait_disconnected(uint32_t wait_time)
{
    if (wifi_mode == WIFI_MODE_AP) return false;

    EventBits_t bits = xEventGroupWaitBits(wifi_event_group, WIFI_DISCONNECTED_BIT, pdFALSE, pdFALSE, wait_time);
    if (bits & WIFI_DISCONNECTED_BIT) {
        return true;
    } else {
        return false;
    }
}

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    /* ---------------WiFi AP event----------------*/
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_START) {
        ESP_LOGI(TAG, "softap start");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
                 MAC2STR(event->mac), event->aid);

        add_wifi_sta_list_node(event->mac, event->aid);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d",
                 MAC2STR(event->mac), event->aid);

        delete_wifi_sta_list_node(event->aid);
    }

    /* ---------------WiFi Station event----------------*/

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        /* wifi disconnected event */
        xEventGroupSetBits(wifi_event_group, WIFI_DISCONNECTED_BIT);
        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);

        if (retry_conn_count != CONN_CONTINUOUS) {
            retry_conn_count--;
        }
        if (retry_conn_count > 0) {
            /* reconnection */
            esp_wifi_connect();
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            /* wait wifi reset */
            ESP_LOGI(TAG, "wifi disconnected.");
        }
        
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_STOP) {
        /* wifi stop event */
        ESP_LOGD(TAG, "wifi stoped.");
        xEventGroupClearBits(wifi_event_group, WIFI_START_BIT);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI(TAG, "wifi connected.");
        /* wifi connection successful */
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        sprintf(local_ip_str, IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "got ip: %s" , local_ip_str);

        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
        xEventGroupClearBits(wifi_event_group, WIFI_DISCONNECTED_BIT);

#if IP_NAPT
        esp_netif_dns_info_t dns;
        if (esp_netif_get_dns_info(sta_netif, ESP_NETIF_DNS_MAIN, &dns) == ESP_OK) {
            esp_netif_set_dns_info(ap_netif, ESP_NETIF_DNS_MAIN, &dns);
            ESP_LOGI(TAG, "set dns to:" IPSTR, IP2STR(&(dns.ip.u_addr.ip4)));
        }
#endif
    }
}


void wifi_wrapper_disconnect()
{
    retry_conn_count = 0;
    if (wifi_wrapper_wait_disconnected(0) == false) {
        esp_wifi_disconnect();
    }
}


void wifi_wrapper_connect_reset(wifi_wrapper_account_cfg_t *account)
{
    if (account == NULL) return;
    wifi_config_t wifi_config;
    esp_wifi_get_config(WIFI_IF_STA, &wifi_config);

    memcpy(wifi_config.sta.ssid, account->ssid, sizeof(wifi_config.sta.ssid));
    memcpy(wifi_config.sta.password, account->password, sizeof(wifi_config.sta.password));

    
    esp_wifi_stop();
    /* wait for wifi disconnected */
    wifi_wrapper_wait_disconnected(portMAX_DELAY);
    
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
}

void wifi_wrapper_sta_init(wifi_wrapper_account_cfg_t *account)
{
    wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config));
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    if (account != NULL) {
        memcpy(wifi_config.sta.ssid, account->ssid, sizeof(wifi_config.sta.ssid));
        memcpy(wifi_config.sta.password, account->password, sizeof(wifi_config.sta.password));
    } else {
        /* NVS 加载配置信息 */
        wifi_wrapper_account_cfg_t config = {0};
        int status = wifi_wrapper_nvs_load_account(&config);
        if (status == 0) {
            memcpy(wifi_config.sta.ssid, config.ssid, sizeof(wifi_config.sta.ssid));
            memcpy(wifi_config.sta.password, config.password, sizeof(wifi_config.sta.password));
            ESP_LOGI(TAG, "Use NVS ssid: %s,password: %s", config.ssid, config.password);
        }
    }

    wifi_mode = WIFI_MODE_STA;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_wrapper_sta_init finished.");

}


void wifi_wrapper_ap_init(wifi_wrapper_account_cfg_t *account)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *netif = esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .channel = WIFI_WRAPPER_CONFIG_CHANNEL,
            .max_connection = WIFI_WRAPPER_SOFTAP_MAXCON,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
            .ssid_hidden = false,
            .beacon_interval = 100,
        },
    };
    memcpy(wifi_config.ap.ssid, account->ssid, sizeof(wifi_config.ap.ssid));
    memcpy(wifi_config.ap.password, account->password, sizeof(wifi_config.ap.password));
    wifi_config.ap.ssid_len = strlen((char *)wifi_config.ap.ssid);

    if (strlen((char *)wifi_config.ap.password) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    wifi_mode = WIFI_MODE_AP;

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_wrapper_ap_init finished. SSID:%s password:%s channel:%d",
             account->ssid, account->password, WIFI_WRAPPER_CONFIG_CHANNEL);

    /* save local ipv4 address string */
    esp_netif_ip_info_t current_ip_info;
    esp_netif_get_ip_info(netif, &current_ip_info);
    inet_ntoa_r(current_ip_info.ip.addr, local_ip_str, sizeof(local_ip_str));
    ESP_LOGI(TAG, "Local IP address: %s", local_ip_str);
}


void wifi_wrapper_apsta_init(wifi_wrapper_apsta_cfg_t *config)
{
    wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ap_netif = esp_netif_create_default_wifi_ap();
    sta_netif = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        NULL));

    /* ESP STATION CONFIG */
    wifi_config_t sta_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .bssid_set = false,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    if (config->router_ssid[0] != 0) {
        memcpy(sta_config.sta.ssid, config->router_ssid, sizeof(sta_config.sta.ssid));
        memcpy(sta_config.sta.password, config->router_pwd, sizeof(sta_config.sta.password));
    } else {
        /* NVS 加载配置信息 */
        wifi_wrapper_account_cfg_t router_config = {0};
        int status = wifi_wrapper_nvs_load_account(&router_config);
        if (status == 0) {
            memcpy(sta_config.sta.ssid, router_config.ssid, sizeof(sta_config.sta.ssid));
            memcpy(sta_config.sta.password, router_config.password, sizeof(sta_config.sta.password));
            ESP_LOGI(TAG, "Use NVS ssid: %s, password: %s", router_config.ssid, router_config.password);
        }
    }

    /* ESP AP CONFIG */
    wifi_config_t ap_config = {
        .ap = {
            .channel = WIFI_WRAPPER_CONFIG_CHANNEL,
            .max_connection = WIFI_WRAPPER_SOFTAP_MAXCON,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
            .ssid_hidden = false,
            .beacon_interval = 100,
        },
    };
    memcpy(ap_config.ap.ssid, config->softap_ssid, sizeof(ap_config.ap.ssid));
    memcpy(ap_config.ap.password, config->softap_pwd, sizeof(ap_config.ap.password));

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &sta_config));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &ap_config));

    ESP_ERROR_CHECK(esp_wifi_start());

    if (config->enable_nat) {
        // Enable DNS (offer) for dhcp server
        dhcps_offer_t dhcps_dns_value = OFFER_DNS;
        esp_netif_dhcps_option(ap_netif, ESP_NETIF_OP_SET, ESP_NETIF_DOMAIN_NAME_SERVER, &dhcps_dns_value, sizeof(dhcps_dns_value));

        // Set custom dns server address for dhcp server
        esp_netif_dns_info_t dnsserver;
        dnsserver.ip.u_addr.ip4.addr = esp_ip4addr_aton(DEFAULT_DNS);
        dnsserver.ip.type = ESP_IPADDR_TYPE_V4;
        esp_netif_set_dns_info(ap_netif, ESP_NETIF_DNS_MAIN, &dnsserver);

#if IP_NAPT
        // !!! 必须启动esp_wifi_start后再设置，否则ap无网络
        esp_netif_napt_enable(ap_netif);
        ESP_LOGI(TAG, "NAT is enabled.");
#endif
    }

    wifi_mode = WIFI_MODE_APSTA;

    ESP_LOGI(TAG, "wifi apsta init finished.");

    /* save local ipv4 address string */
    esp_netif_ip_info_t current_ip_info;
    esp_netif_get_ip_info(ap_netif, &current_ip_info);
    inet_ntoa_r(current_ip_info.ip.addr, local_ip_str, sizeof(local_ip_str));
    ESP_LOGI(TAG, "Local IP address: %s", local_ip_str);
}

