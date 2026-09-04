---
status: record
owner: codex
updated_at: 2026-09-05T00:38:00+08:00
source_commit: eb5297c
working_tree: companion profession labels implemented; Xuanjia/Shanhe runtime and build analysis remain active
---

# 永久伙伴职业统称验收

永久伙伴显示名已从随机姓名池统一为六个职业统称：刀客、守卫、药师、弓手、法师、阵师。

## 完成内容

- `GetCompanionDisplayName` 成为唯一伙伴职业名入口，任意旧 `NameSeed` 均返回同一职业名。
- 姓氏、名字及种子混合算法已移除；`NameSeed` 字段继续序列化，仅作旧档兼容。
- 名册悬停显示 `职业 · Lv.等级 · ★星级`，同职业实例仍由稳定实例ID、等级、星级、出战状态、卡组和装备区分。
- 商店结果从 `随机名（职业）` 改为 `获得伙伴：职业`，结果悬停不再重复职业。
- 路线商店、挂机条和卡牌所有者继续调用统一入口，自动显示职业统称。
- 界面旧同义词“护卫、医师、猎手、术士”已从伙伴名册和商店职业入口移除，统一为守卫、药师、弓手、法师。
- 同步核实伙伴解锁：普通职业1/5/10/15级分别开放6/10/14/18张，阵师开放12/14/16/18张；百级统一从完整18张中选择5张。旧“终身出生6张”文档已纠正。

## TDD证据

- RED：`Saved/Automation/CompanionRoleName_RED/index.json`，1项测试失败，12条错误全部为随机姓名不等于职业统称。
- GREEN：`Saved/Automation/CompanionRoleName_GREEN/index.json`，1/1通过，0 warning、0 error。
- UI初次GREEN：`Saved/Automation/CompanionRoleUi_GREEN/index.json`，商店与路线商店2项通过；名册测试因错误假设新招募必在第0格而失败。
- 修正测试夹具后：`Saved/Automation/CompanionRoleRoster_GREEN2/index.json`，名册1/1通过。
- 最终UI组合：`Saved/Automation/CompanionRoleUi_GREEN2/index.json`，3/3通过。
- 存档往返：`Saved/Automation/CompanionRoleSave_GREEN/index.json`，NameSeed保持、显示名忽略种子，1/1通过。
- 冷UBT均使用 `-NoHotReload -NoHotReloadFromIDE` 并通过；未使用Live Coding。

## 后续边界

本轮没有改伙伴实例ID、招募模板、头像、卡牌种子、18张个人牌池、等级、星级或装备。玄甲、山河消费者及新的配队装备分析继续按已批准计划执行。
