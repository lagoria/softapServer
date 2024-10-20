#ifndef LCD_ST7735_H
#define LCD_ST7735_H

#define LCD_MAX_SIZE    128 * 128 * 2
#define LCD_WIDTH       128
#define LCD_HEIGHT      128

// Some ready-made 16-bit (RGB-565) color settings:
// 颜色都经过翻转，低位在前
#define	COLOR_BLACK         0x0000      // 黑
#define COLOR_WHITE         0xFFFF      // 白
#define	COLOR_RED           0x00F8      // 红
#define	COLOR_GREEN         0xE007      // 绿
#define	COLOR_BLUE          0x1F00      // 蓝
#define COLOR_CYAN          0xFF07      // 青
#define COLOR_MAGENTA       0x1FF8      // 品红
#define COLOR_YELLOW        0xE0FF      // 黄
#define	COLOR_GRAY          0x1084      // 灰
#define	COLOR_OLIVE         0x0084      // 黄绿
#define COLOR_VIOLET        0x1A90      // 深紫罗兰
#define COLOR_AZURE         0x7D86      // 天蓝
#define COLOR_GOLDEN        0xA0FE      // 金
#define COLOR_ORANGE        0x20FD      // 橙

// 显示类型定义
#define LCD_CLEAR           1
#define LCD_LINE            2
#define LCD_REC             3
#define LCD_CIRCLE          4
#define LCD_CHAR            5
#define LCD_STRING          6
#define LCD_PICTURE         7


typedef struct {
    uint8_t  *data;                     // 图像数据
    uint32_t len;                       // 数据长度
    uint16_t width;                     // 图像宽度
    uint16_t height;                    // 图像高度
    uint8_t  type;                      // 显示类型
    uint16_t x;                         // 起始X坐标
    uint16_t y;                         // 起始Y坐标
    uint16_t x_end;                     // 终点X坐标
    uint16_t y_end;                     // 终点Y坐标
    uint16_t color;                     // 文字颜色
    uint16_t back_color;                // 背景颜色
} lcd_data_frame_t;


/*--------------------------------------------*/

// 初始化LCD
void lcd_st7735_init();
// 显示数据帧发送函数
void lcd_frame_display_data(lcd_data_frame_t *data);


#endif