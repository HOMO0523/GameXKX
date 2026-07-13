# 玩家遮挡材质：前景深度门设计

## 背景与替代范围

现有运行时方案会对相机到角色的射线命中组件临时替换为项目自有 `Cutout` 材质。该材质已经保留原始 Base Color、Normal、WPO 和已有 Opacity/Opacity Mask，但圆形开口只根据 `ScreenPosition` 与角色的屏幕坐标判定。因此一个跨越角色深度的屋檐、墙体或树冠，可能在角色身后的像素也被误裁剪。

本设计只替换 Cutout 材质中的“圆形保留值”公式；不改变角色、相机、场景捕获、后处理、放置的关卡资源或原始 Asian Village 资产。

## 目标

当角色被遮挡时，只有同时满足以下两项的遮挡材质像素退让：

1. 像素落在角色周围的屏幕圆形范围内；
2. 像素位于相机到角色之间，即它的相机前向深度小于角色的相机前向深度。

角色身后的建筑、树木和天空必须保持正常显示，即使与角色落在同一屏幕圆内。原有屋檐的镂空、瓦片 Mask、半透明叶片及对应投影逻辑必须继续由其原始材质决定。

## 设计

### 参数数据流

`UGameXXKPlayerOcclusionRevealComponent` 每个遮挡采样周期更新同一个 `MPC_PlayerOcclusion`：

- `OcclusionCenter`：角色投影后的 viewport UV；
- `OcclusionAspect`：当前视口宽高比；
- `OcclusionRadius`：圆形半径；
- `OcclusionCameraLocation`：当前 `PlayerCameraManager` 的世界坐标；
- `OcclusionCameraForward`：当前相机由近到远的单位前向向量；
- `OcclusionHeroViewDepth`：`dot(HeroVisualCenter - CameraLocation, CameraForward)`；
- `OcclusionDepthBias`：一个很小的正偏移，防止与角色平面相同的表面闪烁。

参数只在 Town 画面、且按现有 20Hz 遮挡采样更新；清除遮挡时不再交换组件材质，并恢复原槽位。

### Cutout 材质公式

对每个被临时替换的项目自有 Cutout 根材质，在已有的屏幕圆形公式外增加：

```text
PixelViewDepth = dot(AbsoluteWorldPosition - OcclusionCameraLocation,
                     OcclusionCameraForward)
IsForeground = PixelViewDepth < OcclusionHeroViewDepth - OcclusionDepthBias
Reveal = CircleMask × IsForeground
KeepMask = 1 - Reveal
```

最终输出规则：

- 原 Opaque 材质的项目副本：`OpacityMask = KeepMask`；副本仅为实现局部裁剪而转为 Masked。
- 原 Masked 材质：`OpacityMask = OriginalOpacityMask × KeepMask`。
- 原 Translucent 材质：`Opacity = OriginalOpacity × KeepMask`；保持 Translucent。

这使圆形范围只成为横向选择，而深度门决定像素是否处于角色前景。材质自身的原有 Mask 在乘法链中始终位于 `KeepMask` 之前，不被覆盖或重建。

### 明确不采用的方案

- 不使用 SceneCapture、RenderTarget 或 Post Process 重绘角色/场景；它们会引入画面比例、颜色、分辨率或室内空洞问题。
- 不使用复制的角色 Paper2D 层；角色仍由主相机正常渲染。
- 不隐藏或淡出整栋建筑组件；仅退让满足深度条件的像素。

## 试点与验收

先只重生成一个屋檐/墙体材质族并验证：

1. 相机、角色、屋檐、角色身后建筑在同一视线中；圆内屋檐前景像素消失，身后建筑保持可见。
2. 让一个单组件同时跨越角色前后深度；角色后的同组件部分不出现额外圆洞。
3. 原本已有镂空或 Mask 的屋檐，其镂空和轮廓在圆外完全不变；圆内仅增加前景退让。
4. 清除遮挡、离开 Town 与 PIE EndPlay 后，材质槽指针逐一恢复为原始资产。
5. 冷编译与现有 Town 自动化测试通过；不使用 Live Coding 或 Hot Reload。

## 性能与失败处理

新增的是每个已替换材质像素的一次点积比较和少量 MPC 读取，不新增 SceneCapture、RenderTarget 或逐帧材质交换。若材质缺失、参数集合不可用或映射不存在，组件保持原始材质，并记录一次可定位诊断；不得回退到角色复制层或全组件隐藏。
