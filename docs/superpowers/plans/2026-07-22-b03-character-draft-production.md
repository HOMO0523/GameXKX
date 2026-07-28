# B03 Character Draft Production Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. The active image lead executes all image calls inline so reference order and identity state remain coherent.

**Goal:** Produce 12 identity suites with one built-in `image_gen` call each for MASTER, DIR8, and IDLE4, plus deterministic PORTRAIT derivatives, transparent outputs, QC, contact sheets, and a production report.

**Architecture:** Each identity follows a gated `identity reference → MASTER → template-first DIR8/IDLE4 → PORTRAIT` chain. Existing packed atlases remain read-only and are reflowed into non-destructive layout templates; generated chroma and alpha files live only under `SourceAssets/CharacterVisuals/candidates/v1/Draft`.

**Tech Stack:** Built-in `image_gen`; Pillow/NumPy deterministic layout processing; official `remove_chroma_key.py`; SHA-256 and Markdown production records.

---

### Task 1: Lock references and stable paths

**Files:**
- Read: `docs/production/2026-07-22-image-asset-production-manifest.md`
- Read: `SourceAssets/PartyDeck/card-portraits/generated/*.png`
- Read: `SourceAssets/PartyDeck/character-references/ppt-extract/*.jpeg`
- Read only: `SourceAssets/PartyDeck/character-references/packed/*_idle_8dir.png`
- Read only: `SourceAssets/PartyDeck/character-references/packed/*_walk_8dir.png`
- Create beneath: `SourceAssets/CharacterVisuals/candidates/v1/Draft/`

- [ ] Verify the 12 card portraits, six NPC PPT extracts, and 24 packed atlases exist.
- [ ] Map stable prefixes: `role_blade`, `role_guard`, `role_healer`, `role_hunter`, `role_sorcerer`, `role_formation_master`, `npc_tusi_chief`, `npc_song_jin_bao`, `npc_yue_bai`, `npc_zhou_guang_zu`, `npc_jin_gui`, `npc_qiong_mei_er`.
- [ ] Refuse to overwrite any existing candidate or packed file.

### Task 2: Build read-only-derived layout templates

**Files:**
- Create: `SourceAssets/CharacterVisuals/candidates/v1/Draft/_templates/<prefix>_dir8_template.png`
- Create: `SourceAssets/CharacterVisuals/candidates/v1/Draft/_templates/<prefix>_idle4_template.png`

- [ ] Reflow each packed idle atlas from eight vertical `171×205` cells into a `2048×1024` 4×2 sheet with `512×512` cells, pure `#ff00ff`, order `S, SW, W, NW, N, NE, E, SE`, and bottom-center anchors.
- [ ] Build the `2048×512` IDLE4 template from the packed W idle cell repeated in four `512×512` cells; prompt-generated motion supplies subtle breathing while the template locks left-facing scale and anchor.
- [ ] Verify exact dimensions, pure key corners, eight/four occupied cells, and no writes under `packed/`.

### Task 3: Produce the Blade checkpoint

**Files:**
- Create: `SourceAssets/CharacterVisuals/candidates/v1/Draft/role_blade_master_draft_v1_chroma.png`
- Create: `SourceAssets/CharacterVisuals/candidates/v1/Draft/role_blade_master_draft_v1_alpha.png`
- Create: `SourceAssets/CharacterVisuals/candidates/v1/Draft/role_blade_dir8_draft_v1_chroma.png`
- Create: `SourceAssets/CharacterVisuals/candidates/v1/Draft/role_blade_dir8_draft_v1_alpha.png`
- Create: `SourceAssets/CharacterVisuals/candidates/v1/Draft/role_blade_idle4_draft_v1_chroma.png`
- Create: `SourceAssets/CharacterVisuals/candidates/v1/Draft/role_blade_idle4_draft_v1_alpha.png`
- Create: `SourceAssets/CharacterVisuals/candidates/v1/Draft/role_blade_portrait_draft_v1_alpha.png`
- Create: `SourceAssets/CharacterVisuals/candidates/v1/Draft/checkpoints/B03_role_blade_checkpoint.png`

- [ ] Generate MASTER from `card-portraits/generated/role_blade.png` as identity reference, then key and normalize the alpha output to 2048².
- [ ] Machine-check MASTER identity, one full character, alpha, corners, fill, and safe margins; create an identity checkpoint panel before fan-out.
- [ ] Generate DIR8 with the 4×2 template first and MASTER second; never pass the card portrait into this call.
- [ ] Generate IDLE4 with the 4-frame left-facing template first and MASTER second; never pass the card portrait into this call.
- [ ] Reflow deterministically to exact cell canvases if needed; allow only obvious cell mirror/copy correction for direction errors.
- [ ] Derive PORTRAIT from MASTER without an image call.
- [ ] Verify all outputs and report the Blade checkpoint to the main agent before continuing automatically.

### Task 4: Produce the remaining five role suites

**Files:**
- Create: `SourceAssets/CharacterVisuals/candidates/v1/Draft/role_{guard,healer,hunter,sorcerer,formation_master}_*`

- [ ] Repeat the gated Task 3 chain for Guard, Healer, Hunter, Sorcerer, and FormationMaster.
- [ ] Preserve role identities: Guard round shield/short spear and ochre padded coat; Healer sage clothing/medicine pouch/gourd; Hunter brown-green cloak/bow/quiver; Sorcerer blue-gray robe/staff/two talismans; FormationMaster warm-gray robe/feather fan/formation banner.
- [ ] Check C: after every six built-in calls; stop below 12 GB.

### Task 5: Produce the six NPC suites

**Files:**
- Create: `SourceAssets/CharacterVisuals/candidates/v1/Draft/npc_{tusi_chief,song_jin_bao,yue_bai,zhou_guang_zu,jin_gui,qiong_mei_er}_*`

- [ ] Generate each MASTER using the matching generated card portrait first and PPT extract second as identity-only references.
- [ ] Fan out only from the generated MASTER with that NPC's template first for DIR8/IDLE4.
- [ ] Preserve face, main costume colors, and signature object while simplifying to the shared chibi ink-cartoon language.
- [ ] Check C: after every six built-in calls; stop below 12 GB.

### Task 6: Final QC and reporting

**Files:**
- Create: `SourceAssets/CharacterVisuals/candidates/v1/Draft/DRAFT-B03-CHARACTERS_contact_sheet.png`
- Create: `docs/production/2026-07-22-draft-b03-characters-report.md`

- [ ] Verify 36 distinct built-in generation records, 36 chroma files, 36 alpha files, and 12 deterministic portrait files.
- [ ] Verify MASTER `2048×2048`; DIR8 `2048×1024` with eight nonempty 512² cells in the required direction order; IDLE4 `2048×512` with four nonempty left-facing 512² cells; RGBA, zero-alpha corners, no visible key residue, coherent anchors, and plausible scale.
- [ ] Visually inspect one per-identity contact sheet and the final 12-identity contact sheet.
- [ ] Record stable paths, prompts, SHA-256, pass/fail list, deterministic corrections, and the no-UE-import/final-approval-needed gates.
