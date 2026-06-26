#include <Arduino.h>
#include "Config.h"
#include "CloudReport.h"
#include "EspnowMesh.h"
#include "Display.h"

static uint32_t lastLvTick = 0;
static bool selfTestDone = false;

// ESP-NOW 收到某仓状态变化 → 通知Display更新对应灯
void onBinStateChange(uint8_t binId, bool online, float binWeight, float currentWeight) {
    Display_OnBinStateChange(binId, online, binWeight, currentWeight);
}

// 开发者模式选仓号后 → 同步到ESP-NOW
void Display_OnDevBinSelected(uint8_t binId);

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println();
    Serial.println("[A33E] Unified firmware - UI + ESP-NOW mesh");
    Serial.printf("[A33E] localId=%u gateway=%s fixedGateway=%u\n",
                  DEFAULT_LOCAL_ID,
                  DEFAULT_GATEWAY_FLAG ? "true" : "false",
                  FIXED_GATEWAY_ID);
    Serial.printf("[A33E] DTU placeholder Serial1 TX=%d RX=%d baud=%lu\n",
                  DTU_TX_PIN,
                  DTU_RX_PIN,
                  static_cast<unsigned long>(DTU_BAUD_DEFAULT));

    // 1. 先初始化显示
    Display_Init();

    // 2. 初始化 ESP-NOW 组网
    EspnowMesh_SetMyBin(DEFAULT_LOCAL_ID);
    EspnowMesh_SetGateway(DEFAULT_GATEWAY_FLAG);
    EspnowMesh_SetStateCallback(onBinStateChange);
    if (!EspnowMesh_Init()) {
        Serial.println("[A33E] ESP-NOW初始化失败, 仅UI运行");
    }

    lastLvTick = millis();
}

void loop() {
    const uint32_t now = millis();
    const uint32_t elapsed = now - lastLvTick;
    if (elapsed >= UI_TICK_MS) {
        lv_tick_inc(elapsed);
        lastLvTick = now;
    }

    // 启动后跑一次业务逻辑自测
    if (!selfTestDone && now > 1500) {
        Display_SelfTest();
        selfTestDone = true;
    }

    // 显示循环
    Display_Loop();

    // ESP-NOW 组网循环: 广播心跳 + 离线检测
    EspnowMesh_Loop(Display_GetBinWeight(), Display_GetCurrentWeight());

    delay(2);
}
