"""Read-only geometry report for YueBai's intended 2K blue-flame idle."""

from __future__ import annotations

import json

import unreal


FLIPBOOK_PACKAGE = (
    "/Game/GameXXK/BattleAnimations/IdleFlipbooks/"
    "FB_character_09_yue_bai_2k_idle"
)
FLIPBOOK_PATH = f"{FLIPBOOK_PACKAGE}.FB_character_09_yue_bai_2k_idle"


def _path(value: object) -> str:
    method = getattr(value, "get_path_name", None)
    return str(method()) if callable(method) else ""


def _editor_property(value: object, name: str) -> object:
    try:
        return value.get_editor_property(name)
    except Exception:
        return None


def _vector2(value: object) -> list[float]:
    if value is None:
        return []
    return [round(float(value.x), 3), round(float(value.y), 3)]


def _geometry_summary(sprite: object) -> dict:
    geometry = _editor_property(sprite, "render_geometry")
    if geometry is None:
        return {"present": False}
    summary = {
        "present": True,
        "type": type(geometry).__name__,
        "properties": {},
    }
    property_names = (
        "geometry_type",
        "shapes",
        "pixels_per_subdivision_x",
        "pixels_per_subdivision_y",
        "avoid_vertex_merging",
        "alpha_threshold",
        "detail_amount",
        "simplify_epsilon",
    )
    for name in property_names:
        value = _editor_property(geometry, name)
        if value is None:
            continue
        if isinstance(value, (bool, int, float, str)):
            summary["properties"][name] = str(value)
        elif name == "geometry_type":
            summary["properties"][name] = repr(value)
        elif name == "shapes":
            shapes = list(value)
            summary["properties"][name] = f"list[{len(shapes)}]"
            summary["shapes"] = []
            for shape in shapes:
                vertices = list(_editor_property(shape, "vertices") or [])
                summary["shapes"].append(
                    {
                        "shapeType": repr(
                            _editor_property(shape, "shape_type")
                        ),
                        "vertexCount": len(vertices),
                        "vertices": [_vector2(vertex) for vertex in vertices],
                        "boxPosition": _vector2(
                            _editor_property(shape, "box_position")
                        ),
                        "boxSize": _vector2(_editor_property(shape, "box_size")),
                    }
                )
        elif isinstance(value, (list, tuple)):
            summary["properties"][name] = f"list[{len(value)}]"
        else:
            summary["properties"][name] = type(value).__name__
    for name in ("baked_render_data", "baked_source_uv", "baked_source_dimension"):
        value = _editor_property(sprite, name)
        if isinstance(value, (list, tuple)):
            summary[name] = f"list[{len(value)}]"
        elif value is not None:
            summary[name] = str(value)
    return summary


def probe() -> dict:
    flipbook = unreal.load_asset(FLIPBOOK_PATH)
    if flipbook is None or not isinstance(flipbook, unreal.PaperFlipbook):
        raise RuntimeError(f"YueBai blue-flame idle is missing: {FLIPBOOK_PATH}")
    keyframes = list(_editor_property(flipbook, "key_frames") or [])
    frames = []
    texture_paths = set()
    sprite_paths = set()
    for index, keyframe in enumerate(keyframes):
        sprite = _editor_property(keyframe, "sprite")
        if sprite is None:
            frames.append({"index": index, "missingSprite": True})
            continue
        texture = _editor_property(sprite, "source_texture")
        texture_paths.add(_path(texture))
        sprite_paths.add(_path(sprite))
        if index in (0, 12, 24, 36, 48, 59):
            frames.append(
                {
                    "index": index,
                    "sprite": _path(sprite),
                    "texture": _path(texture),
                    "sourceUv": _vector2(_editor_property(sprite, "source_uv")),
                    "sourceDimension": _vector2(
                        _editor_property(sprite, "source_dimension")
                    ),
                    "pixelsPerUnrealUnit": float(
                        _editor_property(sprite, "pixels_per_unreal_unit") or 0.0
                    ),
                    "geometry": _geometry_summary(sprite),
                }
            )
    return {
        "ok": True,
        "flipbook": _path(flipbook),
        "framesPerSecond": float(
            _editor_property(flipbook, "frames_per_second") or 0.0
        ),
        "keyframeCount": len(keyframes),
        "uniqueSpriteCount": len(sprite_paths),
        "texturePaths": sorted(texture_paths),
        "spriteMethods": sorted(
            name
            for name in dir(
                _editor_property(keyframes[0], "sprite") if keyframes else None
            )
            if "rebuild" in name.lower()
            or "geometry" in name.lower()
            or "post_edit" in name.lower()
        ),
        "polygonModes": sorted(
            name
            for name in dir(unreal.SpritePolygonMode)
            if name.isupper()
        ),
        "sampleFrames": frames,
    }


if __name__ == "__main__":
    print(json.dumps(probe(), ensure_ascii=False, indent=2))
