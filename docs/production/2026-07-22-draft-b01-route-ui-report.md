# DRAFT-B01-ROUTE-UI 生产记录

日期：2026-07-22  
批次状态：`Draft Complete`（15/15）  
生产路径：内置 `image_gen`，每个 AssetId 独立一次调用；未使用 CLI/API 降级  
透明流程：纯 `#ff00ff` 键控源图 → 官方 `remove_chroma_key.py` → `--edge-contract 1` 复检 → 精确尺寸归一  
终版状态：全部仍为 `final-approval-needed`，本记录不代表用户终审通过

## 1. 锁定参考

- `D:/UE5 demo/GameXXK/outputs/UI_PSD/GameXXK_Town_4K.psd`  
  SHA-256：`7AF1E06A6E275F38255ECADAFDC99DB202453EC0BAA676F28FB481C8578A41BA`
- `D:/UE5 demo/GameXXK/SourceArt/UI/Battle/StatusIcons/battle_status_burn_flame_inkflat_v4.png`  
  SHA-256：`7428E39B15B5ED2D3CF216FDFBB637830A55F3C9C502F5081B5A1EF9018F83AB`
- `D:/UE5 demo/GameXXK/SourceArt/UI/Battle/StatusIcons/battle_status_mark_target_inkflat_v4.png`  
  SHA-256：`6D28B7A489B9FA488D3866337F620F2C90AFBD360F4DBEAA2F6BB7566632F9BB`

上述参考均只读，未被覆盖。联系表位于：

`D:/UE5 demo/GameXXK/SourceArt/Generated/Draft/V1/DRAFT-B01-ROUTE-UI_contact_sheet.png`

联系表 SHA-256：`C119EFDF5DF7EE44E10DAC857858E8BF8AB74E60C3EF4E37C63290EC5D99A6C5`

## 2. 稳定文件名与计划 UE 路径

| AssetId | 键控源图 | 项目绑定用 RGBA 初版 | 计划 UE 路径 |
|---|---|---|---|
| `ROUTE.MERCHANT.TRAVEL_MONEY` | `SourceArt/Generated/Draft/V1/RouteMerchant/route_merchant_travel_money_draft_v1_chroma.png` | `SourceArt/Generated/Draft/V1/RouteMerchant/route_merchant_travel_money_draft_v1_alpha.png` | `/Game/GameXXK/Generated/Draft/V1/RouteMerchant/T_RouteMerchant_TravelMoney` |
| `ROUTE.MERCHANT.PORTRAIT` | `SourceArt/Generated/Draft/V1/RouteMerchant/route_merchant_portrait_draft_v1_chroma.png` | `SourceArt/Generated/Draft/V1/RouteMerchant/route_merchant_portrait_draft_v1_alpha.png` | `/Game/GameXXK/Generated/Draft/V1/RouteMerchant/T_RouteMerchant_Portrait` |
| `ROUTE.MERCHANT.PAPER` | `SourceArt/Generated/Draft/V1/RouteMerchant/route_merchant_paper_draft_v1_chroma.png` | `SourceArt/Generated/Draft/V1/RouteMerchant/route_merchant_paper_draft_v1_alpha.png` | `/Game/GameXXK/Generated/Draft/V1/RouteMerchant/T_RouteMerchant_Paper` |
| `ROUTE.EVENT.MOUNTAIN_SPRING` | `SourceArt/Generated/Draft/V1/RouteEncounter/route_event_mountain_spring_draft_v1_chroma.png` | `SourceArt/Generated/Draft/V1/RouteEncounter/route_event_mountain_spring_draft_v1_alpha.png` | `/Game/GameXXK/Generated/Draft/V1/RouteEncounter/T_RouteEvent_MountainSpring` |
| `ROUTE.REWARD.CHEST_BAMBOO` | `SourceArt/Generated/Draft/V1/RouteEncounter/route_reward_chest_bamboo_draft_v1_chroma.png` | `SourceArt/Generated/Draft/V1/RouteEncounter/route_reward_chest_bamboo_draft_v1_alpha.png` | `/Game/GameXXK/Generated/Draft/V1/RouteEncounter/T_RouteReward_ChestBamboo` |
| `ROUTE.REWARD.CHEST_BRONZE` | `SourceArt/Generated/Draft/V1/RouteEncounter/route_reward_chest_bronze_draft_v1_chroma.png` | `SourceArt/Generated/Draft/V1/RouteEncounter/route_reward_chest_bronze_draft_v1_alpha.png` | `/Game/GameXXK/Generated/Draft/V1/RouteEncounter/T_RouteReward_ChestBronze` |
| `ROUTE.REWARD.CHEST_SHRINE` | `SourceArt/Generated/Draft/V1/RouteEncounter/route_reward_chest_shrine_draft_v1_chroma.png` | `SourceArt/Generated/Draft/V1/RouteEncounter/route_reward_chest_shrine_draft_v1_alpha.png` | `/Game/GameXXK/Generated/Draft/V1/RouteEncounter/T_RouteReward_ChestShrine` |
| `ROUTE.REWARD.CHEST_TRAVELLER` | `SourceArt/Generated/Draft/V1/RouteEncounter/route_reward_chest_traveller_draft_v1_chroma.png` | `SourceArt/Generated/Draft/V1/RouteEncounter/route_reward_chest_traveller_draft_v1_alpha.png` | `/Game/GameXXK/Generated/Draft/V1/RouteEncounter/T_RouteReward_ChestTraveller` |
| `STATUS.GLYPH.MEDICINE` | `SourceArt/Generated/Draft/V1/Status/status_glyph_medicine_draft_v1_chroma.png` | `SourceArt/Generated/Draft/V1/Status/status_glyph_medicine_draft_v1_alpha.png` | `/Game/GameXXK/Generated/Draft/V1/Status/T_BattleStatus_Medicine` |
| `STATUS.GLYPH.WEAK` | `SourceArt/Generated/Draft/V1/Status/status_glyph_weak_drooping_broken_blade_draft_v1_chroma.png` | `SourceArt/Generated/Draft/V1/Status/status_glyph_weak_drooping_broken_blade_draft_v1_alpha.png` | `/Game/GameXXK/Generated/Draft/V1/Status/T_BattleStatus_Weak` |
| `STATUS.GLYPH.WEALTH` | `SourceArt/Generated/Draft/V1/Status/status_glyph_wealth_square_coin_draft_v1_chroma.png` | `SourceArt/Generated/Draft/V1/Status/status_glyph_wealth_square_coin_draft_v1_alpha.png` | `/Game/GameXXK/Generated/Draft/V1/Status/T_BattleStatus_Wealth` |
| `STATUS.GLYPH.RAGE` | `SourceArt/Generated/Draft/V1/Status/status_glyph_rage_horn_flame_draft_v1_chroma.png` | `SourceArt/Generated/Draft/V1/Status/status_glyph_rage_horn_flame_draft_v1_alpha.png` | `/Game/GameXXK/Generated/Draft/V1/Status/T_BattleStatus_Rage` |
| `STATUS.GLYPH.PREY` | `SourceArt/Generated/Draft/V1/Status/status_glyph_prey_ink_target_draft_v1_chroma.png` | `SourceArt/Generated/Draft/V1/Status/status_glyph_prey_ink_target_draft_v1_alpha.png` | `/Game/GameXXK/Generated/Draft/V1/Status/T_BattleStatus_Prey` |
| `STATUS.GLYPH.CHARGE` | `SourceArt/Generated/Draft/V1/Status/status_glyph_charge_spiral_horn_draft_v1_chroma.png` | `SourceArt/Generated/Draft/V1/Status/status_glyph_charge_spiral_horn_draft_v1_alpha.png` | `/Game/GameXXK/Generated/Draft/V1/Status/T_BattleStatus_Charge` |
| `STATUS.GLYPH.COUNTER` | `SourceArt/Generated/Draft/V1/Status/status_glyph_counter_return_hook_blade_draft_v1_chroma.png` | `SourceArt/Generated/Draft/V1/Status/status_glyph_counter_return_hook_blade_draft_v1_alpha.png` | `/Game/GameXXK/Generated/Draft/V1/Status/T_BattleStatus_Counter` |

UE 导入需在编辑器协调窗口中通过项目 MCP/`Content/Python` 执行。本批只准备工作区源资产和导入清单，没有关闭编辑器、没有覆盖现有 UAsset、没有提前绑定终版路径。

## 3. Alpha / 尺寸 / 填充 QC

QC 口径：最终 PNG 必须是 RGBA；四角 alpha 全为 0；可见像素中洋红像素数为 0（检测阈值 `alpha > 8, R > 180, B > 180, G < 100`）；主体完整且不裁切。`填充` 为非零 alpha 包围盒相对画布的宽×高。

| AssetId | 最终尺寸 | 填充 | 四角 alpha | 可见洋红 | Alpha SHA-256 |
|---|---:|---:|---:|---:|---|
| `ROUTE.MERCHANT.TRAVEL_MONEY` | `512×512` | `74.0% × 80.1%` | `0/0/0/0` | `0` | `DC7F2FA0E4E4CA2F8E63767C0177DB81A6BAE63CCFE8BE2FA2EF2128A520A225` |
| `ROUTE.MERCHANT.PORTRAIT` | `1024×1024` | `65.9% × 87.4%` | `0/0/0/0` | `0` | `D23EB8A51722D82C35CF20629829D52D6D9D26BB9D899D4CE2B42A7105AD8305` |
| `ROUTE.MERCHANT.PAPER` | `1536×864` | `93.5% × 82.1%` | `0/0/0/0` | `0` | `EF208501AC74D8AD855FEF7CFBF86D5246FB23B05ED98F5FBA850B4697501323` |
| `ROUTE.EVENT.MOUNTAIN_SPRING` | `1024×1024` | `89.6% × 87.6%` | `0/0/0/0` | `0` | `79A5AE28C59718D54BCEC3BDF79F1870BEDF7A1AEE34F5620D05474D7416221A` |
| `ROUTE.REWARD.CHEST_BAMBOO` | `1024×1024` | `80.9% × 61.0%` | `0/0/0/0` | `0` | `0FC8B3CE7D944D3E0B9D73BB702A82BF7E958ABC63BF7694AA25B897D0AACFE2` |
| `ROUTE.REWARD.CHEST_BRONZE` | `1024×1024` | `81.7% × 64.6%` | `0/0/0/0` | `0` | `DFD8E3D60671FAEAAB84B035BEB5F90541E0D8E0FA38FE3792BA0848D9B3BCC9` |
| `ROUTE.REWARD.CHEST_SHRINE` | `1024×1024` | `79.1% × 81.0%` | `0/0/0/0` | `0` | `28A4C70A190B6A97EECDA0B4A24050A1EFAC6B1B8F61851F39146CF32C48E07B` |
| `ROUTE.REWARD.CHEST_TRAVELLER` | `1024×1024` | `82.4% × 79.2%` | `0/0/0/0` | `0` | `311A12C707A37BE6AC180741EE43DA7CAB61CCFFBDD3E0F693578ADCBF3D0D0B` |
| `STATUS.GLYPH.MEDICINE` | `512×512` | `83.0% × 87.5%` | `0/0/0/0` | `0` | `40751698E5569A04F06D7144F231799DD6AFF891A7CD030C9F993CDBEE58A823` |
| `STATUS.GLYPH.WEAK` | `512×512` | `83.6% × 82.8%` | `0/0/0/0` | `0` | `4DD1C921F86D622AC2B4F6ACF0B8DD45D97B33ADCF1FCE38763BB2A1E7467CA1` |
| `STATUS.GLYPH.WEALTH` | `512×512` | `83.0% × 84.2%` | `0/0/0/0` | `0` | `5F4ED876D3C6CFE8454A9EE1A8613BCA810FC3F76EE16716E892CD0169C1FF10` |
| `STATUS.GLYPH.RAGE` | `512×512` | `83.0% × 90.4%` | `0/0/0/0` | `0` | `EE20004B8CFA635DD7C907E759866CD265E5B5789050A1A040777FA81C660573` |
| `STATUS.GLYPH.PREY` | `512×512` | `85.7% × 83.6%` | `0/0/0/0` | `0` | `FC1A0EF13CFAC7CA49281A8A85F300C1A93226E771E35E7E8A357BC51B5A7FFA` |
| `STATUS.GLYPH.CHARGE` | `512×512` | `83.0% × 87.5%` | `0/0/0/0` | `0` | `76B0A5BD75B3AC01930CED0247AAD4CBFFA3E0535BB8576F8E7E55172B035A1E` |
| `STATUS.GLYPH.COUNTER` | `512×512` | `83.4% × 87.9%` | `0/0/0/0` | `0` | `6002A996045E4E4BA0AF7F43D193051B6F0E8B361857395CCF9F4AF09814AE54` |

结果：15/15 RGBA、15/15 尺寸正确、15/15 四角透明、15/15 可见洋红为零、7/7 状态字形达到 82%–94% 高方形填充率。横向箱体以自然宽高比保留完整轮廓，不做非等比拉伸。

视觉复检记录：`COUNTER` 初版轮廓偏钩镰，但“回钩返刃”语义可读，保留供功能开发；正式逐图迭代时优先减少农具感。`WEAK`、`MEDICINE`、`RAGE`、`CHARGE`、`WEALTH` 仅在透明终稿阶段做了确定性等比放大/居中，键控源图未改。

## 4. 最终提示词合同

以下“共享合同 + AssetId 增量”构成每张图的完整最终提示词记录；不需要隐含参数。所有图均使用 `Use case: stylized-concept`。

### 4.1 共享合同

```text
Asset type: GameXXK game UI illustration or battle status glyph, initial Draft
Scene/backdrop: perfectly flat solid #ff00ff chroma-key background for local background removal.
Style/medium: simplified cute Chinese ink-and-light-watercolor game art matching the approved GameXXK layered PSD; restrained hand-inked contour, low saturation, low contrast, compact chibi proportions where applicable, minimal high-frequency detail, extremely restrained paper-pigment texture only on the subject.
Lighting/mood: flat graphic treatment; no realistic studio lighting.
Constraints: the background must be one uniform #ff00ff with no shadow, gradient, texture, reflection, floor plane, or lighting variation; keep the complete subject separated from the background with crisp edges and safe padding; do not use #ff00ff in the subject; opaque subject; no cast shadow, contact shadow, reflection, text, number, logo, watermark, card frame, baked UI panel, photorealism, 3D render, pixel art, neon color, or unrelated props.
```

### 4.2 每图提示词增量

| AssetId | 拼接到共享合同中的完整增量 |
|---|---|
| `ROUTE.MERCHANT.TRAVEL_MONEY` | `Primary request: one compact route-run currency icon: a short bundle of three old Chinese square-hole coins threaded by a muted blue-gray travel cord with one tiny plain cloth knot; distinct from permanent gold currency. Subject/color: antique muted brass, blue-gray cord, warm gray-brown outline. Composition: centered single object, complete silhouette, 65%-88% square fill. Avoid shiny metal, complex tassels, extra loose coins.` |
| `ROUTE.MERCHANT.PORTRAIT` | `Primary request: a friendly travelling merchant, large head and compact body, straw hat, blue-gray short robe, cross-body satchel and rolled blanket; open welcoming hand, no stall. Subject/color: low-saturation blue-gray, warm tan and muted brown. Composition: centered full-body cutout, 55%-88% square fill. Avoid crowd, scenery, merchandise pile, exaggerated realism.` |
| `ROUTE.MERCHANT.PAPER` | `Primary request: one clean pale warm xuan-paper shop backing panel with a lightly irregular dark-ink outline; center must remain blank. Subject/color: warm ivory paper, faint beige wash, restrained charcoal border. Composition: 16:9 landscape, 1536x864 target, four-edge 72px nine-slice-safe border logic. Avoid text, buttons, slots, grids, ornaments, scenery, strong stains.` |
| `ROUTE.EVENT.MOUNTAIN_SPRING` | `Primary request: one compact positive-event illustration of a clear mountain spring flowing between two rounded rocks with a few bamboo leaves. Subject/color: warm gray stone, muted sage bamboo, pale blue-gray water. Composition: centered square vignette, complete silhouette, no full-screen landscape. Avoid people, buildings, text, misty background scene.` |
| `ROUTE.REWARD.CHEST_BAMBOO` | `Primary request: one closed bamboo-woven travel chest with a rounded lid and one simple dark clasp. Subject/color: low-saturation straw ochre and brown bindings. Composition: centered three-quarter view, broad readable square fill. Avoid open lid, treasure spilling out, glow, coins, scenery.` |
| `ROUTE.REWARD.CHEST_BRONZE` | `Primary request: one small closed old-bronze reward coffer with a rounded lid, simple bands and one plain square latch. Subject/color: muted verdigris bronze and dark brown. Composition: centered three-quarter view, broad readable square fill. Avoid gold shine, gems, open lid, treasure, glow, scenery.` |
| `ROUTE.REWARD.CHEST_SHRINE` | `Primary request: one portable small Chinese shrine/reward casket with a compact tiled eave, closed red-brown cabinet and one plain ring pull. Subject/color: muted brick red, charcoal roof and warm brown. Composition: centered front three-quarter view, simplified strong silhouette. Avoid temple scene, statues, incense smoke, text, open doors.` |
| `ROUTE.REWARD.CHEST_TRAVELLER` | `Primary request: one traveller's reward bundle combining a tied cloth bag, one rolled blue-gray bedroll and one small scroll as a single compact parcel. Subject/color: muted tan cloth, blue-gray roll, warm brown rope. Composition: centered single grouped object, broad square fill. Avoid person, loose scattered items, scenery, text.` |
| `STATUS.GLYPH.MEDICINE` | `Primary request: a single tied herb bundle made from three broad medicinal leaves. Subject/color: one flat muted sage-green ink color. Composition: centered bold silhouette, 82%-94% of canvas width and height, readable at 72px. Avoid flowers, bottle, mortar, extra herbs, thin lines.` |
| `STATUS.GLYPH.WEAK` | `Primary request: one drooping broken blade, angled downward, with a clear separated snapped tip. Subject/color: one flat muted blue-gray ink color. Composition: centered diagonal bold silhouette, 82%-94% square fill, readable at 72px. Avoid blood, second weapon, ornate guard, thin lines.` |
| `STATUS.GLYPH.WEALTH` | `Primary request: one ancient Chinese round coin with one square hole, unmistakable wealth symbol. Subject/color: one flat muted antique-bronze/warm-ochre color with gently irregular ink contour. Composition: front-facing centered single coin, 82%-94% square fill, readable at 72px. Avoid characters, engravings, multiple coins, metal glare.` |
| `STATUS.GLYPH.RAGE` | `Primary request: one bold horn-shaped flame whose two upper tongues curl outward like animal horns. Subject/color: one flat muted brick-red/burnt-sienna color. Composition: compact continuous centered silhouette, 82%-94% square fill, readable at 72px. Avoid ordinary teardrop flame, glow, sparks, smoke, thin lines.` |
| `STATUS.GLYPH.PREY` | `Primary request: one bold ink-circle target eye, made from three concentric imperfect brush rings and one solid central dot; no arrow. Subject/color: one flat muted dark plum-brown ink color. Composition: centered, thick widely spaced rings, 82%-94% square fill, readable at 72px. Avoid archery equipment, crosshair ticks, thin rings.` |
| `STATUS.GLYPH.CHARGE` | `Primary request: one powerful ram-like spiral horn in side view, one bold inward spiral with a sturdy blunt base and pointed tip; no animal head. Subject/color: one flat muted cool gray-blue ink color. Composition: centered broad silhouette with a simple internal spiral cut, 82%-94% square fill, readable at 72px. Avoid seashell, snail, tornado, ornate ridges.` |
| `STATUS.GLYPH.COUNTER` | `Primary request: one broad Chinese-style hooked blade curving back toward its origin like a returning counterattack stroke, with one compact grip. Subject/color: one flat muted charcoal-brown ink color. Composition: centered diagonal continuous weapon silhouette, 82%-94% square fill, readable at 72px. Avoid circular arrow, second weapon, ornate guard, thin lines.` |

## 5. 后续门禁

1. 本批可作为功能开发的 Draft 输入，但不得宣称终版通过。
2. 按总清单继续完成 B02-B05 的初版生成/导出/QC。
3. 全部初版完成后再统一绑定 Draft；不得提前绑定终版目标。
4. 正式迭代回到 B01 时，严格按清单顺序一次只展示/修改一个 AssetId，由主 Agent 转交用户明确通过后才能进入下一张。
5. 所有终审项通过后，才统一批量替换/导入终版。
