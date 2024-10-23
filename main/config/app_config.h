#pragma once

namespace AppCfg {

constexpr char SOFTAP_SSID[]    = "ESP-SOFTAP";
constexpr char SOFTAP_PAWD[]    = "3325035137";

constexpr uint16_t SERVER_PORT  = 8888;
  
/* -----------指令定义------------ */
constexpr char JSON_KEY_STATUS[]    = "status";
constexpr char JSON_KEY_MARK[]      = "mark";
constexpr char JSON_KEY_CMD[]       = "cmd";
constexpr char JSON_KEY_ARGS[]      = "args";
constexpr char CMD_KEY_STATUS[]     = "status";


/*--------------LCD屏相关配置------------------*/
constexpr int LCD_SPI_HOST      = 1;            // SPI2_HOST
constexpr int LCD_MAXTRANS_SIZE = 4 * 1024;

// LCD backlight contorl
constexpr int LCD_PIN_BCKL      = 2;
constexpr int LCD_PIN_CS        = 3;
constexpr int LCD_PIN_DC        = 4;
constexpr int LCD_PIN_RST       = 5;
constexpr int LCD_PIN_MOSI      = 6;
constexpr int LCD_PIN_CLK       = 7;
constexpr int LCD_PIN_MISO      = -1;


/** ------------TF卡相关配置-------------*/
constexpr int TF_PIN_MISO       = 8;
constexpr int TF_PIN_MOSI       = 9;
constexpr int TF_PIN_SCK        = 10;
constexpr int TF_PIN_CS         = 11;

}
