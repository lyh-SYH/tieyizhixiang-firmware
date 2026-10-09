/*
 * 铁医智箱 · 板级配置文件(已按微雪官方参数填好,开箱即用)
 * -------------------------------------------------------------
 * 适用:Waveshare ESP32-S3-Touch-LCD-4.3(800×480 ST7262 RGB 屏 + GT911 触摸 + CH422G 扩展)
 * 格式:ESP32_Display_Panel v0.1.x 的 ESP_Panel_Board_Custom.h 模板
 * 参数来源:微雪官方板级配置(ESP32_Display_Panel 库收录的
 *         BOARD_WAVESHARE_ESP32_S3_TOUCH_LCD_4_3,作者 Waveshare@H-sw123)
 *
 * 注意:若你安装的 ESP32_Display_Panel 主版本不是 0.1.x(如 1.x),
 *      库会提示配置文件版本不兼容,此时请改用微雪示例包中
 *      09_lvgl_Porting 例程自带的同名文件替换本文件。
 */

#pragma once

// *INDENT-OFF*

/* ============ 微雪例程通用的便捷宏(.ino 初始化扩展芯片时使用) ============ */
#define I2C_MASTER_SCL_IO           9           /* I2C 时钟脚(CH422G / GT911 共用) */
#define I2C_MASTER_SDA_IO           8           /* I2C 数据脚 */
#define I2C_MASTER_NUM              I2C_NUM_0   /* I2C 端口号 */
#define I2C_MASTER_FREQ_HZ          400000
#define GPIO_INPUT_IO_4             4           /* GT911 中断脚(复位时序用) */

/* CH422G 扩展芯片上的引脚编号(EXIOx) */
#define TP_RST  1   /* 触摸复位 */
#define LCD_BL  2   /* 背光开关 */
#define LCD_RST 3   /* 屏幕复位 */
#define SD_CS   4   /* SD 卡片选 */
#define USB_SEL 5   /* USB 切换 */

/* 触摸开关:1 启用触摸(触摸版),0 关闭(非触摸版) */
#define ESP_OPEN_TOUCH              1

/* Set to 1 if using a custom board */
#define ESP_PANEL_USE_CUSTOM_BOARD       (1)         // 0/1

#if ESP_PANEL_USE_CUSTOM_BOARD

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////// Please update the following macros to configure the LCD panel /////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/* Set to 1 when using an LCD panel */
#define ESP_PANEL_USE_LCD           (1)     // 0/1

#if ESP_PANEL_USE_LCD
/**
 * LCD Controller Name. Choose one of the following:
 *      - EK9716B
 *      - GC9A01, GC9B71, GC9503
 *      - ILI9341
 *      - NV3022B
 *      - SH8601
 *      - SPD2010
 *      - ST7262, ST7701, ST7789, ST7796, ST77916, ST77922
 */
#define ESP_PANEL_LCD_NAME          ST7262

/* LCD resolution in pixels */
#define ESP_PANEL_LCD_WIDTH         (800)
#define ESP_PANEL_LCD_HEIGHT        (480)

/* LCD Bus Settings */
/**
 * Set to 1 if only the RGB interface is used without the 3-wire SPI interface
 */
#define ESP_PANEL_LCD_BUS_SKIP_INIT_HOST    (1)     // 0/1
/**
 * LCD Bus Type. Choose one of the following:
 *      - ESP_PANEL_BUS_TYPE_I2C (not ready)
 *      - ESP_PANEL_BUS_TYPE_SPI
 *      - ESP_PANEL_BUS_TYPE_QSPI
 *      - ESP_PANEL_BUS_TYPE_I80 (not ready)
 *      - ESP_PANEL_BUS_TYPE_RGB (only supported for ESP32-S3)
 */
#define ESP_PANEL_LCD_BUS_TYPE      (ESP_PANEL_BUS_TYPE_RGB)

#if ESP_PANEL_LCD_BUS_TYPE == ESP_PANEL_BUS_TYPE_RGB

    #define ESP_PANEL_LCD_RGB_CLK_HZ            (16 * 1000 * 1000)
    #define ESP_PANEL_LCD_RGB_HPW               (4)
    #define ESP_PANEL_LCD_RGB_HBP               (8)
    #define ESP_PANEL_LCD_RGB_HFP               (8)
    #define ESP_PANEL_LCD_RGB_VPW               (4)
    #define ESP_PANEL_LCD_RGB_VBP               (8)
    #define ESP_PANEL_LCD_RGB_VFP               (8)
    #define ESP_PANEL_LCD_RGB_PCLK_ACTIVE_NEG   (1)     // 0: rising edge, 1: falling edge

                                                        // | 8-bit RGB888 | 16-bit RGB565 |
                                                        // |--------------|---------------|
    #define ESP_PANEL_LCD_RGB_DATA_WIDTH        (16)    // |      8       |      16       |
    #define ESP_PANEL_LCD_RGB_PIXEL_BITS        (16)    // |      24      |      16       |

    #define ESP_PANEL_LCD_RGB_FRAME_BUF_NUM     (1)     // 1/2/3
    #define ESP_PANEL_LCD_RGB_BOUNCE_BUF_SIZE   (ESP_PANEL_LCD_WIDTH * 10)
                                                        // Bounce buffer size in bytes, used to avoid screen drift.
                                                        // The size must satisfy `width * height = size * N`(N 为偶数)。
    #define ESP_PANEL_LCD_RGB_IO_HSYNC          (46)
    #define ESP_PANEL_LCD_RGB_IO_VSYNC          (3)
    #define ESP_PANEL_LCD_RGB_IO_DE             (5)     // -1 if not used
    #define ESP_PANEL_LCD_RGB_IO_PCLK           (7)
    #define ESP_PANEL_LCD_RGB_IO_DISP           (-1)    // -1 if not used

                                                        // | RGB565 | RGB666 | RGB888 |
                                                        // |--------|--------|--------|
    #define ESP_PANEL_LCD_RGB_IO_DATA0          (14)    // |   B0   |  B0-1  |   B0-3 |
    #define ESP_PANEL_LCD_RGB_IO_DATA1          (38)    // |   B1   |  B2    |   B4   |
    #define ESP_PANEL_LCD_RGB_IO_DATA2          (18)    // |   B2   |  B3    |   B5   |
    #define ESP_PANEL_LCD_RGB_IO_DATA3          (17)    // |   B3   |  B4    |   B6   |
    #define ESP_PANEL_LCD_RGB_IO_DATA4          (10)    // |   B4   |  B5    |   B7   |
    #define ESP_PANEL_LCD_RGB_IO_DATA5          (39)    // |   G0   |  G0    |   G0-2 |
    #define ESP_PANEL_LCD_RGB_IO_DATA6          (0)     // |   G1   |  G1    |   G3   |
    #define ESP_PANEL_LCD_RGB_IO_DATA7          (45)    // |   G2   |  G2    |   G4   |
#if ESP_PANEL_LCD_RGB_DATA_WIDTH > 8
    #define ESP_PANEL_LCD_RGB_IO_DATA8          (48)    // |   G3   |  G3    |   G5   |
    #define ESP_PANEL_LCD_RGB_IO_DATA9          (47)    // |   G4   |  G4    |   G6   |
    #define ESP_PANEL_LCD_RGB_IO_DATA10         (21)    // |   G5   |  G5    |   G7   |
    #define ESP_PANEL_LCD_RGB_IO_DATA11         (1)     // |   R0   |  R0-1  |   R0-3 |
    #define ESP_PANEL_LCD_RGB_IO_DATA12         (2)     // |   R1   |  R2    |   R4   |
    #define ESP_PANEL_LCD_RGB_IO_DATA13         (42)    // |   R2   |  R3    |   R5   |
    #define ESP_PANEL_LCD_RGB_IO_DATA14         (41)    // |   R3   |  R4    |   R6   |
    #define ESP_PANEL_LCD_RGB_IO_DATA15         (40)    // |   R4   |  R5    |   R7   |
#endif

#endif /* ESP_PANEL_LCD_BUS_TYPE */

/* LCD Color Settings */
#define ESP_PANEL_LCD_COLOR_BITS    (16)        // 8/16/18/24
#define ESP_PANEL_LCD_BGR_ORDER     (0)         // 0: RGB, 1: BGR
#define ESP_PANEL_LCD_INEVRT_COLOR  (0)         // 0/1

/* LCD Transformation Flags */
#define ESP_PANEL_LCD_SWAP_XY       (0)         // 0/1
#define ESP_PANEL_LCD_MIRROR_X      (0)         // 0/1
#define ESP_PANEL_LCD_MIRROR_Y      (0)         // 0/1

/* LCD Other Settings */
/* Reset pin:屏幕复位由 .ino 通过 CH422G 手动完成,此处置 -1 */
#define ESP_PANEL_LCD_IO_RST          (-1)      // IO num of RESET pin, set to -1 if not use
#define ESP_PANEL_LCD_RST_LEVEL       (0)       // Active level. 0: low level, 1: high level

#endif /* ESP_PANEL_USE_LCD */

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////// Please update the following macros to configure the touch panel ///////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/* Set to 1 when using an touch panel */
#define ESP_PANEL_USE_TOUCH         (ESP_OPEN_TOUCH)
#if ESP_PANEL_USE_TOUCH
/**
 * Touch controller name. Choose one of the following:
 *      - CST816S
 *      - FT5x06
 *      - GT911, GT1151
 *      - ST1633, ST7123
 *      - TT21100
 *      - XPT2046
 */
#define ESP_PANEL_TOUCH_NAME        GT911

/* Touch resolution in pixels */
#define ESP_PANEL_TOUCH_H_RES       (ESP_PANEL_LCD_WIDTH)
#define ESP_PANEL_TOUCH_V_RES       (ESP_PANEL_LCD_HEIGHT)

/* Touch Panel Bus Settings */
/**
 * I2C 主机已由 .ino 初始化 CH422G 时装好,触摸驱动跳过重复初始化
 */
#define ESP_PANEL_TOUCH_BUS_SKIP_INIT_HOST      (1)     // 0/1
#define ESP_PANEL_TOUCH_BUS_TYPE            (ESP_PANEL_BUS_TYPE_I2C)
#if ESP_PANEL_TOUCH_BUS_TYPE == ESP_PANEL_BUS_TYPE_I2C

    #define ESP_PANEL_TOUCH_BUS_HOST_ID     (0)
    #define ESP_PANEL_TOUCH_I2C_ADDRESS     (0)     // 0 = GT911 默认地址 0x5D(复位时序已选定)
#if !ESP_PANEL_TOUCH_BUS_SKIP_INIT_HOST
    #define ESP_PANEL_TOUCH_I2C_CLK_HZ      (400 * 1000)
    #define ESP_PANEL_TOUCH_I2C_SCL_PULLUP  (1)
    #define ESP_PANEL_TOUCH_I2C_SDA_PULLUP  (1)
    #define ESP_PANEL_TOUCH_I2C_IO_SCL      (9)
    #define ESP_PANEL_TOUCH_I2C_IO_SDA      (8)
#endif

#endif /* ESP_PANEL_TOUCH_BUS_TYPE */

/* Touch Transformation Flags */
#define ESP_PANEL_TOUCH_SWAP_XY         (0)     // 0/1
#define ESP_PANEL_TOUCH_MIRROR_X        (0)     // 0/1
#define ESP_PANEL_TOUCH_MIRROR_Y        (0)     // 0/1

/* Touch Other Settings */
/* Reset pin:GT911 复位由 .ino 通过 CH422G 手动完成,此处置 -1 */
#define ESP_PANEL_TOUCH_IO_RST          (-1)
#define ESP_PANEL_TOUCH_RST_LEVEL       (0)
/* Interrupt pin:采用轮询读取,不使用中断 */
#define ESP_PANEL_TOUCH_IO_INT          (-1)
#define ESP_PANEL_TOUCH_INT_LEVEL       (0)

#endif /* ESP_PANEL_USE_TOUCH */

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////// Please update the following macros to configure the backlight ////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/* 背光由 .ino 通过 CH422G(LCD_BL = EXIO2)手动控制,此处关闭库内背光管理 */
#define ESP_PANEL_USE_BACKLIGHT         (0)         // 0/1
#if ESP_PANEL_USE_BACKLIGHT
#define ESP_PANEL_BACKLIGHT_IO          (45)
#define ESP_PANEL_BACKLIGHT_ON_LEVEL    (1)
#define ESP_PANEL_BACKLIGHT_IDLE_OFF    (0)
#define ESP_PANEL_LCD_BL_USE_PWM        (0)
#endif /* ESP_PANEL_USE_BACKLIGHT */

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////// Please update the following macros to configure the IO expander //////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/* CH422G 由 .ino 手动创建并初始化(需先于 panel 完成复位时序),此处关闭库内扩展芯片管理 */
#define ESP_PANEL_USE_EXPANDER          (0)         // 0/1
#if ESP_PANEL_USE_EXPANDER
#define ESP_PANEL_EXPANDER_NAME         CH422G
#define ESP_PANEL_EXPANDER_SKIP_INIT_HOST       (0)
#define ESP_PANEL_EXPANDER_HOST_ID              (0)
#define ESP_PANEL_EXPANDER_I2C_ADDRESS          (0x20)
#if !ESP_PANEL_EXPANDER_SKIP_INIT_HOST
    #define ESP_PANEL_EXPANDER_I2C_CLK_HZ       (400 * 1000)
    #define ESP_PANEL_EXPANDER_I2C_SCL_PULLUP   (1)
    #define ESP_PANEL_EXPANDER_I2C_SDA_PULLUP   (1)
    #define ESP_PANEL_EXPANDER_I2C_IO_SCL       (9)
    #define ESP_PANEL_EXPANDER_I2C_IO_SDA       (8)
#endif
#endif /* ESP_PANEL_USE_EXPANDER */

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////// Please utilize the following macros to execute any additional code if required. //////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// #define ESP_PANEL_BEGIN_START_FUNCTION( panel )
// #define ESP_PANEL_BEGIN_EXPANDER_START_FUNCTION( panel )
// #define ESP_PANEL_BEGIN_EXPANDER_END_FUNCTION( panel )
// #define ESP_PANEL_BEGIN_LCD_START_FUNCTION( panel )
// #define ESP_PANEL_BEGIN_LCD_END_FUNCTION( panel )
// #define ESP_PANEL_BEGIN_TOUCH_START_FUNCTION( panel )
// #define ESP_PANEL_BEGIN_TOUCH_END_FUNCTION( panel )
// #define ESP_PANEL_BEGIN_BACKLIGHT_START_FUNCTION( panel )
// #define ESP_PANEL_BEGIN_BACKLIGHT_END_FUNCTION( panel )
// #define ESP_PANEL_BEGIN_END_FUNCTION( panel )

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////// File Version ///////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/* 与 ESP32_Display_Panel v0.1.x 库内模板版本保持一致 */
#define ESP_PANEL_BOARD_CUSTOM_FILE_VERSION_MAJOR 0
#define ESP_PANEL_BOARD_CUSTOM_FILE_VERSION_MINOR 2
#define ESP_PANEL_BOARD_CUSTOM_FILE_VERSION_PATCH 2

#endif /* ESP_PANEL_USE_CUSTOM_BOARD */

// *INDENT-OFF*
