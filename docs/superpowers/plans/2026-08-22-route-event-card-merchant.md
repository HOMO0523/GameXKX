# Route Event Cards and Card-Upgrade Merchant Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Present route events as confirmable three-card choices and replace relic merchant stock with four independently purchasable carried-card upgrades.

**Architecture:** Route event UI owns only selection state and dispatches one existing route action on Confirm. Merchant rules persist deterministic card-upgrade offers derived through one effective-party card-pool seam; purchases update `UpgradedCardQualities` transactionally, and Leave Merchant remains the sole exit.

**Tech Stack:** Unreal Engine 5.8 C++, UMG/Slate, saved route state, UE Automation Tests, UE MCP, cold UBT.

---

## File map

- Modify `Source/GameXXK/Public/UI/GameXXKRouteEncounterPanelWidget.h`: selected choice, confirm action, card-face test seams.
- Modify `Source/GameXXK/Private/UI/GameXXKRouteEncounterPanelWidget.cpp`: three card faces, selection-only clicks, explicit confirm, event `X`.
- Modify `Source/GameXXK/Private/Tests/GameXXKRouteEncounterPanelTest.cpp`: RED/GREEN event selection and player-flow tests.
- Modify `Source/GameXXK/Public/GameXXKRouteMerchantTypes.h`: persisted card owner/current/next quality and simplified purchase results.
- Modify `Source/GameXXK/Public/GameXXKRouteMerchantRules.h`: four card-upgrade slot contract.
- Modify `Source/GameXXK/Private/GameXXKRouteMerchantRules.cpp`: effective card pool, deterministic stock, atomic upgrade purchase.
- Modify `Source/GameXXK/Public/UI/GameXXKRouteMerchantWidget.h`: remove replacement/relic-specific UI seams, expose no-close test.
- Modify `Source/GameXXK/Private/UI/GameXXKRouteMerchantWidget.cpp`: four card upgrade faces, sold state, refresh, Leave-only exit.
- Modify `Source/GameXXK/Private/Tests/GameXXKRouteMerchantRulesTest.cpp`: stock/purchase/refresh rules.
- Modify `Source/GameXXK/Private/Tests/GameXXKRouteMerchantWidgetTest.cpp`: four-card UI and no-X contract.
- Modify `Source/GameXXK/Private/Tests/GameXXKRouteMerchantFacadeTest.cpp`: subsystem transaction bridge.
- Modify `Source/GameXXK/Private/Tests/GameXXKPlayerFlowWidgetTest.cpp`: lazy desktop merchant entry/leave.

### Task 1: RED — event cards select without resolving

**Files:**
- Modify: `Source/GameXXK/Private/Tests/GameXXKRouteEncounterPanelTest.cpp`

- [ ] **Step 1: Add the selection/confirm test**

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteEncounterThreeCardConfirmTest,
	"GameXXK.MVP.RouteEncounter.Panel.ThreeCardSelectThenConfirm",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteEncounterThreeCardConfirmTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	Subsystem->GetMutableRuntimeState() = BuildPendingRouteEncounterState(EGameXXKNodeKind::Chest, EGameXXKScreen::RouteEvent);
	ConfigureChestRelicEncounter(Subsystem->GetMutableRuntimeState());
	AGameXXKMVPPlayerController* Controller = NewObject<AGameXXKMVPPlayerController>();
	Controller->SetMVPSubsystemForTest(Subsystem);
	TestTrue(TEXT("widgets exist"), Controller->EnsurePlayerFlowWidgetsForTest());
	UGameXXKRouteEncounterPanelWidget* Panel = Controller->GetRouteEncounterPanelWidgetForTest();
	TestTrue(TEXT("panel opens"), Controller->OpenRouteEncounterPanel());

	const TArray<FName> ChoicesBefore = Subsystem->GetRuntimeState().CardRun.PendingRelicOffer.RelicIds;
	TestEqual(TEXT("three full card faces render"), Panel->GetRenderedChoiceCardCountForTest(), 3);
	TestEqual(TEXT("nothing selected initially"), Panel->GetSelectedChoiceIndexForTest(), INDEX_NONE);
	TestTrue(TEXT("third card selects"), Panel->SelectChoiceForTest(2));
	TestEqual(TEXT("third card owns selection"), Panel->GetSelectedChoiceIndexForTest(), 2);
	TestEqual(TEXT("selection does not resolve screen"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::RouteEvent);
	TestFalse(TEXT("selection grants no relic"), Subsystem->GetRuntimeState().CardRun.Relics.Num() > 0);

	TestTrue(TEXT("event X returns without settlement"), Controller->ReturnPendingRouteChoiceToMap());
	TestEqual(TEXT("event X shows route map"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::DungeonMap);
	TestEqual(TEXT("pending choices remain deterministic"), Subsystem->GetRuntimeState().CardRun.PendingRelicOffer.RelicIds, ChoicesBefore);
	TestFalse(TEXT("unconfirmed node remains unvisited"), Subsystem->GetRuntimeState().VisitedRouteNodeIds.Contains(1));

	TestTrue(TEXT("same pending node reopens without reroll"), Controller->GetRouteMapWidgetForTest()->ExecuteRouteNodeById(1));
	TestTrue(TEXT("panel reopens"), Controller->IsRouteEncounterPanelOpenForTest());
	TestTrue(TEXT("third card reselects"), Panel->SelectChoiceForTest(2));
	TestTrue(TEXT("confirm resolves selected choice"), Panel->ConfirmSelectedChoiceForTest());
	TestEqual(TEXT("confirm returns to map"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::DungeonMap);
	TestTrue(TEXT("confirm grants selected relic"), Subsystem->GetRuntimeState().CardRun.Relics.ContainsByPredicate(
		[Expected = ChoicesBefore[2]](const FGameXXKRelicInstance& Relic) { return Relic.RelicId == Expected; }));
	return true;
}
```

- [ ] **Step 2: Cold-build and run RED**

Run cold UBT, then:

```powershell
D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe "D:\UE5 demo\GameXXK\GameXXK.uproject" -Unattended -NoSound -NullRHI -NoSplash -NoPause -ReportOutputPath="D:\UE5 demo\GameXXK\Saved\Automation\RouteCardsRed" -ExecCmds="Automation RunTests GameXXK.MVP.RouteEncounter.Panel.ThreeCardSelectThenConfirm; Quit"
```

Expected: compile initially fails for the new seams; after adding declarations only, the test fails because card clicks still execute immediately and no Confirm exists.

### Task 2: GREEN — render cards, selection state, Confirm, and event X

**Files:**
- Modify: `Source/GameXXK/Public/UI/GameXXKRouteEncounterPanelWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKRouteEncounterPanelWidget.cpp`

- [ ] **Step 1: Add state and public test seams**

```cpp
public:
	bool SelectChoiceForTest(int32 ChoiceIndex);
	bool ConfirmSelectedChoiceForTest();
	int32 GetSelectedChoiceIndexForTest() const { return SelectedChoiceIndex; }
	int32 GetRenderedChoiceCardCountForTest() const;

private:
	bool SelectChoice(int32 ChoiceIndex);
	bool ConfirmSelectedChoice();
	void RefreshChoiceCardStates();
	int32 SelectedChoiceIndex = INDEX_NONE;
	TArray<EGameXXKRouteEncounterAction> ChoiceActions;
	TArray<TObjectPtr<UGameXXKRouteEncounterActionButton>> ChoiceCardButtons;
	TObjectPtr<UButton> ConfirmButton;
```

- [ ] **Step 2: Make event clicks selection-only**

```cpp
bool UGameXXKRouteEncounterPanelWidget::SelectChoice(const int32 ChoiceIndex)
{
	if (!ChoiceActions.IsValidIndex(ChoiceIndex)
		|| ChoiceActions[ChoiceIndex] == EGameXXKRouteEncounterAction::None
		|| !ChoiceCardButtons.IsValidIndex(ChoiceIndex)
		|| !ChoiceCardButtons[ChoiceIndex]
		|| !ChoiceCardButtons[ChoiceIndex]->GetIsEnabled())
	{
		return false;
	}
	SelectedChoiceIndex = ChoiceIndex;
	RefreshChoiceCardStates();
	return true;
}

bool UGameXXKRouteEncounterPanelWidget::ConfirmSelectedChoice()
{
	return ChoiceActions.IsValidIndex(SelectedChoiceIndex)
		&& ExecuteAction(ChoiceActions[SelectedChoiceIndex]);
}
```

Configure the three card buttons with selection indices rather than directly resolving actions. Keep the action value in `ChoiceActions` and bind Confirm to `ConfirmSelectedChoice`.

- [ ] **Step 3: Build full card faces**

For each of three slots, create one portrait card using the existing approved card frame and an overlay containing:

```cpp
UImage* Art;
UTextBlock* Name;
UTextBlock* Description;
UTextBlock* DisabledReason;
UImage* SelectionInk;
```

Set the whole `UGameXXKRouteEncounterActionButton` as the hit target. `RefreshChoiceCardStates` sets selected ink visible only for `SelectedChoiceIndex`; disabled cards remain visible with their reason. Add a 74x74 approved CloseInk button at the event frame top-right and a bottom Confirm button. Camp continues to use its existing two action buttons.

- [ ] **Step 4: Preserve pending state on X**

Add `ReturnPendingRouteChoiceToMap` to the player controller/subsystem path. On a candidate state it changes only `Screen` from `RouteEvent`/`RouteCamp` to `DungeonMap`, preserves `PendingRouteNodeId`, `PendingEvent`, `PendingRelicOffer`, `VisitedRouteNodeIds`, and `ReachableRouteNodeIds`, then hides the modal and restores route-map focus.

Update route-node execution so clicking the same unresolved `PendingRouteNodeId` resumes its preserved Event/Camp screen instead of rejecting it or rerolling. A different node remains blocked while one choice is pending. Bind the event/camp CloseInk button to this return path.

- [ ] **Step 5: Run GREEN and related event tests**

Run the focused test and the full `GameXXK.MVP.RouteEncounter.Panel` prefix. Update old tests that clicked `TriggerPrimaryActionForTest` to call `SelectChoiceForTest(0)` followed by `ConfirmSelectedChoiceForTest` only for three-choice event/chest cases.

Expected: all route encounter tests pass, zero errors.

### Task 3: RED — merchant stock is four carried-card upgrades

**Files:**
- Modify: `Source/GameXXK/Private/Tests/GameXXKRouteMerchantRulesTest.cpp`

- [ ] **Step 1: Add deterministic pool and multi-purchase tests**

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteMerchantCarriedCardUpgradeTest,
	"GameXXK.MVP.RouteMerchant.Rules.FourCarriedCardUpgrades",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteMerchantCarriedCardUpgradeTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State = MakeMerchantFixture();
	TestTrue(TEXT("merchant stock persists"), FGameXXKRouteMerchantRules::EnsureStock(State));
	FGameXXKRouteMerchantView View;
	FString Error;
	TestTrue(TEXT("view builds"), FGameXXKRouteMerchantRules::GetView(State, View, &Error));
	TestEqual(TEXT("exactly four card slots"), View.CardOffers.Num(), 4);
	TestEqual(TEXT("no relic slots"), View.RelicOffers.Num(), 0);

	TSet<FName> CardIds;
	for (const FGameXXKRouteMerchantOfferView& OfferView : View.CardOffers)
	{
		if (!OfferView.SavedOffer.bUnavailable)
		{
			TestTrue(TEXT("offer belongs to deployed owner"), !OfferView.SavedOffer.OwnerMemberId.IsNone());
			TestTrue(TEXT("offer card is unique"), !CardIds.Contains(OfferView.SavedOffer.ContentId));
			CardIds.Add(OfferView.SavedOffer.ContentId);
			TestTrue(TEXT("offer is below Epic"), OfferView.SavedOffer.Quality < EGameXXKCardQuality::Epic);
			TestEqual(TEXT("next quality is one tier higher"), OfferView.SavedOffer.NextQuality,
				FGameXXKCardBattleAdapter::GetNextCardQuality(OfferView.SavedOffer.Quality));
		}
	}

	const TArray<FGameXXKRouteMerchantOfferView> Purchasable = View.CardOffers.FilterByPredicate(
		[](const FGameXXKRouteMerchantOfferView& Offer) { return Offer.bPurchaseEnabled; });
	if (!TestTrue(TEXT("at least two offers can be bought"), Purchasable.Num() >= 2))
	{
		return false;
	}
	FGameXXKRouteMerchantPurchaseResult First;
	FGameXXKRouteMerchantPurchaseResult Second;
	TestTrue(TEXT("first purchase commits"), FGameXXKRouteMerchantRules::Purchase(State, Purchasable[0].SavedOffer.OfferId, NAME_None, First));
	TestTrue(TEXT("second purchase commits"), FGameXXKRouteMerchantRules::Purchase(State, Purchasable[1].SavedOffer.OfferId, NAME_None, Second));
	TestTrue(TEXT("both offers are sold"), First.bPurchased && Second.bPurchased);
	TestEqual(TEXT("first authoritative quality upgraded"),
		FGameXXKCardBattleAdapter::GetConfiguredCardQuality(State.CardRun, First.CardId), First.FinalQuality);
	TestEqual(TEXT("second authoritative quality upgraded"),
		FGameXXKCardBattleAdapter::GetConfiguredCardQuality(State.CardRun, Second.CardId), Second.FinalQuality);
	return true;
}
```

- [ ] **Step 2: Add refresh persistence test**

Build stock, buy one offer, refresh, and assert:

```cpp
TestTrue(TEXT("sold card stays upgraded"),
	FGameXXKCardBattleAdapter::GetConfiguredCardQuality(State.CardRun, PurchasedCardId) == PurchasedQuality);
TestEqual(TEXT("refresh count increments"), State.CardRun.RouteMerchant.RefreshCount, 1);
TestFalse(TEXT("refreshed unsold stock never reoffers a max card"),
	Refreshed.CardOffers.ContainsByPredicate([](const FGameXXKRouteMerchantOfferView& Offer)
	{
		return !Offer.SavedOffer.bUnavailable && Offer.SavedOffer.Quality >= EGameXXKCardQuality::Epic;
	}));
```

- [ ] **Step 3: Run RED**

Expected: FAIL because current stock contains four relics, zero card offers, and purchases acquire relics.

### Task 4: GREEN — persisted offer types and effective deployed card pool

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKRouteMerchantTypes.h`
- Modify: `Source/GameXXK/Public/GameXXKRouteMerchantRules.h`
- Modify: `Source/GameXXK/Private/GameXXKRouteMerchantRules.cpp`

- [ ] **Step 1: Make the offer carry owner and quality transition**

Keep the serialized ordinal of `Card` and repurpose it only as a carried-card upgrade. Add fields:

```cpp
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName OwnerMemberId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKCardQuality NextQuality = EGameXXKCardQuality::Invalid;
```

Set rule constants:

```cpp
	static constexpr int32 CardSlotCount = 4;
	static constexpr int32 RelicSlotCount = 0;
```

The persisted `Offers` contract becomes exactly four `Card` offers (available or explicit unavailable placeholders). Normalize old relic snapshots by clearing and regenerating deterministic card stock on first merchant view/entry.

- [ ] **Step 2: Add one effective party-card-pool seam**

In merchant rules, define:

```cpp
struct FDeployedCardCandidate
{
	FName OwnerMemberId = NAME_None;
	FName CardId = NAME_None;
	EGameXXKCardQuality CurrentQuality = EGameXXKCardQuality::Invalid;
};

bool BuildEffectiveDeployedCardPool(
	const FGameXXKRuntimeState& State,
	TArray<FDeployedCardCandidate>& OutCandidates,
	FString* OutError);
```

For Unit B, resolve the effective order as Hero, active permanent companion, active task NPC. Append only each member's configured carried cards, de-duplicate by CardId, and exclude `Epic`. Unit C later changes only the member-order source to the saved ordered formation.

- [ ] **Step 3: Generate four deterministic offers**

Replace relic-pool generation with:

```cpp
	TArray<FDeployedCardCandidate> Pool;
	if (!BuildEffectiveDeployedCardPool(State, Pool, OutError)) return false;
	Pool.Sort([](const FDeployedCardCandidate& A, const FDeployedCardCandidate& B)
	{
		return A.CardId == B.CardId
			? A.OwnerMemberId.ToString() < B.OwnerMemberId.ToString()
			: A.CardId.ToString() < B.CardId.ToString();
	});
	for (int32 SlotIndex = 0; SlotIndex < CardSlotCount; ++SlotIndex)
	{
		FGameXXKRouteMerchantOffer Offer = MakeUnavailableOffer(
			RootSeed, SourceNodeId, RefreshCount, EGameXXKRouteMerchantOfferKind::Card, SlotIndex);
		if (!Pool.IsEmpty())
		{
			const int32 PickIndex = static_cast<int32>(NextRandom(RandomState) % static_cast<uint32>(Pool.Num()));
			const FDeployedCardCandidate Picked = Pool[PickIndex];
			Pool.RemoveAt(PickIndex);
			Offer.bUnavailable = false;
			Offer.ContentId = Picked.CardId;
			Offer.OwnerMemberId = Picked.OwnerMemberId;
			Offer.Quality = Picked.CurrentQuality;
			Offer.NextQuality = FGameXXKCardBattleAdapter::GetNextCardQuality(Picked.CurrentQuality);
			Offer.Price = FGameXXKCardQualityRules::GetCardPrice(Offer.NextQuality);
		}
		Candidate.Offers.Add(MoveTemp(Offer));
	}
```

- [ ] **Step 4: Replace relic purchase with atomic quality upgrade**

Preview validates route context, stable offer ID, unsold state, affordability, owner still deployed/carries the card, current quality still equals saved quality, and next quality is concrete. Commit on a candidate copy:

```cpp
	Candidate.PlayerGold -= Offer.Price;
	Candidate.CardRun.UpgradedCardQualities.Add(Offer.ContentId, Offer.NextQuality);
	FGameXXKRouteMerchantOffer* MutableOffer = Candidate.CardRun.RouteMerchant.Offers.FindByPredicate(
		[&Offer](const FGameXXKRouteMerchantOffer& Item) { return Item.OfferId == Offer.OfferId; });
	if (!MutableOffer) return false;
	MutableOffer->bSold = true;
	InOutState = MoveTemp(Candidate);
```

Populate purchase result `CardId`, `FinalQuality`, before/after balances, and saved offer. Remove replacement-card and relic-acquisition branches from the merchant purchase path; keep append-only failure enum values for save/UI compatibility.

- [ ] **Step 5: Run GREEN**

Run `GameXXK.MVP.RouteMerchant.Rules`; expected all tests pass, zero errors.

### Task 5: GREEN — four-card merchant UI with no X

**Files:**
- Modify: `Source/GameXXK/Public/UI/GameXXKRouteMerchantWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKRouteMerchantWidget.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKRouteMerchantWidgetTest.cpp`

- [ ] **Step 1: Set four card cells and zero relic cells**

```cpp
	constexpr int32 MerchantCardSlotCount = 4;
	constexpr int32 MerchantRelicSlotCount = 0;
	constexpr int32 MerchantOfferSlotCount = 4;
```

Render one row of four portrait card frames. Card face fields are owner label, art, name, current quality, next-quality effect preview, price, and sold/disabled state. Use existing card catalog and quality-definition builders; never show an empty art region without an explicit unavailable label.

- [ ] **Step 2: Remove close/replacement UI and retain Leave**

Remove the pending replacement panel and Cancel button. Do not create any merchant top-right CloseInk button. Retain Refresh and `Leave Merchant`; `LeaveMerchant()` remains the only exit and calls `ResolveMerchantRouteNode`.

Add test seams:

```cpp
	bool HasTopRightCloseButtonForTest() const { return false; }
	int32 GetRenderedCardOfferCountForTest() const;
	int32 GetRenderedRelicOfferCountForTest() const { return 0; }
```

- [ ] **Step 3: Update widget tests**

Assert four cards, zero relics, no `RouteMerchantCloseButton`, multiple enabled purchases when affordable, owner labels, sold state after purchase, increasing refresh price, and visible Leave.

- [ ] **Step 4: Run GREEN**

Run `GameXXK.MVP.RouteMerchant.Widget`; expected all pass.

### Task 6: RED/GREEN — route-map X opens settlement confirmation

**Files:**
- Modify: `Source/GameXXK/Public/UI/GameXXKOneGameRouteMapWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKOneGameRouteMapWidget.cpp`
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKRouteSettlementTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKOneGameRouteMapAdapterTest.cpp`

- [ ] **Step 1: Add RED preview/cancel/confirm tests**

Build an active generated-route fixture with earned route money/rewards and unfinished nodes. Click the existing top-right close test seam and assert:

```cpp
TestTrue(TEXT("X opens settlement confirmation"), RouteWidget->OpenRouteAbandonConfirmationForTest());
TestTrue(TEXT("preview lists earned ordinary gold"), RouteWidget->GetRouteAbandonPreviewTextForTest().ToString().Contains(TEXT("金币")));
TestEqual(TEXT("opening preview keeps route active"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::DungeonMap);
TestTrue(TEXT("cancel stays on route"), RouteWidget->CancelRouteAbandonConfirmationForTest());
TestTrue(TEXT("route remains active after cancel"), Subsystem->GetRuntimeState().bDungeonActive);
TestTrue(TEXT("confirm settles"), RouteWidget->OpenRouteAbandonConfirmationForTest() && RouteWidget->ConfirmRouteAbandonForTest());
TestEqual(TEXT("confirm returns to pure-2D Town"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::Town);
TestFalse(TEXT("route is cleared"), Subsystem->GetRuntimeState().bDungeonActive);
TestEqual(TEXT("settlement receipt applies once"), Subsystem->GetRuntimeState().PlayerGold, ExpectedGoldAfter);
```

Run RED. Expected: current confirmation previews abandonment and does not perform the approved reward/gold settlement-to-Workbench transaction.

- [ ] **Step 2: Add an authoritative subsystem transaction**

Expose:

```cpp
UFUNCTION(BlueprintCallable, Category = "GameXXK|Route")
bool SettleAndExitActiveRoute(FGameXXKRouteSettlementReceipt& OutReceipt, FString& OutError);
```

On a candidate state, build the existing terminal settlement receipt from only earned route progress, apply it idempotently, clear pending event/merchant/battle/route state, set `Screen=Town`, `CurrentMapId=DesktopTrainingHUD`, and preserve active Training Travel. Commit only after all steps succeed.

- [ ] **Step 3: Reuse the existing route confirmation shell**

Keep the current top-right route-map close button and modal widgets, but replace abandonment copy/actions with:

```text
Title: 本次路线结算
Body: earned rewards, converted ordinary gold, completed progress, forfeited unresolved progress
Confirm: 确认结算并返回挂机
Cancel: 继续路线
```

Confirm calls `SettleAndExitActiveRoute`; cancel only closes the modal. Prevent a second confirm while the first transaction is active or after the route has cleared.

- [ ] **Step 4: Run GREEN**

Run `GameXXK.MVP.RouteSettlement` and route-map adapter tests. Expected: preview/cancel/confirm, idempotency, earned-only rewards, and same-map Town return pass.

### Task 7: Player-flow, full regression, PIE, and commit

**Files:**
- Modify: `Source/GameXXK/Private/Tests/GameXXKPlayerFlowWidgetTest.cpp`
- Verify: all Unit B files.

- [ ] **Step 1: Update real node-click expectations**

In `NodeClick.EventChestAndMerchantReachTheirDedicatedHud`, replace zero-card/four-relic assertions with four-card/zero-relic and preserve the no-generic-panel, focus-lock, Leave, visited-node, and reachable-node assertions.

- [ ] **Step 2: Run focused suites**

```powershell
Automation RunTests GameXXK.MVP.RouteEncounter
Automation RunTests GameXXK.MVP.RouteMerchant
Automation RunTests GameXXK.MVP.UI
```

Use separate fresh `Saved/Automation/RouteChoicesFinal*` report folders and parse every `index.json`. Expected: zero failed/errors.

- [ ] **Step 3: Cold UBT**

Run the canonical cold build. Expected: `Result: Succeeded`.

- [ ] **Step 4: Real pure-2D route loop**

On `L_DesktopTrainingHUD`, enter Challenge route, click an Event node, select card three, verify no immediate reward, confirm, then enter Merchant. Buy at least two carried-card upgrades, refresh, verify sold upgrades persist, and Leave. Confirm the route map remains the same widget and next nodes unlock.

- [ ] **Step 5: Luna Max review**

Review event unselected/selected states and merchant four-card/sold states. Acceptance: card art/name/details are visible, selection is clear, event has `X`, merchant has no `X`, and route map stays dimly visible underneath where specified.

- [ ] **Step 6: Commit Unit B**

Stage only Unit B hunks and commit:

```powershell
git commit -m "feat: connect route choices and card upgrade merchant"
```
