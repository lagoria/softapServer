/*---------------------------------------------------
    APP：软AP的TCP服务器，实现TCP套接字数据转发。
    JSON命令格式：
    {"command":"register","name":"HOST"}      // 设备身份验证
    {"command":"list"}    // 查看在线的客户端信息
    {"command":"uuid","name":"phone"}       // 获取客户端UUID
    {"transmit":"3"}        // 数据转发到ID为3的设备
------------------------------------------------------*/
#ifndef APP_CONFIG_H
#define APP_CONFIG_H


#define CONFIG_SOFTAP_SSID      "ESP-SOFTAP"
#define CONFIG_SOFTAP_PAS       "3325035137"

#define DEFAULT_ROUTER_SSID     "308"
#define DEFAULT_ROUTER_PWD      "308308308"

  
#define TCP_SERVER_PORT         8888        // tcp服务器监听端口
#define BROADCAST_PORT          8080        // udp广播地址监听端口

#define LOCAL_DEVICE_MARK       "SOFTAP_SERVER"  // 本机设备标识
#define MAX_CLIENT_NUM          6
#define MAX_BUFFER_SIZE         4 * 1024        // 发送接收缓存大小


/* -----------指令定义------------ */
#define CMD_KEY_STATUS              "status"
#define CMD_KEY_NAME                "name"
#define CMD_KEY_UDP_REQUEST         "request"
#define CMD_KEY_ID                  "id"
#define CMD_KEY_TRANSMIT            "transmit"
#define CMD_KEY_COMMAND             "command"
#define CMD_KEY_SSID                "ssid"
#define CMD_KEY_PWD                 "pwd"

#define CMD_STRING_REGISTER         "register"
#define CMD_STRING_LIST             "list"
#define CMD_STRING_UUID             "uuid"
#define CMD_STRING_ROUTER           "router"



/*--------------客户端数据处理事件定义------------*/
typedef enum {

/* -------服务器命令指令事件----*/
    CLIENT_DIRECT_EVT,          // 发送给服务器事件
    CMD_UNREGISTER_EVT,         // 未注册事件
    CMD_REGISTER_EVT,           // 信息注册事件
    CMD_LIST_EVT,               // 获取客户端列表事件
    CMD_UUID_EVT,               // 获取客户端ID事件
    CMD_ROUTER_EVT,             // wifi sta帐号配置事件

/* -------客户端数据回应事件----*/
    CLIENT_TRANSMIT_EVT,        // 客户端数据转发事件
    CLIENT_NOT_FOUND_EVT,       // 目标设备未知事件
    CLIENT_FRAME_INVALID_EVT,   // 数据帧格式无效事件
    CLIENT_UNKNOWN_EVT,         // 未知事件

} client_evt_t;


/*--------------LCD屏相关配置------------------*/
#define LCD_SPI_HOST    SPI2_HOST
#define LCD_MAX_TRANSFER_SIZE       4 * 1024

// LCD backlight contorl
#define PIN_NUM_BCKL    2
#define LCD_PIN_CS      3
#define LCD_PIN_DC      4
#define LCD_PIN_RST     5
#define LCD_PIN_MOSI    6
#define LCD_PIN_CLK     7
#define LCD_PIN_MISO    -1

/** ------------TF卡相关配置-------------*/
#define TF_PIN_MISO     8
#define TF_PIN_MOSI     9     
#define TF_PIN_SCK      10
#define TF_PIN_CS       11          // TF卡片选



#endif /* APP_CONFIG_H */