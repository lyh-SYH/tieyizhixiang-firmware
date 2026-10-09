/**
 * 铁医智箱 · 智能售药机 主程序(全部交互在触摸屏完成)
 * ----------------------------------------------------------------
 * 业务流:待机首页 → 屏上选药(分类/购物车)→ 去结算
 *        → 扫码支付页(手机微信扫屏上二维码付款)→ 支付成功
 *        → 确认出药 → 四阶段出药动画 → 完成回首页
 *
 * 硬件 : Waveshare ESP32-S3-Touch-LCD-4.3(800x480 RGB 屏 + GT911 电容触摸)
 * 框架 : Arduino(esp32 core >= 3.0.6)
 * 依赖 : LVGL v8.4.0(微雪离线包)+ ESP32_Display_Panel >= 0.1.4 + ESP32_IO_Expander >= 0.0.4
 *
 * 板级配置:ESP_Panel_Board_Custom.h 已按微雪官方参数填好并随工程附带,无需再手动复制。
 */
#include <Arduino.h>
#include <ESP_Panel_Library.h>
#include <ESP_IOExpander_Library.h>
#include <lvgl.h>
#include "lv_conf.h"
#include "ESP_Panel_Board_Custom.h"   // 已随工程附带(微雪官方参数)
#include "ui_app.h"

#define LCD_H_RES        800
#define LCD_V_RES        480
#define LVGL_BUF_LINES   40
#define LVGL_TICK_MS     2

static ESP_Panel       *panel    = nullptr;
static ESP_IOExpander  *expander = nullptr;
static SemaphoreHandle_t lvgl_mux = nullptr;

/* LVGL 刷新回调 */
static void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    ESP_PanelLcd *lcd = (ESP_PanelLcd *)drv->user_data;
    lcd->drawBitmap(area->x1, area->y1, area->x2 + 1, area->y2 + 1, color_map);
    lv_disp_flush_ready(drv);
}

/* LVGL 触摸回调 */
static void lvgl_touch_cb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    ESP_PanelTouch *touch = (ESP_PanelTouch *)drv->user_data;
    ESP_PanelTouchPoint pt;
    if (touch->readPoints(&pt, 1) > 0) {
        data->point.x = pt.x;
        data->point.y = pt.y;
        data->state   = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

static void lvgl_tick_cb(void *arg) { lv_tick_inc(LVGL_TICK_MS); }

void setup()
{
    Serial.begin(115200);
    Serial.println("[铁医智箱] 启动…");

    /* 1. CH422G IO 扩展芯片 */
    expander = new ESP_IOExpander_CH422G((i2c_port_t)I2C_MASTER_NUM,
                                         ESP_IO_EXPANDER_I2C_CH422G_ADDRESS_000,
                                         I2C_MASTER_SCL_IO, I2C_MASTER_SDA_IO);
    expander->init();
    expander->begin();
    expander->multiPinMode(TP_RST | LCD_BL | LCD_RST | SD_CS | USB_SEL, OUTPUT);
    expander->multiDigitalWrite(TP_RST | LCD_BL | LCD_RST, HIGH);
    delay(100);

    /* GT911 复位时序(微雪官方要求,必须执行,否则触摸屏无法识别) */
    pinMode(GPIO_INPUT_IO_4, OUTPUT);
    expander->multiDigitalWrite(TP_RST | LCD_RST, LOW);
    delay(100);
    digitalWrite(GPIO_INPUT_IO_4, LOW);
    delay(100);
    expander->multiDigitalWrite(TP_RST | LCD_RST, HIGH);
    delay(200);
    pinMode(GPIO_INPUT_IO_4, INPUT);   /* 释放中断脚 */

    /* 背光先关闭,UI 就绪后再打开 */
    expander->digitalWrite(LCD_BL, LOW);

    /* 2. 显示面板(RGB LCD + GT911 触摸) */
    panel = new ESP_Panel();
    panel->init();
    panel->begin();

    /* 3. LVGL 初始化 */
    lv_init();

    static lv_disp_draw_buf_t draw_buf;
    static lv_color_t *buf = (lv_color_t *)heap_caps_malloc(
        LCD_H_RES * LVGL_BUF_LINES * sizeof(lv_color_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, LCD_H_RES * LVGL_BUF_LINES);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res  = LCD_H_RES;
    disp_drv.ver_res  = LCD_V_RES;
    disp_drv.flush_cb = lvgl_flush_cb;
    disp_drv.draw_buf = &draw_buf;
    disp_drv.user_data = panel->getLcd();
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type      = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb   = lvgl_touch_cb;
    indev_drv.user_data = panel->getTouch();
    lv_indev_drv_register(&indev_drv);

    static esp_timer_handle_t tick_timer;
    esp_timer_create_args_t tick_args = {};
    tick_args.callback = &lvgl_tick_cb;
    tick_args.name     = "lvgl_tick";
    esp_timer_create(&tick_args, &tick_timer);
    esp_timer_start_periodic(tick_timer, LVGL_TICK_MS * 1000);

    /* 4. 开背光 */
    expander->digitalWrite(LCD_BL, HIGH);

    /* 5. 创建铁医智箱 UI */
    lvgl_mux = xSemaphoreCreateRecursiveMutex();
    if (xSemaphoreTakeRecursive(lvgl_mux, portMAX_DELAY) == pdTRUE) {
        tieyi_ui_create();
        xSemaphoreGiveRecursive(lvgl_mux);
    }
    Serial.println("[铁医智箱] UI 已加载");
}

void loop()
{
    if (xSemaphoreTakeRecursive(lvgl_mux, portMAX_DELAY) == pdTRUE) {
        lv_timer_handler();
        xSemaphoreGiveRecursive(lvgl_mux);
    }
    delay(5);
}
