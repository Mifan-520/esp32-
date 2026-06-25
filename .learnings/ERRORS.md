# 错误记录

记录命令失败、工具异常、集成报错。

---

## ERR-20260410-001

| 字段 | 内容 |
|------|------|
| **ID** | ERR-20260410-001 |
| **Logged** | 2026-04-10 |
| **Priority** | low |
| **Status** | resolved |
| **Summary** | PowerShell 中文路径编码问题导致 Read/grep 工具无法直接访问 |
| **Details** | 工作区路径包含中文"重庆溧阳项目"，部分工具（bash 直接读取、Read 用相对路径）会因编码失败。解决方案：使用 glob 获取完整路径后再用 Read，或使用绝对路径。 |
| **Suggested Action** | 遇到路径访问失败时，优先用 glob 获取绝对路径 |
| **Metadata** | 环境: Windows PowerShell, 路径编码 |

---

## 待记录项

（后续遇到的错误在此暂存，整理后转为正式条目）