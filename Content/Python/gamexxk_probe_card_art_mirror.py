"""Read the actual painted card-mask transforms in the isolated 2D preview."""
import json
from pathlib import Path

import unreal

assert "/PartyLeft/PreviewUser/" in str(unreal.Paths.project_saved_dir()).replace("\\", "/")
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world and "L_DesktopTrainingHUD" in world.get_path_name()
pc = unreal.GameplayStatics.get_player_controller(world, 0)
board = pc.get_battle_board_widget_for_test()
assert board and board.is_battle_board_visible()
rows = []
for material in unreal.ObjectIterator(unreal.MaterialInstanceDynamic):
    owner = material.get_outer()
    if not isinstance(owner, unreal.GameXXKCardPortraitImage):
        continue
    parent = owner.get_outer()
    while parent and parent != board:
        parent = parent.get_outer()
    if parent != board:
        continue
    texture = material.get_texture_parameter_value("ArtTexture")
    if not texture or not texture.get_name().startswith("T_CardPortrait_"):
        continue
    axis = material.get_vector_parameter_value("ArtAxisX")
    size = material.get_vector_parameter_value("CardSize")
    origin = material.get_vector_parameter_value("ArtOrigin")
    rows.append({"image": owner.get_name(), "texture": texture.get_path_name(),
                 "axis_x": [axis.r, axis.g], "origin": [origin.r, origin.g], "card_size": [size.r, size.g],
                 "card_geometry": bool(owner.uses_card_geometry_for_test())})
report = {"world": world.get_path_name(), "cards": rows,
          "ok": len(rows) >= 6 and all(r["axis_x"][0] < 0 and r["card_geometry"]
              and r["origin"][0] <= r["card_size"][0] + .1
              and r["origin"][0] + r["axis_x"][0] >= -.1 for r in rows)}
out = Path(str(unreal.Paths.project_dir())).resolve() / "Saved/CardArtMirror"
out.mkdir(parents=True, exist_ok=True)
(out / "painted-card-mask-check.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
print(json.dumps(report, ensure_ascii=False))
assert report["ok"], "The visible card illustrations must be mirrored within the actual card geometry"
