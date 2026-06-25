每次对话要调用(skills/self-improving-agent)

## self-improving-agent 工作流

### 1. 何时触发
- 命令、编译、烧录、脚本、外部 API / 工具调用失败
- 米饭明确纠正：如“不是”“错了”“实际是……”
- 发现当前知识过时、判断错误、实现路线不对
- 发现更稳妥、更简洁、可复用的做法
- 米饭提出当前仓库还不具备的新能力/新流程

### 2. 任务开始前检查
- 如果项目根目录已存在 `.learnings/`，开始重要任务前先快速查看：
  - `.learnings/LEARNINGS.md`
  - `.learnings/ERRORS.md`
  - `.learnings/FEATURE_REQUESTS.md`
- 如果 `.learnings/` 不存在，**不要自动创建**；本工作区禁止擅自新建文件或目录，需先征得米饭同意。

### 3. 记录规则
- 纠正、经验、知识缺口、最佳实践 → 记到 `.learnings/LEARNINGS.md`
- 命令失败、工具异常、集成报错 → 记到 `.learnings/ERRORS.md`
- 新能力、新自动化诉求、新工具需求 → 记到 `.learnings/FEATURE_REQUESTS.md`
- 记录时只写**简洁摘要 + 关键上下文 + 建议动作**，不要粘贴整段无关日志
- 严禁记录 secrets、token、私钥、环境变量原文、完整敏感配置

### 4. 推荐条目最小结构
- `ID`：`LRN/ERR/FEAT-YYYYMMDD-XXX`
- `Logged`
- `Priority`
- `Status`：`pending | in_progress | resolved | promoted | wont_fix`
- `Summary`
- `Details / Context`
- `Suggested Action`
- `Metadata`（相关文件、来源、是否可复现等）

### 5. 解决后的处理
- 问题修复后，把对应条目状态更新为 `resolved`
- 如果结论已变成稳定工作流、项目共识或长期约束，再提升到项目记忆：
  - `AGENTS.md`：工作流、自动化规则、代理执行规范
  - 其余项目事实或约束，可按需要写入项目已有长期说明位置

### 6. 本项目中的执行要求
- 发生错误或被米饭纠正时，不只修当前问题，还要判断是否值得沉淀为长期规则
- 对重复出现的坑，优先提炼成 `AGENTS.md` 可执行规则，避免下次再犯
- 任何需要新增 `.learnings/*` 文件的动作，都要先遵守本工作区“禁止自动创建文件”的规则

# 溧阳二期 ESP32 当前已知信息

## 1. 当前目标
- ~~本轮 **不接 485**~~ → RS485 TX 测试已完成
- ~~当前进行：**RS485 TX 灯闪烁验证**~~ → **已完成**
- 下一步：**RS485 读取重量变送器数据**
- 已完成：
  - ✅ 主机 4 寸屏界面（ST7796 + XPT2046 + LVGL）
  - ✅ 触摸点击
  - ✅ 主从 BLE 连通
  - ✅ 主机向从机发送测试值
  - ✅ RS485 TX 灯闪烁验证（测试模块已清理）
- 待完成：
  - RS485 读取重量变送器数据

## 2. 正确引脚配置（来自 LCDWiki 4 寸网页真值）
> ⚠️ **关键发现：TFT 和触摸屏共用 SPI 总线！**

### TFT 显示屏 SPI 引脚（VSPI）
| 功能 | GPIO | 说明 |
|------|------|------|
| TFT_MISO | IO12 | VSPI MISO（与触摸共用） |
| TFT_MOSI | IO13 | VSPI MOSI（与触摸共用） |
| TFT_SCLK | IO14 | VSPI SCLK（与触摸共用） |
| TFT_CS | IO15 | 片选 |
| TFT_DC | IO2 | 数据/命令 |
| TFT_RST | EN | 与 ESP32 共用复位 |
| TFT_BL | **IO27** | 背光控制（高电平点亮） |

### 触摸屏 XPT2046 引脚（共用 SPI）
| 功能 | GPIO | 说明 |
|------|------|------|
| TP_SCK | **IO14** | 与 TFT 共用 SPI 时钟 |
| TP_DIN | **IO13** | 与 TFT 共用 SPI MOSI |
| TP_DOUT | **IO12** | 与 TFT 共用 SPI MISO |
| TP_CS | IO33 | 触摸片选（独立） |
| TP_IRQ | IO36 | 触摸中断 |

### RS485 TTL转RS485模块（接 I2C 母口）
> ⚠️ **模块4P线为正接，不是交叉接！模块标注的TX/RX就是接ESP32的TX/RX**

| 线色 | 模块标注 | I2C母口 | ESP32 GPIO | UART角色 |
|------|---------|---------|------------|----------|
| 红 | VCC | 3.3V | — | 供电 |
| 黑 | TX | IO32 | **GPIO32** | **Serial1 TX** |
| 黄 | RX | IO25 | **GPIO25** | **Serial1 RX** |
| 绿 | GND | GND | — | 地 |

- 模块无DE/RE引脚，**自动方向切换**
- 使用 **Serial1**（不是Serial2），参考三元完整版项目
- 之前Config.h中GPIO4(DE)/GPIO17(TX)/GPIO22(RX)全部冲突（板载LED/音频），已废弃

### 板载引脚冲突清单（不可用）
| GPIO | 板载用途 | 说明 |
|------|---------|------|
| IO4 | 音频使能 | 原RS485_DE，冲突 |
| IO17 | 蓝LED | 原RS485_TX，冲突 |
| IO22 | 红LED | 原RS485_RX，冲突 |
| IO23 | SPI母口 MOSI | 无VCC，不能供电 |
| IO19 | SPI母口 MISO | 无VCC，不能供电 |
| IO18 | SPI母口 SCLK | 无VCC，不能供电 |
| IO21 | SPI母口 SS | 无VCC，不能供电 |

### 注意事项
- **TFT 和触摸共用 IO12/13/14**，只是片选不同（TFT_CS=15, TP_CS=33）
- 背光是 **IO27**，不是之前假设的 IO21
- TFT_RST = EN，与 ESP32 共用复位，代码中应设为 -1
- SPI 频率：TFT 推荐 40MHz，触摸必须 2.5MHz

## 3. 当前工程配置状态
- **屏幕驱动**: ST7796S（320×480，4寸屏，来自 LCDWiki 网页确认）
- **触摸驱动**: XPT2046（共用 SPI）
- **编译状态**: 主机和从机工程均成功编译
- **驱动更正**: 之前错误配置为 ILI9488，现已更正为 ST7796
- **显示模块状态**: 已接入（ST7796 + XPT2046 + LVGL）
- **RS485**: 引脚已更新为 TX=IO32/RX=IO25（I2C母口正接），使用Serial1，无DE控制
- **参考项目**: 三元完整版（`D:\SSSSSSSSSSSSCCCCCCCCCCCCCMMMMMMMMMMMM\ESP32\vscode\esp32-32-2 - 三元完整版`）同样用Serial1，9600bps

## 4. 主机工程当前策略
- 文件：`esp32-master`
- 当前主流程：
  - 显示：**占位状态**，等待重新接入
  - 触摸：**未接入**
  - BLE：主机作为 GATT Server，向从机发两路 float（当前值 + 库存值）
  - 重量来源：**模拟值**，不读 485
- 功能开关：
  - `ENABLE_RS485_INPUT = 0`
  - `ENABLE_WEIGHT_SIMULATION = 1`

## 5. 从机工程当前策略
- 文件：`esp32-slave`
- BLE 客户端通知 API 已修正为：`registerForNotify(...)`
- 从机目前用串口打印接收到的主机数据和连接状态

## 6. 黑屏问题根本原因
- **根本原因已找到**: 
  - 驱动 IC 配置错误（ILI9488 → ST7796）
  - 背光引脚错误（IO21 → IO27）
  - 触摸 SPI 总线假设错误（独立 → 共用）
- **解决方案**: 显示模块已重置为干净基线，按 LCDWiki 真值重新接入

## 7. 串口与烧录信息
- 主机口：`COM13`
- 从机口：`COM14`
- 之前已成功烧录过主从工程

## 8. 官方资料来源
- **LCDWiki 网页**: https://www.lcdwiki.com/zh/4.0inch_ESP32-32E_Display
- **本地资料包**: `C:\Users\Mifan\Desktop\重庆溧阳项目\4寸tft\`
  - ST7796_Init.txt：初始化代码
  - MicroPython 库：ST7796.py, touch.py
  - LVGL demo bin 文件
  - ST7796S 和 XPT2046 数据手册
  - 原理图、规格书、IO 分配表

## 9. 下一步验证重点
- RS485 读取重量变送器数据（重量变送器 Modbus 协议对接）
- 启用 ENABLE_RS485_INPUT 后验证 ModbusRtu 模块
