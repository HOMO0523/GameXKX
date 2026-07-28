# Seedance Idle 模型选型设计

## 目标

使用同一张主角母图作为完全相同的首帧与尾帧，在 720p、5 秒条件下比较 Dreamina `frames2video` 当前公开的六个模型，选出最适合 GameXXK 水墨角色循环动画的模型。

## 固定输入

- 输入图：`SourceAssets/CharacterVisuals/final_selected_v1/00_hero.png`
- 首帧与尾帧：完全相同的输入图
- 时长：5 秒
- 分辨率：720p
- 朝向：角色始终朝屏幕左侧 3/4，不翻向
- 背景：纯 `#FF00FF`，不得漂移、渐变或出现阴影
- 镜头：固定，不推拉、摇移、旋转或裁切

## Idle 动作设计

一个完整循环包含中等幅度的蓄力呼吸：身体先轻微下沉，肩线与胯线形成反向倾斜，再回弹抬起，最后回到原姿。马尾、衣摆和束带在主体之后产生少量延迟摆动。双脚与脚底锚点固定，持剑手和武器结构保持稳定，不走步、不攻击、不转身。

## 测试模型

1. `seedance1.5pro`
2. `seedance2.0`
3. `seedance2.0fast`
4. `seedance2.0_vip`
5. `seedance2.0fast_vip`
6. `seedance2.0mini`

## 评分标准

每项 1–5 分：身份与结构稳定、首尾接缝、动作设计感、笔触稳定、背景稳定、朝向稳定。任何出现翻向、换装、武器变形、脚底滑动或镜头运动的结果直接标记为不合格。

## 输出

所有测试视频与机器可读记录放到 `SourceAssets/AnimationModelTests/seedance_idle_v1/hero/`，保留每个模型的提交 ID、提示词、积分差和最终状态。
