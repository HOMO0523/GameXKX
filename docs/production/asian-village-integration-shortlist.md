# Asian Village — Qingshan town shortlist

Review map: `/Game/GameXXK/Maps/Prototype/L_Qingshan_AsianVillage_Integration`.

The vendor demo map is intentionally not used as a gameplay map or a working scene. Loading the full UE 5.4 demo in the UE 5.8 editor forces its entire texture set and caused a UE world-memory-leak crash during initial review. The integration map instead loads only the 21 selected complete assets below.

| Town purpose | Selected vendor assets | Placement rule |
| --- | --- | --- |
| Main architectural silhouette | `SM_roof_03`, `SM_thatched_roof_01`, `SM_tower_01`, `SM_doorway_01`, `SM_wall_window_03` | Mix the three roof/body families; do not repeat a single house module as a row. |
| Northern approach / river crossing | `SM_bridge_01`, `SM_waterfall` | Bridge stays on the authored river crossing; QuickRoad approaches it with a low, terrain-conforming connection. |
| Market identity | `SM_market_tent_01`, `SM_lantern_01`, `SM_lamppost_01`, `SM_flag_banner_01`, `SM_table_01`, `SM_bamboo_box_01` | Cluster around market nodes, not evenly along every road. Lanterns and banners are reusable actor-blueprint candidates. |
| Vegetation | `SM_tree_01`, `SM_bamboo_01`, `SM_bush_01`, `SM_flowers_01` | All are 3D. Use PCG density falloff: heavy at cliffs/edge, sparse at building fronts and road shoulders. |
| Map boundary / terrain | `SM_cliff_01`, `SM_cliff_03`, `SM_stone_flower_bed_01`, `SM_wooden_fence_01` | Build a discontinuous ring of cliffs, bamboo and trees; avoid a uniform wall around the map. |

## Use order

1. Keep the existing QuickRoad and river layout as the structural source of truth.
2. Place the bridge and the largest roof/tower landmark first, then branch smaller house variants around it.
3. Add market clusters only at route decisions and building fronts.
4. Use PCG for repeated vegetation and small scatter only after roads and major building footprints are locked.
5. Apply UE 5.8 stylized BSDF overrides only to selected town assets; vendor materials remain untouched.

The 21 review actors use the `QingshanAsianVillageReview` tag and are not gameplay content. The vendor `Content/Asian_Village` directory remains local-only and ignored by Git.
