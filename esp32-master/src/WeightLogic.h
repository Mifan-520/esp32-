/*
  WeightLogic.h - 重量业务逻辑
  4仓重量管理，上料/下料操作，操作历史
*/

#ifndef WEIGHT_LOGIC_H
#define WEIGHT_LOGIC_H

#include <Arduino.h>
#include <Preferences.h>
#include "Config.h"
#include "WeightSensor.h"
#include "BleMaster.h"

// ============== NVS 持久化 ==============
inline Preferences nvsStorage;
inline const char* NVS_NAMESPACE = "binweight";

// ============== 4仓重量 ==============
inline float binWeights[BIN_COUNT] = {0};

// ============== 选中仓 ==============
inline int8_t selectedBin = -1;  // -1=未选中, 0-3=仓号

// ============== 操作历史 (用于撤销) ==============
#define HISTORY_SIZE 40  // 每仓10条
struct WeightOperation {
    uint8_t binIndex;   // 操作的仓号
    bool isLoading;     // true=上料, false=下料
    float weight;
    uint32_t timestamp;
};

inline WeightOperation operationHistory[HISTORY_SIZE];
inline uint8_t historyIndex = 0;
inline uint8_t historyCount = 0;

// ============== 状态 ==============
inline bool lastOperationSuccess = false;
inline char lastOperationMsg[64] = {0};

// ============== NVS 保存 ==============
inline void WeightLogic_SaveToNvs() {
    nvsStorage.begin(NVS_NAMESPACE, false);
    for (int i = 0; i < BIN_COUNT; i++) {
        char key[8];
        snprintf(key, sizeof(key), "bin%d", i);
        nvsStorage.putFloat(key, binWeights[i]);
    }
    nvsStorage.end();
}

// ============== NVS 恢复 ==============
inline void WeightLogic_LoadFromNvs() {
    nvsStorage.begin(NVS_NAMESPACE, false);
    bool hasData = false;
    for (int i = 0; i < BIN_COUNT; i++) {
        char key[8];
        snprintf(key, sizeof(key), "bin%d", i);
        if (nvsStorage.isKey(key)) {
            binWeights[i] = nvsStorage.getFloat(key, 0.0f);
            hasData = true;
        }
    }
    nvsStorage.end();
    if (hasData) {
        Serial.printf("[WeightLogic] NVS restored: %.2f / %.2f / %.2f / %.2f kg\n",
                      binWeights[0], binWeights[1], binWeights[2], binWeights[3]);
    }
}

// ============== 初始化 ==============
inline void WeightLogic_Init() {
    for (int i = 0; i < BIN_COUNT; i++) {
        binWeights[i] = 0.0f;
    }
    selectedBin = -1;
    historyIndex = 0;
    historyCount = 0;
    lastOperationSuccess = false;
    strcpy(lastOperationMsg, "就绪");

    // 从 NVS 恢复仓重量
    WeightLogic_LoadFromNvs();

    Serial.printf("[WeightLogic] Initialized, %d bins\n", BIN_COUNT);
}

// ============== 选择仓 ==============
inline void WeightLogic_SelectBin(int8_t bin) {
    if (bin >= 0 && bin < BIN_COUNT) {
        selectedBin = bin;
        Serial.printf("[WeightLogic] Selected bin %d (%s)\n", bin, BIN_NAMES[bin]);
    } else {
        selectedBin = -1;
        Serial.printf("[WeightLogic] Deselected bin\n");
    }
}

inline int8_t WeightLogic_GetSelectedBin() {
    return selectedBin;
}

// ============== 获取仓重量 ==============
inline float WeightLogic_GetBinWeight(int8_t bin) {
    if (bin < 0 || bin >= BIN_COUNT) return 0.0f;
    return binWeights[bin];
}

inline const float* WeightLogic_GetAllBinWeights() {
    return binWeights;
}

inline bool WeightLogic_PrepareOperation(float& currentWeight, float& weight) {
    if (selectedBin < 0 || selectedBin >= BIN_COUNT) {
        lastOperationSuccess = false;
        strcpy(lastOperationMsg, "请先选择仓库");
        return false;
    }

    if (!WeightSensor_IsStable()) {
        lastOperationSuccess = false;
        strcpy(lastOperationMsg, "重量不稳定，请等待");
        return false;
    }

    currentWeight = WeightSensor_GetFilteredWeight();
    weight = currentWeight;
    if (weight <= 0.1f) {
        lastOperationSuccess = false;
        strcpy(lastOperationMsg, "重量太小");
        return false;
    }

    return true;
}

inline void WeightLogic_FinishOperation(bool isLoading, float currentWeight, float weight) {
    operationHistory[historyIndex].binIndex = selectedBin;
    operationHistory[historyIndex].isLoading = isLoading;
    operationHistory[historyIndex].weight = weight;
    operationHistory[historyIndex].timestamp = millis();
    historyIndex = (historyIndex + 1) % HISTORY_SIZE;
    if (historyCount < HISTORY_SIZE) historyCount++;

    WeightLogic_SaveToNvs();

    lastOperationSuccess = true;
    sprintf(lastOperationMsg, "%s %s %.2f kg",
            BIN_NAMES[selectedBin],
            isLoading ? "上料 +" : "下料 -",
            weight);

    Serial.printf("[WeightLogic] %s %s: %c%.2f kg, total=%.2f kg\n",
                  BIN_NAMES[selectedBin],
                  isLoading ? "Loading" : "Unloading",
                  isLoading ? '+' : '-',
                  weight,
                  binWeights[selectedBin]);

    BleMaster_SendWeight(currentWeight, binWeights);
}

// ============== 添加上料 ==============
inline bool WeightLogic_LoadingDone() {
    float currentWeight = 0.0f;
    float weight = 0.0f;
    if (!WeightLogic_PrepareOperation(currentWeight, weight)) return false;

    binWeights[selectedBin] += weight;

    WeightLogic_FinishOperation(true, currentWeight, weight);
    return true;
}

// ============== 添加下料 ==============
inline bool WeightLogic_UnloadingDone() {
    float currentWeight = 0.0f;
    float weight = 0.0f;
    if (!WeightLogic_PrepareOperation(currentWeight, weight)) return false;

    if (weight > binWeights[selectedBin]) {
        lastOperationSuccess = false;
        strcpy(lastOperationMsg, "下料量大于库存");
        return false;
    }
    binWeights[selectedBin] -= weight;

    WeightLogic_FinishOperation(false, currentWeight, weight);
    return true;
}

// ============== 设置仓重量 (手动校准) ==============
inline void WeightLogic_SetBinWeight(int8_t bin, float weight) {
    if (bin >= 0 && bin < BIN_COUNT) {
        binWeights[bin] = weight;
        WeightLogic_SaveToNvs();
        Serial.printf("[WeightLogic] Set %s: %.2f kg\n", BIN_NAMES[bin], weight);
    }
}

// ============== 重置指定仓 ==============
inline void WeightLogic_ResetBin(int8_t bin) {
    if (bin >= 0 && bin < BIN_COUNT) {
        binWeights[bin] = 0.0f;
        WeightLogic_SaveToNvs();
        Serial.printf("[WeightLogic] Reset %s\n", BIN_NAMES[bin]);
    }
}

// ============== 撤销上次操作 ==============
inline bool WeightLogic_Undo() {
    if (historyCount == 0) {
        strcpy(lastOperationMsg, "没有可撤销的操作");
        return false;
    }

    uint8_t idx = (historyIndex + HISTORY_SIZE - 1) % HISTORY_SIZE;
    WeightOperation& op = operationHistory[idx];

    if (op.isLoading) {
        if (op.weight <= binWeights[op.binIndex]) {
            binWeights[op.binIndex] -= op.weight;
            sprintf(lastOperationMsg, "撤销 %s 上料 %.2f kg", BIN_NAMES[op.binIndex], op.weight);
        } else {
            strcpy(lastOperationMsg, "无法撤销，库存不足");
            return false;
        }
    } else {
        binWeights[op.binIndex] += op.weight;
        sprintf(lastOperationMsg, "撤销 %s 下料 %.2f kg", BIN_NAMES[op.binIndex], op.weight);
    }

    historyCount--;
    historyIndex = idx;

    WeightLogic_SaveToNvs();

    BleMaster_SendWeight(WeightSensor_GetFilteredWeight(), binWeights);
    return true;
}

// ============== 获取状态 ==============
inline bool WeightLogic_LastOperationSuccess() {
    return lastOperationSuccess;
}

inline const char* WeightLogic_GetLastMessage() {
    return lastOperationMsg;
}

// ============== 循环更新 ==============
inline void WeightLogic_Loop() {
    static uint32_t lastBroadcast = 0;
    if (millis() - lastBroadcast >= 1000) {
        lastBroadcast = millis();
        BleMaster_SendWeight(WeightSensor_GetFilteredWeight(), binWeights);
    }
}

#endif // WEIGHT_LOGIC_H
