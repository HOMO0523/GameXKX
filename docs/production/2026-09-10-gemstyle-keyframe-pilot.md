# 宝石画风小幅动作与关键帧接入测试

用户先要求3角色＋3怪物各一套idle/attack，随后要求暂停生成、先接入测试，再要求改为少量固定关键帧。

## 当前范围

- 当前接入只覆盖主角、公鸡，各8帧idle和10帧attack；都来自已生成视频，没有为关键帧方案新增付费生成。
- 即梦CLI 1.4.18，默认seedance2.0_vip，4秒、720p、相同首尾图。已接受9个任务，每个56积分；另外3个尚未提交。manifest的submissionsPaused已开启。
- 主角攻击跳过模型擅自添加粉色光圈的过渡帧，关键动作顺序保留蓄力、出拳、收回。
- 角色原画、旧游戏图集、已批准三人样板未覆盖；21怪物批次暂停在9张。

## 接入

`GameXXK.BattleAnimation.GemstyleKeyframes 1` 开启预览，`0` 恢复生产图集；默认0。应在进入训练战斗之前设置。

独立贴图位于 `/Game/GameXXK/BattleAnimations/GemstylePilot/`。真实ClipDescriptor帧数为8/10；idle源速率4fps，攻击源速率10fps，攻击仍由现有战斗事件时长适配。两种旧主角攻击事件变体都选择同一段新出拳动作。其它单位及显式4K回归路径保持原映射。

图集为2048×2048、8×8网格、256像素格；只填前8或10格。关键帧512PNG及选帧记录位于 `SourceAssets/AnimationProduction/gemstyle-pilot-20260910/keyframe-pilot/`。同一单位的idle/attack共用一个固定比例和脚底基准，未逐帧裁切或重新居中。

## 已有证据

- 冷UBT成功，未使用Live Coding或Hot Reload。
- `GameXXK.Presentation.BattleAnimation.GemstyleKeyframePilot` 1/1通过：实际帧数、空格越界防护、攻击末帧、事件时长适配、关闭恢复与其它角色不受影响。
- `GameXXK.UI.Battle.UnitVisualWidget` 3/3通过：IdentityAndLayout、AbsoluteTimePlayback、AtlasAndTerminalLifecycle。
- 36张PNG、4张图集通过尺寸、实际填充格数、未用格透明与所有帧四边Alpha检查。
- 导入成功，4条新贴图路径和2048尺寸读回通过。
- 报告：`Saved/Diagnostics/GemstyleKeyframePilot-20260910/{verification,asset-checks,import}.json`。

## 实机短验与最新公鸡比例

在协调释放的18765上启动了canonical纯2D训练Dev会话，读到主角/公鸡的新idle材质绑定；主角真实攻击卡点选及目标确认成功。Slate窗口截图返回空mimeType/data，采样期间不可见窗口没有推进完整视觉循环，因此不宣称完整实机动画画面验收通过。已严格session.restore、SaveCurrentGame成功、StopPIE后确认PIE=false、dirty0并归还窗口。后续不再自行占用或重启；统一由任务“优化界面引导并完成本地化”协调。

用户随后要求公鸡恢复旧版大小并先补齐全怪物静态图片。公鸡idle/attack共用固定变换已从512画布244px主体高改为422px（匹配生产审计0.824219占格高），脚底恢复原41px留量，即471px。两张公鸡独立试验图集已重导；18帧四边Alpha均为0。主角尺寸未改。测试开关关闭，动画新生成继续暂停。

本任务此前冷编译启动了PID37664。MCP保存脏包返回dirty_before/after均为空；没有主动保存临时玩法到真实槽。before目录仅为磁盘副本，不得用它覆盖其它任务保护的15:47最新真实state；恢复应由持有最新state的主线任务处理。
