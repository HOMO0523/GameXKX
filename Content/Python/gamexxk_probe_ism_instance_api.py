"""Read the available per-instance mutation APIs before hiding B1 replacement proxies."""

from __future__ import annotations

import unreal


def main() -> None:
    actor = next(actor for actor in unreal.EditorLevelLibrary.get_all_level_actors() if str(actor.get_actor_label()) == "QS_B1_PCG_Building_courtyard_wing_orange")
    component = actor.get_components_by_class(unreal.InstancedStaticMeshComponent)[0]
    names = sorted(name for name in dir(component) if "instance" in name.lower() or "hidden" in name.lower())
    print("[GAMEXXK] " + ",".join(names))


if __name__ == "__main__":
    main()
