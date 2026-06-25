/*
  main.cpp - 称重系统从机主程序
  ESP32 DevKit + BLE
*/

#include <Arduino.h>
#include "Config.h"
#include "BleSlave.h"
#include "Display.h"

void setup() {
    Serial.begin(115200);
    Serial.println("\n================================");
    Serial.println("  ESP32 称重系统从机启动");
    Serial.println("================================\n");
    
    // 初始化配置
    Config_Init();
    
    // 初始化 BLE 从机
    BleSlave_Init();
    
    // 初始化显示
    Display_Init();
    
    Serial.println("\n[Main] 初始化完成，开始运行...\n");
}

void loop() {
    // BLE 从机通信
    BleSlave_Loop();
    
    // 显示更新
    Display_Loop();
    
    // 短暂延时让出CPU
    delay(1);
}
