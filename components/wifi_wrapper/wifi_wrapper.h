#ifndef WIFI_WRAPPER_H
#define WIFI_WRAPPER_H

#include "esp_wifi.h"
#include "esp_netif.h"
#include "netdb.h"


#define WIFI_WRAPPER_NVS_NAMESPACE      "wifi_wrapper"
#define WIFI_WRAPPER_NVS_KEY_SSID       "ssid"
#define WIFI_WRAPPER_NVS_KEY_PASS       "password"

#define WIFI_WRAPPER_CONFIG_CHANNEL     7           // WiFi广播信道
#define WIFI_WRAPPER_SOFTAP_MAXCON      8           // 最多连接数量（最多10个）

typedef enum {
    CONN_5X = 5,
    CONN_10X = 10,
    CONN_20X = 20,
    CONN_CONTINUOUS = 255,
} wifi_wrapper_retry_t;

typedef struct {
    uint8_t ssid[32];
    uint8_t password[64];
} wifi_wrapper_account_cfg_t;


typedef struct wifi_sta_info_wrapper {  
    uint8_t mac[6];  
    uint8_t aid;
    struct wifi_sta_info_wrapper *next;  
} wifi_wrapper_sta_info_t;


typedef struct {
    uint8_t router_ssid[32];
    uint8_t router_pwd[64];
    uint8_t softap_ssid[32];
    uint8_t softap_pwd[64];
    bool enable_nat;
} wifi_wrapper_apsta_cfg_t;

/*--------------function declarations-----------*/

/**
 * @brief 初始化软AP,创建WiFi
 * 
 * @param account wifi账户配置
 */
void wifi_wrapper_ap_init(wifi_wrapper_account_cfg_t *account);

/**
 * @brief Get the local ip addr object
 * 
 * @return char* 
 */
char* wifi_wrapper_get_local_ip(void);

/**
 * @brief Get WiFi mode
 * 
 * @return wifi_mode_t mode
 */
wifi_mode_t wifi_wrapper_get_mode();

/**
 * @brief Get the wifi sta list info object
 * 
 * @return wifi_wrapper_sta_info_t* 
 */
wifi_wrapper_sta_info_t * wifi_wrapper_get_sta_info(void);

/*--------------station------------------*/

// 初始化wifi station模式
void wifi_wrapper_sta_init(wifi_wrapper_account_cfg_t *account);

/**
 * @brief wait for wifi connected
 * 
 * @param wait_time wait time (ticks)
 * @return true connected 
 * @return false disconnected
 */
bool wifi_wrapper_wait_connected(uint32_t wait_time);

/**
 * @brief wait for wifi disconnected
 * 
 * @param wait_time wait time (ticks)
 * @return true disconnected 
 * @return false connected
 */
bool wifi_wrapper_wait_disconnected(uint32_t wait_time);

/**
 * @brief activate wifi connect
 * 
 * @param count connect retry count
 */
void wifi_wrapper_connect(wifi_wrapper_retry_t count);

/**
 * @brief 断开wifi连接
 * 
 */
void wifi_wrapper_disconnect();

/**
 * @brief 重置wifi sta的连接
 * 
 * @param account 连接账户
 */
void wifi_wrapper_connect_reset(wifi_wrapper_account_cfg_t *account);


/**
 * @brief NVS flash 保存wife配置信息
 * 
 * @param data 保存的信息
 * @return int 0：成功，others：失败
 */
int wifi_wrapper_nvs_store_account(wifi_wrapper_account_cfg_t *data);

/**
 * @brief NVS flash中加载wife配置信息
 * 
 * @param data 加载到的数据
 * @return int 0：成功，others：失败
 */
int wifi_wrapper_nvs_load_account(wifi_wrapper_account_cfg_t *data);


/**
 * @brief wifi softAP STA共存模式
 * 
 * @param config 配置结构体参数
 */
void wifi_wrapper_apsta_init(wifi_wrapper_apsta_cfg_t *config);

#endif