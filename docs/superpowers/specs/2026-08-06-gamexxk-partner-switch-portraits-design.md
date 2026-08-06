# GameXXK 伙伴切换头像设计

日期：2026-08-06  
状态：用户已选择方案 C（脸部头像裁切）与 A（透明底水墨翻页箭头）

## 目标

为伙伴背包底部的伙伴切换条制作六职业头像。切换条仅出现在伙伴界面，主角界面不显示。

## 布局规则

- 单个头像画布固定为 `105 × 62 px`，RGBA 透明底。
- 每页显示 3 个伙伴，左右按钮翻页，共 4 页、12 个伙伴实例位置。
- 空槽不显示任何头像，也不使用锁图标。
- 已获得但未出战的伙伴显示灰暗待命头像；当前唯一出战伙伴显示正常亮色头像。
- 打开伙伴界面时默认定位到当前出战伙伴所在页；没有出战伙伴时默认显示第一页。
- 左右箭头只负责翻页。点击任一已拥有的伙伴头像即直接将其设为出战，不再提供独立“出战”按钮；切换成功后保持在该伙伴所在页，旧出战伙伴同步变为灰暗待命态。
- 伙伴包获得的新伙伴填入空槽并以灰暗待命态显示。
- 悬停已拥有的伙伴头像时显示该伙伴实例名称；空槽无名称 Tooltip。
- 头像在槽内按视觉中心对齐，不改变用户已排好的三个槽位和页面布局。

## 翻页按钮

- 左右按钮均为 `36 × 62 px` RGBA 透明 PNG，与 `105 × 62 px` 头像槽同高。
- 使用粗水墨单箭头，不增加纸框、圆底或文字；透明区域仍属于运行时完整按钮命中框。
- 默认态为深墨箭头，按下态在同一轮廓上增加轻微墨晕，不改变中心位置。
- 文件与 PSD 图层命名：
  - `companion_page_left_Button`
  - `companion_page_right_Button`

## 图像来源

六张头像只从当前局内终版 Idle 动画首帧裁切：

- `character_01_blade_idle/frames/frame_0000.png`
- `character_02_guard_idle/frames/frame_0000.png`
- `character_03_healer_idle/frames/frame_0000.png`
- `character_04_hunter_idle/frames/frame_0000.png`
- `character_05_sorcerer_idle/frames/frame_0000.png`
- `character_06_formation_master_idle/frames/frame_0000.png`

禁止使用已退役的 `SourceAssets/PartyDeck/card-portraits/generated` 头像。

## 裁切与校色

- 采用脸部特写，不强制保留完整武器或全身。
- 保留发型、头饰、面部轮廓和少量领口，六张脸部视觉尺寸一致。
- 允许按人物朝向微调水平位置，使视线朝向槽内中心。
- 校色只统一亮度、对比度和饱和度，不改变职业主配色和身份特征。
- 缩放使用高质量重采样；最终 Alpha 边缘不得出现洋红残边、黑边或半透明脏边。

## 文件命名

- `partner_portrait_blade.png`
- `partner_portrait_guard.png`
- `partner_portrait_healer.png`
- `partner_portrait_hunter.png`
- `partner_portrait_sorcerer.png`
- `partner_portrait_formation_master.png`

待命态在文件名后追加 `_inactive`。PSD 图层使用同名但不含扩展名。此前生成的 `_locked` 文件不得继续作为未获得状态使用，应在下一资产批次中确定性迁移为 `_inactive`。

## 遣散规则记录

- 伙伴页底部复用背包危险操作按钮的纸片，但文本由“分解”改为“遣散”。
- 只有永久伙伴名册达到 `12 / 12` 时按钮才可点击，操作目标是当前选中的伙伴。
- 未满 12 人时按钮保持禁用；本轮只记录规则，不实施遣散事务。

## 验收

- 12 张头像输出均为 `105 × 62 px` RGBA PNG：六张正常态、六张灰暗待命态。
- 正常态脸部大小和基线一致，小尺寸下可快速区分六职业。
- 待命态与正常态裁切位置完全一致，只降低饱和度和明度，不叠加锁图标。
- 两张翻页按钮均为 `36 × 62 px` RGBA，左右轮廓互为严格镜像，墨迹重心一致。
- 不修改主角页，不覆盖终版 Idle 源图，也不改变用户手调的 PSD 布局。
