/*
  Config.h - ESP32 从机配置
  4寸 ST7796 配置（与主机一致）
*/

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ============== 从机版本 ==============
#define SLAVE_VERSION 4

// ============== 从机 ID (0-3，每台物理从机不同) ==============
#define SLAVE_ID 0

// ============== 显示引脚 (4寸 ST7796 配置) ==============
#define TFT_CS_PIN    15
#define TFT_DC_PIN    2
#define TFT_RST_PIN   -1
#define TFT_MOSI_PIN  13
#define TFT_MISO_PIN  12
#define TFT_SCLK_PIN  14
#define TFT_BL_PIN    27

// ============== 屏幕参数 (480x320，rotation=1 后横屏) ==============
#define SCREEN_WIDTH   480
#define SCREEN_HEIGHT  320
#define SCREEN_ROTATION 1

// ============== 主题颜色 ==============
#define COLOR_BORDER          0x661E2B
#define COLOR_BG              0xBFBFBF
#define COLOR_PANEL           0xD9D9D9
#define COLOR_TEXT_DARK       0x1A1A1A
#define COLOR_TEXT_LIGHT      0xFFFFFF

// ============== 仓配置 ==============
#define BIN_COUNT 4

// ============== BLE 配置 ==============
#define BLE_DEVICE_NAME_PREFIX "WeighingSlave_"  // 前缀 + MAC地址
#define BLE_MASTER_DEVICE_NAME "WeighingMaster"
#define BLE_MAX_CONNECTION_RETRIES 3
#define BLE_SCAN_INTERVAL_MS 1000
#define BLE_PAYLOAD_SIZE 21  // float(4) + 4*float(16) + uint8(1)

// ============== BLE GATT UUID (与主机一致) ==============
#define SERVICE_UUID           "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHAR_WEIGHT_DATA_UUID  "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CHAR_HEARTBEAT_UUID    "a3c8a500-0b6d-4f8a-b9e2-8e9c1f5d2a7b"

// ============== 心跳配置 ==============
#define HEARTBEAT_INTERVAL_MS 2000    // 发送心跳间隔
#define HEARTBEAT_TIMEOUT_MS  10000   // 主机超时判断

// ============== 显示配置 ==============
#define DISPLAY_UPDATE_INTERVAL_MS 500  // 显示刷新间隔

// ============== 初始化函数 ==============
inline void Config_Init() {
    pinMode(TFT_BL_PIN, OUTPUT);
    digitalWrite(TFT_BL_PIN, HIGH);
    Serial.printf("[Config] Slave v%d (ST7796) initialized, SLAVE_ID=%d\n", SLAVE_VERSION, SLAVE_ID);
}

#endif // CONFIG_H
