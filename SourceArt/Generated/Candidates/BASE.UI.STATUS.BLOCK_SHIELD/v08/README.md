# BASE.UI.STATUS.BLOCK_SHIELD v08

- Provider: TowerAI project client
- Model: `gemini-3-pro-image`
- Selected: 2026-08-21
- User approval: `这个不错`
- Style authority: exported approved `T_BattleStatus_ArmorShield`
- Geometry-only reference: `2ead5f73d24ff2b339391a64644295d9daf1a72a_knock_capture_image.jpg`
- Chroma SHA-256: `F8E6141BBAF061DA178B09B1E8125C1F10E01EFAA096C136CD03929FA7CF7209`
- Final alpha SHA-256: `FD3A1FA3D56DFB9C78800B1D20AE96804F2A7CE97F3A61C212B63E0D15BAFA56`

The geometry reference controls only the attack trajectory and side-facing shield relationship. Its white background, bright blue color, pixel edge, and rough rendering are not style sources.

## Generation prompt

```text
Use case: precise-object-edit
Asset type: GameXXK battle-status UI icon master
Input images: Image 1 is authoritative only for the complete rice-paper badge, outer charcoal border, canvas scale, #ff00ff chroma-key background, low-saturation palette, and ink rendering style; Image 2 is a rough geometry diagram only and is authoritative for the spatial relationship among the incoming attack, the single contact point, the smooth upward deflection, and the shield surface; do not copy Image 2's white background, bright blue color, pixelated edge, or rough drawing quality
Primary request: redesign the entire center symbol inside Image 1 to match Image 2's action geometry much more closely while keeping the GameXXK V4 ink-paper style
Action geometry: a tapered charcoal attack stroke enters horizontally from the left; as it meets one central contact point on the shield, the same unbroken stroke bends upward in one smooth flowing arc and ends above the contact point in a clear upward-pointing arrowhead; the change of direction must be curved and fluid like a redirected brush stroke, with no sharp corner, no right angle, no elbow, and no L shape
Shield geometry: one compact muted steel-blue-gray shield/parry surface begins at the contact point and sweeps to the right and downward, with a curved raised upper-right rim and a tapered lower point, echoing Image 2's right-hand blue guard shape; it must still read clearly as a shield, but not as a large symmetrical front-facing heraldic shield; the contact point is near the center of the complete symbol, not on the far-left edge of a huge shield
Layering: the trajectory visibly touches the shield at the contact point and then flows upward from it without breaking; the shield surface sits immediately behind and to the right of the bend, making the attack look smoothly redirected upward by the shield; add at most two tiny charcoal impact ticks beside the bend
Composition: keep the complete center symbol compact, balanced, and readable at 44x44 pixels; all parts remain well inside the rice-paper badge; preserve generous padding
Preserve exactly from Image 1: one complete rounded square rice-paper badge and its single outer ink border, badge size and placement, #ff00ff background, paper texture, charcoal dry-brush language, and one muted blue-gray accent
Constraints: exactly one icon; no extra frame or second border; no full-canvas paper; no large symmetric frontal shield; no rectangular arrow path; no hard bend, right angle, elbow, or L-shaped arrow; no diagonal incoming strike; no arrow from upper right; no text, letters, digits, question mark, stack count, logo, watermark, sword, spear, second projectile, circular arrow, rune, shadow, gloss, 3D effect, magenta inside the badge, or magenta fringe
```

## Final contact refinement prompt

```text
Use case: precise-object-edit
Asset type: GameXXK battle-status UI icon master
Input images: Image 1 is the sole edit target and authority for the arrow curve, side-facing shield design, rice-paper badge, palette, texture, scale language, and #ff00ff background
Primary request: change only the relative placement and contact relationship between the curved arrow and the blue-gray shield surface
Contact correction: shift the entire shield surface leftward just enough that its upper-left rim directly touches the outside of the curved arrow at one clear tangent contact point; there must be no visible paper-colored gap between arrow and shield at that point, but the arrow must not pass through or cover the shield; place the two small impact ticks immediately beside this exact contact point
Visibility correction: make the shield surface slightly smaller and keep more of its recognizable curved shield body inside the central viewing area, while preserving its current side-facing swept shape; the arrow and shield together must remain fully readable after a runtime crop of x=22%-80% and y=20%-80%
Preserve exactly: the arrow's smooth continuous left-to-up curve and arrowhead; no hard corner or right angle; all colors, brush texture, paper badge, single outer border, #ff00ff background, padding, and every other visual element
Constraints: exactly one icon; no gap at contact; no arrow crossing the shield; no L shape; no right angle; no diagonal incoming stroke; no large frontal heraldic shield; no extra frame, text, letters, digits, stack count, logo, watermark, weapon, second projectile, rune, shadow, gloss, 3D effect, magenta inside the badge, or magenta fringe
```

## Local normalization

- Auto-sampled magenta key: `#FD01F9`
- Soft matte: transparent threshold `12`, opaque threshold `220`, despill enabled
- Centered on the detected visible-alpha bounds without enlarging the generated composition
- Final size: `1254x1254`, 8-bit RGBA
- Shield accent collapsed to one V4-compatible muted steel blue-gray: RGB `(100, 123, 128)`
- Runtime review uses the native status-icon UV crop: x `22%-80%`, y `20%-80%`
