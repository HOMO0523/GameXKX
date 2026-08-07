
import unreal

level = unreal.load_asset("/Game/GameXXK/Maps/L_Qingshan_AsianVillage_Demo")
print("level:", type(level).__name__ if level else None)
actors = unreal.EditorLevelLibrary.get_all_level_actors()
npc_actors = [a for a in actors if "Npc" in a.get_actor_label() or "NPC" in a.get_actor_label() or "npc" in a.get_actor_label()]
print("npc actor count:", len(npc_actors))
for a in npc_actors:
    label = a.get_actor_label()
    comps = [c.get_name() for c in a.get_components_by_class(unreal.PaperFlipbookComponent)]
    print("actor:", label, "flipbook comps:", comps)
    if comps:
        comp = a.get_components_by_class(unreal.PaperFlipbookComponent)[0]
        fb = comp.get_editor_property("source_flipbook")
        print("   flipbook:", fb.get_path_name() if fb else None)
