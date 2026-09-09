---
status: active
owner: codex
updated_at: 2026-09-09
source_commit: aea5194cde166f844ed13dee953746c56ea45828
---

# 六章主线系统实施进度

用户已明确批准本次继续根目录 `codex/ui-visual-optimization` 分支。任务范围已从最初六图纠正为 **61节点各一张，共61张剧情插图**；包含各段主要人物，严格绑定该节点剧情结果。默认仅纯2D桌面到路线/BattleBoard，不恢复旧马车或3D默认路径。

## 第一版历史实施记录（视觉已被用户否决）

- 核对完整draw.io，与PNG内嵌图数据一致；61个候选节点稳定编号。
- 写入 `docs/superpowers/plans/2026-09-09-main-story-system-plan.md`。
- `scripts/main_story_authoring.py` 与 `main_story_s00.py` 至 `main_story_s05.py`：全61节点的简介、对白、调查、三级提示、结果和插图合同。
- `scripts/build_main_story_content.py`：生成 `SourceAssets/Narrative/MainStory/campaign.json`、完整剧情审阅Markdown、61条图像请求与运行时嵌入目录。
- 内容合同测试先因缺少剧情文件失败，补齐后 `python -B scripts/test_main_story_content.py` 为4/4通过。此证据仅覆盖内容合同，不冒充UE运行时通过。
- 冷UBT基线调用成功，目标已是最新：`Saved/StorySystem/baseline-editor-build.log`。尚非新增系统的编译证明。
- 六章61张独立插图已全部使用内置image_gen生成，逐张核对人物与剧情结果；局部修订保留独立版本。当前61图均视觉通过，尺寸/哈希/内容合同/唯一性/导入检查通过。
- 61个Texture2D与独立软边UI材质已通过UE5.8 MCP导入。`SourceArt/UI/StoryNodes/gallery.html` 提供分章原图与对白总览。
- 主线目录、规则、Subsystem事务、v38存档迁移、六个必经问号层与真实BattleBoard桥接已实现；显示名统一为幽白、琼幺儿，存档ID不变。
- `Saved/Automation/MainStoryCampaignFinal/index.json`：主线与路线结算12/12通过，0警告、0错误；包含实际目录61节点连跑、全部分支补做、六次旅程、每个节点存档往返、6100000金币与分级120箱。
- 冷UBT通过。实机已打开任务红点、章节列表、中央树与节点简介；正在修正观察到的长标题与按钮对比度，并完成余下交互。

## 当前真实状态

第一版规则和首章流程已验证，但用户明确否决剧情/插图与排版质量。目前进入[剧情关键帧与横向界面重设计](../superpowers/plans/2026-09-09-story-visual-redesign-v2.md)：S00—S04共50个节点已写入转折和镜头草案，S05尚未改写。试出了断桥、船夫泥手、合力清渠三张高光构图，仍需用户审定；旧61图不算新设计完成。横向主线图卡、独立加宽对白纸框与框内立绘已经实施；幽白保留完整火灵轮廓。天赋按用户最新偏好恢复四向并新增合计属性。宿主与子控件生命周期已修复并有关闭/GC专项证据，最新冷构建及119项合并检查通过；关闭背包首帧跳变已修复，50/75/100逐帧验证通过，隔离PIE三轮关闭/切页/GC回归通过。详见[视觉实施记录](../design/2026-09-09-story-ui-visual-implementation.md)。

**用户最新要求已暂停批量出图：先逐张核对剧情高光/人物动作/构图，确认后只出一张，再询问用户这张有什么问题。未确认满意不推进下一张。** 当前审阅对象为S02-02「船夫翻开泥手」主角身份修订稿s02_02_hero_identity_v1：已按用户明确反馈保留带笔触画风，将主角改回项目原设；1536×512 WebP约154KB。用户已通过此图，现已导入原S02-02纹理路径；详情与缩略图统一篝火式四边羽化，BC7资源0.75MiB，保存卸载重载和实机图检查通过。下一张S02-03先核对俯视小舟/筷子讲渠的构图；没有批量生成。`Saved/StorySystem/art.pause`记录此暂停，不能自行清除后恢复批量生成。候选未获认可前，不设为全61张的风格基准，也不导入UE覆盖当前素材。

随后用户认可的是所贴带笔触版本，指出主角不像项目角色并要求控制单图大小。已按实际主角生产帧/高清原设查明脸型、眉眼与发型漂移，以及前轮错误的成年人比例提示；已测同图2170×725 JPG约491KB、1536×512 JPG约275KB/WebP约159KB。导入脚本仍TC_EDITOR_ICON未压缩，2170×725还不满足BC7的4像素对齐；UE改动和Cook均未实施。详见[体积与角色核对](../design/2026-09-09-story-image-budget-and-hero-lock.md)。下一步船夫图只修主角，等待用户确认；禁止恢复批量。

测试使用 `-UserDir=Saved/StorySystem/PIEUser` 隔离玩家存档和HUD设置；默认地图始终为 `L_DesktopTrainingHUD`。

参考图为 `SourceArt/UI/RouteCamp/campfire_rest_banner_v3.png`。主要角色参考在 `SourceAssets/CharacterVisuals/final_selected_v1/`；主角00、土司07、宋金宝08、幽白09、周光祖10、金贵11、琼幺儿12均已视觉查看。输入角色图的洋红色是色键背景，生成图必须忽略。

## 验收口径

每节点首次完成100000金币；六个进图任务S00-04/S01-07/S02-05/S03-10/S04-04/S05-06另给10高级和10普通箱，等级随章5/10/15/20/25/30。任务树总金币6100000，高级60、普通60；普通关卡收益另计。主干结束不锁掉未做支线。每条线性流程至多要求一次入图，前两层之后所有路线入口必须经过同一剧情问号层。

无Live Coding/Hot Reload；后续关闭编辑器前必须MCP保存。测试使用隔离存档，保护原角色、装备、地图与HUD设置。
