# 宝石箭头梯形尾迹

基点：`7bca496f`，根项目 `main`。按用户要求，用数量更少的梯形方块替换旧水墨点；依据后续方向反馈，最终为角色端细、箭头端粗。

## 表现

- 沿既有向上拱的二次曲线排列分离的梯形块，随距离调整数量，最多 8 块。
- 从角色端向箭头端逐渐变长、变粗；每块自身后窄前宽，与整体增长方向一致。
- 使用蓝青高光、深蓝切面、金边和深色轮廓，配合现有宝石箭头。
- 按曲线弧长取样，块间留白，并给箭头头部留出空间；极短距离不堆叠尾迹。
- 使用一批 Slate 自定义顶点绘制，不再加载或绘制十二张旧墨点纹理。旧资产保留。
- 沿用 viewport-client / SafeStage 本地坐标与箭尖热点，没有更改战斗结算或玩家资产。

## 验证入口

- 冷 UBT：`Saved/GemTrapezoidTrail/` 构建日志。
- 自动化：`GameXXK.MVP.Battle.TargetTrapezoidTrail`，以及已有 TargetArcUpward、TargetArrowAlignment、TargetPointerViewportCoordinates。
- 实际 BattleBoard 探针：`Content/Python/gamexxk_probe_gem_target_arrow.py`，第二参数 `GemTrapezoidTrail`，避免覆盖上一版截图。
- 探针在真实 PIE 控件中临时设置舞台本地指针并即时渲染；不结算卡牌，不代表鼠标实操录像。

## 验证结果

- `build-final.log`：冷 UBT 成功。前两次构建分别遇到可提交内存不足、其他任务编辑器占用 DLL；降低并发并禁用 UBA，待该编辑器退出后成功链接，未使用热加载。
- `automation-results.json`：上述四项测试 4/4 通过，无错误或警告，覆盖方向变化、间距、宽度递减、短距离留空与箭尖对齐。
- `Live/right.png`、`upper-right.png`、`left.png`：待角色与卡图异步载入完成后复拍并目视复核。右向与右上方向均为 8 块梯形尾迹，旧墨点消失；近距离反向指向自动省略尾迹，保留箭头。
- `cleanup.json`：测试会话已还原，PIE 已停止，脏包为 0；43 份既有玩家存档 SHA256 全部未变。

本轮验证仅覆盖指向表现，未重跑无关战斗数值回归或更新 Shipping 包。

## 方向修正

用户确认上一版拖尾方向相反后，反转尺寸插值与每块梯形的前后宽度：角色端细，箭头端粗。块数、弧线、间距及箭头本身保持原配置。

- `Saved/GemTrapezoidTrail/build-direction.log`：冷 UBT 成功。
- `Saved/GemTrapezoidDirection/automation-results.json`：调整后的递增断言与三项既有指向测试 4/4 通过。
- `Saved/GemTrapezoidDirection/Live/right.png`、`upper-right.png`：真实 PIE 控件即时渲染，已目视确认尺寸向箭头端增长。
- `Saved/GemTrapezoidDirection/cleanup.json`：会话还原、PIE 停止、脏包为 0，43 份玩家存档哈希未变。
