# Town MCP Key-State Input Design

## Goal

Allow the existing real-PIE flow harness to hold and release the town hero's real movement-key state without relying on Windows focus or transient legacy axis callbacks.

## Root cause

`MoveHorizontal` and `MoveVertical` are bound legacy Axis delegates. UE invokes them with zero on the next frame when no physical axis input exists, so an MCP call such as `MoveHorizontal(1)` changes facing briefly but cannot sustain movement. The existing D/W/A/S pressed/released handlers instead update independent key counters, which remain active across zero axis callbacks.

## Chosen design

`AGameXXKHeroCharacter` gains one BlueprintCallable automation seam:

```cpp
bool SetTownAutomationKeyState(FName KeyName, bool bPressed);
```

It accepts only `D`, `A`, `W`, and `S`, dispatches to the corresponding existing pressed/released method, and returns `false` for any other key. It changes no player-facing input path, collision, animation, or follower behavior.

The project-Python probe exposes `--town-key <D|A|W|S> <down|up>`. The existing harness replaces its transient axis hold with a key-hold context manager that releases every successfully pressed key in reverse order, including on MCP errors. Interaction stays on the existing `Interact()` path.

## Verification

- A C++ automation test proves a reflected/BlueprintCallable key-state seam holds movement across a simulated `MoveHorizontal(0)` callback and returns to idle on release.
- Python contract tests prove probe and harness use `--town-key`, emit `mcp_project_python` metadata, and release keys on primary or release failure.
- A cold `-NoHotReload` build plus the existing real PIE flow must reach the battle map and record a battle screenshot.
