# A33E 称重小车蓝牙方案 — 总计划 (plan1.md)

> 项目根目录: `C:\Users\伍米饭\Desktop\十日谈项目\esp32\溧阳二期esp32`
> 新工程: `esp32-firmware/`
> GitHub: `https://github.com/Mifan-520/esp32-.git`
> 分支: `main`
> 当前 HEAD: `5031c44`

---

## 物理链路

```
3个称重传感器 → 三合一接线盒 → XK3190-A33E称重表头(P6=13 Modbus-RTU)
  → RS485 → I6328A蓝牙模块(双模SPP/BLE,默认从机,点对点)
  → 蓝牙SPP → 6块中的1块ESP32(固定网关,先连上A33E的成为MASTER)
  → ESP-NOW(WiFi ch6 广播,无需路由器) → 其余5块ESP32
  → 有人DTU(串口透传) → 云端
```

---

## 硬件配置

| 硬件 | 参数 |
|---|---|
| 屏幕 | LCDWIKI 4寸 TFT ST7796S 320×480 (rotation=1 → 480×320横屏) |
| 触摸 | XPT2046 电阻触摸, 校准 `{275,3620,264,3532,1}` |
| ESP32 | ESP32-WROOM-32E (非S3/S2) |
| 烧录口 | COM3 / COM4 (CH340) |
| TFT引脚 | CS=15, DC=2, MOSI=13, MISO=12, SCLK=14, BL=27, RST=-1 |
| 触摸引脚 | TP_CS=33, TP_IRQ=36 |
| DTU预留 | TX=32, RX=25 |
| 编译环境 | VSCode + PlatformIO (espressif32@6.5.0, Arduino框架) |
| 固件分区 | huge_app.csv |

---

## 已完成 ✅

### M0: 老代码备份
- [x] 旧 `esp32-master/` / `esp32-slave/` 提交到 GitHub
- [x] 打标签 `v0-legacy`
- [x] commit `37a563b`
- **注意**: 旧代码是 CJMCU-752/BLE GATT 主从/4仓/模拟数据原型,与新方案链路不同

### M1: 统一固件工程 + UI
- [x] `esp32-firmware/` 工程骨架 (PlatformIO + TFT_eSPI@2.5.43 + LVGL@8.4.0)
- [x] 白底全屏 (0xFFFFFF)
- [x] **顶栏** (高60): logo(96×53原始尺寸) + 仓号文字 + 大圆灯(29px,在线绿/离线红) + 6仓状态灯(18px,左到右1-6,在线绿/离线灰) + 编辑按钮(右上)
- [x] **中间区分两栏**: 左1/4称重重量(montserrat_48 + 小kg) + 右3/4仓重大数字(130px + 小kg)
- [x] **底部**: 上料完毕 + 下料完毕 两个大按钮(230×58)
- [x] 中文仅使用字体已有字符(避免方框): 仓/在线/离线/上料完毕/下料完毕/编辑/确认/取消/修改完毕/仓重不足
- [x] kg 在数字下方缩小(montserrat_24),不再穿模
- [x] 上料/下料确认弹窗 → 仓重累加/扣减
- [x] **数字键盘编辑面板**: 左6个仓按钮 + 右4×3键盘(7 8 9/4 5 6/1 2 3/. 0 ⌫) + 输入框 + 确认按钮 → 参考老版 LvglGui
- [x] **离线警告框**: 左上角红框,只占左1/4区域,不挡仓重大数字,离线显在线隐
- [x] 触摸诊断日志 `[Touch]` + 事件日志 `[EVENT]`
- [x] 启动自测(SELFTEST ALL PASS): 上料累加/下料扣减/下料不足保护/报错态按钮禁用
- [x] COM3/COM4 双板编译SUCCESS + 烧录 + 启动无报错

### M3: ESP-NOW 组网
- [x] `EspnowMesh.h` — 6台ESP32 WiFi ch6 广播组网 (无需路由器)
- [x] 心跳包 (24B): magic(0xA33E0001) + binId + role + 体重 + 称重 + seq + MAC
- [x] 每2秒广播一次, 10秒没收到心跳→标记离线
- [x] MAC 过滤排除自己, 不同仓号自动发现
- [x] 回调通知 Display 更新6仓状态灯
- [x] 开发者模式(长按logo 4秒)选仓号联动 ESP-NOW (`EspnowMesh_OnBinChanged`)
- [x] COM3(仓1) ↔ COM4(仓2) ESP-NOW 收发心跳验证通过
- [x] 发送/接收日志 (每5秒打印)

### M4: 蓝牙 SPP 读 A33E (框架完成,等硬件)
- [x] `BleScaleClient.h` — BluetoothSerial 连接 I6328A, 透传 Modbus-RTU
- [x] Modbus CRC16 + 读寄存器8(float毛重) + 非阻塞 + 自动回退模拟
- [x] main.cpp 中已写但**注释**(等I6328A到货取消注释)
- [ ] **待硬件**: I6328A模块 + A33E表头到货后启动

### NVS 持久化
- [x] Preferences 库,命名空间 "binweight"
- [x] 6仓重量 + 本机仓号 保存/加载
- [x] 上料/下料/编辑/选仓号 时自动保存
- [x] 断电重启数据仍在 (已验证: COM3 NVS显示 `仓重=66.1` 是上次上料保存的)

### 已修复的UI问题
- [x] 中文缺字→方框 → 改用字体已有字符
- [x] logo穿模 → 仓1文字右移让位
- [x] kg穿模 → 放数字下方缩小
- [x] 消息提示栏 → 已移除,仅保留离线警告框
- [x] 6灯默认全灰 → ESP-NOW初始化后本机才变绿
- [x] 大圆灯 26→29px
- [x] 仓重数字 transform_zoom 导致消失 → 去掉zoom,用原始130px
- [x] DRAM溢出 → LVGL缓冲降至10行 + LV_MEM_SIZE 24KB + 蓝牙代码注释
- [x] 换仓号后ESP-NOW不同步 → EspnowMesh_OnBinChanged
- [x] 长按logo时间 → 4秒 (LV_INDEV_DEF_LONG_PRESS_TIME=4000)

---

## 未完成 ❌

### 待用户验证 (需肉眼确认)
- [ ] COM4 长按4秒选仓2后: COM4大圆灯变绿 + COM4第2盏灯变绿 + COM3第2盏灯也变绿
- [ ] 编辑面板数字键盘功能正常
- [ ] 上料/下料按钮触摸正常
- [ ] 离线警告框(长按logo后模拟离线)显示正常
- [ ] UI各元素位置/大小满意

### M5: 角色选举 + 故障接管
- [ ] 多台竞争连接蓝牙读A33E (先连上的成为MASTER)
- [ ] MASTER断连后另一台接管
- [ ] 仓号优先级(1>2>...>6) + 随机退避

### M6: 配置页
- [ ] 运行时设置仓号/网关/蓝牙参数
- [ ] NVS 保存配置
- [ ] 恢复默认值

### M7: 有人DTU上云
- [ ] DTU 串口透传 (TX=32, RX=25)
- [ ] 固定网关(仓1)转发
- [ ] JSON 格式上报状态变化 (哪个仓装载/卸载了多少公斤)
- [ ] 非阻塞发送

### 蓝牙硬件 (等到货)
- [ ] I6328A 模块到货 → 取消注释 BleScaleClient
- [ ] A33E 表头接线
- [ ] 蓝牙配对/连接 I6328A
- [ ] 真实 Modbus 读毛重验证

---

## 关键文件清单

| 文件 | 用途 |
|---|---|
| `esp32-firmware/platformio.ini` | PlatformIO 项目配置 (引脚/库/编译标志) |
| `esp32-firmware/src/main.cpp` | 主程序 (初始化/主循环/蓝牙注释位) |
| `esp32-firmware/src/Config.h` | 全局常量 (引脚/颜色/仓号/超时) |
| `esp32-firmware/src/Display.h` | UI 全部 (LVGL布局/事件/编辑面板/开发者模式) |
| `esp32-firmware/src/EspnowMesh.h` | ESP-NOW 组网 (心跳广播/离线检测/换仓) |
| `esp32-firmware/src/BleScaleClient.h` | 蓝牙读A33E (SPP+Modbus+CRC16,等硬件) |
| `esp32-firmware/src/CloudReport.h` | DTU云上报 (JSON占位,待M7) |
| `esp32-firmware/src/lv_font_chinese_14.c` | 中文字体 (仅含指定汉字,新增需重新生成) |
| `esp32-firmware/src/lv_font_numbers_130.c` | 130px数字字体 (0-9 + .) |
| `esp32-firmware/src/logo_roastek.c` | Logo 位图 (96×53, RGBA) |
| `esp32-firmware/include/lv_conf.h` | LVGL v8.4 配置 (字体/缓冲/长按时间/btnmatrix) |

---

## 中文字体可用字符 (lv_font_chinese_14)

```
溧阳二期称重系统当前重量库存运行中稳定实时采样从机连接上料完毕下等待操作正常测量传感器故障失败到已请先选择取消确认仓就绪太小大于撤销无法足不可有没的在线离线同步待数据已连接操作编辑修改为输入
+ ASCII 0x20-0x7E
```

**如需新字** → 用 LVGL font converter 重新生成:
```
--font simhei.ttf --size 24 --bpp 4 -r 0x20-0x7E --symbols <新字列表> --format lvgl
```

---

## 常用操作

```bash
# 编译
cd esp32-firmware && pio run -e esp32dev

# 烧录 (先确保PlatformIO CLI可用)
pio run -e esp32dev -t upload --upload-port COM3

# 串口监视
pio device monitor -p COM3 -b 115200

# 提交
cd .. && git add esp32-firmware/ && git commit -m "..." && git push
```

---

## 下个对话要处理的最优先事项

1. COM4 长按4秒选仓2,验证 ESP-NOW 跨仓心跳 + 灯变绿
2. 编辑面板数字键盘功能联调
3. 等 I6328A + A33E 到货后启动真实称重
