# GameXXK 伙伴切换头像设计

日期：2026-08-06  
状态：用户已选择方案 C（脸部头像裁切）

## 目标

为伙伴背包底部的伙伴切换条制作六职业头像。切换条仅出现在伙伴界面，主角界面不显示。

## 布局规则

- 单个头像画布固定为 `105 × 62 px`，RGBA 透明底。
- 每页显示 3 个伙伴，左右按钮翻页，共 4 页、12 个伙伴实例位置。
- 当前六职业头像用于已获得伙伴；其余实例位置显示由同一头像派生的灰暗锁定态。
- 头像在槽内按视觉中心对齐，不改变用户已排好的三个槽位和页面布局。

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

锁定态在文件名后追加 `_locked`。PSD 图层使用同名但不含扩展名。

## 验收

- 12 张输出均为 `105 × 62 px` RGBA PNG：六张正常态、六张锁定态。
- 正常态脸部大小和基线一致，小尺寸下可快速区分六职业。
- 锁定态与正常态裁切位置完全一致，降低饱和度和明度，并预留统一锁图标叠加。
- 不修改主角页，不覆盖终版 Idle 源图，也不改变用户手调的 PSD 布局。
