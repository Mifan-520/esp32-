/*
  WeightSensor.h - 重量传感器数据处理
  滑动平均滤波、稳定检测、单位转换
*/

#ifndef WEIGHT_SENSOR_H
#define WEIGHT_SENSOR_H

#include <Arduino.h>
#include "Config.h"
#if ENABLE_RS485_INPUT
#include "ModbusRtu.h"
#endif

// ============== 滤波窗口大小 ==============
#define FILTER_WINDOW_SIZE 10

// ============== 状态定义 ==============
enum WeightStatus {
    WEIGHT_UNSTABLE = 0,
    WEIGHT_STABLE = 1,
    WEIGHT_ERROR = 2
};

// ============== 全局变量 ==============
inline float weightBuffer[FILTER_WINDOW_SIZE] = {0};
inline uint8_t bufferIndex = 0;
inline bool bufferFull = false;

inline float currentWeight = 0.0f;      // 当前重量 (kg)
inline float filteredWeight = 0.0f;     // 滤波后重量 (kg)
inline float lastStableWeight = 0.0f;   // 上次稳定重量
inline WeightStatus weightStatus = WEIGHT_UNSTABLE;

inline uint32_t stableStartTime = 0;    // 开始稳定时间
inline uint32_t lastChangeTime = 0;     // 上次变化时间

inline float weightOffset = 0.0f;       // 去皮偏移量
inline float calibrationFactor = 1.0f;  // 校准系数

// ============== 初始化 ==============
inline void WeightSensor_Init() {
    // 初始化滤波缓冲区
    for (int i = 0; i < FILTER_WINDOW_SIZE; i++) {
        weightBuffer[i] = 0.0f;
    }
    bufferIndex = 0;
    bufferFull = false;
    
    currentWeight = 0.0f;
    filteredWeight = 0.0f;
    lastStableWeight = 0.0f;
    weightStatus = WEIGHT_UNSTABLE;
    weightOffset = 0.0f;
    calibrationFactor = 0.1f;  // CJMCU-752 默认 0.1kg 单位
    
    Serial.printf("[WeightSensor] Initialized, window=%d\n", FILTER_WINDOW_SIZE);
}

// ============== 添加数据到缓冲区 ==============
inline void addToBuffer(float value) {
    weightBuffer[bufferIndex] = value;
    bufferIndex = (bufferIndex + 1) % FILTER_WINDOW_SIZE;
    if (bufferIndex == 0) bufferFull = true;
}

// ============== 计算滑动平均 ==============
inline float calculateAverage() {
    float sum = 0.0f;
    uint8_t count = bufferFull ? FILTER_WINDOW_SIZE : bufferIndex;
    if (count == 0) return 0.0f;
    
    for (uint8_t i = 0; i < count; i++) {
        sum += weightBuffer[i];
    }
    return sum / count;
}

// ============== 计算标准差 (用于稳定性判断) ==============
inline float calculateStdDev(float avg) {
    uint8_t count = bufferFull ? FILTER_WINDOW_SIZE : bufferIndex;
    if (count < 2) return 0.0f;
    
    float sumSq = 0.0f;
    for (uint8_t i = 0; i < count; i++) {
        float diff = weightBuffer[i] - avg;
        sumSq += diff * diff;
    }
    return sqrt(sumSq / count);
}

// ============== 主循环 ==============
inline void WeightSensor_Loop() {
    static uint32_t lastUpdate = 0;
    
    // 100ms 更新一次
    if (millis() - lastUpdate < 100) return;
    lastUpdate = millis();

#if ENABLE_WEIGHT_SIMULATION
    static float simWeight = 12.5f;
    static bool simUp = true;
    static uint32_t lastStep = 0;

    if (millis() - lastStep >= 800) {
        lastStep = millis();
        if (simUp) {
            simWeight += 1.8f;
            if (simWeight >= 36.0f) simUp = false;
        } else {
            simWeight -= 1.4f;
            if (simWeight <= 8.0f) simUp = true;
        }
    }

    currentWeight = simWeight;
    filteredWeight = simWeight;
    lastStableWeight = simWeight;
    weightStatus = WEIGHT_STABLE;

    static uint32_t lastPrint = 0;
    if (millis() - lastPrint >= 3000) {
        lastPrint = millis();
        Serial.printf("[WeightSensor] Simulated: %.2f kg\n", simWeight);
    }
    return;
#endif
    
    #if ENABLE_RS485_INPUT
    // 检查 Modbus 是否连接
    if (!ModbusRtu_IsConnected()) {
        weightStatus = WEIGHT_ERROR;
        return;
    }
    
    // 获取原始重量并转换
    uint16_t raw = ModbusRtu_GetFilteredWeight();
    currentWeight = (raw * calibrationFactor) - weightOffset;
    
    // 添加到滤波缓冲区
    addToBuffer(currentWeight);
    
    // 计算滤波后重量
    filteredWeight = calculateAverage();
    
    // 稳定性检测
    float stdDev = calculateStdDev(filteredWeight);
    bool isStableNow = (stdDev < (WEIGHT_STABLE_THRESHOLD * calibrationFactor / 10.0f));
    
    if (isStableNow) {
        if (weightStatus != WEIGHT_STABLE) {
            // 开始稳定
            if (stableStartTime == 0) {
                stableStartTime = millis();
            } else if (millis() - stableStartTime >= WEIGHT_STABLE_TIME_MS) {
                // 稳定时间达到阈值
                weightStatus = WEIGHT_STABLE;
                lastStableWeight = filteredWeight;
                Serial.printf("[WeightSensor] Stable: %.2f kg\n", filteredWeight);
            }
        }
    } else {
        // 不稳定
        weightStatus = WEIGHT_UNSTABLE;
        stableStartTime = 0;
        lastChangeTime = millis();
    }
#else
    // RS485未启用时，无实际数据来源
    weightStatus = WEIGHT_ERROR;
#endif
}

// ============== 获取函数 ==============
inline float WeightSensor_GetCurrentWeight() {
    return currentWeight;
}

inline float WeightSensor_GetFilteredWeight() {
    return filteredWeight;
}

inline float WeightSensor_GetLastStableWeight() {
    return lastStableWeight;
}

inline WeightStatus WeightSensor_GetStatus() {
    return weightStatus;
}

inline bool WeightSensor_IsStable() {
    return weightStatus == WEIGHT_STABLE;
}

inline bool WeightSensor_IsError() {
    return weightStatus == WEIGHT_ERROR;
}

// ============== 去皮操作 ==============
inline void WeightSensor_Tare() {
    weightOffset = filteredWeight;
    Serial.printf("[WeightSensor] Tare: offset=%.2f kg\n", weightOffset);
}

// ============== 校准操作 ==============
inline void WeightSensor_SetCalibration(float knownWeight) {
#if ENABLE_RS485_INPUT
    if (knownWeight > 0 && filteredWeight > 0) {
        calibrationFactor = knownWeight / ModbusRtu_GetFilteredWeight();
        Serial.printf("[WeightSensor] Calibration: factor=%.4f\n", calibrationFactor);
    }
#else
    Serial.println("[WeightSensor] Calibration not available (RS485 disabled)");
#endif
}

// ============== 重置 ==============
inline void WeightSensor_Reset() {
    weightOffset = 0.0f;
    stableStartTime = 0;
    weightStatus = WEIGHT_UNSTABLE;
    Serial.printf("[WeightSensor] Reset\n");
}

// ============== 调试输出 ==============
inline void WeightSensor_PrintStatus() {
    const char* statusStr = (weightStatus == WEIGHT_STABLE) ? "STABLE" :
                            (weightStatus == WEIGHT_UNSTABLE) ? "UNSTABLE" : "ERROR";
    Serial.printf("[WeightSensor] Cur=%.2f, Filt=%.2f, Status=%s\n",
                  currentWeight, filteredWeight, statusStr);
}

#endif // WEIGHT_SENSOR_H
