---
status: active
owner: codex
updated_at: 2026-09-12T21:20:00+08:00
source_commit: 200c42b
---

# Steam 前置：本地存档加固

状态：第一阶段已实现并进入最终复核，见 `2026-09-12-save-hardening-acceptance.md`。承接 `docs/design/2026-09-10-idle-save-system.md`，不宣称已实现。

## 顺序

1. 独立测试数据、玩家文件哈希备份，确认当前编辑器是否存在 PIE 或开发会话；通过 MCP 保存后再关闭编译。
2. 存储校验：验证原始落盘字节，兼容原 GVAS/IntegritySchema 0、1；不通过增加游戏结构字段破坏旧 CRC。新格式需完整性与跨进程回读测试，保留标准 UE 读取兼容。
3. 主档与检查点：明确角色/槽位归属和提交顺序；旧格式采用保守兼容，未来版本不得被旧备份覆盖。覆盖普通保存成功但检查点失败、损坏恢复点、新角色及多槽混读。
4. 周期保存：30 秒有变化才写，失败可重试；Dev/教学/自动化写保护；正常退出既有保存门保持。不能宣称强制结束进程时还能临时写档。
5. 冷 UBT、针对性的 Automation，再进行隔离用户目录的桌面地图行为验证；报告真实通过项与剩余边界。

## 相关实现

- `Source/GameXXK/Private/MVP/GameXXKSaveStorage.cpp`
- `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`
- 对应 Public/MVP 头文件和 Private/Tests 存档测试。
- `scripts/ue_mcp_client.py`、UBT、`scripts/ai_production_loop.py` 的自动化入口；运行测试时显式隔离 UserDir。

## 范围边界

本地存档完成后才接 Steam 云同步、账号及成就。Steam 资料准备并行见 `steam-release-readiness.md`。不改包体策略、不清理缓存、不覆盖用户资产；不把现有 382.24 MiB 成品当成此次代码验证产物。
