#include "Misc/AutomationTest.h"

#include "GameXXKCardCatalog.h"
#include "GameXXKCardBattleAdapter.h"
#include "GameXXKCardRules.h"
#include "GameXXKMVPRules.h"
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
	TestEqual(TEXT("all formation-master card/terrain pairs resolve"), ResolveCount, 18 * 7);

	TArray<FGameXXKCardCombatUnit> GuanShiUnits;
	GuanShiUnits.Add(MakeFormationTargetingUnit(TEXT("FormationOwner"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::FormationMaster, 1));
	GuanShiUnits.Add(MakeFormationTargetingUnit(TEXT("Hero"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 2));
	GuanShiUnits.Add(MakeFormationTargetingUnit(TEXT("Enemy"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 10));
	FGameXXKCardBattleRuntime GuanShiRuntime;
	FString GuanShiError;
	const bool bGuanShiInitialized = GameXXKCardRules::InitializeCardBattleRuntime(
		GuanShiRuntime,
		MakeFormationTargetingInstances(TEXT("Profession.FormationMaster.GuanShi")),
		GuanShiUnits,
		EGameXXKCardTerrain::Plain,
		17201,
		&GuanShiError);
	TestTrue(
		FString::Printf(TEXT("平野观势 same-terrain fixture initializes: %s"), *GuanShiError),
		bGuanShiInitialized);
	if (!bGuanShiInitialized || GuanShiRuntime.Deck.Hand.IsEmpty())
	{
		return false;
	}
	const FName GuanShiInstanceId = GuanShiRuntime.Deck.Hand[0].InstanceId;
	FGameXXKCardPlayPreview GuanShiPreview;
	TestTrue(
		FString::Printf(TEXT("平野观势 builds its fixed enemy preview: %s"), *GuanShiError),
		GameXXKCardRules::BuildCardPlayPreview(GuanShiRuntime, GuanShiInstanceId, GuanShiPreview, &GuanShiError));
	TestEqual(TEXT("平野观势 always uses a single-enemy target"), GuanShiPreview.TargetRequest.EffectiveMode, EGameXXKCardTargetMode::SingleEnemy);
	TestTrue(TEXT("平野观势 can select the living enemy"), IsCandidateLegal(GuanShiPreview.TargetRequest, TEXT("Enemy")));
	FGameXXKCardPlayResult GuanShiResult;
	const bool bGuanShiResolved = GameXXKCardRules::ResolveCardPlay(
		GuanShiRuntime,
		GuanShiInstanceId,
		TEXT("Enemy"),
		GuanShiResult,
		&GuanShiError);
	TestTrue(
		FString::Printf(TEXT("平野观势 resolves on its already-active terrain: %s"), *GuanShiError),
		bGuanShiResolved);
	if (!bGuanShiResolved)
	{
		return false;
	}
	TestEqual(TEXT("平野观势 keeps the terrain on Plain"), GuanShiRuntime.Terrain, EGameXXKCardTerrain::Plain);
	TestFalse(TEXT("same-terrain 平野观势 does not report a terrain change"), GuanShiRuntime.bTerrainChangedThisRound);
	const FGameXXKCardCombatUnit* GuanShiEnemy = GuanShiRuntime.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Enemy");
	});
	TestNotNull(TEXT("平野观势 retains its selected enemy"), GuanShiEnemy);
	if (GuanShiEnemy)
	{
		TestEqual(TEXT("same-terrain 平野观势 still grants the Plain Burn2 benefit"), GameXXKCardRules::GetCombatStatusStacks(*GuanShiEnemy, EGameXXKCardStatus::Burn), 2);
	}
	TestFalse(TEXT("平野观势 no longer opens an obsolete insight choice"), GameXXKCardRules::HasPendingChoice(GuanShiRuntime.Deck));

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
	FGameXXKQuestNpcTerrainTargetingDiagnosticTest,
	"GameXXK.Diagnostics.DynamicTerrainTargeting.QuestNpcTengQiao",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKQuestNpcTerrainTargetingDiagnosticTest::RunTest(const FString& Parameters)
{
	for (const EGameXXKCardTerrain Terrain : {
		EGameXXKCardTerrain::Plain,
		EGameXXKCardTerrain::Cliff,
		EGameXXKCardTerrain::Forest,
		EGameXXKCardTerrain::WaterShore,
		EGameXXKCardTerrain::Ferry,
		EGameXXKCardTerrain::Village,
		EGameXXKCardTerrain::Cave})
	{
		TArray<FGameXXKCardCombatUnit> Units;
		Units.Add(MakeFormationTargetingUnit(TEXT("FormationOwner"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::QuestNpc, 1));
		Units.Add(MakeFormationTargetingUnit(TEXT("Hero"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 2));
		Units.Add(MakeFormationTargetingUnit(TEXT("Enemy"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 10));
		FGameXXKCardBattleRuntime Runtime;
		FString Error;
		const FString Context = FString::Printf(TEXT("TengQiao terrain=%d"), static_cast<int32>(Terrain));
		const bool bInitialized = GameXXKCardRules::InitializeCardBattleRuntime(
			Runtime,
			MakeFormationTargetingInstances(TEXT("Npc.QiongMeiEr.TengQiaoFeiDu")),
			Units,
			Terrain,
			17400 + static_cast<int32>(Terrain),
			&Error);
		TestTrue(FString::Printf(TEXT("%s initializes: %s"), *Context, *Error), bInitialized);
		if (!bInitialized || Runtime.Deck.Hand.IsEmpty())
		{
			continue;
		}
		for (FGameXXKCardCombatUnit& Unit : Runtime.Units)
		{
			if (Unit.Side == EGameXXKCardTargetSide::Party)
			{
				Unit.Mana = 20;
			}
		}
		FGameXXKCardPlayPreview Preview;
		TestTrue(
			FString::Printf(TEXT("%s previews: %s"), *Context, *Error),
			GameXXKCardRules::BuildCardPlayPreview(Runtime, Runtime.Deck.Hand[0].InstanceId, Preview, &Error));
		TestTrue(FString::Printf(TEXT("%s requires one enemy anchor"), *Context), Preview.TargetRequest.bRequiresManualSelection);
		TestEqual(FString::Printf(TEXT("%s remains fixed SingleEnemy"), *Context), Preview.TargetRequest.EffectiveMode, EGameXXKCardTargetMode::SingleEnemy);
		TestTrue(FString::Printf(TEXT("%s permits the living enemy"), *Context), IsCandidateLegal(Preview.TargetRequest, TEXT("Enemy")));
		TestFalse(FString::Printf(TEXT("%s never permits the owner"), *Context), IsCandidateLegal(Preview.TargetRequest, TEXT("FormationOwner")));
		TestFalse(FString::Printf(TEXT("%s never permits another ally"), *Context), IsCandidateLegal(Preview.TargetRequest, TEXT("Hero")));
		FGameXXKCardPlayResult Result;
		Error.Reset();
		const bool bResolved = GameXXKCardRules::ResolveCardPlay(Runtime, Runtime.Deck.Hand[0].InstanceId, TEXT("Enemy"), Result, &Error);
		TestTrue(FString::Printf(TEXT("%s resolves on its selected enemy: %s"), *Context, *Error), bResolved);
		if (!bResolved)
		{
			continue;
		}
		TestEqual(FString::Printf(TEXT("%s commits one target"), *Context), Result.TargetUnitIds.Num(), 1);
		if (Result.TargetUnitIds.Num() == 1)
		{
			TestEqual(FString::Printf(TEXT("%s preserves the selected enemy anchor"), *Context), Result.TargetUnitIds[0], FName(TEXT("Enemy")));
		}
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
	TestTrue(TEXT("plain-terrain 引水回元 resolves with its fixed automatic all-allies target"), Board->ClickCardInHand(CardInstanceId));
	TestFalse(
		TEXT("automatic 引水回元 removes the card from hand"),
		Subsystem->GetRuntimeState().CardRun.ActiveBattle.Deck.Hand.ContainsByPredicate([CardInstanceId](const FGameXXKCardInstance& Candidate)
		{
			return Candidate.InstanceId == CardInstanceId;
		}));
	TestFalse(TEXT("automatic 引水回元 never opens card-targeting mode"), Board->IsCardTargetingForTest());
	TestEqual(TEXT("plain-terrain 引水回元 switches the battle to Water Shore"), Subsystem->GetRuntimeState().CardRun.ActiveBattle.Terrain, EGameXXKCardTerrain::WaterShore);
	TestTrue(TEXT("plain-terrain 引水回元 records an actual terrain change"), Subsystem->GetRuntimeState().CardRun.ActiveBattle.bTerrainChangedThisRound);

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
	TestTrue(TEXT("water-terrain 引水回元 commits through its fixed automatic all-allies target"), bWaterTerrainResolved);
	TestFalse(TEXT("same-terrain 引水回元 never opens a stale arrow"), WaterBoard->IsCardTargetingForTest());
	TestFalse(TEXT("same-terrain 引水回元 does not report an actual terrain change"), WaterSubsystem->GetRuntimeState().CardRun.ActiveBattle.bTerrainChangedThisRound);
	const bool bWaterCardRemainsInHand = WaterSubsystem->GetRuntimeState().CardRun.ActiveBattle.Deck.Hand.ContainsByPredicate(
		[WaterCardInstanceId](const FGameXXKCardInstance& Candidate)
		{
			return Candidate.InstanceId == WaterCardInstanceId;
		});
	TestFalse(TEXT("water-terrain 引水回元 leaves the hand after its automatic group effect"), bWaterCardRemainsInHand);
	return true;
}

#endif
