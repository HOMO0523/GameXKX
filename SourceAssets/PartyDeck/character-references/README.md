# PartyDeck 八方向角色素材准备

这份目录保存来源、规格、验证规则和已打包的角色图集；目前没有导入 UE、没有改动任何现有角色资产。

## 范围

“12 个新角色包”指 6 名固定职业伙伴原型加 6 名任务 NPC。主角是第 13 个参战主体，但已有完整的八方向 idle/walk 图集，因此只作为只读风格、比例和落脚点模板，绝不能覆盖或重画。

| 类别 | 稳定 ID | 来源 | 当前状态 |
| --- | --- | --- | --- |
| 主角 | `Hero.Player` | PPT `image31`、PSD `001`、现有 Hero 图集 | 已可复用，只读 |
| 任务 NPC | `Npc.TusiChief` / 土司首领 | PPT `image38`（第 7 页）、PSD `065` | 身份锁定，待八方向制作 |
| 任务 NPC | `Npc.SongJinBao` / 宋金宝 | PPT `image37`（第 7 页） | 身份锁定，待八方向制作 |
| 任务 NPC | `Npc.YueBai` / 月白 | PPT `image35`（第 6 页）、PSD `064` | 身份锁定，待八方向制作 |
| 任务 NPC | `Npc.ZhouGuangZu` / 周光祖 | PPT `image32`（第 5 页）、PSD `063` | 身份锁定，待八方向制作 |
| 任务 NPC | `Npc.JinGui` / 金贵 | PPT `image33`（第 6 页） | 身份锁定，待八方向制作 |
| 任务 NPC | `Npc.QiongMeiEr` / 琼么儿 | PPT `image34`（第 6 页） | 身份锁定，待八方向制作 |
| 永久伙伴 | `PartnerRole.Blade` / 刀客 | 无可复用身份图 | 待生成统一职业原型 |
| 永久伙伴 | `PartnerRole.Guard` / 护卫 | 无可复用身份图 | 待生成统一职业原型 |
| 永久伙伴 | `PartnerRole.Healer` / 医者 | 无可复用身份图 | 待生成统一职业原型 |
| 永久伙伴 | `PartnerRole.Hunter` / 猎手 | 无可复用身份图 | 待生成统一职业原型 |
| 永久伙伴 | `PartnerRole.Sorcerer` / 术士 | 无可复用身份图 | 待生成统一职业原型 |
| 永久伙伴 | `PartnerRole.FormationMaster` / 阵师 | 无可复用身份图 | 待生成统一职业原型 |

`clean_assets_v2/065.png` 的正确身份是 **土司首领**，不是司青娘。牛欢（PPT `image42`、PSD `066`）仅作事件 NPC；黑熊、老虎、金钱鼠均为敌方素材。它们、PPT `image36` 的村长，以及身份不明的 PSD `068` 都不能进入这 12 个角色包。

## 图集契约

- Idle：`171 × 1640`，8 行各一张 `171 × 205` 单元。
- Walk：`1026 × 1640`，8 行、每行 6 帧 `171 × 205` 单元。
- 行顺序固定：`S, SW, W, NW, N, NE, E, SE`。
- 引擎映射固定：`South, SouthWest, West, NorthWest, North, NorthEast, East, SouthEast`。
- 枢轴：bottom-center，`pixels_per_unreal_unit = 1`；纹理 nearest、无 mipmaps；walk 8 fps、idle 1 fps。
- 8 个方向必须有独立可读的造型，不能把正交方向镜像成对角方向；idle 不可拿 walk 第 0 帧替代。

每个职业只制作一个统一角色原型，供该职业的 4 个随机招募模板共用；不是为每个模板重做一个角色。职业卡色标分别是：刀客朱砂、护卫墨青、医者玉绿、猎手赭、术士靛、阵师莲灰紫。色标不允许反过来改色已有 PSD 框或已锁定 NPC 身份。

## 现有脚本的取舍

以下脚本已核对，但都是已有 Hero/Follower/Merchant 的只读行为范本，不能直接用于新角色：

- `Content/Python/gamexxk_assemble_npc_character_visuals.py`：证明了 `171×205`、bottom-center、6 帧和 flipbook 命名约定，但它引用旧素材并会替换已有纹理。
- `scripts/gamexxk_npc_character_visual_apply.py`：提供现有 idle 对齐的质量思路，但会镜像方向；新流程因“独立对角方向”要求不能复用其镜像步骤。
- `Content/Python/gamexxk_assemble_character_visuals.py` 与 `gamexxk_import_sprite_sources.py`：仅用于主角/旧 NPC 资产，禁止作为新角色的写入目标。

后续必须另建只写入 `/Game/GameXXK/Sprites/Generated/PartyDeck`、`/Game/GameXXK/Characters/PartyDeckNPC`、`/Game/GameXXK/Characters/PartyDeckPartners` 的导入脚本，并默认 `replace_existing=False`。

## 只读验证与可选来源提取

```powershell
python scripts/test_party_deck_sprite_manifest.py
python scripts/test_extract_party_deck_ppt_references.py
python scripts/verify_party_deck_sprite_sources.py --json
python scripts/extract_party_deck_ppt_references.py --json
```

最后一条默认只列出 6 个可提取的 PPT 原图。需要将经过哈希验证的副本写到本目录的 `ppt-extract/` 时，才显式执行：

```powershell
python scripts/extract_party_deck_ppt_references.py --write
```

## 本地八方向图集打包

`scripts/prepare_party_deck_sprite_atlas.py` 只接受 4×2 绿幕方向图，按固定行优先顺序
`S, SW, W, NW, N, NE, E, SE` 打包。它只允许写入本目录的 `packed/`，绝不镜像方向；输出会在落盘前后校验尺寸、透明通道、8 行和 walk 帧差异。已验证的月白源图与输出为：

- `generated/raw/npc_yue_bai_8dir_source_v1.png`
- `packed/npc_yue_bai_idle_8dir.png`
- `packed/npc_yue_bai_walk_8dir.png`

其他角色在拥有同规格的源图后，按相同命令替换输入与前缀即可：

```powershell
python scripts/prepare_party_deck_sprite_atlas.py `
  --input SourceAssets/PartyDeck/character-references/generated/raw/npc_yue_bai_8dir_source_v1.png `
  --output-dir SourceAssets/PartyDeck/character-references/packed `
  --prefix npc_yue_bai --json
```

输出文件已存在且内容变化时，脚本默认拒绝覆盖；仅在确认重打包后增加 `--replace-existing`。可用以下命令运行打包器回归测试：

```powershell
python scripts/test_party_deck_sprite_atlas_packer.py
```

下面的命令在其余角色素材尚未准备好时会返回非零：这是正常的“尚未满足全部角色图集门禁”，而不是来源数据错误。

```powershell
python scripts/verify_party_deck_sprite_sources.py --require-ready
```
