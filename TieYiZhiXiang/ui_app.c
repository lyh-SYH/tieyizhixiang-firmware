/**
 * 铁医智箱 UI 实现 —— LVGL v8.4(全部在触摸屏完成)
 * -------------------------------------------------
 * 业务流:待机首页 → 屏上选药(分类/购物车)→ 去结算
 *        → 扫码支付页(手机微信扫屏上二维码付款)→ 支付成功
 *        → 确认出药 → 四阶段出药动画 → 完成回首页
 *
 * 需要在 lv_conf.h 中打开:
 *   #define LV_FONT_SIMSUN_16_CJK       1   (中文显示)
 *   #define LV_USE_QRCODE               1   (支付二维码)
 *   #define LV_FONT_MONTSERRAT_20/28/32 1   (数字大字)
 * 性能相关(微雪官方建议):LV_MEM_CUSTOM / LV_MEMCPY_MEMSET_STD / LV_ATTRIBUTE_FAST_MEM
 */
#include "ui_app.h"
#include <string.h>
#include <stdio.h>

/* ==================== 配置 ==================== */
/* TODO(部署):换成你的微信支付收款码内容。
   方案A(最简单):微信收款商业版固定收款码链接,金额由顾客扫码后手输;
   方案B(推荐):微信支付商户平台 Native 支付,由后台按订单生成带金额的支付链接,
   ESP32 连网后从后台获取链接并调用 lv_qrcode_update() 刷新二维码。 */
#define PAY_QR_TEXT    "https://pay.example.com/tyzx001  ← 部署时替换为真实微信支付链接"
#define BOX_NO         "TYZX-001"
#define UI_DEMO_BUTTON 1   /* 1:支付页显示「模拟支付成功」演示按钮;正式部署改 0 */
#define PAY_TIMEOUT_S  120 /* 支付倒计时(秒) */

/* ==================== 主题 ==================== */
#define C_BG        lv_color_hex(0xF2F6F9)
#define C_TEAL      lv_color_hex(0x0EA486)
#define C_TEAL_D    lv_color_hex(0x0E7C6B)
#define C_DARK      lv_color_hex(0x12263A)
#define C_GRAY      lv_color_hex(0x5B6B80)
#define C_GRAY_L    lv_color_hex(0x8A98AB)
#define C_RED       lv_color_hex(0xE8532F)
#define C_AMBER     lv_color_hex(0xFFD23E)
#define C_GREEN     lv_color_hex(0x0A8F5F)
#define C_WX        lv_color_hex(0x09BB07)
#define C_CHUTE     lv_color_hex(0x0B1526)
#define C_LINE      lv_color_hex(0x2C3F5C)

#define F_CHN       (&lv_font_simsun_16_cjk)
#define F_NUM_P     (&lv_font_montserrat_20)
#define F_NUM_M     (&lv_font_montserrat_28)
#define F_NUM_L     (&lv_font_montserrat_32)

#define SCR_W 800
#define SCR_H 480
#define BAR_H 44

/* ==================== 药品数据 ==================== */
typedef struct {
    const char *name;
    const char *spec;
    uint16_t    price;      /* 单位:分 */
    uint8_t     stock;
    uint8_t     cat;        /* 1感冒发热 2肠胃不适 3外伤护理 4营养防护 */
    lv_color_t  icon_bg;
} med_t;

#define MED_COUNT 9
static const med_t MEDS[MED_COUNT] = {
    { "布洛芬缓释胶囊", "0.3g×20粒/盒", 1850, 12, 1, { 0xE8,0xF6,0xF3 } },
    { "感冒灵颗粒",     "10g×9袋/盒",   1200, 20, 1, { 0xFD,0xEE,0xE8 } },
    { "对乙酰氨基酚片", "0.5g×16片/盒",  980, 15, 1, { 0xEE,0xF2,0xFD } },
    { "藿香正气水",     "10ml×10支/盒",  990, 18, 2, { 0xFD,0xF6,0xE8 } },
    { "蒙脱石散",       "3g×10袋/盒",   1580,  9, 2, { 0xF0,0xEA,0xFD } },
    { "创可贴",         "20片/盒",       650, 30, 3, { 0xFD,0xEE,0xF2 } },
    { "碘伏棉签",       "50支/罐",       800,  0, 3, { 0xE8,0xF3,0xFD } },
    { "维生素C片",      "100片/瓶",     2200, 14, 4, { 0xFD,0xF1,0xE4 } },
    { "医用外科口罩",   "10只/包",      1000, 25, 4, { 0xEA,0xFA,0xF1 } },
};
static const char *CAT_NAMES[5] = { "全部", "感冒发热", "肠胃不适", "外伤护理", "营养防护" };

/* ==================== 全局状态 ==================== */
static lv_obj_t *scr_home, *scr_shop, *scr_pay, *scr_disp;
static lv_obj_t *clk_lbl[4];
static int       clk_cnt = 0;

/* 选购页 */
static uint8_t   cart_qty[MED_COUNT];
static int       cur_tab = 0;
static lv_obj_t *tab_btn[5];
static lv_obj_t *grid_box;
/* 每张可见卡片的局部引用:只原地更新/显隐,绝不在事件回调中整体删除重建 */
static lv_obj_t *card_add[MED_COUNT], *card_step[MED_COUNT], *card_qn[MED_COUNT];
static lv_obj_t *cart_badge, *cart_total_lbl, *cart_info_lbl, *checkout_btn;

/* 支付页 */
static lv_obj_t *pay_amt_lbl, *pay_cnt_lbl, *pay_list, *pay_total_lbl;
static lv_timer_t *pay_timer = NULL;
static int       pay_left = PAY_TIMEOUT_S;

/* 出药页 */
static lv_obj_t *disp_list, *disp_state_lbl[MED_COUNT];
static lv_obj_t *confirm_btn;
static lv_obj_t *spiral_cv, *belt_cv, *pkg_obj, *pkg_lbl, *door_obj, *glow_obj, *arrow_lbl;
static lv_obj_t *stage_led[4], *m_status, *m_prog;
static lv_obj_t *success_mask;
static int       disp_idx = 0, disp_total = 0;
static int       disp_order[MED_COUNT];   /* 药品索引 */
static uint8_t   disp_qty[MED_COUNT];
static bool      spiral_run = false, belt_run = false;

static lv_color_t spiral_buf[194 * 18];
static lv_color_t belt_buf[250 * 28];

/* ==================== 小工具 ==================== */
static void later(lv_timer_cb_t cb, uint32_t ms)
{
    lv_timer_t *t = lv_timer_create(cb, ms, NULL);
    lv_timer_set_repeat_count(t, 1);
}

static void go_screen(lv_obj_t *scr)
{
    lv_scr_load_anim(scr, LV_SCR_LOAD_ANIM_MOVE_LEFT, 320, 0, false);
}

static lv_obj_t *box(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    return o;
}

static lv_obj_t *lbl(lv_obj_t *parent, const char *txt, const lv_font_t *font, lv_color_t color,
                     lv_coord_t x, lv_coord_t y)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, color, 0);
    lv_obj_set_pos(l, x, y);
    return l;
}

static lv_obj_t *lbl_big(lv_obj_t *parent, const char *txt, lv_color_t color,
                         lv_coord_t x, lv_coord_t y, uint16_t zoom)
{
    lv_obj_t *l = lbl(parent, txt, F_CHN, color, x, y);
    lv_obj_set_style_transform_zoom(l, zoom, 0);
    return l;
}

/* 分 → "12.00" */
static void fmt_price(uint32_t cents, char *out, size_t n)
{
    snprintf(out, n, "%lu.%02lu", (unsigned long)(cents / 100), (unsigned long)(cents % 100));
}

static void anim_zoom_cb(void *obj, int32_t v)  { lv_obj_set_style_transform_zoom((lv_obj_t *)obj, v, 0); }
static void anim_opa_cb(void *obj, int32_t v)   { lv_obj_set_style_opa((lv_obj_t *)obj, v, 0); }
static void anim_y_cb(void *obj, int32_t v)     { lv_obj_set_y((lv_obj_t *)obj, v); }
static void anim_angle_cb(void *obj, int32_t v) { lv_obj_set_style_transform_angle((lv_obj_t *)obj, v, 0); }

/* ==================== 状态栏 ==================== */
static void clock_cb(lv_timer_t *t)
{
    /* 真机可换成 SNTP 网络时间;此处用开机秒数模拟 */
    uint32_t s = lv_tick_get() / 1000;
    char buf[16];
    sprintf(buf, "%02lu:%02lu:%02lu", (unsigned long)(s / 3600 % 24),
            (unsigned long)(s / 60 % 60), (unsigned long)(s % 60));
    for (int i = 0; i < clk_cnt; i++) if (clk_lbl[i]) lv_label_set_text(clk_lbl[i], buf);
}

static void status_bar(lv_obj_t *scr)
{
    lv_obj_t *bar = box(scr, 0, 0, SCR_W, BAR_H);
    lv_obj_set_style_bg_color(bar, C_TEAL_D, 0);
    lv_obj_set_style_bg_grad_color(bar, lv_color_hex(0x12A086), 0);
    lv_obj_set_style_bg_grad_dir(bar, LV_GRAD_DIR_HOR, 0);
    lv_obj_set_style_radius(bar, 0, 0);

    lv_obj_t *sq = box(bar, 14, 10, 24, 24);
    lv_obj_set_style_bg_color(sq, lv_color_white(), 0);
    lv_obj_set_style_radius(sq, 6, 0);
    lv_obj_t *h = box(sq, 5, 10, 14, 4);
    lv_obj_set_style_bg_color(h, C_TEAL_D, 0); lv_obj_set_style_radius(h, 2, 0);
    lv_obj_t *v = box(sq, 10, 5, 4, 14);
    lv_obj_set_style_bg_color(v, C_TEAL_D, 0); lv_obj_set_style_radius(v, 2, 0);

    lbl(bar, "铁医智箱", F_CHN, lv_color_white(), 48, 13);
    lbl(bar, "在线 · 货道正常", F_CHN, lv_color_hex(0xD8F3EC), 160, 14);
    lbl(bar, "WiFi", F_CHN, lv_color_hex(0xD8F3EC), 640, 14);

    if (clk_cnt < 4) clk_lbl[clk_cnt] = lbl(bar, "00:00:00", F_CHN, lv_color_white(), 716, 13);
    clk_cnt++;
}

/* ==================== 页面一:待机首页 ==================== */
static void home_click_cb(lv_event_t *e) { go_screen(scr_shop); }

static void ring_start(lv_obj_t *ring, uint32_t delay)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, ring);
    lv_anim_set_values(&a, 140, 330);
    lv_anim_set_time(&a, 2600);
    lv_anim_set_delay(&a, delay);
    lv_anim_set_exec_cb(&a, anim_zoom_cb);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);

    lv_anim_set_values(&a, LV_OPA_80, LV_OPA_TRANSP);
    lv_anim_set_exec_cb(&a, anim_opa_cb);
    lv_anim_start(&a);
}

static void build_home(void)
{
    scr_home = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_home, C_BG, 0);
    lv_obj_clear_flag(scr_home, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(scr_home, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(scr_home, home_click_cb, LV_EVENT_CLICKED, NULL);
    status_bar(scr_home);

    lbl_big(scr_home, "铁医智箱", C_DARK, 44, 110, 512);
    lbl(scr_home, "铁路职工身边的 24 小时智能药箱", F_CHN, C_GRAY, 44, 165);

    const char *feats[3] = { "常备药品", "扫码支付", "秒速出药" };
    for (int i = 0; i < 3; i++) {
        lv_obj_t *f = box(scr_home, 44 + i * 150, 210, 134, 48);
        lv_obj_set_style_bg_color(f, lv_color_white(), 0);
        lv_obj_set_style_radius(f, 14, 0);
        lv_obj_set_style_shadow_width(f, 14, 0);
        lv_obj_set_style_shadow_opa(f, LV_OPA_10, 0);
        lv_obj_set_style_shadow_ofs_y(f, 5, 0);
        lv_obj_t *fl = lbl(f, feats[i], F_CHN, C_DARK, 0, 0);
        lv_obj_align(fl, LV_ALIGN_CENTER, 0, 0);
    }

    lv_obj_t *tip = box(scr_home, 44, 300, 300, 54);
    lv_obj_set_style_bg_color(tip, C_TEAL, 0);
    lv_obj_set_style_radius(tip, 27, 0);
    lv_obj_set_style_shadow_width(tip, 22, 0);
    lv_obj_set_style_shadow_color(tip, C_TEAL, 0);
    lv_obj_set_style_shadow_opa(tip, LV_OPA_40, 0);
    lv_obj_t *tl = lbl(tip, "触摸屏幕  开始购药", F_CHN, lv_color_white(), 0, 0);
    lv_obj_align(tl, LV_ALIGN_CENTER, 0, 0);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, tip);
    lv_anim_set_values(&a, 256, 268);
    lv_anim_set_time(&a, 900);
    lv_anim_set_playback_time(&a, 900);
    lv_anim_set_exec_cb(&a, anim_zoom_cb);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);

    lv_obj_t *ring1 = box(scr_home, 505, 95, 260, 260);
    lv_obj_set_style_bg_opa(ring1, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(ring1, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(ring1, 3, 0);
    lv_obj_set_style_border_color(ring1, C_TEAL, 0);
    lv_obj_t *ring2 = box(scr_home, 505, 95, 260, 260);
    lv_obj_set_style_bg_opa(ring2, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(ring2, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(ring2, 3, 0);
    lv_obj_set_style_border_color(ring2, C_TEAL, 0);
    ring_start(ring1, 0);
    ring_start(ring2, 1300);

    lv_obj_t *cross = box(scr_home, 550, 140, 170, 170);
    lv_obj_set_style_bg_color(cross, lv_color_hex(0x12B394), 0);
    lv_obj_set_style_bg_grad_color(cross, lv_color_hex(0x0A7F6A), 0);
    lv_obj_set_style_bg_grad_dir(cross, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_radius(cross, 42, 0);
    lv_obj_set_style_shadow_width(cross, 40, 0);
    lv_obj_set_style_shadow_color(cross, C_TEAL, 0);
    lv_obj_set_style_shadow_opa(cross, LV_OPA_40, 0);
    lv_obj_set_style_shadow_ofs_y(cross, 16, 0);
    lv_obj_t *ch = box(cross, 37, 71, 96, 28);
    lv_obj_set_style_bg_color(ch, lv_color_white(), 0); lv_obj_set_style_radius(ch, 10, 0);
    lv_obj_t *cv = box(cross, 71, 37, 28, 96);
    lv_obj_set_style_bg_color(cv, lv_color_white(), 0); lv_obj_set_style_radius(cv, 10, 0);

    lv_anim_init(&a);
    lv_anim_set_var(&a, cross);
    lv_anim_set_values(&a, 140, 128);
    lv_anim_set_time(&a, 1500);
    lv_anim_set_playback_time(&a, 1500);
    lv_anim_set_exec_cb(&a, anim_y_cb);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
}

/* ==================== 页面二:药品选购 ==================== */
static uint32_t cart_sum_cents(void)
{
    uint32_t s = 0;
    for (int i = 0; i < MED_COUNT; i++) s += (uint32_t)MEDS[i].price * cart_qty[i];
    return s;
}

static int cart_count(void)
{
    int n = 0;
    for (int i = 0; i < MED_COUNT; i++) n += cart_qty[i];
    return n;
}

static void update_cart_bar(void)
{
    char buf[24];
    int n = cart_count();
    snprintf(buf, sizeof(buf), "%d", n);
    lv_label_set_text(cart_badge, buf);
    if (n > 0) lv_obj_clear_flag(cart_badge, LV_OBJ_FLAG_HIDDEN);
    else       lv_obj_add_flag(cart_badge, LV_OBJ_FLAG_HIDDEN);

    fmt_price(cart_sum_cents(), buf, sizeof(buf));
    lv_label_set_text(cart_total_lbl, buf);
    lv_label_set_text(cart_info_lbl, n > 0 ? "已加入购物车" : "购物车是空的");

    if (n > 0) {
        lv_obj_clear_state(checkout_btn, LV_STATE_DISABLED);
        lv_obj_add_flag(checkout_btn, LV_OBJ_FLAG_CLICKABLE);
    } else {
        lv_obj_add_state(checkout_btn, LV_STATE_DISABLED);
    }
}

/* 原地刷新某张卡片的「+ / 步进器」显隐与数量(不删除任何对象,避免事件派发中野指针) */
static void card_refresh(int idx)
{
    if (!card_add[idx]) return;   /* 该药品当前分类下不可见 */
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", cart_qty[idx]);
    lv_label_set_text(card_qn[idx], buf);
    if (cart_qty[idx] > 0) {
        lv_obj_add_flag(card_add[idx], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(card_step[idx], LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(card_add[idx], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(card_step[idx], LV_OBJ_FLAG_HIDDEN);
    }
}

/* 飞入购物车小动画 */
static void fly_done_cb(lv_anim_t *a)
{
    lv_obj_del_async((lv_obj_t *)a->var);
}

static void fly_to_cart(lv_obj_t *from)
{
    lv_area_t fa, ba;
    lv_obj_get_coords(from, &fa);
    lv_obj_get_coords(cart_badge, &ba);
    lv_obj_t *fly = box(lv_layer_top(), 0, 0, 26, 26);
    lv_obj_set_style_bg_color(fly, C_TEAL, 0);
    lv_obj_set_style_radius(fly, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_pos(fly, fa.x1, fa.y1);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, fly);
    lv_anim_set_values(&a, fa.x1, ba.x1);
    lv_anim_set_time(&a, 550);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_x);
    lv_anim_set_ready_cb(&a, fly_done_cb);
    lv_anim_start(&a);

    lv_anim_set_values(&a, fa.y1, ba.y1);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_ready_cb(&a, NULL);
    lv_anim_start(&a);
}

static void add_cart_cb(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (cart_qty[idx] >= MEDS[idx].stock) return;
    cart_qty[idx]++;
    fly_to_cart(lv_event_get_target(e));
    card_refresh(idx);
    update_cart_bar();
}

static void minus_cart_cb(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (cart_qty[idx] == 0) return;
    cart_qty[idx]--;
    card_refresh(idx);
    update_cart_bar();
}

/* 药品卡片(240×128) */
static void med_card(lv_obj_t *parent, int idx)
{
    const med_t *m = &MEDS[idx];
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, 240, 128);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(card, lv_color_white(), 0);
    lv_obj_set_style_radius(card, 16, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_set_style_shadow_width(card, 12, 0);
    lv_obj_set_style_shadow_opa(card, LV_OPA_10, 0);
    lv_obj_set_style_shadow_ofs_y(card, 4, 0);
    if (m->stock == 0) lv_obj_set_style_opa(card, LV_OPA_60, 0);

    lv_obj_t *ic = box(card, 12, 12, 42, 42);
    lv_obj_set_style_bg_color(ic, m->icon_bg, 0);
    lv_obj_set_style_radius(ic, 11, 0);
    lv_obj_t *cx1 = box(ic, 12, 19, 18, 4);
    lv_obj_set_style_bg_color(cx1, C_TEAL_D, 0); lv_obj_set_style_radius(cx1, 2, 0);
    lv_obj_t *cx2 = box(ic, 19, 12, 4, 18);
    lv_obj_set_style_bg_color(cx2, C_TEAL_D, 0); lv_obj_set_style_radius(cx2, 2, 0);

    lbl(card, m->name, F_CHN, C_DARK, 62, 12);
    lbl(card, m->spec, F_CHN, C_GRAY_L, 62, 36);

    if (m->stock == 0) {
        lv_obj_t *so = box(card, 168, 12, 60, 24);
        lv_obj_set_style_bg_color(so, C_GRAY_L, 0);
        lv_obj_set_style_radius(so, 12, 0);
        lv_obj_t *sl2 = lbl(so, "已售罄", F_CHN, lv_color_white(), 0, 0);
        lv_obj_align(sl2, LV_ALIGN_CENTER, 0, 0);
    }

    char buf[24];
    fmt_price(m->price, buf, sizeof(buf));
    char pbuf[28];
    snprintf(pbuf, sizeof(pbuf), "¥%s", buf);
    lv_obj_t *pl = lbl(card, pbuf, F_NUM_P, C_RED, 12, 98);

    if (m->stock > 0) {
        /* 加购按钮(qty==0 时显示) */
        lv_obj_t *add = box(card, 194, 86, 34, 34);
        lv_obj_set_style_bg_color(add, C_TEAL, 0);
        lv_obj_set_style_radius(add, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_shadow_width(add, 8, 0);
        lv_obj_set_style_shadow_color(add, C_TEAL, 0);
        lv_obj_set_style_shadow_opa(add, LV_OPA_40, 0);
        lv_obj_t *al = lbl(add, "+", F_NUM_P, lv_color_white(), 0, 0);
        lv_obj_align(al, LV_ALIGN_CENTER, 0, 0);
        lv_obj_add_flag(add, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(add, add_cart_cb, LV_EVENT_CLICKED, (void *)(intptr_t)idx);

        /* 步进器(qty>0 时显示):− n + */
        lv_obj_t *step = box(card, 118, 86, 110, 34);
        lv_obj_set_style_bg_opa(step, LV_OPA_TRANSP, 0);
        lv_obj_t *minus = box(step, 0, 3, 28, 28);
        lv_obj_set_style_bg_color(minus, lv_color_hex(0xEEF2F6), 0);
        lv_obj_set_style_radius(minus, LV_RADIUS_CIRCLE, 0);
        lv_obj_t *ml = lbl(minus, "-", F_NUM_P, C_GRAY, 0, 0);
        lv_obj_align(ml, LV_ALIGN_CENTER, 0, 0);
        lv_obj_add_flag(minus, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(minus, minus_cart_cb, LV_EVENT_CLICKED, (void *)(intptr_t)idx);
        lv_obj_t *qn = lbl(step, "0", F_NUM_P, C_DARK, 41, 6);
        lv_obj_t *plus = box(step, 76, 3, 28, 28);
        lv_obj_set_style_bg_color(plus, C_TEAL, 0);
        lv_obj_set_style_radius(plus, LV_RADIUS_CIRCLE, 0);
        lv_obj_t *pl2 = lbl(plus, "+", F_NUM_P, lv_color_white(), 0, 0);
        lv_obj_align(pl2, LV_ALIGN_CENTER, 0, 0);
        lv_obj_add_flag(plus, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(plus, add_cart_cb, LV_EVENT_CLICKED, (void *)(intptr_t)idx);

        card_add[idx]  = add;
        card_step[idx] = step;
        card_qn[idx]   = qn;
        card_refresh(idx);
    } else {
        card_add[idx] = card_step[idx] = card_qn[idx] = NULL;
    }
}

/* 重铺网格:仅在点击分类 Tab 时调用(Tab 不在网格内,清理是安全的) */
static void shop_render_grid(void)
{
    memset(card_add, 0, sizeof(card_add));
    memset(card_step, 0, sizeof(card_step));
    memset(card_qn, 0, sizeof(card_qn));
    lv_obj_clean(grid_box);
    for (int i = 0; i < MED_COUNT; i++) {
        if (cur_tab != 0 && MEDS[i].cat != cur_tab) continue;
        med_card(grid_box, i);
    }
}

static void tab_cb(lv_event_t *e)
{
    cur_tab = (int)(intptr_t)lv_event_get_user_data(e);
    for (int i = 0; i < 5; i++) {
        bool on = (i == cur_tab);
        lv_obj_set_style_bg_color(tab_btn[i], on ? C_TEAL : lv_color_white(), 0);
        lv_obj_set_style_text_color(lv_obj_get_child(tab_btn[i], 0),
                                    on ? lv_color_white() : C_GRAY, 0);
    }
    shop_render_grid();
}

static void shop_back_cb(lv_event_t *e) { go_screen(scr_home); }

static void enter_pay(void);

static void checkout_cb(lv_event_t *e)
{
    if (lv_obj_has_state(checkout_btn, LV_STATE_DISABLED)) return;
    if (cart_count() == 0) return;
    enter_pay();
}

static void build_shop(void)
{
    scr_shop = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_shop, C_BG, 0);
    lv_obj_clear_flag(scr_shop, LV_OBJ_FLAG_SCROLLABLE);
    status_bar(scr_shop);

    lv_obj_t *back = box(scr_shop, 16, 54, 90, 34);
    lv_obj_set_style_bg_color(back, lv_color_hex(0xEEF2F6), 0);
    lv_obj_set_style_radius(back, 10, 0);
    lv_obj_t *bl = lbl(back, "< 返回", F_CHN, C_GRAY, 0, 0);
    lv_obj_align(bl, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(back, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(back, shop_back_cb, LV_EVENT_CLICKED, NULL);

    lbl(scr_shop, "选择药品", F_CHN, C_DARK, 118, 62);
    lbl(scr_shop, "点击 + 加入购物车", F_CHN, C_GRAY_L, 230, 64);

    /* 分类 Tab */
    for (int i = 0; i < 5; i++) {
        tab_btn[i] = box(scr_shop, 16 + i * 100, 96, 90, 32);
        lv_obj_set_style_bg_color(tab_btn[i], i == 0 ? C_TEAL : lv_color_white(), 0);
        lv_obj_set_style_radius(tab_btn[i], 16, 0);
        lv_obj_set_style_shadow_width(tab_btn[i], 6, 0);
        lv_obj_set_style_shadow_opa(tab_btn[i], LV_OPA_10, 0);
        lv_obj_t *tl = lbl(tab_btn[i], CAT_NAMES[i], F_CHN,
                           i == 0 ? lv_color_white() : C_GRAY, 0, 0);
        lv_obj_align(tl, LV_ALIGN_CENTER, 0, 0);
        lv_obj_add_flag(tab_btn[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(tab_btn[i], tab_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }

    /* 药品网格:LVGL flex 自动换行 + 可滚动 */
    grid_box = lv_obj_create(scr_shop);
    lv_obj_set_pos(grid_box, 16, 138);
    lv_obj_set_size(grid_box, 768, 254);
    lv_obj_set_style_bg_opa(grid_box, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(grid_box, 0, 0);
    lv_obj_set_style_pad_all(grid_box, 4, 0);
    lv_obj_set_style_pad_row(grid_box, 12, 0);
    lv_obj_set_style_pad_column(grid_box, 12, 0);
    lv_obj_set_flex_flow(grid_box, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_scroll_dir(grid_box, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(grid_box, LV_SCROLLBAR_MODE_AUTO);

    /* 购物车栏 */
    lv_obj_t *bar = box(scr_shop, 16, 402, 768, 62);
    lv_obj_set_style_bg_color(bar, C_DARK, 0);
    lv_obj_set_style_radius(bar, 16, 0);
    lv_obj_set_style_shadow_width(bar, 20, 0);
    lv_obj_set_style_shadow_opa(bar, LV_OPA_30, 0);

    lv_obj_t *cico = box(bar, 20, 15, 32, 32);
    lv_obj_set_style_bg_color(cico, lv_color_hex(0x1E3A55), 0);
    lv_obj_set_style_radius(cico, LV_RADIUS_CIRCLE, 0);
    lv_obj_t *cil = lbl(cico, "购", F_CHN, lv_color_white(), 0, 0);
    lv_obj_align(cil, LV_ALIGN_CENTER, 0, 0);

    cart_badge = lbl(bar, "0", F_CHN, lv_color_white(), 44, 6);
    lv_obj_set_style_bg_color(cart_badge, C_RED, 0);
    lv_obj_set_style_bg_opa(cart_badge, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(cart_badge, 9, 0);
    lv_obj_set_style_pad_left(cart_badge, 5, 0);
    lv_obj_set_style_pad_right(cart_badge, 5, 0);
    lv_obj_add_flag(cart_badge, LV_OBJ_FLAG_HIDDEN);

    lbl(bar, "¥", F_CHN, C_AMBER, 66, 24);
    cart_total_lbl = lbl(bar, "0.00", F_NUM_M, C_AMBER, 86, 18);
    cart_info_lbl = lbl(bar, "购物车是空的", F_CHN, lv_color_hex(0x9DB2C8), 220, 24);

    checkout_btn = box(bar, 580, 9, 172, 44);
    lv_obj_set_style_bg_color(checkout_btn, C_TEAL, 0);
    lv_obj_set_style_radius(checkout_btn, 12, 0);
    lv_obj_t *cl = lbl(checkout_btn, "去结算", F_CHN, lv_color_white(), 0, 0);
    lv_obj_align(cl, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_state(checkout_btn, LV_STATE_DISABLED);
    lv_obj_add_event_cb(checkout_btn, checkout_cb, LV_EVENT_CLICKED, NULL);

    cur_tab = 0;
    shop_render_grid();
}

/* ==================== 页面三:扫码支付 ==================== */
static void enter_dispense(void);

static void pay_timer_cb(lv_timer_t *t)
{
    pay_left--;
    char buf[40];
    snprintf(buf, sizeof(buf), "请在 %d 秒内完成支付", pay_left);
    lv_label_set_text(pay_cnt_lbl, buf);
    if (pay_left <= 0) {
        lv_timer_del(pay_timer);
        pay_timer = NULL;
        memset(cart_qty, 0, sizeof(cart_qty));
        shop_render_grid();      /* 支付页点击超时,操作的是选购页,安全 */
        update_cart_bar();
        go_screen(scr_shop);
    }
}

static void pay_stop_timer(void)
{
    if (pay_timer) { lv_timer_del(pay_timer); pay_timer = NULL; }
}

/* 模拟支付成功(真机:换成微信支付后台回调/轮询查到已支付) */
static void pay_success_cb(lv_event_t *e)
{
    pay_stop_timer();
    /* 生成出药订单 */
    disp_total = 0;
    for (int i = 0; i < MED_COUNT; i++) {
        if (cart_qty[i] > 0) {
            disp_order[disp_total] = i;
            disp_qty[disp_total]   = cart_qty[i];
            disp_total++;
        }
    }
    memset(cart_qty, 0, sizeof(cart_qty));
    enter_dispense();
}

static void pay_continue_cb(lv_event_t *e)
{
    pay_stop_timer();
    go_screen(scr_shop);          /* 保留购物车 */
}

static void pay_cancel_cb(lv_event_t *e)
{
    pay_stop_timer();
    memset(cart_qty, 0, sizeof(cart_qty));
    shop_render_grid();           /* 当前在支付页,重建选购页网格是安全的 */
    update_cart_bar();
    go_screen(scr_shop);
}

static void enter_pay(void)
{
    /* 金额与订单明细 */
    char buf[24];
    fmt_price(cart_sum_cents(), buf, sizeof(buf));
    lv_label_set_text(pay_amt_lbl, buf);
    char tbuf[28];
    snprintf(tbuf, sizeof(tbuf), "合计:¥%s", buf);
    lv_label_set_text(pay_total_lbl, tbuf);

    lv_obj_clean(pay_list);       /* enter_pay 由结算按钮触发(在选购页),清理支付页列表安全 */
    int y = 0;
    for (int i = 0; i < MED_COUNT; i++) {
        if (cart_qty[i] == 0) continue;
        const med_t *m = &MEDS[i];
        lv_obj_t *row = box(pay_list, 0, y, 318, 40);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_border_color(row, lv_color_hex(0xE3EAF0), 0);
        lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
        lbl(row, m->name, F_CHN, C_DARK, 0, 10);
        char qbuf[12];
        snprintf(qbuf, sizeof(qbuf), "×%d", cart_qty[i]);
        lbl(row, qbuf, F_CHN, C_GRAY_L, 190, 10);
        char abuf[24];
        fmt_price((uint32_t)m->price * cart_qty[i], abuf, sizeof(abuf));
        char pbuf[28];
        snprintf(pbuf, sizeof(pbuf), "¥%s", abuf);
        lbl(row, pbuf, F_CHN, C_RED, 250, 10);
        y += 42;
    }

    /* TODO(部署):动态金额二维码 —— 连网后向你的后台请求本单 Native 支付链接,
       然后 lv_qrcode_update(qr, 链接, 长度) 刷新。此处为固定收款码演示。 */

    pay_left = PAY_TIMEOUT_S;
    snprintf(buf, sizeof(buf), "请在 %d 秒内完成支付", pay_left);
    lv_label_set_text(pay_cnt_lbl, buf);
    pay_stop_timer();
    pay_timer = lv_timer_create(pay_timer_cb, 1000, NULL);

    go_screen(scr_pay);
}

static void build_pay(void)
{
    scr_pay = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_pay, C_BG, 0);
    lv_obj_clear_flag(scr_pay, LV_OBJ_FLAG_SCROLLABLE);
    status_bar(scr_pay);

    /* 左侧:二维码面板 */
    lv_obj_t *pane = box(scr_pay, 16, 58, 400, 406);
    lv_obj_set_style_bg_color(pane, lv_color_white(), 0);
    lv_obj_set_style_radius(pane, 20, 0);
    lv_obj_set_style_shadow_width(pane, 24, 0);
    lv_obj_set_style_shadow_opa(pane, LV_OPA_10, 0);

    lv_obj_t *t1 = lbl(pane, "请使用 微信 扫码支付", F_CHN, C_DARK, 0, 0);
    lv_obj_align(t1, LV_ALIGN_TOP_MID, 0, 16);

    lbl(pane, "应付金额 ¥", F_CHN, C_GRAY, 108, 52);
    pay_amt_lbl = lbl(pane, "0.00", F_NUM_L, C_RED, 200, 42);

    lv_obj_t *qr = lv_qrcode_create(pane, 190, C_DARK, lv_color_white());
    lv_qrcode_update(qr, PAY_QR_TEXT, strlen(PAY_QR_TEXT));
    lv_obj_align(qr, LV_ALIGN_TOP_MID, 0, 92);
    lv_obj_set_style_border_color(qr, lv_color_hex(0xEAF3F0), 0);
    lv_obj_set_style_border_width(qr, 6, 0);

    /* 扫描线循环动画 */
    lv_obj_t *scan = box(pane, 0, 0, 190, 4);
    lv_obj_set_style_bg_color(scan, lv_color_hex(0x12D3A7), 0);
    lv_obj_set_style_radius(scan, 2, 0);
    lv_obj_set_style_shadow_width(scan, 12, 0);
    lv_obj_set_style_shadow_color(scan, lv_color_hex(0x12D3A7), 0);
    lv_obj_set_style_shadow_opa(scan, LV_OPA_60, 0);
    lv_obj_align(scan, LV_ALIGN_TOP_MID, 0, 96);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, scan);
    lv_anim_set_values(&a, 96, 276);
    lv_anim_set_time(&a, 1100);
    lv_anim_set_playback_time(&a, 1100);
    lv_anim_set_exec_cb(&a, anim_y_cb);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);

    pay_cnt_lbl = lbl(pane, "请在 120 秒内完成支付", F_CHN, C_GRAY_L, 0, 0);
    lv_obj_align(pay_cnt_lbl, LV_ALIGN_TOP_MID, 0, 310);

    lv_obj_t *chip = box(pane, 0, 0, 200, 30);
    lv_obj_set_style_bg_color(chip, lv_color_hex(0xE6F9F3), 0);
    lv_obj_set_style_radius(chip, 15, 0);
    lv_obj_align(chip, LV_ALIGN_TOP_MID, 0, 344);
    lv_obj_t *cl2 = lbl(chip, "药箱编号:" BOX_NO, F_CHN, C_TEAL_D, 0, 0);
    lv_obj_align(cl2, LV_ALIGN_CENTER, 0, 0);

    /* 右侧:订单明细 + 操作 */
    lbl(scr_pay, "订单明细", F_CHN, C_DARK, 440, 66);

    pay_list = box(scr_pay, 436, 96, 348, 216);
    lv_obj_set_style_bg_color(pay_list, lv_color_white(), 0);
    lv_obj_set_style_radius(pay_list, 16, 0);
    lv_obj_set_style_shadow_width(pay_list, 12, 0);
    lv_obj_set_style_shadow_opa(pay_list, LV_OPA_10, 0);
    lv_obj_set_style_pad_all(pay_list, 12, 0);
    lv_obj_set_scroll_dir(pay_list, LV_DIR_VER);

    pay_total_lbl = lbl(scr_pay, "合计:¥0.00", F_CHN, C_RED, 560, 324);

    lv_obj_t *bc = box(scr_pay, 436, 356, 104, 44);
    lv_obj_set_style_bg_color(bc, lv_color_hex(0xEEF2F6), 0);
    lv_obj_set_style_radius(bc, 12, 0);
    lv_obj_t *bcl = lbl(bc, "< 继续选药", F_CHN, C_GRAY, 0, 0);
    lv_obj_align(bcl, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(bc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(bc, pay_continue_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *bx = box(scr_pay, 552, 356, 104, 44);
    lv_obj_set_style_bg_color(bx, lv_color_hex(0xFDECEB), 0);
    lv_obj_set_style_radius(bx, 12, 0);
    lv_obj_t *bxl = lbl(bx, "取消订单", F_CHN, C_RED, 0, 0);
    lv_obj_align(bxl, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(bx, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(bx, pay_cancel_cb, LV_EVENT_CLICKED, NULL);

#if UI_DEMO_BUTTON
    lv_obj_t *bp = box(scr_pay, 668, 356, 116, 44);
    lv_obj_set_style_bg_color(bp, C_WX, 0);
    lv_obj_set_style_radius(bp, 12, 0);
    lv_obj_set_style_shadow_width(bp, 10, 0);
    lv_obj_set_style_shadow_color(bp, C_WX, 0);
    lv_obj_set_style_shadow_opa(bp, LV_OPA_30, 0);
    lv_obj_t *bpl = lbl(bp, "模拟支付成功", F_CHN, lv_color_white(), 0, 0);
    lv_obj_align(bpl, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(bp, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(bp, pay_success_cb, LV_EVENT_CLICKED, NULL);
#endif

    lbl(scr_pay, "真机部署:手机扫码付款后,由微信支付后台回调触发「支付成功」",
        F_CHN, C_GRAY_L, 436, 424);
}

/* ==================== 页面四:确认出药 + 出药动画 ==================== */
static void start_dispense(void);

static void build_disp_list(void)
{
    lv_obj_clean(disp_list);   /* 由支付页按钮触发,操作出药页,安全 */
    int y = 0;
    for (int i = 0; i < disp_total; i++) {
        const med_t *m = &MEDS[disp_order[i]];
        lv_obj_t *row = box(disp_list, 0, y, 330, 62);
        lv_obj_set_style_bg_color(row, lv_color_hex(0xF3F6F9), 0);
        lv_obj_set_style_radius(row, 12, 0);
        lv_obj_t *ic = box(row, 10, 11, 40, 40);
        lv_obj_set_style_bg_color(ic, m->icon_bg, 0);
        lv_obj_set_style_radius(ic, 10, 0);
        lv_obj_t *cx1 = box(ic, 11, 18, 18, 4);
        lv_obj_set_style_bg_color(cx1, C_TEAL_D, 0);
        lv_obj_t *cx2 = box(ic, 18, 11, 4, 18);
        lv_obj_set_style_bg_color(cx2, C_TEAL_D, 0);
        char nbuf[40];
        snprintf(nbuf, sizeof(nbuf), "%s ×%d", m->name, disp_qty[i]);
        lbl(row, nbuf, F_CHN, C_DARK, 62, 8);
        lbl(row, m->spec, F_CHN, C_GRAY, 62, 32);
        disp_state_lbl[i] = lbl(row, "待出药", F_CHN, C_GRAY_L, 264, 22);
        y += 70;
    }
    if (confirm_btn) {
        lv_obj_set_style_bg_color(confirm_btn, C_TEAL, 0);
        lv_obj_set_style_shadow_opa(confirm_btn, LV_OPA_40, 0);
        lv_obj_clear_state(confirm_btn, LV_STATE_DISABLED);
        lv_obj_add_flag(confirm_btn, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_t *cl3 = lv_obj_get_child(confirm_btn, 0);
        lv_label_set_text(cl3, "确认出药");
    }
}

static void spiral_draw(int offset)
{
    for (int x = 0; x < 194; x++) {
        bool g = (((x + offset) / 10) % 2) == 0;
        for (int y = 0; y < 18; y++) spiral_buf[y * 194 + x] = g ? C_TEAL : lv_color_hex(0x0B7A64);
    }
    lv_obj_invalidate(spiral_cv);
}

static void belt_draw(int offset)
{
    for (int x = 0; x < 250; x++) {
        bool on = (((x + offset) % 34) < 3);
        for (int y = 0; y < 28; y++) {
            belt_buf[y * 250 + x] = on ? lv_color_hex(0x3A5F86) : lv_color_hex(0x223450);
        }
    }
    lv_obj_invalidate(belt_cv);
}

static void machine_timer_cb(lv_timer_t *t)
{
    static int so = 0, bo = 0;
    if (spiral_run) { so += 6; spiral_draw(so % 20); }
    if (belt_run)   { bo += 7; belt_draw(bo % 34); }
}

static void set_stage_led(int idx, bool done)
{
    for (int i = 0; i < 4; i++) {
        lv_color_t c = (i < idx || (i == idx && done)) ? lv_color_hex(0x17C964)
                     : (i == idx) ? C_AMBER : lv_color_hex(0x2C3F5C);
        lv_obj_set_style_bg_color(stage_led[i], c, 0);
        lv_obj_set_style_shadow_width(stage_led[i], (i == idx && !done) ? 12 : 0, 0);
        lv_obj_set_style_shadow_color(stage_led[i], C_AMBER, 0);
    }
}

static void disp_mark_done(int i)
{
    lv_label_set_text(disp_state_lbl[i], "√ 已出药");
    lv_obj_set_style_text_color(disp_state_lbl[i], C_GREEN, 0);
}

static void success_hide(lv_timer_t *t)
{
    lv_obj_add_flag(success_mask, LV_OBJ_FLAG_HIDDEN);
    go_screen(scr_home);
}

static void success_show(void)
{
    lv_obj_clear_flag(success_mask, LV_OBJ_FLAG_HIDDEN);
    later(success_hide, 2600);
}

/* ---------- 出药主动画:四阶段 ---------- */
static void disp_stage4(lv_timer_t *t);
static void disp_stage3(lv_timer_t *t);
static void disp_stage2(lv_timer_t *t);
static void disp_item_done(lv_timer_t *t);
static void disp_finish(lv_timer_t *t);

/* 阶段① 货道运转:螺旋推杆旋转,药品滑出货道 */
static void disp_stage1(lv_timer_t *t)
{
    /* TODO(硬件对接):在此通过 RS485/串口发送出药指令
       (按 disp_order[disp_idx] 选货道电机,按 disp_qty[disp_idx] 出对应数量) */
    set_stage_led(0, false);
    lv_label_set_text(m_status, "正在出药:货道运转中…");

    /* 每个药品开始前:复位包装、关门、收起取药提示 */
    lv_obj_set_pos(pkg_obj, 176, 66);
    lv_obj_set_style_transform_angle(pkg_obj, 0, 0);
    lv_obj_clear_flag(pkg_obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_y(door_obj, 154);
    lv_obj_add_flag(glow_obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(arrow_lbl, LV_OBJ_FLAG_HIDDEN);

    spiral_run = true;

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, pkg_obj);
    lv_anim_set_values(&a, 176, 240);
    lv_anim_set_time(&a, 760);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_x);
    lv_anim_start(&a);

    later(disp_stage2, 820);
}

/* 阶段② 药品下落:翻转坠落 + 回弹压扁 */
static void bounce_settle(lv_anim_t *a)
{
    lv_obj_set_size(pkg_obj, 44, 38);
    lv_obj_set_pos(pkg_obj, 310, 196);
}

static void bounce_back(lv_anim_t *a)
{
    lv_anim_t b;
    lv_anim_init(&b);
    lv_anim_set_var(&b, pkg_obj);
    lv_anim_set_values(&b, 178, 196);
    lv_anim_set_time(&b, 200);
    lv_anim_set_path_cb(&b, lv_anim_path_ease_in);
    lv_anim_set_exec_cb(&b, anim_y_cb);
    lv_anim_set_ready_cb(&b, bounce_settle);
    lv_anim_start(&b);
}

static void fall_ready(lv_anim_t *a)
{
    lv_obj_set_size(pkg_obj, 44, 32);            /* 压扁 */
    lv_obj_set_pos(pkg_obj, 310, 202);
    lv_anim_t b;
    lv_anim_init(&b);
    lv_anim_set_var(&b, pkg_obj);
    lv_anim_set_values(&b, 202, 178);            /* 回弹 */
    lv_anim_set_time(&b, 170);
    lv_anim_set_path_cb(&b, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&b, anim_y_cb);
    lv_anim_set_ready_cb(&b, bounce_back);
    lv_anim_start(&b);
}

static void disp_stage2(lv_timer_t *t)
{
    spiral_run = false;
    set_stage_led(1, false);
    lv_label_set_text(m_status, "正在出药:药品下落…");

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, pkg_obj);
    lv_anim_set_values(&a, 66, 196);
    lv_anim_set_time(&a, 640);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
    lv_anim_set_exec_cb(&a, anim_y_cb);
    lv_anim_set_ready_cb(&a, fall_ready);
    lv_anim_start(&a);

    lv_anim_set_values(&a, 0, 3600);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&a, anim_angle_cb);
    lv_anim_set_ready_cb(&a, NULL);
    lv_anim_start(&a);

    later(disp_stage3, 880);
}

/* 阶段③ 传送带运送:皮带滚动,药品送至取药口 */
static void disp_stage3(lv_timer_t *t)
{
    set_stage_led(2, false);
    lv_label_set_text(m_status, "正在出药:传送中…");
    belt_run = true;

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, pkg_obj);
    lv_anim_set_values(&a, 310, 176);
    lv_anim_set_time(&a, 900);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_x);
    lv_anim_start(&a);

    later(disp_stage4, 960);
}

/* 阶段④ 开门取药:取药门打开,光晕与箭头提示 */
static void disp_stage4(lv_timer_t *t)
{
    belt_run = false;
    set_stage_led(3, false);
    lv_label_set_text(m_status, "已到达取药口,请取药");

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, door_obj);
    lv_anim_set_values(&a, 154, 210);
    lv_anim_set_time(&a, 560);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_exec_cb(&a, anim_y_cb);
    lv_anim_start(&a);

    lv_obj_clear_flag(glow_obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(arrow_lbl, LV_OBJ_FLAG_HIDDEN);

    lv_anim_init(&a);
    lv_anim_set_var(&a, arrow_lbl);
    lv_anim_set_values(&a, 224, 236);
    lv_anim_set_time(&a, 420);
    lv_anim_set_playback_time(&a, 420);
    lv_anim_set_exec_cb(&a, anim_y_cb);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);

    later(disp_item_done, 660);
}

/* 单个药品出完:点亮完成灯,推进清单与进度,决定下一步 */
static void disp_item_done(lv_timer_t *t2)
{
    set_stage_led(3, true);
    disp_mark_done(disp_idx);
    lv_bar_set_value(m_prog, (disp_idx + 1) * 100 / disp_total, LV_ANIM_ON);
    disp_idx++;
    if (disp_idx < disp_total) {
        later(disp_stage1, 500);
    } else {
        lv_label_set_text(m_status, "全部药品已出完,请核对取药");
        later(disp_finish, 900);
    }
}

static void disp_finish(lv_timer_t *t3)
{
    success_show();
}

static void start_dispense(void)
{
    disp_idx = 0;
    if (confirm_btn) {
        lv_obj_add_state(confirm_btn, LV_STATE_DISABLED);
        lv_obj_clear_flag(confirm_btn, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_t *cl3 = lv_obj_get_child(confirm_btn, 0);
        lv_label_set_text(cl3, "出药中…");
    }
    lv_bar_set_value(m_prog, 0, LV_ANIM_OFF);
    for (int i = 0; i < disp_total; i++) {
        if (i == 0) {
            lv_label_set_text(disp_state_lbl[i], "出药中…");
            lv_obj_set_style_text_color(disp_state_lbl[i], C_TEAL_D, 0);
        } else {
            lv_label_set_text(disp_state_lbl[i], "待出药");
            lv_obj_set_style_text_color(disp_state_lbl[i], C_GRAY_L, 0);
        }
    }
    lv_bar_set_value(m_prog, 100 / (disp_total * 2), LV_ANIM_ON);
    later(disp_stage1, 300);
}

static void confirm_cb(lv_event_t *e)
{
    if (lv_obj_has_state(confirm_btn, LV_STATE_DISABLED)) return;
    start_dispense();
}

static void enter_dispense(void)
{
    build_disp_list();
    go_screen(scr_disp);
}

static void build_dispense(void)
{
    scr_disp = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_disp, C_BG, 0);
    lv_obj_clear_flag(scr_disp, LV_OBJ_FLAG_SCROLLABLE);
    status_bar(scr_disp);

    /* 左侧:订单药品清单 */
    lv_obj_t *list_box = box(scr_disp, 16, 98, 360, 366);
    lv_obj_set_style_bg_color(list_box, lv_color_white(), 0);
    lv_obj_set_style_radius(list_box, 18, 0);
    lv_obj_set_style_shadow_width(list_box, 24, 0);
    lv_obj_set_style_shadow_opa(list_box, LV_OPA_10, 0);
    lbl(list_box, "出药清单", F_CHN, C_DARK, 16, 12);
    lbl(list_box, "支付成功,请核对后出药", F_CHN, C_GRAY, 16, 36);

    disp_list = box(list_box, 12, 64, 336, 236);
    lv_obj_set_style_bg_opa(disp_list, LV_OPA_TRANSP, 0);

    confirm_btn = box(list_box, 12, 306, 336, 46);
    lv_obj_set_style_bg_color(confirm_btn, C_TEAL, 0);
    lv_obj_set_style_radius(confirm_btn, 12, 0);
    lv_obj_set_style_shadow_width(confirm_btn, 16, 0);
    lv_obj_set_style_shadow_color(confirm_btn, C_TEAL, 0);
    lv_obj_set_style_shadow_opa(confirm_btn, LV_OPA_40, 0);
    lv_obj_t *cl3 = lbl(confirm_btn, "确认出药", F_CHN, lv_color_white(), 0, 0);
    lv_obj_align(cl3, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(confirm_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(confirm_btn, confirm_cb, LV_EVENT_CLICKED, NULL);

    /* 右侧:出药机剖面 */
    lv_obj_t *mac = box(scr_disp, 392, 98, 392, 366);
    lv_obj_set_style_bg_color(mac, lv_color_hex(0x101A2E), 0);
    lv_obj_set_style_radius(mac, 22, 0);
    lv_obj_set_style_shadow_width(mac, 30, 0);
    lv_obj_set_style_shadow_opa(mac, LV_OPA_20, 0);
    lv_obj_set_style_shadow_ofs_y(mac, 8, 0);

    lv_obj_t *inner = box(mac, 14, 56, 364, 252);
    lv_obj_set_style_bg_color(inner, lv_color_hex(0x182740), 0);
    lv_obj_set_style_radius(inner, 14, 0);
    lv_obj_set_style_border_width(inner, 2, 0);
    lv_obj_set_style_border_color(inner, lv_color_hex(0x2C4166), 0);

    lbl(mac, "智能出药通道", F_CHN, lv_color_hex(0xCFE3EE), 20, 16);

    /* 阶段指示灯 */
    const char *snames[4] = { "货道运转", "药品下落", "传送带", "开门取药" };
    for (int i = 0; i < 4; i++) {
        stage_led[i] = box(mac, 20 + i * 92, 320, 14, 14);
        lv_obj_set_style_bg_color(stage_led[i], C_LINE, 0);
        lv_obj_set_style_radius(stage_led[i], LV_RADIUS_CIRCLE, 0);
        lbl(mac, snames[i], F_CHN, lv_color_hex(0x7E93AC), 12 + i * 92, 340);
    }

    /* 货道:螺旋推杆 */
    spiral_cv = lv_canvas_create(inner);
    lv_canvas_set_buffer(spiral_cv, spiral_buf, 194, 18, LV_IMG_CF_TRUE_COLOR);
    lv_obj_set_pos(spiral_cv, 20, 26);
    lv_obj_t *rail = box(inner, 14, 50, 206, 6);
    lv_obj_set_style_bg_color(rail, lv_color_hex(0x3A5F86), 0);
    lv_obj_set_style_radius(rail, 3, 0);
    lbl(inner, "货道", F_CHN, lv_color_hex(0x7E93AC), 20, 4);

    /* 下落通道 */
    lv_obj_t *chute = box(inner, 236, 20, 64, 132);
    lv_obj_set_style_bg_color(chute, C_CHUTE, 0);
    lv_obj_set_style_radius(chute, 10, 0);
    lv_obj_set_style_border_width(chute, 2, 0);
    lv_obj_set_style_border_color(chute, lv_color_hex(0x2C4166), 0);
    lv_obj_t *arr = lbl(inner, "v", F_CHN, C_AMBER, 260, 74);
    lv_obj_set_style_transform_zoom(arr, 256, 0);

    /* 传送带 */
    belt_cv = lv_canvas_create(inner);
    lv_canvas_set_buffer(belt_cv, belt_buf, 250, 28, LV_IMG_CF_TRUE_COLOR);
    lv_obj_set_pos(belt_cv, 60, 206);
    belt_draw(0);
    spiral_draw(0);

    /* 取药口:光晕 + 门 + 箭头 */
    glow_obj = box(inner, 56, 176, 96, 66);
    lv_obj_set_style_bg_color(glow_obj, lv_color_hex(0x17C964), 0);
    lv_obj_set_style_bg_opa(glow_obj, LV_OPA_20, 0);
    lv_obj_set_style_radius(glow_obj, 14, 0);
    lv_obj_add_flag(glow_obj, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *chute_frame = box(inner, 62, 158, 84, 84);
    lv_obj_set_style_bg_color(chute_frame, C_CHUTE, 0);
    lv_obj_set_style_radius(chute_frame, 12, 0);
    lv_obj_set_style_border_width(chute_frame, 2, 0);
    lv_obj_set_style_border_color(chute_frame, lv_color_hex(0x2C4166), 0);

    door_obj = box(inner, 66, 162, 76, 76);
    lv_obj_set_style_bg_color(door_obj, lv_color_hex(0x3B5A80), 0);
    lv_obj_set_style_bg_grad_color(door_obj, lv_color_hex(0x1F3450), 0);
    lv_obj_set_style_bg_grad_dir(door_obj, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_radius(door_obj, 10, 0);
    lv_obj_t *dh = box(door_obj, 26, 10, 24, 5);
    lv_obj_set_style_bg_color(dh, lv_color_hex(0x8FB4D9), 0);
    lv_obj_set_style_radius(dh, 2, 0);
    lbl(inner, "取药口", F_CHN, lv_color_hex(0x7E93AC), 76, 138);

    arrow_lbl = lbl(inner, "v 请取药 v", F_CHN, lv_color_hex(0x17C964), 62, 246);
    lv_obj_add_flag(arrow_lbl, LV_OBJ_FLAG_HIDDEN);

    /* 药包(动画主角,置于最上层) */
    pkg_obj = box(inner, 176, 66, 44, 38);
    lv_obj_set_style_bg_color(pkg_obj, lv_color_hex(0xFFF4E0), 0);
    lv_obj_set_style_radius(pkg_obj, 6, 0);
    lv_obj_set_style_border_width(pkg_obj, 2, 0);
    lv_obj_set_style_border_color(pkg_obj, lv_color_hex(0xF2B63D), 0);
    pkg_lbl = lbl(pkg_obj, "药", F_CHN, C_RED, 12, 10);

    /* 状态文字 + 进度条 */
    m_status = lbl(mac, "待确认出药", F_CHN, lv_color_hex(0xCFE3EE), 20, 292);
    m_prog = lv_bar_create(mac);
    lv_obj_set_pos(m_prog, 20, 366 - 8);
    lv_obj_set_size(m_prog, 352, 8);
    lv_obj_set_style_bg_color(m_prog, lv_color_hex(0x2C4166), LV_PART_MAIN);
    lv_obj_set_style_bg_color(m_prog, C_TEAL, LV_PART_INDICATOR);
    lv_bar_set_range(m_prog, 0, 100);

    /* 机器驱动定时器 */
    lv_timer_create(machine_timer_cb, 40, NULL);

    /* 成功遮罩 */
    success_mask = box(scr_disp, 0, 0, SCR_W, SCR_H);
    lv_obj_set_style_bg_color(success_mask, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(success_mask, LV_OPA_70, 0);
    lv_obj_add_flag(success_mask, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *card = box(success_mask, 250, 110, 300, 260);
    lv_obj_set_style_bg_color(card, lv_color_white(), 0);
    lv_obj_set_style_radius(card, 20, 0);

    lv_obj_t *ck = box(card, 110, 34, 80, 80);
    lv_obj_set_style_bg_color(ck, C_TEAL, 0);
    lv_obj_set_style_radius(ck, LV_RADIUS_CIRCLE, 0);
    static lv_point_t ck_pts[] = { {24, 50}, {42, 68}, {72, 30} };
    lv_obj_t *line = lv_line_create(card);
    lv_line_set_points(line, ck_pts, 3);
    lv_obj_set_style_line_width(line, 7, 0);
    lv_obj_set_style_line_color(line, C_TEAL, 0);
    lv_obj_set_style_line_rounded(line, true, 0);
    lv_obj_set_pos(line, 102, 26);

    lv_obj_t *ok1 = lbl(card, "取药成功", F_CHN, C_DARK, 0, 0);
    lv_obj_set_style_transform_zoom(ok1, 320, 0);
    lv_obj_align(ok1, LV_ALIGN_TOP_MID, 0, 132);
    lv_obj_t *ok2 = lbl(card, "请取走药品,祝您早日康复", F_CHN, C_GRAY, 0, 0);
    lv_obj_align(ok2, LV_ALIGN_TOP_MID, 0, 176);
    lv_obj_t *ok3 = lbl(card, "即将返回首页…", F_CHN, C_GRAY_L, 0, 0);
    lv_obj_align(ok3, LV_ALIGN_TOP_MID, 0, 210);
}

/* ==================== 入口 ==================== */
void tieyi_ui_create(void)
{
    build_home();
    build_shop();
    build_pay();
    build_dispense();
    lv_timer_create(clock_cb, 1000, NULL);
    lv_scr_load(scr_home);
}
