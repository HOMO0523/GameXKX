# 宝石风格战斗指向箭头

使用内置 `image_gen` 生成。最终源文件 `T_BattleTargetArrowHead_GemV1_Keyed.png` 保留生成器输出原始字节；尺寸 1254×1254，RGB。生成器没有返回实际 Alpha，因此以纯品红底配合 UE UI 材质实现透明显示，不将棋盘格图伪称为透明 PNG。

## 运行时资产

- `/Game/GameXXK/UI/Battle/Textures/T_BattleTargetArrowHead_GemV1`
- `/Game/GameXXK/UI/Battle/Materials/M_BattleTargetArrowHead_GemV1`
- 导入入口：`Content/Python/gamexxk_import_gem_target_arrow.py`。
- UI 纹理构建为 512×512；绘制框 132×132，源图保持原比例。
- 根据实际去底规则测得尖端 (1148.5,625)，归一化见 `manifest.json`。四角与外围 90 像素边界的材质 Alpha 均为 0。
- 原箭头与十二张水墨轨迹小图保留。后续尾迹改为 Slate 绘制的蓝色切面梯形块，沿上拱曲线排列，最多 8 块，从角色端细到箭头端粗；旧墨点不再加载或绘制。

## 风格参考

- `SourceArt/UI/Items/Gems/review/gem-quality-progression-contact-sheet.png`
- `SourceArt/UI/Items/gemstyle-resources-20260910/icons/strengthening_stone.png`

## 提示词记录

### 初稿生成

Use case: stylized-concept. Create ONE finished game UI targeting arrowhead sprite on a genuinely transparent RGBA background, square 1024x1024 canvas. The supplied images are STYLE REFERENCES ONLY: match this project's bold hand-painted faceted gemstone inventory icons, thick near-black ink silhouette, simple large angular facets and crisp cel highlights. Draw a broad chunky arrowhead pointing EXACTLY RIGHT, front view, horizontally symmetric silhouette with its sharp rightmost tip precisely on the horizontal centerline. Shape: a strong wide spearhead/chevron, two broad swept back wings and a short thick central root, compact and immediately recognizable at 80px. Cobalt/teal blue gemstone body with light cyan faceted highlights, restrained warm gold rim like the framed blue gems in reference 1, dark substantial outline. Strong graphic shape, high contrast on warm parchment. Fill approximately x=100..920 and y=220..804; isolated central single object, generous clean transparent margins, no shadow outside the silhouette. No enclosing rock (the blue rock in reference 2 must NOT be included), no badge, no circle, no particles, no streak/trail, no long shaft, no text, no white background, no checkerboard baked into pixels. Not a thin mouse cursor: a bold solid gemstone targeting arrowhead asset.

### 透明底修订（输出仍为 RGB 棋盘格，未采用）

Background extraction only. Preserve this exact right-pointing blue faceted gemstone arrow with gold rim and thick black outline, same size and position and every design feature. Completely REMOVE the gray checkerboard background and its shadows; the checkerboard is unwanted painted pixels. Output a real transparent RGBA PNG: alpha=0 outside the arrow, alpha=255 on its black outline and colored interior, antialiasing only at the silhouette edge. NO visible checkerboard, NO white or gray backdrop, NO background graphics. Do not redraw, rotate, thin, or crop the arrow. This is a transparent game sprite cutout for Unreal Engine.

### 最终纯色底修订

Change ONLY the background of this exact existing blue gemstone right-pointing arrow. Replace every checkerboard/background pixel with one perfectly flat solid chroma-key MAGENTA color RGB(255,0,255), #FF00FF. No gradients, no shadow, no texture, NO checkerboard, NO transparency pattern. Preserve the arrow's exact silhouette, dark outline, gold rim, blue facets, position, proportions and right-facing horizontal direction. Do not put any magenta inside the arrow. This flat single-color background will be removed by a real-time game UI shader. Output a square image of this exact arrow on uniform vivid pure MAGENTA.
