#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"

#include "lcd_st7735.h"
#include "lcd_font.h"
#include "app_config.h"

#define TAG         "lcd_st7735"


#define ST_CMD_DELAY 0x80  // special signifier for command lists

/*
 The LCD needs a bunch of command/argument values to be initialized. They are stored in this struct.
*/
typedef struct {
    uint8_t cmd;
    uint8_t data[16];
    uint8_t databytes;  // No of data in data, 0xFF = end of cmds
} lcd_init_cmd_t;

static spi_device_handle_t lcd_spi_dev;

static uint8_t *lcd_tx_buffer = NULL;

// This function is called (in irq context!) just before a transmission starts. It will
// set the D/C line to the value indicated in the user field.
void lcd_spi_pre_transfer_callback(spi_transaction_t *t)
{
    int dc = (int)t->user;
    gpio_set_level(LCD_PIN_DC, dc);
}

static void lcd_spi_init(void)
{
    // ESP_LOGI(TAG, "Initializing bus SPI%d...", LCD_SPI_HOST+1);
    spi_bus_config_t buscfg = {
        .miso_io_num = LCD_PIN_MISO,
        .mosi_io_num = LCD_PIN_MOSI,
        .sclk_io_num = LCD_PIN_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_MAX_TRANSFER_SIZE
    };
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 16 * 1000 * 1000,      // Clock out at 16 MHz
        .mode = 0,                               // SPI mode 0
        .spics_io_num = LCD_PIN_CS,              // CS pin
        .queue_size = 3,                         // We want to be able to queue 3 transactions at a time
        .pre_cb = lcd_spi_pre_transfer_callback, // Specify pre-transfer callback to handle D/C line
    };
    // Initialize the SPI bus
    ESP_ERROR_CHECK( spi_bus_initialize(LCD_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO) );
    // Attach the LCD to the SPI bus
    ESP_ERROR_CHECK( spi_bus_add_device(LCD_SPI_HOST, &devcfg, &lcd_spi_dev) );

    gpio_config_t io_conf;
    // disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    // set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    // bit mask of the pins that you want to set,e.g.
    io_conf.pin_bit_mask = BIT64(LCD_PIN_DC) | BIT64(LCD_PIN_RST);
    // disable pull-down mode
    io_conf.pull_down_en = 0;
    // disable pull-up mode
    io_conf.pull_up_en = 0;
    // configure GPIO with the given settings
    gpio_config(&io_conf);
}

static void st7735_cmd(uint8_t cmd) 
{
    esp_err_t ret;
    spi_transaction_t t = {
        .length = 8,
        .flags = SPI_TRANS_USE_TXDATA,
        .tx_buffer = NULL,
        .tx_data[0] = cmd,
        .user = (void *)0,
    };
    ret = spi_device_transmit(lcd_spi_dev, &t);     // 异步传输
    assert(ret == ESP_OK);
}

/**
 * transmit data to st7735
*/
static void lcd_send_data(uint8_t *data, int len) 
{
    esp_err_t ret;
    spi_transaction_t t;
    if (len == 0) return;
    memset(&t, 0, sizeof(t));
    if (len > 4) {
        t.length = len * 8;
        t.tx_buffer = data;
        t.user = (void *)1;                 // D/C needs to be set to 1
    } else {
        t.length = len * 8;
        t.flags = SPI_TRANS_USE_TXDATA;
        t.user = (void *)1;
        memcpy(t.tx_data, data, len); 
    }
    
    ret = spi_device_transmit(lcd_spi_dev, &t);
    assert(ret == ESP_OK);
}



void lcd_st7735_init()
{
    /* malloc lcd_tx_buffer */
    lcd_tx_buffer = (uint8_t *)heap_caps_malloc(LCD_MAX_TRANSFER_SIZE, MALLOC_CAP_DMA);
    if (lcd_tx_buffer == NULL) {
        ESP_LOGE(TAG, "lcd_tx_buffer malloc failed");
    }

    /* spi interface init */
    lcd_spi_init();

    // Reset the display
    gpio_set_level(LCD_PIN_RST, 0);
    vTaskDelay(500 / portTICK_PERIOD_MS);
    gpio_set_level(LCD_PIN_RST, 1);
    vTaskDelay(500 / portTICK_PERIOD_MS);

    lcd_init_cmd_t st7735_init_cmds[] = {
        // software reset with delay
        {0x01, {0}, ST_CMD_DELAY},
        // Out of sleep mode with delay
        {0x11, {0}, ST_CMD_DELAY},
        // Framerate ctrl - normal mode. Rate = fosc/(1x2+40) * (LINE+2C+2D)
        {0xB1, {0x01, 0x2C, 0x2D}, 3},
        // Framerate ctrl - idle mode.  Rate = fosc/(1x2+40) * (LINE+2C+2D)
        {0xB2, {0x01, 0x2C, 0x2D}, 3},
        // Framerate - partial mode. Dot/Line inversion mode
        {0xB3, {0x01, 0x2C, 0x2D, 0x01, 0x2C, 0x2D}, 6},
        // Display inversion ctrl: No inversion
        {0xB4, {0x07}, 1},
        // Power control1 set GVDD: -4.6V, AUTO mode.
        {0xC0, {0xA2, 0x02, 0x84}, 3},
        // Power control2 set VGH/VGL: VGH25=2.4C VGSEL=-10 VGH=3 * AVDD
        {0xC1, {0xC5}, 1},
        // Power control3 normal mode(Full color): Op-amp current small, booster voltage
        {0xC2, {0x0A, 0x00}, 2},
        // Power control4 idle mode(8-colors): Op-amp current small & medium low
        {0xC3, {0x8A, 0x2A}, 2},
        // Power control5 partial mode + full colors
        {0xC4, {0x8A, 0xEE}, 2},
        // VCOMH VoltageVCOM control 1: VCOMH=0x0E=2.850
        {0xC5, {0x0E}, 1},
        // Display Inversion Off
        {0x20, {0}, 0},
        // Memory Data Access Control: top-bottom/left-right refresh
        {0x36, {0xC8}, 1},
        // Color mode, Interface Pixel Format: RGB-565, 16-bit/pixel
        {0x3A, {0x05}, 1},

        // Column Address Set: 2, 127
        {0x2A, {0x00, 0x00, 0x00, 0x7F}, 4},
        // Row Address Set: 1, 159
        {0x2B, {0x00, 0x00, 0x00, 0x9F}, 4},

        // Gamma Adjustments (pos. polarity). Not entirely necessary, but provides accurate colors.
        {0xE0,
        {0x0f, 0x1a, 0x0f, 0x18, 0x2f, 0x28, 0x20, 0x22, 0x1f, 0x1b, 0x23, 0x37, 0x00, 0x07, 0x02, 0x10},
        16},
        // Gamma Adjustments (neg. polarity). Not entirely necessary, but provides accurate colors.
        {0xE1,
        {0x0f, 0x1b, 0x0f, 0x17, 0x33, 0x2C, 0x29, 0x2e, 0x30, 0x30, 0x39, 0x3F, 0x00, 0x07, 0x03, 0x10},
        16},
        // Display On
        {0x29, {0}, ST_CMD_DELAY},
        {0, {0}, 0xFF},
    };

    // Send all the init commands
    int cmd = 0;
    while (st7735_init_cmds[cmd].databytes != 0xff) {
        st7735_cmd(st7735_init_cmds[cmd].cmd);
        lcd_send_data(st7735_init_cmds[cmd].data, st7735_init_cmds[cmd].databytes & 0x1F);
        if (st7735_init_cmds[cmd].databytes & ST_CMD_DELAY) {
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
        cmd++;
    }

    // Enable backlight
    gpio_set_level(PIN_NUM_BCKL, 1);
}

// 设置显示区域（x0,y0）->(x1,y1)
static void lcd_set_window(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1)
{
    uint8_t data[4];

    // 设置列地址范围
    st7735_cmd(0x2A);
    data[0] = 0x00;
    data[1] = x0 + 0x02;
    data[2] = 0x00;
    data[3] = x1 + 0x02;
    lcd_send_data(data, 4);

    // 设置行地址范围
    st7735_cmd(0x2B);
    data[0] = 0x00;
    data[1] = y0 + 0x03;
    data[2] = 0x00;
    data[3] = y1 + 0x03;
    lcd_send_data(data, 4);

    // 内存写入命令
    st7735_cmd(0x2C);
}


static void lcd_send_word(uint16_t data)
{
    static uint8_t temp[2];
    temp[0] = (uint8_t) (data & 0xFF);
    temp[1] = (uint8_t) (data >> 8);
    lcd_send_data(temp, 2);
}

void lcd_fill_screen(uint16_t color)
{
    uint32_t len = 0;
    lcd_set_window(0, 0, LCD_WIDTH - 1, LCD_HEIGHT -1);
    for (uint32_t i = 0; i < LCD_WIDTH * LCD_HEIGHT; i ++) {
        *(uint16_t *)&lcd_tx_buffer[len] = color;
        len += 2;
        if (len >= LCD_MAX_TRANSFER_SIZE) {
            len = 0;
            lcd_send_data(lcd_tx_buffer, LCD_MAX_TRANSFER_SIZE);        
        }
    }
    lcd_send_data(lcd_tx_buffer, len);
}

// 画一个像素
void lcd_draw_pixel(uint16_t x, uint16_t y, uint16_t color) 
{
    lcd_set_window(x, y, x, y);
    lcd_send_word(color);
}


// 画线
void lcd_draw_line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    int16_t dx = abs(x2 - x1);
    int16_t dy = abs(y2 - y1);
    int16_t sx = x1 < x2 ? 1 : -1;
    int16_t sy = y1 < y2 ? 1 : -1;
    int16_t err = dx - dy;
    while (x1 != x2 || y1 != y2) {
        lcd_draw_pixel(x1, y1, color);
        int16_t e2 = err * 2;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
    // 绘制终点坐标
    lcd_draw_pixel(x2, y2, color);
}

/**
 * lcd draw a fill rectangle
*/
static void lcd_draw_fill_rectangle(uint16_t x, uint16_t y, uint16_t x_end, uint16_t y_end, uint16_t color)
{
    uint32_t len = 0;
    uint32_t size = (x_end - x + 1) * (y_end - y + 1);
    lcd_set_window(x, y, x_end, y_end);
    for (uint32_t i = 0; i < size; i ++) {
        *(uint16_t *)&lcd_tx_buffer[len] = color;
        len += 2;
        if (len >= LCD_MAX_TRANSFER_SIZE) {
            len = 0;
            lcd_send_data(lcd_tx_buffer, LCD_MAX_TRANSFER_SIZE);        
        }
    }
    lcd_send_data(lcd_tx_buffer, len);

}


// 画圆
void lcd_draw_circle(uint16_t cx, uint16_t cy, uint16_t radius, uint16_t color)
{
    int16_t x = 0;
    int16_t y = radius;
    int16_t delta = 1 - 2 * radius;
    int16_t error = 0;
    while (y >= 0) {
        lcd_draw_pixel(cx + x, cy + y, color);
        lcd_draw_pixel(cx - x, cy + y, color);
        lcd_draw_pixel(cx + x, cy - y, color);
        lcd_draw_pixel(cx - x, cy - y, color);

        error = 2 * (delta + y) - 1;
        if (delta < 0 && error <= 0) {
            delta += 2 * ++x + 1;
            continue;
        }
        if (delta > 0 && error > 0) {
            delta += 1 - 2 * --y;
            continue;
        }
        x++;
        delta += 2 * (x - y);
        y--;
    }
}


// 显示单个字符
void lcd_show_char(uint16_t x,uint16_t y, char char_num, 
                    uint16_t color, uint16_t back_color)
{
    uint8_t index;
    uint32_t len = 0;
    uint8_t temp;
    index = char_num - ' ';         // 得到偏移后的值
    lcd_set_window(x, y, x + 8 - 1, y + 16 -1);
    for(uint8_t i = 0; i < 16; i++) {
        temp = ascii_1608[index][i];
        for(uint8_t j = 0; j < 8; j++) {
            if (temp & (0x01 << j)) {
                *(uint16_t *)&lcd_tx_buffer[len] = color;
            } else {
                *(uint16_t *)&lcd_tx_buffer[len] = back_color;
            }
            len += 2;
            if (len >= LCD_MAX_TRANSFER_SIZE) {
                len = 0;
                lcd_send_data(lcd_tx_buffer, LCD_MAX_TRANSFER_SIZE);        
            }
        }
    }
    lcd_send_data(lcd_tx_buffer, len);

}

// 显示字符串
void lcd_show_string(uint16_t x, uint16_t y, char *string,
                        uint16_t color, uint16_t back_color)
{
    uint32_t index = 0;
    while(string[index] != '\0') {
        if (x > (LCD_WIDTH - 8)) {      // 空间不够换行
            x = 4;
            y += 16;
        }
        if (string[index] == '\n') {
            x = 4;                      // 自定义偏移
            y += 16;
            index++;
            continue;
        }
        lcd_show_char(x, y, string[index], color, back_color);    
        x += 8;
        index ++;
    }
}


// 画一张图像
void lcd_draw_image(uint8_t *image_data, uint16_t width, uint16_t height)
{
    if ((width > LCD_WIDTH) || (height > LCD_HEIGHT)) {
        return;
    }

    // 使图像居中
    uint8_t begin_x = (LCD_WIDTH - width) / 2;
    uint8_t begin_y = (LCD_HEIGHT - height) / 2;
    lcd_set_window(begin_x, begin_y, begin_x - width - 1, begin_y + height - 1);
    lcd_send_data(image_data, width * height * 2);

}

void lcd_frame_display_data(lcd_data_frame_t *data)
{
    lcd_data_frame_t image = *data;
    switch(image.type) {
    case LCD_CLEAR: {       // 清屏
        lcd_fill_screen(image.color);
        break;
    }
    case LCD_LINE: {        // 画线
        lcd_draw_line(image.x, image.y, image.x_end, image.y_end, image.color);
        break;
    }
    case LCD_REC: {         // 画矩形
        lcd_draw_fill_rectangle(image.x, image.y, image.x_end, image.y_end, image.color);
        break;
    }
    case LCD_CIRCLE: {      // 画圆
        lcd_draw_circle(image.x, image.y, image.width, image.color);
        break;
    }
    case LCD_CHAR: {        // 显示字符
        lcd_show_char(image.x, image.y, (char)image.data[0], image.color, image.back_color);
        break;
    }
    case LCD_STRING: {      // 显示字符串
        lcd_show_string(image.x, image.y, (char *)image.data, image.color, image.back_color);
        break;
    }
    case LCD_PICTURE: {     // 显示图片
        lcd_draw_image(image.data, image.width, image.height);
        break;                    
    }
    default: break;
    }
}

