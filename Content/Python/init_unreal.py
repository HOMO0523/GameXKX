import unreal

try:
    from gamexxk_mcp_tdd_toolset import GameXXKTDDToolset

    unreal.ToolsetRegistry.register_toolset_class(GameXXKTDDToolset)
    unreal.log("[GameXXKMCP] GameXXKTDDToolset registered")
except Exception as exc:
    unreal.log_warning(f"[GameXXKMCP] GameXXKTDDToolset registration failed: {exc}")
