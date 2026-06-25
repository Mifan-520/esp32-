/*
  main.cpp - 主机主程序
  溧阳二期主机：重量采样 + BLE 广播 + LVGL 界面
*/

#include <Arduino.h>
#include "Config.h"
#if ENABLE_RS485_INPUT
#include "ModbusRtu.h"
#endif

#include "WeightSensor.h"
#include "WeightLogic.h"
#include "BleMaster.h"
#include "LvglGui.h"

void setup() {
    Serial.begin(115200);
    delay(200);

    Serial.println("\n================================");
    Serial.println("  溧阳二期主机启动");
    Serial.println("================================\n");

    Config_Init();

#if ENABLE_RS485_INPUT
    ModbusRtu_Init();
#endif



    WeightSensor_Init();
    BleMaster_Init();
    WeightLogic_Init();
    LvglGui_Init();

    Serial.println("[Main] 初始化完成，开始运行...");
}

void loop() {
#if ENABLE_RS485_INPUT
    ModbusRtu_Loop();
#endif



    WeightSensor_Loop();
    BleMaster_Loop();
    WeightLogic_Loop();
    LvglGui_Loop();
}


