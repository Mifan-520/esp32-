/*
  Display.h - 从机显示模块 (LVGL版本)
  4寸 ST7796, 480x320 横屏
  粗体大字数字 + 仓名 + 连接状态
*/

#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <lvgl.h>
#include "Config.h"
#include "BleSlave.h"

// 声明中文字体
extern "C" const lv_font_t lv_font_chinese_14;


// 声明130px数字字体
extern "C" const lv_font_t lv_font_numbers_130;

// 声明logo图片
LV_IMG_DECLARE(logo_roastek);

// ============== TFT_eSPI 底层 ==============
inline TFT_eSPI slaveTft = TFT_eSPI();
inline bool slaveDisplayInitialized = false;

// ============== LVGL 对象 ==============
inline lv_disp_draw_buf_t draw_buf;
inline lv_color_t buf[SCREEN_WIDTH * 10];
inline lv_disp_drv_t disp_drv;
inline lv_obj_t* weightLabel = nullptr;
inline lv_obj_t* kgLabel = nullptr;
inline lv_obj_t* titleLabel = nullptr;
inline lv_obj_t* statusLabel = nullptr;
inline lv_obj_t* logoImg = nullptr;
inline uint32_t lastDisplayUpdate = 0;

// ============== 颜色 (与主机一致) ==============
inline const lv_color_t CLR_BG       = lv_color_hex(COLOR_BG);
inline const lv_color_t CLR_BORDER   = lv_color_hex(COLOR_BORDER);
inline const lv_color_t CLR_TEXT     = lv_color_hex(COLOR_TEXT_DARK);
// 在线=深绿色，离线=红色（与主机一致）
inline const lv_color_t CLR_ONLINE   = lv_color_hex(0x00AA00);
inline const lv_color_t CLR_OFFLINE  = lv_color_hex(0xFF3333);
// 仓名用玫瑰红色（主机上料完毕同款）
inline const lv_color_t CLR_TITLE    = lv_color_hex(0x661E2B);

// ============== LVGL flush 回调 ==============
inline void disp_flush(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    slaveTft.startWrite();
    slaveTft.setAddrWindow(area->x1, area->y1, w, h);
    slaveTft.pushColors((uint16_t*)color_p, w * h);
    slaveTft.endWrite();

    lv_disp_flush_ready(drv);
}

// ============== LVGL tick ==============
inline void lv_tick_handler() {
    lv_tick_inc(5);
}

// ============== UI 创建 ==============
inline void Display_CreateUI() {
    lv_obj_t* scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, CLR_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    // --- 边框 ---
    lv_obj_set_style_border_color(scr, CLR_BORDER, 0);
    lv_obj_set_style_border_width(scr, 4, 0);

    // --- 左上角 Logo ---
    logoImg = lv_img_create(scr);
    lv_img_set_src(logoImg, &logo_roastek);
    lv_img_set_zoom(logoImg, 280);
    lv_obj_align(logoImg, LV_ALIGN_TOP_LEFT, 8, 8);

    // --- 仓名 (右上角，玫瑰红色，chinese_14=24px，与logo齐平) ---
    titleLabel = lv_label_create(scr);
    lv_label_set_text_fmt(titleLabel, "仓%d", SLAVE_ID + 1);
    lv_obj_set_style_text_color(titleLabel, CLR_TITLE, 0);
    lv_obj_set_style_text_font(titleLabel, &lv_font_chinese_14, 0);
    lv_obj_set_style_pad_top(titleLabel, 0, 0);
    lv_obj_set_style_pad_left(titleLabel, 0, 0);
    lv_obj_set_style_pad_right(titleLabel, 0, 0);
    lv_obj_set_style_pad_bottom(titleLabel, 0, 0);
    lv_obj_set_style_radius(titleLabel, 0, 0);
    lv_obj_set_style_bg_opa(titleLabel, 0, 0);
    lv_obj_set_style_border_width(titleLabel, 0, 0);
    lv_obj_align(titleLabel, LV_ALIGN_TOP_RIGHT, -8, 12);  // y=12与logo视觉齐平

    // --- 大数字重量 (居中，130px超大字) ---
    weightLabel = lv_label_create(scr);
    lv_label_set_text(weightLabel, "--");
    lv_obj_set_style_text_color(weightLabel, CLR_TEXT, 0);
    lv_obj_set_style_text_font(weightLabel, &lv_font_numbers_130, 0);
    lv_obj_align(weightLabel, LV_ALIGN_CENTER, 0, -30);

    // --- 单位 kg (居中下方，往下移20px总计) ---
    kgLabel = lv_label_create(scr);
    lv_label_set_text(kgLabel, "kg");
    lv_obj_set_style_text_color(kgLabel, CLR_TEXT, 0);
    lv_obj_set_style_text_font(kgLabel, &lv_font_montserrat_48, 0);
    lv_obj_align(kgLabel, LV_ALIGN_CENTER, 0, 70);  // y=50+20=70

    // --- 连接状态 (底部居中，chinese_14=24px) ---
    statusLabel = lv_label_create(scr);
    lv_label_set_text(statusLabel, "离线");
    lv_obj_set_style_text_color(statusLabel, CLR_OFFLINE, 0);
    lv_obj_set_style_text_font(statusLabel, &lv_font_chinese_14, 0);
    lv_obj_align(statusLabel, LV_ALIGN_BOTTOM_MID, 0, -12);
}

// ============== 更新显示 ==============
inline void Display_UpdateContent(bool connected, bool hasSnapshot, float binWeight) {
    static float lastBinWeight = -999;
    static bool lastConnected = false;
    static bool lastHasSnapshot = false;

    bool changed = (binWeight != lastBinWeight) || (connected != lastConnected) || (hasSnapshot != lastHasSnapshot);
    if (!changed) return;

    lastBinWeight = binWeight;
    lastConnected = connected;
    lastHasSnapshot = hasSnapshot;

    // 重量数字
    char buf[16];
    if (connected || hasSnapshot) {
        snprintf(buf, sizeof(buf), "%.1f", binWeight);
    } else {
        snprintf(buf, sizeof(buf), "--");
    }
    lv_label_set_text(weightLabel, buf);

    // 连接状态
    if (connected) {
        lv_label_set_text(statusLabel, "在线");
        lv_obj_set_style_text_color(statusLabel, CLR_ONLINE, 0);
    } else {
        lv_label_set_text(statusLabel, "离线");
        lv_obj_set_style_text_color(statusLabel, CLR_OFFLINE, 0);
    }

    Serial.printf("[Display] %s 仓%d: %.1f kg\n",
                  connected ? "在线" : "离线", SLAVE_ID + 1, binWeight);
}

// ============== 初始化 ==============
inline void Display_Init() {
    slaveTft.begin();
    slaveTft.setRotation(SCREEN_ROTATION);
    slaveTft.fillScreen(TFT_BGR);

    pinMode(TFT_BL_PIN, OUTPUT);
    digitalWrite(TFT_BL_PIN, HIGH);

    lv_init();

    lv_disp_draw_buf_init(&draw_buf, buf, nullptr, SCREEN_WIDTH * 10);
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = SCREEN_WIDTH;
    disp_drv.ver_res = SCREEN_HEIGHT;
    disp_drv.flush_cb = disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    Display_CreateUI();
    slaveDisplayInitialized = true;

    Serial.printf("[Display] LVGL initialized, %dx%d\n", SCREEN_WIDTH, SCREEN_HEIGHT);
}

// ============== 主循环 ==============
inline void Display_Loop() {
    if (!slaveDisplayInitialized) return;

    lv_tick_handler();
    lv_task_handler();

    if (millis() - lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL_MS) {
        lastDisplayUpdate = millis();
        bool connected = BleSlave_IsConnected() && BleSlave_HasRecentData();
        bool hasSnapshot = BleSlave_HasWeightSnapshot();
        float binWeight = BleSlave_GetMyBinWeight();
        Display_UpdateContent(connected, hasSnapshot, binWeight);
    }
}

#endif // DISPLAY_H