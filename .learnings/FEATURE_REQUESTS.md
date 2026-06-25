# 功能请求记录

记录新能力诉求、新自动化需求、新工具需求。

---

## FEAT-20260410-001

| 字段 | 内容 |
|------|------|
| **ID** | FEAT-20260410-001 |
| **Logged** | 2026-04-10 |
| **Priority** | high |
| **Status** | pending |
| **Summary** | RS485 读取重量变送器数据（Modbus 协议对接） |
| **Details** | 当前 ENABLE_RS485_INPUT=0，重量来源为模拟值。下一步需要启用 RS485 功能，对接重量变送器 Modbus RTU 协议，读取实际重量数据。 |
| **Suggested Action** | 1. 启用 ENABLE_RS485_INPUT 2. 实现 ModbusRtu_Loop 真实读取逻辑 3. 验证重量变送器响应格式 |
| **Metadata** | Config.h: ENABLE_RS485_INPUT, ModbusRtu.h, WeightSensor.h |

---

## 待记录项

（后续的功能需求在此暂存，整理后转为正式条目）