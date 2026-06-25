/*
  BleSlave.h - BLE 从机模块（稳定性修复版）
  自动连接主机，接收21字节重量数据，发送2字节心跳带从机ID

  修复内容：
    - 复用同一个 BLEClient，不再每次断连 delete/create
    - 异步扫描，不再阻塞主循环
    - 全局 lastHeartbeatSentAt 替代 static 局部变量
    - notifyRetryAt 从 400ms 改为 1200ms
    - 统一重连入口，不在回调里 delete 对象
    - 重连时清空所有旧时间戳

  协议：
    Master→Slave Notify (21B): [float currentWeight(4) + float binWeights[4](16) + uint8_t onlineFlags(1)]
    Slave→Master Write  (2B):  [uint8_t slaveId(1) + uint8_t heartbeatValue(1)]
*/

#ifndef BLE_SLAVE_H
#define BLE_SLAVE_H

#include <Arduino.h>
#include <Preferences.h>
#include <BLEDevice.h>
#include <BLEClient.h>
#include <BLEAdvertisedDevice.h>
#include "Config.h"

// ============== 状态定义 ==============
enum BleSlaveState {
    SLAVE_DISCONNECTED = 0,
    SLAVE_SCANNING = 1,
    SLAVE_CONNECTING = 2,
    SLAVE_CONNECTED = 3
};

// ============== 全局变量 ==============
inline BLEClient* bleClient = nullptr;
inline BLERemoteCharacteristic* weightChar = nullptr;
inline BLERemoteCharacteristic* heartbeatChar = nullptr;
inline BLEAdvertisedDevice* targetMasterDevice = nullptr;

inline BleSlaveState slaveState = SLAVE_DISCONNECTED;
inline bool bleInitialized = false;
inline bool doScan = false;
inline bool doConnect = false;
inline bool scanInProgress = false;
inline uint32_t nextScanAllowedAt = 0;
inline uint8_t reconnectRetryCount = 0;
inline uint32_t lastConnectSuccessAt = 0;
inline uint32_t notifyRetryAt = 0;
inline bool notifyRetryDone = false;

// 心跳发送时间戳（全局，可在重连时清零）
inline uint32_t lastHeartbeatSentAt = 0;

// NVS 持久化
inline Preferences slaveNvs;
inline const char* SLAVE_NVS_NAMESPACE = "slavecache";
inline const char* SLAVE_NVS_KEY_BINS = "bins";
inline const char* SLAVE_NVS_KEY_CUR = "cur";
inline const char* SLAVE_NVS_KEY_FLAGS = "flags";
inline const char* SLAVE_NVS_KEY_VALID = "valid";

// 接收到的重量数据 (21字节协议)
inline float receivedCurrentWeight = 0.0f;
inline float receivedBinWeights[BIN_COUNT] = {0};
inline uint8_t receivedOnlineFlags = 0;
inline uint32_t lastDataReceived = 0;

// 本机仓重量 (只取 binWeights[SLAVE_ID])
inline float receivedMyBinWeight = 0.0f;
inline bool receivedWeightValid = false;

// 主机地址和名称
inline char masterAddress[18] = {0};

inline void BleSlave_SaveCachedWeights() {
    slaveNvs.begin(SLAVE_NVS_NAMESPACE, false);
    slaveNvs.putBytes(SLAVE_NVS_KEY_BINS, receivedBinWeights, sizeof(receivedBinWeights));
    slaveNvs.putFloat(SLAVE_NVS_KEY_CUR, receivedCurrentWeight);
    slaveNvs.putUChar(SLAVE_NVS_KEY_FLAGS, receivedOnlineFlags);
    slaveNvs.putBool(SLAVE_NVS_KEY_VALID, true);
    slaveNvs.end();
    receivedWeightValid = true;
}

inline void BleSlave_LoadCachedWeights() {
    slaveNvs.begin(SLAVE_NVS_NAMESPACE, false);
    receivedWeightValid = slaveNvs.getBool(SLAVE_NVS_KEY_VALID, false);
    if (receivedWeightValid) {
        size_t readLen = slaveNvs.getBytes(SLAVE_NVS_KEY_BINS, receivedBinWeights, sizeof(receivedBinWeights));
        if (readLen != sizeof(receivedBinWeights)) {
            for (int i = 0; i < BIN_COUNT; i++) {
                receivedBinWeights[i] = 0.0f;
            }
            receivedWeightValid = false;
        } else {
            receivedCurrentWeight = slaveNvs.getFloat(SLAVE_NVS_KEY_CUR, 0.0f);
            receivedOnlineFlags = slaveNvs.getUChar(SLAVE_NVS_KEY_FLAGS, 0);
            if (SLAVE_ID < BIN_COUNT) {
                receivedMyBinWeight = receivedBinWeights[SLAVE_ID];
            }
            Serial.printf("[BleSlave] NVS restored: bin%d=%.2f kg\n", SLAVE_ID + 1, receivedMyBinWeight);
        }
    }
    slaveNvs.end();
}

// ============== 回调类声明 ==============
class AdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice);
};

class ClientCallbacks : public BLEClientCallbacks {
    void onConnect(BLEClient* pClient);
    void onDisconnect(BLEClient* pClient);
};

inline AdvertisedDeviceCallbacks advertisedDeviceCallbacks;

// ============== 扫描完成回调 ==============
inline void BleSlave_OnScanComplete(BLEScanResults results) {
    (void)results;
    scanInProgress = false;

    if (slaveState == SLAVE_SCANNING && !doConnect && targetMasterDevice == nullptr) {
        slaveState = SLAVE_DISCONNECTED;
        nextScanAllowedAt = millis() + 500;
        Serial.printf("[BleSlave] Scan complete, no master found, retrying...\n");
    }
}

inline uint32_t BleSlave_GetRetryDelayMs() {
    // 固定2秒重试，保证断电重连快速
    return 2000UL;
}

// ============== 确保 client 存在（复用，不删除重建） ==============
inline void BleSlave_EnsureClient() {
    if (bleClient == nullptr) {
        bleClient = BLEDevice::createClient();
        bleClient->setClientCallbacks(new ClientCallbacks());
        Serial.printf("[BleSlave] BLE client created\n");
    }
    weightChar = nullptr;
    heartbeatChar = nullptr;
}

inline void BleSlave_DeleteTargetDevice() {
    if (targetMasterDevice != nullptr) {
        delete targetMasterDevice;
        targetMasterDevice = nullptr;
    }
}

// ============== 统一重连入口：清状态 + 延后再扫 ==============
inline void BleSlave_ScheduleReconnect(const char* reason) {
    uint32_t delayMs = BleSlave_GetRetryDelayMs();
    if (reason) {
        Serial.printf("[BleSlave] %s, retry in %lu ms\n", reason, delayMs);
    }
    // 安全断开
    if (bleClient != nullptr && bleClient->isConnected()) {
        bleClient->disconnect();
    }
    slaveState = SLAVE_DISCONNECTED;
    doConnect = false;
    doScan = true;
    nextScanAllowedAt = millis() + delayMs;   // >= 2秒后再重扫

    weightChar = nullptr;
    heartbeatChar = nullptr;
    lastDataReceived = 0;
    lastConnectSuccessAt = 0;
    notifyRetryAt = 0;
    notifyRetryDone = false;
    lastHeartbeatSentAt = 0;    // 重连时清发送节拍
    scanInProgress = false;
    BleSlave_DeleteTargetDevice();
}

// ============== 初始化 ==============
inline void BleSlave_Init() {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_BT);
    char deviceName[32];
    snprintf(deviceName, sizeof(deviceName), "%s%02X%02X",
             BLE_DEVICE_NAME_PREFIX, mac[4], mac[5]);

    BLEDevice::init(deviceName);

    BleSlave_EnsureClient();

    bleInitialized = true;
    doScan = true;
    doConnect = false;
    reconnectRetryCount = 0;
    nextScanAllowedAt = 0;
    lastConnectSuccessAt = 0;
    notifyRetryAt = 0;
    notifyRetryDone = false;
    lastHeartbeatSentAt = 0;

    BleSlave_LoadCachedWeights();

    Serial.printf("[BleSlave] BLE initialized, name: %s, SLAVE_ID=%d\n", deviceName, SLAVE_ID);
}

// ============== 重量数据回调 (21字节) ==============
inline void WeightNotifyCallback(BLERemoteCharacteristic* pChar, uint8_t* data, size_t length, bool isNotify) {
    if (length >= BLE_PAYLOAD_SIZE) {
        // 解析: [currentWeight(4) + binWeights[0..3](16) + onlineFlags(1)]
        memcpy(&receivedCurrentWeight, data, 4);
        memcpy(receivedBinWeights, data + 4, 16);
        receivedOnlineFlags = data[20];
        lastDataReceived = millis();

        // 提取本仓重量
        if (SLAVE_ID < BIN_COUNT) {
            receivedMyBinWeight = receivedBinWeights[SLAVE_ID];
        }

        BleSlave_SaveCachedWeights();

        static uint32_t lastPrint = 0;
        if (millis() - lastPrint >= 2000) {
            lastPrint = millis();
            Serial.printf("[BleSlave] Cur=%.2f, MyBin=%d:%.2f, Online=0x%02X\n",
                          receivedCurrentWeight, SLAVE_ID, receivedMyBinWeight, receivedOnlineFlags);
        }
    }
}

inline bool BleSlave_ConnectToMaster() {
    if (targetMasterDevice == nullptr) {
        Serial.printf("[BleSlave] No target master device\n");
        slaveState = SLAVE_DISCONNECTED;
        doScan = true;
        nextScanAllowedAt = millis() + 500;
        return false;
    }

    BleSlave_EnsureClient();  // 确保 client 存在

    Serial.printf("[BleSlave] Connecting to %s\n", targetMasterDevice->getAddress().toString().c_str());

    if (!bleClient->connect(targetMasterDevice)) {
        reconnectRetryCount++;
        BleSlave_ScheduleReconnect("Connect failed");
        return false;
    }

    Serial.printf("[BleSlave] Connected to master\n");

    BLERemoteService* service = bleClient->getService(SERVICE_UUID);
    if (service == nullptr) {
        Serial.printf("[BleSlave] Service not found\n");
        bleClient->disconnect();
        reconnectRetryCount++;
        BleSlave_ScheduleReconnect("Service not found");
        return false;
    }

    weightChar = service->getCharacteristic(CHAR_WEIGHT_DATA_UUID);
    if (weightChar == nullptr) {
        Serial.printf("[BleSlave] Weight char not found\n");
        bleClient->disconnect();
        reconnectRetryCount++;
        BleSlave_ScheduleReconnect("Weight char not found");
        return false;
    }

    if (weightChar->canNotify()) {
        weightChar->registerForNotify(WeightNotifyCallback, true, true);
        Serial.printf("[BleSlave] Notify registered (21-byte payload)\n");
    }

    heartbeatChar = service->getCharacteristic(CHAR_HEARTBEAT_UUID);
    if (heartbeatChar == nullptr) {
        Serial.printf("[BleSlave] Heartbeat char not found\n");
    }

    slaveState = SLAVE_CONNECTED;
    doScan = false;
    doConnect = false;
    reconnectRetryCount = 0;
    nextScanAllowedAt = 0;
    lastConnectSuccessAt = millis();
    notifyRetryAt = lastConnectSuccessAt + 1200;  // 给栈和CCCD注册留足时间
    notifyRetryDone = false;
    lastHeartbeatSentAt = 0;
    lastDataReceived = 0;
    BleSlave_DeleteTargetDevice();
    return true;
}

// ============== 扫描回调 ==============
inline void AdvertisedDeviceCallbacks::onResult(BLEAdvertisedDevice advertisedDevice) {
    bool serviceMatch = advertisedDevice.haveServiceUUID() &&
                        advertisedDevice.isAdvertisingService(BLEUUID(SERVICE_UUID));
    bool nameMatch = advertisedDevice.haveName() &&
                     advertisedDevice.getName() == BLE_MASTER_DEVICE_NAME;

    if (serviceMatch || nameMatch) {

        Serial.printf("[BleSlave] Found master: %s (service=%d, name=%d)\n",
                      advertisedDevice.getName().c_str(), serviceMatch ? 1 : 0, nameMatch ? 1 : 0);
        strcpy(masterAddress, advertisedDevice.getAddress().toString().c_str());

        advertisedDevice.getScan()->stop();
        scanInProgress = false;
        doScan = false;
        slaveState = SLAVE_CONNECTING;
        nextScanAllowedAt = 0;

        BleSlave_DeleteTargetDevice();
        targetMasterDevice = new BLEAdvertisedDevice(advertisedDevice);
        doConnect = true;
    }
}

// ============== 客户端回调 ==============
inline void ClientCallbacks::onConnect(BLEClient* pClient) {
    Serial.printf("[BleSlave] onConnect callback\n");
}

inline void ClientCallbacks::onDisconnect(BLEClient* pClient) {
    Serial.printf("[BleSlave] onDisconnect callback\n");
    reconnectRetryCount = 0;
    BleSlave_ScheduleReconnect("Disconnected");
}

// ============== 循环处理 ==============
inline void BleSlave_Loop() {
    if (!bleInitialized) return;

    static uint32_t lastScan = 0;

    // 发送心跳 (2字节: slaveId + heartbeatValue)
    if (slaveState == SLAVE_CONNECTED && heartbeatChar != nullptr) {
        if (millis() - lastHeartbeatSentAt >= HEARTBEAT_INTERVAL_MS) {
            lastHeartbeatSentAt = millis();

            uint8_t hbData[2] = {(uint8_t)SLAVE_ID, 0x02};  // [slaveId, ack]
            heartbeatChar->writeValue(hbData, 2);

            static uint32_t lastPrint = 0;
            if (millis() - lastPrint >= 5000) {
                lastPrint = millis();
                Serial.printf("[BleSlave] Heartbeat sent: id=%d\n", SLAVE_ID);
            }
        }

        // 1200ms 后重注册 notify（首次可能没生效）
        if (lastDataReceived == 0 && !notifyRetryDone && millis() >= notifyRetryAt) {
            notifyRetryDone = true;
            if (weightChar != nullptr && weightChar->canNotify()) {
                Serial.printf("[BleSlave] Re-register notify after warmup\n");
                weightChar->registerForNotify(WeightNotifyCallback, true, true);
            }
        }

        // 2500ms 仍无数据则重连
        if (lastDataReceived == 0 && notifyRetryDone && millis() - lastConnectSuccessAt >= 2500) {
            reconnectRetryCount++;
            BleSlave_ScheduleReconnect("No notify after connect");
            return;
        }

        // 10秒数据超时则重连
        if (lastDataReceived > 0 && millis() - lastDataReceived >= HEARTBEAT_TIMEOUT_MS) {
            reconnectRetryCount = 0;
            BleSlave_ScheduleReconnect("Notify timeout");
            return;
        }
    }

    // 异步扫描
    if (doScan && slaveState == SLAVE_DISCONNECTED && !scanInProgress) {
        if (millis() < nextScanAllowedAt) {
            return;
        }
        if (millis() - lastScan >= BLE_SCAN_INTERVAL_MS) {
            lastScan = millis();
            slaveState = SLAVE_SCANNING;

            Serial.printf("[BleSlave] Starting async scan...\n");

            BLEScan* scan = BLEDevice::getScan();
            scan->setAdvertisedDeviceCallbacks(&advertisedDeviceCallbacks, false, true);
            scan->setInterval(120);
            scan->setWindow(100);
            scan->setActiveScan(true);
            scanInProgress = true;
            scan->start(5, BleSlave_OnScanComplete, false);
            return;
        }
    }

    if (doConnect && slaveState == SLAVE_CONNECTING) {
        BleSlave_ConnectToMaster();
    }
}

// ============== 获取数据 ==============
inline float BleSlave_GetCurrentWeight() {
    return receivedCurrentWeight;
}

inline float BleSlave_GetMyBinWeight() {
    return receivedMyBinWeight;
}

inline bool BleSlave_HasWeightSnapshot() {
    return receivedWeightValid;
}

inline bool BleSlave_IsConnected() {
    return slaveState == SLAVE_CONNECTED;
}

inline BleSlaveState BleSlave_GetState() {
    return slaveState;
}

inline bool BleSlave_HasRecentData() {
    return (millis() - lastDataReceived) < HEARTBEAT_TIMEOUT_MS;
}

#endif // BLE_SLAVE_H
