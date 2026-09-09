import json

import unreal


def main():
    out = {}
    for suffix in ("GameXXKBattleBoardWidget_0", "GameXXKBattleBoardWidget_1", "GameXXKBattleBoardWidget_2", "GameXXKBattleBoardWidget_3"):
        path = f"/Engine/Transient.UnrealEdEngine_0:GameInstance_1.{suffix}"
        try:
            obj = unreal.load_object(None, path)
            out[suffix] = {"exists": obj is not None}
            if obj is not None:
                try:
                    out[suffix]["is_in_viewport"] = bool(obj.is_in_viewport())
                except Exception:
                    pass
                try:
                    out[suffix]["visibility"] = str(obj.get_visibility())
                except Exception:
                    pass
        except Exception as exc:
            out[suffix] = {"error": str(exc)[:120]}
    print(json.dumps(out, ensure_ascii=False, default=str))


if __name__ == "__main__":
    main()
