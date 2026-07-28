# 青山镇地形战斗场迁移设计

## 目标

把当前青山镇可玩地图中的一小块真实地形视觉复制到战斗场的地面上，使战斗画面延续城镇的地表、草地与土路风格；不复制整个城镇，也不覆盖任何用户手调的城镇、角色、镜头或 HD2D 资源。

## 已确认的范围

- 源地图为当前实际可玩城镇地图 `/Game/GameXXK/Maps/Prototype/L_Qingshan_AsianVillage_Demo`。
- 源对象是城镇 Landscape（`Landscape` / `Landscape_0`），而不是旧 `L_QingshanInn`、PCG 草图或普通静态地面网格。
- 目标是战斗图 `/Game/GameXXK/Maps/L_BattleScene` 中已存在、精确 Actor Label `GameXXK_Encounter_Floor` 的既有 StaticMeshActor 地面组件。
- 复制范围为一个约 `24m × 14m` 的平坦城镇地形切片；切片需要保留可辨识的高度、地表颜色与材质印象，但不迁移城镇建筑、NPC、关卡逻辑、草的运行时散布、Landscape LayerInfo 或 World Partition 状态。
- 战斗地图现有相机、Presenter、PlayerStart、光源、单位摆位与交互逻辑保持原位；只替换该地面组件所引用的网格/材质。
- 原先生成式河岸战斗背景只保留为未使用来源文件，不导入也不应用。它不能替代本设计的城镇地形来源。

## 为什么不用直接把 Landscape 材质赋给平面

城镇材质 `/Game/Asian_Village/materials/landscape_materials/MI_landscape_material_Inst` 依赖 Landscape 权重层、草输出与地形组件上下文。直接赋给一块平面会丢失层权重、坡度与草实例，既不能得到城镇观感，也会把复杂运行时依赖带进战斗图。因此采用“局部地形几何 + 局部烘焙材质”的自包含方式。

## 方案比较与选择

1. 直接复用 Landscape 材质：改动最少，但没有权重层和草输出，视觉不成立；排除。
2. 复制整张 Landscape：视觉最接近，但会把城镇关卡、LayerInfo、流送和草输出耦合进战斗图，违反不破坏用户资产的约束；排除。
3. 烘焙一个本地、只读来源的地形切片：将小范围高度转成静态网格，并烘焙同一范围的地表贴图，放入项目自有的战斗环境路径；所需资源最少、能锁住战斗性能和视觉范围，且可用严格快照验证源地图未被触碰；采用。

## 资源与命名

所有新资源放在 `/Game/GameXXK/Environment/Battle/TownTerrain`：

- `SM_Battle_QingshanGround_01`：24m × 14m 本地静态网格，保留选定切片的高度关系。
- `M_Battle_QingshanGround_01`：仅为该网格服务的材质。
- `T_Battle_QingshanGround_Albedo_01`、`T_Battle_QingshanGround_Normal_01`、`T_Battle_QingshanGround_ORM_01`：由城镇切片烘焙而来；若引擎能够安全地只烘焙底色，则只创建 Albedo，并在材质中明确使用稳定的常量粗糙度/法线，不伪造不存在的图层数据。
- `battle-town-terrain-manifest-v1.json`：记录源地图、Landscape 名称、切片世界坐标、源材质、输出资源、命令时间与文件哈希。

## 受保护执行协议

1. 在写入前对源地图和目标地图分别记录 SHA-256、指定 Actor 的精确 Actor Label/变换/组件引用快照。
2. 通过 UE 5.8 MCP 或项目 Python 脚本进行只读 Landscape 审计，确定没有建筑物、河流或陡坡的 24m × 14m 坐标范围。审计不能保存城镇地图；它只在 PIE 已停止、编辑器干净时临时切换地图，并在成功或普通失败后恢复原地图。若审计过程中出现任何意外脏包，则停止恢复操作并保留当前地图，以免丢弃用户工作。
3. 创建项目自有的网格、纹理和材质，且不改写 `/Game/Asian_Village/*` 的任何资产。
4. 只修改精确 Actor Label `GameXXK_Encounter_Floor` 的 StaticMesh/Material 引用；不改其 Transform，不改战斗相机、灯光、PlayerStart、Presenter 或单位。
5. 保存目标资源和战斗图；重新采集两个地图的快照。城镇二进制哈希、Landscape 组件数据和所有受保护 Actor 必须完全相同；战斗图差异仅允许地面组件的资源引用变化。
6. 使用冷编译、静态契约、PIE 主流程和战斗卡牌目标选择验证。任何验证失败都停止，不通过复制旧地图或直接手改城镇资产“补救”。

## 验收标准

- 战斗地面在 PIE 中呈现来自青山镇的土路/草地地表语言，不再是 `WorldGrid` 或裸灰色平面。
- 战斗地面与现有卡牌、角色、奖励和事件 UI 同屏时可读；不遮挡鼠标选目标、箭头或单位。
- 城镇地图的包哈希、Landscape 元数据、相机及用户手调内容在前后完全一致。
- 战斗地图除精确 Actor Label `GameXXK_Encounter_Floor` 的资源引用外，其余受保护 Actor 的 Transform 与组件状态一致。
- 冷编译、地形管线静态测试、战斗目标测试和真实 PIE 流程通过。

## 非目标

- 不重做城镇 Landscape，不更换城镇材质，不复制建筑和装饰。
- 不修改现有主角、伙伴、NPC 的 Sprite、PaperZD、镜头、HD2D Plane 和关卡摆放。
- 不在本轮创建新的可玩地图或切换主线地图路径。
