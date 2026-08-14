---
status: record
owner: codex
updated_at: 2026-07-28
source_commit: e3ebf6a0c7d12ade0ef73c2fe24ee1a8f939b996
---
# DRAFT-B02-EQUIPMENT 生产记录

日期：2026-07-22  
批次状态：`Draft Complete`（43/43）  
生产路径：内置 `image_gen`，每个 AssetId 独立一次调用；未使用 CLI/API 降级  
透明流程：洋红键控源图 → 官方 `remove_chroma_key.py` → `--edge-contract 1` → 等比居中到 1024×1024 RGBA  
终版状态：全部仍为 `final-approval-needed`；本记录只表示初版可用于后续功能绑定，不代表用户逐图终审通过

## 1. 批次构成与联系表

- 六套基础装备：6 套 × 6 部件 = 36 张。
- 套装徽记：6 张。
- 局外商店伙伴包：1 张。
- 完整联系表：`SourceArt/Generated/Draft/V1/DRAFT-B02-EQUIPMENT_contact_sheet.png`
  - SHA-256：`6A6D025ECBC3479E802EDBD3B0BB629B225D571E27A62234C7E7AB7D5B54CF5D`
- 31 张恢复检查点联系表：`SourceArt/Generated/Draft/V1/DRAFT-B02-EQUIPMENT_checkpoint31_contact_sheet.png`
  - SHA-256：`DAC8D4976040F312F3D3EA4387D6B818469B823D368C17B6ACEC8567301326C9`

主 Agent 已目检完整联系表：透明边缘、填充率、六套辨识度、六枚徽记与伙伴包在初版阶段可用。

## 2. 稳定文件名与计划 Draft UE 路径

每个基础名同时保留两个非覆盖文件：`<base>_draft_v1_chroma.png` 为生成记录，`<base>_draft_v1_alpha.png` 为项目绑定候选。**UE 绑定只允许使用 `*_alpha.png`，不得使用 chroma 源图。**

| AssetId | 工作区目录 / 基础名 | 计划 Draft UE 路径 |
|---|---|---|
| `EQUIP.POJUN.WEAPON` | `Equipment/pojun_weapon` | `/Game/GameXXK/Generated/Draft/V1/Equipment/T_Equipment_PoJun_Weapon` |
| `EQUIP.POJUN.HEAD` | `Equipment/pojun_head` | `/Game/GameXXK/Generated/Draft/V1/Equipment/T_Equipment_PoJun_Head` |
| `EQUIP.POJUN.ARMOR` | `Equipment/pojun_armor` | `/Game/GameXXK/Generated/Draft/V1/Equipment/T_Equipment_PoJun_Armor` |
| `EQUIP.POJUN.BELT` | `Equipment/pojun_belt` | `/Game/GameXXK/Generated/Draft/V1/Equipment/T_Equipment_PoJun_Belt` |
| `EQUIP.POJUN.SHOES` | `Equipment/pojun_shoes` | `/Game/GameXXK/Generated/Draft/V1/Equipment/T_Equipment_PoJun_Shoes` |
| `EQUIP.POJUN.ACCESSORY` | `Equipment/pojun_accessory` | `/Game/GameXXK/Generated/Draft/V1/Equipment/T_Equipment_PoJun_Accessory` |
| `EQUIP.XUANJIA.WEAPON` | `Equipment/xuanjia_weapon` | `/Game/GameXXK/Generated/Draft/V1/Equipment/T_Equipment_XuanJia_Weapon` |
| `EQUIP.XUANJIA.HEAD` | `Equipment/xuanjia_head` | `/Game/GameXXK/Generated/Draft/V1/Equipment/T_Equipment_XuanJia_Head` |
| `EQUIP.XUANJIA.ARMOR` | `Equipment/xuanjia_armor` | `/Game/GameXXK/Generated/Draft/V1/Equipment/T_Equipment_XuanJia_Armor` |
| `EQUIP.XUANJIA.BELT` | `Equipment/xuanjia_belt` | `/Game/GameXXK/Generated/Draft/V1/Equipment/T_Equipment_XuanJia_Belt` |
| `EQUIP.XUANJIA.SHOES` | `Equipment/xuanjia_shoes` | `/Game/GameXXK/Generated/Draft/V1/Equipment/T_Equipment_XuanJia_Shoes` |
| `EQUIP.XUANJIA.ACCESSORY` | `Equipment/xuanjia_accessory` | `/Game/GameXXK/Generated/Draft/V1/Equipment/T_Equipment_XuanJia_Accessory` |
| `EQUIP.QINGNANG.WEAPON` | `Equipment/qingnang_weapon` | `/Game/GameXXK/Generated/Draft/V1/Equipment/T_Equipment_QingNang_Weapon` |
| `EQUIP.QINGNANG.HEAD` | `Equipment/qingnang_head` | `/Game/GameXXK/Generated/Draft/V1/Equipment/T_Equipment_QingNang_Head` |
| `EQUIP.QINGNANG.ARMOR` | `Equipment/qingnang_armor` | `/Game/GameXXK/Generated/Draft/V1/Equipment/T_Equipment_QingNang_Armor` |
| `EQUIP.QINGNANG.BELT` | `Equipment/qingnang_belt` | `/Game/GameXXK/Generated/Draft/V1/Equipment/T_Equipment_QingNang_Belt` |
| `EQUIP.QINGNANG.SHOES` | `Equipment/qingnang_shoes` | `/Game/GameXXK/Generated/Draft/V1/Equipment/T_Equipment_QingNang_Shoes` |
| `EQUIP.QINGNANG.ACCESSORY` | `Equipment/qingnang_accessory` | `/Game/GameXXK/Generated/Draft/V1/Equipment/T_Equipment_QingNang_Accessory` |
| `EQUIP.ZHUIFENG.WEAPON` | `Equipment/zhuifeng_weapon` | `/Game/GameXXK/Generated/Draft/V1/Equipment/T_Equipment_ZhuiFeng_Weapon` |
| `EQUIP.ZHUIFENG.HEAD` | `Equipment/zhuifeng_head` | `/Game/GameXXK/Generated/Draft/V1/Equipment/T_Equipment_ZhuiFeng_Head` |
| `EQUIP.ZHUIFENG.ARMOR` | `Equipment/zhuifeng_armor` | `/Game/GameXXK/Generated/Draft/V1/Equipment/T_Equipment_ZhuiFeng_Armor` |
| `EQUIP.ZHUIFENG.BELT` | `Equipment/zhuifeng_belt` | `/Game/GameXXK/Generated/Draft/V1/Equipment/T_Equipment_ZhuiFeng_Belt` |
| `EQUIP.ZHUIFENG.SHOES` | `Equipment/zhuifeng_shoes` | `/Game/GameXXK/Generated/Draft/V1/Equipment/T_Equipment_ZhuiFeng_Shoes` |
| `EQUIP.ZHUIFENG.ACCESSORY` | `Equipment/zhuifeng_accessory` | `/Game/GameXXK/Generated/Draft/V1/Equipment/T_Equipment_ZhuiFeng_Accessory` |
| `EQUIP.SHIGU.WEAPON` | `Equipment/shigu_weapon` | `/Game/GameXXK/Generated/Draft/V1/Equipment/T_Equipment_ShiGu_Weapon` |
| `EQUIP.SHIGU.HEAD` | `Equipment/shigu_head` | `/Game/GameXXK/Generated/Draft/V1/Equipment/T_Equipment_ShiGu_Head` |
| `EQUIP.SHIGU.ARMOR` | `Equipment/shigu_armor` | `/Game/GameXXK/Generated/Draft/V1/Equipment/T_Equipment_ShiGu_Armor` |
| `EQUIP.SHIGU.BELT` | `Equipment/shigu_belt` | `/Game/GameXXK/Generated/Draft/V1/Equipment/T_Equipment_ShiGu_Belt` |
| `EQUIP.SHIGU.SHOES` | `Equipment/shigu_shoes` | `/Game/GameXXK/Generated/Draft/V1/Equipment/T_Equipment_ShiGu_Shoes` |
| `EQUIP.SHIGU.ACCESSORY` | `Equipment/shigu_accessory` | `/Game/GameXXK/Generated/Draft/V1/Equipment/T_Equipment_ShiGu_Accessory` |
| `EQUIP.SHANHE.WEAPON` | `Equipment/shanhe_weapon` | `/Game/GameXXK/Generated/Draft/V1/Equipment/T_Equipment_ShanHe_Weapon` |
| `EQUIP.SHANHE.HEAD` | `Equipment/shanhe_head` | `/Game/GameXXK/Generated/Draft/V1/Equipment/T_Equipment_ShanHe_Head` |
| `EQUIP.SHANHE.ARMOR` | `Equipment/shanhe_armor` | `/Game/GameXXK/Generated/Draft/V1/Equipment/T_Equipment_ShanHe_Armor` |
| `EQUIP.SHANHE.BELT` | `Equipment/shanhe_belt` | `/Game/GameXXK/Generated/Draft/V1/Equipment/T_Equipment_ShanHe_Belt` |
| `EQUIP.SHANHE.SHOES` | `Equipment/shanhe_shoes` | `/Game/GameXXK/Generated/Draft/V1/Equipment/T_Equipment_ShanHe_Shoes` |
| `EQUIP.SHANHE.ACCESSORY` | `Equipment/shanhe_accessory` | `/Game/GameXXK/Generated/Draft/V1/Equipment/T_Equipment_ShanHe_Accessory` |
| `EQUIP.SET_EMBLEM.POJUN` | `Equipment/SetEmblems/pojun_emblem` | `/Game/GameXXK/Generated/Draft/V1/Equipment/SetEmblems/T_EquipmentSet_PoJun` |
| `EQUIP.SET_EMBLEM.XUANJIA` | `Equipment/SetEmblems/xuanjia_emblem` | `/Game/GameXXK/Generated/Draft/V1/Equipment/SetEmblems/T_EquipmentSet_XuanJia` |
| `EQUIP.SET_EMBLEM.QINGNANG` | `Equipment/SetEmblems/qingnang_emblem` | `/Game/GameXXK/Generated/Draft/V1/Equipment/SetEmblems/T_EquipmentSet_QingNang` |
| `EQUIP.SET_EMBLEM.ZHUIFENG` | `Equipment/SetEmblems/zhuifeng_emblem` | `/Game/GameXXK/Generated/Draft/V1/Equipment/SetEmblems/T_EquipmentSet_ZhuiFeng` |
| `EQUIP.SET_EMBLEM.SHIGU` | `Equipment/SetEmblems/shigu_emblem` | `/Game/GameXXK/Generated/Draft/V1/Equipment/SetEmblems/T_EquipmentSet_ShiGu` |
| `EQUIP.SET_EMBLEM.SHANHE` | `Equipment/SetEmblems/shanhe_emblem` | `/Game/GameXXK/Generated/Draft/V1/Equipment/SetEmblems/T_EquipmentSet_ShanHe` |
| `SHOP.PACK.COMPANION` | `MetaShop/companion_pack` | `/Game/GameXXK/Generated/Draft/V1/MetaShop/T_MetaShop_CompanionPack` |

清单中的长期终版目标仍由 `2026-07-22-image-asset-production-manifest.md` 管理。本批没有导入 UE、没有创建或覆盖 UAsset、没有关闭编辑器。

## 3. Alpha / 尺寸 / 填充 QC

官方命令口径：

```text
python %CODEX_HOME%/skills/.system/imagegen/scripts/remove_chroma_key.py
  --input <source>
  --out <alpha.png>
  --auto-key border
  --soft-matte
  --transparent-threshold 12
  --opaque-threshold 220
  --despill
  --edge-contract 1
```

随后只对 alpha 输出进行确定性处理：清除 `alpha <= 8` 的亚像素噪声、清除无歧义洋红残键、使用预乘 alpha 的 Lanczos 等比缩放、居中到 1024×1024。主体最长包围盒轴统一为 900 px（87.9%）；窄长武器、腰带、护符和徽记保持原比例，不做非等比拉伸。

机器验收口径：RGBA；1024×1024；四角 alpha `0/0/0/0`；画布边缘无可见像素；可见像素中洋红数为 0（`alpha > 8, R > 180, B > 180, G < 100, R+B > 2G+160`）；chroma/alpha 基础名一一对应。

结果：`43/43 PASS`，`43/43` chroma/alpha 配对，失败清单为空。

| AssetId | 包围盒宽×高 | Alpha 覆盖 | Alpha SHA-256 | 结果 |
|---|---:|---:|---|---|
| `EQUIP.POJUN.WEAPON` | `82.6% × 87.9%` | `16.4%` | `94D509474C9D7A15BF691C5EE5A080A4CE49CC9D3A3EBC23931753DD2DFCC561` | PASS |
| `EQUIP.POJUN.HEAD` | `59.3% × 87.9%` | `33.3%` | `2E9DE9CBEFC7A3C8CA6733BAA045A2B7B96794DC28F4FE516FF58B0D155E421B` | PASS |
| `EQUIP.POJUN.ARMOR` | `73.3% × 87.9%` | `52.2%` | `8F013F5DF8CF916F98B35D68332356CE5BE8BAF2AC967B894224CC41D673D956` | PASS |
| `EQUIP.POJUN.BELT` | `87.9% × 73.4%` | `39.5%` | `CFC3C5C4D6FC98AA6B851DF4E55E19E66AA98D518F1D6E3DB5F0299240917753` | PASS |
| `EQUIP.POJUN.SHOES` | `69.9% × 87.9%` | `42.4%` | `5A38374CF6316C843B4A5ADDA773D3014E051B09F8E6854C46CD90A34E164C43` | PASS |
| `EQUIP.POJUN.ACCESSORY` | `60.7% × 87.9%` | `30.4%` | `26D0675EA49C30F989C0896C7AE249438EFFEE5077F6909727E7DBA513882E69` | PASS |
| `EQUIP.XUANJIA.WEAPON` | `73.6% × 87.9%` | `21.4%` | `9706EA90EF957BE27471710E5C470069B68E1D17A4C32EFA3231BFDA51082732` | PASS |
| `EQUIP.XUANJIA.HEAD` | `83.4% × 87.9%` | `51.8%` | `172F46A7CAFFF086C5199B70A66B14876261A13EA45D027E04BD7F73EFE2ACF8` | PASS |
| `EQUIP.XUANJIA.ARMOR` | `87.9% × 78.6%` | `49.6%` | `5ADF46574985358CF460D68085E488B705F6251076931F6A91F2519449445205` | PASS |
| `EQUIP.XUANJIA.BELT` | `87.9% × 69.4%` | `45.4%` | `10921F90A1CF487FCB013C65F4DC2F3A71B4A744D335F0590AFC0E1670FECF4F` | PASS |
| `EQUIP.XUANJIA.SHOES` | `78.8% × 87.9%` | `51.7%` | `FB89673BEAC4D4D244776A7BE0F70579B239ABAC65418A79955AE41FC11BE80D` | PASS |
| `EQUIP.XUANJIA.ACCESSORY` | `68.2% × 87.9%` | `33.0%` | `1E57564B4C8BB4E57ED8A3881389611AF064B10AC856406A99D2075E0CD496D6` | PASS |
| `EQUIP.QINGNANG.WEAPON` | `69.7% × 87.9%` | `15.1%` | `EB9595980A65BC569E896BB6E168261297C9EA3F3218684CA4BA1F4A5C46951B` | PASS |
| `EQUIP.QINGNANG.HEAD` | `87.9% × 78.9%` | `44.9%` | `E79E630C0F18441C4C59C2708FEC1FAE895AA71ADECFB92BE149B15866431D52` | PASS |
| `EQUIP.QINGNANG.ARMOR` | `82.6% × 87.9%` | `50.5%` | `F09E533209F0C22C4AD4504BC033687D26FC07890AFF6D8D12A5C6150FA46543` | PASS |
| `EQUIP.QINGNANG.BELT` | `87.9% × 81.5%` | `50.5%` | `E8F1D428FC6C5A9196B7470872D0CBC110EABB621F6408021CEC455E0CD0E3F8` | PASS |
| `EQUIP.QINGNANG.SHOES` | `87.9% × 80.3%` | `46.3%` | `324D8F503A6C6D8673C13018692ABCFE529CCC1539BB3D6D548FB66176955060` | PASS |
| `EQUIP.QINGNANG.ACCESSORY` | `59.9% × 87.9%` | `33.4%` | `7D2B92B9BB705178296809C1F1D28E102EC527703C679D6195D104D88B638150` | PASS |
| `EQUIP.ZHUIFENG.WEAPON` | `85.2% × 87.9%` | `10.3%` | `B37709505201266E21287E082787662B093ACA5B962798809F8F5CA989AD6B0B` | PASS |
| `EQUIP.ZHUIFENG.HEAD` | `72.6% × 87.9%` | `39.7%` | `51E675CE9ECCD6DE88BD88E49493D106778553594B78762BC58209AC6BB1034E` | PASS |
| `EQUIP.ZHUIFENG.ARMOR` | `82.4% × 87.9%` | `45.7%` | `E4C894ECA05E9E18F5777196C8ED9D1179114005B56EDF5CA3A1001BAF5DB013` | PASS |
| `EQUIP.ZHUIFENG.BELT` | `87.9% × 49.8%` | `26.6%` | `6F1D651B133189E1AD23369402F8E180AF419F8DED024D770750ACF77CC65464` | PASS |
| `EQUIP.ZHUIFENG.SHOES` | `87.9% × 71.8%` | `33.0%` | `46275AB7A97BF06AAFB98BEC29D48691F36746613D6FEA89AC0564FD2B580D1B` | PASS |
| `EQUIP.ZHUIFENG.ACCESSORY` | `62.9% × 87.9%` | `31.5%` | `F2DF595B020E789C9FC7F6F14EAA4A7F78055BB73F667AF135785244D814CBC1` | PASS |
| `EQUIP.SHIGU.WEAPON` | `86.7% × 87.9%` | `20.2%` | `B94B59B6B3EF14952FB697F646B97E19DD24219533EC1332F1622D28484B85CB` | PASS |
| `EQUIP.SHIGU.HEAD` | `87.9% × 78.1%` | `35.2%` | `7F8459E86D0E0D1E413EE6D94730652383B0052BFBE4CC0BB7C9C197C216B568` | PASS |
| `EQUIP.SHIGU.ARMOR` | `83.2% × 87.9%` | `50.9%` | `6993BE61E96474ED3E38175C22F120B64258BC9959BB43B901E281C7D5FEA83B` | PASS |
| `EQUIP.SHIGU.BELT` | `87.9% × 70.4%` | `48.2%` | `30C3030EE2B43B2066968A3029AAAE149D15F613DC0B3F8E294FBF2B49FD0948` | PASS |
| `EQUIP.SHIGU.SHOES` | `86.6% × 87.9%` | `51.7%` | `819C0D8D429561E4F29DFFB6515F1F6472473D426467EEE7713D38EF5C94B437` | PASS |
| `EQUIP.SHIGU.ACCESSORY` | `74.6% × 87.9%` | `35.3%` | `0932D5DB51B3DE834EA8B1EABF91508BC0FA6A48C565D44BA90B21A8A2A0F284` | PASS |
| `EQUIP.SHANHE.WEAPON` | `80.1% × 87.9%` | `13.8%` | `A17999E106260D6D0B47ECF7127079F5B4D9209B1C66D227A72B8C25E4AB95B2` | PASS |
| `EQUIP.SHANHE.HEAD` | `87.7% × 87.9%` | `47.2%` | `280D00080D532A76D53432639A0143DC98C6C04CE0F756AC0B8405383E05E825` | PASS |
| `EQUIP.SHANHE.ARMOR` | `87.9% × 84.2%` | `50.7%` | `ADBC8FB6BA83DC3168870CE1628CCFF274C3EFD9969ADB4F7F78A1A33EA151E8` | PASS |
| `EQUIP.SHANHE.BELT` | `87.9% × 78.5%` | `44.1%` | `3CC29F28B654C1D9C47077FA1DFEF50426FDB1A0477BDCF3349C915605C4523F` | PASS |
| `EQUIP.SHANHE.SHOES` | `87.9% × 77.1%` | `45.1%` | `319AE51B48D53D3F1E01D863533C5A08379AB540EB0A0C4A0BC44F6D34BBC891` | PASS |
| `EQUIP.SHANHE.ACCESSORY` | `54.6% × 87.9%` | `21.7%` | `7D7207EEA3D5375DFDFDA50F949522DF6409CB994888695AE91763C745C5EE9F` | PASS |
| `EQUIP.SET_EMBLEM.POJUN` | `38.9% × 87.9%` | `15.1%` | `F092432255A2116BE74FA5E05741E119A370F8E526129B704F985CD16FDDCC4B` | PASS |
| `EQUIP.SET_EMBLEM.XUANJIA` | `71.0% × 87.9%` | `44.5%` | `659EFB53A32C97580FDD5D96B494B5BAE0B32C92B8237BD050C6B14C97DAAFB4` | PASS |
| `EQUIP.SET_EMBLEM.QINGNANG` | `87.9% × 85.9%` | `41.1%` | `477BC6655E46E5A122E25BE05C7F57436AD28D3C8C11FAE07758CC2F5DC83E60` | PASS |
| `EQUIP.SET_EMBLEM.ZHUIFENG` | `77.5% × 87.9%` | `37.2%` | `BF7BBDEECF0F7B54F15230A9A671F041EA43623A9FCA25D7FD3FECE9AA69151E` | PASS |
| `EQUIP.SET_EMBLEM.SHIGU` | `60.4% × 87.9%` | `32.5%` | `E503F14D629942DEC0E6FF32DE43B1BEB0E7ACC413545BCCB4665BB082504A99` | PASS |
| `EQUIP.SET_EMBLEM.SHANHE` | `87.9% × 87.8%` | `60.3%` | `FD695B10F7D03668635372E4E14420FFEBE8D95C9E7A5379F199FABD287A0539` | PASS |
| `SHOP.PACK.COMPANION` | `87.9% × 72.3%` | `48.9%` | `F9315F3627857648749B2144BC0B803122EF49D184C05C5607C449F461C6457D` | PASS |

## 4. 提示词合同与差异

### 4.1 共享合同

```text
Use case: stylized-concept（套装徽记使用 logo-brand）
Asset type: GameXXK equipment inventory icon / equipment-set emblem / meta-shop companion-pack icon, initial Draft
Scene/backdrop: bright uniform magenta chroma-key field, approximately #ff00ff, intended for local removal.
Style/medium: simplified cute Chinese ink-and-light-watercolor game item matching the GameXXK layered PSD; low saturation, low contrast, clear hand-inked outer contour, compact chibi proportions, restrained pigment texture, minimal small detail.
Composition/framing: one complete centered item only; square icon; high fill with safe margin; readable at 72 px (emblems at 48 px).
Lighting/mood: flat graphic treatment.
Constraints: opaque subject; uniform chroma background; no magenta in subject; no shadow, gradient, texture, floor, text, numeral, stars, rarity glow, border, card frame, logo, watermark, photorealism, 3D render, shiny material, neon, unrelated objects.
```

前 31 张生成提示词要求主体约占画布宽高 84%–90%；阶段联系表确认后，剩余 12 张改写为“最长轴约 86%–90%”，以明确保留自然宽高比。最终所有 alpha 使用同一 87.9% 最长轴规范。

### 4.2 套装身份增量

| 套装 | 材质 / 色彩 | 轮廓与纹样身份 |
|---|---|---|
| `PoJun` | 暗铁、炭灰、克制绯红绑带 | 破浪缺口、虎牙、进攻型角线 |
| `XuanJia` | 黑铁、赭黄皮革 | 龟甲六边片、厚重守御轮廓 |
| `QingNang` | 米白、鼠尾草绿、竹木、淡陶 | 药葫芦、草叶、柔软治疗布装 |
| `ZhuiFeng` | 棕绿皮革、青灰布 | 风纹、扫羽、轻快流线 |
| `ShiGu` | 骨白、烟紫、毒绿、暗皮 | 骨笼、钩牙、毒滴与潜行绑带 |
| `ShanHe` | 暖灰、青蓝、哑玉 | 山峰、河流、云纹与均衡守护轮廓 |

### 4.3 部件与徽记增量

- `Weapon`：每套分别为破军战刀、玄甲方头锤、青囊药杖、追风反曲弓、蚀骨毒钩匕、山河直剑。
- `Head`：只画头部装备，不画头脸；分别使用突击盔、守御盔、医者软帽、轻兜帽、骨半面罩、山峰冠帽。
- `Armor`：只画衣甲躯干，不画人体；分别以斜片层甲、龟甲层甲、交领医袍、轻皮背心、肋骨皮甲、山水袍甲区分。
- `Belt`：盘卷成单一物件；分别使用虎牙扣、龟甲盾扣、药囊葫芦扣、风结扣、骨笼药瓶扣、河石扣。
- `Shoes`：一对鞋作为单一图标；分别为突击甲靴、重守靴、药师软靴、轻跑靴、骨钩潜行靴、山云行旅靴。
- `Accessory`：分别为虎符、龟甲护符、药葫芦、羽毛罗盘、骨笼毒瓶、山水玉佩。
- 六徽记：破军“裂枪头 + 虎牙”、玄甲“龟甲盾”、青囊“药葫芦 + 草叶”、追风“扫羽 + 风旋”、蚀骨“骨笼毒滴”、山河“山水玉盘”。
- 伙伴包：青绿色旅囊与卷铺组成一个物件，附木牌；木牌上恰有六个匿名头肩剪影，不绑定名字或具体职业装备。

## 5. 初版门禁与后续迭代

1. 本批只能作为功能开发的 Draft 输入；绑定时只使用 `*_draft_v1_alpha.png`。
2. chroma 文件是生成记录，必须继续保留且不得导入 UE。
3. 本批尚未执行 UE 导入；后续导入需在编辑器协调窗口中使用项目 MCP/`Content/Python`，并遵守主工程保存规则。
4. B01-B05 初版全部完成后再统一进行 Draft 绑定；本批完成不授权提前绑定终版目标。
5. 正式视觉迭代时，按总清单顺序一次只展示/修改一个 AssetId，用户明确通过后才能进入下一张。
6. 逐图终审可优先复查窄长的破军徽记、追风腰带/武器和细节较密的山河甲，但这些不构成当前初版阻塞项。
