# 六章主线英文接入

用户要求主线全部翻译，并指出宝石/装备标题、Maps/难度/章节/挑战以及敌意图小字描边问题。主线全文与独立对白显示由本任务负责；同一Workbench、EquipmentTooltip、BattleBoard区域的改动与《优化界面引导并完成本地化》合并，避免并发覆盖。

## 文本与显示

- 中文剧情、61节点ID、依赖、奖励、战斗证据及全部已批准插图保持。
- `Content/Localization/GameXXK/MainStory/S00.en.json`至`S05.en.json`：六章标题/简介、61节点标题/简介/目标/结果、486句对白（包括22句战后收尾）、全部调查选项/反馈及提示。
- `UI.en.json`：主线按钮、提示、状态及失败说明；生成器补齐每章入口、支线状态与不同等级奖励说明。
- `scripts/build_main_story_localization.py`输出独立`main-story.entries.json`，1106个唯一条目（含24个短节点名）、1137个正文/界面数据位置。汇总运行词库`strings.json`由协调任务按namespace/key合入；重生成主线片段不覆盖其他词库。
- 沿用Hero、Youbai、Jin Gui、Zhou Guangzu、Qiong Yaoer、Song Jinbao、Tusi Chief既有英名。保留无厘头武侠口吻，线索与正确选项含义不变。
- `MainStoryPanelWidget`在展示边界本地化。实拍发现长节点名换行会挤压状态，最终改为英文短节点名、单行有界缩放；详情和悬停保留完整译名。长章名/详情标题采用20字号及有界换行，修正旧字符数公式把标题压成12字号的问题。侧栏使用Xiang & Xiao、Yongzhou等短地名，完整章名仍保留在正文。切换语言后重绘而不推进任务。原对话框对角色名、正文、选项及提示应用本地化FText，英语长选项换行；启程不再显示无效的调查提示按钮。任务问号节点下的名称也在适配时本地化。

## 验证

分章生成验证已通过：6/6章节、61/61节点、每个对白/选项/提示数目一一对应，英文无残留汉字，同源译文一致。合入后的共用词库4项结构/参数/数值检查通过，包含10万与100,000等值、叙事once连词不误当机制次数的处理。

联合冷UBT通过：44动作1444.43秒，日志`Saved/Codex/UIGuidanceLocalization-20260910/ui-surface-localization-build.log`。`Saved/Automation/MainStoryEnglish20260910/index.json`共31/31通过、0警告、0错误；包含新增`GameXXK.MainStory.EnglishFullTextAndDialoguePresentation`，逐项验证985个正文位置、486句真实对白控件、角色显示名，以及同一可见对白即时中英切换。所有主线/两项存储/普通关卡编组/三项桌面宿主回归同时通过。

本次61节点实际战斗与磁盘读档流程再次完整通过，证据`Saved/StorySystem/Localization/playthrough-report.json`。代表实拍已完成：S03英文树、S04-06完整详情，实际原生回看按钮点击后也读到英文Narrator与完整正文。词库覆盖与这些实机证据分别记录。

最终两处排版/地图名修订已再次冷编译：`Saved/Codex/UIGuidanceLocalization-20260910/warehouse-physical-scale-build.log`，6动作97.91秒Succeeded；包含协调任务的仓库与像素比例修订。

后续实拍确认短节点名及S04详情正确；S03章标题仍被ScaleBox自动换行压小，进一步改成22号地名和14号副标题的固定两行。`ink-disclosure-build.log`冷UBT104.30秒Succeeded，`ink-disclosure-tests.json`5/5、0警告/错误，明确包含主线全文与实际对白即时语言切换。MCP单项过滤应使用完整名称或首尾锚定；`StartsWith:`会自动补尾部点，只适合测试组，先前“未发现该测试”是过滤表达式问题，测试实际已链接入当前DLL。

最终S03标题及节点图：`Saved/Codex/UIGuidanceLocalization-20260910/ui-review/main-story-s03-heading-final-w4.png`；完整长节点详情：`main-story-s04-06-final-w4.png`。已人工确认标题与状态分离、英文完整，主线审计15个可见文本无汉字；审计对ScaleBox内部文本的自然宽度提示不直接等于实际裁切，需与图中显示相互核对。预览结束严格恢复原RuntimeState，再正常保存、停止PIE、保存脏包0；后续编辑器由协调任务管理。

## 共享编辑器重启后的真实进度保护

首轮英文章节实拍中，另一制作任务在未协调的情况下重启了编辑器。其测试未把61节点视觉夹具写入玩家槽；但磁盘仍为8级，而本任务已保留的真实快照是9级/472160金币。只读报告`save-audit-after-restart.json`确认了差异。

恢复前全部sav已备份，真实原快照另存`ui-original-before-shared-restart.json`。经Dev受保护导入、唯一恢复槽、结束Dev、正常MVP加载/保存后，Slot1和Checkpoint均读回9级/472160金币/22箱/6个真实任务且哈希相等；库存、天赋、任务逐项与原状态一致。报告`original-restoration.json`及`before-original-recovery-20260910-160840`保留证据。不会恢复较早的根任务快照。

之后新临时预览恢复成功，最新474260金币/9级/HP220已正常保存，备份`Saved/StorySystem/GateSaveFix/PlayerSaveBackup-20260910-161428`。英文预览脚本现在先保存当前正常玩家再导出并进入Dev；协调任务也给通用编译脚本加入活动PIE保护。

先前主线战斗/恢复/奖励通跑证据见`2026-09-10-story-battle-aftermath.md`：61节点、6行程、4实战、69次磁盘读档，610万金币、60普通箱、60高级箱。该流程检查与本轮英文显示验收分别记录。
