# Hero HD2D Visual And Idle Verification

Date: 2026-07-02

## Tuned BP_HeroCharacter Visual

Current `BP_HeroCharacter` inherited `Visual` component values, recorded through UE MCP:

- Relative Location: `X=0`, `Y=0`, `Z=-80`
- Relative Rotation: `Pitch=0`, `Yaw=90`, `Roll=-30`
- Relative Scale: `X=1`, `Y=1`, `Z=1`

The native `AGameXXKHeroCharacter` defaults were synchronized to these values so the editable Blueprint and C++ baseline stay aligned.

## Idle Source

Canonical idle source directory:

`D:\UE5 demo\GameXXK\Saved\AssetAnalysis\HeroDeepBluePonytail\imagegen_idle_canonical5\idle_8dir_scaled_to_walk_height`

The project pipeline builds this atlas:

`Content/GameXXK/Sprites/Generated/hero_deepblue_high_ponytail_idle_8dir.png`

Generated Idle assets:

- `SPR_Hero_Idle_{Direction}_00`
- `FB_Hero_Idle_{Direction}`
- `PZD_Hero_Idle_8Dir`

Directions are ordered: South, SouthWest, West, NorthWest, North, NorthEast, East, SouthEast.

## Idle Atlas Alignment

`scripts/gamexxk_character_visual_apply.py` aligns each idle source frame against the same-direction walk atlas alpha bounds. It uses the median walk-frame bottom gap and horizontal center offset as the idle placement target.

Current validation tolerances:

- Idle bottom gap within `2 px` of the same-direction walk median.
- Idle horizontal center offset within `3 px` of the same-direction walk median.

Observed after rebuild:

- Bottom delta: `0 px` for six directions, `-0.5 px` for NorthWest and NorthEast.
- Center delta: within `0.25 px` for all eight directions.

## Verification

Commands run:

```powershell
python scripts\gamexxk_character_visual_apply.py
python scripts\gamexxk_paperzd_character_apply.py
python scripts\gamexxk_real_play_flow_mcp.py --timeout 30 --keep-pie
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development -Project='D:\UE5 demo\GameXXK\GameXXK.uproject' -NoHotReload -NoHotReloadFromIDE
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -ExecCmds='Automation RunTests GameXXK.MVP.Town.ShellInputInteractionFollower;Quit' -TestExit='Automation Test Queue Empty' -unattended -nop4 -nosplash -log
```

Observed:

- Character visual validation: `ok: true`
- PaperZD validation: `ok: true`
- Real flow: `ok: true`
- Runtime after town load: `FB_Hero_Idle_South`
- Runtime while holding `D`: `FB_Hero_Walk_East`
- Runtime after releasing `D`: `FB_Hero_Idle_East`
- Runtime while holding `W+D`: `FB_Hero_Walk_NorthEast`
- Runtime after releasing `W+D` inside the `40 ms` diagonal release window: `FB_Hero_Idle_NorthEast`
- Runtime after releasing `W`, then releasing `D` after `80 ms`: `FB_Hero_Idle_East`
- Runtime `BP_HeroCharacter.Visual` transform: Location `Z=-80`, Rotation `Yaw=90`, `Roll=-30`, Scale `1,1,1`
- Movement distance: about `450 cm`
- Quest interaction with `F`: success
- Automation test `GameXXK.MVP.Town.ShellInputInteractionFollower`: `Result={Success}`, exit code `0`
