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
	TestNotNull(TEXT("companion roster builds a named three-column roster grid"),
		Widget && Widget->WidgetTree ? Cast<UUniformGridPanel>(Widget->WidgetTree->FindWidget(TEXT("CompanionRosterGrid"))) : nullptr);
	TestEqual(TEXT("the companion backpack always reserves twelve roster slots"), Widget->GetRosterSlotCountForTest(), 12);
	TestEqual(TEXT("the companion backpack fixes the roster at three columns"), Widget->GetRosterColumnCountForTest(), 3);
	TestTrue(TEXT("the window uses the separated PSD companion background"),
		Widget->GetWindowFrameResourcePathForTest().Contains(TEXT("/Game/GameXXK/UI/Town/Textures/PSD/Backgrounds/T_TownPsd_Background_Companion")));
	TestTrue(TEXT("the roster slots use the PSD companion card grammar"),
		Widget->GetRosterSlotResourcePathForTest().Contains(TEXT("/Game/GameXXK/UI/Town/Textures/PSD/Companion/T_TownPsd_CompanionCardFrame")));
	TestTrue(TEXT("personal cards use the un-tinted PSD057 card frame"),
		Widget->GetPersonalCardFrameResourcePathForTest().Contains(TEXT("/Game/GameXXK/UI/Cards/Textures/T_CardFrame_PSD057")));
	TestTrue(TEXT("the card list exposes a scroll-box reservation for the future PSD scroll bar"),
		Widget->HasPersonalCardScrollBoxForTest());
	TestTrue(TEXT("the card list applies the shared PSD paper scroll track"),
		Widget->GetPersonalCardScrollTrackResourcePathForTest().Contains(TEXT("/Game/GameXXK/UI/PartyDeck/Scrollbars/T_PartyDeck_ScrollPaperTrack")));
	TestTrue(TEXT("the card list applies the shared PSD ink scroll thumb"),
		Widget->GetPersonalCardScrollThumbResourcePathForTest().Contains(TEXT("/Game/GameXXK/UI/PartyDeck/Scrollbars/T_PartyDeck_ScrollInkThumb")));
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

	const FName* LockedPersonalCardId = VisibleCards.FindByPredicate([Widget](const FName CardId)
	{
		return !Widget->GetPendingPersonalCardIds().Contains(CardId);
	});
	FGameXXKPermanentCompanion* MutableCompanion = Subsystem->GetMutableRuntimeState().CardRun.CompanionRoster.PermanentCompanions.FindByPredicate([&Companion](const FGameXXKPermanentCompanion& Candidate)
	{
		return Candidate.InstanceId == Companion.InstanceId;
	});
	TestNotNull(TEXT("the personal deck fixture exposes a mutable saved companion for locked-card presentation"), MutableCompanion);
	TestNotNull(TEXT("the personal deck fixture has an excluded card to mark unavailable"), LockedPersonalCardId);
	if (MutableCompanion && LockedPersonalCardId)
	{
		MutableCompanion->UnlockedPersonalCardIds.RemoveSingle(*LockedPersonalCardId);
		Widget->RefreshFromState();
		Widget->HandleConfiguredCardHoverChanged(*LockedPersonalCardId, false, true);
		TestTrue(TEXT("an unavailable personal card states its exact locked reason"),
			Widget->GetCardTooltipTextForTest().Contains(TEXT("此牌尚未解锁。")));
		TestFalse(TEXT("an unavailable personal card never advertises a click interaction"),
			Widget->GetCardTooltipTextForTest().Contains(TEXT("点击后编入/移出")));
		Widget->HandleConfiguredCardHoverChanged(*LockedPersonalCardId, false, false);
	}

	UButton* SetActiveButton = Widget->WidgetTree ? Cast<UButton>(Widget->WidgetTree->FindWidget(TEXT("CompanionRosterSetActive"))) : nullptr;
	TestNotNull(TEXT("the selected companion exposes a real active-partner button"), SetActiveButton);
	if (SetActiveButton)
	{
		SetActiveButton->OnClicked.Broadcast();
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
	TestEqual(TEXT("the hero editor fixture owns the complete twelve-card hero pool"), HeroPool.Num(), 12);
	TestNotNull(TEXT("the hero editor fixture can probe a non-hero card id"), NonHeroDefinition);
	if (HeroPool.Num() != 12 || !NonHeroDefinition)
	{
		return false;
	}

	FGameXXKRuntimeState& RuntimeState = Subsystem->GetMutableRuntimeState();
	RuntimeState.CardRun.HeroUnlockedCardIds = HeroPool;
	RuntimeState.CardRun.HeroSelectedCardIds = FirstCards(HeroPool, 8);
	UGameXXKCompanionRosterWidget* Widget = BuildWidget(Subsystem);
	TestFalse(TEXT("the companion backpack initially shows the selected partner deck"), Widget->IsHeroDeckEditorOpenForTest());
	UButton* HeroDeckToggleButton = Widget->WidgetTree ? Cast<UButton>(Widget->WidgetTree->FindWidget(TEXT("CompanionRosterHeroDeckToggle"))) : nullptr;
	TestNotNull(TEXT("the companion backpack exposes a real hero-deck toggle"), HeroDeckToggleButton);
	if (!HeroDeckToggleButton)
	{
		return false;
	}
	HeroDeckToggleButton->OnClicked.Broadcast();
	TestTrue(TEXT("the real hero-deck toggle opens the player-editable hero deck"), Widget->IsHeroDeckEditorOpenForTest());
	HeroDeckToggleButton->OnClicked.Broadcast();
	TestFalse(TEXT("the same toggle returns to the selected partner deck"), Widget->IsHeroDeckEditorOpenForTest());
	TestTrue(TEXT("the companion backpack opens a dedicated player-editable hero deck"), Widget->OpenHeroDeckEditor());
	TestTrue(TEXT("the hero deck editor is active after opening"), Widget->IsHeroDeckEditorOpenForTest());
	TestEqual(TEXT("the hero editor renders all twelve available hero cards"), Widget->GetVisibleHeroCardIds().Num(), 12);
	TestEqual(TEXT("the hero editor stages the saved eight-card hero selection"), Widget->GetPendingHeroCardIds().Num(), 8);
	UUniformGridPanel* HeroCardGrid = Widget->WidgetTree ? Cast<UUniformGridPanel>(Widget->WidgetTree->FindWidget(TEXT("CompanionRosterPersonalCardGrid"))) : nullptr;
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
		return !PendingHeroCards.Contains(CardId);
	});
	TestNotNull(TEXT("the twelve-card hero pool has an excluded replacement card"), ReplacementCardId);
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
	const FName* NinthCardId = Widget->GetVisibleHeroCardIds().FindByPredicate([Widget](const FName CardId)
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

	UButton* ApplyHeroButton = Widget->WidgetTree ? Cast<UButton>(Widget->WidgetTree->FindWidget(TEXT("CompanionRosterApplyHeroLoadout"))) : nullptr;
	TestNotNull(TEXT("the hero deck editor exposes a real apply button"), ApplyHeroButton);
	if (!ApplyHeroButton)
	{
		return false;
	}
	const TArray<FName> StagedHeroCardsBeforeApply = Widget->GetPendingHeroCardIds();
	ApplyHeroButton->OnClicked.Broadcast();
	TestEqual(TEXT("the real apply button persists the player-selected hero cards in order"),
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
	UTextBlock* TaskNpcSummary = Widget && Widget->WidgetTree
		? Cast<UTextBlock>(Widget->WidgetTree->FindWidget(TEXT("CompanionRosterTaskNpcDeckSummary")))
		: nullptr;
	TestNotNull(TEXT("the companion backpack renders a dedicated task-NPC deck summary"), TaskNpcSummary);
	if (!TaskNpcSummary)
	{
		return false;
	}

	const FString SummaryText = TaskNpcSummary->GetText().ToString();
	TestTrue(TEXT("the task-NPC summary identifies its fixed support deck"), SummaryText.Contains(TEXT("固定支援牌组")));
	TestTrue(TEXT("the task-NPC summary visibly states that the deck is read-only"), SummaryText.Contains(TEXT("只读")));
	TestTrue(TEXT("the task-NPC summary retains the active temporary NPC identity"), SummaryText.Contains(TusiChief->NpcId.ToString()));
	TestEqual(TEXT("the task-NPC summary remains the canonical fixed three-card selection"),
		Widget->GetTaskNpcCardSummary().SelectedCardIds.Num(), 3);
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
	TestEqual(TEXT("opening the backpack initializes all twelve permanent hero cards"),
		Subsystem->GetRuntimeState().CardRun.HeroUnlockedCardIds.Num(), 12);
	TestEqual(TEXT("opening the backpack initializes eight selected hero cards"),
		Subsystem->GetRuntimeState().CardRun.HeroSelectedCardIds.Num(), 8);
	TestEqual(TEXT("the backpack stages the initialized eight-card hero loadout"), Widget->GetPendingHeroCardIds().Num(), 8);

	TestTrue(TEXT("the real random-recruit action succeeds from the initialized town backpack"), Widget->BeginRandomRecruitment());
	const TArray<FGameXXKPermanentCompanion> RecruitedRoster = Subsystem->GetPermanentCompanionViews();
	TestEqual(TEXT("the first successful recruit enters the permanent roster"), RecruitedRoster.Num(), 1);
	if (RecruitedRoster.Num() != 1)
	{
		return false;
	}
	TestEqual(TEXT("a successful random recruit is immediately selected for display"),
		Widget->GetSelectedCompanionIdForTest(), RecruitedRoster[0].InstanceId);
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
	UButton* ClearActiveButton = Widget->WidgetTree
		? Cast<UButton>(Widget->WidgetTree->FindWidget(TEXT("CompanionRosterClearActive")))
		: nullptr;
	TestNotNull(TEXT("the companion backpack exposes the town-only 暂不编入 action"), ClearActiveButton);
	if (!ClearActiveButton)
	{
		return false;
	}
	ClearActiveButton->OnClicked.Broadcast();
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
