# PSD Card Frame and Palette Contract

## Locked source and import

Only `clean_assets_v2/057.png` may become the reusable card-frame texture:

| Source | SHA-256 | Source pixels | Runtime draw size | UE asset |
| --- | --- | ---: | ---: | --- |
| `057.png` | `c9b0333eca9a21c45f79450db5c4f940eb23c4ffbb4290807d4194cb44025209` | `452 x 516` | `113 x 129` | `/Game/GameXXK/UI/Cards/Textures/T_CardFrame_PSD057` |

`058.png` and `059.png` are duplicate first-row exports, but are deliberately not import sources. `060.png` through `062.png` are second-row cuts and are forbidden. The frame stays un-tinted; runtime ownership colors apply only to its lower information strip, badges, and text accents.

Run the read-only source check outside UE:

```powershell
python Content/Python/gamexxk_import_psd_card_frame.py
```

After the check is green and the editor is running, the only approved write is a UE MCP/editor-Python command that imports the verified single texture:

```python
import importlib.util
path = r"D:\UE5 demo\GameXXK\Content\Python\gamexxk_import_psd_card_frame.py"
spec = importlib.util.spec_from_file_location("gamexxk_import_psd_card_frame", path)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
print(module.import_verified_card_frame())
```

Do not run the import while another user is modifying the destination asset. It uses `replace_existing=True` only for this one named texture and touches no widget, material, or adjacent PSD atom.

## Card ownership palette

| Card ownership | Lower strip / badge | Ink/text | Use |
| --- | --- | --- | --- |
| 主角 | parchment `#F1E4CC` | ink `#231E19` | Interface-matched pale yellow-white paper. |
| 刀客 | cinnabar `#B6483F` | `#FFF6E8` | Profession-only information strip. |
| 护卫 | ink teal `#254D4D` | `#F3EBDD` | Profession-only information strip. |
| 医者 | jade `#5A936D` | `#F6F1E4` | Profession-only information strip. |
| 猎手 | ochre `#9A6833` | `#FFF4DD` | Profession-only information strip. |
| 术士 | indigo `#40518D` | `#F5F2E9` | Profession-only information strip. |
| 阵师 | lotus gray-violet `#806279` | `#F6F0E9` | Profession-only information strip. |
| 任务 NPC | near-black `#252321`, pale-gray paper `#B8B4AB` | `#F3F0E9` | Black/light-gray title and strip; use existing named NPC art only. |
| Paper/ink scrollbar | paper track `#E1D3B8`, ink thumb `#292522` | — | Right-side scrollbars for codex, deck, and long reward lists. |

The six profession hues are semantic labels, not a reason to recolor the PSD card frame, NPC art, or character portraits.

## Scrollbar provenance

The approved PSD reconstruction has no independent scrollbar track or thumb. `019.png` (玩法入口框) and `051.png` (详细属性按钮) are expressly forbidden as scrollbar sources. The two right-side scrollbar atoms are therefore non-text generated assets, processed through the locked two-phase source path:

- Phase 1 source: `SourceAssets/PartyDeck/ui-scrollbar/scrollbar_track_thumb_source_v1.png`
- Phase 2 alpha/crop manifest: `SourceAssets/PartyDeck/ui-scrollbar/scrollbar_generated_manifest_v1.json`
- Derived import sources: `SourceAssets/PartyDeck/ui-scrollbar/derived/`
- UE textures: `/Game/GameXXK/UI/PartyDeck/Scrollbars/T_PartyDeck_ScrollPaperTrack_GeneratedV1` and `T_PartyDeck_ScrollInkThumb_GeneratedV1`

Run `python scripts/prepare_partydeck_scrollbar_generated_assets.py` for read-only validation, then execute `Content/Python/gamexxk_execute_psd_paper_ink_scrollbar_import.py` through an Unreal commandlet or UE MCP to import missing textures. The importer never replaces or deletes an existing asset.
