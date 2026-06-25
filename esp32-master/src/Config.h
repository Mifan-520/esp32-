/*
  Config.h - ESP32 主机配置
  当前状态：显示模块已重置，等待按 LCDWiki 4寸页面重新接入
*/

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ============== 显示引脚 ==============
#define TFT_CS_PIN      15
#define TFT_DC_PIN      2
#define TFT_RST_PIN     -1
#define TFT_MOSI_PIN    13
#define TFT_MISO_PIN    12
#define TFT_SCLK_PIN    14
#define TFT_BL_PIN      27

#define TOUCH_CS_PIN    33
#define TOUCH_IRQ_PIN   36

#define SCREEN_WIDTH    320
#define SCREEN_HEIGHT   480
#define SCREEN_ROTATION 0

// ============== 主题颜色 ==============
#define COLOR_BORDER          0x661E2B
#define COLOR_BG              0x999999
#define COLOR_PANEL           0xD9D9D9
#define COLOR_BUTTON_NORMAL   0x777777
#define COLOR_BUTTON_HIGHLIGHT 0x404040
#define COLOR_TEXT_DARK       0x1A1A1A
#define COLOR_TEXT_LIGHT      0xFFFFFF

// ============== 仓配置 ==============
#define BIN_COUNT 4
#define BLE_PAYLOAD_SIZE 21  // float(4) + 4*float(16) + uint8(1)
static const char* BIN_NAMES[BIN_COUNT] = {"仓1", "仓2", "仓3", "仓4"};

// ============== RS485/Modbus 引脚 ==============
// TTL转RS485模块接 I2C 母口（正接）: 红=3.3V, 黑(TX)=IO32, 黄(RX)=IO25, 绿=GND
// 模块无DE引脚，自动方向切换
#define RS485_TX_PIN   32  // Serial1 TX → 黑线(IO32)
#define RS485_RX_PIN   25  // Serial1 RX → 黄线(IO25)

// ============== 当前轮次功能开关 ==============
#define ENABLE_RS485_INPUT 0
#define ENABLE_RS485_TEST  1   // RS485最小TX闪烁测试（与INPUT互斥）
#define ENABLE_WEIGHT_SIMULATION 1

// RS485 INPUT 与 TEST 互斥保护
#if (ENABLE_RS485_INPUT == 1 && ENABLE_RS485_TEST == 1)
#error "ENABLE_RS485_INPUT and ENABLE_RS485_TEST cannot both be 1"
#endif

// ============== CJMCU-752 重量变送器 ==============
#define MODBUS_SLAVE_ID    1
#define MODBUS_BAUD_RATE   9600
#define MODBUS_READ_INTERVAL_MS 100

// ============== BLE 配置 ==============
#define BLE_DEVICE_NAME    "WeighingMaster"
#define BLE_MAX_SLAVES     4
#define BLE_HEARTBEAT_INTERVAL_MS 2000
#define BLE_OFFLINE_TIMEOUT_MS    10000

// ============== BLE GATT UUID ==============
#define SERVICE_UUID           "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHAR_WEIGHT_DATA_UUID  "beb5483e-36e1-4688-b7f5-ea07361b26a8"  // Notify
#define CHAR_HEARTBEAT_UUID    "a3c8a500-0b6d-4f8a-b9e2-8e9c1f5d2a7b"  // Write
#define CHAR_RSSI_UUID         "c5d9b200-1e7f-4c9a-d0f3-9f0a2e6c3b8d"  // Read

// ============== 称重配置 ==============
#define WEIGHT_FILTER_WINDOW    10     // 滑动平均窗口大小
#define WEIGHT_STABLE_THRESHOLD 5      // 稳定阈值 (单位: 0.1kg)
#define WEIGHT_STABLE_TIME_MS   1000   // 稳定时间 (ms)
#define WEIGHT_DECIMAL_PLACES   1      // 小数位数

inline void Config_Init() {
    Serial.printf("[Config] 引脚配置: BL=%d, TOUCH_CS=%d, RS485 TX=%d RX=%d\n",
                  TFT_BL_PIN, TOUCH_CS_PIN, RS485_TX_PIN, RS485_RX_PIN);
}

#endif // CONFIG_H
