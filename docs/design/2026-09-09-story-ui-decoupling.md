# 当前必须修复：任务、对话与桌面刷新解耦

用户最新反馈：任务切换后关闭无效，关闭导致其他图片刷新异常；对白不应放在背包中央重型界面，要恢复项目已有角色+对话框形式。先修稳定性与对话接入，暂停进一步美术/压缩扩展。

## 已取证的问题路径

1. 主线按钮`UGameXXKMainStoryActionButton::Clicked`直接调用`HandleAction`。关闭经`ClosePanel -> OnClosed -> Workbench.CloseCentralPageToBackpack -> RefreshLayout`。它没有桌面按钮的`bInActionCallback`保护。
2. Workbench.RefreshLayout只在本类NativeTick/ActionCallback时延迟，其他调用会立即RebuildLayoutNow。该函数移除窗口内容、ReleaseSlateResources(true)、重建整个WidgetTree。主线子控件回调于是可能在Slate输入分发中释放全局图片/按钮资源。
3. 主线StartTask对所有节点调用Commit(...,true)，使纯对白也要求PlayerController全流程刷新。主线本地画布还在NativeTick中ClearChildren重建。
4. 当前对白是MainStoryPanel.BuildDialogue/BuildChoices写在945×533背包区域，未使用现有UGameXXKDialoguePanelWidget。

## 实施方向

- 全局结构刷新统一排入下一次World/GameThread tick并合并请求，不能由外部子控件在Slate输入/绘制时同步重建。无World的离线测试保留明确同步路径；有World的NativeTick不能抢先执行全局重建。
- 主线局部结构刷新也只走安全调度；关闭先停掉自己的回调/加载/视图，不能释放别的界面的图像。
- 原主线规则与存档仍唯一负责进度/奖励。纯对白推进只更新呈现数据，不调用全流程重建；仅进出游历/战斗时请求全流程切换。
- 复用现有DialoguePanel及FGameXXKDialoguePresentationView，提供轻量的角色+纸面对话框布局与明确继续/提示/关闭事件。不要再在MainStoryPanel里写第二套对白布局。
- 桌面对白呈现时收起背包视觉，保留任务树状态，采用独立轻量对话层；逐句更新稳定控件，不重建桌面。地图内对白使用同一呈现接口。对话结束/暂停后回到任务详情或树，任务树右上关闭只关闭本页。
- 角色图绑定项目现有原设资源，主角/幽白/金贵等主要NPC不能用生成图中的变脸代替。缺少专属配角头像时不擅自套别的角色身份；后续随逐张美术确认补齐。
- NativeWindow输入区域必须随轻量对白实际区域更新，避免隐藏背包的大矩形继续挡住桌面。

## 2026-09-09当前实施点

- 已添加`ChildCallbacksDeferRootRebuild`真实World测试。修正测试重复初始化World的夹具错误后，旧代码按预期失败：外部子控件请求未排队，调用栈内LayoutBuildCount已从0变1。证据`Saved/Automation/StoryUiDecouplingRed/index.json`。
- 已实现全局RefreshLayout在有World时统一排队；NativeTick不再抢先同步重建，内部重建期间再次请求也只标记pending。主线局部刷新同样排队。
- `GameXXKAsyncStoryImage`区分显式关闭和Slate资源重建：后者保留期望路径，重新挂载自动恢复图片；新增重挂载断言。
- 新增无状态`GameXXKMainStoryDialoguePresentation`适配器，从既有MainStory存档状态生成原DialoguePresentationView；不创建第二份DialogueSession。主要人物使用项目原立绘。配角缺头像时作为画外发言，不替换成其他NPC。
- 现有`GameXXKDialoguePanelWidget`新增compact布局和独立继续/提示/暂停事件，保留原默认布局供旧系统。紧凑模式在背包视觉之外展示角色和小纸面对话框，稳定更新文本和选项。
- 工作台缓存该对话框，只有进入/离开对话时改变结构；对白/提示逐句只Present新视图。背包、任务抽屉、底部导航在对白时隐藏；NativeWindow仅保留旅途条、角色和纸面对话框的实际输入区域。
- 主线地图内对话/回看复用同一DialoguePanel，地图任务树缓存但从视口移除。Subsystem的视口增删/全流程刷新也统一排到下一World tick；纯对白StartTask不再请求全流程刷新。
- 背包中的BuildDialogue/BuildChoices/BuildReplay旧实现已移除，任务树/节点详情/结果图保留，字体统一江湖体。任务树右上关闭同时收起任务抽屉。
- 新增`ExistingDialoguePresentation`和`DialogueUpdatesPreserveControls`测试，验证原角色图、无第二份进度、复用控件和关闭不推进剧情。

初轮冷编译通过。进一步实机发现原RebuildLayoutNow仍卸载整个桌面宿主，BuildProgrammaticLayout也每次拆掉设计画布。新增HostSurvivesNavigation测试在旧实现失败：导航按钮的当前Slate资源失效，见`Saved/Automation/StoryHostRed/index.json`。最终改为保留UUserWidget/SObjectWidget及设计画布，通过Canvas槽更新内容；去掉切页时的全局ReleaseSlateResources/重挂载。旧Slate引用不能再触发新页面资源释放。

最终冷UBT通过：`Saved/ImageOptimization/stable-host-build.log`；35项针对性检查全部通过、0警告，见`Saved/Automation/StoryUiStableHost/index.json`。两条旧对白测试也对齐实际契约：可滚动只读历史仍允许子控件命中，存档迁移不硬编码已经过期的版本36。

随后工作台完整回归79/79通过，见`Saved/Automation/StoryWorkbenchRegression/index.json`；有3条既有1K可选图集缺失警告，由CompactTravelClipPair回退2K（guard idle、宋金宝idle、铁羽attack）。不声称该完整报告零警告，也不把之前其他任务详细属性的失败或正在修改的宝石设计覆盖掉。

## 实机结果

用户暂停操作后，在隔离PIEUser、默认L_DesktopTrainingHUD执行三轮真实Win32鼠标循环，证据`Saved/StorySystem/stable-ui-verification.json`。每轮均打开任务树、S00-01结果图、回看对白，逐句核对前四句、关闭对白返回树、关闭任务返回背包、切天赋/编队，触发GC后再次切页。剧情图每次加载成功，对白逐句不增加全局LayoutBuildCount，回看与关闭均不改变主线进度。

SlateInspector的Click返回true并不保证动作成功；自动分发在该DPI原生窗口仍偶有仅聚焦。验收脚本`verify_main_story_ui_lifecycle.py`使用项目Win32输入控制器，以Slate控件截图实测尺寸和窗口物理矩形换算点击点，并在每步断言可见文本/状态。没有把clicked:true当通过。

另用真实StartTask(S00-04)进入普通1-1路线，点击第一层可达战斗节点，完整显示BattleBoard的六个角色、卡牌和背景；点击关闭、确认退出后回到同一2D地图的工作台。截图：`stable-dialogue-hero.png`、`stable-after-story-close.png`、`stable-battle-loaded.png`、`stable-return-from-battle.png`，均位于`Saved/StorySystem`。

本轮实机没有通关整章，也没有逐一实测六章每个调查选项/地图内对白。规则层保留原61节点、奖励、分支和存档测试；横向任务/天赋树与逐张插图重绘仍是后续设计工作，不计入本次修复完成。

## 必须验证

从主线子按钮触发关闭时，同步调用栈内全局LayoutBuildCount不增加；下一World tick才完成一次结构刷新。连续关闭/打开/切换后其他图片和按钮仍有效。

对白首句/逐句/提示/选择/结束/暂停有明确状态切换，使用既有角色+对话框，不在背包纸面堆全文。正常对话逐句不增加全局LayoutBuildCount。剧情图请求取消后旧图不能复活，重开后能重新加载。

保留所有61节点、奖励、分支、一次游历和存档语义。391项BC7已通过的资源检查可复用；不能用这些检查替代本次UI行为验收。
