---
status: complete
owner: codex
updated_at: 2026-09-09
---

# 项目图片BC7优化进度

## 本轮完成结果

1101项纹理盘点、391项BC7与构建尺寸优化、391项重载验证和322张WebP派生均完成；原纹理资产路径与源像素保持。最终可查报告`Saved/ImageOptimization/report.html`已更新。

随后用户指出的任务关闭/全局图片刷新与对白过重问题已单独修复和验证：桌面根控件/设计画布持续挂载，切页在安全tick更新内容；主线对白复用既有DialoguePanel，采用项目立绘+独立纸面对话框。最终冷UBT通过，35项专项测试全部通过、0警告；工作台完整79项全部通过，另有3条既有1K备用图集缺失警告（guard idle、宋金宝idle、铁羽attack，代码会回退2K），不能将完整工作台报告称为零警告。

三轮真实Win32鼠标循环覆盖结果图重载、逐句回看、两层关闭、天赋/编队切换和GC后操作，主线进度不变、逐句不重建桌面。普通1-1路线→BattleBoard→退出返回2D工作台实拍通过。证据与覆盖限制见[解耦记录](../design/2026-09-09-story-ui-decoupling.md)。本轮未逐关通关六章，也未Cook，不宣称完整新美术验收或发行包大小已实测。

共享预算模块重载后，现有30项宝石与6项地图资源验证通过，`final-boundary-validation.log`。压缩试验资产经UE检查无引用；编辑器保存关闭后，仅将本任务未跟踪的pilot uasset移到`Saved/ImageOptimization/RetiredPilot`保留，移前移后SHA256一致，已从Content排除。UE自动删除未移除磁盘文件，所以没有伪称UE删除成功；证据`pilot-retirement.json`。测试编辑器已保存并关闭，无提交、推送或玩家原档写入。

61张新插图逐张确认、横向任务/天赋树和余下剧情重写仍属后续设计工作。

用户最新选择：`ok bc7吧`。范围是检查项目实际图片、处理过大资源、确保加载。复用当前任务已许可的`codex/ui-visual-optimization`根目录分支。剧情重绘仍按逐张确认，未生成新的剧情画面。

## 已完成且有证据

- 注册表审计及逐一对象加载：1101个/Game纹理，未发现缺失；未加载3D地图。清单`Saved/ImageOptimization/registry.json`和`loaded-textures.json`。
- 391个过大资源完成BC7/构建尺寸优化（264个UI，120个固定尺寸图集，7个独立角色/剧情图）。资产路径、原始嵌入像素、sRGB/alpha用途/filter/帧布局保留。
- `applied-textures.json`391项格式均为BC7，均有优化前后原像素哈希校验；`reload-verification.json`391项卸载重载成功，关闭图标/篝火/遗物的导入保存边界检查通过。
- 271张可直接和初始导出图对照的UI源像素，与初始PNG解码后的BGRA逐字节哈希一致，见`source-snapshot-comparison.json`。
- 资源汇总旧BGRA8像素估算8,157,090,268字节，新UE读回纹理分配合计1,999,503,360字节。不是同时驻留量，也不是最终发行包大小。61张剧情纹理共47,972,352字节；单张1536×512 BC7为786,432字节。关闭图标1254²→256²，65536字节。
- 710项按原用途保留：370已压缩/平台处理，250小图，55专用数据格式，1特殊类型，34非4像素对齐的精确帧格图。最后34项不为统一格式而破坏帧布局。
- 322张现有显示图片WebP派生完成，359MB→38.7MB；全部可解码、文件哈希匹配、alpha通道编码前后精确一致。15个多帧图集超过500KB单图目标，保留帧信息。原PNG/原设不覆盖，游戏不依赖WebP解码器。
- 44个图片导入脚本经共享`gamexxk_texture_budget.py`保存入口保留已审定预算。离线测试通过注入测试unreal模块验证兼容。主线导入校验比较原图尺寸，不把构建尺寸误当成母版变更。
- 第一轮冷UBT通过：`Saved/ImageOptimization/loading-build.log`；新增`UGameXXKAsyncStoryImage`取消/释放/异步请求，移除同步纹理尺寸编译，子按钮命名唯一以免覆写活跃Slate样式对象。
- `Saved/Automation/ImageOptimization/index.json`18项通过、0错误/警告，包含16项原主线/存档检查与2项图片请求取消/释放。`python-targeted-validation.log`26项通过。较大Python组合另有3项既有立绘测试失配（17/37数量、旧prepare接口、旧禁止重导约定）；相关立绘脚本本轮diff仅保存入口。
- `bc7-desktop.png`、`bc7-story-tree.png`已实拍，角色、背景、纸边和按钮图片显示正常；这不是剧情美术或旧排版的最终认可。

## 历史排障过程（以下为当时状态）

**用户进一步否决当前UI接入（最新优先）**：任务关闭无效并影响其他图片刷新，对话应恢复既有角色+对话框，不能把对白塞入背包。先按[解耦修复记录](../design/2026-09-09-story-ui-decoupling.md)修复。已取证主线子按钮缺少桌面ActionCallback保护，关闭会在Slate回调中同步触发全局ReleaseSlateResources；当前还未实施这项根因修复。此前“只剩实测”已被该真实回归推翻，不能宣称完成。

实机发现已有主线面板可能停在旧树：公开剧情操作已把S00-01推进Result，视图仍显示初始树；点击节点也未可靠刷新。已修正为工作台保留同一MainStoryCentralPanel，重新挂载时绑定变更通知，并把状态刷新排到下一次GameThread tick。此修改涉及共享工作台头文件，需要较大范围冷编译，当前尚未实机验收。

面板复用冷编译已通过：`view-refresh-build.log`（续跑114项，505秒），并追加在NativeDestruct清空图片引用，`final-build.log`再次通过（60秒）。`ImageOptimizationFinal/index.json`为22成功、1失败：图片/主线/桌面资源相关项通过，额外的详细属性页滚动恢复测试期望250但读到0。该详细属性测试文件在核对期间又发生外部修改（TakeWidget移到OpenFreeInventory之后），不覆盖这些变更；测试失败单列，不冒称全项目全绿。须继续隔离PIE复测节点详情/对白/结果/关闭/重复切换和桌面到BattleBoard。

编辑器已在MCP保存后关闭。下次启动使用已热身的磁盘缓存`Saved/ImageOptimization/DDC`，隔离UserDir=`Saved/ImageOptimization/PIEUser`，默认地图`/Game/GameXXK/Maps/L_DesktopTrainingHUD`。当前测试档S00-01已完成但未领奖，适合验证结果图。不要把测试档当原玩家存档。

## 批处理内存故障的准确记录

316项保存后，旧大批编辑器因系统提交内存不足退出，日志`bulk-build-memory-error.log`。复查当时Zen缓存可用，故不能把这次事故归因于ForceMemoryCache回退。普通编辑器GC保留独立资产；新增每批显式卸载已保存包与小批续跑后，剩余资源正常完成，重载检查也完成。

另将`ue_tdd_pipeline.py`的默认自动化启动改为可写的项目磁盘DDC，避免未来无缓存时无上限RAM回退；已有启动测试先红后绿，日志`pipeline-ddc-red.log`/`pipeline-ddc-green.log`。当前专项缓存首次基础着色器构建耗时较长，但已完成，后续应复用。

## 当时的收尾清单（当前结果见上）

1. 等当前冷UBT成功，复测主线刷新修正与相关自动化；若界面仍停旧状态继续定位，不冒称通过。
2. 实测图像重复切换/关闭及2D桌面→路线→BattleBoard，保存可读截图。SlateInspector模拟点击曾无法可靠反映动作，必须以状态/截图核对，不能把clicked:true当成功。
3. 编辑器非PIE时重新加载共享预算模块，运行gem/map验证器并检查修订后的导入保存边界。移除本任务临时`/Game/GameXXK/UI/ImageOptimizationPilot/T_StoryCompressionPilot`资产（确认无引用后用UE删除）；其他源图、历史图不删除。
4. 重建`Saved/ImageOptimization/report.html`，更新最终验收/当前目标指针。未Cook不宣称发行包实测缩减。
5. 保存并结束隔离测试编辑器；用户原始存档、手调地图/角色/摄像机保持。无提交或推送授权。
