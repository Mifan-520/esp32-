# 学习记录

记录纠正、经验、知识缺口、最佳实践。

---

## LRN-20260410-001

| 字段 | 内容 |
|------|------|
| **ID** | LRN-20260410-001 |
| **Logged** | 2026-04-10 |
| **Priority** | medium |
| **Status** | resolved |
| **Summary** | 代码清理前必须先分析依赖关系，避免删除被引用的文件导致编译失败 |
| **Details** | esp32-master 的 Rs485Test.h 被 main.cpp 第9行 include，如果直接删除会编译失败。正确顺序：先移除引用 → 再删除文件。esp32-slave 的 lv_font_chinese_18.c 被 Display.h extern 声明，同样需要先移除声明再删除文件。 |
| **Suggested Action** | 任何删除操作前，先用 grep 搜索引用关系，制定删除顺序 |
| **Metadata** | 文件: main.cpp, Display.h, Rs485Test.h, lv_font_chinese_18.c |

---

## LRN-20260410-002

| 字段 | 内容 |
|------|------|
| **ID** | LRN-20260410-002 |
| **Logged** | 2026-04-10 |
| **Priority** | medium |
| **Status** | resolved |
| **Summary** | 条件编译的 include 语句也要用 #if 包裹，避免死引用 |
| **Details** | WeightSensor.h 无条件 include "ModbusRtu.h"，但 ENABLE_RS485_INPUT=0 时 ModbusRtu 模块被禁用，形成死引用。解决方案：用 `#if ENABLE_RS485_INPUT` 包裹 include。 |
| **Suggested Action** | 检查所有条件编译模块的 include 依赖，确保 include 本身也条件化 |
| **Metadata** | 文件: WeightSensor.h, ModbusRtu.h, Config.h |

---

## LRN-20260410-003

| 字段 | 内容 |
|------|------|
| **ID** | LRN-20260410-003 |
| **Logged** | 2026-04-10 |
| **Priority** | low |
| **Status** | resolved |
| **Summary** | platformio.ini 的 lib_deps 应与实际功能开关同步，避免无用依赖 |
| **Details** | esp32-master 的 ModbusMaster 库依赖在 ENABLE_RS485_INPUT=0 时无用。解决方案：注释掉 lib_dep 行，并添加说明"仅在 RS485 开启时需要"。 |
| **Suggested Action** | 定期检查 lib_deps 与 Config.h 功能开关的一致性 |
| **Metadata** | 文件: platformio.ini, Config.h |

---

## LRN-20260410-004

| 字段 | 内容 |
|------|------|
| **ID** | LRN-20260410-004 |
| **Logged** | 2026-04-10 |
| **Priority** | high |
| **Status** | resolved |
| **Summary** | 字体文件 extern 声明要与实际使用同步，避免声明未用的死代码 |
| **Details** | esp32-slave 的 Display.h 声明了 lv_font_chinese_18 extern，但 CreateUI 中从未使用该字体。grep 搜索确认整个工程无实际引用。 |
| **Suggested Action** | 添加新字体后，必须确保 UI 代码中通过 lv_obj_set_style_text_font 使用 |
| **Metadata** | 文件: Display.h, lv_font_chinese_18.c |

---

## 待记录项

（后续发现的学习点在此暂存，整理后转为正式条目）