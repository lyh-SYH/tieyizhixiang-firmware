/**
 * 铁医智箱 UI 层头文件 —— LVGL v8.4
 * 全部交互在触摸屏完成:首页 → 选药 → 扫码支付 → 出药动画
 */
#ifndef UI_APP_H
#define UI_APP_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 创建铁医智箱全部界面(在 lv_init 与显示/触摸驱动注册完成后调用) */
void tieyi_ui_create(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_APP_H */
