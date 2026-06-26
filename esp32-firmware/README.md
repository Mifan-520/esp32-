# A33E 蓝牙称重统一固件 - M1

这是第一阶段的统一固件工程，只验证 4 寸 ESP32 集成屏的 UI、触摸和模拟重量显示。

## 当前范围

- 同一固件可烧录到 COM3 / COM4 两块 CH340 ESP32 集成屏。
- 默认 `localId=1`，`gwFlag=true`，显示为“仓1 网关”。
- 当前不接 A33E、不连 I6328A、不启用 ESP-NOW、不处理真实 DTU。
- DTU 后续只通过串口上发 JSON 行，预留串口为 `Serial1 TX=GPIO32, RX=GPIO25`。

## 屏幕引脚

- ST7796: CS15, DC2, MOSI13, MISO12, SCLK14, BL27, RST=-1
- 电阻触摸: TP_CS33, TP_IRQ36

## VSCode PlatformIO

如果命令行没有 `pio`，请在 VSCode 中用 PlatformIO 打开本目录并初始化/编译/烧录。

