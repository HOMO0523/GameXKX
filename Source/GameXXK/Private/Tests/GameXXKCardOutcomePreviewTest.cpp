#include "GameXXKCardOutcomePreview.h"

#include "GameXXKCardBattleAdapter.h"
#include "GameXXKCardCatalog.h"
#include "GameXXKCardRules.h"
#include "GameXXKEquipmentSetCatalog.h"
#include "GameXXKMVPRules.h"
#include "GameXXKRelicRules.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKCardOutcomePreviewTestBridge
{
	FGameXXKCardOutcomeTarget AggregateDamagePackets(
		const TArray<FGameXXKCardDamageResult>& Packets);
}

namespace GameXXKCardOutcomePreviewTest
{
	const FName OwnerUnitId(TEXT("Hero"));
	const FName AllyUnitId(TEXT("OutcomePreview.Ally"));
	const FName EnemyOneUnitId(TEXT("OutcomePreview.Enemy.1P"));
	const FName EnemyTwoUnitId(TEXT("OutcomePreview.Enemy.2P"));
	const FName EnemyThreeUnitId(TEXT("OutcomePreview.Enemy.3P"));
	const FName PrimaryCardInstanceId(TEXT("OutcomePreview.Card.0"));

	FGameXXKCardCombatUnit MakeUnit(
		const FName UnitId,
		const EGameXXKCardTargetSide Side,
		const EGameXXKCharacterRole Role,
		const int32 StableSortOrder,
		const int32 BattleSlotNumber = INDEX_NONE)
	{
		FGameXXKCardCombatUnit Unit;
		Unit.UnitId = UnitId;
		Unit.Side = Side;
		Unit.Role = Role;
		Unit.bLiving = true;
		Unit.HP = 500;
		Unit.MaxHP = 500;
		Unit.Mana = Side == EGameXXKCardTargetSide::Party ? 100 : 0;
		Unit.MaxMana = Unit.Mana;
		Unit.Attack = Side == EGameXXKCardTargetSide::Party ? 30 : 10;
		Unit.Defense = 0;
		Unit.Speed = 1;
		Unit.StableSortOrder = StableSortOrder;
		Unit.BattleSlotNumber = BattleSlotNumber;
		if (Side == EGameXXKCardTargetSide::Enemy && BattleSlotNumber != INDEX_NONE)
		{
			static const FName EnemyDefinitions[] = {
				TEXT("Enemy.Ch1.Rooster"),
				TEXT("Enemy.Ch1.Goat"),
				TEXT("Enemy.Ch1.Weasel")};
			Unit.EnemyDefinitionId = EnemyDefinitions[FMath::Clamp(BattleSlotNumber, 1, 3) - 1];
			Unit.CombatLevel = 1;
		}
		return Unit;
	}

	FGameXXKCardInstance MakeCard(
		const FName InstanceId,
		const FName CardId,
		const int32 AcquisitionOrdinal)
	{
		FGameXXKCardInstance Card;
		Card.InstanceId = InstanceId;
		Card.CardId = CardId;
		Card.CurrentQuality = EGameXXKCardQuality::Common;
		Card.OwnerUnitId = OwnerUnitId;
		Card.SourceEntryId = FName(*FString::Printf(TEXT("OutcomePreview.Source.%d"), AcquisitionOrdinal));
		Card.AcquisitionOrdinal = AcquisitionOrdinal;
		return Card;
	}

	FGameXXKCardCombatUnit* FindUnit(FGameXXKRuntimeState& State, const FName UnitId)
	{
		return State.CardRun.ActiveBattle.Units.FindByPredicate([UnitId](const FGameXXKCardCombatUnit& Unit)
		{
			return Unit.UnitId == UnitId;
		});
	}

	const FGameXXKCardCombatUnit* FindUnit(const FGameXXKRuntimeState& State, const FName UnitId)
	{
		return State.CardRun.ActiveBattle.Units.FindByPredicate([UnitId](const FGameXXKCardCombatUnit& Unit)
		{
			return Unit.UnitId == UnitId;
		});
	}

	bool BuildState(
		FAutomationTestBase& Test,
		FGameXXKRuntimeState& OutState,
		const FName CardId,
		const EGameXXKCharacterRole OwnerRole,
		const TArray<FGameXXKCardCombatUnit>& Units,
		const int32 Seed,
		const int32 CardCount = 1)
	{
		(void)OwnerRole;
		TArray<FGameXXKCardInstance> Cards;
		for (int32 Index = 0; Index < CardCount; ++Index)
		{
			Cards.Add(MakeCard(
				Index == 0 ? PrimaryCardInstanceId : FName(*FString::Printf(TEXT("OutcomePreview.Card.%d"), Index)),
				CardId,
				Index));
		}
		FGameXXKCardBattleRuntime Runtime;
		FString Error;
		if (!GameXXKCardRules::InitializeCardBattleRuntime(
			Runtime,
			Cards,
			Units,
			EGameXXKCardTerrain::Plain,
			Seed,
			&Error))
		{
			Test.AddError(FString::Printf(TEXT("real outcome-preview runtime failed to initialize: %s"), *Error));
			return false;
		}
		Runtime.Deck.Hand = {Cards[0]};
		Runtime.Deck.DrawPile.Reset();
		for (int32 Index = 1; Index < Cards.Num(); ++Index)
		{
			Runtime.Deck.DrawPile.Add(Cards[Index]);
		}
		Runtime.Deck.DiscardPile.Reset();
		Runtime.Deck.ExhaustPile.Reset();
		Runtime.Deck.SharedEnergy = 20;
		if (!GameXXKCardRules::ValidateCardBattleRuntime(Runtime, &Error))
		{
			Test.AddError(FString::Printf(TEXT("real outcome-preview fixture is invalid: %s"), *Error));
			return false;
		}

		OutState = UGameXXKMVPRules::CreateNewGame();
		FGameXXKRelicRules::ClearRouteRelics(OutState);
		OutState.CardRun.bHasActiveCardBattle = true;
		OutState.CardRun.ActiveBattleSourceNodeId = 1;
		OutState.CardRun.ActiveBattle = MoveTemp(Runtime);
		return true;
	}

	bool BuildStandardState(
		FAutomationTestBase& Test,
		FGameXXKRuntimeState& OutState,
		const FName CardId,
		const EGameXXKCharacterRole OwnerRole,
		const int32 Seed,
		const bool bIncludeThreeEnemies = false,
		const int32 CardCount = 1)
	{
		TArray<FGameXXKCardCombatUnit> Units = {
			MakeUnit(OwnerUnitId, EGameXXKCardTargetSide::Party, OwnerRole, 0),
			MakeUnit(AllyUnitId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Guard, 1),
			MakeUnit(EnemyOneUnitId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 10, 1)};
		if (bIncludeThreeEnemies)
		{
			Units.Add(MakeUnit(EnemyTwoUnitId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 11, 2));
			Units.Add(MakeUnit(EnemyThreeUnitId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 12, 3));
		}
		return BuildState(Test, OutState, CardId, OwnerRole, Units, Seed, CardCount);
	}

	FString RenderLine(const FGameXXKCardOutcomeTextLine& Line)
	{
		TArray<FString> Phrases;
		for (const FGameXXKCardOutcomeTextSegment& Segment : Line.Segments)
		{
			Phrases.Add(Segment.Text.ToString());
		}
		return FString::Join(Phrases, TEXT(" · "));
	}

	template <typename PacketType>
	bool ComparePacketArrays(
		FAutomationTestBase& Test,
		const TArray<PacketType>& Expected,
		const TArray<PacketType>& Actual,
		const TCHAR* Label)
	{
		if (!Test.TestEqual(FString::Printf(TEXT("%s packet count"), Label), Actual.Num(), Expected.Num()))
		{
			return false;
		}
		for (int32 Index = 0; Index < Expected.Num(); ++Index)
		{
			if (!Test.TestTrue(
				FString::Printf(TEXT("%s packet %d matches the independently committed copy"), Label, Index),
				PacketType::StaticStruct()->CompareScriptStruct(&Actual[Index], &Expected[Index], PPF_None)))
			{
				return false;
			}
		}
		return true;
	}

	int32 Status(
		const FGameXXKRuntimeState& State,
		const FName UnitId,
		const EGameXXKCardStatus StatusType)
	{
		const FGameXXKCardCombatUnit* Unit = FindUnit(State, UnitId);
		return Unit ? GameXXKCardRules::GetCombatStatusStacks(*Unit, StatusType) : INDEX_NONE;
	}

	bool InstallHealerFormula(
		FAutomationTestBase& Test,
		FGameXXKRuntimeState& State,
		const FName FormulaCardId)
	{
		const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(FormulaCardId);
		if (!Definition || Definition->HealerRule.FormulaKind == EGameXXKHealerFormulaKind::None)
		{
			Test.AddError(FString::Printf(TEXT("real formula card is missing: %s"), *FormulaCardId.ToString()));
			return false;
		}
		FGameXXKHealerFormulaRuntime& Formula = State.CardRun.ActiveBattle.HealerFormulas.AddDefaulted_GetRef();
		Formula.OwnerUnitId = OwnerUnitId;
		Formula.SourceCardId = Definition->Id;
		Formula.Kind = Definition->HealerRule.FormulaKind;
		FString Error;
		if (!GameXXKCardRules::ValidateCardBattleRuntime(State.CardRun.ActiveBattle, &Error))
		{
			Test.AddError(FString::Printf(TEXT("real formula fixture is invalid: %s"), *Error));
			return false;
		}
		return true;
	}

	bool InstallSetTier(
		FAutomationTestBase& Test,
		FGameXXKRuntimeState& State,
		const FName DefinitionId,
		const EGameXXKEquipmentModifierKind ModifierKind)
	{
		const FGameXXKEquipmentSetBonusDefinition* Definition =
			FGameXXKEquipmentSetCatalog::FindDefinition(DefinitionId);
		if (!Definition)
		{
			Test.AddError(FString::Printf(TEXT("real set definition is missing: %s"), *DefinitionId.ToString()));
			return false;
		}
		FGameXXKEquipmentBattleEffectRuntime& RuntimeEffect =
			State.CardRun.ActiveBattle.EquipmentEffects.AddDefaulted_GetRef();
		RuntimeEffect.SourceCharacterId = OwnerUnitId;
		RuntimeEffect.ActiveEffect.EffectId = Definition->Id;
		RuntimeEffect.ActiveEffect.SourceCharacterId = OwnerUnitId;
		RuntimeEffect.ActiveEffect.Set = Definition->Set;
		RuntimeEffect.ActiveEffect.RequiredPieces = Definition->RequiredPieces;
		RuntimeEffect.ActiveEffect.Scope = Definition->Scope;
		RuntimeEffect.ActiveEffect.Hook = Definition->Hook;
		RuntimeEffect.ActiveEffect.ModifierKind = ModifierKind;
		RuntimeEffect.ActiveEffect.Magnitude = Definition->Value;
		RuntimeEffect.ActiveEffect.Unit = Definition->Unit;
		RuntimeEffect.ActiveEffect.MaxTriggersPerRound = Definition->TriggersPerRound;
		FString Error;
		if (!GameXXKCardRules::ValidateCardBattleRuntime(State.CardRun.ActiveBattle, &Error))
		{
			Test.AddError(FString::Printf(TEXT("real set-tier fixture is invalid: %s"), *Error));
			return false;
		}
		return true;
	}

	int32 SumDamageCause(
		const TArray<FGameXXKCardDamageResult>& Results,
		const FName TargetUnitId,
		const EGameXXKCardDamageCause Cause)
	{
		int32 Total = 0;
		for (const FGameXXKCardDamageResult& Result : Results)
		{
			if (Result.ResolvedTargetUnitId == TargetUnitId && Result.Cause == Cause)
			{
				Total += Result.HealthDamage;
			}
		}
		return Total;
	}

	bool HasSegment(
		const FGameXXKCardOutcomeTextLine& Line,
		const FString& Text,
		const EGameXXKCardOutcomeTone Tone)
	{
		return Line.Segments.ContainsByPredicate([&Text, Tone](const FGameXXKCardOutcomeTextSegment& Segment)
		{
			return Segment.Text.ToString() == Text && Segment.Tone == Tone;
		});
	}

}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCardOutcomePreviewInputIsImmutableTest,
	"GameXXK.Data.CardOutcomePreview.Rules.InputIsImmutable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardOutcomePreviewInputIsImmutableTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKCardOutcomePreviewTest;
	FGameXXKRuntimeState State;
	if (!BuildStandardState(*this, State, TEXT("Hero.Generic.SuiYanJi"), EGameXXKCharacterRole::Hero, 61101))
	{
		return false;
	}
	FGameXXKCardCombatUnit* Target = FindUnit(State, EnemyOneUnitId);
	if (!TestNotNull(TEXT("immutable fixture exposes its real target"), Target))
	{
		return false;
	}
	TestEqual(TEXT("immutable fixture gives its target one real Agility"),
		GameXXKCardRules::AddCombatStatus(*Target, EGameXXKCardStatus::Agility, 1), 1);
	State.CardRun.ActiveBattle.CombatRandomState = 3;
	const FGameXXKRuntimeState Before = State;
	FGameXXKCardOutcomePreview Preview;
	FString Error;
	const bool bBuilt = FGameXXKCardOutcomePreviewRules::Build(
		State, PrimaryCardInstanceId, EnemyOneUnitId, Preview, &Error);
	TestTrue(FString::Printf(TEXT("real adapter-backed dodge preview succeeds: %s"), *Error), bBuilt);
	TestTrue(TEXT("preview never mutates any field in the complete source state"),
		FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(&State, &Before, PPF_None));
	if (!bBuilt || !Preview.FocusedTarget.IsSet())
	{
		return false;
	}
	TestEqual(TEXT("deterministic Agility preview reports actual zero direct damage"), Preview.FocusedTarget->DirectDamage, 0);
	TestTrue(TEXT("deterministic Agility preview retains the avoided audit flag"), Preview.FocusedTarget->bAvoided);
	TestEqual(TEXT("zero-damage attack attempt remains visible"), Preview.FocusedLines.Num(), 1);
	if (Preview.FocusedLines.Num() == 1)
	{
		TestEqual(TEXT("avoided attempt uses concise text"), RenderLine(Preview.FocusedLines[0]), FString(TEXT("伤害 0")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCardOutcomePreviewMatchesCommittedCopyTest,
	"GameXXK.Data.CardOutcomePreview.Rules.MatchesCommittedCopy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardOutcomePreviewMatchesCommittedCopyTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKCardOutcomePreviewTest;
	FGameXXKRuntimeState State;
	if (!BuildStandardState(*this, State, TEXT("Hero.Generic.QingFengYiShi"), EGameXXKCharacterRole::Hero, 61102))
	{
		return false;
	}
	FGameXXKCardCombatUnit* Owner = FindUnit(State, OwnerUnitId);
	FGameXXKCardCombatUnit* Target = FindUnit(State, EnemyOneUnitId);
	if (!TestNotNull(TEXT("parity fixture exposes owner"), Owner)
		|| !TestNotNull(TEXT("parity fixture exposes target"), Target))
	{
		return false;
	}
	Owner->Attack = 40;
	Target->HP = 500;
	Target->MaxHP = 500;
	Target->Defense = 5;
	Target->Armor = 7;
	TestEqual(TEXT("parity fixture applies Momentum3"), GameXXKCardRules::AddCombatStatus(*Owner, EGameXXKCardStatus::Momentum, 3), 3);
	TestEqual(TEXT("parity fixture applies Weak1"), GameXXKCardRules::AddCombatStatus(*Owner, EGameXXKCardStatus::Weak, 1), 1);
	TestEqual(TEXT("parity fixture applies Vulnerability2"), GameXXKCardRules::AddCombatStatus(*Target, EGameXXKCardStatus::Vulnerability, 2), 2);
	TestEqual(TEXT("parity fixture applies Mark1"), GameXXKCardRules::AddCombatStatus(*Target, EGameXXKCardStatus::Mark, 1), 1);
	const FGameXXKRuntimeState Before = State;

	FGameXXKCardOutcomePreview Preview;
	FString PreviewError;
	if (!TestTrue(
		FString::Printf(TEXT("preview resolves the real Momentum/Weak/armor/Vulnerability/Mark stack: %s"), *PreviewError),
		FGameXXKCardOutcomePreviewRules::Build(
			State, PrimaryCardInstanceId, EnemyOneUnitId, Preview, &PreviewError)))
	{
		return false;
	}
	FGameXXKRuntimeState CommittedState = State;
	FGameXXKCardPlayResult CommittedResult;
	FString CommitError;
	if (!TestTrue(
		FString::Printf(TEXT("independent state copy commits through the same public adapter: %s"), *CommitError),
		FGameXXKCardBattleAdapter::ResolveCardPlay(
			CommittedState, PrimaryCardInstanceId, EnemyOneUnitId, CommittedResult, &CommitError)))
	{
		return false;
	}

	FGameXXKRuntimeState AuditState = State;
	FGameXXKCardPlayResult AuditResult;
	FString AuditError;
	if (!TestTrue(TEXT("second independent adapter copy resolves for packet determinism"),
		FGameXXKCardBattleAdapter::ResolveCardPlay(
			AuditState, PrimaryCardInstanceId, EnemyOneUnitId, AuditResult, &AuditError)))
	{
		AddError(AuditError);
		return false;
	}
	ComparePacketArrays(*this, CommittedResult.DamageResults, AuditResult.DamageResults, TEXT("damage"));
	ComparePacketArrays(*this, CommittedResult.HealingResults, AuditResult.HealingResults, TEXT("healing"));
	ComparePacketArrays(*this, CommittedResult.ArmorResults, AuditResult.ArmorResults, TEXT("armor"));
	if (!Preview.FocusedTarget.IsSet())
	{
		AddError(TEXT("committed-copy preview omitted its focused enemy"));
		return false;
	}
	int32 ExpectedDirectDamage = 0;
	for (const FGameXXKCardDamageResult& Packet : CommittedResult.DamageResults)
	{
		if (Packet.ResolvedTargetUnitId == EnemyOneUnitId
			&& Packet.Kind == EGameXXKCardDamageKind::SingleTargetAttack
			&& Packet.Cause == EGameXXKCardDamageCause::DirectAttack)
		{
			ExpectedDirectDamage += Packet.HealthDamage;
		}
	}
	TestEqual(TEXT("preview aggregate uses only committed HealthDamage"), Preview.FocusedTarget->DirectDamage, ExpectedDirectDamage);
	TestTrue(TEXT("both committed copies advance only their own combat-random streams"),
		CommittedState.CardRun.ActiveBattle.CombatRandomState != State.CardRun.ActiveBattle.CombatRandomState
		&& AuditState.CardRun.ActiveBattle.CombatRandomState == CommittedState.CardRun.ActiveBattle.CombatRandomState);
	TestTrue(TEXT("both preview and independent commits preserve the original complete input"),
		FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(&State, &Before, PPF_None));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCardOutcomePreviewFailureClearsStaleOutputTest,
	"GameXXK.Data.CardOutcomePreview.Rules.FailureClearsStaleOutput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardOutcomePreviewFailureClearsStaleOutputTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKCardOutcomePreviewTest;
	FGameXXKRuntimeState State;
	if (!BuildStandardState(*this, State, TEXT("Hero.Generic.QingFengYiShi"), EGameXXKCharacterRole::Hero, 61103))
	{
		return false;
	}
	FGameXXKCardOutcomePreview Preview;
	FString Error;
	if (!TestTrue(TEXT("stale-output fixture first builds a real success"),
		FGameXXKCardOutcomePreviewRules::Build(State, PrimaryCardInstanceId, EnemyOneUnitId, Preview, &Error)))
	{
		return false;
	}
	TestTrue(TEXT("successful fixture actually populated stale-prone target data"), Preview.FocusedTarget.IsSet());
	TestTrue(TEXT("successful fixture actually populated stale-prone lines"), !Preview.FocusedLines.IsEmpty());
	Error.Reset();
	TestFalse(TEXT("the same output rejects an invalid stable card instance"),
		FGameXXKCardOutcomePreviewRules::Build(
			State, TEXT("OutcomePreview.Card.DoesNotExist"), EnemyOneUnitId, Preview, &Error));
	TestFalse(TEXT("failure clears the success flag"), Preview.bSuccess);
	TestEqual(TEXT("failure text is the only UI-facing error"), Preview.FailureText, FString(TEXT("无法预演")));
	TestFalse(TEXT("failure clears the focused target"), Preview.FocusedTarget.IsSet());
	TestTrue(TEXT("failure clears all enemy-position targets"), Preview.EnemyPositionTargets.IsEmpty());
	TestTrue(TEXT("failure clears all focused lines"), Preview.FocusedLines.IsEmpty());
	TestTrue(TEXT("failure clears all enemy-position lines"), Preview.EnemyPositionLines.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCardOutcomePreviewManualDamageAndRedirectTest,
	"GameXXK.Data.CardOutcomePreview.Rules.ManualDamageAndRedirect",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardOutcomePreviewManualDamageAndRedirectTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKCardOutcomePreviewTest;
	FGameXXKRuntimeState State;
	if (!BuildStandardState(
		*this, State, TEXT("Hero.Generic.QingFengYiShi"), EGameXXKCharacterRole::Hero, 61104, true))
	{
		return false;
	}
	FGameXXKCardGuardLinkRuntime& Link = State.CardRun.ActiveBattle.GuardLinks.AddDefaulted_GetRef();
	Link.GuardianUnitId = EnemyTwoUnitId;
	Link.ProtectedUnitId = EnemyOneUnitId;
	Link.Stacks = 1;
	Link.RedirectPolicy = EGameXXKCardGuardRedirectPolicy::RedirectNextSingleTargetDirectAttackToGuardian;
	FString ValidationError;
	if (!TestTrue(TEXT("real enemy-side redirect fixture validates"),
		GameXXKCardRules::ValidateCardBattleRuntime(State.CardRun.ActiveBattle, &ValidationError)))
	{
		AddError(ValidationError);
		return false;
	}
	const FGameXXKRuntimeState Before = State;
	FGameXXKCardOutcomePreview Preview;
	FString Error;
	if (!TestTrue(
		FString::Printf(TEXT("manual redirect preview succeeds through the adapter: %s"), *Error),
		FGameXXKCardOutcomePreviewRules::Build(
			State, PrimaryCardInstanceId, EnemyOneUnitId, Preview, &Error)))
	{
		return false;
	}
	TestEqual(TEXT("manual card classification is stable"), Preview.Classification, EGameXXKCardOutcomePreviewClass::ManualUnit);
	TestFalse(TEXT("a redirect does not invent a group-position list"), Preview.bUsesEnemyPositionList);
	TestTrue(TEXT("manual redirect keeps only the hovered focus"), Preview.FocusedTarget.IsSet());
	TestTrue(TEXT("manual redirect does not expand the guardian or relic targets"), Preview.EnemyPositionTargets.IsEmpty());
	if (!Preview.FocusedTarget.IsSet())
	{
		return false;
	}
	TestEqual(TEXT("redirect focus remains the original stable hovered unit"), Preview.FocusedTarget->UnitId, EnemyOneUnitId);
	TestEqual(TEXT("redirect focus keeps zero actual health damage"), Preview.FocusedTarget->DirectDamage, 0);
	TestTrue(TEXT("redirect focus exposes the real redirect audit"), Preview.FocusedTarget->bRedirected);
	TestFalse(TEXT("redirect focus is not misreported as avoided"), Preview.FocusedTarget->bAvoided);
	TestEqual(TEXT("redirect produces exactly one concise focus line"), Preview.FocusedLines.Num(), 1);
	if (Preview.FocusedLines.Num() == 1)
	{
		TestEqual(TEXT("redirect text is concise and ordered"),
			RenderLine(Preview.FocusedLines[0]), FString(TEXT("伤害 0 · 已改向")));
	}
	TestTrue(TEXT("redirect preview preserves the full original state"),
		FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(&State, &Before, PPF_None));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCardOutcomePreviewGroupPositionsTest,
	"GameXXK.Data.CardOutcomePreview.Rules.GroupPositions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardOutcomePreviewGroupPositionsTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKCardOutcomePreviewTest;
	TArray<FGameXXKCardCombatUnit> Units = {
		MakeUnit(OwnerUnitId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Healer, 0),
		MakeUnit(AllyUnitId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Guard, 1),
		MakeUnit(EnemyThreeUnitId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 12, 3),
		MakeUnit(EnemyOneUnitId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 10, 1)};
	FGameXXKRuntimeState State;
	if (!BuildState(
		*this,
		State,
		TEXT("Profession.Healer.YaoNangFeiTou"),
		EGameXXKCharacterRole::Healer,
		Units,
		61105))
	{
		return false;
	}
	FindUnit(State, EnemyOneUnitId)->HP = 10;
	FindUnit(State, EnemyOneUnitId)->MaxHP = 10;
	FString RelicError;
	if (!TestTrue(FString::Printf(TEXT("group fixture acquires the real Drum Charm: %s"), *RelicError),
		FGameXXKRelicRules::AcquireRelic(State, TEXT("Relic.DrumCharm"), &RelicError)))
	{
		return false;
	}
	FGameXXKRuntimeState GroupOracleState = State;
	FGameXXKCardPlayResult GroupOracleResult;
	FString GroupOracleError;
	if (!TestTrue(TEXT("independent adapter oracle resolves the real group card"),
		FGameXXKCardBattleAdapter::ResolveCardPlay(
			GroupOracleState, PrimaryCardInstanceId, NAME_None, GroupOracleResult, &GroupOracleError)))
	{
		AddError(GroupOracleError);
		return false;
	}
	int32 OracleThreePBleedDamage = 0;
	for (const FGameXXKCardDamageResult& Packet : GroupOracleResult.DamageResults)
	{
		if (Packet.ResolvedTargetUnitId == EnemyThreeUnitId
			&& Packet.Cause == EGameXXKCardDamageCause::Bleed)
		{
			OracleThreePBleedDamage += Packet.HealthDamage;
		}
	}
	TestEqual(TEXT("direct adapter oracle records the card's immediate 3P Bleed health damage"),
		OracleThreePBleedDamage, 3);
	FGameXXKCardOutcomePreview Preview;
	FString Error;
	if (!TestTrue(
		FString::Printf(TEXT("real 1P/3P group preview succeeds: %s"), *Error),
		FGameXXKCardOutcomePreviewRules::Build(
			State, PrimaryCardInstanceId, NAME_None, Preview, &Error)))
	{
		return false;
	}
	TestEqual(TEXT("real all-enemies group is classified as pure group"),
		Preview.Classification, EGameXXKCardOutcomePreviewClass::PureEnemyGroup);
	TestTrue(TEXT("real GroupAttack packet enables the position list"), Preview.bUsesEnemyPositionList);
	TestFalse(TEXT("pure group does not duplicate a focused target"), Preview.FocusedTarget.IsSet());
	TestEqual(TEXT("only living 1P and 3P targets are precreated"), Preview.EnemyPositionTargets.Num(), 2);
	TestEqual(TEXT("only living 1P and 3P lines are emitted"), Preview.EnemyPositionLines.Num(), 2);
	if (Preview.EnemyPositionTargets.Num() != 2 || Preview.EnemyPositionLines.Num() != 2)
	{
		return false;
	}
	TestEqual(TEXT("first row is 1P"), Preview.EnemyPositionTargets[0].SlotNumber, 1);
	TestEqual(TEXT("second row is 3P"), Preview.EnemyPositionTargets[1].SlotNumber, 3);
	TestTrue(TEXT("1P lethal outcome remains present"), Preview.EnemyPositionTargets[0].bLethal);
	TestEqual(TEXT("1P uses actual capped group health damage"), Preview.EnemyPositionTargets[0].GroupDamage, 10);
	TestEqual(TEXT("dead 1P receives no later Drum packet"), Preview.EnemyPositionTargets[0].LinkedDamage, 0);
	TestEqual(TEXT("3P receives its own group health damage"), Preview.EnemyPositionTargets[1].GroupDamage, 13);
	TestEqual(TEXT("3P matches the direct adapter's immediate Bleed packets"),
		Preview.EnemyPositionTargets[1].BleedDamage, OracleThreePBleedDamage);
	TestEqual(TEXT("3P receives its own linked Drum health damage"), Preview.EnemyPositionTargets[1].LinkedDamage, 1);
	TestEqual(TEXT("1P row is concise and has no global total"),
		RenderLine(Preview.EnemyPositionLines[0]), FString(TEXT("1P · 群体伤害 10 · 致死")));
	TestEqual(TEXT("3P row includes only its own category total"),
		RenderLine(Preview.EnemyPositionLines[1]), FString(TEXT("3P · 群体伤害 13 · 流血 3 · 联动伤害 1 · 合计 17")));
	TestFalse(TEXT("group output contains no invented 2P row"),
		RenderLine(Preview.EnemyPositionLines[0]).Contains(TEXT("2P"))
		|| RenderLine(Preview.EnemyPositionLines[1]).Contains(TEXT("2P")));

	FGameXXKRuntimeState ManualGroupState;
	if (!BuildStandardState(
		*this,
		ManualGroupState,
		TEXT("Hero.Guard.XuanJiaZhenYue"),
		EGameXXKCharacterRole::Guard,
		61106))
	{
		return false;
	}
	FindUnit(ManualGroupState, AllyUnitId)->Armor = 20;
	FGameXXKCardOutcomePreview ManualGroupPreview;
	Error.Reset();
	if (!TestTrue(TEXT("manual ally card with a real group packet previews"),
		FGameXXKCardOutcomePreviewRules::Build(
			ManualGroupState, PrimaryCardInstanceId, AllyUnitId, ManualGroupPreview, &Error)))
	{
		AddError(Error);
		return false;
	}
	TestEqual(TEXT("manual plus group stays ManualUnit"),
		ManualGroupPreview.Classification, EGameXXKCardOutcomePreviewClass::ManualUnit);
	TestTrue(TEXT("manual plus group still enables enemy positions"), ManualGroupPreview.bUsesEnemyPositionList);
	TestTrue(TEXT("manual party target remains separately addressable"), ManualGroupPreview.FocusedTarget.IsSet());
	TestTrue(TEXT("manual party target with no immediate positive audit has no empty panel"),
		ManualGroupPreview.FocusedLines.IsEmpty());
	TestEqual(TEXT("manual group creates one row for its only enemy"), ManualGroupPreview.EnemyPositionLines.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCardOutcomePreviewDotToxicMedicineRelicTest,
	"GameXXK.Data.CardOutcomePreview.Rules.DotToxicMedicineRelic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardOutcomePreviewDotToxicMedicineRelicTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKCardOutcomePreviewTest;
	struct FDotCase
	{
		EGameXXKCardStatus Status;
		EGameXXKCardDamageCause Cause;
		const TCHAR* Label;
	};
	const FDotCase DotCases[] = {
		{EGameXXKCardStatus::Bleed, EGameXXKCardDamageCause::Bleed, TEXT("Bleed")},
		{EGameXXKCardStatus::Poison, EGameXXKCardDamageCause::Poison, TEXT("Poison")},
		{EGameXXKCardStatus::Burn, EGameXXKCardDamageCause::Burn, TEXT("Burn")}};
	for (int32 CaseIndex = 0; CaseIndex < UE_ARRAY_COUNT(DotCases); ++CaseIndex)
	{
		const FDotCase& DotCase = DotCases[CaseIndex];
		FGameXXKRuntimeState State;
		if (!BuildStandardState(
			*this, State, TEXT("Hero.Generic.HeYuZhan"), EGameXXKCharacterRole::Hero, 61110 + CaseIndex))
		{
			return false;
		}
		FGameXXKCardCombatUnit* Target = FindUnit(State, EnemyOneUnitId);
		TestEqual(FString::Printf(TEXT("%s fixture applies three real DOT stacks"), DotCase.Label),
			GameXXKCardRules::AddCombatStatus(*Target, DotCase.Status, 3), 3);
		TestEqual(FString::Printf(TEXT("%s fixture applies Rot2"), DotCase.Label),
			GameXXKCardRules::AddCombatStatus(*Target, EGameXXKCardStatus::DamageOverTime, 2), 2);
		FGameXXKCardOutcomePreview Preview;
		FString Error;
		if (!TestTrue(FString::Printf(TEXT("%s plus Rot preview succeeds: %s"), DotCase.Label, *Error),
			FGameXXKCardOutcomePreviewRules::Build(
				State, PrimaryCardInstanceId, EnemyOneUnitId, Preview, &Error)))
		{
			return false;
		}
		FGameXXKRuntimeState Committed = State;
		FGameXXKCardPlayResult Result;
		if (!TestTrue(TEXT("DOT parity copy resolves through the adapter"),
			FGameXXKCardBattleAdapter::ResolveCardPlay(
				Committed, PrimaryCardInstanceId, EnemyOneUnitId, Result, &Error)))
		{
			AddError(Error);
			return false;
		}
		const int32 ExpectedDotDamage = SumDamageCause(Result.DamageResults, EnemyOneUnitId, DotCase.Cause);
		const int32 PreviewDotDamage = DotCase.Status == EGameXXKCardStatus::Bleed
			? Preview.FocusedTarget->BleedDamage
			: DotCase.Status == EGameXXKCardStatus::Poison
				? Preview.FocusedTarget->PoisonDamage
				: Preview.FocusedTarget->BurnDamage;
		TestTrue(FString::Printf(TEXT("%s creates real positive DOT damage"), DotCase.Label), ExpectedDotDamage > 0);
		TestEqual(FString::Printf(TEXT("%s preview includes the actual Rot-augmented DOT packet"), DotCase.Label),
			PreviewDotDamage, ExpectedDotDamage);
		TestEqual(FString::Printf(TEXT("%s resolution preserves unrelated Rot stacks"), DotCase.Label),
			Status(Committed, EnemyOneUnitId, EGameXXKCardStatus::DamageOverTime), 2);
	}

	FGameXXKRuntimeState ToxicState;
	if (!BuildStandardState(
		*this, ToxicState, TEXT("Hero.Healer.DuHuoTongLu"), EGameXXKCharacterRole::Healer, 61120))
	{
		return false;
	}
	FGameXXKCardCombatUnit* ToxicTarget = FindUnit(ToxicState, EnemyOneUnitId);
	GameXXKCardRules::AddCombatStatus(*ToxicTarget, EGameXXKCardStatus::Bleed, 3);
	GameXXKCardRules::AddCombatStatus(*ToxicTarget, EGameXXKCardStatus::Poison, 2);
	GameXXKCardRules::AddCombatStatus(*ToxicTarget, EGameXXKCardStatus::Burn, 4);
	GameXXKCardRules::AddCombatStatus(*ToxicTarget, EGameXXKCardStatus::DamageOverTime, 2);
	if (!InstallSetTier(
		*this,
		ToxicState,
		TEXT("Set.ShiGu.6"),
		static_cast<EGameXXKEquipmentModifierKind>(40)))
	{
		return false;
	}
	FGameXXKCardOutcomePreview ToxicPreview;
	FString Error;
	if (!TestTrue(TEXT("three-type Toxic Explosion previews through the full state copy"),
		FGameXXKCardOutcomePreviewRules::Build(
			ToxicState, PrimaryCardInstanceId, EnemyOneUnitId, ToxicPreview, &Error)))
	{
		AddError(Error);
		return false;
	}
	FGameXXKRuntimeState ToxicCommitted = ToxicState;
	FGameXXKCardPlayResult ToxicResult;
	if (!TestTrue(TEXT("three-type Toxic Explosion commits on an independent copy"),
		FGameXXKCardBattleAdapter::ResolveCardPlay(
			ToxicCommitted, PrimaryCardInstanceId, EnemyOneUnitId, ToxicResult, &Error)))
	{
		AddError(Error);
		return false;
	}
	TestTrue(TEXT("real Toxic Explosion reports a three-type operation"),
		ToxicResult.ToxicExplosionDistinctDotTypeCounts.Contains(3));
	const int32 ExpectedToxic =
		SumDamageCause(ToxicResult.DamageResults, EnemyOneUnitId, EGameXXKCardDamageCause::ToxicExplosionBleed)
		+ SumDamageCause(ToxicResult.DamageResults, EnemyOneUnitId, EGameXXKCardDamageCause::ToxicExplosionPoison)
		+ SumDamageCause(ToxicResult.DamageResults, EnemyOneUnitId, EGameXXKCardDamageCause::ToxicExplosionBurn);
	TestEqual(TEXT("three Toxic Explosion packets aggregate into one preview category"),
		ToxicPreview.FocusedTarget->ToxicExplosionDamage, ExpectedToxic);
	TestEqual(TEXT("ShiGu first explosion preserves live Rot"),
		Status(ToxicCommitted, EnemyOneUnitId, EGameXXKCardStatus::DamageOverTime), 2);
	TestEqual(TEXT("direct plus Toxic Explosion stacks one row per damage type"), ToxicPreview.FocusedLines.Num(), 3);
	if (ToxicPreview.FocusedLines.Num() == 3)
	{
		TestTrue(TEXT("direct damage owns its own focused row"),
			HasSegment(ToxicPreview.FocusedLines[0],
				FString::Printf(TEXT("伤害 %d"), ToxicPreview.FocusedTarget->DirectDamage),
				EGameXXKCardOutcomeTone::Damage));
		TestTrue(TEXT("toxic explosion owns the dot row"),
			HasSegment(ToxicPreview.FocusedLines[1],
				FString::Printf(TEXT("毒爆 %d"), ExpectedToxic),
				EGameXXKCardOutcomeTone::Dot));
		TestEqual(TEXT("the mixed outcome keeps its total on a trailing row"),
			RenderLine(ToxicPreview.FocusedLines[2]),
			FString::Printf(TEXT("合计 %d"),
				ToxicPreview.FocusedTarget->DirectDamage
					+ ToxicPreview.FocusedTarget->BleedDamage
					+ ToxicPreview.FocusedTarget->PoisonDamage
					+ ToxicPreview.FocusedTarget->BurnDamage
					+ ToxicPreview.FocusedTarget->ToxicExplosionDamage));
	}

	FGameXXKRuntimeState MedicineState;
	if (!BuildStandardState(
		*this, MedicineState, TEXT("Profession.Healer.CaoMuFuZhi"), EGameXXKCharacterRole::Healer, 61121)
		|| !InstallHealerFormula(*this, MedicineState, TEXT("Profession.Healer.CaoMuFuZhi")))
	{
		return false;
	}
	TestEqual(TEXT("Medicine fixture applies the consumed Medicine5 snapshot"),
		GameXXKCardRules::AddCombatStatus(
			*FindUnit(MedicineState, OwnerUnitId), EGameXXKCardStatus::Medicine, 5), 5);
	FString RelicError;
	if (!TestTrue(TEXT("Medicine fixture acquires real Drum linked damage"),
		FGameXXKRelicRules::AcquireRelic(MedicineState, TEXT("Relic.DrumCharm"), &RelicError)))
	{
		AddError(RelicError);
		return false;
	}
	FGameXXKCardOutcomePreview MedicinePreview;
	if (!TestTrue(TEXT("Medicine plus relic preview succeeds"),
		FGameXXKCardOutcomePreviewRules::Build(
			MedicineState, PrimaryCardInstanceId, EnemyOneUnitId, MedicinePreview, &Error)))
	{
		AddError(Error);
		return false;
	}
	FGameXXKRuntimeState MedicineCommitted = MedicineState;
	FGameXXKCardPlayResult MedicineResult;
	if (!TestTrue(TEXT("Medicine plus relic independent copy commits"),
		FGameXXKCardBattleAdapter::ResolveCardPlay(
			MedicineCommitted, PrimaryCardInstanceId, EnemyOneUnitId, MedicineResult, &Error)))
	{
		AddError(Error);
		return false;
	}
	const int32 ExpectedMedicine = SumDamageCause(
		MedicineResult.DamageResults, EnemyOneUnitId, EGameXXKCardDamageCause::Medicine);
	const int32 ExpectedLinked = SumDamageCause(
		MedicineResult.DamageResults, EnemyOneUnitId, EGameXXKCardDamageCause::Relic);
	TestTrue(TEXT("Medicine reversal creates actual health damage"), ExpectedMedicine > 0);
	TestTrue(TEXT("Drum creates actual linked health damage"), ExpectedLinked > 0);
	TestEqual(TEXT("preview keeps Medicine damage separate"),
		MedicinePreview.FocusedTarget->MedicineDamage, ExpectedMedicine);
	TestEqual(TEXT("preview keeps linked relic damage separate"),
		MedicinePreview.FocusedTarget->LinkedDamage, ExpectedLinked);
	TestEqual(TEXT("the consumed snapshot does not remove newly granted Medicine"),
		Status(MedicineCommitted, OwnerUnitId, EGameXXKCardStatus::Medicine), 2);
	TestEqual(TEXT("Medicine plus linked damage gets a row per type plus one total row"),
		MedicinePreview.FocusedLines.Num(), 3);
	if (MedicinePreview.FocusedLines.Num() == 3)
	{
		TestTrue(TEXT("relic phrase has the Damage tone on its own row"),
			HasSegment(MedicinePreview.FocusedLines[0],
				FString::Printf(TEXT("联动伤害 %d"), ExpectedLinked), EGameXXKCardOutcomeTone::Damage));
		TestTrue(TEXT("Medicine phrase has the Medicine tone on its own row"),
			HasSegment(MedicinePreview.FocusedLines[1],
				FString::Printf(TEXT("药效伤害 %d"), ExpectedMedicine), EGameXXKCardOutcomeTone::Medicine));
		TestEqual(TEXT("the mixed outcome keeps its total on a trailing row"),
			RenderLine(MedicinePreview.FocusedLines[2]),
			FString::Printf(TEXT("合计 %d"),
				MedicinePreview.FocusedTarget->DirectDamage
					+ MedicinePreview.FocusedTarget->GroupDamage
					+ MedicinePreview.FocusedTarget->BleedDamage
					+ MedicinePreview.FocusedTarget->PoisonDamage
					+ MedicinePreview.FocusedTarget->BurnDamage
					+ MedicinePreview.FocusedTarget->ToxicExplosionDamage
					+ MedicinePreview.FocusedTarget->MedicineDamage
					+ MedicinePreview.FocusedTarget->LinkedDamage));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCardOutcomePreviewHealingArmorAndZeroTest,
	"GameXXK.Data.CardOutcomePreview.Rules.HealingArmorAndZero",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardOutcomePreviewHealingArmorAndZeroTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKCardOutcomePreviewTest;
	FGameXXKRuntimeState State;
	if (!BuildStandardState(
		*this, State, TEXT("Profession.Healer.WenYangGao"), EGameXXKCharacterRole::Healer, 61130))
	{
		return false;
	}
	FGameXXKCardCombatUnit* Ally = FindUnit(State, AllyUnitId);
	Ally->HP = 490;
	Ally->Armor = 93;
	FGameXXKCardOutcomePreview Preview;
	FString Error;
	if (!TestTrue(TEXT("combined healing and armor preview succeeds"),
		FGameXXKCardOutcomePreviewRules::Build(
			State, PrimaryCardInstanceId, AllyUnitId, Preview, &Error)))
	{
		AddError(Error);
		return false;
	}
	TestTrue(TEXT("combined positive preview keeps the selected ally"), Preview.FocusedTarget.IsSet());
	TestEqual(TEXT("combined preview uses actual effective healing"), Preview.FocusedTarget->EffectiveHealing, 10);
	TestEqual(TEXT("combined preview uses actual effective armor"), Preview.FocusedTarget->EffectiveArmor, 6);
	TestEqual(TEXT("healing and armor split into one row per type"), Preview.FocusedLines.Num(), 2);
	if (Preview.FocusedLines.Num() == 2)
	{
		TestEqual(TEXT("healing owns its own row"),
			RenderLine(Preview.FocusedLines[0]), FString(TEXT("治疗 +10")));
		TestTrue(TEXT("healing segment has Healing tone"),
			HasSegment(Preview.FocusedLines[0], TEXT("治疗 +10"), EGameXXKCardOutcomeTone::Healing));
		TestEqual(TEXT("armor owns the trailing row"),
			RenderLine(Preview.FocusedLines[1]), FString(TEXT("护甲 +6")));
		TestTrue(TEXT("armor segment has Armor tone"),
			HasSegment(Preview.FocusedLines[1], TEXT("护甲 +6"), EGameXXKCardOutcomeTone::Armor));
	}

	FGameXXKRuntimeState ZeroState;
	if (!BuildStandardState(
		*this, ZeroState, TEXT("Profession.Healer.WenYangGao"), EGameXXKCharacterRole::Healer, 61131))
	{
		return false;
	}
	FindUnit(ZeroState, AllyUnitId)->Armor = 99;
	FGameXXKCardOutcomePreview ZeroPreview;
	Error.Reset();
	if (!TestTrue(TEXT("legal full-health/full-armor attempt previews"),
		FGameXXKCardOutcomePreviewRules::Build(
			ZeroState, PrimaryCardInstanceId, AllyUnitId, ZeroPreview, &Error)))
	{
		AddError(Error);
		return false;
	}
	TestEqual(TEXT("full-health attempt retains effective healing zero"), ZeroPreview.FocusedTarget->EffectiveHealing, 0);
	TestEqual(TEXT("armor-cap attempt retains effective armor zero"), ZeroPreview.FocusedTarget->EffectiveArmor, 0);
	TestEqual(TEXT("legal zero attempts still create one row per type"), ZeroPreview.FocusedLines.Num(), 2);
	if (ZeroPreview.FocusedLines.Num() == 2)
	{
		TestEqual(TEXT("legal zero healing text remains explicit"),
			RenderLine(ZeroPreview.FocusedLines[0]), FString(TEXT("治疗 +0")));
		TestEqual(TEXT("legal zero armor text remains explicit"),
			RenderLine(ZeroPreview.FocusedLines[1]), FString(TEXT("护甲 +0")));
	}

	FGameXXKRuntimeState SelfLossState;
	if (!BuildStandardState(
		*this, SelfLossState, TEXT("Profession.Healer.JinChuangXuMing"), EGameXXKCharacterRole::Healer, 61132)
		|| !InstallHealerFormula(*this, SelfLossState, TEXT("Profession.Healer.XingQiZhen")))
	{
		return false;
	}
	FindUnit(SelfLossState, AllyUnitId)->HP = 480;
	FindUnit(SelfLossState, AllyUnitId)->MaxHP = 500;
	FGameXXKRuntimeState SelfLossCommitted = SelfLossState;
	FGameXXKCardPlayResult SelfLossResult;
	Error.Reset();
	if (!TestTrue(TEXT("direct adapter oracle resolves real party self-loss plus formula healing"),
		FGameXXKCardBattleAdapter::ResolveCardPlay(
			SelfLossCommitted, PrimaryCardInstanceId, AllyUnitId, SelfLossResult, &Error)))
	{
		AddError(Error);
		return false;
	}
	const int32 AllySelfLoss = SumDamageCause(
		SelfLossResult.DamageResults, AllyUnitId, EGameXXKCardDamageCause::SelfLoss);
	int32 AllyHealing = 0;
	for (const FGameXXKCardHealingResult& Healing : SelfLossResult.HealingResults)
	{
		if (Healing.TargetUnitId == AllyUnitId)
		{
			AllyHealing += Healing.EffectiveHealing;
		}
	}
	TestEqual(TEXT("real high-energy healer formula emits selected-ally SelfLoss"), AllySelfLoss, 1);
	TestEqual(TEXT("real card plus formula heals the selected ally"), AllyHealing, 14);
	FGameXXKCardOutcomePreview SelfLossPreview;
	Error.Reset();
	if (!TestTrue(TEXT("party self-loss plus healing preview succeeds"),
		FGameXXKCardOutcomePreviewRules::Build(
			SelfLossState, PrimaryCardInstanceId, AllyUnitId, SelfLossPreview, &Error)))
	{
		AddError(Error);
		return false;
	}
	if (!TestTrue(TEXT("party self-loss preview keeps the selected ally focus"), SelfLossPreview.FocusedTarget.IsSet()))
	{
		return false;
	}
	TestEqual(TEXT("party focus keeps only the adapter's effective healing"),
		SelfLossPreview.FocusedTarget->EffectiveHealing, AllyHealing);
	TestEqual(TEXT("party SelfLoss is never mislabeled as linked damage"),
		SelfLossPreview.FocusedTarget->LinkedDamage, 0);
	TestEqual(TEXT("party focus never exposes SelfLoss as a direct category"),
		SelfLossPreview.FocusedTarget->DirectDamage, 0);
	TestEqual(TEXT("party self-loss plus healing remains one concise line"), SelfLossPreview.FocusedLines.Num(), 1);
	if (SelfLossPreview.FocusedLines.Num() == 1)
	{
		TestEqual(TEXT("party self-loss text contains only healing"),
			RenderLine(SelfLossPreview.FocusedLines[0]), FString(TEXT("治疗 +14")));
		TestFalse(TEXT("party focus has no linked-damage phrase"),
			RenderLine(SelfLossPreview.FocusedLines[0]).Contains(TEXT("联动伤害")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCardOutcomePreviewHeavyArrowPassiveAndLethalTest,
	"GameXXK.Data.CardOutcomePreview.Rules.HeavyArrowPassiveAndLethal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardOutcomePreviewHeavyArrowPassiveAndLethalTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKCardOutcomePreviewTest;
	FGameXXKRuntimeState HeavyState;
	if (!BuildStandardState(
		*this, HeavyState, TEXT("Hero.Hunter.LieYuLianShi"), EGameXXKCharacterRole::Hunter, 61140))
	{
		return false;
	}
	TestEqual(TEXT("Heavy Arrow fixture applies Charge3"),
		GameXXKCardRules::AddCombatStatus(
			*FindUnit(HeavyState, OwnerUnitId), EGameXXKCardStatus::Charge, 3), 3);
	FGameXXKCardOutcomePreview HeavyPreview;
	FString Error;
	if (!TestTrue(TEXT("multi-hit Heavy Arrow preview succeeds"),
		FGameXXKCardOutcomePreviewRules::Build(
			HeavyState, PrimaryCardInstanceId, EnemyOneUnitId, HeavyPreview, &Error)))
	{
		AddError(Error);
		return false;
	}
	FGameXXKRuntimeState HeavyCommitted = HeavyState;
	FGameXXKCardPlayResult HeavyResult;
	if (!TestTrue(TEXT("multi-hit Heavy Arrow commits on an independent copy"),
		FGameXXKCardBattleAdapter::ResolveCardPlay(
			HeavyCommitted, PrimaryCardInstanceId, EnemyOneUnitId, HeavyResult, &Error)))
	{
		AddError(Error);
		return false;
	}
	TestEqual(TEXT("real Heavy Arrow consumes Charge3 once"), HeavyResult.HeavyArrowChargeConsumed, 3);
	TestEqual(TEXT("real Heavy Arrow appends exactly three extra hits"), HeavyResult.HeavyArrowExtraAttackCount, 3);
	int32 ExpectedDirect = 0;
	for (const FGameXXKCardDamageResult& Packet : HeavyResult.DamageResults)
	{
		ExpectedDirect += Packet.ResolvedTargetUnitId == EnemyOneUnitId
			&& Packet.Kind == EGameXXKCardDamageKind::SingleTargetAttack
			&& Packet.Cause == EGameXXKCardDamageCause::DirectAttack
			? Packet.HealthDamage : 0;
	}
	TestEqual(TEXT("multi-hit preview aggregates every actual direct packet"),
		HeavyPreview.FocusedTarget->DirectDamage, ExpectedDirect);

	FGameXXKRuntimeState LethalState;
	if (!BuildStandardState(
		*this, LethalState, TEXT("Hero.Hunter.LieYuLianShi"), EGameXXKCharacterRole::Hunter, 61141))
	{
		return false;
	}
	GameXXKCardRules::AddCombatStatus(
		*FindUnit(LethalState, OwnerUnitId), EGameXXKCardStatus::Charge, 3);
	FindUnit(LethalState, EnemyOneUnitId)->HP = 35;
	FindUnit(LethalState, EnemyOneUnitId)->MaxHP = 35;
	FGameXXKCardOutcomePreview LethalPreview;
	Error.Reset();
	if (!TestTrue(TEXT("lethal Heavy Arrow preview succeeds"),
		FGameXXKCardOutcomePreviewRules::Build(
			LethalState, PrimaryCardInstanceId, EnemyOneUnitId, LethalPreview, &Error)))
	{
		AddError(Error);
		return false;
	}
	FGameXXKRuntimeState LethalCommitted = LethalState;
	FGameXXKCardPlayResult LethalResult;
	if (!TestTrue(TEXT("lethal Heavy Arrow commits independently"),
		FGameXXKCardBattleAdapter::ResolveCardPlay(
			LethalCommitted, PrimaryCardInstanceId, EnemyOneUnitId, LethalResult, &Error)))
	{
		AddError(Error);
		return false;
	}
	TestEqual(TEXT("lethal base packet cancels every later Heavy Arrow hit"), LethalResult.HeavyArrowExtraAttackCount, 0);
	TestEqual(TEXT("lethal preview uses capped actual health damage"), LethalPreview.FocusedTarget->DirectDamage, 35);
	TestTrue(TEXT("lethal preview derives lethal from the actual health transition"), LethalPreview.FocusedTarget->bLethal);
	TestEqual(TEXT("single-category lethal stays on one concise line"), LethalPreview.FocusedLines.Num(), 1);
	if (LethalPreview.FocusedLines.Num() == 1)
	{
		TestEqual(TEXT("lethal text is ordered after damage"),
			RenderLine(LethalPreview.FocusedLines[0]), FString(TEXT("伤害 35 · 致死")));
		TestTrue(TEXT("lethal marker has Lethal tone"),
			HasSegment(LethalPreview.FocusedLines[0], TEXT("致死"), EGameXXKCardOutcomeTone::Lethal));
	}

	FGameXXKRuntimeState ThickHideState;
	if (!BuildStandardState(
		*this, ThickHideState, TEXT("Profession.Guard.ZhenDun"), EGameXXKCharacterRole::Guard, 61142))
	{
		return false;
	}
	FindUnit(ThickHideState, OwnerUnitId)->Attack = 20;
	FGameXXKCardCombatUnit* BlackBear = FindUnit(ThickHideState, EnemyOneUnitId);
	BlackBear->EnemyDefinitionId = TEXT("Enemy.Ch2.BlackBear");
	BlackBear->HP = 1000;
	BlackBear->MaxHP = 1000;
	BlackBear->Defense = 4;
	ThickHideState.CardRun.ActiveBattle.EnemyStates.Reset();
	FGameXXKCardOutcomePreview ThickHidePreview;
	Error.Reset();
	if (!TestTrue(TEXT("Black Bear Thick Hide preview succeeds"),
		FGameXXKCardOutcomePreviewRules::Build(
			ThickHideState, PrimaryCardInstanceId, EnemyOneUnitId, ThickHidePreview, &Error)))
	{
		AddError(Error);
		return false;
	}
	TestEqual(TEXT("Black Bear preview includes defense then Thick Hide actual damage"),
		ThickHidePreview.FocusedTarget->DirectDamage, 13);

	FGameXXKRuntimeState WhiteApeState;
	if (!BuildStandardState(
		*this, WhiteApeState, TEXT("Hero.Generic.SuiYanJi"), EGameXXKCharacterRole::Hero, 61143))
	{
		return false;
	}
	FGameXXKCardCombatUnit* WhiteApe = FindUnit(WhiteApeState, EnemyOneUnitId);
	WhiteApe->EnemyDefinitionId = TEXT("Enemy.Ch3.WhiteApe");
	WhiteApeState.CardRun.ActiveBattle.EnemyStates.Reset();
	FGameXXKEnemyBattleState& WhiteApeRuntimeState =
		WhiteApeState.CardRun.ActiveBattle.EnemyStates.FindOrAdd(EnemyOneUnitId);
	WhiteApeRuntimeState.DefinitionId = WhiteApe->EnemyDefinitionId;
	WhiteApeRuntimeState.bFirstStatusPassiveAvailable = true;
	FGameXXKCardOutcomePreview WhiteApePreview;
	Error.Reset();
	if (!TestTrue(TEXT("White Ape first-status preview succeeds"),
		FGameXXKCardOutcomePreviewRules::Build(
			WhiteApeState, PrimaryCardInstanceId, EnemyOneUnitId, WhiteApePreview, &Error)))
	{
		AddError(Error);
		return false;
	}
	TestEqual(TEXT("White Ape preview includes its real first-status armor packet"),
		WhiteApePreview.FocusedTarget->EffectiveArmor, 8);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCardOutcomePreviewExcludedDamageDoesNotPropagateFlagsTest,
	"GameXXK.Data.CardOutcomePreview.Rules.ExcludedDamageDoesNotPropagateFlags",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardOutcomePreviewExcludedDamageDoesNotPropagateFlagsTest::RunTest(const FString& Parameters)
{
	FGameXXKCardDamageResult DirectPacket;
	DirectPacket.Kind = EGameXXKCardDamageKind::SingleTargetAttack;
	DirectPacket.Cause = EGameXXKCardDamageCause::DirectAttack;
	DirectPacket.HealthDamage = 7;
	DirectPacket.TargetHealthBefore = 20;
	DirectPacket.TargetHealthAfter = 13;

	FGameXXKCardDamageResult ExcludedPacket;
	ExcludedPacket.Kind = EGameXXKCardDamageKind::EnvironmentalHealthLoss;
	ExcludedPacket.Cause = EGameXXKCardDamageCause::Environment;
	ExcludedPacket.HealthDamage = 13;
	ExcludedPacket.TargetHealthBefore = 13;
	ExcludedPacket.TargetHealthAfter = 0;
	ExcludedPacket.bAvoidedByAgility = true;
	ExcludedPacket.bRedirected = true;

	TArray<FGameXXKCardDamageResult> Packets;
	Packets.Add(DirectPacket);
	Packets.Add(ExcludedPacket);
	const FGameXXKCardOutcomeTarget Aggregate =
		GameXXKCardOutcomePreviewTestBridge::AggregateDamagePackets(Packets);

	TestEqual(TEXT("displayable direct damage remains aggregated"), Aggregate.DirectDamage, 7);
	TestEqual(TEXT("excluded environment damage remains outside linked damage"), Aggregate.LinkedDamage, 0);
	TestFalse(TEXT("excluded packet cannot mark displayed damage avoided"), Aggregate.bAvoided);
	TestFalse(TEXT("excluded packet cannot mark displayed damage redirected"), Aggregate.bRedirected);
	TestFalse(TEXT("excluded packet cannot mark displayed damage lethal"), Aggregate.bLethal);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCardOutcomePreviewPendingChoiceDoesNotAutoSelectTest,
	"GameXXK.Data.CardOutcomePreview.Rules.PendingChoiceDoesNotAutoSelect",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardOutcomePreviewPendingChoiceDoesNotAutoSelectTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKCardOutcomePreviewTest;
	FGameXXKRuntimeState State;
	if (!BuildStandardState(
		*this,
		State,
		TEXT("Profession.FormationMaster.BaMenLunZhuan"),
		EGameXXKCharacterRole::FormationMaster,
		61150,
		false,
		10))
	{
		return false;
	}
	const FGameXXKRuntimeState Before = State;
	FGameXXKCardOutcomePreview Preview;
	FString Error;
	if (!TestTrue(TEXT("pending-choice card preview resolves only its immediate card audit"),
		FGameXXKCardOutcomePreviewRules::Build(
			State, PrimaryCardInstanceId, NAME_None, Preview, &Error)))
	{
		AddError(Error);
		return false;
	}
	TestTrue(TEXT("pending-choice preview succeeds without inventing numeric lines"), Preview.bSuccess);
	TestEqual(TEXT("nonmanual no-group pending card classifies None"),
		Preview.Classification, EGameXXKCardOutcomePreviewClass::None);
	TestTrue(TEXT("pending card has no focused numeric panel"), Preview.FocusedLines.IsEmpty());
	TestTrue(TEXT("pending card has no enemy-position numeric panel"), Preview.EnemyPositionLines.IsEmpty());
	TestTrue(TEXT("preview does not open or auto-select a choice in the original state"),
		FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(&State, &Before, PPF_None));

	FGameXXKRuntimeState DirectCopy = State;
	FGameXXKCardPlayResult DirectResult;
	if (!TestTrue(TEXT("direct public adapter copy opens the real blocking choice"),
		FGameXXKCardBattleAdapter::ResolveCardPlay(
			DirectCopy, PrimaryCardInstanceId, NAME_None, DirectResult, &Error)))
	{
		AddError(Error);
		return false;
	}
	TestTrue(TEXT("direct result reports an unresolved pending choice"), DirectResult.bOpenedPendingChoice);
	TestEqual(TEXT("real pending choice remains ForcedDiscard"),
		DirectCopy.CardRun.ActiveBattle.Deck.PendingChoice.Kind,
		EGameXXKCardPendingChoiceKind::ForcedDiscard);
	TestTrue(TEXT("real pending choice exposes candidates instead of auto-selecting one"),
		!DirectCopy.CardRun.ActiveBattle.Deck.PendingChoice.Candidates.IsEmpty());
	FGameXXKRuntimeState SecondDirectCopy = State;
	FGameXXKCardPlayResult SecondDirectResult;
	if (!TestTrue(TEXT("second direct copy opens the same unresolved choice"),
		FGameXXKCardBattleAdapter::ResolveCardPlay(
			SecondDirectCopy, PrimaryCardInstanceId, NAME_None, SecondDirectResult, &Error)))
	{
		AddError(Error);
		return false;
	}
	TestTrue(TEXT("pending deck, hand, and candidate order match a second direct adapter copy"),
		FGameXXKBattleDeckState::StaticStruct()->CompareScriptStruct(
			&DirectCopy.CardRun.ActiveBattle.Deck,
			&SecondDirectCopy.CardRun.ActiveBattle.Deck,
			PPF_None));
	TestTrue(TEXT("pending preview leaves the original hand and complete state untouched"),
		FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(&State, &Before, PPF_None));
	return true;
}

#endif
