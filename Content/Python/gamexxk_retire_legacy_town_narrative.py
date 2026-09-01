"""Delete only the four disconnected Tusi/Song town Narrative DataAssets."""

from __future__ import annotations

import json

import unreal


RETIRED_ASSETS = (
    "/Game/GameXXK/Narrative/Dialogues/DA_Dialogue_Npc_TusiChief_Default",
    "/Game/GameXXK/Narrative/Dialogues/DA_Dialogue_Npc_SongJinBao_Default",
    "/Game/GameXXK/Narrative/Sequences/DA_Sequence_Npc_TusiChief_Default",
    "/Game/GameXXK/Narrative/Sequences/DA_Sequence_Npc_SongJinBao_Default",
)
CHARACTER_CATALOG = (
    "/Game/GameXXK/Narrative/Characters/DA_CharacterCatalog.DA_CharacterCatalog"
)


def _package_name(package: object) -> str:
    getter = getattr(package, "get_name", None)
    return str(getter() if callable(getter) else package)


def _dirty_packages() -> list[str]:
    result = {
        _package_name(package)
        for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
    }
    result.update(
        _package_name(package)
        for package in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    )
    return sorted(result)


def retire() -> dict[str, object]:
    dirty_before = _dirty_packages()
    if dirty_before:
        raise RuntimeError(
            "refusing legacy Narrative retirement with dirty packages: "
            + ", ".join(dirty_before)
        )

    catalog = unreal.load_asset(CHARACTER_CATALOG)
    if catalog is None:
        raise RuntimeError("character catalog is unavailable after source reimport")
    by_id = {
        str(definition.get_editor_property("character_id")): definition
        for definition in catalog.get_editor_property("characters")
    }
    for character_id in ("Npc.TusiChief", "Npc.SongJinBao"):
        definition = by_id.get(character_id)
        if definition is None:
            raise RuntimeError(f"character catalog is missing {character_id}")
        sequence_id = str(
            definition.get_editor_property("default_interaction_sequence_id")
        )
        if sequence_id not in ("", "None"):
            raise RuntimeError(
                f"{character_id} still exposes retired sequence {sequence_id}"
            )

    deleted: list[str] = []
    already_absent: list[str] = []
    for asset_path in RETIRED_ASSETS:
        if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
            if not unreal.EditorAssetLibrary.delete_asset(asset_path):
                raise RuntimeError(f"could not delete retired asset: {asset_path}")
            deleted.append(asset_path)
        else:
            already_absent.append(asset_path)

    remaining = [
        asset_path
        for asset_path in RETIRED_ASSETS
        if unreal.EditorAssetLibrary.does_asset_exist(asset_path)
    ]
    if remaining:
        raise RuntimeError("retired assets remain: " + ", ".join(remaining))
    dirty_after = _dirty_packages()
    if dirty_after:
        raise RuntimeError(
            "packages remain dirty after legacy Narrative retirement: "
            + ", ".join(dirty_after)
        )
    report = {
        "ok": True,
        "deleted": deleted,
        "already_absent": already_absent,
        "remaining": remaining,
        "disabled_characters": ["Npc.TusiChief", "Npc.SongJinBao"],
        "dirty_after": dirty_after,
    }
    print(json.dumps(report, ensure_ascii=False, indent=2, sort_keys=True))
    return report


if __name__ == "__main__":
    retire()
