/*
  LvglGui.h - 溧阳二期称重系统 4仓布局
  ST7796 480x320 横屏 + LVGL v8
  左侧称重大数字 + 右侧4仓卡片 + 底部操作按钮
*/

#ifndef LVGL_GUI_H
#define LVGL_GUI_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <lvgl.h>
#include "Config.h"
#include "WeightSensor.h"
#include "WeightLogic.h"
#include "BleMaster.h"

extern "C" const lv_font_t lv_font_chinese_14;
extern "C" const lv_font_t lv_font_chinese_18;
LV_IMG_DECLARE(logo_roastek);

// ============== 全局对象 ==============
inline TFT_eSPI tft = TFT_eSPI();
inline bool tftReady = false;
inline const uint8_t TFT_ROTATION = 1;
inline bool touchReady = false;
inline bool touchLastPressed = false;
inline uint32_t touchLastLogMs = 0;
inline uint32_t lvglLastTickMs = 0;

// UI 对象
inline lv_obj_t* lvWeightLabel = nullptr;
inline lv_obj_t* lvUnitLabel = nullptr;
inline lv_obj_t* lvBinCards[BIN_COUNT] = {nullptr};
inline lv_obj_t* lvBinNameLabels[BIN_COUNT] = {nullptr};
inline lv_obj_t* lvBinWeightLabels[BIN_COUNT] = {nullptr};
inline lv_obj_t* lvBinOnlineDots[BIN_COUNT] = {nullptr};
inline lv_obj_t* lvBtnFeed = nullptr;
inline lv_obj_t* lvBtnDischarge = nullptr;
inline lv_obj_t* lvMessageLabel = nullptr;
inline lv_obj_t* lvConfirmBox = nullptr;

// LVGL 驱动
inline lv_disp_draw_buf_t lvDrawBuf;
inline lv_disp_drv_t lvDispDrv;
inline lv_indev_drv_t lvIndevDrv;
inline lv_color_t lvDrawBuffer[480 * 10];
inline uint16_t touchCalData[5] = {275, 3620, 264, 3532, 1};

// 消息自动清除
inline uint32_t lvMsgShowTime = 0;
inline const uint32_t MSG_CLEAR_MS = 4000;
inline bool lvPendingIsLoading = false;
inline int8_t lvPendingBin = -1;
inline float lvPendingWeight = 0.0f;

// ============== 颜色（来自 Config.h） ==============
inline const lv_color_t CLR_BG       = lv_color_hex(COLOR_BG);
inline const lv_color_t CLR_PANEL    = lv_color_hex(COLOR_PANEL);
inline const lv_color_t CLR_BORDER   = lv_color_hex(COLOR_BORDER);
inline const lv_color_t CLR_BTN      = lv_color_hex(COLOR_BUTTON_NORMAL);
inline const lv_color_t CLR_BTN_HL   = lv_color_hex(COLOR_BUTTON_HIGHLIGHT);
inline const lv_color_t CLR_TEXT     = lv_color_hex(COLOR_TEXT_DARK);
inline const lv_color_t CLR_TEXT_LT  = lv_color_hex(COLOR_TEXT_LIGHT);
inline const lv_color_t CLR_ONLINE   = lv_color_hex(0x00CC00);
inline const lv_color_t CLR_OFFLINE  = lv_color_hex(0xCC0000);

// ============== 字体 ==============
inline const lv_font_t* GetChineseFont() { return &lv_font_chinese_14; }
inline const lv_font_t* GetChineseFont18() { return &lv_font_chinese_14; }
inline const lv_font_t* GetWeightMixedFont() { return &lv_font_chinese_18; }
inline const lv_font_t* GetFont48() { return &lv_font_montserrat_48; }
inline const lv_font_t* GetFont32() { return &lv_font_montserrat_32; }
inline const lv_font_t* GetFont24() { return &lv_font_montserrat_24; }
inline const lv_font_t* GetFont18() { return &lv_font_montserrat_18; }

// ============== 仓卡片点击 ==============
inline void LvglGui_BinCardEvent(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    WeightLogic_SelectBin(WeightLogic_GetSelectedBin() == idx ? -1 : idx);
    Serial.printf("[LVGL] 选中仓: %d\n", WeightLogic_GetSelectedBin());
}

// ============== 按钮事件 ==============
inline void LvglGui_ShowMessage(const char* text) {
    if (!lvMessageLabel) return;
    lv_label_set_text(lvMessageLabel, text);
    lv_obj_set_style_text_font(lvMessageLabel, GetChineseFont(), 0);
    lv_obj_set_style_text_color(lvMessageLabel, CLR_TEXT, 0);
    lvMsgShowTime = millis();
}

// --- 自定义模态对话框（替代 lv_msgbox，修复白框和乱码）---
inline lv_obj_t* lvDialogConfirmBtn = nullptr;
inline lv_obj_t* lvDialogCancelBtn = nullptr;

inline void LvglGui_CloseConfirmDialog() {
    if (!lvConfirmBox) return;
    lv_obj_del(lvConfirmBox);
    lvConfirmBox = nullptr;
    lvDialogConfirmBtn = nullptr;
    lvDialogCancelBtn = nullptr;
}

inline void LvglGui_DialogBtnEvent(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    bool confirmed = (lv_event_get_target(e) == lvDialogConfirmBtn);

    LvglGui_CloseConfirmDialog();

    if (!confirmed) {
        LvglGui_ShowMessage("已取消操作");
        return;
    }

    WeightLogic_SelectBin(lvPendingBin);
    if (lvPendingIsLoading) {
        WeightLogic_LoadingDone();
    } else {
        WeightLogic_UnloadingDone();
    }
    LvglGui_ShowMessage(WeightLogic_GetLastMessage());
}

inline void LvglGui_ShowConfirmDialog(bool isLoading) {
    int8_t selectedBin = WeightLogic_GetSelectedBin();
    if (selectedBin < 0 || selectedBin >= BIN_COUNT) {
        LvglGui_ShowMessage("已取消操作");
        return;
    }

    lvPendingIsLoading = isLoading;
    lvPendingBin = selectedBin;
    lvPendingWeight = WeightSensor_GetFilteredWeight();

    LvglGui_CloseConfirmDialog();

    // 半透明遮罩
    lv_obj_t* overlay = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(overlay);
    lv_obj_set_size(overlay, 480, 320);
    lv_obj_set_style_bg_color(overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_50, 0);
    lv_obj_set_pos(overlay, 0, 0);
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(overlay, LvglGui_DialogBtnEvent, LV_EVENT_CLICKED, nullptr);
    // 点遮罩 = 取消
    lvConfirmBox = overlay;

    // 对话框主体
    lv_obj_t* dialog = lv_obj_create(overlay);
    lv_obj_remove_style_all(dialog);
    lv_obj_set_size(dialog, 300, 176);
    lv_obj_set_style_bg_color(dialog, CLR_PANEL, 0);
    lv_obj_set_style_bg_opa(dialog, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(dialog, CLR_BORDER, 0);
    lv_obj_set_style_border_width(dialog, 2, 0);
    lv_obj_set_style_radius(dialog, 10, 0);
    lv_obj_set_style_pad_all(dialog, 14, 0);
    lv_obj_center(dialog);
    lv_obj_clear_flag(dialog, LV_OBJ_FLAG_CLICKABLE);  // 不拦截点击

    // 提示文字（使用放大后的中文字体）
    lv_obj_t* msgLabel = lv_label_create(dialog);
    char text[96];
    snprintf(text, sizeof(text), "%s%s %.1fkg",
             isLoading ? "上料" : "下料",
             BIN_NAMES[selectedBin],
             lvPendingWeight);
    lv_label_set_text(msgLabel, text);
    lv_obj_set_style_text_font(msgLabel, GetChineseFont(), 0);
    lv_obj_set_style_text_color(msgLabel, CLR_TEXT, 0);
    lv_obj_set_width(msgLabel, 256);
    lv_label_set_long_mode(msgLabel, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(msgLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(msgLabel, LV_ALIGN_TOP_MID, 0, 12);

    // 按钮容器
    lv_obj_t* btnContainer = lv_obj_create(dialog);
    lv_obj_remove_style_all(btnContainer);
    lv_obj_set_size(btnContainer, 264, 44);
    lv_obj_set_style_pad_all(btnContainer, 0, 0);
    lv_obj_set_flex_flow(btnContainer, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btnContainer, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_align(btnContainer, LV_ALIGN_BOTTOM_MID, 0, -10);

    // 确认按钮
    lvDialogConfirmBtn = lv_btn_create(btnContainer);
    lv_obj_set_size(lvDialogConfirmBtn, 118, 40);
    lv_obj_set_style_radius(lvDialogConfirmBtn, 6, 0);
    lv_obj_set_style_bg_color(lvDialogConfirmBtn, CLR_BORDER, 0);
    lv_obj_set_style_bg_opa(lvDialogConfirmBtn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(lvDialogConfirmBtn, 0, 0);
    lv_obj_set_style_shadow_width(lvDialogConfirmBtn, 0, 0);
    lv_obj_add_event_cb(lvDialogConfirmBtn, LvglGui_DialogBtnEvent, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* confirmLbl = lv_label_create(lvDialogConfirmBtn);
    lv_label_set_text(confirmLbl, "确认");
    lv_obj_set_style_text_font(confirmLbl, GetChineseFont18(), 0);
    lv_obj_set_style_text_color(confirmLbl, CLR_TEXT_LT, 0);
    lv_obj_center(confirmLbl);

    // 取消按钮
    lvDialogCancelBtn = lv_btn_create(btnContainer);
    lv_obj_set_size(lvDialogCancelBtn, 118, 40);
    lv_obj_set_style_radius(lvDialogCancelBtn, 6, 0);
    lv_obj_set_style_bg_color(lvDialogCancelBtn, CLR_BTN, 0);
    lv_obj_set_style_bg_opa(lvDialogCancelBtn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(lvDialogCancelBtn, 0, 0);
    lv_obj_set_style_shadow_width(lvDialogCancelBtn, 0, 0);
    lv_obj_add_event_cb(lvDialogCancelBtn, LvglGui_DialogBtnEvent, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* cancelLbl = lv_label_create(lvDialogCancelBtn);
    lv_label_set_text(cancelLbl, "取消");
    lv_obj_set_style_text_font(cancelLbl, GetChineseFont18(), 0);
    lv_obj_set_style_text_color(cancelLbl, CLR_TEXT_LT, 0);
    lv_obj_center(cancelLbl);
}

inline void LvglGui_BtnFeedEvent(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    Serial.println("[LVGL] 上料完毕按钮点击");
    LvglGui_ShowConfirmDialog(true);
}

inline void LvglGui_BtnDischargeEvent(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    Serial.println("[LVGL] 下料完毕按钮点击");
    LvglGui_ShowConfirmDialog(false);
}

// ============================================================
//  编辑按钮 → 数字键盘 → 确认对话框（仓重量手动输入）
// ============================================================

// --- 状态 ---
inline lv_obj_t* lvEditOverlay = nullptr;
inline lv_obj_t* lvEditBinBtns[BIN_COUNT] = {nullptr};
inline int8_t lvEditSelectedBin = 0;
inline char lvEditInputBuf[16] = "";
inline lv_obj_t* lvEditInputLabel = nullptr;
inline lv_obj_t* lvEditKeyMatrix = nullptr;

// --- 键盘 map（4×3：7 8 9 / 4 5 6 / 1 2 3 / . 0 ⌫）---
static const char* kbMap[] = {
    "7", "8", "9", "\n",
    "4", "5", "6", "\n",
    "1", "2", "3", "\n",
    ".", "0", LV_SYMBOL_BACKSPACE, ""
};

// --- 关闭编辑面板 ---
inline void LvglGui_CloseEditPanel() {
    if (!lvEditOverlay) return;
    lv_obj_del(lvEditOverlay);
    lvEditOverlay = nullptr;
    lvEditInputLabel = nullptr;
    lvEditKeyMatrix = nullptr;
    for (int i = 0; i < BIN_COUNT; i++) lvEditBinBtns[i] = nullptr;
}

// --- 更新仓按钮选中高亮 ---
inline void LvglGui_UpdateEditBinHighlight() {
    for (int i = 0; i < BIN_COUNT; i++) {
        if (!lvEditBinBtns[i]) continue;
        bool sel = (i == lvEditSelectedBin);
        lv_obj_set_style_bg_color(lvEditBinBtns[i],
            sel ? lv_color_hex(0xE8D0D0) : CLR_PANEL, 0);
        lv_obj_set_style_border_color(lvEditBinBtns[i],
            sel ? CLR_BORDER : CLR_PANEL, 0);
        lv_obj_set_style_border_width(lvEditBinBtns[i], sel ? 2 : 1, 0);
    }
}

// --- 更新输入显示 ---
inline void LvglGui_UpdateEditInputDisplay() {
    if (!lvEditInputLabel) return;
    char display[24];
    snprintf(display, sizeof(display), "%s kg",
             strlen(lvEditInputBuf) == 0 ? "0" : lvEditInputBuf);
    lv_label_set_text(lvEditInputLabel, display);
}

inline void LvglGui_LoadBinWeightToEditInput(int8_t bin) {
    if (bin < 0 || bin >= BIN_COUNT) return;
    snprintf(lvEditInputBuf, sizeof(lvEditInputBuf), "%.1f", WeightLogic_GetBinWeight(bin));
    LvglGui_UpdateEditInputDisplay();
}

// --- 编辑确认对话框（复用现有风格） ---
inline void LvglGui_ShowEditConfirm() {
    if (strlen(lvEditInputBuf) == 0) {
        LvglGui_ShowMessage("请输入重量");
        return;
    }
    float val = atof(lvEditInputBuf);
    int8_t bin = lvEditSelectedBin;

    // 关闭编辑面板
    LvglGui_CloseEditPanel();

    // 复用确认对话框框架
    lv_obj_t* overlay = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(overlay);
    lv_obj_set_size(overlay, 480, 320);
    lv_obj_set_style_bg_color(overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_50, 0);
    lv_obj_set_pos(overlay, 0, 0);
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_CLICKABLE);
    lvConfirmBox = overlay;

    lv_obj_t* dialog = lv_obj_create(overlay);
    lv_obj_remove_style_all(dialog);
    lv_obj_set_size(dialog, 300, 176);
    lv_obj_set_style_bg_color(dialog, CLR_PANEL, 0);
    lv_obj_set_style_bg_opa(dialog, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(dialog, CLR_BORDER, 0);
    lv_obj_set_style_border_width(dialog, 2, 0);
    lv_obj_set_style_radius(dialog, 10, 0);
    lv_obj_set_style_pad_all(dialog, 14, 0);
    lv_obj_center(dialog);
    lv_obj_clear_flag(dialog, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* msgLabel = lv_label_create(dialog);
    char text[96];
    snprintf(text, sizeof(text), "修改%s为%.1fkg", BIN_NAMES[bin], val);
    lv_label_set_text(msgLabel, text);
    lv_obj_set_style_text_font(msgLabel, GetChineseFont(), 0);
    lv_obj_set_style_text_color(msgLabel, CLR_TEXT, 0);
    lv_obj_set_width(msgLabel, 256);
    lv_label_set_long_mode(msgLabel, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(msgLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(msgLabel, LV_ALIGN_TOP_MID, 0, 12);

    lv_obj_t* btnContainer = lv_obj_create(dialog);
    lv_obj_remove_style_all(btnContainer);
    lv_obj_set_size(btnContainer, 264, 44);
    lv_obj_set_style_pad_all(btnContainer, 0, 0);
    lv_obj_set_flex_flow(btnContainer, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btnContainer, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_align(btnContainer, LV_ALIGN_BOTTOM_MID, 0, -10);

    // 确认按钮
    lv_obj_t* confirmBtn = lv_btn_create(btnContainer);
    lv_obj_set_size(confirmBtn, 118, 40);
    lv_obj_set_style_radius(confirmBtn, 6, 0);
    lv_obj_set_style_bg_color(confirmBtn, CLR_BORDER, 0);
    lv_obj_set_style_bg_opa(confirmBtn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(confirmBtn, 0, 0);
    lv_obj_set_style_shadow_width(confirmBtn, 0, 0);
    lv_obj_t* cLbl = lv_label_create(confirmBtn);
    lv_label_set_text(cLbl, "确认");
    lv_obj_set_style_text_font(cLbl, GetChineseFont18(), 0);
    lv_obj_set_style_text_color(cLbl, CLR_TEXT_LT, 0);
    lv_obj_center(cLbl);

    // 取消按钮
    lv_obj_t* cancelBtn = lv_btn_create(btnContainer);
    lv_obj_set_size(cancelBtn, 118, 40);
    lv_obj_set_style_radius(cancelBtn, 6, 0);
    lv_obj_set_style_bg_color(cancelBtn, CLR_BTN, 0);
    lv_obj_set_style_bg_opa(cancelBtn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(cancelBtn, 0, 0);
    lv_obj_set_style_shadow_width(cancelBtn, 0, 0);
    lv_obj_t* xLbl = lv_label_create(cancelBtn);
    lv_label_set_text(xLbl, "取消");
    lv_obj_set_style_text_font(xLbl, GetChineseFont18(), 0);
    lv_obj_set_style_text_color(xLbl, CLR_TEXT_LT, 0);
    lv_obj_center(xLbl);

    lvPendingBin = bin;
    lvPendingWeight = val;

    lv_obj_add_event_cb(confirmBtn, [](lv_event_t* e) {
        if (lvConfirmBox) { lv_obj_del(lvConfirmBox); lvConfirmBox = nullptr; }
        WeightLogic_SetBinWeight(lvPendingBin, lvPendingWeight);
        BleMaster_SendWeight(WeightSensor_GetFilteredWeight(), binWeights);
        char msg[64];
        snprintf(msg, sizeof(msg), "修改%s为%.1fkg", BIN_NAMES[lvPendingBin], lvPendingWeight);
        LvglGui_ShowMessage(msg);
    }, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_event_cb(cancelBtn, [](lv_event_t* e) {
        if (lvConfirmBox) { lv_obj_del(lvConfirmBox); lvConfirmBox = nullptr; }
        LvglGui_ShowMessage("已取消");
    }, LV_EVENT_CLICKED, nullptr);
    // 点遮罩也取消
    lv_obj_add_event_cb(overlay, [](lv_event_t* e) {
        if (lvConfirmBox) { lv_obj_del(lvConfirmBox); lvConfirmBox = nullptr; }
        LvglGui_ShowMessage("已取消");
    }, LV_EVENT_CLICKED, nullptr);
}

// --- 键盘按键回调 ---
inline void LvglGui_EditKeyEventHandler(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
    uint16_t btn = lv_btnmatrix_get_selected_btn(lvEditKeyMatrix);
    if (btn == LV_BTNMATRIX_BTN_NONE) return;
    const char* txt = lv_btnmatrix_get_btn_text(lvEditKeyMatrix, btn);
    if (!txt) return;

    if (strcmp(txt, LV_SYMBOL_BACKSPACE) == 0) {
        // 退格：删除最后一个字符
        int len = strlen(lvEditInputBuf);
        if (len > 0) {
            lvEditInputBuf[len - 1] = '\0';
        }
    } else if (strcmp(txt, ".") == 0) {
        // 小数点：只允许一个
        if (strchr(lvEditInputBuf, '.') == nullptr) {
            // 如果为空或只有数字，可以加小数点
            int len = strlen(lvEditInputBuf);
            if (len < (int)sizeof(lvEditInputBuf) - 1) {
                // 小数点后最多1位
                lvEditInputBuf[len] = '.';
                lvEditInputBuf[len + 1] = '\0';
            }
        }
    } else {
        // 数字 0-9
        int len = strlen(lvEditInputBuf);
        // 小数点后最多1位
        const char* dotPos = strchr(lvEditInputBuf, '.');
        if (dotPos != nullptr) {
            int decimals = len - (dotPos - lvEditInputBuf) - 1;
            if (decimals >= 1) return;  // 已有1位小数，不再接受
        }
        // 总长度限制（避免溢出，最多6字符如 "999.9"）
        if (len >= 6) return;
        lvEditInputBuf[len] = txt[0];
        lvEditInputBuf[len + 1] = '\0';
    }
    LvglGui_UpdateEditInputDisplay();
}

// --- 仓选择回调 ---
inline void LvglGui_EditBinSelectHandler(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    lvEditSelectedBin = idx;
    LvglGui_UpdateEditBinHighlight();
    LvglGui_LoadBinWeightToEditInput(idx);
}

// --- 确认按钮回调（键盘面板内的确认） ---
inline void LvglGui_EditConfirmHandler(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    LvglGui_ShowEditConfirm();
}

// --- 打开编辑面板 ---
inline void LvglGui_OpenEditPanel() {
    if (lvEditOverlay) return;  // 已打开

    int8_t currentBin = WeightLogic_GetSelectedBin();
    lvEditSelectedBin = (currentBin >= 0 && currentBin < BIN_COUNT) ? currentBin : 0;
    memset(lvEditInputBuf, 0, sizeof(lvEditInputBuf));

    // 半透明遮罩
    lvEditOverlay = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(lvEditOverlay);
    lv_obj_set_size(lvEditOverlay, 480, 320);
    lv_obj_set_style_bg_color(lvEditOverlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(lvEditOverlay, LV_OPA_50, 0);
    lv_obj_set_pos(lvEditOverlay, 0, 0);
    lv_obj_add_flag(lvEditOverlay, LV_OBJ_FLAG_CLICKABLE);

    // 主面板
    lv_obj_t* panel = lv_obj_create(lvEditOverlay);
    lv_obj_remove_style_all(panel);
    lv_obj_set_size(panel, 460, 280);
    lv_obj_set_style_bg_color(panel, CLR_PANEL, 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(panel, CLR_BORDER, 0);
    lv_obj_set_style_border_width(panel, 2, 0);
    lv_obj_set_style_radius(panel, 10, 0);
    lv_obj_set_style_pad_all(panel, 6, 0);
    lv_obj_center(panel);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_CLICKABLE);

    // === 顶部标题 ===
    lv_obj_t* titleLabel = lv_label_create(panel);
    lv_label_set_text(titleLabel, "编辑仓重量");
    lv_obj_set_style_text_font(titleLabel, GetChineseFont18(), 0);
    lv_obj_set_style_text_color(titleLabel, CLR_TEXT, 0);
    lv_obj_align(titleLabel, LV_ALIGN_TOP_LEFT, 4, 2);

    // 关闭按钮（右上角 X）
    lv_obj_t* closeBtn = lv_btn_create(panel);
    lv_obj_set_size(closeBtn, 32, 32);
    lv_obj_set_style_radius(closeBtn, 16, 0);
    lv_obj_set_style_bg_color(closeBtn, CLR_BTN, 0);
    lv_obj_set_style_bg_opa(closeBtn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(closeBtn, 0, 0);
    lv_obj_set_style_shadow_width(closeBtn, 0, 0);
    lv_obj_align(closeBtn, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_t* xLabel = lv_label_create(closeBtn);
    lv_label_set_text(xLabel, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_color(xLabel, CLR_TEXT_LT, 0);
    lv_obj_center(xLabel);
    // 点关闭 = 关闭面板
    lv_obj_add_event_cb(closeBtn, [](lv_event_t* e) {
        LvglGui_CloseEditPanel();
    }, LV_EVENT_CLICKED, nullptr);
    // 点遮罩 = 关闭面板
    lv_obj_add_event_cb(lvEditOverlay, [](lv_event_t* e) {
        LvglGui_CloseEditPanel();
    }, LV_EVENT_CLICKED, nullptr);

    // === 内容区：左侧仓选择 + 右侧键盘 ===
    lv_obj_t* content = lv_obj_create(panel);
    lv_obj_remove_style_all(content);
    lv_obj_set_size(content, 440, 244);
    lv_obj_set_style_pad_all(content, 4, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_align(content, LV_ALIGN_TOP_LEFT, 0, 24);

    // --- 左侧：仓1-4选择 ---
    lv_obj_t* leftCol = lv_obj_create(content);
    lv_obj_remove_style_all(leftCol);
    lv_obj_set_size(leftCol, 90, 236);
    lv_obj_set_style_pad_all(leftCol, 2, 0);
    lv_obj_set_style_pad_row(leftCol, 4, 0);
    lv_obj_set_flex_flow(leftCol, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(leftCol, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    for (int i = 0; i < BIN_COUNT; i++) {
        lvEditBinBtns[i] = lv_btn_create(leftCol);
        lv_obj_set_size(lvEditBinBtns[i], 82, 54);
        lv_obj_set_style_radius(lvEditBinBtns[i], 6, 0);
        lv_obj_set_style_bg_color(lvEditBinBtns[i],
            i == 0 ? lv_color_hex(0xE8D0D0) : CLR_PANEL, 0);
        lv_obj_set_style_bg_opa(lvEditBinBtns[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(lvEditBinBtns[i],
            i == 0 ? CLR_BORDER : CLR_PANEL, 0);
        lv_obj_set_style_border_width(lvEditBinBtns[i], i == 0 ? 2 : 1, 0);
        lv_obj_set_style_shadow_width(lvEditBinBtns[i], 0, 0);
        lv_obj_add_event_cb(lvEditBinBtns[i], LvglGui_EditBinSelectHandler,
                            LV_EVENT_CLICKED, (void*)(intptr_t)i);
        lv_obj_t* bLbl = lv_label_create(lvEditBinBtns[i]);
        lv_label_set_text(bLbl, BIN_NAMES[i]);
        lv_obj_set_style_text_font(bLbl, GetChineseFont18(), 0);
        lv_obj_set_style_text_color(bLbl, CLR_TEXT, 0);
        lv_obj_center(bLbl);
    }

    // --- 右侧：输入显示 + 数字键盘 + 确认按钮 ---
    lv_obj_t* rightCol = lv_obj_create(content);
    lv_obj_remove_style_all(rightCol);
    lv_obj_set_size(rightCol, 338, 236);
    lv_obj_set_style_pad_all(rightCol, 4, 0);
    lv_obj_set_style_pad_row(rightCol, 4, 0);
    lv_obj_set_flex_flow(rightCol, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(rightCol, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // 输入显示区
    lv_obj_t* inputBox = lv_obj_create(rightCol);
    lv_obj_remove_style_all(inputBox);
    lv_obj_set_size(inputBox, 320, 36);
    lv_obj_set_style_bg_color(inputBox, CLR_BG, 0);
    lv_obj_set_style_bg_opa(inputBox, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(inputBox, 6, 0);
    lv_obj_set_style_border_color(inputBox, CLR_BORDER, 0);
    lv_obj_set_style_border_width(inputBox, 1, 0);

    lvEditInputLabel = lv_label_create(inputBox);
    lv_label_set_text(lvEditInputLabel, "0.0 kg");
    lv_obj_set_style_text_font(lvEditInputLabel, GetFont18(), 0);
    lv_obj_set_style_text_color(lvEditInputLabel, CLR_TEXT, 0);
    lv_obj_align(lvEditInputLabel, LV_ALIGN_RIGHT_MID, -8, 0);

    // 数字键盘 btnmatrix
    lvEditKeyMatrix = lv_btnmatrix_create(rightCol);
    lv_obj_set_size(lvEditKeyMatrix, 320, 130);
    lv_btnmatrix_set_map(lvEditKeyMatrix, kbMap);
    lv_obj_set_style_bg_color(lvEditKeyMatrix, CLR_PANEL, 0);
    lv_obj_set_style_bg_opa(lvEditKeyMatrix, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(lvEditKeyMatrix, 0, 0);
    lv_obj_set_style_radius(lvEditKeyMatrix, 6, 0);
    lv_obj_set_style_pad_all(lvEditKeyMatrix, 2, 0);
    lv_obj_set_style_pad_gap(lvEditKeyMatrix, 4, 0);
    // 按钮样式
    lv_obj_set_style_bg_color(lvEditKeyMatrix, CLR_BG, LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(lvEditKeyMatrix, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_text_color(lvEditKeyMatrix, CLR_TEXT, LV_PART_ITEMS);
    lv_obj_set_style_text_font(lvEditKeyMatrix, GetFont18(), LV_PART_ITEMS);
    lv_obj_set_style_radius(lvEditKeyMatrix, 6, LV_PART_ITEMS);
    lv_obj_set_style_border_width(lvEditKeyMatrix, 0, LV_PART_ITEMS);
    lv_obj_set_style_shadow_width(lvEditKeyMatrix, 0, LV_PART_ITEMS);
    // 按下高亮
    lv_obj_set_style_bg_color(lvEditKeyMatrix, CLR_BORDER, LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_text_color(lvEditKeyMatrix, CLR_TEXT_LT, LV_PART_ITEMS | LV_STATE_PRESSED);
    // 退格键特殊颜色
    // （btnmatrix 在 v8 中不方便单独设某键颜色，统一即可）

    lv_obj_add_event_cb(lvEditKeyMatrix, LvglGui_EditKeyEventHandler,
                        LV_EVENT_VALUE_CHANGED, nullptr);

    // 确认按钮
    lv_obj_t* editConfirmBtn = lv_btn_create(rightCol);
    lv_obj_set_size(editConfirmBtn, 320, 36);
    lv_obj_set_style_radius(editConfirmBtn, 6, 0);
    lv_obj_set_style_bg_color(editConfirmBtn, CLR_BORDER, 0);
    lv_obj_set_style_bg_opa(editConfirmBtn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(editConfirmBtn, 0, 0);
    lv_obj_set_style_shadow_width(editConfirmBtn, 0, 0);
    lv_obj_t* ecLbl = lv_label_create(editConfirmBtn);
    lv_label_set_text(ecLbl, "确认");
    lv_obj_set_style_text_font(ecLbl, GetChineseFont18(), 0);
    lv_obj_set_style_text_color(ecLbl, CLR_TEXT_LT, 0);
    lv_obj_center(ecLbl);
    lv_obj_add_event_cb(editConfirmBtn, LvglGui_EditConfirmHandler,
                        LV_EVENT_CLICKED, nullptr);

    LvglGui_UpdateEditBinHighlight();
    LvglGui_LoadBinWeightToEditInput(lvEditSelectedBin);

    Serial.println("[LVGL] 编辑面板已打开");
}

// --- 编辑按钮事件 ---
inline void LvglGui_BtnEditEvent(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    Serial.println("[LVGL] 编辑按钮点击");
    LvglGui_OpenEditPanel();
}

// ============== 刷新业务数据 ==============
inline void LvglGui_UpdateBusinessLabels() {
    // 当前重量
    if (lvWeightLabel) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.1f", WeightSensor_GetFilteredWeight());
        lv_label_set_text(lvWeightLabel, buf);
    }

    // 4仓数据
    const float* bins = WeightLogic_GetAllBinWeights();
    int8_t sel = WeightLogic_GetSelectedBin();

    for (int i = 0; i < BIN_COUNT; i++) {
        if (lvBinWeightLabels[i]) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%.1f kg", bins[i]);
            lv_label_set_text(lvBinWeightLabels[i], buf);
        }
        if (lvBinOnlineDots[i]) {
            // 在线=浅绿色背景，离线=红色背景
            lv_obj_set_style_bg_color(lvBinOnlineDots[i],
                BleMaster_IsSlaveOnline(i) ? lv_color_hex(0x66FF66) : lv_color_hex(0xFF3333), 0);
        }
        if (lvBinCards[i]) {
            bool isSel = (sel == i);
            lv_obj_set_style_border_color(lvBinCards[i],
                isSel ? CLR_BORDER : CLR_PANEL, 0);
            lv_obj_set_style_border_width(lvBinCards[i], isSel ? 3 : 1, 0);
            lv_obj_set_style_bg_color(lvBinCards[i],
                isSel ? lv_color_hex(0xE8D0D0) : CLR_PANEL, 0);
        }
    }

    // 消息自动清除
    if (lvMsgShowTime > 0 && lvMessageLabel) {
        if (millis() - lvMsgShowTime >= MSG_CLEAR_MS) {
            lv_label_set_text(lvMessageLabel, "");
            lvMsgShowTime = 0;
        }
    }
}

// ============== 显示回调 ==============
inline void LvglGui_FlushCb(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* colorP) {
    uint32_t w = (uint32_t)(area->x2 - area->x1 + 1);
    uint32_t h = (uint32_t)(area->y2 - area->y1 + 1);
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t*)&colorP->full, w * h, true);
    tft.endWrite();
    lv_disp_flush_ready(disp);
}

inline uint16_t LvglGui_FlipTouchAxis(uint16_t value, int maxValue) {
    if (maxValue <= 0) return value;
    if ((int)value >= maxValue) return 0;
    return (uint16_t)((maxValue - 1) - value);
}

inline void LvglGui_TouchReadCb(lv_indev_drv_t* indev, lv_indev_data_t* data) {
    (void)indev;
    uint16_t tx = 0, ty = 0;
    bool pressed = tft.getTouch(&tx, &ty, 300);
    if (pressed) {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = LvglGui_FlipTouchAxis(tx, tft.width());
        data->point.y = LvglGui_FlipTouchAxis(ty, tft.height());
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

// ============== 创建 UI ==============
inline void LvglGui_CreateUi() {
    // 背景
    lv_obj_set_style_bg_color(lv_scr_act(), CLR_BG, 0);
    lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, 0);

    // 根容器 480x320，竖向 Flex
    lv_obj_t* root = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, 480, 320);
    lv_obj_set_style_bg_opa(root, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);

    // ====== Logo 区 ======
    lv_obj_t* headerRow = lv_obj_create(root);
    lv_obj_remove_style_all(headerRow);
    lv_obj_set_size(headerRow, 480, 56);
    lv_obj_set_style_pad_left(headerRow, 8, 0);
    lv_obj_set_style_pad_ver(headerRow, 0, 0);

    lv_obj_t* logoImg = lv_img_create(headerRow);
    lv_img_set_src(logoImg, &logo_roastek);
    lv_img_set_zoom(logoImg, 280);  // 约 109%，轻微放大且不越界
    lv_obj_align(logoImg, LV_ALIGN_LEFT_MID, 0, 0);

    // 编辑按钮（右上角，与 logo 同行）
    lv_obj_t* lvBtnEdit = lv_btn_create(headerRow);
    lv_obj_set_size(lvBtnEdit, 56, 36);
    lv_obj_set_style_radius(lvBtnEdit, 6, 0);
    lv_obj_set_style_bg_color(lvBtnEdit, CLR_BORDER, 0);
    lv_obj_set_style_bg_opa(lvBtnEdit, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(lvBtnEdit, 0, 0);
    lv_obj_set_style_shadow_width(lvBtnEdit, 0, 0);
    lv_obj_align(lvBtnEdit, LV_ALIGN_RIGHT_MID, -8, 0);
    lv_obj_add_event_cb(lvBtnEdit, LvglGui_BtnEditEvent, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* editLbl = lv_label_create(lvBtnEdit);
    lv_label_set_text(editLbl, "编辑");
    lv_obj_set_style_text_font(editLbl, GetChineseFont18(), 0);
    lv_obj_set_style_text_color(editLbl, CLR_TEXT_LT, 0);
    lv_obj_center(editLbl);

    // ====== 内容区：左称重 + 右4仓 ======
    lv_obj_t* content = lv_obj_create(root);
    lv_obj_remove_style_all(content);
    lv_obj_set_size(content, 480, 196);
    lv_obj_set_style_pad_all(content, 4, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // --- 左侧: 称重面板 ---
    lv_obj_t* leftPanel = lv_obj_create(content);
    lv_obj_remove_style_all(leftPanel);
    lv_obj_set_size(leftPanel, 268, 196);
    lv_obj_set_style_bg_color(leftPanel, CLR_PANEL, 0);
    lv_obj_set_style_bg_opa(leftPanel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(leftPanel, 8, 0);
    lv_obj_set_style_border_width(leftPanel, 2, 0);
    lv_obj_set_style_border_color(leftPanel, CLR_BORDER, 0);
    lv_obj_set_style_pad_all(leftPanel, 2, 0);

    // "称重" 标题（真实大字体）
    lv_obj_t* titleLabel = lv_label_create(leftPanel);
    lv_label_set_text(titleLabel, "称重");
    lv_obj_set_style_text_font(titleLabel, GetChineseFont18(), 0);
    lv_obj_set_style_text_color(titleLabel, CLR_TEXT, 0);
    lv_obj_align(titleLabel, LV_ALIGN_TOP_LEFT, 0, 2);

    // 大数字重量（真实大号混排字体，继续放大）
    lvWeightLabel = lv_label_create(leftPanel);
    lv_label_set_text(lvWeightLabel, "0.0");
    lv_obj_set_style_text_font(lvWeightLabel, GetWeightMixedFont(), 0);
    lv_obj_set_style_text_color(lvWeightLabel, CLR_TEXT, 0);
    lv_obj_align(lvWeightLabel, LV_ALIGN_CENTER, 0, 0);

    // "kg" 单位（真实变大）
    lvUnitLabel = lv_label_create(leftPanel);
    lv_label_set_text(lvUnitLabel, "kg");
    lv_obj_set_style_text_font(lvUnitLabel, GetFont32(), 0);
    lv_obj_set_style_text_color(lvUnitLabel, CLR_TEXT, 0);
    lv_obj_align(lvUnitLabel, LV_ALIGN_CENTER, 0, 60);

    // --- 右侧：4仓卡片（紧密排列，与称重面板齐平） ---
    lv_obj_t* rightPanel = lv_obj_create(content);
    lv_obj_remove_style_all(rightPanel);
    lv_obj_set_size(rightPanel, 192, 196);
    lv_obj_set_style_pad_all(rightPanel, 2, 0);
    lv_obj_set_style_pad_row(rightPanel, 2, 0);
    lv_obj_set_flex_flow(rightPanel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(rightPanel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    for (int i = 0; i < BIN_COUNT; i++) {
        // 卡片容器（紧密排列）
        lvBinCards[i] = lv_obj_create(rightPanel);
        lv_obj_remove_style_all(lvBinCards[i]);
        lv_obj_set_size(lvBinCards[i], 186, 47);
        lv_obj_set_style_bg_color(lvBinCards[i], CLR_PANEL, 0);
        lv_obj_set_style_bg_opa(lvBinCards[i], LV_OPA_COVER, 0);
        lv_obj_set_style_radius(lvBinCards[i], 4, 0);
        lv_obj_set_style_border_width(lvBinCards[i], 1, 0);
        lv_obj_set_style_border_color(lvBinCards[i], CLR_PANEL, 0);
        lv_obj_set_style_pad_all(lvBinCards[i], 4, 0);
        lv_obj_add_flag(lvBinCards[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(lvBinCards[i], LvglGui_BinCardEvent,
                            LV_EVENT_CLICKED, (void*)(intptr_t)i);

        // 仓名 (左侧，真实大字体)
        lvBinNameLabels[i] = lv_label_create(lvBinCards[i]);
        lv_label_set_text(lvBinNameLabels[i], BIN_NAMES[i]);
        lv_obj_set_style_text_font(lvBinNameLabels[i], GetChineseFont18(), 0);
        lv_obj_set_style_text_color(lvBinNameLabels[i], CLR_TEXT, 0);
        lv_obj_align(lvBinNameLabels[i], LV_ALIGN_LEFT_MID, 4, 0);

        // 重量（右侧）
        lvBinWeightLabels[i] = lv_label_create(lvBinCards[i]);
        lv_label_set_text(lvBinWeightLabels[i], "0.0 kg");
        lv_obj_set_style_text_font(lvBinWeightLabels[i], GetFont18(), 0);
        lv_obj_set_style_text_color(lvBinWeightLabels[i], CLR_TEXT, 0);
        lv_obj_align(lvBinWeightLabels[i], LV_ALIGN_CENTER, 8, 0);

        // 在线圆点（右侧，加大到 16×16，在线浅绿 / 离线红色）
        lvBinOnlineDots[i] = lv_obj_create(lvBinCards[i]);
        lv_obj_remove_style_all(lvBinOnlineDots[i]);
        lv_obj_set_size(lvBinOnlineDots[i], 16, 16);
        lv_obj_set_style_bg_color(lvBinOnlineDots[i], lv_color_hex(0x66FF66), 0);  // 默认浅绿
        lv_obj_set_style_bg_opa(lvBinOnlineDots[i], LV_OPA_COVER, 0);
        lv_obj_set_style_radius(lvBinOnlineDots[i], 8, 0);
        lv_obj_align(lvBinOnlineDots[i], LV_ALIGN_RIGHT_MID, -6, 0);
    }

    // ====== 底部区域 (68px): 消息 + 按钮 ======
    lv_obj_t* bottomArea = lv_obj_create(root);
    lv_obj_remove_style_all(bottomArea);
    lv_obj_set_size(bottomArea, 480, 70);
    lv_obj_set_style_pad_all(bottomArea, 4, 0);
    lv_obj_set_flex_flow(bottomArea, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(bottomArea, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // 消息区
    lvMessageLabel = lv_label_create(bottomArea);
    lv_label_set_text(lvMessageLabel, "");
    lv_obj_set_style_text_font(lvMessageLabel, GetChineseFont(), 0);
    lv_obj_set_style_text_color(lvMessageLabel, CLR_TEXT, 0);
    lv_obj_set_style_text_align(lvMessageLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_size(lvMessageLabel, 470, 24);

    // 按钮区
    lv_obj_t* btnRow = lv_obj_create(bottomArea);
    lv_obj_remove_style_all(btnRow);
    lv_obj_set_size(btnRow, 472, 44);
    lv_obj_set_flex_flow(btnRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btnRow, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // 上料完毕按钮
    lvBtnFeed = lv_btn_create(btnRow);
    lv_obj_set_size(lvBtnFeed, 230, 44);
    lv_obj_set_style_radius(lvBtnFeed, 6, 0);
    lv_obj_set_style_bg_color(lvBtnFeed, CLR_BORDER, 0);
    lv_obj_set_style_bg_opa(lvBtnFeed, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(lvBtnFeed, 0, 0);
    lv_obj_set_style_shadow_width(lvBtnFeed, 0, 0);
    lv_obj_add_event_cb(lvBtnFeed, LvglGui_BtnFeedEvent, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* feedLbl = lv_label_create(lvBtnFeed);
    lv_label_set_text(feedLbl, "上料完毕");
    lv_obj_set_style_text_font(feedLbl, GetChineseFont18(), 0);
    lv_obj_set_style_text_color(feedLbl, CLR_TEXT_LT, 0);
    lv_obj_center(feedLbl);

    // 下料完毕按钮
    lvBtnDischarge = lv_btn_create(btnRow);
    lv_obj_set_size(lvBtnDischarge, 230, 44);
    lv_obj_set_style_radius(lvBtnDischarge, 6, 0);
    lv_obj_set_style_bg_color(lvBtnDischarge, CLR_BTN, 0);
    lv_obj_set_style_bg_opa(lvBtnDischarge, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(lvBtnDischarge, 0, 0);
    lv_obj_set_style_shadow_width(lvBtnDischarge, 0, 0);
    lv_obj_add_event_cb(lvBtnDischarge, LvglGui_BtnDischargeEvent, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* dischLbl = lv_label_create(lvBtnDischarge);
    lv_label_set_text(dischLbl, "下料完毕");
    lv_obj_set_style_text_font(dischLbl, GetChineseFont18(), 0);
    lv_obj_set_style_text_color(dischLbl, CLR_TEXT_LT, 0);
    lv_obj_center(dischLbl);

    LvglGui_UpdateBusinessLabels();
}

// ============== 字体探针 ==============
inline void LvglGui_LogFontProbe() {
    lv_font_glyph_dsc_t dsc = {};
    const uint32_t uiChars[] = {
        0x4ED3,  // 仓
        0x6EA7, 0x9633, 0x4E8C, 0x671F, 0x79F0, 0x91CD, 0x7CFB, 0x7EDF,
        0x5F53, 0x524D, 0x5E93, 0x5B58, 0x8FD0, 0x884C, 0x7A33, 0x5B9A,
        0x5B9E, 0x65F6, 0x91C7, 0x6837, 0x4ECE, 0x673A, 0x8FDE, 0x63A5,
        0x4E0A, 0x6599, 0x5B8C, 0x6BD5, 0x4E0B, 0x7B49, 0x5F85, 0x64CD,
        0x4F5C, 0x6B63, 0x5E38, 0x6D4B, 0x4F20, 0x611F, 0x5668, 0x6545,
        0x969C, 0x5931, 0x8D25, 0x79F0, 0x7A33, 0x5B9A,
        0x7F16, 0x8F91, 0x4FEE, 0x6539, 0x4E3A, 0x8F93, 0x5165
    };
    bool allOk = true;
    for (size_t i = 0; i < sizeof(uiChars) / sizeof(uiChars[0]); ++i) {
        bool ok = lv_font_get_glyph_dsc(&lv_font_chinese_14, &dsc, uiChars[i], 0);
        if (!ok) {
            allOk = false;
            Serial.printf("[LVGL] Font missing: U+%04lX\n", (unsigned long)uiChars[i]);
        }
    }
    Serial.printf("[LVGL] Font probe: all=%d total=%u\n",
                  allOk ? 1 : 0, (unsigned)(sizeof(uiChars) / sizeof(uiChars[0])));
}

// ============== 初始化 LVGL ==============
inline void LvglGui_InitLvgl() {
    lv_init();
    lv_disp_draw_buf_init(&lvDrawBuf, lvDrawBuffer, nullptr, tft.width() * 10);

    lv_disp_drv_init(&lvDispDrv);
    lvDispDrv.hor_res = tft.width();
    lvDispDrv.ver_res = tft.height();
    lvDispDrv.flush_cb = LvglGui_FlushCb;
    lvDispDrv.draw_buf = &lvDrawBuf;
    lv_disp_drv_register(&lvDispDrv);

    lv_indev_drv_init(&lvIndevDrv);
    lvIndevDrv.type = LV_INDEV_TYPE_POINTER;
    lvIndevDrv.read_cb = LvglGui_TouchReadCb;
    lv_indev_drv_register(&lvIndevDrv);

    lvglLastTickMs = millis();
    LvglGui_CreateUi();
    LvglGui_LogFontProbe();
    Serial.printf("[LVGL] 初始化完成: %dx%d\n", tft.width(), tft.height());
}

// ============== 初始化入口 ==============
inline void LvglGui_Init() {
    Serial.println("\n[LVGL] === 溧阳二期称重系统 4仓 ===");
    pinMode(TFT_BL_PIN, OUTPUT);
    digitalWrite(TFT_BL_PIN, HIGH);
    Serial.printf("[LVGL] 背光开启: GPIO%d\n", TFT_BL_PIN);

    tft.init();
    delay(120);
    tft.setRotation(TFT_ROTATION);
    tft.setTouch(touchCalData);
    tftReady = true;
    touchReady = true;
    pinMode(TOUCH_IRQ_PIN, INPUT);

    Serial.printf("[LVGL] TFT就绪, 旋转=%d, 分辨率=%dx%d\n",
                  TFT_ROTATION, tft.width(), tft.height());
    LvglGui_InitLvgl();
}

// ============== 主循环 ==============
inline void LvglGui_Loop() {
    if (!tftReady || !touchReady) return;

    uint32_t now = millis();
    if (now - lvglLastTickMs >= 5) {
        lv_tick_inc(now - lvglLastTickMs);
        lvglLastTickMs = now;
    }

    lv_timer_handler();

    static uint32_t lastBizUpdate = 0;
    if (now - lastBizUpdate >= 200) {
        lastBizUpdate = now;
        LvglGui_UpdateBusinessLabels();
    }

    // 触摸日志（非阻塞）
    uint16_t tx = 0, ty = 0;
    bool pressed = tft.getTouch(&tx, &ty, 300);
    if (pressed) {
        touchLastPressed = true;
        if (millis() - touchLastLogMs >= 200) {
            touchLastLogMs = millis();
            Serial.printf("[Touch] x=%u y=%u\n",
                          LvglGui_FlipTouchAxis(tx, tft.width()),
                          LvglGui_FlipTouchAxis(ty, tft.height()));
        }
    } else if (touchLastPressed) {
        touchLastPressed = false;
    }
}

#endif // LVGL_GUI_H

