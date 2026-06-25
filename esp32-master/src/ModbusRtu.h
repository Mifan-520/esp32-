/*
  ModbusRtu.h - RS485 + CJMCU-752 Modbus RTU 通信
  非阻塞实现，使用 ModbusMaster 库
*/

#ifndef MODBUS_RTU_H
#define MODBUS_RTU_H

#include <Arduino.h>
#include <ModbusMaster.h>
#include "Config.h"

// ============== 状态定义 ==============
enum ModbusState {
    MODBUS_IDLE = 0,
    MODBUS_READING = 1,
    MODBUS_ERROR = 2
};

// ============== 全局变量 ==============
inline ModbusMaster modbusNode;
inline ModbusState modbusState = MODBUS_IDLE;
inline uint16_t weightRaw = 0;              // 原始重量值
inline uint16_t weightFiltered = 0;         // 滤波后重量
inline uint8_t lastError = 0;               // 上次错误码
inline uint32_t lastReadTime = 0;           // 上次读取时间
inline uint32_t lastSuccessTime = 0;        // 上次成功时间
inline bool modbusInitialized = false;

// ============== 错误计数 ==============
inline uint32_t successCount = 0;
inline uint32_t errorCount = 0;

// ============== 初始化 ==============
inline void ModbusRtu_Init() {
    // TTL转RS485模块无DE引脚，自动方向切换，无需GPIO控制
    // 使用 Serial1 (参考三元完整版项目)
    Serial1.begin(MODBUS_BAUD_RATE, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);
    
    // 初始化 ModbusMaster
    modbusNode.begin(MODBUS_SLAVE_ID, Serial1);
    
    modbusInitialized = true;
    modbusState = MODBUS_IDLE;
    
    Serial.printf("[ModbusRtu] Initialized: SlaveID=%d, Baud=%d, TX=%d, RX=%d\n", 
                  MODBUS_SLAVE_ID, MODBUS_BAUD_RATE, RS485_TX_PIN, RS485_RX_PIN);
}

// ============== 非阻塞读取循环 ==============
inline void ModbusRtu_Loop() {
    if (!modbusInitialized) return;
    
    // 间隔读取
    if (millis() - lastReadTime < MODBUS_READ_INTERVAL_MS) {
        return;
    }
    lastReadTime = millis();
    
    // 读取保持寄存器 0x0000 (重量值)
    uint8_t result = modbusNode.readHoldingRegisters(0x0000, 1);
    
    if (result == modbusNode.ku8MBSuccess) {
        weightRaw = modbusNode.getResponseBuffer(0);
        
        // 简单一阶滤波
        if (weightFiltered == 0) {
            weightFiltered = weightRaw;
        } else {
            weightFiltered = (weightFiltered * 7 + weightRaw) / 8;
        }
        
        lastSuccessTime = millis();
        successCount++;
        modbusState = MODBUS_IDLE;
        
        // 调试输出 (每10次成功打印一次)
        if (successCount % 10 == 0) {
            Serial.printf("[ModbusRtu] Weight: raw=%d, filtered=%d\n", 
                          weightRaw, weightFiltered);
        }
    } else {
        lastError = result;
        errorCount++;
        modbusState = MODBUS_ERROR;
        
        // 错误码解析
        const char* errorStr = "Unknown";
        switch (result) {
            case 0xE0: errorStr = "CRC Error"; break;
            case 0xE1: errorStr = "Response Len"; break;
            case 0xE2: errorStr = "Timeout"; break;
            case 0xE3: errorStr = "Invalid Data"; break;
        }
        
        // 每10次错误打印一次
        if (errorCount % 10 == 1) {
            Serial.printf("[ModbusRtu] Error 0x%02X: %s (total: %lu)\n", 
                          result, errorStr, errorCount);
        }
    }
}

// ============== 获取函数 ==============
inline uint16_t ModbusRtu_GetRawWeight() {
    return weightRaw;
}

inline uint16_t ModbusRtu_GetFilteredWeight() {
    return weightFiltered;
}

inline float ModbusRtu_GetWeightKg() {
    // CJMCU-752 输出通常是 0.1kg 或 0.01kg 单位，根据实际校准调整
    return weightFiltered / 10.0f;  // 假设 0.1kg 单位
}

inline bool ModbusRtu_IsConnected() {
    // 5秒内成功读取视为连接正常
    return (millis() - lastSuccessTime) < 5000;
}

inline uint32_t ModbusRtu_GetSuccessCount() {
    return successCount;
}

inline uint32_t ModbusRtu_GetErrorCount() {
    return errorCount;
}

// ============== 复位统计 ==============
inline void ModbusRtu_ResetStats() {
    successCount = 0;
    errorCount = 0;
}

// ============== 调试信息 ==============
inline void ModbusRtu_PrintStats() {
    Serial.printf("[ModbusRtu] Stats: OK=%lu, ERR=%lu, LastWeight=%.2fkg, Connected=%d\n",
                  successCount, errorCount, ModbusRtu_GetWeightKg(), ModbusRtu_IsConnected());
}

#endif // MODBUS_RTU_H
