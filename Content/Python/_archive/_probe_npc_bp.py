
import unreal

# Blueprint generated class property scan for sprite/flipbook switches.
bp = unreal.load_asset("/Game/GameXXK/Characters/Follower/BP_NpcCharacter")
gen = bp.generated_class() if bp else None
print("class:", gen.get_name() if gen else None)
if gen:
    for prop in gen.properties():
        name = str(prop.get_name())
        if any(k in name.lower() for k in ["sprite", "flip", "npc", "role", "portrait"]):
            print("prop:", name, type(prop).__name__)
