#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Engine/GameInstance.h"
#include "GameXXKCardBattleAdapter.h"
#include "GameXXKCardCatalog.h"
#include "GameXXKCompanionCatalog.h"
#include "GameXXKCompanionRules.h"
#include "GameXXKMVPRules.h"
#include "Misc/AutomationTest.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKCompanionRosterWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	bool RecruitCompanion(
		FAutomationTestBase& Test,
		UGameXXKMVPSubsystem* Subsystem,
		const int32 Seed,
		FGameXXKPermanentCompanion& OutCompanion)
	{
		OutCompanion = FGameXXKPermanentCompanion();
		if (!Test.TestTrue(TEXT("a roster fixture enters town before claiming its seeded recruit"),
			Subsystem && Subsystem->EnsureQingshanTownRuntimeForDirectMap()))
		{
			return false;
		}
		FGameXXKCompanionRecruitResult Result;
		if (!Test.TestTrue(TEXT("a permanent companion fixture can be recruited through the facade"),
			Subsystem && Subsystem->RecruitPermanentCompanionFromSeed(Seed, Result)))
		{
			return false;
		}
		if (!Test.TestEqual(TEXT("the roster fixture resolves a new permanent companion"), Result.Outcome, EGameXXKCompanionRecruitOutcome::Recruited))
		{
			return false;
		}
		OutCompanion = Result.Companion;
		return !OutCompanion.InstanceId.IsNone();
	}

	UGameXXKCompanionRosterWidget* BuildWidget(UGameXXKMVPSubsystem* Subsystem)
	{
		UGameXXKCompanionRosterWidget* Widget = NewObject<UGameXXKCompanionRosterWidget>();
		Widget->SetMVPSubsystem(Subsystem);
		Widget->TakeWidget();
		Widget->RefreshFromState();
		return Widget;
	}

	TArray<FName> FirstCards(const TArray<FName>& CardIds, const int32 Count)
	{
		TArray<FName> Result;
		for (const FName CardId : CardIds)
		{
			if (Result.Num() >= Count)
			{
				break;
			}
			Result.Add(CardId);
		}
		return Result;
	}

	bool RecruitUntilCount(
		FAutomationTestBase& Test,
		UGameXXKMVPSubsystem* Subsystem,
		const int32 DesiredCount)
	{
		if (!Subsystem || !Subsystem->EnsureQingshanTownRuntimeForDirectMap())
		{
			return false;
		}
		for (int32 Seed = 43000; Seed < 43200 && Subsystem->GetPermanentCompanionViews().Num() < DesiredCount; ++Seed)
		{
			FGameXXKCompanionRecruitResult Result;
			Subsystem->RecruitPermanentCompanionFromSeed(Seed, Result);
		}
		return Test.TestEqual(
			TEXT("the paged companion fixture recruits the requested unique roster size"),
			Subsystem->GetPermanentCompanionViews().Num(),
			DesiredCount);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKFinalCompanionBackpackPagingTest,
	"GameXXK.UI.CompanionRoster.FinalBackpackPagingAndActiveSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKFinalCompanionBackpackPagingTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!RecruitUntilCount(*this, Subsystem, 4))
	{
		return false;
	}
	UGameXXKCompanionRosterWidget* Widget = BuildWidget(Subsystem);
	TestEqual(TEXT("the final companion strip keeps the twelve-person capacity"), Widget->GetRosterSlotCountForTest(), 12);
	TestEqual(TEXT("the final companion strip exposes three portraits per page"), Widget->GetRosterPageSizeForTest(), 3);
	TestEqual(TEXT("the final companion strip exposes four pages"), Widget->GetRosterPageCountForTest(), 4);
	TestEqual(TEXT("the final companion strip initially opens on page zero"), Widget->GetCurrentRosterPageForTest(), 0);
	TestEqual(TEXT("only three companion portrait controls render at once"), Widget->GetVisibleRosterButtonCountForTest(), 3);
	TestTrue(TEXT("the left page control uses the approved ink arrow"), Widget->GetRosterPageLeftResourcePathForTest().Contains(TEXT("T_MasterV2_CompanionPageLeft")));
	TestTrue(TEXT("the right page control uses the approved ink arrow"), Widget->GetRosterPageRightResourcePathForTest().Contains(TEXT("T_MasterV2_CompanionPageRight")));
	TestFalse(TEXT("the final companion backpack has no separate deploy button"), Widget->HasSeparateSetActiveButtonForTest());

	const TArray<FName> FirstPage = Widget->GetVisibleRosterSlotInstanceIdsForTest();
	TestEqual(TEXT("the first companion page exposes three occupied slots"), FirstPage.Num(), 3);
	TestTrue(TEXT("inactive companions use a gray approved portrait"), Widget->GetRosterPortraitResourcePathForTest(0).Contains(TEXT("Inactive")));
	TestTrue(TEXT("the right arrow advances to the second companion page"), Widget->GoToNextRosterPageForTest());
	TestEqual(TEXT("the second companion page becomes current"), Widget->GetCurrentRosterPageForTest(), 1);
	const TArray<FName> SecondPage = Widget->GetVisibleRosterSlotInstanceIdsForTest();
	TestEqual(TEXT("the second page contains the fourth recruited companion only"), SecondPage.Num(), 1);
	Widget->HandleConfiguredRosterSlotClicked(0);
	TestEqual(TEXT("clicking a portrait immediately selects the companion"), Widget->GetSelectedCompanionIdForTest(), SecondPage[0]);
	TestEqual(
		TEXT("clicking a portrait immediately makes that companion the active route partner"),
		Subsystem->GetRuntimeState().CardRun.PartySelection.ActivePermanentCompanionInstanceId,
		SecondPage[0]);
	TestFalse(TEXT("the active companion portrait switches from gray to bright"), Widget->GetRosterPortraitResourcePathForTest(0).Contains(TEXT("Inactive")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCompanionRosterWidgetLayoutTest,
	"GameXXK.UI.CompanionRoster.LayoutAndProfile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCompanionRosterWidgetLayoutTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	FGameXXKPermanentCompanion Companion;
	if (!RecruitCompanion(*this, Subsystem, 42001, Companion))
	{
		return false;
	}
	FGameXXKPermanentCompanion SecondCompanion;
	if (!RecruitCompanion(*this, Subsystem, 42011, SecondCompanion))
	{
		return false;
	}

	UGameXXKCompanionRosterWidget* Widget = BuildWidget(Subsystem);
	TestNotNull(TEXT("companion roster builds a widget instance"), Widget);
	TestNotNull(TEXT("companion roster reserves a named personal-card scroll box"),
		Widget && Widget->WidgetTree ? Cast<UScrollBox>(Widget->WidgetTree->FindWidget(TEXT("CompanionRosterPersonalCardScroll"))) : nullptr);
	TestNotNull(TEXT("companion roster builds a named page-18 avatar slot"),
		Widget && Widget->WidgetTree ? Cast<UButton>(Widget->WidgetTree->FindWidget(TEXT("CompanionRosterSlot_00"))) : nullptr);
	TestEqual(TEXT("the companion backpack always reserves twelve roster slots"), Widget->GetRosterSlotCountForTest(), 12);
	TestEqual(TEXT("the companion backpack fixes the roster at three columns"), Widget->GetRosterColumnCountForTest(), 3);
	TestTrue(TEXT("the window uses the approved final large paper panel"),
		Widget->GetWindowFrameResourcePathForTest().Contains(TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_PanelLarge")));
	TestTrue(TEXT("the avatar slots use the approved page-18 tab paper base"),
		Widget->GetRosterSlotResourcePathForTest().Contains(TEXT("/Game/GameXXK/UI/MasterV2/Approved/003_tab_1")));
	TestTrue(TEXT("personal cards use the approved final card frame"),
		Widget->GetPersonalCardFrameResourcePathForTest().Contains(TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_CardFrame")));
	TestTrue(TEXT("the card list exposes a scroll-box reservation for the PSD scroll bar"),
		Widget->HasPersonalCardScrollBoxForTest());
	TestTrue(TEXT("the card list shares the page-03 PSD scrollbar track"),
		Widget->GetPersonalCardScrollTrackResourcePathForTest().Contains(TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_BackpackScrollbarRight")));
	TestTrue(TEXT("the card list shares the page-03 PSD scrollbar thumb"),
		Widget->GetPersonalCardScrollThumbResourcePathForTest().Contains(TEXT("/Game/GameXXK/UI/MasterV2/Approved/inventory_scrollbar_Button")));
	TestEqual(TEXT("the first recruited companion is selected for its profile"), Widget->GetSelectedCompanionIdForTest(), Companion.InstanceId);
	UUniformGridPanel* PersonalCardGrid = Widget->WidgetTree ? Cast<UUniformGridPanel>(Widget->WidgetTree->FindWidget(TEXT("CompanionRosterPersonalCardGrid"))) : nullptr;
	USizeBox* FirstPersonalCardSize = PersonalCardGrid ? Cast<USizeBox>(PersonalCardGrid->GetChildAt(0)) : nullptr;
	UGameXXKCompanionRosterCardButton* FirstPersonalCard = FirstPersonalCardSize ? Cast<UGameXXKCompanionRosterCardButton>(FirstPersonalCardSize->GetChildAt(0)) : nullptr;
	TestNotNull(TEXT("the companion deck grid exposes a real hoverable card"), FirstPersonalCard);
	if (!FirstPersonalCard)
	{
		return false;
	}
	const TArray<FName> PersonalDeckBeforeHover = Widget->GetPendingPersonalCardIds();
	FirstPersonalCard->OnHovered.Broadcast();
	TestTrue(TEXT("hovering a companion deck card reveals its roster-owned tooltip"), Widget->IsCardTooltipVisibleForTest());
	TestTrue(TEXT("the companion deck tooltip remains input-transparent"), Widget->IsCardTooltipHitTestInvisibleForTest());
	TestTrue(TEXT("the companion deck tooltip states the five-card editing rule"), Widget->GetCardTooltipTextForTest().Contains(TEXT("点击后编入/移出该伙伴个人牌组；需保持 5 张。")));
	TestEqual(TEXT("companion deck hover never changes the staged personal deck"), Widget->GetPendingPersonalCardIds(), PersonalDeckBeforeHover);
	FirstPersonalCard->OnUnhovered.Broadcast();
	TestFalse(TEXT("leaving a companion deck card immediately hides its tooltip"), Widget->IsCardTooltipVisibleForTest());
	FirstPersonalCard->OnHovered.Broadcast();
	TestTrue(TEXT("the roster stale-hover fixture starts with a visible tooltip"), Widget->IsCardTooltipVisibleForTest());
	Widget->RefreshFromState();
	TestFalse(TEXT("a structural roster refresh clears the old card tooltip"), Widget->IsCardTooltipVisibleForTest());
	PersonalCardGrid = Widget->WidgetTree ? Cast<UUniformGridPanel>(Widget->WidgetTree->FindWidget(TEXT("CompanionRosterPersonalCardGrid"))) : nullptr;
	FirstPersonalCardSize = PersonalCardGrid ? Cast<USizeBox>(PersonalCardGrid->GetChildAt(0)) : nullptr;
	UGameXXKCompanionRosterCardButton* RebuiltPersonalCard = FirstPersonalCardSize ? Cast<UGameXXKCompanionRosterCardButton>(FirstPersonalCardSize->GetChildAt(0)) : nullptr;
	TestNotNull(TEXT("the structural refresh rebuilds the same visible personal-card slot"), RebuiltPersonalCard);
	TestFalse(TEXT("a rebuilt matching card id never resurrects the old tooltip"), Widget->IsCardTooltipVisibleForTest());
	if (!RebuiltPersonalCard)
	{
		return false;
	}
	RebuiltPersonalCard->OnHovered.Broadcast();
	TestTrue(TEXT("the rebuilt card can open a fresh tooltip before a companion switch"), Widget->IsCardTooltipVisibleForTest());
	TestTrue(TEXT("switching companions succeeds for the stale-hover regression"), Widget->SelectCompanion(SecondCompanion.InstanceId));
	TestFalse(TEXT("switching companions clears the prior grid tooltip before rebuilding cards"), Widget->IsCardTooltipVisibleForTest());
	UButton* SecondRosterSlot = Widget->WidgetTree ? Cast<UButton>(Widget->WidgetTree->FindWidget(TEXT("CompanionRosterSlot_01"))) : nullptr;
	TestNotNull(TEXT("the second fixed roster slot is a real interactive button"), SecondRosterSlot);
	if (SecondRosterSlot)
	{
		SecondRosterSlot->OnClicked.Broadcast();
	}
	TestEqual(TEXT("clicking a roster slot selects its stable permanent companion"), Widget->GetSelectedCompanionIdForTest(), SecondCompanion.InstanceId);
	const FGameXXKCompanionRosterProfileView Profile = Widget->GetSelectedCompanionProfile();
	TestEqual(TEXT("profile keeps the stable companion identity"), Profile.InstanceId, SecondCompanion.InstanceId);
	TestEqual(TEXT("profile exposes the companion role"), Profile.Role, SecondCompanion.Role);
	TestEqual(TEXT("profile exposes the persistent level"), Profile.Level, SecondCompanion.Level);
	TestEqual(TEXT("profile exposes the persistent star"), Profile.Star, SecondCompanion.Star);
	TestTrue(TEXT("profile derives canonical health attributes"), Profile.Attributes.Health > 0);
	TestTrue(TEXT("profile derives canonical attack attributes"), Profile.Attributes.Attack > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCompanionRosterWidgetPersonalDeckTest,
	"GameXXK.UI.CompanionRoster.PersonalDeck",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCompanionRosterWidgetPersonalDeckTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	FGameXXKPermanentCompanion Companion;
	if (!RecruitCompanion(*this, Subsystem, 42002, Companion))
	{
		return false;
	}

	UGameXXKCompanionRosterWidget* Widget = BuildWidget(Subsystem);
	TestEqual(TEXT("the selected companion exposes its deterministic twelve-card personal pool"), Widget->GetVisiblePersonalCardIds().Num(), 12);
	TArray<FName> PendingCards = Widget->GetPendingPersonalCardIds();
	TestEqual(TEXT("the saved permanent companion begins with five staged loadout cards"), PendingCards.Num(), 5);
	if (PendingCards.Num() != 5)
	{
		return false;
	}

	const FName RemovedCardId = PendingCards[0];
	const TArray<FName> VisibleCards = Widget->GetVisiblePersonalCardIds();
	const FName* ReplacementCardId = VisibleCards.FindByPredicate([&PendingCards, &Companion](const FName CardId)
	{
		return !PendingCards.Contains(CardId) && Companion.UnlockedPersonalCardIds.Contains(CardId);
	});
	TestNotNull(TEXT("the twelve-card pool contains an unselected replacement card"), ReplacementCardId);
	if (!ReplacementCardId)
	{
		return false;
	}

	TestTrue(TEXT("a selected personal card can be removed from the staged five-card loadout"), Widget->ToggleSelectedCompanionCard(RemovedCardId));
	TestTrue(TEXT("an unlocked unselected personal card can replace it"), Widget->ToggleSelectedCompanionCard(*ReplacementCardId));
	TestEqual(TEXT("the staged companion loadout remains five cards"), Widget->GetPendingPersonalCardIds().Num(), 5);
	UButton* ApplyLoadoutButton = Widget->WidgetTree ? Cast<UButton>(Widget->WidgetTree->FindWidget(TEXT("CompanionRosterApplyLoadout"))) : nullptr;
	TestNotNull(TEXT("the staged personal deck exposes a real apply button"), ApplyLoadoutButton);
	if (ApplyLoadoutButton)
	{
		ApplyLoadoutButton->OnClicked.Broadcast();
	}

	FGameXXKPermanentCompanion SavedCompanion;
	TestTrue(TEXT("the saved companion remains readable after a UI deck apply"), Subsystem->TryGetPermanentCompanionView(Companion.InstanceId, SavedCompanion));
	TestEqual(TEXT("the facade receives the staged UI loadout in player order"), SavedCompanion.SelectedCardIds, Widget->GetPendingPersonalCardIds());

	const FName* SixthCardId = VisibleCards.FindByPredicate([Widget, &SavedCompanion](const FName CardId)
	{
		return !Widget->GetPendingPersonalCardIds().Contains(CardId) && SavedCompanion.UnlockedPersonalCardIds.Contains(CardId);
	});
	TestNotNull(TEXT("the five-card selection still has an excluded sixth-card candidate"), SixthCardId);
	const FName SelectedPersonalCardId = Widget->GetPendingPersonalCardIds()[0];
	Widget->HandleConfiguredCardHoverChanged(SelectedPersonalCardId, false, true);
	TestTrue(TEXT("an already-selected companion card remains actionable at the five-card cap"),
		Widget->GetCardTooltipTextForTest().Contains(TEXT("点击后编入/移出该伙伴个人牌组；需保持 5 张。")));
	Widget->HandleConfiguredCardHoverChanged(SelectedPersonalCardId, false, false);
	if (SixthCardId)
	{
		Widget->HandleConfiguredCardHoverChanged(*SixthCardId, false, true);
		TestTrue(TEXT("a sixth companion card explains the exact five-card capacity reason"),
			Widget->GetCardTooltipTextForTest().Contains(TEXT("该伙伴个人牌组已满（5 张），无法编入此牌。")));
		TestFalse(TEXT("a full companion deck never advertises an unavailable add interaction"),
			Widget->GetCardTooltipTextForTest().Contains(TEXT("点击后编入/移出")));
		Widget->HandleConfiguredCardHoverChanged(*SixthCardId, false, false);
		TestFalse(TEXT("the UI refuses a sixth personal loadout card before mutating the facade"), Widget->ToggleSelectedCompanionCard(*SixthCardId));
	}

	const FName* LockedPersonalCardId = VisibleCards.FindByPredicate([Widget, &SavedCompanion](const FName CardId)
	{
		return !Widget->GetPendingPersonalCardIds().Contains(CardId)
			&& SavedCompanion.UnlockedPersonalCardIds.Contains(CardId);
	});
	FGameXXKPermanentCompanion* MutableCompanion = Subsystem->GetMutableRuntimeState().CardRun.CompanionRoster.PermanentCompanions.FindByPredicate([&Companion](const FGameXXKPermanentCompanion& Candidate)
	{
		return Candidate.InstanceId == Companion.InstanceId;
	});
	TestNotNull(TEXT("the personal deck fixture exposes a mutable saved companion for locked-card presentation"), MutableCompanion);
	TestNotNull(TEXT("the personal deck fixture has an excluded card to mark unavailable"), LockedPersonalCardId);
	if (MutableCompanion && LockedPersonalCardId)
	{
		const int32 OriginalUnlockedIndex = MutableCompanion->UnlockedPersonalCardIds.IndexOfByKey(*LockedPersonalCardId);
		MutableCompanion->UnlockedPersonalCardIds.RemoveSingle(*LockedPersonalCardId);
		Widget->RefreshFromState();
		Widget->HandleConfiguredCardHoverChanged(*LockedPersonalCardId, false, true);
		TestTrue(TEXT("an unavailable personal card states its exact locked reason"),
			Widget->GetCardTooltipTextForTest().Contains(TEXT("此牌尚未解锁。")));
		TestFalse(TEXT("an unavailable personal card never advertises a click interaction"),
			Widget->GetCardTooltipTextForTest().Contains(TEXT("点击后编入/移出")));
		Widget->HandleConfiguredCardHoverChanged(*LockedPersonalCardId, false, false);
		// The locked-card presentation above intentionally creates a state the runtime never saves.
		// Restore the canonical unlocked pool before probing the real town-only active-partner action.
		MutableCompanion->UnlockedPersonalCardIds.Insert(*LockedPersonalCardId, OriginalUnlockedIndex);
		Widget->RefreshFromState();
	}

	UButton* FirstRosterSlot = Widget->WidgetTree ? Cast<UButton>(Widget->WidgetTree->FindWidget(TEXT("CompanionRosterSlot_00"))) : nullptr;
	TestNotNull(TEXT("the selected companion exposes a real portrait selection control"), FirstRosterSlot);
	TestEqual(TEXT("the active-partner fixture remains in town"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::Town);
	TestFalse(TEXT("the active-partner fixture remains editable"), Widget->IsLoadoutReadOnlyForTest());
	TestEqual(TEXT("the first visible portrait keeps the recruited stable id"), Widget->GetVisibleRosterSlotInstanceIdsForTest()[0], Companion.InstanceId);
	TestTrue(TEXT("the selected companion is directly eligible for active-partner assignment"), Widget->SetSelectedCompanionAsActive());
	TestTrue(TEXT("the assigned partner can be cleared before the button-path assertion"), Widget->ClearActivePermanentCompanion());
	if (FirstRosterSlot)
	{
		FirstRosterSlot->OnClicked.Broadcast();
	}
	TestEqual(TEXT("the facade routes the active permanent partner by stable id"),
		Subsystem->GetRuntimeState().CardRun.PartySelection.ActivePermanentCompanionInstanceId, Companion.InstanceId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCompanionRosterWidgetHeroDeckEditorTest,
	"GameXXK.UI.CompanionRoster.HeroDeckEditor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCompanionRosterWidgetHeroDeckEditorTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	TestTrue(TEXT("the hero deck editor fixture enters town before editing the loadout"),
		Subsystem && Subsystem->EnsureQingshanTownRuntimeForDirectMap());
	if (!Subsystem || Subsystem->GetRuntimeState().Screen != EGameXXKScreen::Town)
	{
		return false;
	}
	const TArray<FGameXXKCardDefinition> HeroDefinitions = FGameXXKCardCatalog::GetCardDefinitionsForOwner(FName(TEXT("Hero")));
	TArray<FName> HeroPool;
	for (const FGameXXKCardDefinition& Definition : HeroDefinitions)
	{
		HeroPool.Add(Definition.Id);
	}
	const FGameXXKCardDefinition* NonHeroDefinition = FGameXXKCardCatalog::GetAllCardDefinitions().FindByPredicate([](const FGameXXKCardDefinition& Definition)
	{
		return Definition.Owner != EGameXXKCardOwner::Hero;
	});
	TestEqual(TEXT("the hero editor fixture owns the complete thirty-six-card hero pool"), HeroPool.Num(), 36);
	TestNotNull(TEXT("the hero editor fixture can probe a non-hero card id"), NonHeroDefinition);
	if (HeroPool.Num() != 36 || !NonHeroDefinition)
	{
		return false;
	}

	FGameXXKRuntimeState& RuntimeState = Subsystem->GetMutableRuntimeState();
	RuntimeState.CardRun.HeroUnlockedCardIds = HeroPool;
	RuntimeState.CardRun.HeroSelectedCardIds = FirstCards(HeroPool, 8);
	UGameXXKCompanionRosterWidget* Widget = BuildWidget(Subsystem);
	TestFalse(TEXT("the companion backpack initially shows the selected partner deck"), Widget->IsHeroDeckEditorOpenForTest());
	// Page 18 removes the standalone hero-deck toggle; the editor opens through the retained API.
	TestTrue(TEXT("the companion backpack opens a dedicated player-editable hero deck"), Widget->OpenHeroDeckEditor());
	TestTrue(TEXT("the hero deck editor is active after opening"), Widget->IsHeroDeckEditorOpenForTest());
	TestEqual(TEXT("the hero editor renders all thirty-six available hero cards"), Widget->GetVisibleHeroCardIds().Num(), 36);
	TestEqual(TEXT("the hero editor stages the saved eight-card hero selection"), Widget->GetPendingHeroCardIds().Num(), 8);
	UUniformGridPanel* HeroCardGrid = Widget->WidgetTree ? Cast<UUniformGridPanel>(Widget->WidgetTree->FindWidget(TEXT("CompanionRosterPersonalCardGrid"))) : nullptr;
	TestEqual(TEXT("the retained scroll grid materializes one selectable slot per hero card"),
		HeroCardGrid ? HeroCardGrid->GetChildrenCount() : 0, 36);
	USizeBox* FirstHeroCardSize = HeroCardGrid ? Cast<USizeBox>(HeroCardGrid->GetChildAt(0)) : nullptr;
	UGameXXKCompanionRosterCardButton* FirstHeroCard = FirstHeroCardSize ? Cast<UGameXXKCompanionRosterCardButton>(FirstHeroCardSize->GetChildAt(0)) : nullptr;
	TestNotNull(TEXT("the hero deck grid exposes a real hoverable card"), FirstHeroCard);
	if (!FirstHeroCard)
	{
		return false;
	}
	const TArray<FName> HeroDeckBeforeHover = Widget->GetPendingHeroCardIds();
	FirstHeroCard->OnHovered.Broadcast();
	TestTrue(TEXT("hovering a hero deck card reveals its roster-owned tooltip"), Widget->IsCardTooltipVisibleForTest());
	TestTrue(TEXT("the hero deck tooltip states the eight-card editing rule"), Widget->GetCardTooltipTextForTest().Contains(TEXT("点击后编入/移出主角牌组；需保持 8 张。")));
	TestEqual(TEXT("hero deck hover never changes the staged hero deck"), Widget->GetPendingHeroCardIds(), HeroDeckBeforeHover);
	FirstHeroCard->OnUnhovered.Broadcast();
	TestFalse(TEXT("leaving a hero deck card immediately hides its tooltip"), Widget->IsCardTooltipVisibleForTest());
	TestFalse(TEXT("the hero editor refuses a non-hero card even if called directly"), Widget->ToggleHeroCard(NonHeroDefinition->Id));
	if (Widget->GetPendingHeroCardIds().Num() != 8)
	{
		return false;
	}

	TArray<FName> PendingHeroCards = Widget->GetPendingHeroCardIds();
	const FName RemovedCardId = PendingHeroCards[0];
	const TArray<FName> VisibleHeroCards = Widget->GetVisibleHeroCardIds();
	const FName* ReplacementCardId = VisibleHeroCards.FindByPredicate([&PendingHeroCards](const FName CardId)
	{
		const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(CardId);
		return !PendingHeroCards.Contains(CardId)
			&& Definition
			&& Definition->LinkedRole != EGameXXKCharacterRole::Invalid;
	});
	TestNotNull(TEXT("an initially unlocked profession-linked card is an excluded replacement card"), ReplacementCardId);
	if (!ReplacementCardId)
	{
		return false;
	}
	const FName ReplacementCard = *ReplacementCardId;

	TestTrue(TEXT("the hero editor can locally stage fewer than eight cards without saving"), Widget->ToggleHeroCard(RemovedCardId));
	TestEqual(TEXT("removing one hero card leaves a seven-card staged selection"), Widget->GetPendingHeroCardIds().Num(), 7);
	TestFalse(TEXT("the hero editor blocks saving fewer than eight cards"), Widget->ApplyHeroCardLoadout());
	TestEqual(TEXT("the rejected seven-card staging never mutates the saved hero loadout"), Subsystem->GetHeroCardLoadout().Num(), 8);

	TestTrue(TEXT("an excluded hero card can restore an eight-card staged selection"), Widget->ToggleHeroCard(ReplacementCard));
	TestEqual(TEXT("the staged replacement restores exactly eight hero cards"), Widget->GetPendingHeroCardIds().Num(), 8);
	const TArray<FName> VisibleHeroCardsAfterStaging = Widget->GetVisibleHeroCardIds();
	const FName* NinthCardId = VisibleHeroCardsAfterStaging.FindByPredicate([Widget](const FName CardId)
	{
		return !Widget->GetPendingHeroCardIds().Contains(CardId);
	});
	TestNotNull(TEXT("the staged eight-card hero selection still has a ninth-card candidate"), NinthCardId);
	const FName SelectedHeroCardId = Widget->GetPendingHeroCardIds()[0];
	Widget->HandleConfiguredCardHoverChanged(SelectedHeroCardId, true, true);
	TestTrue(TEXT("an already-selected hero card remains actionable at the eight-card cap"),
		Widget->GetCardTooltipTextForTest().Contains(TEXT("点击后编入/移出主角牌组；需保持 8 张。")));
	Widget->HandleConfiguredCardHoverChanged(SelectedHeroCardId, true, false);
	if (NinthCardId)
	{
		Widget->HandleConfiguredCardHoverChanged(*NinthCardId, true, true);
		TestTrue(TEXT("a ninth hero card explains the exact eight-card capacity reason"),
			Widget->GetCardTooltipTextForTest().Contains(TEXT("主角牌组已满（8 张），无法编入此牌。")));
		TestFalse(TEXT("a full hero deck never advertises an unavailable add interaction"),
			Widget->GetCardTooltipTextForTest().Contains(TEXT("点击后编入/移出")));
		Widget->HandleConfiguredCardHoverChanged(*NinthCardId, true, false);
		TestFalse(TEXT("the hero editor refuses a ninth staged card before mutating the facade"), Widget->ToggleHeroCard(*NinthCardId));
	}

	const TArray<FName> StagedHeroCardsBeforeApply = Widget->GetPendingHeroCardIds();
	TestTrue(TEXT("the hero deck editor persists the staged eight-card selection through its API"),
		Widget->ApplyHeroCardLoadout());
	TestEqual(TEXT("the persisted hero-card loadout preserves the staged order"),
		Subsystem->GetHeroCardLoadout(), StagedHeroCardsBeforeApply);
	TestEqual(TEXT("the refreshed editor keeps the persisted hero-card order after applying"),
		Widget->GetPendingHeroCardIds(), StagedHeroCardsBeforeApply);

	RuntimeState.CardRun.bLoadoutLockedForRoute = true;
	Widget->RefreshFromState();
	TestTrue(TEXT("the hero deck remains visible but is read-only after the route lock"), Widget->IsHeroDeckEditorOpenForTest());
	const TArray<FName> LockedPendingHeroCards = Widget->GetPendingHeroCardIds();
	TestTrue(TEXT("the locked hero editor retains its persisted eight-card display"), LockedPendingHeroCards.Num() == 8);
	if (LockedPendingHeroCards.Num() != 8)
	{
		return false;
	}
	TestFalse(TEXT("the route-locked hero editor rejects card staging"), Widget->ToggleHeroCard(LockedPendingHeroCards[0]));
	TestFalse(TEXT("the route-locked hero editor rejects persistence"), Widget->ApplyHeroCardLoadout());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCompanionRosterWidgetRouteLockTest,
	"GameXXK.UI.CompanionRoster.RouteLock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCompanionRosterWidgetRouteLockTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	FGameXXKPermanentCompanion Companion;
	if (!RecruitCompanion(*this, Subsystem, 42003, Companion))
	{
		return false;
	}
	const FGameXXKQuestNpcDefinition* TusiChief = FGameXXKCompanionCatalog::FindQuestNpcDefinition(TEXT("Npc.TusiChief"));
	TestNotNull(TEXT("the route-lock fixture has a named task NPC card definition"), TusiChief);
	if (!TusiChief || !FGameXXKCardBattleAdapter::SetQuestNpcForCurrentRun(Subsystem->GetMutableRuntimeState(), TusiChief->NpcId, {}))
	{
		return false;
	}

	UGameXXKCompanionRosterWidget* Widget = BuildWidget(Subsystem);
	TestEqual(TEXT("the hero summary is facade-backed at exactly eight cards"), Widget->GetHeroCardSummary().Num(), 8);
	TestEqual(TEXT("the task NPC summary is facade-backed at exactly three cards"), Widget->GetTaskNpcCardSummary().SelectedCardIds.Num(), 3);

	Subsystem->GetMutableRuntimeState().CardRun.bLoadoutLockedForRoute = true;
	Widget->RefreshFromState();
	TestTrue(TEXT("the roster UI becomes read-only after the route loadout lock"), Widget->IsLoadoutReadOnlyForTest());
	const TArray<FName> PendingCards = Widget->GetPendingPersonalCardIds();
	TestTrue(TEXT("the locked fixture retains a card to attempt a mutation"), PendingCards.Num() > 0);
	if (PendingCards.Num() > 0)
	{
		TestFalse(TEXT("the locked roster rejects personal-card toggles"), Widget->ToggleSelectedCompanionCard(PendingCards[0]));
	}
	UUniformGridPanel* LockedCardGrid = Widget->WidgetTree ? Cast<UUniformGridPanel>(Widget->WidgetTree->FindWidget(TEXT("CompanionRosterPersonalCardGrid"))) : nullptr;
	USizeBox* LockedCardSize = LockedCardGrid ? Cast<USizeBox>(LockedCardGrid->GetChildAt(0)) : nullptr;
	UGameXXKCompanionRosterCardButton* LockedCard = LockedCardSize ? Cast<UGameXXKCompanionRosterCardButton>(LockedCardSize->GetChildAt(0)) : nullptr;
	TestNotNull(TEXT("a route-locked personal deck card remains hoverable for its unavailable reason"), LockedCard);
	if (LockedCard)
	{
		LockedCard->OnHovered.Broadcast();
		TestTrue(TEXT("a route-locked card tooltip states the actual read-only reason"), Widget->GetCardTooltipTextForTest().Contains(TEXT("本次路线已锁定，牌组只读。")));
		TestFalse(TEXT("a route-locked card tooltip does not advertise a disabled deck interaction"), Widget->GetCardTooltipTextForTest().Contains(TEXT("点击后编入/移出")));
		LockedCard->OnUnhovered.Broadcast();
	}
	TestFalse(TEXT("the locked roster rejects committing a companion loadout"), Widget->ApplySelectedCompanionCardLoadout());
	TestFalse(TEXT("the locked roster rejects changing the active permanent partner"), Widget->SetSelectedCompanionAsActive());
	TestEqual(TEXT("the hero summary stays readable after lock"), Widget->GetHeroCardSummary().Num(), 8);
	TestEqual(TEXT("the task NPC summary stays readable after lock"), Widget->GetTaskNpcCardSummary().SelectedCardIds.Num(), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCompanionRosterWidgetTaskNpcFixedDeckReadOnlyTest,
	"GameXXK.UI.CompanionRoster.TaskNpcFixedDeckReadOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCompanionRosterWidgetTaskNpcFixedDeckReadOnlyTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	TestNotNull(TEXT("the task-NPC roster fixture creates its subsystem"), Subsystem);
	if (!Subsystem || !Subsystem->EnsureQingshanTownRuntimeForDirectMap())
	{
		return false;
	}

	const FGameXXKQuestNpcDefinition* TusiChief = FGameXXKCompanionCatalog::FindQuestNpcDefinition(TEXT("Npc.TusiChief"));
	TestNotNull(TEXT("the fixed-deck roster fixture resolves the named task NPC"), TusiChief);
	if (!TusiChief || !FGameXXKCardBattleAdapter::SetQuestNpcForCurrentRun(Subsystem->GetMutableRuntimeState(), TusiChief->NpcId, {}))
	{
		return false;
	}

	UGameXXKCompanionRosterWidget* Widget = BuildWidget(Subsystem);
	// Page 18 removes the dedicated task-NPC summary panel; the canonical
	// three-card selection remains facade-backed and read-only.
	TestEqual(TEXT("the task-NPC summary remains the canonical fixed three-card selection"),
		Widget->GetTaskNpcCardSummary().SelectedCardIds.Num(), 3);
	TestEqual(TEXT("the task-NPC summary retains the active temporary NPC identity"),
		Widget->GetTaskNpcCardSummary().NpcId, TusiChief->NpcId);
	TestNull(TEXT("the companion backpack does not expose a task-NPC loadout save action"),
		Widget->WidgetTree->FindWidget(TEXT("CompanionRosterApplyTaskNpcLoadout")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCompanionRosterWidgetFreshTownInitializationTest,
	"GameXXK.UI.CompanionRoster.FreshTownInitialization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCompanionRosterWidgetFreshTownInitializationTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	TestNotNull(TEXT("fresh-town subsystem exists"), Subsystem);
	if (!Subsystem)
	{
		return false;
	}

	TestTrue(TEXT("a fresh save starts through the world map"), Subsystem->StartGame());
	TestTrue(TEXT("the world-map town selection enters Qingshan"),
		Subsystem->SelectWorldRegion(UGameXXKMVPRules::RegionQingshan()));
	TestEqual(TEXT("the fresh fixture reaches town before opening the backpack"),
		Subsystem->GetRuntimeState().Screen, EGameXXKScreen::Town);

	UGameXXKCompanionRosterWidget* Widget = BuildWidget(Subsystem);
	TestNotNull(TEXT("the fresh-town companion backpack builds"), Widget);
	if (!Widget)
	{
		return false;
	}
	TestEqual(TEXT("opening the backpack initializes eight generic and all twenty-four profession-linked hero cards"),
		Subsystem->GetRuntimeState().CardRun.HeroUnlockedCardIds.Num(), 32);
	TestEqual(TEXT("opening the backpack initializes eight selected hero cards"),
		Subsystem->GetRuntimeState().CardRun.HeroSelectedCardIds.Num(), 8);
	TestEqual(TEXT("the backpack stages the initialized eight-card hero loadout"), Widget->GetPendingHeroCardIds().Num(), 8);

	const TArray<FGameXXKPermanentCompanion> StarterRoster = Subsystem->GetPermanentCompanionViews();
	TestEqual(TEXT("a fresh game exposes its two deterministic starter companions"), StarterRoster.Num(), 2);
	TestTrue(TEXT("the real random-recruit action succeeds from the initialized town backpack"), Widget->BeginRandomRecruitment());
	const TArray<FGameXXKPermanentCompanion> RecruitedRoster = Subsystem->GetPermanentCompanionViews();
	TestEqual(TEXT("the first successful recruit is added after the deterministic starter"), RecruitedRoster.Num(), StarterRoster.Num() + 1);
	if (RecruitedRoster.Num() != StarterRoster.Num() + 1)
	{
		return false;
	}
	TestEqual(TEXT("a successful random recruit is immediately selected for display"),
		Widget->GetSelectedCompanionIdForTest(), RecruitedRoster.Last().InstanceId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCompanionRosterWidgetProfileAndTownActionTest,
	"GameXXK.UI.CompanionRoster.ProfileExperienceAndTownOnlyClear",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCompanionRosterWidgetProfileAndTownActionTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	FGameXXKPermanentCompanion Companion;
	if (!RecruitCompanion(*this, Subsystem, 42031, Companion))
	{
		return false;
	}
	TestTrue(TEXT("the canonical facade records earned companion experience"),
		Subsystem->AwardPermanentCompanionExperience(Companion.InstanceId, 10));

	UGameXXKCompanionRosterWidget* Widget = BuildWidget(Subsystem);
	TestNotNull(TEXT("the profile/town-action backpack builds"), Widget);
	if (!Widget)
	{
		return false;
	}
	const FGameXXKCompanionRosterProfileView Profile = Widget->GetSelectedCompanionProfile();
	TestEqual(TEXT("the profile exposes saved companion experience"), Profile.Experience, 10);
	TestEqual(TEXT("the profile exposes the canonical next-level threshold"),
		Profile.ExperienceRequiredForNextLevel,
		FGameXXKCompanionRules::GetExperienceRequiredForNextLevel(Profile.Level));

	TestTrue(TEXT("the selected partner can be assigned before clearing"), Widget->SetSelectedCompanionAsActive());
	// Page 18 removes the standalone 暂不编入 button; the town-only capability stays reachable.
	TestTrue(TEXT("the town clear action succeeds through the retained capability"), Widget->ClearActivePermanentCompanion());
	TestTrue(TEXT("the town clear action removes the active permanent partner"),
		Subsystem->GetRuntimeState().CardRun.PartySelection.ActivePermanentCompanionInstanceId.IsNone());

	TestTrue(TEXT("the partner can be assigned again for the out-of-town safety check"), Widget->SetSelectedCompanionAsActive());
	Subsystem->GetMutableRuntimeState().Screen = EGameXXKScreen::WorldMap;
	TestFalse(TEXT("the clear action is rejected if invoked after leaving town"), Widget->ClearActivePermanentCompanion());
	TestEqual(TEXT("a rejected out-of-town clear preserves the active partner"),
		Subsystem->GetRuntimeState().CardRun.PartySelection.ActivePermanentCompanionInstanceId,
		Companion.InstanceId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCompanionRosterWidgetDuplicateRecruitmentFeedbackTest,
	"GameXXK.UI.CompanionRoster.DuplicateRecruitmentFeedback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCompanionRosterWidgetDuplicateRecruitmentFeedbackTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	TestNotNull(TEXT("duplicate-feedback subsystem exists"), Subsystem);
	if (!Subsystem || !Subsystem->EnsureQingshanTownRuntimeForDirectMap())
	{
		return false;
	}
	TestTrue(TEXT("the town roster is prepared before arranging a deterministic duplicate"),
		Subsystem->PrepareCompanionRosterForTown());

	FGameXXKCompanionRosterState SequenceProbe;
	FGameXXKCompanionRecruitResult FirstSequenceResult;
	TestTrue(TEXT("the first deterministic sequence candidate can seed the duplicate fixture"),
		FGameXXKCompanionRules::CreateAndResolveNextRecruitment(SequenceProbe, FirstSequenceResult, nullptr));
	if (FirstSequenceResult.Outcome != EGameXXKCompanionRecruitOutcome::Recruited)
	{
		AddError(TEXT("the duplicate fixture requires a first recruited sequence candidate"));
		return false;
	}
	Subsystem->GetMutableRuntimeState().CardRun.CompanionRoster.PermanentCompanions.Add(FirstSequenceResult.Companion);

	UGameXXKCompanionRosterWidget* Widget = BuildWidget(Subsystem);
	TestNotNull(TEXT("the duplicate-feedback backpack builds"), Widget);
	if (!Widget)
	{
		return false;
	}
	TestTrue(TEXT("the duplicate fixture can trigger the visible random-recruit action"), Widget->BeginRandomRecruitment());
	TestEqual(TEXT("a duplicate recruit is converted into exactly one persistent sigil"),
		Subsystem->GetPermanentCompanionSigilCount(), 1);
	TestTrue(TEXT("the backpack visibly explains duplicate-to-sigil conversion"),
		Widget->GetRecruitmentStatusForTest().Contains(TEXT("升星印")));
	return true;
}

#endif
