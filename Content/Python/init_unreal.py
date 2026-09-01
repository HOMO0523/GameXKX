import unreal

try:
    from gamexxk_mcp_tdd_toolset import GameXXKTDDToolset

    unreal.ToolsetRegistry.register_toolset_class(GameXXKTDDToolset)
    unreal.log("[GameXXKMCP] GameXXKTDDToolset registered")
except Exception as exc:
    unreal.log_warning(f"[GameXXKMCP] GameXXKTDDToolset registration failed: {exc}")

try:
    if "-GameXXKTask10BProbe" in unreal.SystemLibrary.get_command_line():
        import gamexxk_probe_task10b_game_runtime  # noqa: F401

        unreal.log("[GameXXKTask10B] opt-in runtime probe registered")
except Exception as exc:
    unreal.log_warning(f"[GameXXKTask10B] runtime probe registration failed: {exc}")
