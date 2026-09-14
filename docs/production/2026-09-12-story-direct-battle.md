---
status: verified
owner: codex
updated_at: 2026-09-13T00:33:11+08:00
source_commit: 200c42b
---

# 剧情特殊关：对话按钮直接进入战斗

## 用户确认的流程

玩家报告382.24MiB试玩包在“任务1-1·初次战斗”只显示任务地图背景、标题及行程0/1。视频与截图一致。用户随后明确要求：保留战前对话，点击对话中的“进入1-1战斗”直接进入该剧情特殊关；胜利后接该任务的战后剧情和奖励，不再展示中转路线地图。

本轮按新的流程修复入口，不把空白地图武断归因为资源被裁剪。内部兼容路线记录仍用于旧档和战斗归属，但新剧情战斗按钮不再发布DungeonMap中间状态。普通历练和剧情调查节点的既有逻辑保留。

## 实现

- MainStory BeginJourney将剧情Battle节点的准备、进入目标与战斗准备放在同一候选状态/保存边界，直接提交Battle状态。
- 使用原BuildJourneyBattleEncounter和节点EnemyDefinitionIds，保持每个剧情关自己的敌人、战后对话及奖励回执。
- 胜利后复用退出挑战的清理与游历恢复，保留剧情胜利/后续对话证据；在保存成功后返回桌面任务流程，不产生普通历练卡牌奖励或清关进度。
- 对话按钮中文显示“进入X-X战斗”，英文显示“Enter X-X battle”；保留原MainStory.Travel动作ID供既有对话入口兼容。
- 旧档停留在AwaitingGate/ReadyToBattle的剧情战斗，由任务子系统接续为战斗；保留旧的门口未读完对话兼容。
- 重复点击不重复开战；保存失败时仍留在之前状态。

## 验证

- 新增DepartureButtonDirectlyStartsSpecialBattle红测试，在修改前按预期失败：点击出发没有产生战斗。
- 冷Editor UBT通过。43/43定向Automation通过，0 errors，9条warnings，见`Saved/StoryDirectBattle/FinalResume/index.json`。
- 覆盖MainStory、SaveIntegrity和Persistence，包括ActualCombatAndDiskFullPlaythrough、FullCampaignBothBranchesAndSixJourneys、原子保存失败、重复点击、剧情独立奖励和旧存档对话恢复。
- 旧测试中“出发后手动点地图”的假设按本次新要求更新；旧地图退出与门口对话测试使用明确的旧格式状态夹具保留覆盖。
- ShippingF10修正版正在独立目录打包，最终包体和实际运行验证待追加。

## 发布边界

新包同时包含已完成的v42存档加固及当前字体改造，不能称为旧382.24MiB包的纯EXE替换补丁。旧包保留，不修改玩家源存档。未声称所有历史回归全绿，也未改动普通历练或失败重试规则。

## 实包发现并修正的重复恢复边界

首个候选包通过旧16节点等待状态恢复，但随后导入相同修订号的两节点等待状态仍停在DungeonMap。根因是LastLegacyMapAttemptRevision成功后未清除，阻止后续恢复。已在成功提交及离开等待状态时清除缓存，新增RepeatedLegacyWaitingStateResumesBattle回归；冷UBT和43/43通过。旧失败证据保留为`Saved/StoryDirectBattle/packaged-two-node-red.json`，最终包正在复核。

## 最终交付

- 最终包：`Packaged/GameXXK-2D-Shipping-F10-StoryDirect-20260913.zip`，386.68 MiB；ZIP CRC通过，38文件，无玩家存档/PDB/ILK。
- 实际Shipping程序通过F10的30命令、30关、132检查，见`packaged-f10-final.json`。
- 旧16节点、旧2节点及同修订号2节点再次恢复三例均进入Battle/AwaitingBattle，见`packaged-story-final.json`；每例均还原临时会话。
- 修正通过状态验证与自动化入口验证；本轮未声称跨机器显卡实测或物理鼠标点击逐关验收。
- 20260912目录及ZIP为中间候选，不作为最终交付；玩家应使用20260913版。
- SHA256：`2ef37402d71518f38f2748803f24ae1746c73f49ec6a486ab6b4df18ddf496ca`。
