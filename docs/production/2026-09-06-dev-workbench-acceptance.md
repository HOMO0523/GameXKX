# F10 开发手札验收

本任务经用户单独确认，继续在根目录`codex/ui-visual-optimization`分支实施。用户同时要求符合项目水墨UI风格。本轮未提交或推送。

## 实现范围

- F10打开/收起、Esc关闭；采用独立Slate窗口，宣纸九宫格、江湖体标题/按钮、清晰数值字体和墨迹滑块。分为万物匣、配装台、试武场、试验记录。手动战斗和批量推演分成两个页签。
- 临时测试会话保留原运行时状态和游历快照；正常存档、训练恢复点和新游戏恢复点清除均受保护。“返回原进度”恢复会话来源。
- 真实物品目录、材料/金币/遗物发放，主角/伙伴/NPC等级及配装，六件套/四加二/两加两加两、品质、强化、词缀档位、宝石和实际属性来源。
- 指定真实关卡/场次/种子进入现有BattleBoard；同种子重开及返回战前。模拟复用正式战斗适配器，支持从当前战斗继续，保留阶段、难度和手牌。
- JSON命令层共用于面板、MCP和开发包本机文件入口；支持快照导入导出、异步批测、同种子A/B客户端、逐种子记录与实际运行文件指纹。
- 参数类型/范围/未知字段明确校验；失败的状态事务整体回滚。批测独立于当前游玩，执行错误与战败/僵局分开统计。

使用说明：[开发手札](../design/dev-workbench.md)。主要证据目录：`Saved/Codex/DevWorkbench-20260906/`。

## 已完成验证

- Editor最终冷UBT通过：`cold-build-release.log`。
- `GameXXK Win64 Development`实际游戏目标编译通过：`development-game-build-release.log`，生成`Binaries/Win64/GameXXK.exe`。本轮未生成完整Cook/Stage发行包，不将编译通过表述为打包游玩验收。
- 最终聚焦4/4通过，零警告：`Saved/Automation/DevWorkbench_Final_20260906/index.json`。包括会话与事务、真实关卡模拟及确定性、快照往返和目标平台透明窗口约定。
- 客户端4项测试通过：完整JSON原子投递、相同ID结果收取、非法/冲突ID拒绝、超时保留请求、批次执行错误不能被当成成功（相关行为组合在4个用例中）。
- 真实键鼠获得金币，精确增加321；主角一键生成穿戴后为100级、生命4621、攻击950；NPC土司首领独立配装后为100级、攻击584。数值读取实际投影，不是设计表估算。
- 指定普通1-1第7场进入真实BattleBoard；战斗窗口F10打开、同种子重开、返回战前通过。重开后的完整`activeBattle`与开场快照一致。
- 20个种子的实机批测全部完成，零执行错误；整份游玩状态在批测前后完全一致。报告`batch-result.json`。该样本使用主动构造的百级配装，仅证明工具执行，不作为正常等级平衡结论。

## 扩展旧基线

扩展31项回归中26项通过、5项失败，保留原始报告`Saved/Automation/DevWorkbench_Regression_20260906/index.json`。

- `CompanionBirthPoolV13.Roster`、`BladePartnerCardsV14`、`PermanentNpcV30Migration`仍断言当前存档版本为36，当前已是之前UI/结算版本的37。
- `HeroCardPoolV12`包含同一旧版本断言，以及缺少四张装备法师牌的旧迁移夹具。
- `Simulation.Policy.SetupAwareProfessionPuzzles`的手工敌方夹具缺少支持的唯一敌方展示槽，另有自动选目标卡提交了显式目标。

本轮未改变这些存档迁移规则或策略选择器。策略文件在`RunScenario`之前的代码与Dev开始前副本完全一致；本轮仅为运行入口增加“继续已初始化战斗”。旧失败不通过放宽真实规则来掩盖。

Development目标还暴露了一个旧测试不加条件引用`EWindowTransparency::PerPixel`的问题；现按既有生产代码的`ALPHA_BLENDED_WINDOWS`条件使用PerPixel或None，仅修正测试的目标兼容性。

## 最终实机与清理

- F10在HUD和真实BattleBoard窗口中开关通过。早期空响应取证显示实际前台是其他窗口；测试脚本现先核验目标HWND再发送按键，不把测试聚焦失败算成游戏故障。输入记录见`input_trace`。
- “配装台”输入88后F10收起/重开仍保持该页和88；Esc关闭，包括菜单打开时关闭通过。证据`retention-before.png`、`retention-after.png`及`release-after-escape.png`。
- 最终四页实拍：`release-final-items.png`、`release-final-battle.png`、`release-final-batch.png`、`release-final-records.png`；下拉菜单见`release-final-menu.png`。右侧最终属性固定可见，来源区域墨迹滑块真实拖动通过。
- A/B客户端对相同配装各跑3个相同种子，胜率、平均回合、剩余生命差值均为0：`compare-client.json`。
- 在真实PIE的Dev会话内调用正常保存接口返回成功，玩家原存档SHA256前后相同：`real-save-suppression.json`。
- 原存档、HUD和GameUserSettings已恢复至Dev任务开始前哈希；地图哈希保持不变，没有新增活动存档槽。证据`cleanup-and-integrity.json`；概要`verification-summary.json`。
