# Card Art and Eight-Direction Sprite Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: use `imagegen` for all newly generated raster art, then use `superpowers:subagent-driven-development` to execute implementation tasks. Do not alter existing Hero, Follower, Merchant, PaperZD, level, camera, or manually tuned HD2D assets.

**Goal:** Make the whole approved 174-card catalogue visibly usable and give the six task NPCs plus the six permanent-partner role archetypes a project-consistent, validated eight-direction world-character package. This is a source-preserving production plan: named NPCs retain the approved character identity; role partners share one coherent role silhouette; card UI text, values and frames remain editable UMG layers instead of baked into generated bitmaps.

**Architecture:** A data-driven visual recipe maps each `CardId` to a named art key, ownership treatment and PSD frame key. The UI plan owns frames, labels and stat text; this plan only supplies transparent or keyed art inputs and imported UE textures. A second manifest maps every named task NPC and partner role to isolated source sheets, sprites, flipbooks and PaperZD-compatible animation assets. The existing Hero/Follower/Merchant pipeline is a read-only template—not an output destination.

**Tech Stack:** approved PPT character references, approved PSD card/scroll assets, built-in image generation, chroma-key removal only after a generated source has been reviewed, UE 5.8 Python asset import, existing PaperZD/Flipbook conventions, C++ automation tests, image contact sheets, UE MCP save/import workflow and cold verification.

---

## Locked visual rules

| Subject | Source/art rule | Card treatment | World sprite requirement |
| --- | --- | --- | --- |
| Hero | Existing approved hero source only; no face regeneration. | Pale yellow-white parchment body matching the main interface. | Existing package remains untouched. |
| Six named task NPCs: 土司首领、宋金宝、月白、周光祖、金贵、琼么儿 | Preserve the approved PPT identity reference. The production journal must record slide/crop/hash. No face or costume reinvention. | Black / shallow-ink gray NPC card treatment. | One new idle and walk package for every identity, eight facings. |
| Six permanent partner roles | Generate one stable archetype identity per role, then use it consistently across that role's 4 recruit templates and 18 cards. | Role-colored strip: cinnabar, ink-cyan, jade, ochre, indigo or purple-gray as defined by the UI token map. | One new idle and walk package per role archetype, eight facings. |
| Route / travel cards | Generate only terrain, relic, move, weapon or action motifs; never a fake named character. | Route ownership treatment from UI token map. | No world sprite required. |
| Monster cards | Reuse/derive from approved monster identity references where available; do not map 牛欢 to a monster. | Encounter/monster styling determined by card type. | Existing Money Rat, Black Bear and Tiger assets are not overwritten. |

The final card set must contain **174 card definitions and 174 valid visual recipes**: Hero 12, six role pools ×18 =108, six NPCs ×4 =24, route 30. The two starter travel cards are route definitions, not two extra art entries. A card has no baked Chinese text, logo, cost number, frame or rarity glyph in its generated bitmap; all of those remain localized editable UI layers.

## Source and destination contract

Create only these new, auditable paths:

```text
SourceAssets/PartyDeck/
  source-ledger.json
  card-art/
  character-references/
  generated-raw/
  generated-keyed/
  contact-sheets/
Content/GameXXK/UI/PartyDeck/CardArt/
Content/GameXXK/Sprites/Generated/PartyDeck/
Content/GameXXK/Characters/PartyDeckNPC/
Content/GameXXK/Characters/PartyDeckPartners/
```

The source ledger records: source category, absolute original reference path, source SHA-256, output SHA-256, generated prompt identifier, generation mode, image dimensions, transparent/keyed cleanup rule, identity owner, destination UE path and reviewer status. It must be UTF-8 and preserve Chinese display names as data, never as shell interpolation.

Required references:

```text
D:\360Downloads\B4_01 (1).pptx
C:\Users\shxuw\AppData\Local\Temp\gamexxk-b4-01-render\slide-5.png
C:\Users\shxuw\AppData\Local\Temp\gamexxk-b4-01-render\slide-6.png
C:\Users\shxuw\AppData\Local\Temp\gamexxk-b4-01-render\slide-7.png
C:\Users\shxuw\AppData\Local\Temp\gamexxk-b4-01-render\slide-8.png
C:\Users\shxuw\Downloads\nw-studio-nwueball-https-github-com\nw-studio-nwueball-https-github-com\work\psd_rebuild\clean_assets_v2\057.png
```

If a rendered slide is absent, regenerate it through the documented presentation renderer with a valid `HOME` environment, verify the PPT remains unchanged, and record the render command/result in the ledger. Do not use the PPT as an output source that gets modified.

## Task 1: Build the card/character source ledger and red contract tests

**Files:**

- Create: `SourceAssets/PartyDeck/source-ledger.json`
- Create: `scripts/verify_party_deck_visual_sources.py`
- Create: `scripts/build_party_deck_art_contact_sheet.py`
- Create: `Source/GameXXK/Private/Tests/GameXXKCardVisualContractTest.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKPartyCharacterVisualContractTest.cpp`
- Modify: `Source/GameXXK/Public/GameXXKCardCatalog.h`
- Modify: `Source/GameXXK/Private/GameXXKCardCatalog.cpp`

- [ ] **Step 1: Write the red data-contract tests.**

Add a `FGameXXKCardVisualDefinition` with at least `CardId`, `ArtKey`, `FrameKey`, `OwnershipTreatment`, `SourceCategory`, `CropRule` and `bIdentityLocked`. Extend card catalogue validation so all 174 immutable `CardId`s have exactly one valid visual definition and no art key is empty, duplicated accidentally across unrelated identity-locked subjects, or points outside `Content/GameXXK/UI/PartyDeck/CardArt/`.

`GameXXKCardVisualContractTest` must fail before assets are imported and prove all of the following:

```cpp
TestEqual(TEXT("all card definitions have visual recipes"), Visuals.Num(), 174);
TestTrue(TEXT("every recipe resolves one imported art texture"), AllRecipesResolve);
TestTrue(TEXT("identity locked NPC art has an approved ledger origin"), AllLockedOriginsRecorded);
TestFalse(TEXT("no card art is a PSD frame or generated text"), AnyRecipeBakesFrameOrText);
```

`GameXXKPartyCharacterVisualContractTest` must expect exactly 12 newly mapped identities (six named NPCs and six partner roles), each with one idle atlas, one six-frame walk atlas, eight populated direction rows, a sprite set and a movement animation. It must reject output paths under existing `Characters/Hero`, `Characters/Follower` or `Characters/Merchant`.

- [ ] **Step 2: Implement the manifest verifier before producing art.**

`verify_party_deck_visual_sources.py` loads only the ledger and validates source hashes, expected type, dimensions, unique `ArtKey`, non-empty visual coverage, identity locks and allowed output roots. It exits non-zero for missing input, unreviewed output, a new NPC face without a PPT-origin entry, an unapproved visual reuse, or any attempt to write into an existing character package. It must run locally without UE and without trying to infer a replacement image.

- [ ] **Step 3: Record the approved visual inventory.**

Create ledger entries for the six named NPC references from PPT slides 5–7 and for existing Hero/monster visual references used only as style/context. Add entries for the six role archetypes with state `planned-generation`, but do not claim them as reviewed until image QA is complete. Card recipes may point to a shared art key only where the catalogue explicitly intends a shared role/action image; they may not silently fall back to a generic blank card.

- [ ] **Step 4: Run the red gate.**

```powershell
python scripts/verify_party_deck_visual_sources.py
```

Expected result before art is made: the test identifies the precise missing art keys/source sheet IDs. It must not create substitute assets.

## Task 2: Produce the 174-card visual library without baking UI text

**Files:**

- Create: `SourceAssets/PartyDeck/card-art/card-art-manifest.json`
- Create: `SourceAssets/PartyDeck/card-art/prompts.json`
- Create: `scripts/prepare_party_deck_card_art.py`
- Create: `Content/Python/gamexxk_import_party_deck_card_art.py`
- Modify: card visual catalogue files from Task 1
- Modify: UI card renderer files named by the PSD UI plan only after their asset-path API exists

- [ ] **Step 1: Author the visual recipe table before generating.**

Group card artwork into semantically deliberate art keys rather than inventing visual noise per card:

1. Hero cards use approved hero action/source crops and effect overlays.
2. Each partner role has a uniform role portrait/action family used consistently by its 18-card library, with weapon/effect crop variants as required by the card recipe.
3. Each named NPC uses an approved identity portrait/action crop across its four cards, retaining facial identity and costume silhouette.
4. Route cards use landscape, relic, terrain, medicine, weapon or movement motifs. They must communicate target/effect role through editable overlays and color, not embedded lettering.

The manifest must name every card, its art key, crop focal point, source category and approved frame/ownership treatment. It is valid for several cards to use a shared role art key when the overlay/crop produces a distinct readable card; it is invalid to use a generic placeholder.

- [ ] **Step 2: Generate only role and route raster material through the image-generation workflow.**

Before each image-generation batch, visually inspect the relevant existing PSD/PPT/style reference. Use image generation as Stage 1 only—before local transparency, montage, import or OCR. Do not call an OCR pipeline for logo/character identity. Prompts must specify: Chinese ink-and-pixel / painterly game style matching approved PSD, isolated art without UI, no readable text, no logo, no card frame, centered subject/action with safe crop margins, and a solid chroma-key background when transparency is required.

Named NPC card images may be cropped/cleaned from approved references or subjected only to a reference-preserving background cleanup. They may **not** be regenerated into a different face, gender presentation or costume identity. If the generator cannot preserve identity faithfully, mark that output rejected and use a non-generative crop/effect composition from the approved source.

For generated role/route sources, save exact prompts and generation IDs to `prompts.json` and the ledger. Use the built-in image-generation tool rather than local invention. Review raw output before cleanup; rejected assets remain in `generated-raw/rejected/` with reason, never imported.

- [ ] **Step 3: Key, crop, and import.**

For chroma-keyed generated material, remove the key only after review using the image-generation skill's provided chroma-key script. Avoid a native transparent CLI fallback unless the user explicitly authorizes it. `prepare_party_deck_card_art.py` performs deterministic crop/padding checks, proves no magenta key pixels remain at the safe edge, and emits only manifest-listed PNGs. It must preserve original approved reference pixels for identity-locked crops.

`gamexxk_import_party_deck_card_art.py` imports assets under `Content/GameXXK/UI/PartyDeck/CardArt/` with stable `T_PDCA_*` names and no replacement of existing textures. The shared UMG renderer resolves texture by `ArtKey`; it overlays PSD first-row frames, ownership color, rarity, name, cost and text dynamically.

- [ ] **Step 4: Make an inspectable card contact sheet and run contract tests.**

Produce a numbered contact sheet grouped `Hero / six roles / six NPCs / Route`, then inspect it visually at original detail. The review must check: recognizability at card crop, no text/frame baked in, hero/NPC identity preservation, role silhouette consistency, route motif clarity and no transparent-edge halo. Add the review state and screenshot path to the ledger.

```powershell
python scripts/prepare_party_deck_card_art.py
python scripts/build_party_deck_art_contact_sheet.py
python scripts/verify_party_deck_visual_sources.py
```

Only after source validation, save imported packages through UE MCP and run `GameXXK.PartyDeck.CardVisual` automation.

## Task 3: Generate and validate six role archetypes and six named NPC eight-direction source sheets

**Files:**

- Create: `SourceAssets/PartyDeck/character-references/character-sheet-manifest.json`
- Create: `SourceAssets/PartyDeck/character-references/prompts.json`
- Create: `scripts/prepare_party_deck_sprite_sources.py`
- Create: `scripts/verify_party_deck_sprite_sources.py`
- Create: `SourceAssets/PartyDeck/contact-sheets/`

- [ ] **Step 1: Fix the exact atlas contract from the live project before generation.**

Read, do not modify, `Content/Python/gamexxk_assemble_npc_character_visuals.py` and its Hero/Follower/Merchant inputs. The current compatibility target is:

```text
idle atlas: 171 × 1640 (one 171 × 205 cell in each of 8 direction rows)
walk atlas: 1026 × 1640 (six 171 × 205 cells in each of 8 direction rows)
row order: South, SouthWest, West, NorthWest, North, NorthEast, East, SouthEast
```

Every row needs a non-empty subject footprint; every walk row needs six distinct readable temporal poses; diagonal rows may not be copied cardinal rows. Pivots, texture settings, sprite dimensions and naming must match the existing project script's convention. If the live script differs, update the manifest contract to the observed script first—do not force inputs to an outdated assumption.

- [ ] **Step 2: Use a staged reference-preserving generation process.**

For each of six role archetypes, create a consistent design sheet first: silhouette, garment, primary tool/weapon, palette and idle front/right/back/left anchor. For each named NPC, crop and preserve the approved PPT reference as the identity anchor. Use existing Hero/Follower eight-direction inputs strictly as pose/style templates, not as output destinations and not as a face source.

Then produce source material in stages:

1. inspected identity/style reference → role/NPC idle anchor sheet;
2. reviewed idle anchor → cardinal walk template sheet;
3. reviewed cardinal sheet plus current eight-direction template → diagonals and eight-row continuity sheet;
4. reviewed idle/pose sheet → six-frame walks for the same eight rows.

No generated sheet contains UI text or a card border. Named NPCs cannot be sent through a face-reinvention generation stage; use the approved identity crop and controlled pose/background work only. If a source cannot meet identity or direction QA, leave the manifest entry blocked and do not replace it with a generic partner or monster.

- [ ] **Step 3: Pack deterministically into the engine atlases.**

`prepare_party_deck_sprite_sources.py` performs only deterministic normalization after reviewed source images: chroma-key removal, per-cell placement, transparent padding, pivot-safe alignment and atlas packing. It writes raw/reviewed/packed sources under `SourceAssets/PartyDeck/` and outputs a manifest with exact source/output hashes. It must never resample an existing Hero/Follower/Merchant source or use a one-frame image as a walk substitute.

`verify_party_deck_sprite_sources.py` checks every identity's atlas dimensions, alpha/key cleanup, eight populated rows, six populated walk frames per row, consistent world-foot alignment, no accidental mirrored duplicate for a diagonal pair, manifest coverage and output-root isolation. It also emits a contact sheet with direction labels outside the actual texture pixels so labels cannot leak into game art.

- [ ] **Step 4: Perform visual QA on every character set.**

Inspect twelve identity contact-sheet sections. Verify facing readability, visual consistency with existing pixel/ink world rendering, no name/text, no chopped feet, no magenta halo, no copied diagonal, named NPC identity preservation and six role silhouettes recognizable in motion. Rework only the rejected component; keep approved inputs and their hashes stable.

## Task 4: Import new character assets without modifying tuned assets and register runtime mappings

**Files:**

- Create: `Content/Python/gamexxk_assemble_party_deck_character_visuals.py`
- Create: `Source/GameXXK/Public/GameXXKPartyCharacterVisualCatalog.h`
- Create: `Source/GameXXK/Private/GameXXKPartyCharacterVisualCatalog.cpp`
- Modify: character/scene presentation files only where a new runtime visual lookup is needed
- Create: `Source/GameXXK/Private/Tests/GameXXKPartyCharacterVisualCatalogTest.cpp`

- [ ] **Step 1: Write red import/mapping tests.**

The C++/asset tests require every approved `TaskNpcId` and `PartnerRoleId` to resolve through one catalog entry to isolated assets: source texture, sprite set, idle animation and walk animation. It must reject a missing row, a source path outside `PartyDeckNPC`/`PartyDeckPartners`, or an ID that maps to Hero/Follower/Merchant. Task-NPC visual lookup must use the exact six named NPC IDs; partner visual lookup must use the six approved role IDs and apply the role palette token separately from the sprite.

- [ ] **Step 2: Write a narrowly scoped UE import/assembly script.**

Copy the *behavioral pattern* of `gamexxk_assemble_npc_character_visuals.py`, but create a new script which only imports manifest-listed files under:

```text
/Game/GameXXK/Characters/PartyDeckNPC/
/Game/GameXXK/Characters/PartyDeckPartners/
```

For each identity, create new textures with nearest filtering/no mipmaps (where matching the existing pixel pipeline), eight-row sprite sources, idle/walk flipbooks or PaperZD animation assets using the established names, and a new identity-specific visual definition. Use `replace_existing=False` by default and fail rather than overwrite a populated destination. The script cannot touch placed levels, existing blueprints, Hero/Follower/Merchant assets or camera settings.

- [ ] **Step 3: Integrate only through lookup, not asset mutation.**

The runtime presentation layer gets a read-only `FindPartyCharacterVisual(StableSubjectId)` catalog lookup. Town/task or battle scene actors may ask for a visual definition when representing the current task NPC/partner, but they retain their UnitId and tuned transforms. Missing lookup shows an explicit placeholder/error state in developer builds; it must not substitute 牛欢, a wolf, or another named NPC.

- [ ] **Step 4: Save/import through UE MCP and run narrow checks.**

Save dirty editor packages through MCP before an editor restart. Run the visual source validator, execute the assembly script via the established UE MCP Python path, save only new packages, then verify with the project's character asset checks plus `GameXXK.PartyDeck.CharacterVisual` automation. Never use Live Coding or Hot Reload as evidence.

## Task 5: Complete card/character visual UI integration and test full visual coverage

**Files:**

- Modify: UI widgets named in `2026-07-16-psd-party-deck-ui-implementation.md`
- Modify: `GameXXKCardVisualContractTest.cpp` and party-character visual tests
- Create: `Source/GameXXK/Private/Tests/GameXXKPartyDeckVisualWidgetTest.cpp`

- [ ] **Step 1: Write red end-user visual tests.**

Test Hero, partner, NPC and route card renderers using actual `CardId`s. Assert each resolves an imported card art texture, the first-row PSD `057–059` frame stays separate, hero card body gets parchment treatment, partner gets role color strip, task NPC gets black/gray treatment, and no generic blank/square frame is returned. Test a 12-slot roster and route reward overlay can render all needed art keys without synchronous disk reads or UI-side fallback generation.

- [ ] **Step 2: Bind all read models to the shared resolver.**

Roster, character panel, task page, route deck, battle hand, pile viewer and reward overlay obtain `CardId → VisualDefinition → ArtKey` from the same catalogue. Widgets never determine identity using index position or a display name. New party character visual definitions feed portraits/world presentation, while card art keeps its own crop geometry; neither uses an image with baked frame/text.

- [ ] **Step 3: Inspect a full UI contact flow.**

At runtime inspect, in order: Hero character cards, a different card set for each of six partner roles, all six NPCs, partner roster scrolling, NPC task selection, route card reward, battle hand, discard viewer and map deck viewer. Validate source identity, card frame geometry, scrollbars and role/NPC treatments. Capture an evidence screenshot/contact sheet path in the ledger.

## Task 6: Final visual and gameplay verification

- [ ] **Step 1: Run static source and asset verification.**

```powershell
python scripts/verify_party_deck_visual_sources.py
python scripts/verify_party_deck_sprite_sources.py
python scripts/build_party_deck_art_contact_sheet.py
```

- [ ] **Step 2: Cold-build and run focused automation.**

```powershell
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development '-Project=D:/UE5 demo/GameXXK/GameXXK.uproject' -WaitMutex -NoHotReloadFromIDE
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.PartyDeck.CardVisual+GameXXK.PartyDeck.CharacterVisual+GameXXK.UI.PartyDeck.Visual;Quit' '-TestExit=Automation Test Queue Empty' -log -stdout -FullStdOutLogOutput
```

- [ ] **Step 3: Verify in the playable flow.**

From a new and migrated save, traverse `L_Main → L_QingshanInn → F quest → map → node → battle → reward`. Confirm visible cards resolve correctly, a permanent partner and a task NPC can appear simultaneously only alongside Hero (max three), battle card targeting still works on friendly/enemy targets, and return/route cleanup does not orphan any temporary sprite or card reference.

- [ ] **Step 4: Pre-handoff audit.**

Run `git diff --check`, inspect every touched source/asset manifest, list all generated output paths and prompts, and verify no unapproved existing asset changed. Report any intentionally blocked generated asset by identity/art key rather than silently substituting it.
