# 扩展回归记录（2026-09-06）

本轮核心96项全部通过。扩展213项的首次结果为188通过、25失败；其中两项v36固定版本断言已修复并在核心复验通过。以下23项未改写为通过，单列供后续整体清理。它们涉及旧战斗/伙伴夹具、旧素材约定与此前布局基线；不因此恢复已退役Boss卡或覆盖用户手调素材。

- `GameXXK.DesktopTraining.Workbench.InnerGeometry`：embedded InventoryEquipmentSlot_Weapon position: The two values are not equal.（共2条断言）
- `GameXXK.DesktopTraining.Workbench.TravelCombatPresentation`：ordinary encounter leaves its third enemy slot empty: The two values are not equal.（共6条断言）
- `GameXXK.DesktopTraining.Workbench.TravelPartyAtlasAsyncFallback`：Expected 'Hero single-clip wrapper requests its 1K atlas exactly once' to be 1, but it was 0.（共4条断言）
- `GameXXK.DesktopTraining.Workbench.TravelPartyAtlasFallbackInventory`：Expected 'Companion_Blade_Test action 2 resolves a preferred 1K descriptor' to be true.（共150条断言）
- `GameXXK.Integration.CardBattle.BoardRetreat.PresentationAndInvalidCheckpointGate`：Expected 'invalid checkpoint fixture builds a non-route battle' to be true.（共1条断言）
- `GameXXK.Integration.CardBattle.BoardAutoPlayGuards`：Expected 'auto guard fixture builds: No affordable manual enemy-target card was found in the deterministic opening-hand fixtures.' to be true.（共1条断言）
- `GameXXK.Integration.CardBattle.BoardAutoPlayManualTarget`：Expected 'manual-target auto fixture builds: No affordable manual enemy-target card was found in the deterministic opening-hand fixtures.' to be true.（共2条断言）
- `GameXXK.Integration.CardBattle.BoardAutoPlayPendingChoices`：Expected 'pending-choice auto fixture builds: No affordable manual enemy-target card was found in the deterministic opening-hand fixtures.' to be true.（共5条断言）
- `GameXXK.Integration.CardBattle.BoardAutoPlayPresentationGate`：Expected 'presentation-gate fixture builds: No affordable manual enemy-target card was found in the deterministic opening-hand fixtures.' to be true.（共6条断言）
- `GameXXK.Integration.CardBattle.BoardAutoPlayRealTimeCadence`：Expected 'real-time cadence fixture builds: No affordable manual enemy-target card was found in the deterministic opening-hand fixtures.' to be true.（共1条断言）
- `GameXXK.Integration.CardBattle.BoardAutoPlayToggle`：Expected 'auto-play toggle fixture builds an active battle: No affordable manual enemy-target card was found in the deterministic opening-hand fixtures.' to be true.（共1条断言）
- `GameXXK.Integration.CardBattle.BoardBossCardReward`：the boss reward opens with a boss-card option: The two values are not equal.（共7条断言）
- `GameXXK.Integration.CardBattle.BoardPendingHeroTaskSearch`：Expected 'Mage-search fixture is a valid saved runtime: An active protagonist spell task has invalid locked IDs, progress, or starter metadata.' to be true.（共3条断言）
- `GameXXK.Integration.CardBattle.BoardPresentationGate`：Expected 'the marker exposes the packet-local intermediate health' to be 90, but it was 100.（共7条断言）
- `GameXXK.Integration.CardBattle.BoardTargeting`：Expected 'card board fixture opens a deterministic card battle: No affordable manual enemy-target card was found in the deterministic opening-hand fixtures.' to be true.（共1条断言）
- `GameXXK.Route.Relics.EventAttributeAndChestChoice`：Expected 'leaving the route succeeds' to be true.（共3条断言）
- `GameXXK.Route.Relics.EventAttributesProjectOncePerBattle`：Expected 'the fixture reaches an active route' to be true.（共3条断言）
- `GameXXK.UI.CompanionRoster.FinalBackpackPagingAndActiveSelection`：Expected 'the paged companion fixture recruits the requested unique roster size' to be 4, but it was 6.（共1条断言）
- `GameXXK.UI.CompanionRoster.HeroDeckEditor`：Expected 'the hero deck tooltip states the eight-card editing rule' to be true.（共2条断言）
- `GameXXK.UI.CompanionRoster.LayoutAndProfile`：the roster fixture resolves a new permanent companion: The two values are not equal.（共1条断言）
- `GameXXK.UI.CompanionRoster.PersonalDeck`：Expected 'the unlocked birth-card frontier contains an unselected replacement card' to be not null.（共1条断言）
- `GameXXK.UI.CompanionRoster.ProfileExperienceAndTownOnlyClear`：Expected 'the profile exposes saved companion experience' to be 10, but it was 0.（共3条断言）
- `GameXXK.UI.CompanionRoster.RouteLock`：Expected 'a route-locked card tooltip states the actual read-only reason' to be true.（共1条断言）

原始证据：`Saved/Automation/UI_Phase3_20260906/index.json`；完整错误：`Saved/Codex/RouteUI-20260906/extended-regression-findings.json`。
五项额外旧Boss卡槽夹具失败也保留在`UI_Phase2_20260906`中；当前单关Boss不再发Boss卡，不能用它们的旧期待驱动新玩法。
