---
status: active
owner: codex
updated_at: 2026-09-09
source_commit: aea5194cde166f844ed13dee953746c56ea45828
---

# 六章主线、61节点与61张插图验收

用户已允许本任务继续根目录 `codex/ui-visual-optimization`，未创建worktree，未提交或推送。最后的规模裁决为61个节点各配一张独立插图，覆盖该段主要剧情人物。

## 已交付内容

- 天台山7、黄山10、湘江潇水8、永州柳子庙13、土司寨12、苍山11，共61节点。每节点包含简介、目标、对白、三级免费提示、完成结果与独立插图；调查节点包含选项及错误反馈。
- 红点任务入口、左侧章节列表、中央背包区域流程树、分支连线、节点详情与开始/继续、结果插图、领奖、对白回看、项目关闭图标；局内复用相同内容，提供任务树按钮。
- S00随开局1-1可用；后章同时检查前章主干完成和对应关卡可用。主干完结后仍可补支线，合流节点不重复领奖。
- 每章一个需要进入路线的节点；先在外面交流，再进入已有路线。前两层之后整层问号指向同一任务，数量跟随生成路线。其余连续剧情接续本次行程，也可在退出后继续已开放对白。
- 四个任务战斗使用真实BattleBoard；湘江治水、土司寨巡查为调查。六个进图任务都额外奖励10高级箱、10普通箱，等级5/10/15/20/25/30。每节点100000金币，合计6100000金币及120箱。
- v38存档保存节点进度、领取凭据、提示位置和旅程上下文。候选状态校验、保存成功后再发布，奖励与剧情行程退出均支持保存失败后重试。
- 显示名统一幽白、琼幺儿；旧存档ID、角色图片和动画资源路径保留。

## 美术与文本

- `SourceAssets/Narrative/MainStory/完整剧情与任务节点.md`：完整可审阅剧情。
- `SourceArt/UI/StoryNodes/gallery.html`：按章查看61张选定原图及对应对白。
- `SourceArt/UI/StoryNodes/manifest.json`：逐图来源、版本、哈希、尺寸、角色和视觉复核记录。
- 61图均使用内置image_gen生成；依据当前篝火横幅与已有主要人物参考。逐张复核后修正了误加桥梁、文字、人体多余手掌、提桶动作、背景多余角色/兽影与失牛配色。旧版本和默认生成原图保留。
- 61个Texture2D与 `M_StoryIllustrationSoftEdge` 已通过UE5.8 MCP导入。结果图保持约3:1原始比例，在任务完成后显示。

## 自动化与构建证据

- 内容合同：`python -B -X utf8 -m unittest scripts/test_main_story_content.py`，4/4通过。
- 图片：`python -B -X utf8 scripts/validate_main_story_art.py --require-import`，61个唯一图片、61个视觉通过、内容合同/哈希/尺寸/UE包全部通过。
- `Saved/Automation/MainStoryAcceptance/index.json`：16/16通过，0警告、0错误。含六章61节点连跑、两分支补做、六次旅程、各节点存档往返、全量金币/宝箱数量及等级、错误答案和提示、重领奖、胜利来源、普通重玩隔离、保存失败原子性。
- 规则初始RED：`Saved/Automation/MainStoryRed/index.json`，目录通过、未实现行为按预期失败；实现后通过。
- 退出事务RED：`Saved/Automation/StoryExitRed/index.json`，四条断言证明原退出未经过保存边界；补齐后纳入16项全通过。
- 冷UBT使用 `GameXXKEditor Win64 Development -NoHotReload -NoHotReloadFromIDE`，未使用Live Coding/Hot Reload。日志在 `Saved/StorySystem/`。

## 真实PIE证据

全部测试位于 `/Game/GameXXK/Maps/L_DesktopTrainingHUD`，编辑器以 `-UserDir=Saved/StorySystem/PIEUser` 隔离玩家存档与HUD设置。

- 通过实际Slate按钮打开任务、章节、节点、开始任务；完成首章所有对白与调查，实测错误选项不推进、三级提示给出答案。
- 两场普通路线战斗实际获胜，再进入第三层剧情问号；该层三个入口全部为“初次战斗”。
- 任务战斗实际打出卡牌并在第4回合获胜。主角入战保持113/145气血，沿用原编队和途中取得的镇煞墨符、清心莲子。
- 任务报酬：金币321130→421130，宝箱7→27。再次调用领取保持原值；普通战斗奖励另行处理。
- 在同一旅程继续国清寺、天台山简图并补做客栈支线，首章7/7均已领奖，黄山首节点开放。记录：`Saved/StorySystem/live-s00-completed.json`。
- 关闭局内剧情只返回原路线；确认路线结算后返回纯2D桌面，任务状态与奖励仍保留。
- 已检查对白回看、右上关闭、三档缩放及插图比例；最后的按钮字高和待领奖刷新复核进行中。

最终显示检查完成后关闭隔离PIE，保留项目正常启动配置。
