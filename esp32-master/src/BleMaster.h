/*
  BleMaster.h - BLE 主机模块
  支持最多4个从机连接，21字节 GATT 通知，per-slave 心跳检测

  协议：
    Master→Slave Notify (21B): [float currentWeight(4) + float binWeights[4](16) + uint8_t onlineFlags(1)]
    Slave→Master Write  (2B):  [uint8_t slaveId(1) + uint8_t heartbeatValue(1)]
*/

#ifndef BLE_MASTER_H
#define BLE_MASTER_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLECharacteristic.h>
#include <BLEAdvertising.h>
#include <BLE2902.h>
#include "Config.h"

// ============== 从机连接信息结构 ==============
struct SlaveInfo {
    bool connected = false;
    bool online = false;           // 10秒心跳在线标记
    int8_t rssi = -127;
    uint32_t lastHeartbeat = 0;
    uint32_t connectTime = 0;
    uint16_t connId = 0xFFFF;
    char address[18] = {0};
};

// ============== 全局变量 ==============
inline BLEServer* bleServer = nullptr;
inline BLEService* bleService = nullptr;
inline BLECharacteristic* weightCharacteristic = nullptr;
inline BLECharacteristic* heartbeatCharacteristic = nullptr;
inline BLEAdvertising* bleAdvertising = nullptr;

inline SlaveInfo slaves[BLE_MAX_SLAVES];
inline uint8_t connectedSlaveCount = 0;
inline bool bleInitialized = false;
inline uint32_t bleAdvRestartAt = 0;

// ============== 回调类声明 ==============
class ServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer);
    void onConnect(BLEServer* pServer, esp_ble_gatts_cb_param_t* param);
    void onDisconnect(BLEServer* pServer);
    void onDisconnect(BLEServer* pServer, esp_ble_gatts_cb_param_t* param);
};

class HeartbeatCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pCharacteristic);
};

// ============== 初始化 ==============
inline void BleMaster_Init() {
    BLEDevice::init(BLE_DEVICE_NAME);

    bleServer = BLEDevice::createServer();
    bleServer->setCallbacks(new ServerCallbacks());

    bleService = bleServer->createService(SERVICE_UUID);

    // 重量数据特征 (Notify, 21 bytes)
    weightCharacteristic = bleService->createCharacteristic(
        CHAR_WEIGHT_DATA_UUID,
        BLECharacteristic::PROPERTY_NOTIFY
    );
    weightCharacteristic->addDescriptor(new BLE2902());

    // 心跳特征 (Write, 2 bytes: slaveId + heartbeatValue)
    heartbeatCharacteristic = bleService->createCharacteristic(
        CHAR_HEARTBEAT_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );
    heartbeatCharacteristic->setCallbacks(new HeartbeatCallbacks());

    bleService->start();

    bleAdvertising = BLEDevice::getAdvertising();
    bleAdvertising->addServiceUUID(SERVICE_UUID);
    bleAdvertising->setScanResponse(true);
    bleAdvertising->setMinPreferred(0x06);
    bleAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();

    bleInitialized = true;

    Serial.printf("[BleMaster] BLE initialized, name: %s\n", BLE_DEVICE_NAME);
    Serial.printf("[BleMaster] Payload: 21 bytes (float + 4*float + uint8)\n");
}

// ============== 循环处理 ==============
inline void BleMaster_Loop() {
    if (!bleInitialized) return;

    static uint32_t lastHeartbeatCheck = 0;

    if (bleAdvRestartAt > 0 && millis() >= bleAdvRestartAt) {
        bleAdvRestartAt = 0;
        if (connectedSlaveCount < BLE_MAX_SLAVES) {
            BLEDevice::startAdvertising();
            Serial.printf("[BleMaster] Restarting advertising (delayed)\n");
        }
    }

    // 每秒检查心跳超时
    if (millis() - lastHeartbeatCheck >= 1000) {
        lastHeartbeatCheck = millis();

        for (int i = 0; i < BLE_MAX_SLAVES; i++) {
            if (!slaves[i].connected) {
                slaves[i].online = false;
                continue;
            }
            if (slaves[i].lastHeartbeat == 0) {
                // 尚未收到首个心跳，不算在线
                slaves[i].online = false;
                continue;
            }
            if (millis() - slaves[i].lastHeartbeat > BLE_OFFLINE_TIMEOUT_MS) {
                if (slaves[i].online) {
                    Serial.printf("[BleMaster] Slave %d went offline (timeout)\n", i);
                }
                slaves[i].online = false;
            } else {
                slaves[i].online = true;
            }
        }
    }
}

// ============== 发送重量数据 (21字节) ==============
inline void BleMaster_SendWeight(float currentWeight, const float binWeights[BIN_COUNT]) {
    if (!bleInitialized || !weightCharacteristic) return;

    // 打包: [currentWeight(4) + binWeights[0..3](16) + onlineFlags(1)] = 21 bytes
    uint8_t data[BLE_PAYLOAD_SIZE];
    memcpy(data, &currentWeight, 4);
    memcpy(data + 4, binWeights, 16);

    // onlineFlags: bit0=slave0, bit1=slave1, ...
    uint8_t onlineFlags = 0;
    for (int i = 0; i < BLE_MAX_SLAVES; i++) {
        if (slaves[i].online) {
            onlineFlags |= (1 << i);
        }
    }
    data[20] = onlineFlags;

    weightCharacteristic->setValue(data, BLE_PAYLOAD_SIZE);
    weightCharacteristic->notify();

    static uint32_t lastPrint = 0;
    if (millis() - lastPrint >= 2000) {
        lastPrint = millis();
        Serial.printf("[BleMaster] Notify: cur=%.2f, bins=[%.2f,%.2f,%.2f,%.2f], online=0x%02X, conn=%d\n",
                      currentWeight, binWeights[0], binWeights[1], binWeights[2], binWeights[3],
                      onlineFlags, connectedSlaveCount);
    }
}

// ============== 获取连接/在线状态 ==============
inline uint8_t BleMaster_GetConnectedCount() {
    return connectedSlaveCount;
}

inline bool BleMaster_IsSlaveConnected(uint8_t index) {
    if (index >= BLE_MAX_SLAVES) return false;
    return slaves[index].connected;
}

inline bool BleMaster_IsSlaveOnline(uint8_t index) {
    if (index >= BLE_MAX_SLAVES) return false;
    return slaves[index].online;
}

inline int BleMaster_FindSlaveSlotByConnId(uint16_t connId) {
    for (int i = 0; i < BLE_MAX_SLAVES; i++) {
        if (slaves[i].connected && slaves[i].connId == connId) {
            return i;
        }
    }
    return -1;
}

// 重算连接计数（替代 ++/-- 避免飘掉）
inline void BleMaster_RecountConnected() {
    connectedSlaveCount = 0;
    for (int i = 0; i < BLE_MAX_SLAVES; i++) {
        if (slaves[i].connected) {
            connectedSlaveCount++;
        }
    }
}

inline int BleMaster_FindSlaveSlotByAddress(const char* addr) {
    if (!addr || !addr[0]) return -1;
    for (int i = 0; i < BLE_MAX_SLAVES; i++) {
        if (strcmp(slaves[i].address, addr) == 0) {
            return i;
        }
    }
    return -1;
}

inline void BleMaster_ClearSlaveSlot(int index, const char* reason) {
    if (index < 0 || index >= BLE_MAX_SLAVES) return;
    bool wasConnected = slaves[index].connected;
    uint16_t oldConnId = slaves[index].connId;
    char oldAddr[18] = {0};
    strncpy(oldAddr, slaves[index].address, sizeof(oldAddr) - 1);

    slaves[index].connected = false;
    slaves[index].online = false;
    slaves[index].rssi = -127;
    slaves[index].lastHeartbeat = 0;
    slaves[index].connectTime = 0;
    slaves[index].connId = 0xFFFF;
    slaves[index].address[0] = '\0';

    if (wasConnected) {
        BleMaster_RecountConnected();
    }

    Serial.printf("[BleMaster] Slave %d disconnected (%s, conn_id=%u, addr=%s)\n",
                  index,
                  reason ? reason : "unknown",
                  oldConnId,
                  oldAddr[0] ? oldAddr : "?");
}

// ============== 服务器回调实现 ==============
inline void ServerCallbacks::onConnect(BLEServer* pServer) {
    (void)pServer;
}

inline void ServerCallbacks::onConnect(BLEServer* pServer, esp_ble_gatts_cb_param_t* param) {
    char incomingAddr[18] = {0};
    sprintf(incomingAddr, "%02X:%02X:%02X:%02X:%02X:%02X",
            param->connect.remote_bda[0], param->connect.remote_bda[1],
            param->connect.remote_bda[2], param->connect.remote_bda[3],
            param->connect.remote_bda[4], param->connect.remote_bda[5]);

    int existingConnSlot = BleMaster_FindSlaveSlotByConnId(param->connect.conn_id);
    if (existingConnSlot >= 0) {
        BleMaster_ClearSlaveSlot(existingConnSlot, "replace-conn-id");
    }

    int existingAddrSlot = BleMaster_FindSlaveSlotByAddress(incomingAddr);
    if (existingAddrSlot >= 0) {
        BleMaster_ClearSlaveSlot(existingAddrSlot, "replace-address");
    }

    for (int i = 0; i < BLE_MAX_SLAVES; i++) {
        if (!slaves[i].connected) {
            slaves[i].connected = true;
            slaves[i].online = false;          // 只代表链路连上，等首个心跳才在线
            slaves[i].connectTime = millis();
            slaves[i].lastHeartbeat = 0;       // 尚未收到首个心跳
            slaves[i].connId = param->connect.conn_id;
            strncpy(slaves[i].address, incomingAddr, sizeof(slaves[i].address) - 1);
            BleMaster_RecountConnected();

            Serial.printf("[BleMaster] Slave %d connected: %s (conn_id=%u)\n",
                          i, slaves[i].address, slaves[i].connId);
            break;
        }
    }

    bleAdvRestartAt = 0;
    if (connectedSlaveCount < BLE_MAX_SLAVES) {
        BLEDevice::startAdvertising();
    }
}

inline void ServerCallbacks::onDisconnect(BLEServer* pServer) {
    (void)pServer;
    // 该库会先调用 onDisconnect(this)，再调用 onDisconnect(this, param)
    // 无参版本只保留兼容占位，避免和带 conn_id 的版本重复清理。
}

inline void ServerCallbacks::onDisconnect(BLEServer* pServer, esp_ble_gatts_cb_param_t* param) {
    (void)pServer;
    int slot = BleMaster_FindSlaveSlotByConnId(param->disconnect.conn_id);
    if (slot >= 0) {
        BleMaster_ClearSlaveSlot(slot, "conn-id");
    } else {
        for (int i = BLE_MAX_SLAVES - 1; i >= 0; i--) {
            if (slaves[i].connected) {
                BleMaster_ClearSlaveSlot(i, "fallback-missing-conn-id");
                break;
            }
        }
    }
    bleAdvRestartAt = millis() + 150;
}

// ============== 心跳回调实现 ==============
inline void HeartbeatCallbacks::onWrite(BLECharacteristic* pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    if (value.length() >= 2) {
        // 解析: [slaveId(1) + heartbeatValue(1)]
        uint8_t slaveId = value[0];
        uint8_t hbValue = value[1];

        if (slaveId < BLE_MAX_SLAVES && slaves[slaveId].connected) {
            slaves[slaveId].lastHeartbeat = millis();
            slaves[slaveId].online = true;
        }

        static uint32_t lastPrint = 0;
        if (millis() - lastPrint >= 5000) {
            lastPrint = millis();
            Serial.printf("[BleMaster] Heartbeat: slave=%d, val=0x%02X\n", slaveId, hbValue);
        }
    }
}

// ============== 打印状态 ==============
inline void BleMaster_PrintStatus() {
    Serial.printf("[BleMaster] Status: %d/%d slaves connected\n",
                  connectedSlaveCount, BLE_MAX_SLAVES);
    for (int i = 0; i < BLE_MAX_SLAVES; i++) {
        Serial.printf("  Slave %d: conn=%d, online=%d, %s\n",
                      i, slaves[i].connected, slaves[i].online, slaves[i].address);
    }
}

#endif // BLE_MASTER_H
