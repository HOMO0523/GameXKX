# 2026-07-02 NPC Character Visuals

Scope:
- Copied `BP_HeroCharacter` into Merchant and person NPC character blueprints.
- Reparented Merchant/person NPC blueprints to `AGameXXKTownNpcCharacter`, a hero-style Character/Pawn runtime class.
- Replaced their town idle/walk flipbook defaults with role-specific Paper2D assets.
- Wired MVP GameMode to spawn `BP_NpcCharacter_C` and `BP_MerchantCharacter_C` directly as NPC bodies, not as visual-only children under `GameXXKTownNpcActor`.

Assets:
- `/Game/GameXXK/Characters/Merchant/BP_MerchantCharacter`
- `/Game/GameXXK/Characters/Follower/BP_NpcCharacter`
- `/Game/GameXXK/Characters/Merchant/Flipbooks/FB_Merchant_{Idle,Walk}_8dir`
- `/Game/GameXXK/Characters/Follower/Flipbooks/FB_Npc_{Idle,Walk}_8dir`
- Independent idle source sheets:
  - `Saved\AssetAnalysis\TownNpcIdle\merchant_teal_robed_idle_8dir_imagegen.png`
  - `Saved\AssetAnalysis\TownNpcIdle\follower_blue_scholar_idle_8dir_imagegen.png`

Idle pose fix:
- Root cause: NPC idle atlases were originally derived from frame 0 of the walk atlas, so the runtime used idle flipbooks but the visible pose still looked like a walk/run pose.
- Fix: `scripts\gamexxk_npc_character_visual_apply.py` now requires independent 8-direction idle source sheets, removes the magenta chroma background, extracts the eight standing character components, sorts them row-major, and repacks them to the 171x205 atlas cells using the existing walk baseline/height references.
- Regression guard: `Content\Python\gamexxk_validate_npc_character_visuals.py` now compares each idle cell against the corresponding walk frame 0 cell and fails with `idle_reuses_walk_frame0_pose` if an idle atlas reuses the walk pose.

Verification:
- `python scripts\gamexxk_npc_character_visual_check.py`
  - Result: `ok=true`; `npc_native_class=/Script/GameXXK.GameXXKTownNpcCharacter`, Merchant/person blueprints keep South idle defaults.
- `python scripts\gamexxk_npc_character_visual_apply.py`
  - Result: `ok=true`, Merchant and person idle atlases regenerated from independent standing-pose source sheets and imported through UE MCP.
- `UnrealEditor-Cmd.exe ... -ExecCmds="Automation RunTests GameXXK.MVP.PlayableShell.GameModeDefaults;Quit" ...`
  - Result: `GameXXK.MVP.PlayableShell.GameModeDefaults` success; GameMode NPC classes are hero-style Character blueprints and implement direct NPC interaction.
- `UnrealEditor-Cmd.exe ... -ExecCmds="Automation RunTests GameXXK.MVP.Town.ShellInputInteractionFollower;Quit" ...`
  - Result: `GameXXK.MVP.Town.ShellInputInteractionFollower` success; follower NPCs stay idle while the hero remains inside `FollowDistance`, start chasing only after the hero leaves range, enter the Character walk visual state while chasing, and return to same-facing idle after closing back inside range.
- `python scripts\gamexxk_real_play_flow_mcp.py --timeout 120 --report Saved\HarnessReports\gamexxk-real-play-flow-range-chase.json`
  - Report: `Saved\HarnessReports\gamexxk-real-play-flow-range-chase.json`
  - Result: Start/Qingshan/WASD/F flow ok. Runtime quest NPC is `BP_NpcCharacter_C_0` with role `QUEST`; merchant is `BP_MerchantCharacter_C_0` with role `MERCHANT`; follower is `BP_NpcCharacter_C_1` with role `FOLLOWER`. All are direct Character bodies with South idle flipbooks and `QUERY_ONLY` collision. After accepting the quest, holding `D` long enough to leave the follow range moved the quest NPC follower body 88.42 cm, kept root `z=72.0`, set `body_character.is_town_moving=true`, and used `/Game/GameXXK/Characters/Follower/Flipbooks/FB_Npc_Walk_East.FB_Npc_Walk_East`.
- `python -c "import sys; sys.path.insert(0, 'scripts'); import ue_tdd_pipeline as p; raise SystemExit(0 if p.build_project() else 1)"`
  - Result: UBT target up to date / compile succeeded.
