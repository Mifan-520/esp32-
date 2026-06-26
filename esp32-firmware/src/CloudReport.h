#pragma once

#include <Arduino.h>
#include "Config.h"

// M1 placeholder only. Later phases will call this to send one JSON line to DTU.
inline void CloudReport_Init(uint32_t baud = DTU_BAUD_DEFAULT) {
    Serial1.begin(baud, SERIAL_8N1, DTU_RX_PIN, DTU_TX_PIN);
}

inline void CloudReport_SendEventJson(const char* jsonLine) {
    if (!jsonLine || !jsonLine[0]) return;
    Serial1.println(jsonLine);
}

