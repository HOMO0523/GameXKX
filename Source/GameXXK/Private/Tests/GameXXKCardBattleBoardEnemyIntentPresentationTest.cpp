#include "GameXXKCardBattleAdapter.h"
#include "GameXXKCardRules.h"
#include "GameXXKMVPRules.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Misc/AutomationTest.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKBattleBoardWidget.h"
#include "UI/GameXXKInRunUiStyle.h"

#include <type_traits>
#include <utility>

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FGameXXKBattleRuntimeUnit MakeEnemyIntentTestEnemy(
		const TCHAR* Id,
		const TCHAR* DisplayName,
		const TCHAR* EnemyDefinitionId,
		const int32 BattleSlotNumber)
	{
		FGameXXKBattleRuntimeUnit Unit;
		Unit.Id = FName(Id);
		Unit.DisplayName = FText::FromString(DisplayName);
		Unit.EnemyDefinitionId = FName(EnemyDefinitionId);
		Unit.HP = 240;
		Unit.MaxHP = 240;
		Unit.Attack = 8;
		Unit.Defense = 0;
		Unit.Speed = 8;
		Unit.BattleSlotNumber = BattleSlotNumber;
		Unit.CombatLevel = 100;
		Unit.bEnemy = true;
		return Unit;
	}

	bool BuildThreeEnemyIntentPresentationFixture(UGameXXKMVPSubsystem* Subsystem, FString& OutError)
	{
		OutError.Reset();
		if (!Subsystem)
		{
			OutError = TEXT("The test subsystem is missing.");
			return false;
		}
		if (!Subsystem->StartGame())
		{
			OutError = TEXT("The test subsystem could not start a permanent party.");
			return false;
		}

		FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
		State.Screen = EGameXXKScreen::Battle;
		State.bHasActiveBattle = true;
		State.ActiveBattleNodeId = 41;
		State.ActiveBattleEnemies = {
			MakeEnemyIntentTestEnemy(TEXT("IntentEnemy.One"), TEXT("意图敌一"), TEXT("Enemy.Ch1.Rooster"), 1),
			MakeEnemyIntentTestEnemy(TEXT("IntentEnemy.Two"), TEXT("意图敌二"), TEXT("Enemy.Ch2.BlackBear"), 2),
			MakeEnemyIntentTestEnemy(TEXT("IntentEnemy.Three"), TEXT("意图敌三"), TEXT("Enemy.Ch3.Tiger"), 3)};

		return FGameXXKCardBattleAdapter::EnsureCardRunInitialized(State, &OutError)
			&& FGameXXKCardBattleAdapter::BeginCardBattle(
				State,
				EGameXXKNodeKind::Battle,
				EGameXXKCardTerrain::Plain,
				401,
				&OutError);
	}

	template <typename TBoard, typename = void>
	struct TEnemyIntentBoardPresentationApi
	{
		static constexpr bool bAvailable = false;
		static bool IsLocked(const TBoard*) { return false; }
		static FName Attacker(const TBoard*) { return NAME_None; }
		static FName Target(const TBoard*) { return NAME_None; }
	};

	template <typename TBoard>
	struct TEnemyIntentBoardPresentationApi<TBoard, std::void_t<
		decltype(std::declval<const TBoard&>().IsBattlePresentationLockedForTest()),
		decltype(std::declval<const TBoard&>().GetActiveBattlePresentationAttackerUnitIdForTest()),
		decltype(std::declval<const TBoard&>().GetActiveBattlePresentationTargetUnitIdForTest())>>
	{
		static constexpr bool bAvailable = true;
		static bool IsLocked(const TBoard* Board) { return Board->IsBattlePresentationLockedForTest(); }
		static FName Attacker(const TBoard* Board) { return Board->GetActiveBattlePresentationAttackerUnitIdForTest(); }
		static FName Target(const TBoard* Board) { return Board->GetActiveBattlePresentationTargetUnitIdForTest(); }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCardBattleBoardEnemyIntentPresentationTest,
	"GameXXK.Integration.CardBattle.BoardEnemyIntentPresentation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardBattleBoardEnemyIntentPresentationTest::RunTest(const FString& Parameters)
{
	using FPresentationApi = TEnemyIntentBoardPresentationApi<UGameXXKBattleBoardWidget>;
	TestTrue(TEXT("enemy-intent Board exposes the shared presentation gate"), FPresentationApi::bAvailable);
	if (!FPresentationApi::bAvailable)
	{
		return false;
	}

	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	FString Error;
	TestTrue(FString::Printf(TEXT("three-enemy intent fixture enters a card battle: %s"), *Error),
		BuildThreeEnemyIntentPresentationFixture(Subsystem, Error));

	UGameXXKBattleBoardWidget* Board = NewObject<UGameXXKBattleBoardWidget>();
	Board->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("enemy-intent board initializes its widget tree"), Board->Initialize());
	Board->NativeConstruct();
	Board->RefreshFromState();

	FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
	TestEqual(TEXT("the intent rail is already forecast during the player phase"),
		State.CardRun.ActiveBattle.Phase,
		EGameXXKCardBattlePhase::Player);
	TestEqual(TEXT("the player phase stores one intent for every living enemy"), State.CardRun.EnemyIntents.Num(), 3);
	TestEqual(TEXT("the player phase immediately shows every living enemy intent"), Board->GetVisibleEnemyIntentCardCountForTest(), 3);
	TestEqual(TEXT("player-phase intent cards retain the fixed source order"), Board->GetEnemyIntentSlotLabelForTest(0), FString(TEXT("敌 1P")));
	TestEqual(TEXT("player-phase intent cards retain the fixed source order"), Board->GetEnemyIntentSlotLabelForTest(1), FString(TEXT("敌 2P")));
	TestEqual(TEXT("player-phase intent cards retain the fixed source order"), Board->GetEnemyIntentSlotLabelForTest(2), FString(TEXT("敌 3P")));
	TestEqual(TEXT("normal enemy intents use the matching left-anchored card portrait"),
		Board->GetEnemyIntentPortraitResourcePathForTest(0),
		FString(TEXT("/Game/GameXXK/UI/Battle/EnemyCardArt/T_CardPortrait_Enemy_Ch1_Rooster.T_CardPortrait_Enemy_Ch1_Rooster")));
	TestEqual(TEXT("chapter boss intents use the matching real boss card portrait"),
		Board->GetEnemyIntentPortraitResourcePathForTest(1),
		FString(TEXT("/Game/GameXXK/UI/Battle/EnemyCardArt/T_CardPortrait_Enemy_Ch2_BlackBearBoss.T_CardPortrait_Enemy_Ch2_BlackBearBoss")));
	TestEqual(TEXT("the final tiger boss intent uses the approved final-idle tiger portrait"),
		Board->GetEnemyIntentPortraitResourcePathForTest(2),
		FString(TEXT("/Game/GameXXK/UI/Battle/EnemyCardArt/T_CardPortrait_Enemy_Ch3_TigerBoss.T_CardPortrait_Enemy_Ch3_TigerBoss")));
	UImage* FirstIntentPortrait = Board->WidgetTree
		? Cast<UImage>(Board->WidgetTree->FindWidget(TEXT("BattleEnemyIntentCard_00Portrait")))
		: nullptr;
	TestTrue(TEXT("enemy intent card builds a dedicated portrait image layer"),
		FirstIntentPortrait && FirstIntentPortrait->GetVisibility() == ESlateVisibility::HitTestInvisible);
	const UCanvasPanelSlot* PortraitSlot = FirstIntentPortrait ? Cast<UCanvasPanelSlot>(FirstIntentPortrait->Slot) : nullptr;
	const UTexture2D* PortraitTexture = FirstIntentPortrait ? Cast<UTexture2D>(FirstIntentPortrait->GetBrush().GetResourceObject()) : nullptr;
	if (PortraitSlot && PortraitTexture)
		AddInfo(FString::Printf(TEXT("intent art %.1fx%.1f texture %dx%d opacity %.2f layer %d"),
			PortraitSlot->GetSize().X, PortraitSlot->GetSize().Y, PortraitTexture->GetSizeX(), PortraitTexture->GetSizeY(),
			FirstIntentPortrait->GetRenderOpacity(), PortraitSlot->GetZOrder()));
	TestTrue(TEXT("enemy art is large, proportional and behind the information at 70 percent opacity"),
		PortraitSlot && PortraitTexture && PortraitSlot->GetSize().Y >= 200
		&& FMath::IsNearlyEqual(PortraitSlot->GetSize().X / PortraitSlot->GetSize().Y,
			PortraitTexture->GetSizeX() > 0 && PortraitTexture->GetSizeY() > 0
				? static_cast<float>(PortraitTexture->GetSizeX()) / PortraitTexture->GetSizeY() : 171.0f / 205.0f, 0.001f)
		&& PortraitSlot->GetZOrder() == 0 && FMath::IsNearlyEqual(FirstIntentPortrait->GetRenderOpacity(), 0.7f));
	TestEqual(TEXT("the player-phase forecast has no active enemy showcase"), Board->GetActiveEnemyIntentPresentationIndexForTest(), INDEX_NONE);
	if (State.CardRun.EnemyIntents.Num() != 3)
	{
		return false;
	}

	FGameXXKCardEnemyIntent& RichIntent = State.CardRun.EnemyIntents[0];
	const FGameXXKCardEnemyIntent OriginalIntent = RichIntent;
	RichIntent.CardDisplayName = TEXT("臭雾");
	RichIntent.Damage = 0;
	RichIntent.TargetRule = EGameXXKEnemyIntentTargetRule::AllLivingParty;
	RichIntent.OnHitStatuses.Reset();
	RichIntent.Effects.Reset();
	FGameXXKResolvedEnemyIntentEffect WeakEffect;
	WeakEffect.Type = EGameXXKEnemyIntentEffectType::ApplyStatus;
	WeakEffect.TargetRule = EGameXXKEnemyIntentTargetRule::AllLivingParty;
	WeakEffect.TargetUnitIds = {TEXT("Player"), TEXT("Npc.TusiChief")};
	WeakEffect.Status = EGameXXKCardStatus::Weak;
	WeakEffect.StatusStacks = 1;
	RichIntent.Effects.Add(WeakEffect);
	Board->RefreshFromState();
	UTextBlock* FirstIntentBody = Board->WidgetTree
		? Cast<UTextBlock>(Board->WidgetTree->FindWidget(TEXT("BattleEnemyIntentCard_00Body")))
		: nullptr;
	auto VisibleIntentText = [&]()
	{
		FString Result;
		for (const TCHAR* Suffix : {TEXT("Target"), TEXT("Primary"), TEXT("PrimaryLabel"), TEXT("Body")})
			if (const UTextBlock* Label = Cast<UTextBlock>(Board->WidgetTree->FindWidget(*(FString(TEXT("BattleEnemyIntentCard_00")) + Suffix))))
				Result += Label->GetText().ToString() + TEXT("\n");
		return Result;
	};
	const FString StinkFogBody = VisibleIntentText();
	const FString StinkFogTooltip = Board->GetEnemyIntentTooltipForTest(0);
	TestTrue(TEXT("status-only intent card names its all-party weak effect"),
		StinkFogBody.Contains(TEXT("我方全体")) && StinkFogBody.Contains(TEXT("虚弱")) && StinkFogBody.Contains(TEXT("1层")));
	UImage* PrimaryStatusIcon = Cast<UImage>(Board->WidgetTree->FindWidget(TEXT("BattleEnemyIntentCard_00PrimaryStatusIcon")));
	UTextBlock* PrimaryStatusValue = Cast<UTextBlock>(Board->WidgetTree->FindWidget(TEXT("BattleEnemyIntentCard_00PrimaryStatusValue")));
	UTextBlock* PrimaryStatusName = Cast<UTextBlock>(Board->WidgetTree->FindWidget(TEXT("BattleEnemyIntentCard_00PrimaryStatusName")));
	UCanvasPanel* PrimaryStatusRow = Cast<UCanvasPanel>(Board->WidgetTree->FindWidget(TEXT("BattleEnemyIntentCard_00PrimaryStatusRow")));
	TestTrue(TEXT("status-only intentions have a real icon beside a one-line stack count"),
		PrimaryStatusIcon && PrimaryStatusIcon->GetBrush().GetResourceObject()
		&& PrimaryStatusValue && PrimaryStatusValue->GetText().ToString() == TEXT("1层")
		&& !PrimaryStatusValue->GetAutoWrapText());
	TestTrue(TEXT("status rows explicitly name the status before the value and place the larger icon last"),
		PrimaryStatusName && PrimaryStatusName->GetText().ToString() == TEXT("虚弱")
		&& PrimaryStatusRow && PrimaryStatusRow->GetChildrenCount() == 3
		&& PrimaryStatusIcon && PrimaryStatusRow->GetChildAt(2) == PrimaryStatusIcon->GetParent()->GetParent()
		&& PrimaryStatusValue && PrimaryStatusValue->GetFont().Size >= 30);
	UBorder* IntentTooltipPaper = Cast<UBorder>(Board->WidgetTree->FindWidget(TEXT("BattleEnemyIntentDetailPanel")));
	TestTrue(TEXT("enemy intent tooltip shares the original ItemSlot paper and slice"),
		IntentTooltipPaper && IntentTooltipPaper->Background.GetResourceObject()
		&& IntentTooltipPaper->Background.GetResourceObject()->GetPathName().Contains(TEXT("T_MasterV2_ItemSlot"))
		&& IntentTooltipPaper->Background.Margin == FMargin(0.065f));
	UTextBlock* SideWord = Cast<UTextBlock>(Board->WidgetTree->FindWidget(TEXT("BattleEnemyIntentSlotLabel_00")));
	UTextBlock* SideNumber = Cast<UTextBlock>(Board->WidgetTree->FindWidget(TEXT("BattleEnemyIntentSlotNumber_00")));
	TestTrue(TEXT("side name and outlined slot identifier use separate rows"), SideWord && SideNumber
		&& SideWord->GetText().ToString() == TEXT("敌方") && SideNumber->GetText().ToString() == TEXT("1P")
		&& SideNumber->GetFont().OutlineSettings.OutlineSize > 0 && !SideNumber->GetAutoWrapText());
	TestFalse(TEXT("status-only intent card never claims zero damage"), StinkFogBody.Contains(TEXT("伤害 0")));
	TestTrue(TEXT("status-only intent tooltip names the saved weak effect"), StinkFogTooltip.Contains(TEXT("虚弱 1层")));
	TestFalse(TEXT("status-only intent tooltip never claims zero base damage"), StinkFogTooltip.Contains(TEXT("基础伤害 0")));

	RichIntent = OriginalIntent;
	RichIntent.CardDisplayName = TEXT("遁逃");
	RichIntent.Damage = 0;
	RichIntent.TargetRule = EGameXXKEnemyIntentTargetRule::Self;
	RichIntent.Effects.Reset();
	FGameXXKResolvedEnemyIntentEffect SpeedEffect;
	SpeedEffect.Type = EGameXXKEnemyIntentEffectType::ModifySpeed;
	SpeedEffect.TargetRule = EGameXXKEnemyIntentTargetRule::Self;
	SpeedEffect.TargetUnitIds = {RichIntent.SourceUnitId};
	SpeedEffect.Magnitude = 1;
	RichIntent.Effects.Add(SpeedEffect);
	FGameXXKResolvedEnemyIntentEffect ArmorEffect;
	ArmorEffect.Type = EGameXXKEnemyIntentEffectType::AddArmor;
	ArmorEffect.TargetRule = EGameXXKEnemyIntentTargetRule::Self;
	ArmorEffect.TargetUnitIds = {RichIntent.SourceUnitId};
	ArmorEffect.Magnitude = 5;
	RichIntent.Effects.Add(ArmorEffect);
	Board->RefreshFromState();
	const FString EscapeBody = VisibleIntentText();
	TestTrue(TEXT("non-damage intent card lists its saved speed and armor effects"),
		EscapeBody.Contains(TEXT("速度")) && EscapeBody.Contains(TEXT("+1")) && EscapeBody.Contains(TEXT("+5护甲")));
	TestFalse(TEXT("non-damage intent card never claims zero damage"), EscapeBody.Contains(TEXT("伤害 0")));

	RichIntent = OriginalIntent;
	RichIntent.CardDisplayName = TEXT("双重啄击");
	RichIntent.Damage = 37;
	RichIntent.Effects.Reset();
	FGameXXKResolvedEnemyIntentEffect MultiHitEffect;
	MultiHitEffect.Type = EGameXXKEnemyIntentEffectType::DirectDamage;
	MultiHitEffect.TargetRule = EGameXXKEnemyIntentTargetRule::LowestHealthParty;
	MultiHitEffect.TargetUnitIds = {RichIntent.SuggestedTargetUnitId};
	MultiHitEffect.Magnitude = 37;
	MultiHitEffect.HitCount = 2;
	RichIntent.Effects.Add(MultiHitEffect);
	Board->RefreshFromState();
	const FString DoublePeckBody = VisibleIntentText();
	TestTrue(TEXT("multi-hit intent card shows the saved per-hit magnitude and hit count"),
		DoublePeckBody.Contains(TEXT("37 × 2")) && DoublePeckBody.Contains(TEXT("伤害")));
	UTextBlock* PrimaryValue = Cast<UTextBlock>(Board->WidgetTree->FindWidget(TEXT("BattleEnemyIntentCard_00Primary")));
	UTextBlock* PrimaryDamageLabel = Cast<UTextBlock>(Board->WidgetTree->FindWidget(TEXT("BattleEnemyIntentCard_00PrimaryLabel")));
	const UCanvasPanelSlot* NumberSlot = PrimaryValue ? Cast<UCanvasPanelSlot>(PrimaryValue->Slot) : nullptr;
	const UCanvasPanelSlot* DamageLabelSlot = PrimaryDamageLabel && PrimaryDamageLabel->GetParent()
		? Cast<UCanvasPanelSlot>(PrimaryDamageLabel->GetParent()->Slot) : nullptr;
	TestTrue(TEXT("damage number remains centered with a small suffix on its right"),
		NumberSlot && DamageLabelSlot && FMath::IsNearlyEqual(NumberSlot->GetPosition().X + NumberSlot->GetSize().X*0.5f, 103.0f)
		&& DamageLabelSlot->GetPosition().X > 103.0f
		&& PrimaryDamageLabel && PrimaryDamageLabel->GetText().ToString() == TEXT("伤害")
		&& PrimaryDamageLabel->GetFont().Size == 18 && !PrimaryDamageLabel->GetAutoWrapText());
	TestTrue(TEXT("damage is larger than the supporting text and uses the body font"),
		PrimaryValue && FirstIntentBody && PrimaryValue->GetFont().Size >= 29
		&& PrimaryValue->GetFont().Size > FirstIntentBody->GetFont().Size
		&& PrimaryValue->GetFont().FontObject == FGameXXKInRunUiStyle::BodyFont(34, true).FontObject);

	RichIntent = OriginalIntent;
	RichIntent.CardDisplayName = TEXT("毒牙突袭");
	FGameXXKCardStatusStack PoisonStatus;
	PoisonStatus.Status = EGameXXKCardStatus::Poison;
	PoisonStatus.Stacks = 2;
	RichIntent.OnHitStatuses = {PoisonStatus};
	if (!RichIntent.Effects.IsEmpty())
	{
		RichIntent.Effects[0].Magnitude = 8;
		RichIntent.Effects[0].Status = EGameXXKCardStatus::Poison;
		RichIntent.Effects[0].StatusStacks = 2;
	}
	Board->RefreshFromState();
	const FString FirstIntentTooltip = Board->GetEnemyIntentTooltipForTest(0);
	TestTrue(TEXT("the pending intent tooltip starts with its saved skill"), FirstIntentTooltip.StartsWith(TEXT("毒牙突袭")));
	TestTrue(TEXT("the pending intent tooltip uses its saved central hero P label"), FirstIntentTooltip.Contains(TEXT("我 2P")));
	TestTrue(TEXT("the pending intent tooltip identifies its target on a dedicated row"), FirstIntentTooltip.Contains(TEXT("对象：我 2P")));
	TestTrue(TEXT("the pending intent tooltip uses its resolved damage"), FirstIntentTooltip.Contains(TEXT("8伤害")));
	TestTrue(TEXT("the pending intent tooltip preserves direct saved status"), FirstIntentTooltip.Contains(TEXT("命中附加中毒 2")));
	TestFalse(TEXT("target armor settlement stays in the unit tooltip"), FirstIntentTooltip.Contains(TEXT("护甲结算")));
	TestFalse(TEXT("the intent tooltip does not repeat its visible source card"), FirstIntentTooltip.Contains(TEXT("攻击者：")));
	UButton* FirstHandCard = Board->WidgetTree ? Cast<UButton>(Board->WidgetTree->FindWidget(TEXT("BattleHandCard_00"))) : nullptr;
	UButton* EndTurnButton = Board->WidgetTree ? Cast<UButton>(Board->WidgetTree->FindWidget(TEXT("BattleEndTurnButton"))) : nullptr;
	TestTrue(TEXT("the player hand remains enabled while the player reads the forecast"), FirstHandCard && FirstHandCard->GetIsEnabled());
	TestTrue(TEXT("the end-turn control remains enabled while the player reads the forecast"), EndTurnButton && EndTurnButton->GetIsEnabled());
	const FName ForecastCardId = RichIntent.CardId;
	const FName ForecastSourceUnitId = RichIntent.SourceUnitId;
	const FName ForecastSuggestedTargetUnitId = RichIntent.SuggestedTargetUnitId;
	const EGameXXKCardStatus ForecastStatus = PoisonStatus.Status;
	const int32 ForecastStatusStacks = PoisonStatus.Stacks;

	TestTrue(TEXT("end turn executes the already visible saved enemy intent rail"), Board->EndCardPlayerPhase());
	TestEqual(TEXT("ending the player phase enters enemy presentation"), State.CardRun.ActiveBattle.Phase, EGameXXKCardBattlePhase::Enemy);
	TestEqual(TEXT("ending the player phase keeps every saved enemy intent"), State.CardRun.EnemyIntents.Num(), 3);
	TestEqual(TEXT("ending the player phase starts from the first saved intent"), State.CardRun.NextEnemyIntentIndex, 0);
	if (State.CardRun.EnemyIntents.Num() != 3)
	{
		return false;
	}

	const FGameXXKCardEnemyIntent& SavedIntentAfterEnd = State.CardRun.EnemyIntents[0];
	TestEqual(TEXT("end turn keeps the forecast card identity"), SavedIntentAfterEnd.CardId, ForecastCardId);
	TestEqual(TEXT("end turn keeps the forecast source"), SavedIntentAfterEnd.SourceUnitId, ForecastSourceUnitId);
	TestEqual(TEXT("end turn keeps the forecast target"), SavedIntentAfterEnd.SuggestedTargetUnitId, ForecastSuggestedTargetUnitId);
	TestTrue(TEXT("end turn keeps the forecast poison status"),
		SavedIntentAfterEnd.OnHitStatuses.ContainsByPredicate([ForecastStatus, ForecastStatusStacks](const FGameXXKCardStatusStack& Status)
		{
			return Status.Status == ForecastStatus && Status.Stacks == ForecastStatusStacks;
		}));
	TestEqual(TEXT("the first saved intent begins as the current showcase"), Board->GetActiveEnemyIntentPresentationIndexForTest(), 0);
	TestFalse(TEXT("the player hand is disabled while enemy intents display"), FirstHandCard && FirstHandCard->GetIsEnabled());
	TestFalse(TEXT("the end-turn control is disabled while enemy intents display"), EndTurnButton && EndTurnButton->GetIsEnabled());
	UButton* Showcase = Cast<UButton>(Board->WidgetTree->FindWidget(TEXT("BattleEnemyIntentShowcaseCard")));
	UImage* ShowcasePortrait = Cast<UImage>(Board->WidgetTree->FindWidget(TEXT("BattleEnemyIntentShowcasePortrait")));
	TestTrue(TEXT("the central intent uses the same real monster portrait as its forecast"),
		ShowcasePortrait && FirstIntentPortrait
		&& ShowcasePortrait->GetBrush().GetResourceObject() == FirstIntentPortrait->GetBrush().GetResourceObject()
		&& ShowcasePortrait->GetVisibility() == ESlateVisibility::HitTestInvisible);

	double PresentationClock = 1000.0;
	for (int32 IntentIndex = 0; IntentIndex < 3; ++IntentIndex)
	{
		if (IntentIndex == 0)
		{
			const FWidgetTransform EntryPose = Showcase ? Showcase->GetRenderTransform() : FWidgetTransform();
			Board->AdvanceEnemyIntentPresentationForTest(0.18f);
			TestTrue(TEXT("central intent moves out of the rail and fades into its reveal"),
				Showcase && Showcase->GetRenderTransform().Translation != EntryPose.Translation
				&& Showcase->GetRenderTransform().Scale.X > EntryPose.Scale.X && Showcase->GetRenderOpacity() > 0.9f);
			Board->AdvanceEnemyIntentPresentationForTest(0.37f);
		}
		else Board->AdvanceEnemyIntentPresentationForTest(0.55f);
		TestEqual(FString::Printf(TEXT("intent %d waits for its resolve interval"), IntentIndex + 1),
			Subsystem->GetRuntimeState().CardRun.NextEnemyIntentIndex,
			IntentIndex);
		Board->AdvanceEnemyIntentPresentationForTest(0.18f);
		TestEqual(FString::Printf(TEXT("intent %d resolves exactly once"), IntentIndex + 1),
			Subsystem->GetRuntimeState().CardRun.NextEnemyIntentIndex,
			IntentIndex + 1);
		if (IntentIndex < 2)
		{
			TestTrue(FString::Printf(TEXT("damaging intent %d locks its Board-owned Attack/Hit presentation"), IntentIndex + 1),
				FPresentationApi::IsLocked(Board));
			const int32 IndexAfterMutation = Subsystem->GetRuntimeState().CardRun.NextEnemyIntentIndex;
			Board->AdvanceEnemyIntentPresentationForTest(999.0f);
			TestEqual(FString::Printf(TEXT("damaging intent %d cannot advance again under a large delta while presentation is pending"), IntentIndex + 1),
				Subsystem->GetRuntimeState().CardRun.NextEnemyIntentIndex,
				IndexAfterMutation);
			Board->AdvanceVisualsAtRealTime(PresentationClock);
			Board->AdvanceVisualsAtRealTime(PresentationClock + 100.0);
			PresentationClock += 100.0;
			TestFalse(FString::Printf(TEXT("damaging intent %d unlocks only after its Attack/Hit timeline drains"), IntentIndex + 1),
				FPresentationApi::IsLocked(Board));
		}
		else
		{
			TestFalse(TEXT("the third non-damaging mutation updates state without a status close-up"),
				FPresentationApi::IsLocked(Board));
			FGameXXKCardCombatUnit* const DotEnemy = State.CardRun.ActiveBattle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
			{
				return Unit.UnitId == TEXT("IntentEnemy.Three");
			});
			TestNotNull(TEXT("phase-completion DOT target remains in the persistent enemy formation"), DotEnemy);
			if (DotEnemy)
			{
				TestEqual(TEXT("phase-completion fixture adds two persistent poison stacks"),
					GameXXKCardRules::AddCombatStatus(*DotEnemy, EGameXXKCardStatus::Poison, 2),
					2);
			}
		}
		Board->AdvanceEnemyIntentPresentationForTest(0.32f);
		if (IntentIndex < 2)
		{
			TestEqual(FString::Printf(TEXT("intent %d settles into the next showcase"), IntentIndex + 1),
				Board->GetActiveEnemyIntentPresentationIndexForTest(),
				IntentIndex + 1);
		}
	}

	TestTrue(TEXT("enemy-phase completion DOT owns the same presentation gate"), FPresentationApi::IsLocked(Board));
	const FGameXXKCardCombatUnit* const DotTargetBeforeMarker = State.CardRun.ActiveBattle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("IntentEnemy.Three");
	});
	const int32 DotHealthAfter = DotTargetBeforeMarker ? DotTargetBeforeMarker->HP : 0;
	Board->AdvanceVisualsAtRealTime(PresentationClock);
	TestEqual(TEXT("phase-completion DOT omits the attacker visual identity"), FPresentationApi::Attacker(Board), NAME_None);
	TestEqual(TEXT("phase-completion DOT retains its target identity"), FPresentationApi::Target(Board), FName(TEXT("IntentEnemy.Three")));
	TestEqual(TEXT("target-only DOT seeds its packet-local pre-damage HUD"), Board->GetDisplayedHealthForTest(TEXT("IntentEnemy.Three")), DotHealthAfter + 2);
	Board->AdvanceEnemyIntentPresentationForTest(999.0f);
	TestTrue(TEXT("large enemy-intent deltas remain blocked throughout target-only DOT"), FPresentationApi::IsLocked(Board));
	Board->AdvanceVisualsAtRealTime(PresentationClock + 1.1);
	TestEqual(TEXT("target-only DOT marker applies its post-packet health"), Board->GetDisplayedHealthForTest(TEXT("IntentEnemy.Three")), DotHealthAfter);
	Board->AdvanceVisualsAtRealTime(PresentationClock + 100.0);
	PresentationClock += 100.0;
	TestFalse(TEXT("phase-completion DOT drains and resumes finalization once"), FPresentationApi::IsLocked(Board));

	TestEqual(TEXT("the presentation completes into a stable next player phase"),
		State.CardRun.ActiveBattle.Phase,
		EGameXXKCardBattlePhase::Player);
	TestEqual(TEXT("the stable next player phase immediately stores a new forecast"), State.CardRun.EnemyIntents.Num(), 3);
	TestEqual(TEXT("the stable next player phase immediately shows its new forecast"), Board->GetVisibleEnemyIntentCardCountForTest(), 3);
	TestEqual(TEXT("the next player phase has no active enemy showcase"), Board->GetActiveEnemyIntentPresentationIndexForTest(), INDEX_NONE);
	TestTrue(TEXT("the next player phase leaves no faded or transformed showcase for reuse"),
		Showcase && Showcase->GetVisibility() == ESlateVisibility::Collapsed
		&& Showcase->GetRenderTransform() == FWidgetTransform() && Showcase->GetRenderOpacity() == 1.0f);
	TestTrue(TEXT("the end-turn control re-enables after the enemy sequence completes"), EndTurnButton && EndTurnButton->GetIsEnabled());

	FGameXXKCardCombatUnit* DefeatedEnemy = State.CardRun.ActiveBattle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("IntentEnemy.Two");
	});
	TestTrue(TEXT("the second intent enemy exists before it is defeated"), DefeatedEnemy != nullptr);
	if (!DefeatedEnemy)
	{
		return false;
	}

	DefeatedEnemy->bLiving = false;
	DefeatedEnemy->HP = 0;
	Board->RefreshFromState();
	TestEqual(TEXT("a defeated enemy disappears from the player-phase intent rail"), Board->GetVisibleEnemyIntentCardCountForTest(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCardBattleBoardEnemyIntentDeathLabelsTest,
	"GameXXK.Integration.CardBattle.BoardEnemyIntentDeathLabels",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardBattleBoardEnemyIntentDeathLabelsTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	FString Error;
	if (!TestTrue(TEXT("three-enemy label fixture starts"), BuildThreeEnemyIntentPresentationFixture(Subsystem, Error))) return false;
	UGameXXKBattleBoardWidget* Board = NewObject<UGameXXKBattleBoardWidget>();
	Board->SetMVPSubsystem(Subsystem);
	Board->Initialize();
	Board->NativeConstruct();
	const FGameXXKRuntimeState Baseline = Subsystem->GetRuntimeState();
	const FName Ids[] = {TEXT("IntentEnemy.One"), TEXT("IntentEnemy.Two"), TEXT("IntentEnemy.Three")};
	// First dies, two die, all die, reuse, middle dies, last dies.
	for (const int32 LivingMask : {7, 6, 4, 0, 7, 5, 3})
	{
		FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
		State = Baseline;
		TArray<FString> ExpectedNumbers;
		for (int32 Index = 0; Index < 3; ++Index)
		{
			if ((LivingMask & (1 << Index)) != 0)
			{
				ExpectedNumbers.Add(FString::Printf(TEXT("%dP"), Index + 1));
				continue;
			}
			FGameXXKCardCombatUnit* Unit = State.CardRun.ActiveBattle.Units.FindByPredicate(
				[&](const FGameXXKCardCombatUnit& Candidate) { return Candidate.UnitId == Ids[Index]; });
			if (!TestNotNull(TEXT("defeated enemy exists"), Unit)) return false;
			Unit->HP = 0;
			Unit->bLiving = false;
		}
		Board->RefreshFromState();
		TestEqual(TEXT("intent count matches survivors"), Board->GetVisibleEnemyIntentCardCountForTest(), ExpectedNumbers.Num());
		for (int32 Index = 0; Index < 3; ++Index)
		{
			UWidget* Slot = Board->WidgetTree->FindWidget(*FString::Printf(TEXT("BattleEnemyIntentSlot_%02d"), Index));
			UTextBlock* Number = Cast<UTextBlock>(Board->WidgetTree->FindWidget(*FString::Printf(TEXT("BattleEnemyIntentSlotNumber_%02d"), Index)));
			if (!TestNotNull(TEXT("intent wrapper exists"), Slot) || !TestNotNull(TEXT("slot number exists"), Number)) return false;
			if (ExpectedNumbers.IsValidIndex(Index))
			{
				TestEqual(TEXT("surviving card and label share a visible wrapper"), Slot->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
				TestEqual(TEXT("survivors retain their original slot number once"), Number->GetText().ToString(), ExpectedNumbers[Index]);
			}
			else
			{
				TestEqual(TEXT("empty card collapses its label and spacing too"), Slot->GetVisibility(), ESlateVisibility::Collapsed);
				TestTrue(TEXT("empty slot clears its stale number"), Number->GetText().IsEmpty());
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCardBattleBoardEnemyIntentRefreshResumeTest,
	"GameXXK.Integration.CardBattle.BoardEnemyIntentRefreshResume",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardBattleBoardEnemyIntentRefreshResumeTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	FString Error;
	TestTrue(FString::Printf(TEXT("enemy-intent refresh-resume fixture enters a card battle: %s"), *Error),
		BuildThreeEnemyIntentPresentationFixture(Subsystem, Error));

	UGameXXKBattleBoardWidget* InitialBoard = NewObject<UGameXXKBattleBoardWidget>();
	InitialBoard->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("initial enemy-intent board initializes its widget tree"), InitialBoard->Initialize());
	InitialBoard->NativeConstruct();
	TestTrue(TEXT("initial board stores the enemy phase before reconstruction"), InitialBoard->EndCardPlayerPhase());
	TestEqual(TEXT("the reconstructed runtime starts at the first pending saved intent"),
		Subsystem->GetRuntimeState().CardRun.NextEnemyIntentIndex,
		0);

	UGameXXKBattleBoardWidget* ReconstructedBoard = NewObject<UGameXXKBattleBoardWidget>();
	ReconstructedBoard->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("reconstructed enemy-intent board initializes its widget tree"), ReconstructedBoard->Initialize());
	ReconstructedBoard->NativeConstruct();
	ReconstructedBoard->RefreshFromState();
	TestEqual(TEXT("refresh rebuilds the full pending enemy-intent rail"), ReconstructedBoard->GetVisibleEnemyIntentCardCountForTest(), 3);
	TestEqual(TEXT("refresh resumes at the saved first pending intent"), ReconstructedBoard->GetActiveEnemyIntentPresentationIndexForTest(), 0);
	UButton* FirstHandCard = ReconstructedBoard->WidgetTree ? Cast<UButton>(ReconstructedBoard->WidgetTree->FindWidget(TEXT("BattleHandCard_00"))) : nullptr;
	UButton* EndTurnButton = ReconstructedBoard->WidgetTree ? Cast<UButton>(ReconstructedBoard->WidgetTree->FindWidget(TEXT("BattleEndTurnButton"))) : nullptr;
	TestFalse(TEXT("reconstructed presentation locks card input while the intent advances"), FirstHandCard && FirstHandCard->GetIsEnabled());
	TestFalse(TEXT("reconstructed presentation locks end turn while the intent advances"), EndTurnButton && EndTurnButton->GetIsEnabled());

	ReconstructedBoard->AdvanceEnemyIntentPresentationForTest(0.30f);
	ReconstructedBoard->RefreshFromState();
	ReconstructedBoard->AdvanceEnemyIntentPresentationForTest(0.25f);
	ReconstructedBoard->AdvanceEnemyIntentPresentationForTest(0.18f);
	TestEqual(TEXT("the resumed presentation consumes the saved first intent exactly once"),
		Subsystem->GetRuntimeState().CardRun.NextEnemyIntentIndex,
		1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCardBattleBoardEnemyIntentRecoveryTest,
	"GameXXK.Integration.CardBattle.BoardEnemyIntentRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardBattleBoardEnemyIntentRecoveryTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	FString Error;
	TestTrue(FString::Printf(TEXT("enemy-intent recovery fixture enters a card battle: %s"), *Error),
		BuildThreeEnemyIntentPresentationFixture(Subsystem, Error));

	UGameXXKBattleBoardWidget* Board = NewObject<UGameXXKBattleBoardWidget>();
	Board->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("enemy-intent recovery board initializes its widget tree"), Board->Initialize());
	Board->NativeConstruct();
	Board->RefreshFromState();
	TestTrue(TEXT("recovery fixture prepares saved enemy intents"), Board->EndCardPlayerPhase());

	FGameXXKResolvedEnemyIntentEffect* MalformedDamageEffect =
		Subsystem->GetMutableRuntimeState().CardRun.EnemyIntents[0].Effects.FindByPredicate(
			[](const FGameXXKResolvedEnemyIntentEffect& Effect)
			{
				return Effect.Type == EGameXXKEnemyIntentEffectType::DirectDamage;
			});
	TestNotNull(TEXT("malformed-intent fixture retains a catalog direct-damage effect"), MalformedDamageEffect);
	if (!MalformedDamageEffect)
	{
		return false;
	}
	MalformedDamageEffect->Status = EGameXXKCardStatus::Invalid;
	MalformedDamageEffect->StatusStacks = 1;
	FGameXXKCardEnemyIntent DirectResolvedIntent;
	TArray<FGameXXKCardDamageResult> DirectDamageResults;
	bool bDirectIntentsFinished = false;
	TestFalse(TEXT("a malformed saved status leaves the direct adapter intent unresolved"),
		FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(
			Subsystem->GetMutableRuntimeState(),
			DirectResolvedIntent,
			DirectDamageResults,
			bDirectIntentsFinished,
			&Error));
	TestEqual(TEXT("a malformed saved status leaves the direct adapter index unchanged before board recovery"),
		Subsystem->GetRuntimeState().CardRun.NextEnemyIntentIndex,
		0);
	Board->AdvanceEnemyIntentPresentationForTest(0.55f);
	Board->AdvanceEnemyIntentPresentationForTest(0.18f);
	TestEqual(TEXT("the board deliberately skips only the malformed saved intent after its adapter failure"),
		Subsystem->GetRuntimeState().CardRun.NextEnemyIntentIndex,
		1);
	TestEqual(TEXT("malformed intent recovery preserves the enemy phase for its remaining saved intents"),
		Subsystem->GetRuntimeState().CardRun.ActiveBattle.Phase,
		EGameXXKCardBattlePhase::Enemy);
	TestEqual(TEXT("malformed intent recovery remains in a recoverable settle presentation instead of clearing the board state"),
		Board->GetActiveEnemyIntentPresentationIndexForTest(),
		0);
	Board->AdvanceEnemyIntentPresentationForTest(0.32f);
	TestEqual(TEXT("malformed intent recovery advances visibly to the next saved intent"),
		Board->GetActiveEnemyIntentPresentationIndexForTest(),
		1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCardBattleBoardEnemyIntentProjectionSyncRecoveryTest,
	"GameXXK.Integration.CardBattle.BoardEnemyIntentProjectionSyncRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardBattleBoardEnemyIntentProjectionSyncRecoveryTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	FString Error;
	TestTrue(FString::Printf(TEXT("enemy-intent projection-sync recovery fixture enters a card battle: %s"), *Error),
		BuildThreeEnemyIntentPresentationFixture(Subsystem, Error));

	UGameXXKBattleBoardWidget* Board = NewObject<UGameXXKBattleBoardWidget>();
	Board->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("projection-sync recovery board initializes its widget tree"), Board->Initialize());
	Board->NativeConstruct();
	TestTrue(TEXT("projection-sync recovery fixture prepares saved enemy intents"), Board->EndCardPlayerPhase());
	FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
	const FName TargetUnitId = State.CardRun.EnemyIntents[0].SuggestedTargetUnitId;
	const FGameXXKCardCombatUnit* TargetBeforeResolution = State.CardRun.ActiveBattle.Units.FindByPredicate([TargetUnitId](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TargetUnitId;
	});
	TestNotNull(TEXT("the first saved intent keeps a live card-runtime target before projection failure"), TargetBeforeResolution);
	const int32 TargetHealthBeforeResolution = TargetBeforeResolution ? TargetBeforeResolution->HP : 0;
	const int32 TargetArmorBeforeResolution = TargetBeforeResolution ? TargetBeforeResolution->Armor : 0;
	const FName OriginalLegacyEnemyId = State.ActiveBattleEnemies.IsEmpty()
		? NAME_None
		: State.ActiveBattleEnemies[0].Id;
	if (!State.ActiveBattleEnemies.IsEmpty())
	{
		State.ActiveBattleEnemies[0].Id = FName(TEXT("BrokenLegacyProjection.Enemy"));
	}
	TestFalse(TEXT("the fixture deliberately breaks one legacy enemy projection after the saved intent was created"),
		FGameXXKCardBattleAdapter::SyncCardBattleToLegacyProjection(State, &Error));
	const FGameXXKCardRunState CardRunBeforeFailedPresentation = State.CardRun;

	Board->AdvanceEnemyIntentPresentationForTest(0.55f);
	Board->AdvanceEnemyIntentPresentationForTest(0.18f);
	const FGameXXKCardCombatUnit* TargetAfterResolution = State.CardRun.ActiveBattle.Units.FindByPredicate([TargetUnitId](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TargetUnitId;
	});
	TestTrue(TEXT("projection-sync failure preserves the complete authoritative card run"),
		FGameXXKCardRunState::StaticStruct()->CompareScriptStruct(
			&State.CardRun,
			&CardRunBeforeFailedPresentation,
			PPF_None));
	TestTrue(TEXT("projection-sync failure preserves the authoritative target health and armor"),
		TargetAfterResolution
		&& TargetAfterResolution->HP == TargetHealthBeforeResolution
		&& TargetAfterResolution->Armor == TargetArmorBeforeResolution);
	TestEqual(TEXT("projection-sync failure leaves the saved intent pending for retry"),
		State.CardRun.NextEnemyIntentIndex,
		0);
	TestEqual(TEXT("projection-sync failure keeps the same intent visibly retryable"),
		Board->GetActiveEnemyIntentPresentationIndexForTest(),
		0);

	if (!State.ActiveBattleEnemies.IsEmpty())
	{
		State.ActiveBattleEnemies[0].Id = OriginalLegacyEnemyId;
	}
	Board->AdvanceEnemyIntentPresentationForTest(0.55f);
	Board->AdvanceEnemyIntentPresentationForTest(0.18f);
	TestEqual(TEXT("repairing the projection lets the same saved intent resolve exactly once"),
		State.CardRun.NextEnemyIntentIndex,
		1);
	TestTrue(TEXT("the repaired intent keeps its settle transition locked behind damage presentation"),
		Board->IsBattlePresentationLockedForTest());
	Board->AdvanceVisualsAtRealTime(0.0);
	Board->AdvanceVisualsAtRealTime(100.0);
	TestFalse(TEXT("the repaired intent unlocks after its damage presentation drains"),
		Board->IsBattlePresentationLockedForTest());
	Board->AdvanceEnemyIntentPresentationForTest(0.32f);
	TestEqual(TEXT("the retried intent settles into the next pending presentation without stalling"),
		Board->GetActiveEnemyIntentPresentationIndexForTest(),
		1);
	return true;
}

#endif
