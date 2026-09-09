---
status: complete
owner: codex
updated_at: 2026-09-09
source_commit: aea5194
---

# 14类必要音效试接入

用户确认只保留最能提升效果的必要音效，按钮点击和五类工具操作各共用一条；并明确授权本任务继续在根项目 codex/ui-visual-optimization 分支接入测试。

## 素材来源与范围

以下作者发布页明确提供CC0许可；Spell sounds页面同时列其他许可，本次选择CC0。音频素材使用开放素材许可，不把它们称作软件源码。

| 来源 | 选用内容 |
|---|---|
| [Kenney Impact Sounds](https://kenney.nl/assets/impact-sounds) | 轻重打击、护甲碰撞、工具 |
| [Kenney Interface Sounds](https://kenney.nl/assets/interface-sounds) | 共用按钮点击 |
| [Kenney RPG Audio](https://kenney.nl/assets/rpg-audio) | 出牌纸面、倒下落点 |
| [Kenney Music Jingles](https://kenney.nl/assets/music-jingles) | 短胜负反馈；没有接入BGM |
| [rubberduck 80 CC0 RPG SFX](https://opengameart.org/content/80-cc0-rpg-sfx) | 火焰、开箱 |
| [bart Ice spells](https://opengameart.org/content/ice-spells) | 碎冰；原录音在作者页署名Stephan/pdsounds |
| [Brandon Morris Spell sounds](https://opengameart.org/content/spell-sounds) | 短电击与治疗片段 |

[CC0说明](https://creativecommons.org/publicdomain/zero/1.0/)允许复制、修改和商业使用。项目保留原文件、包内许可（如有）、作者页面记录、下载地址、SHA256和裁切/变速/增益处理记录。

- 源与处理清单：SourceArt/Audio/Essential/manifest.json、README.md、Originals、Licenses。
- 22个导入版WAV：SourceArt/Audio/Essential/Waves；48kHz、mono、16-bit PCM，合计1,013,040字节、10.542秒。
- 14类：轻击、重击、出牌、格挡、雷、火、冰、治疗、倒下、胜利、失败、奖励、按钮、工具。
- 游戏资产：/Game/GameXXK/Audio/SFX/Essential，22个SoundWave，均非循环。
- 当前是音色试用版；未声称已完成主观听感验收。

## 已完成

- 公开作者页与包内许可核对；22个派生文件的格式、时长、峰值、哈希检查。
- 按已有伤害表现事件选择物理/元素/护甲声音，闪避与零伤害保持静默；实际Block反应也有盾击反馈。
- BattleBoard按出牌提交、命中时刻、死亡展示、实际累计治疗和成功结算触发声音；不以每帧UI刷新为依据。
- 背包/桌面/战斗/共用局内按钮、天赋、主线/对白采用同一按钮音；不增加悬停音。
- 工具成功提交、洗炼采用及实际开箱/奖励接线；空输入、失败、洗炼预览/取消不播放工具完成音。
- 没有把声音接到后台游历每次普通伤害，也没有修改玩法数值、存档版本、角色素材或界面几何。
- 当前篝火已改为保命护符或行旅钱，不能沿用旧文档的立即治疗说法；领取护符复用奖励声，实际护符回血才由治疗事件发声。

## 验证进度

| 项目 | 结果 |
|---|---|
| 首轮冷UBT | 成功；用于测试先行的空实现基线 |
| 红阶段策略检查 | 3项按预期失败，分别是分型、真实治疗、静音/节流；不是编译/环境失败 |
| 实现后冷UBT | 成功；最后一次final-build.log含策略补充检查 |
| MCP导入 | 22/22 SoundWave加载、时长与保存通过；导入后脏包为0 |
| 离线复核 | 22个WAV与uasset存在、源哈希一致；14类源音频预览已生成 |
| 联合构建/绿色自动化 | ForceHeaderGeneration正常完整UBT成功；合并13项0失败0警告，其中GameXXK.Audio 5/5通过 |
| 真实操作与游戏混音录音 | 完成：22/22声音库播放有输出，真实战斗与桌面控制通过，Dev状态已恢复 |

证据目录：Saved/Codex/EssentialSfx-20260909。首轮失败报告：Saved/Automation/EssentialSfx_Red_20260909/index.json。

联合构建日志：Saved/Diagnostics/DetailedAttributesCompact/unified-build.log。联合自动化报告：Saved/Automation/DetailedAttributesCompact20260909/index.json；已单独复读5项音效结果并存为Saved/Codex/EssentialSfx-20260909/green-tests.json。没有重复运行已通过且相关源码保持冻结的检查。

## 实机结果与验证边界

- 在L_DesktopTrainingHUD真实PIE中逐条调用22个声音，全部创建播放实例并输出到游戏混音。录音为essential-sfx-output.wav，48kHz立体声、13.376秒、峰值0.208893；按顺序与22个源文件做归一化互相关，全部匹配，相关系数0.9679～0.9998。见audition.json、recording-analysis.json。
- 录音中的空闲段与轮播墙钟间隔不等长；这份记录用于核验实际声音输出，不把它作为画面同步误差的计时证据。
- 真实点击“清风一式”与敌方目标，目标气血74→55，出牌声与轻命中声各触发1次；不存在的卡牌被拒绝，播放计数保持不变。
- 随后常规自动战斗实际产生出牌25、轻击15、格挡7、重击9、倒下4、胜利1、奖励2次播放。业务流程已经过胜利/奖励并继续到后续战斗；随后主动关闭自动模式。对应混音为essential-sfx-live-battle.wav，48kHz立体声、21.6747秒，无削波。
- 桌面静音时播放调用返回false、计数不增加；恢复静音切换前的状态后继续正常测试。
- 工具空输入被拒绝、不播放完成声。测试档原本未解锁工具，先在临时Dev会话中购买Talent.Root与Talent.Entry.Tools，再分解本会话生成的装备：成功返回true，Tool计数恰好+1。未修改解锁规则或原玩家存档。
- 一个法师卡Tooltip夹具因未满足“5张唯一携带牌”的既有构筑规则而被拒绝，已清除夹具，没有计作成功的法术实操，也未放宽规则。火/冰/雷的资源、播放和事件分型已通过；本轮不宣称逐张法术构筑均已实操。
- Dev session.restore成功；复读为session_active=false、Town、无活动战斗、无工具预留；停止PIE并保存，脏包为0。14份.sav与协调前备份哈希相同。协调前HUD备份为75%，交还约定是50%，本轮沿用50%而没有覆盖为旧75%。
- 当前验证覆盖编辑器试接入，没有执行Cook/打包，没有对全部173张牌或全项目回归作全绿声明。主观听感仍按试用版处理。

实机控制记录见live-controls.json，手动出牌见physical-play.json与physical-result.json，自动流程/混音见essential-sfx-live-battle.json，恢复与交接见restored-state.json、handoff.json，最终文件检查见final-validation.json。

## 测试时段协调

头像/主线任务统一协调本项目的源码、UBT、编辑器时段。音效在归还的时段中完成实机验证，清理后已把PID42856/MCP18765交给地图节点任务并通知协调任务。交接时PIE停止、Dev恢复、脏包0；未重新编译C++。后续离线文档/录音分析没有再占用编辑器。
