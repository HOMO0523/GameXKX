---
status: record
owner: codex
updated_at: 2026-08-14
source_commit: d4c5e1f5a45162e8e924e956dc53c53fee574e3c
---
# GameXXK 当前目标(滚动指针)

> 本文件是"当前做到哪了"的**唯一滚动指针**。`AGENTS.md` 不再硬编码验收状态,改指向这里。每次目标收尾后更新本文件。

## 当前基线(更新于 2026-08-14)

- 分支:`main`
- 最近一次完整验收:`docs/production/2026-08-13-current-goal-final-acceptance.md`

## 已落地(最近一轮)

- 198 张卡(36 主角 + 108 永久伙伴 + 24 任务 NPC + 30 路线牌)完整执行,全 198 × 七地势 + 条件缺失分支通过。
- 战斗目标结果提示(悬停结算预演):水墨底图、指向目标上方、群体牌按真实存活槽位 `1P/2P/3P`、悬停不提交状态。
- 护甲前后快照与命中读条(`护甲 -N`/`气血 -N`)、出牌确认缩放 1.26×、防 0 费无限循环。

## 关键语义冻结(注意差异)

- **当前生产仍维持 v15 follower 语义**:接任务即 NPC 跟随/入队。
- **未来(放置迁移包执行后)改为**:接任务不跟随、不自动入队,玩家另点"入队"才进入战斗队伍。
- 迁移入口:`docs/superpowers/plans/2026-08-13-gamexxk-idle-desktop-migration-implementation-index.md`。

## 仅规划未实施(7 包)

历练放置、离线收益、双宝箱、2D 主界面、桌面迷你窗、自动战斗、任务 NPC 显式入队 + 默认入口迁移。**未执行生产实现。**

## 下一步待办

- 见 `docs/production/optimization-plan.md`(项目自身优化计划)。
- 战斗奖励分层(新规格 `docs/superpowers/specs/2026-08-14-battle-reward-tiering-design.md`)。
