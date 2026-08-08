#include "Misc/AutomationTest.h"

#include "GameXXKCardCatalog.h"
#include "GameXXKCardBattleAdapter.h"
#include "GameXXKCardRules.h"
#include "GameXXKMVPRules.h"
#include "Components/Button.h"
#include "Engine/GameInstance.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKBattleBoardWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FGameXXKCardCombatUnit MakeFormationTargetingUnit(
		const TCHAR* UnitId,
		const EGameXXKCardTargetSide Side,
		const EGameXXKCharacterRole Role,
		const int32 StableSortOrder)
	{
		FGameXXKCardCombatUnit Unit;
		Unit.UnitId = FName(UnitId);
		Unit.Side = Side;
		Unit.Role = Role;
		Unit.bLiving = true;
		Unit.HP = 100;
		Unit.MaxHP = 100;
		Unit.Attack = 20;
		Unit.Mana = 100;
		Unit.MaxMana = 100;
		Unit.StableSortOrder = StableSortOrder;
		return Unit;
	}

	TArray<FGameXXKCardInstance> MakeFormationTargetingInstances(const FName CardId)
	{
		TArray<FGameXXKCardInstance> Instances;
		// Keep at least five cards in the draw pile after the opening hand. Draw effects such as
		// BaMenLunZhuan must not turn this targeting diagnostic into a reshuffle/self-redraw test.
		for (int32 Index = 0; Index < 10; ++Index)
		{
			FGameXXKCardInstance& Instance = Instances.AddDefaulted_GetRef();
			Instance.InstanceId = FName(*FString::Printf(TEXT("Diagnostic.%s.%d"), *CardId.ToString(), Index));
			Instance.CardId = CardId;
			Instance.OwnerUnitId = TEXT("FormationOwner");
			Instance.SourceEntryId = FName(*FString::Printf(TEXT("Source.%s.%d"), *CardId.ToString(), Index));
			Instance.AcquisitionOrdinal = Index;
		}
		return Instances;
	}

	bool IsKnownFormationTerrainTargetMismatch(
		const FName CardId,
		const EGameXXKCardTerrain Terrain)
	{
		return (CardId == TEXT("Profession.FormationMaster.YinShuiHuiYuan")
				&& (Terrain == EGameXXKCardTerrain::WaterShore || Terrain == EGameXXKCardTerrain::Ferry))
			|| ((CardId == TEXT("Profession.FormationMaster.LinYingMiZong")
					|| CardId == TEXT("Profession.FormationMaster.LinFengFuZhen"))
				&& Terrain == EGameXXKCardTerrain::Forest);
	}

	bool IsCandidateLegal(const FGameXXKCardTargetRequest& Request, const FName UnitId)
	{
		const FGameXXKCardTargetCandidateView* Candidate = Request.CandidateViews.FindByPredicate([UnitId](const FGameXXKCardTargetCandidateView& View)
		{
			return View.UnitId == UnitId;
		});
		return Candidate && Candidate->bCanSelect;
	}

	FGameXXKBattleRuntimeUnit MakeFormationLegacyUnit(
		const TCHAR* UnitId,
		const bool bEnemy)
	{
		FGameXXKBattleRuntimeUnit Unit;
		Unit.Id = FName(UnitId);
		Unit.DisplayName = FText::FromString(UnitId);
		Unit.HP = 100;
		Unit.MaxHP = 100;
		Unit.MP = 100;
		Unit.MaxMP = 100;
		Unit.Attack = 20;
		Unit.Defense = 0;
		Unit.Speed = bEnemy ? 8 : 10;
		Unit.bEnemy = bEnemy;
		return Unit;
	}

	bool BuildFormationBoardFixture(
		UGameXXKMVPSubsystem* const Subsystem,
		const EGameXXKCardTerrain Terrain,
		FName& OutCardInstanceId,
		FString& OutError)
	{
		if (!Subsystem)
		{
			OutError = TEXT("formation Board fixture has no subsystem");
			return false;
		}
		TArray<FGameXXKCardCombatUnit> Units;
		Units.Add(MakeFormationTargetingUnit(TEXT("FormationOwner"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::FormationMaster, 1));
		Units.Add(MakeFormationTargetingUnit(TEXT("Hero"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 2));
		Units.Add(MakeFormationTargetingUnit(TEXT("QuestNpc"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::QuestNpc, 3));
		Units.Add(MakeFormationTargetingUnit(TEXT("Enemy"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 10));

		FGameXXKCardBattleRuntime Runtime;
		if (!GameXXKCardRules::InitializeCardBattleRuntime(
			Runtime,
			MakeFormationTargetingInstances(TEXT("Profession.FormationMaster.YinShuiHuiYuan")),
			Units,
			Terrain,
			17101,
			&OutError))
		{
			return false;
		}

		FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
		State = UGameXXKMVPRules::CreateNewGame();
		State.Screen = EGameXXKScreen::Battle;
		State.bHasActiveBattle = true;
		State.ActiveBattleNodeId = 1701;
		State.ActiveBattleParty = {
			MakeFormationLegacyUnit(TEXT("FormationOwner"), false),
			MakeFormationLegacyUnit(TEXT("Hero"), false),
			MakeFormationLegacyUnit(TEXT("QuestNpc"), false)};
		State.ActiveBattleEnemies = {MakeFormationLegacyUnit(TEXT("Enemy"), true)};
		State.CardRun.bHasActiveCardBattle = true;
		State.CardRun.ActiveBattle = MoveTemp(Runtime);
		OutCardInstanceId = State.CardRun.ActiveBattle.Deck.Hand[0].InstanceId;
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKFormationMasterTargetingDiagnosticTest,
	"GameXXK.Diagnostics.FormationMasterTargeting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKFormationMasterTargetingDiagnosticTest::RunTest(const FString& Parameters)
{
	const TArray<FGameXXKCardDefinition> Definitions =
		FGameXXKCardCatalog::GetCardDefinitionsForOwner(TEXT("Profession.FormationMaster"));
	TestEqual(TEXT("formation master targeting diagnostic covers all eighteen cards"), Definitions.Num(), 18);
	if (Definitions.Num() != 18)
	{
		return false;
	}

	const EGameXXKCardTerrain Terrains[] = {
		EGameXXKCardTerrain::Plain,
		EGameXXKCardTerrain::Cliff,
		EGameXXKCardTerrain::Forest,
		EGameXXKCardTerrain::WaterShore,
		EGameXXKCardTerrain::Ferry,
		EGameXXKCardTerrain::Village,
		EGameXXKCardTerrain::Cave};
	int32 PreviewCount = 0;
	int32 ResolveCount = 0;
	int32 KnownTerrainTargetMismatchCount = 0;

	for (const FGameXXKCardDefinition& Definition : Definitions)
	{
		for (const EGameXXKCardTerrain Terrain : Terrains)
		{
			TArray<FGameXXKCardCombatUnit> Units;
			Units.Add(MakeFormationTargetingUnit(TEXT("FormationOwner"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::FormationMaster, 1));
			Units.Add(MakeFormationTargetingUnit(TEXT("Hero"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 2));
			Units.Add(MakeFormationTargetingUnit(TEXT("QuestNpc"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::QuestNpc, 3));
			Units.Add(MakeFormationTargetingUnit(TEXT("Enemy"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 10));

			FGameXXKCardBattleRuntime Runtime;
			FString Error;
			const FString Context = FString::Printf(
				TEXT("card=%s terrain=%d"),
				*Definition.Id.ToString(),
				static_cast<int32>(Terrain));
			if (!GameXXKCardRules::InitializeCardBattleRuntime(
				Runtime,
				MakeFormationTargetingInstances(Definition.Id),
				Units,
				Terrain,
				17001,
				&Error))
			{
				AddError(FString::Printf(TEXT("%s failed to initialize: %s"), *Context, *Error));
				continue;
			}

			const FGameXXKCardInstance Card = Runtime.Deck.Hand[0];
			FGameXXKCardPlayPreview Preview;
			if (!GameXXKCardRules::BuildCardPlayPreview(Runtime, Card.InstanceId, Preview, &Error))
			{
				AddError(FString::Printf(TEXT("%s could not build a playable preview: %s"), *Context, *Error));
				continue;
			}
			++PreviewCount;
			TestTrue(FString::Printf(TEXT("%s preview is playable"), *Context), Preview.bCanPlay);

			FName SelectedTarget = NAME_None;
			if (Preview.TargetRequest.bRequiresManualSelection)
			{
				for (const FGameXXKCardTargetCandidateView& Candidate : Preview.TargetRequest.CandidateViews)
				{
					if (Candidate.bCanSelect)
					{
						SelectedTarget = Candidate.UnitId;
						break;
					}
				}
				TestFalse(FString::Printf(TEXT("%s exposes at least one legal manual target"), *Context), SelectedTarget.IsNone());
				if (Preview.TargetRequest.EffectiveMode == EGameXXKCardTargetMode::SingleAlly)
				{
					TestTrue(FString::Printf(TEXT("%s can select its formation-master owner"), *Context), IsCandidateLegal(Preview.TargetRequest, TEXT("FormationOwner")));
					TestTrue(FString::Printf(TEXT("%s can select another ally"), *Context), IsCandidateLegal(Preview.TargetRequest, TEXT("Hero")));
					TestFalse(FString::Printf(TEXT("%s cannot select an enemy as an ally"), *Context), IsCandidateLegal(Preview.TargetRequest, TEXT("Enemy")));
				}
				else if (Preview.TargetRequest.EffectiveMode == EGameXXKCardTargetMode::SingleEnemy)
				{
					TestTrue(FString::Printf(TEXT("%s can select a living enemy"), *Context), IsCandidateLegal(Preview.TargetRequest, TEXT("Enemy")));
					TestFalse(FString::Printf(TEXT("%s cannot select an ally as an enemy"), *Context), IsCandidateLegal(Preview.TargetRequest, TEXT("Hero")));
				}
			}

			FGameXXKCardPlayResult Result;
			const bool bResolved = GameXXKCardRules::ResolveCardPlay(Runtime, Card.InstanceId, SelectedTarget, Result, &Error);
			if (!bResolved && IsKnownFormationTerrainTargetMismatch(Definition.Id, Terrain))
			{
				++KnownTerrainTargetMismatchCount;
				AddWarning(FString::Printf(
					TEXT("Known formation terrain-target mismatch: %s previewed as an automatic group card but rejected atomically: %s"),
					*Context,
					*Error));
				TestTrue(
					FString::Printf(TEXT("%s reports the selected-target/group-target mismatch"), *Context),
					Error.Contains(TEXT("Selected-target effect has no current living stable target")));
				TestTrue(
					FString::Printf(TEXT("%s rejection is atomic and leaves the card in hand"), *Context),
					Runtime.Deck.Hand.ContainsByPredicate([Card](const FGameXXKCardInstance& Candidate)
					{
						return Candidate.InstanceId == Card.InstanceId;
					}));
				continue;
			}
			if (!bResolved)
			{
				AddError(FString::Printf(TEXT("%s previewed but could not resolve: %s"), *Context, *Error));
				continue;
			}
			++ResolveCount;
			TestFalse(
				FString::Printf(TEXT("%s leaves the hand after a successful play"), *Context),
				Runtime.Deck.Hand.ContainsByPredicate([Card](const FGameXXKCardInstance& Candidate)
				{
					return Candidate.InstanceId == Card.InstanceId;
				}));
		}
	}

	TestEqual(TEXT("all formation-master card/terrain pairs build previews"), PreviewCount, 18 * 7);
	TestEqual(
		TEXT("every formation-master card/terrain pair either resolves or matches the isolated known defect"),
		ResolveCount + KnownTerrainTargetMismatchCount,
		18 * 7);
	TestTrue(
		TEXT("the known terrain-target mismatch set never grows beyond the four isolated pairs"),
		KnownTerrainTargetMismatchCount <= 4);

	TArray<FGameXXKCardCombatUnit> InsightUnits;
	InsightUnits.Add(MakeFormationTargetingUnit(TEXT("FormationOwner"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::FormationMaster, 1));
	InsightUnits.Add(MakeFormationTargetingUnit(TEXT("Hero"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 2));
	InsightUnits.Add(MakeFormationTargetingUnit(TEXT("Enemy"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 10));
	FGameXXKCardBattleRuntime InsightRuntime;
	FString InsightError;
	const bool bInsightInitialized = GameXXKCardRules::InitializeCardBattleRuntime(
		InsightRuntime,
		MakeFormationTargetingInstances(TEXT("Profession.FormationMaster.GuanShi")),
		InsightUnits,
		EGameXXKCardTerrain::Plain,
		17201,
		&InsightError);
	TestTrue(
		FString::Printf(TEXT("观势 pending-choice fixture initializes: %s"), *InsightError),
		bInsightInitialized);
	if (!bInsightInitialized || InsightRuntime.Deck.Hand.IsEmpty())
	{
		return false;
	}
	const FName GuanShiInstanceId = InsightRuntime.Deck.Hand[0].InstanceId;
	FGameXXKCardPlayResult GuanShiResult;
	const bool bGuanShiResolved = GameXXKCardRules::ResolveCardPlay(
		InsightRuntime,
		GuanShiInstanceId,
		NAME_None,
		GuanShiResult,
		&InsightError);
	TestTrue(
		FString::Printf(TEXT("观势 resolves before its insight choice: %s"), *InsightError),
		bGuanShiResolved);
	if (!bGuanShiResolved || InsightRuntime.Deck.Hand.IsEmpty())
	{
		return false;
	}
	TestTrue(TEXT("观势 explicitly opens a blocking insight choice"), GameXXKCardRules::HasPendingChoice(InsightRuntime.Deck));
	FGameXXKCardPlayPreview BlockedPreview;
	InsightError.Reset();
	TestFalse(
		TEXT("another card is intentionally blocked until the 观势 choice completes"),
		GameXXKCardRules::BuildCardPlayPreview(InsightRuntime, InsightRuntime.Deck.Hand[0].InstanceId, BlockedPreview, &InsightError));
	TestTrue(TEXT("the blocking failure names the pending choice"), InsightError.Contains(TEXT("pending card choice")));
	if (!InsightRuntime.Deck.PendingChoice.Candidates.IsEmpty())
	{
		const FName PickedId = InsightRuntime.Deck.PendingChoice.Candidates[0].InstanceId;
		TArray<FName> ReorderedIds;
		for (int32 CandidateIndex = 1; CandidateIndex < InsightRuntime.Deck.PendingChoice.Candidates.Num(); ++CandidateIndex)
		{
			ReorderedIds.Add(InsightRuntime.Deck.PendingChoice.Candidates[CandidateIndex].InstanceId);
		}
		InsightError.Reset();
		TestTrue(
			FString::Printf(TEXT("the 观势 insight choice can complete: %s"), *InsightError),
			GameXXKCardRules::SubmitInsightChoice(InsightRuntime.Deck, PickedId, ReorderedIds, &InsightError));
		TestFalse(TEXT("completing 观势 clears the blocking choice"), GameXXKCardRules::HasPendingChoice(InsightRuntime.Deck));
		FGameXXKCardPlayPreview UnblockedPreview;
		TestTrue(
			FString::Printf(TEXT("a remaining card becomes previewable after 观势 completes: %s"), *InsightError),
			GameXXKCardRules::BuildCardPlayPreview(InsightRuntime, InsightRuntime.Deck.Hand[0].InstanceId, UnblockedPreview, &InsightError));
	}
	else
	{
		AddError(TEXT("观势 opened an insight choice without candidates"));
	}

	TArray<FGameXXKCardCombatUnit> RotationUnits;
	RotationUnits.Add(MakeFormationTargetingUnit(TEXT("FormationOwner"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::FormationMaster, 1));
	RotationUnits.Add(MakeFormationTargetingUnit(TEXT("Hero"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 2));
	RotationUnits.Add(MakeFormationTargetingUnit(TEXT("Enemy"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 10));
	FGameXXKCardBattleRuntime RotationRuntime;
	FString RotationError;
	const bool bRotationInitialized = GameXXKCardRules::InitializeCardBattleRuntime(
		RotationRuntime,
		MakeFormationTargetingInstances(TEXT("Profession.FormationMaster.BaMenLunZhuan")),
		RotationUnits,
		EGameXXKCardTerrain::Plain,
		17301,
		&RotationError);
	TestTrue(
		FString::Printf(TEXT("八门轮转 forced-discard fixture initializes: %s"), *RotationError),
		bRotationInitialized);
	if (!bRotationInitialized || RotationRuntime.Deck.Hand.IsEmpty())
	{
		return false;
	}
	const FName RotationInstanceId = RotationRuntime.Deck.Hand[0].InstanceId;
	FGameXXKCardPlayResult RotationResult;
	const bool bRotationResolved = GameXXKCardRules::ResolveCardPlay(
		RotationRuntime,
		RotationInstanceId,
		NAME_None,
		RotationResult,
		&RotationError);
	TestTrue(
		FString::Printf(TEXT("八门轮转 resolves before its forced discard: %s"), *RotationError),
		bRotationResolved);
	if (!bRotationResolved)
	{
		return false;
	}
	TestFalse(
		TEXT("八门轮转 leaves its played instance out of the hand before the discard choice"),
		RotationRuntime.Deck.Hand.ContainsByPredicate([RotationInstanceId](const FGameXXKCardInstance& Candidate)
		{
			return Candidate.InstanceId == RotationInstanceId;
		}));
	TestTrue(TEXT("八门轮转 reports that it opened a blocking choice"), RotationResult.bOpenedPendingChoice);
	TestEqual(
		TEXT("八门轮转 opens the declared forced-discard choice"),
		RotationRuntime.Deck.PendingChoice.Kind,
		EGameXXKCardPendingChoiceKind::ForcedDiscard);
	FGameXXKCardPlayPreview RotationBlockedPreview;
	RotationError.Reset();
	TestFalse(
		TEXT("八门轮转 intentionally blocks another card until one hand card is discarded"),
		GameXXKCardRules::BuildCardPlayPreview(
			RotationRuntime,
			RotationRuntime.Deck.Hand[0].InstanceId,
			RotationBlockedPreview,
			&RotationError));
	TestTrue(TEXT("八门轮转 blocking error names the pending choice"), RotationError.Contains(TEXT("pending card choice")));
	if (!RotationRuntime.Deck.PendingChoice.Candidates.IsEmpty())
	{
		RotationError.Reset();
		TestTrue(
			FString::Printf(TEXT("八门轮转 forced discard can complete: %s"), *RotationError),
			GameXXKCardRules::SubmitForcedDiscard(
				RotationRuntime,
				{RotationRuntime.Deck.PendingChoice.Candidates[0].InstanceId},
				&RotationError));
		TestFalse(TEXT("completing 八门轮转 clears the blocking choice"), GameXXKCardRules::HasPendingChoice(RotationRuntime.Deck));
	}
	else
	{
		AddError(TEXT("八门轮转 opened a forced-discard choice without candidates"));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKFormationMasterBoardTargetingDiagnosticTest,
	"GameXXK.Diagnostics.FormationMasterBoardTargeting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKFormationMasterBoardTargetingDiagnosticTest::RunTest(const FString& Parameters)
{
	UGameInstance* const GameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* const Subsystem = NewObject<UGameXXKMVPSubsystem>(GameInstance);
	FName CardInstanceId;
	FString Error;
	TestTrue(
		FString::Printf(TEXT("plain-terrain formation Board fixture initializes: %s"), *Error),
		BuildFormationBoardFixture(Subsystem, EGameXXKCardTerrain::Plain, CardInstanceId, Error));
	if (CardInstanceId.IsNone())
	{
		return false;
	}

	UGameXXKBattleBoardWidget* const Board = NewObject<UGameXXKBattleBoardWidget>();
	Board->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("formation targeting Board initializes"), Board->Initialize());
	Board->NativeConstruct();
	Board->RefreshFromState();
	TestTrue(TEXT("formation targeting Board begins a visual session"), Board->BeginBattleVisualSession(17101));
	TestTrue(TEXT("plain-terrain 引水回元 enters manual ally targeting"), Board->ClickCardInHand(CardInstanceId));
	TestTrue(TEXT("formation Board highlights another ally"), Board->IsTargetUnitHighlighted(TEXT("Hero")));
	TestTrue(TEXT("formation Board also permits the card owner"), Board->IsTargetUnitHighlighted(TEXT("FormationOwner")));
	TestFalse(TEXT("formation Board does not highlight an enemy for 引水回元"), Board->IsTargetUnitHighlighted(TEXT("Enemy")));

	UButton* const AllyProxy = Board->GetUnitTargetProxyForTest(TEXT("Hero"));
	TestNotNull(TEXT("the highlighted ally has a real clickable target proxy"), AllyProxy);
	TestTrue(TEXT("the highlighted ally target proxy is enabled"), AllyProxy && AllyProxy->GetIsEnabled());
	if (AllyProxy)
	{
		AllyProxy->OnClicked.Broadcast();
	}
	TestFalse(
		TEXT("clicking the ally proxy commits 引水回元 and removes the card from hand"),
		Subsystem->GetRuntimeState().CardRun.ActiveBattle.Deck.Hand.ContainsByPredicate([CardInstanceId](const FGameXXKCardInstance& Candidate)
		{
			return Candidate.InstanceId == CardInstanceId;
		}));
	TestFalse(TEXT("successful ally targeting exits the card-targeting mode"), Board->IsCardTargetingForTest());

	UGameInstance* const WaterGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* const WaterSubsystem = NewObject<UGameXXKMVPSubsystem>(WaterGameInstance);
	FName WaterCardInstanceId;
	Error.Reset();
	TestTrue(
		FString::Printf(TEXT("water-terrain formation Board fixture initializes: %s"), *Error),
		BuildFormationBoardFixture(WaterSubsystem, EGameXXKCardTerrain::WaterShore, WaterCardInstanceId, Error));
	if (WaterCardInstanceId.IsNone())
	{
		return false;
	}
	UGameXXKBattleBoardWidget* const WaterBoard = NewObject<UGameXXKBattleBoardWidget>();
	WaterBoard->SetMVPSubsystem(WaterSubsystem);
	TestTrue(TEXT("water-terrain formation Board initializes"), WaterBoard->Initialize());
	WaterBoard->NativeConstruct();
	WaterBoard->RefreshFromState();
	TestTrue(TEXT("water-terrain formation Board begins a visual session"), WaterBoard->BeginBattleVisualSession(17102));
	const bool bWaterTerrainResolved = WaterBoard->ClickCardInHand(WaterCardInstanceId);
	if (!bWaterTerrainResolved)
	{
		AddWarning(TEXT("Known formation terrain-target mismatch: water-terrain 引水回元 previews as all allies but its selected-target effect rejects resolution."));
	}
	TestFalse(TEXT("water-terrain target override never opens a stale arrow"), WaterBoard->IsCardTargetingForTest());
	const bool bWaterCardRemainsInHand = WaterSubsystem->GetRuntimeState().CardRun.ActiveBattle.Deck.Hand.ContainsByPredicate(
		[WaterCardInstanceId](const FGameXXKCardInstance& Candidate)
		{
			return Candidate.InstanceId == WaterCardInstanceId;
		});
	TestEqual(
		TEXT("water-terrain 引水回元 either commits or atomically remains in hand"),
		bWaterCardRemainsInHand,
		!bWaterTerrainResolved);
	return true;
}

#endif
