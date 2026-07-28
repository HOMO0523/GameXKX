# Route Card Visuals Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Give every existing route card category a non-empty, PSD-compatible illustration in the battle hand, reward choice, and replacement list.

**Architecture:** Four non-text alpha PNG crests are generated and stored as source assets, then imported by a deterministic Unreal Python pipeline into `/Game/GameXXK/UI/PartyDeck/CardArt`. `UGameXXKBattleBoardWidget` resolves only the route category encoded in `AcquisitionKey` (`Route.General`, `Route.Terrain`, `Route.Rare`, or `Route.Boss`) to those four assets; it never switches on individual card IDs.

**Tech Stack:** GPT image generation, PNG alpha validation, Python `unittest`, Unreal Python `Texture2D` importer, UE C++ automation test.

---

### Task 1: Lock the four generated source assets

**Files:**
- Create: `SourceAssets/PartyDeck/card-portraits/route-source/route_*_chroma_v1.png`
- Create: `SourceAssets/PartyDeck/card-portraits/route-alpha/route_*_alpha_v1.png`
- Create: `SourceAssets/PartyDeck/card-portraits/route-card-art-manifest.json`
- Create: `SourceAssets/PartyDeck/card-portraits/generated/route_general.png`
- Create: `SourceAssets/PartyDeck/card-portraits/generated/route_terrain.png`
- Create: `SourceAssets/PartyDeck/card-portraits/generated/route_rare.png`
- Create: `SourceAssets/PartyDeck/card-portraits/generated/route_boss.png`
- Modify: `Content/Python/gamexxk_import_party_deck_card_portraits.py`
- Test: `scripts/test_party_deck_card_portrait_pipeline.py`

- [x] **Step 1: Add a failing source-contract test**

```python
self.assertEqual({record["key"] for record in route_records}, {
    "Route.General", "Route.Terrain", "Route.Rare", "Route.Boss",
})
self.assertTrue(all(record["source_mode"] == "generated_alpha" for record in route_records))
```

- [x] **Step 2: Run the focused pipeline test and verify it fails because the route records do not exist**

Run: `python scripts/test_party_deck_card_portrait_pipeline.py`

Expected: failure describing the absent Route records.

- [x] **Step 3: Generate and chroma-key-remove four non-text crest images**

Generate a 171 x 205 card-facing alpha PNG for each category. Every image uses watercolor/ink, parchment-compatible colors, no lettering, no logo, no human portrait, and transparent corners.

- [x] **Step 4: Extend the importer with the four records and generated-alpha validation**

```python
PortraitRecord("Route.General", "T_CardPortrait_Route_General", _route_alpha("route_general_alpha_v1.png"), "<sha256>", "generated_alpha", "route_general.png")
```

The chroma-key source and its alpha counterpart are distinct immutable files; the manifest locks both hashes. Use the same output-size, UE UI texture, no-delete, and no-replace contracts as existing card portraits.

- [x] **Step 5: Re-run the focused pipeline test and import the four prepared assets through UE**

Run: `python scripts/test_party_deck_card_portrait_pipeline.py`

Expected: pass with all 17 owner/category visual records and four generated route sources; UE import reports `imported_count: 4`.

### Task 2: Map route category data to its visible card art

**Files:**
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardBattleBoardWidgetTest.cpp`
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp`

- [x] **Step 1: Replace the empty-route assertion with category-mapping coverage**

```cpp
TestEqual(TEXT("route terrain cards use terrain crest"),
    BattleWidget->GetCardPortraitResourcePathForTest(TEXT("Route.Terrain.DuanYaLuoShi")),
    FString(TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Route_Terrain.T_CardPortrait_Route_Terrain")));
```

Cover General, Terrain, Rare, and Boss with representative fixtures, then iterate all 30 route definitions and assert each resolves a non-empty path in the approved route-art root.

- [x] **Step 2: Run the focused static test and verify the old resolver returns an empty route path before implementation**

Run: `rg -n "Route cards are terrain/relic techniques|return FString\(\);" Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp`

Expected: evidence that no route art mapping exists before the implementation.

- [x] **Step 3: Add the minimal AcquisitionKey category resolver**

```cpp
if (Definition.Owner == EGameXXKCardOwner::Route)
{
    const FString AcquisitionKey = Definition.AcquisitionKey.ToString();
    if (AcquisitionKey == TEXT("Route.General")) return RouteGeneralCardPortraitTexturePath;
    if (AcquisitionKey == TEXT("Route.Terrain")) return RouteTerrainCardPortraitTexturePath;
    if (AcquisitionKey == TEXT("Route.Rare")) return RouteRareCardPortraitTexturePath;
    if (AcquisitionKey.StartsWith(TEXT("Route.Boss."))) return RouteBossCardPortraitTexturePath;
}
```

Keep the existing Hero, named NPC, and six profession mappings unchanged; `ApplyCardPresentation` already serves hand, reward, and replacement cards.

- [x] **Step 4: Re-run focused static tests**

Run: `python scripts/test_party_deck_card_portrait_pipeline.py`

Expected: `scripts/test_route_card_visual_mapping.py`, `scripts/test_party_deck_card_portrait_pipeline.py`, and `scripts/test_psd_card_frame_pipeline.py` pass. The C++ automation test is ready for the next incremental compile/automation pass; no full C++ build is run in this scoped task.
