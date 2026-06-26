#include <Arduino.h>
#include "Config.h"
#include "CloudReport.h"
#include "Display.h"

static uint32_t lastLvTick = 0;

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println();
    Serial.println("[A33E] Unified firmware M1 UI simulation");
    Serial.printf("[A33E] localId=%u gateway=%s fixedGateway=%u\n",
                  DEFAULT_LOCAL_ID,
                  DEFAULT_GATEWAY_FLAG ? "true" : "false",
                  FIXED_GATEWAY_ID);
    Serial.printf("[A33E] DTU placeholder Serial1 TX=%d RX=%d baud=%lu\n",
                  DTU_TX_PIN,
                  DTU_RX_PIN,
                  static_cast<unsigned long>(DTU_BAUD_DEFAULT));

    Display_Init();
    lastLvTick = millis();
}

void loop() {
    const uint32_t now = millis();
    const uint32_t elapsed = now - lastLvTick;
    if (elapsed >= UI_TICK_MS) {
        lv_tick_inc(elapsed);
        lastLvTick = now;
    }

    Display_Loop();
    delay(2);
}

