# 青山镇环境原画与 JSON 生产设计

日期：2026-07-10

状态：设计已根据用户方向收敛，等待书面规格复核

## 1. 目标

在任何新模型进入 Tripo、DCC 或 Unreal Engine 前，先完成青山镇核心环境资产的原画和结构化 JSON。原画必须继承现有项目的中国水墨卡通语言，并覆盖已确认的横向 S 形城镇、混合环山、实体建筑、桥、码头、山石、Paper2D 植物、地表和水体。

本阶段只生产：

- 风格锁定板。
- 资产原画与生产参考图。
- 每项资产的 JSON。
- 原画审核记录、文件哈希和来源记录。

本阶段不执行：

- Tripo 生成、重拓扑和贴图。
- UE 资产导入、关卡摆放和 PCG 修改。
- 正式 Sprite 切分、Atlas 打包和 Flipbook 创建。
- 对 `/Game/GameXXK/Maps/L_QingshanInn` 的任何保存。

## 2. 范围解释

“全部原画”采用套件级资产而不是每个散件一个资产任务。石头、木箱、草叶等以最多 2–3 个变体的套件表达，避免目录扩张到 50–80 个不可维护任务。

本规格包括：

- 6 个建筑登记项，其中已有 L-A 只登记现状，不重复生成单体原画。
- 3 个固定地标。
- 4 个山景项。
- 2 个岩石套件。
- 6 个 Paper2D 植物族。
- 5 个地表与水体族。
- 2 个线性套件。
- 3 个道具套件。
- 4 张全局风格与尺度锁定板。

共 31 个环境资产 JSON 和 4 个全局参考 JSON。所有 JSON 都在本阶段写好；所有核心原画都在风格锁定后分批生成。装饰套件可以生成原画，但它们的 3D/Paper2D 正式生产保持锁定，直到性能和画面空缺证明需要。

## 3. 目录与命名

项目源资产根目录：

```text
SourceAssets/TownPCG/QingshanEnvironment/
  manifest.json
  schema/
    qingshan_environment_asset.schema.json
  style/
    QS_InkToon_v1.json
    references/
    boards/
  assets/
    <ASSET_ID>/
      asset.json
      concepts/
      approved/
      source/
```

命名格式：

```text
<TYPE>_QS_<FAMILY>_<VARIANT>
```

合法 TYPE：

- `REF`：全局风格、尺度和镜头参考。
- `BLD`：建筑。
- `LMK`：固定地标。
- `ENV`：实体自然环境。
- `P2D`：Paper2D 环境素材。
- `SRF`：地表、水体和材质参考。
- `KIT`：线性模块套件。
- `PROP`：场景道具套件。

原画文件名：

```text
<ASSET_ID>__<VIEW_KIND>__v001.png
```

示例：

```text
BLD_QS_M_A_INN__hero_3q__v001.png
P2D_QS_GRASS_TUFT_A__wind_poses__v001.png
```

禁止覆盖已有版本。每个 `VIEW_KIND` 最多存在 `v001`、`v002`、`v003` 三个版本：`v001` 是初稿，后两版只能针对一个明确问题做受控修订。禁止自动生成 `v004`；两次修订后仍未通过时，资产进入 `blocked_needs_user_decision`。

## 4. Style Profile：QS_InkToon_v1

### 4.1 风格来源

以下用户提供文件作为风格参考，不作为编辑目标：

- `74a507717280d3a9e8f075e8a3d8201b.jpeg`：山水场景、角色与生物同框比例。
- `6cb6304fa60ec4084affc48be722555e.jpeg`：夜色山路、树冠和云雾层次。
- `b3e9c4c5f18ab38abfa6f49336b623c0.jpeg`：主角比例和简化轮廓。
- `b470ad572a2d2eb3959754c13d6cf0f1.jpeg`：人物、动物和自然物组合语言。
- `d848c66bef90df2589b2b81c7575cd22.jpeg`：夸张动物造型、暖色和墨线。
- `55bc66504b926d1917317b93c0a42f6b.jpeg`：大体块、低细节和深色轮廓。
- `ed2a4aca8dd0aac3628ef65c3b405348.jpg`：只参考高密度树冠布局，不参考其具体美术风格或色彩。

在正式生成前，将实际使用的稳定参考图复制到 `style/references/`，记录原路径、SHA-256 和用途。任何临时目录中的参考图都不得成为唯一来源。

### 4.2 视觉规则

- 轮廓使用深青灰、深褐或炭黑墨线，粗细有变化，局部允许断笔和干刷边缘。
- 形体以大轮廓和清楚剪影为先，避免写实小零件堆叠。
- 使用 2–3 个主要明暗层级，辅以少量水彩晕染，不使用照片级 PBR 高光。
- 主色为低饱和玉绿、青灰、蓝绿、靛蓝、暖赭、陶红和土黄。
- 白墙偏暖灰或米白，不使用纯白；植被不使用荧光绿。
- 建筑屋顶和木构允许局部不完全笔直，但门、入口和结构尺寸必须可信。
- 场景构图使用非对称组团、S 形动线、重叠树冠和层叠远山，禁止镜像和等距排列。
- 资产本体不生成文字、招牌字、Logo、水印或 UI；文字由 UE 独立材质或贴花完成。

### 4.3 视角规则

- 3D 模型参考图使用略俯视的三分之四视角，接近项目固定相机的高位 3/4 观察。
- 模型参考图必须展示完整轮廓、底部、入口方向和屋顶，不裁切主体。
- Tripo 输入图使用单一物件、干净浅暖背景、无投影、无地面场景和无附属杂物。
- Paper2D 概念图使用正视或为玩家相机校准的固定斜视，不使用透视夸张。
- 玩家镜头合成只用于尺度和美术审核，不直接输入 Tripo 或切 Sprite。

## 5. 通用 JSON 契约

每个 `asset.json` 必须包含：

```json
{
  "schema_version": 1,
  "asset_id": "BLD_QS_M_A_INN",
  "display_name": "青山镇中型客栈 A",
  "category": "building",
  "batch": "B1",
  "priority": "P0",
  "status": "concept_planned",
  "style_profile": "QS_InkToon_v1",
  "intended_zone": ["town_interior"],
  "gameplay_role": "street_node",
  "target_dimensions_m": [4.8, 5.6, 5.2],
  "pivot": "bottom_center",
  "forward_axis": "+Y",
  "palette": ["warm_off_white", "warm_red_brown", "dark_teal_ink"],
  "silhouette_keywords": ["two_storey", "asymmetric_xieshan_roof", "deep_eaves"],
  "material_language": ["ink_toon", "warm_plaster", "dark_timber"],
  "negative_prompt": ["photorealistic", "modern_signage", "mirror_symmetry"],
  "dependencies": ["REF_QS_ENV_STYLE_LOCK", "REF_QS_SCALE_LINEUP"],
  "source_provenance": {"style_profile": "QS_InkToon_v1", "source_kind": "project_concept"},
  "reference_images": [{"kind": "hero_3q", "approval_state": "planned", "file_stub": "BLD_QS_M_A_INN__hero_3q"}],
  "generation": {"use_case": "stylized-concept", "asset_type": "game_environment_asset_reference"},
  "unreal": {"asset_class": "StaticMesh", "nanite": true, "material_slots_max": 2},
  "pcg": {"allowed_zones": ["town_interior"], "excluded_zones": ["road", "river", "interaction"]},
  "workflow_gates": {
    "style_locked": false,
    "reference_approved": false,
    "concept_generation_allowed": false,
    "batch_unlocked": false,
    "previous_batch_approved": false,
    "batch_approval_id": null,
    "model_or_sprite_production_allowed": false,
    "tripo_allowed": false,
    "ue_import_allowed": false
  },
  "acceptance": {"silhouette_unique": false, "player_camera_readability": false}
}
```

正式 JSON 不允许保留未填写占位符、空数组或空对象。上例只说明结构。

### 5.1 reference_images

每个参考图条目包含：

```text
kind
camera
background
required_annotations
approval_state
file_stub
output_path
sha256
```

### 5.2 generation

所有图像生成使用内置 imagegen 路径。每个 JSON 保存完整提示词，并显式声明：

- use case：`stylized-concept`。
- asset type。
- 输入图片及其角色：style reference、scale reference 或 layout reference。
- scene/backdrop。
- subject。
- style/medium。
- composition/framing。
- lighting/mood。
- palette。
- constraints。
- avoid。

不同资产必须使用独立生成调用，不能用同一提示词的随机多图替代不同资产。

每个资产还必须写入固定生成预算：

```json
{
  "required_view_kinds": ["hero_3q", "structure_sheet"],
  "max_versions_per_view": 3,
  "max_generation_calls": 6,
  "generation_calls_used": 0,
  "blocked_after_revisions": 2
}
```

`required_view_kinds` 由资产分类固定，不允许在生成过程中临时新增视图。正交图、尺度图和材质细节优先从已批准主图进行受控派生，不得用无限随机候选替代修订。

### 5.3 workflow_gates

```json
{
  "style_locked": false,
  "reference_approved": false,
  "concept_generation_allowed": false,
  "batch_unlocked": false,
  "previous_batch_approved": false,
  "batch_approval_id": null,
  "model_or_sprite_production_allowed": false,
  "tripo_allowed": false,
  "ue_import_allowed": false
}
```

`concept_generation_allowed` 必须由批次状态计算，不得手工绕过：Batch 0 初始解锁；其它批次只有在 `batch_unlocked=true` 且前一批存在有效 `batch_approval_id` 时才为 `true`。风格板未批准时，只允许生成和修订 Batch 0。Batch 0 批准后只解锁 Batch 1；Batch 1 的全部必需视图通过后才解锁 Batch 2；Batch 2 通过后才解锁 Batch 3。单项参考图未批准时不得进入 Tripo、Sprite 切分或 UE。

### 5.4 分类固定视图

| 分类 | required_view_kinds | 每项最大调用数 |
|---|---|---:|
| 全局 REF | `board` | 3 |
| 已有 L-A 登记 | `existing_asset_registry` | 0 |
| 新建筑 | `hero_3q`、`structure_sheet` | 6 |
| 固定地标 | `route_3q`、`structure_connection_sheet` | 6 |
| 近山模块 | `module_lineup`、`assembly_player_scale` | 6 |
| 远山 Paper2D | `silhouette_layers`、`player_camera_composite` | 6 |
| 岩石套件 | `variant_lineup`、`silhouette_scale` | 6 |
| Paper2D 植物 | `neutral_variants`、`silhouette_scale_cluster`、`wind_poses` | 9 |
| 地表与水体 | `seamless_transition_sheet`、`player_camera_material_sheet` | 6 |
| 线性/道具套件 | `variant_lineup`、`usage_scale_sheet` | 6 |

Manifest 还必须限制整个批次的总调用数：Batch 0 最多 12 次、Batch 1 最多 48 次、Batch 2 最多 52 次、Batch 3 最多 20 次。达到批次上限时停止自动生成并请求用户裁决，即使个别视图仍有剩余版本额度也不得越过批次上限。

## 6. 资产目录

### 6.1 建筑

| Asset ID | 描述 | 原画要求 | 后续预算 |
|---|---|---|---|
| `BLD_QS_L_A_MARKET_SHOP` | 已有最大商铺，只登记现状 | 纳入尺度板，不重新生成独立 Tripo 图 | 约 51k 四边面、4K |
| `BLD_QS_M_A_INN` | 两层中型客栈，暖红褐歇山顶 | `hero_3q`；`structure_sheet` 内含入口正面、侧背、顶视占地、尺度和材质分区 | 30–35k、2K |
| `BLD_QS_M_B_STREET_SHOP` | 一层半沿街商铺，墨绿深檐 | `hero_3q`；同结构面板构成的 `structure_sheet` | 25–30k、2K |
| `BLD_QS_S_A_HOUSE` | 靛蓝双坡小民居 | `hero_3q`；同结构面板构成的 `structure_sheet` | 15–20k、2K |
| `BLD_QS_S_B_WORKSHOP` | 赭黄硬山顶和侧棚 | `hero_3q`；同结构面板构成的 `structure_sheet` | 12–18k、2K |
| `BLD_QS_S_C_RIVER_HUT` | 青绿色低檐河岸小屋 | `hero_3q`；同结构面板构成的 `structure_sheet` | 15–20k、2K |

建筑 JSON 增加：

```text
size_class
use
footprint_m
entrance_axis
roof.form
roof.primary_color
eave_height_m
door_socket
retopo_target_quads
texture_resolution
simple_collision_required
allowed_cluster_roles
```

### 6.2 固定地标

| Asset ID | 描述 | 核心约束 |
|---|---|---|
| `LMK_QS_GATE_NORTH` | 北门视觉建筑 | 与 `QingshanInn_TownExit` 逻辑解耦，不移动现有锚点；谷口净宽 18–24m |
| `LMK_QS_BRIDGE_MAIN` | 约 9m 宽主桥 | 桥面净宽、桥头连接、河床净空和简单碰撞必须明确 |
| `LMK_QS_DOCK_SOUTH` | 南岸码头主平台 | 只做主平台、桩、台阶和栏杆，不扩张船只系统 |

地标原画固定为两张：`route_3q` 展示玩家路线主视图；`structure_connection_sheet` 在一张分面板图中展示正侧结构、顶视通行尺寸、连接接口和材质分区。

### 6.3 混合环山与岩石

| Asset ID | 类型 | 变体上限 |
|---|---|---:|
| `ENV_QS_MTN_NEAR_A_RIDGE` | 近景实体长脊和缓坡模块 | 2 |
| `ENV_QS_MTN_NEAR_B_CLIFF` | 近景实体陡壁和转角模块 | 2 |
| `P2D_QS_MTN_FAR_A_MIST` | 第一景深雾山带 | 2 |
| `P2D_QS_MTN_FAR_B_SILHOUETTE` | 第二远景剪影层 | 2 |
| `ENV_QS_ROCK_KIT_A_FIELD` | 普通山石 S/M/L | 3 |
| `ENV_QS_ROCK_KIT_B_RIVERBANK` | 河岸湿石 S/M/L | 3 |

环山构图固定为：屏幕上方主远山偏左、屏幕左侧实体山脚较强、屏幕右侧保留北门谷口、屏幕下方只在两角放低岩肩，中间保持开阔。

### 6.4 Paper2D 植物

| Asset ID | 类型 | A 轮廓首版风摆帧数 |
|---|---|---|
| `P2D_QS_TREE_PINE_A` | 松树和松冠 | 5，主要末端叶团变化 |
| `P2D_QS_TREE_BROADLEAF_A` | 阔叶树和树冠 | 5，树干固定 |
| `P2D_QS_TREE_BAMBOO_A` | 竹丛 | 5，顶部 4–7px 摆动 |
| `P2D_QS_SHRUB_A` | 灌木 | 5，根部固定 |
| `P2D_QS_GRASS_TUFT_A` | 草簇 | 4，顶部 2–3px 摆动 |
| `P2D_QS_FLOWER_WILD_A` | 野花簇 | 4，花茎小幅摆动 |

每个植物族固定 3 个轮廓变体 A/B/C。只有 A 轮廓制作风摆；B/C 保持静态。树、竹和灌木的 A 轮廓固定 5 个姿态：中立、左小摆、左大摆、右小摆、右大摆。草和野花的 A 轮廓固定 4 个姿态：中立、左摆、右摆、轻微回稳。首批 Atlas 上限为 40 个 Cell：四个 5 帧 A 轮廓共 20 Cell、两个 4 帧 A 轮廓共 8 Cell、六族 B/C 静态轮廓共 12 Cell。不得为 B/C 临时追加动画。

原画固定包含：`neutral_variants` 单体 A/B/C 表、`silhouette_scale_cluster` 黑色剪影、项目角色/建筑尺度标尺与 3–5 株组团表、`wind_poses` 仅 A 轮廓的风摆姿态表。

植物 JSON 增加：

```text
source_canvas_px
sprite_cell_px
pixels_per_unreal_unit
pivot_mode
card_world_size_m
facing_mode
depth_layer
atlas_group
wind_frames
animated_variants
atlas_cell_count
masked_material
cluster_size
spacing_m
cull_distance_m
collision
ground_trace_required
```

`wind_frames` 必须是上表规定的整数 5 或 4；`animated_variants` 固定为 `["A"]`；`atlas_cell_count` 必须包含 A 动画帧和 B/C 两个静态 Cell，不能写成文字计划或范围。

概念图阶段使用浅暖纯色背景。正式 Sprite 生产阶段另行使用纯洋红色键控背景，因为植物主体包含大量绿色；背景移除和 Alpha 验证不在本阶段执行。

### 6.5 地表、水体和河岸

| Asset ID | 内容 |
|---|---|
| `SRF_QS_GROUND_EARTH_A` | 镇内泥土地表 |
| `SRF_QS_GROUND_GRASS_A` | 外围草地表面，不是草株 |
| `SRF_QS_ROAD_STONE_A` | 主街和小径石土路，可参考但不得直接复制 QuickRoad 材质 |
| `SRF_QS_WATER_RIVER_A` | 河面 Toon 水、流向和泡沫 |
| `SRF_QS_RIVERBANK_BLEND_A` | 湿岸、泥岸、碎石岸混合带 |

表面原画固定为两张：`seamless_transition_sheet` 合并无透视平铺样张与交界过渡条；`player_camera_material_sheet` 合并斜面材质效果和玩家相机 5–20m 样张。水体的同名两张 Sheet 还必须包含流向、岸线、深浅色和泡沫层。

### 6.6 线性套件与道具

| Asset ID | 变体 |
|---|---|
| `KIT_QS_FENCE_WOOD_A` | 直段、转角、门洞 |
| `KIT_QS_PATH_STEPPING_STONE_A` | 直、弯、散点 |
| `PROP_QS_MARKET_KIT_A` | 摊架、招牌框、筐 |
| `PROP_QS_DOCK_KIT_A` | 系缆柱、绳卷、木箱 |
| `PROP_QS_STREET_KIT_A` | 灯笼架、长凳、路牌框 |

每套最多 3 个变体。原画可以在本阶段生成，但正式模型或 Sprite 生产保持锁定，直到核心场景完成性能和空旷度验收。

## 7. 生成批次

### Registry：已有资产登记

- `BLD_QS_L_A_MARKET_SHOP`

该项的 `batch` 固定为 `REGISTRY`，`required_view_kinds` 为 `existing_asset_registry`，生成调用预算为 0。它使用现有模型、材质、贴图、源 ZIP、生产记录和 Batch 0 尺度板完成登记，不计入 Batch 1 的 12 个新原画项。

### Batch 0：风格锁定

必须先生成并批准四张板：

1. `REF_QS_ENV_STYLE_LOCK`：整体水墨 Toon、色板、线条和材质语言。
2. `REF_QS_SCALE_LINEUP`：角色、L/M/S 建筑、树、桥和山体同框比例。
3. `REF_QS_CAMERA_ROUTE`：北门、集市、桥、码头四个玩家镜头。
4. `REF_QS_SURFACE_PALETTE`：路、土、草、河岸、水和岩石关系。

四张板都通过后，`style_locked` 才能设为 `true`。

Batch 0 Manifest 的 `max_generation_calls` 为 12。四张板全部通过后记录 `batch_approval_id`，只解锁 Batch 1。

### Batch 1：最小完整镜头链

12 个核心项：

- `BLD_QS_M_A_INN`
- `BLD_QS_S_A_HOUSE`
- `LMK_QS_GATE_NORTH`
- `LMK_QS_BRIDGE_MAIN`
- `LMK_QS_DOCK_SOUTH`
- `ENV_QS_MTN_NEAR_A_RIDGE`
- `P2D_QS_MTN_FAR_A_MIST`
- `ENV_QS_ROCK_KIT_A_FIELD`
- `P2D_QS_TREE_PINE_A`
- `P2D_QS_SHRUB_A`
- `SRF_QS_GROUND_EARTH_A`
- `SRF_QS_WATER_RIVER_A`

Batch 1 Manifest 的 `max_generation_calls` 为 48。12 项的全部 `required_view_kinds` 均通过后记录批次审批，才允许解锁 Batch 2。

### Batch 2：补全核心资产

13 个项目：

- `BLD_QS_M_B_STREET_SHOP`
- `BLD_QS_S_B_WORKSHOP`
- `BLD_QS_S_C_RIVER_HUT`
- `ENV_QS_MTN_NEAR_B_CLIFF`
- `P2D_QS_MTN_FAR_B_SILHOUETTE`
- `ENV_QS_ROCK_KIT_B_RIVERBANK`
- `P2D_QS_TREE_BROADLEAF_A`
- `P2D_QS_TREE_BAMBOO_A`
- `P2D_QS_GRASS_TUFT_A`
- `P2D_QS_FLOWER_WILD_A`
- `SRF_QS_GROUND_GRASS_A`
- `SRF_QS_ROAD_STONE_A`
- `SRF_QS_RIVERBANK_BLEND_A`

Batch 2 Manifest 的 `max_generation_calls` 为 52。13 项的全部 `required_view_kinds` 均通过后记录批次审批，才允许解锁 Batch 3。

### Batch 3：装饰原画

5 个套件：

- `KIT_QS_FENCE_WOOD_A`
- `KIT_QS_PATH_STEPPING_STONE_A`
- `PROP_QS_MARKET_KIT_A`
- `PROP_QS_DOCK_KIT_A`
- `PROP_QS_STREET_KIT_A`

本阶段允许生成它们的套件原画和 JSON，但不解锁后续模型生产。禁止新增船、第二座桥、季节植物、动物、农田、瀑布或第二套城门。

Batch 3 Manifest 的 `max_generation_calls` 为 20。达到上限或任何必需视图在两次受控修订后仍失败时，整批停止自动生成并等待用户裁决。

## 8. 图像生成策略

### 8.1 参考图角色

所有输入图片必须在提示中标注角色：

- 风格参考：只学习线条、色块、轮廓和气氛。
- 尺度参考：只学习人物与建筑、树和道具的相对大小。
- 布局参考：只学习构图关系，不复制画面内容。
- 现有资产参考：只锁定已经批准的建筑语言和材质方向。

### 8.2 单资产与套件

- 建筑和固定地标使用单资产独立图，不使用多物件联系表作为 Tripo 输入。
- 岩石、围栏和小道具允许一张套件板展示最多 3 个变体。
- Paper2D 植物每个族独立生成，不能在一个图里混合六种植物后直接切分。
- 风摆关键帧先锁定中立姿态，再用受控编辑生成左右姿态；禁止每帧重新独立生成。

### 8.3 背景策略

- 风格板和玩家镜头合成允许完整场景背景。
- Tripo 单体图使用浅暖纯色背景，无地面、无阴影、无雾和无附属场景。
- Paper2D 概念图使用浅暖纯色背景。
- Paper2D 正式生产图以后使用纯洋红键控背景，再通过本地工具生成 Alpha；若复杂树冠无法可靠去色，再单独申请原生透明 CLI 回退，不在本阶段擅自降级模型。

## 9. 审核与失败回退

每张原画至少检查：

- 是否符合 `QS_InkToon_v1` 的墨线、色阶和低饱和色板。
- 剪影是否清楚且与同类资产不同。
- 是否保持项目相机的高位 3/4 阅读方向。
- 是否出现错误文字、水印、Logo、现代物件或无关装饰。
- 建筑入口、桥面、码头接口和资产底部是否完整可见。
- Paper2D 根部或树干是否保持稳定。
- 套件变体是否不超过上限。

若失败：

1. 只修改一个明确问题并生成新版本。
2. 不覆盖失败图；标记 `rejected` 并记录原因。
3. 每个视图最多允许初稿加两次受控修订；第二次修订仍失败时设为 `blocked_needs_user_decision`。
4. 当前规格执行期禁止生成 `v004`；用户只能裁决为修改规格并建立新的审批预算，或接受现有版本。
5. 同一资产连续两版仍偏离风格时，返回 Batch 0 对照 Style Profile，而不是继续随机重抽。
6. 未批准图不得写入 `approved/`，不得设 `reference_approved=true`。

## 10. 完成本阶段的验收标准

- 35 个 JSON 文件通过 Schema 校验：1 个 `REGISTRY` 环境登记项、Batch 1 的 12 项、Batch 2 的 13 项、Batch 3 的 5 项，以及 Batch 0 的 4 个全局参考。
- Batch 0 四张风格板全部批准并记录 SHA-256。
- Batch 1、Batch 2 和 Batch 3 的要求原画均有文件、版本、审核状态和哈希。
- 五个新建筑各有独立、干净、可用于后续 Tripo 的主 3/4 参考图。
- 北门、桥和码头的结构与通行关系在原画中可读。
- 环山同时包含实体近山和 Paper2D 远山，北门谷口保持可见。
- 六个植物族具有独立轮廓、尺度图、组团图和受控风摆计划。
- 地表、水体与河岸色板在同一玩家相机样张中协调。
- 所有原画保持无文字、无水印、无现代元素、无镜像长排。
- 所有 `tripo_allowed`、`model_or_sprite_production_allowed` 和 `ue_import_allowed` 仍为 `false`，直到用户完成原画审核。

## 11. 后续转换条件

用户批准本书面规格后，下一步先创建实现计划和 JSON/目录骨架，再生成 Batch 0 四张风格板。用户确认 Batch 0 后只生成 Batch 1；Batch 1 全部必需视图通过后才生成 Batch 2；Batch 2 通过后才生成 Batch 3。所有原画完成并通过用户审核后，再为建筑、地标和自然资产分别建立 Tripo/DCC、Paper2D 和 UE 导入计划。
