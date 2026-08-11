#include "GameXXKRouteBalanceRules.h"

#include "GameXXKCardBattleAdapter.h"
#include "GameXXKCombatSimulationRules.h"
#include "GameXXKCompanionRules.h"
#include "GameXXKEncounterRules.h"
#include "GameXXKEnemyCatalog.h"
#include "GameXXKEquipmentEconomyRules.h"
#include "GameXXKEquipmentRules.h"
#include "HAL/PlatformTime.h"

namespace
{
	FGameXXKRouteBalanceCohort MakeCohort(
		const TCHAR* CohortId,
		const TCHAR* QuestNpcId,
		const TCHAR* EquipmentSetId,
		const TCHAR* EquipmentQualityId,
		const int32 EnhancementLevel)
	{
		FGameXXKRouteBalanceCohort Cohort;
		Cohort.CohortId = FName(CohortId);
		Cohort.QuestNpcId = FName(QuestNpcId);
		Cohort.EquipmentSetId = FName(EquipmentSetId);
		Cohort.EquipmentQualityId = FName(EquipmentQualityId);
		Cohort.EnhancementLevel = EnhancementLevel;
		return Cohort;
	}

	bool SetError(FString* OutError, const FString& Error)
	{
		if (OutError)
		{
			*OutError = Error;
		}
		return false;
	}

	bool IsLockedNodeKinds(const TArray<EGameXXKNodeKind>& NodeKinds)
	{
		return NodeKinds == TArray<EGameXXKNodeKind>{
			EGameXXKNodeKind::Battle,
			EGameXXKNodeKind::Elite,
			EGameXXKNodeKind::Boss};
	}

	bool ResolveEquipmentSet(const FName Id, EGameXXKEquipmentSet& OutSet)
	{
		if (Id == TEXT("PoJun")) { OutSet = EGameXXKEquipmentSet::PoJun; return true; }
		if (Id == TEXT("XuanJia")) { OutSet = EGameXXKEquipmentSet::XuanJia; return true; }
		if (Id == TEXT("QingNang")) { OutSet = EGameXXKEquipmentSet::QingNang; return true; }
		if (Id == TEXT("ZhuiFeng")) { OutSet = EGameXXKEquipmentSet::ZhuiFeng; return true; }
		if (Id == TEXT("ShiGu")) { OutSet = EGameXXKEquipmentSet::ShiGu; return true; }
		if (Id == TEXT("ShanHe")) { OutSet = EGameXXKEquipmentSet::ShanHe; return true; }
		return false;
	}

	bool ResolveEquipmentQuality(const FName Id, EGameXXKEquipmentQuality& OutQuality)
	{
		if (Id == TEXT("Common")) { OutQuality = EGameXXKEquipmentQuality::Common; return true; }
		if (Id == TEXT("Rare")) { OutQuality = EGameXXKEquipmentQuality::Rare; return true; }
		if (Id == TEXT("Epic")) { OutQuality = EGameXXKEquipmentQuality::Epic; return true; }
		return false;
	}

	bool ApplyHeroEquipmentFixture(
		FGameXXKRuntimeState& InOutState,
		const FGameXXKRouteBalanceCase& Case,
		FString* OutError)
	{
		if (Case.EquipmentSetId == TEXT("None"))
		{
			return true;
		}

		EGameXXKEquipmentSet Set = EGameXXKEquipmentSet::Invalid;
		EGameXXKEquipmentQuality Quality = EGameXXKEquipmentQuality::Invalid;
		if (!ResolveEquipmentSet(Case.EquipmentSetId, Set) || !ResolveEquipmentQuality(Case.EquipmentQualityId, Quality))
		{
			return SetError(OutError, TEXT("The balance fixture has an unknown equipment set or quality."));
		}

		InOutState.Inventory.FindOrAdd(UGameXXKMVPRules::ItemEnhancementStone()) = 1000000;
		constexpr EGameXXKEquipmentSlot Slots[] = {
			EGameXXKEquipmentSlot::Weapon,
			EGameXXKEquipmentSlot::Head,
			EGameXXKEquipmentSlot::Armor,
			EGameXXKEquipmentSlot::Belt,
			EGameXXKEquipmentSlot::Shoes,
			EGameXXKEquipmentSlot::Accessory};
		for (const EGameXXKEquipmentSlot Slot : Slots)
		{
			FGameXXKEquipmentCreateRequest Request;
			Request.Set = Set;
			Request.Quality = Quality;
			Request.ItemLevel = Case.RouteLevel;
			Request.bForceSlot = true;
			Request.ForcedSlot = Slot;
			FName InstanceId;
			if (!FGameXXKEquipmentRules::CreateRolledInstance(InOutState.EquipmentCollection, Request, InstanceId, OutError))
			{
				return false;
			}
			for (int32 Index = 0; Index < Case.EnhancementLevel; ++Index)
			{
				FGameXXKEquipmentTransactionResult Enhancement;
				if (!FGameXXKEquipmentEconomyRules::EnhanceInstance(InOutState, InstanceId, Enhancement))
				{
					return SetError(OutError, Enhancement.Message.ToString());
				}
			}
			FGameXXKEquipmentTransactionResult Equip;
			if (!FGameXXKEquipmentEconomyRules::Equip(
				InOutState,
				FGameXXKEquipmentRules::HeroCharacterId(),
				Slot,
				InstanceId,
				Equip))
			{
				return SetError(OutError, Equip.Message.ToString());
			}
		}
		return true;
	}

	bool AddDeterministicStarterCompanion(FGameXXKRuntimeState& InOutState, const FGameXXKRouteBalanceCase& Case, FString* OutError)
	{
		FGameXXKCompanionRosterState& Roster = InOutState.CardRun.CompanionRoster;
		Roster.RecruitSequenceSeed = Case.Seed ^ 0x3A7D9E1;
		FGameXXKCompanionRecruitResult Recruit;
		if (!FGameXXKCompanionRules::CreateAndResolveNextRecruitment(Roster, Recruit, OutError)
			|| Recruit.Outcome != EGameXXKCompanionRecruitOutcome::Recruited
			|| !FGameXXKCompanionRules::SetActivePermanentCompanion(Roster, Recruit.Companion.InstanceId, OutError))
		{
			return false;
		}
		FGameXXKPermanentCompanion* Companion = Roster.PermanentCompanions.FindByPredicate([&Recruit](const FGameXXKPermanentCompanion& Candidate)
		{
			return Candidate.InstanceId == Recruit.Companion.InstanceId;
		});
		if (!Companion)
		{
			return SetError(OutError, TEXT("The deterministic companion recruit was not retained in the roster."));
		}
		Companion->Level = Case.RouteLevel;
		if (!FGameXXKCompanionRules::RefreshUnlockedPersonalCards(*Companion, OutError))
		{
			return false;
		}
		return FGameXXKCardBattleAdapter::EnsureCardRunInitialized(InOutState, OutError);
	}

	bool AddRequestedStarterCompanion(
		FGameXXKRuntimeState& InOutState,
		const FGameXXKRouteBalanceCase& Case,
		FString* OutError)
	{
		if (Case.CompanionTemplateId.IsNone())
		{
			if (Case.CompanionCardSeed != INDEX_NONE)
			{
				return SetError(OutError, TEXT("A diagnostic companion card seed requires an explicit template."));
			}
			return AddDeterministicStarterCompanion(InOutState, Case, OutError);
		}
		if (Case.CompanionCardSeed <= 0)
		{
			return SetError(OutError, TEXT("An explicit diagnostic companion requires a positive card seed."));
		}

		FGameXXKCompanionRosterState& Roster = InOutState.CardRun.CompanionRoster;
		FGameXXKCompanionRecruitResult Recruit;
		if (!FGameXXKCompanionRules::RecruitPermanentCompanion(
			Roster,
			Case.CompanionTemplateId,
			Case.CompanionCardSeed,
			Recruit,
			OutError)
			|| Recruit.Outcome != EGameXXKCompanionRecruitOutcome::Recruited
			|| !FGameXXKCompanionRules::SetActivePermanentCompanion(Roster, Recruit.Companion.InstanceId, OutError))
		{
			return false;
		}

		FGameXXKPermanentCompanion* Companion = Roster.PermanentCompanions.FindByPredicate(
			[&Recruit](const FGameXXKPermanentCompanion& Candidate)
			{
				return Candidate.InstanceId == Recruit.Companion.InstanceId;
			});
		if (!Companion)
		{
			return SetError(OutError, TEXT("The explicit diagnostic companion was not retained in the roster."));
		}
		Companion->Level = Case.RouteLevel;
		if (!FGameXXKCompanionRules::RefreshUnlockedPersonalCards(*Companion, OutError))
		{
			return false;
		}
		return FGameXXKCardBattleAdapter::EnsureCardRunInitialized(InOutState, OutError);
	}

	bool IsConcreteBalanceTerrain(const EGameXXKCardTerrain Terrain)
	{
		return Terrain == EGameXXKCardTerrain::Plain
			|| Terrain == EGameXXKCardTerrain::Cliff
			|| Terrain == EGameXXKCardTerrain::Forest
			|| Terrain == EGameXXKCardTerrain::WaterShore
			|| Terrain == EGameXXKCardTerrain::Ferry
			|| Terrain == EGameXXKCardTerrain::Village
			|| Terrain == EGameXXKCardTerrain::Cave;
	}

	void AppendOrthogonalDimension(
		TArray<FGameXXKRouteBalanceCase>& OutCases,
		const FName DimensionId,
		const TArray<FName>& VariantIds,
		const int32 SeedBase,
		TFunctionRef<void(FName, FGameXXKRouteBalanceCase&)> ConfigureVariant)
	{
		const EGameXXKNodeKind NodeKinds[] = {
			EGameXXKNodeKind::Battle,
			EGameXXKNodeKind::Elite,
			EGameXXKNodeKind::Boss};
		for (int32 NodeIndex = 0; NodeIndex < UE_ARRAY_COUNT(NodeKinds); ++NodeIndex)
		{
			for (int32 SeedOrdinal = 0; SeedOrdinal < 30; ++SeedOrdinal)
			{
				const int32 Seed = SeedBase + NodeIndex * 100 + SeedOrdinal;
				for (const FName VariantId : VariantIds)
				{
					FGameXXKRouteBalanceCase& Case = OutCases.Emplace_GetRef();
					Case.DimensionId = DimensionId;
					Case.VariantId = VariantId;
					Case.CohortId = FName(*FString::Printf(TEXT("Orthogonal.%s"), *DimensionId.ToString()));
					Case.QuestNpcId = TEXT("Npc.TusiChief");
					Case.EquipmentSetId = TEXT("None");
					Case.EquipmentQualityId = TEXT("Rare");
					Case.CompanionTemplateId = TEXT("Companion.Blade.01");
					Case.CompanionCardSeed = Seed ^ 0x13579B;
					Case.Terrain = EGameXXKCardTerrain::Plain;
					Case.NodeKind = NodeKinds[NodeIndex];
					Case.EnhancementLevel = 0;
					Case.Chapter = 2;
					Case.RouteLevel = 10;
					Case.SeedOrdinal = SeedOrdinal;
					Case.Seed = Seed;
					ConfigureVariant(VariantId, Case);
				}
			}
		}
	}

	FName MakeRuntimeEnemyId(const FName DefinitionId, const int32 Slot)
	{
		return FName(*FString::Printf(TEXT("Balance.%s.P%d"), *DefinitionId.ToString(), Slot));
	}

	FGameXXKRouteBalanceStatScale ResolveEncounterScale(
		const FGameXXKRouteBalanceCase& Case,
		const FGameXXKRouteBalanceCalibrationProfile* CalibrationProfile)
	{
		if (CalibrationProfile)
		{
			const int32 Key = FGameXXKRouteBalanceCalibrationProfile::MakeEncounterKey(Case.Chapter, Case.NodeKind);
			if (const FGameXXKRouteBalanceStatScale* Scale = CalibrationProfile->EncounterScales.Find(Key))
			{
				return *Scale;
			}
		}
		const FGameXXKEncounterStatScale AuthoredScale = FGameXXKEncounterRules::GetAuthoredStatScale(Case.Chapter, Case.NodeKind);
		return FGameXXKRouteBalanceStatScale{
			AuthoredScale.MaxHPPercent,
			AuthoredScale.AttackPercent,
			AuthoredScale.DefensePercent};
	}

	bool BuildEncounterProjection(
		FGameXXKRuntimeState& InOutState,
		const FGameXXKRouteBalanceCase& Case,
		const FGameXXKRouteBalanceCalibrationProfile* CalibrationProfile,
		TArray<FGameXXKRouteBalanceInitialEnemy>* OutInitialEnemies,
		FString* OutError)
	{
		TArray<FGameXXKEncounterSlot> Slots;
		const int32 ChapterSeed = FGameXXKEncounterRules::DeriveChapterSeed(Case.Seed, Case.Chapter);
		if (!FGameXXKEncounterRules::BuildFormation(Case.Chapter, Case.NodeKind, ChapterSeed, Case.Seed, Case.RouteLevel, Slots, OutError))
		{
			return false;
		}
		const FGameXXKRouteBalanceStatScale Scale = ResolveEncounterScale(Case, CalibrationProfile);
		TArray<FGameXXKBattleRuntimeUnit> Enemies;
		Enemies.Reserve(Slots.Num());
		for (const FGameXXKEncounterSlot& Slot : Slots)
		{
			const FGameXXKEnemyDefinition* Definition = FGameXXKEnemyCatalog::Find(Slot.EnemyDefinitionId);
			if (!Definition)
			{
				return SetError(OutError, TEXT("An encounter slot references an unknown enemy definition."));
			}
			const FGameXXKEnemyComputedStats Stats = FGameXXKEnemyCatalog::ComputeStats(Definition->Id, Slot.CombatLevel);
			const int32 ScaledMaxHP = FGameXXKEncounterRules::ScaleStat(Stats.MaxHP, Scale.MaxHPPercent, 1);
			const int32 ScaledAttack = FGameXXKEncounterRules::ScaleStat(Stats.Attack, Scale.AttackPercent, 1);
			const int32 ScaledDefense = FGameXXKEncounterRules::ScaleStat(Stats.Defense, Scale.DefensePercent, 0);
			FGameXXKBattleRuntimeUnit& Enemy = Enemies.Emplace_GetRef();
			Enemy.Id = MakeRuntimeEnemyId(Definition->Id, Slot.BattleSlotNumber);
			Enemy.DisplayName = Definition->DisplayName;
			Enemy.HP = ScaledMaxHP;
			Enemy.MaxHP = ScaledMaxHP;
			Enemy.MP = 0;
			Enemy.MaxMP = 0;
			Enemy.Attack = ScaledAttack;
			Enemy.Defense = ScaledDefense;
			Enemy.Speed = Stats.Speed;
			Enemy.EnemyDefinitionId = Definition->Id;
			Enemy.BattleSlotNumber = Slot.BattleSlotNumber;
			Enemy.CombatLevel = Slot.CombatLevel;
			Enemy.bEnemy = true;
			if (OutInitialEnemies)
			{
				FGameXXKRouteBalanceInitialEnemy& InitialEnemy = OutInitialEnemies->Emplace_GetRef();
				InitialEnemy.DefinitionId = Definition->Id;
				InitialEnemy.BattleSlotNumber = Slot.BattleSlotNumber;
				InitialEnemy.CombatLevel = Slot.CombatLevel;
				InitialEnemy.MaxHP = ScaledMaxHP;
				InitialEnemy.Attack = ScaledAttack;
				InitialEnemy.Defense = ScaledDefense;
			}
		}
		InOutState.bHasActiveBattle = true;
		InOutState.ActiveBattleNodeId = Case.Seed;
		InOutState.ActiveBattleEnemies = MoveTemp(Enemies);
		return true;
	}

	bool BuildScenario(
		const FGameXXKRouteBalanceCase& Case,
		FGameXXKSimulationScenario& OutScenario,
		const FGameXXKRouteBalanceCalibrationProfile* CalibrationProfile,
		TArray<FGameXXKRouteBalanceInitialEnemy>* OutInitialEnemies,
		FString* OutError)
	{
		if (Case.Chapter < 1 || Case.Chapter > 3 || Case.RouteLevel < 1 || Case.RouteLevel > 20 || Case.Seed <= 0)
		{
			return SetError(OutError, TEXT("A balance case needs a valid chapter, route snapshot level, and positive seed."));
		}
		FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
		State.PlayerLevel = Case.RouteLevel;
		State.RouteSeed = Case.Seed;
		UGameXXKMVPRules::RecalculatePlayerStatsFromEquipment(State);
		FGameXXKRouteProgress& Progress = State.CardRun.RouteProgress;
		Progress.SchemaVersion = 1;
		Progress.RootSeed = Case.Seed;
		Progress.CurrentChapter = Case.Chapter;
		Progress.RouteCombatLevel = Case.RouteLevel;
		Progress.ChapterSeeds = {
			FGameXXKEncounterRules::DeriveChapterSeed(Case.Seed, 1),
			FGameXXKEncounterRules::DeriveChapterSeed(Case.Seed, 2),
			FGameXXKEncounterRules::DeriveChapterSeed(Case.Seed, 3)};
		if (!FGameXXKCardBattleAdapter::EnsureCardRunInitialized(State, OutError)
			|| !AddRequestedStarterCompanion(State, Case, OutError)
			|| !FGameXXKCardBattleAdapter::SetQuestNpcForCurrentRun(State, Case.QuestNpcId, {}, OutError)
			|| !ApplyHeroEquipmentFixture(State, Case, OutError)
			|| !FGameXXKEquipmentEconomyRules::SynchronizeRuntimeMirrors(State)
			|| !BuildEncounterProjection(State, Case, CalibrationProfile, OutInitialEnemies, OutError))
		{
			return false;
		}
		OutScenario = FGameXXKSimulationScenario();
		OutScenario.Seed = Case.Seed;
		OutScenario.InitialRuntimeState = MoveTemp(State);
		OutScenario.NodeKind = Case.NodeKind;
		OutScenario.Terrain = Case.Terrain == EGameXXKCardTerrain::Invalid
			? EGameXXKCardTerrain::Plain
			: Case.Terrain;
		if (!IsConcreteBalanceTerrain(OutScenario.Terrain))
		{
			return SetError(OutError, TEXT("A balance case requests an invalid starting terrain."));
		}
		OutScenario.Policy = EGameXXKSimulationPolicy::Skilled;
		OutScenario.MaxRounds = 100;
		OutScenario.MaxDecisions = 2000;
		return true;
	}

	void BuildAggregates(FGameXXKRouteBalanceReport& InOutReport)
	{
		InOutReport.Aggregates.Reset();
		for (int32 Chapter = 1; Chapter <= 3; ++Chapter)
		{
			for (const EGameXXKNodeKind NodeKind : {
				EGameXXKNodeKind::Battle,
				EGameXXKNodeKind::Elite,
				EGameXXKNodeKind::Boss})
			{
				FGameXXKRouteBalanceAggregate& Aggregate = InOutReport.Aggregates.Emplace_GetRef();
				Aggregate.Chapter = Chapter;
				Aggregate.NodeKind = NodeKind;
				for (const FGameXXKRouteBalanceCaseResult& Result : InOutReport.Results)
				{
					if (Result.Case.Chapter == Chapter && Result.Case.NodeKind == NodeKind)
					{
						++Aggregate.CaseCount;
						Aggregate.VictoryCount += Result.Metrics.bVictory ? 1 : 0;
					}
				}
				Aggregate.VictoryRate = Aggregate.CaseCount > 0
					? static_cast<double>(Aggregate.VictoryCount) / static_cast<double>(Aggregate.CaseCount)
					: 0.0;
			}
		}
	}
}

FGameXXKRouteBalanceMatrix FGameXXKRouteBalanceRules::MakeLockedFullMatrix()
{
	FGameXXKRouteBalanceMatrix Matrix;
	Matrix.SchemaVersion = 1;
	Matrix.SeedCount = 100;
	Matrix.NodeKinds = {
		EGameXXKNodeKind::Battle,
		EGameXXKNodeKind::Elite,
		EGameXXKNodeKind::Boss};
	Matrix.RouteLevelsByChapter = {{1, 5}, {2, 10}, {3, 15}};
	Matrix.Cohorts = {
		MakeCohort(TEXT("NakedBaseline"), TEXT("Npc.TusiChief"), TEXT("None"), TEXT("Common"), 0),
		MakeCohort(TEXT("PoJunSong"), TEXT("Npc.SongJinBao"), TEXT("PoJun"), TEXT("Rare"), 0),
		MakeCohort(TEXT("XuanJiaYueBai"), TEXT("Npc.YueBai"), TEXT("XuanJia"), TEXT("Rare"), 0),
		MakeCohort(TEXT("QingNangZhou"), TEXT("Npc.ZhouGuangZu"), TEXT("QingNang"), TEXT("Rare"), 0),
		MakeCohort(TEXT("ZhuiFengJinGui"), TEXT("Npc.JinGui"), TEXT("ZhuiFeng"), TEXT("Rare"), 0),
		MakeCohort(TEXT("ShiGuQiong"), TEXT("Npc.QiongMeiEr"), TEXT("ShiGu"), TEXT("Rare"), 0),
		MakeCohort(TEXT("ShanHeTusi"), TEXT("Npc.TusiChief"), TEXT("ShanHe"), TEXT("Epic"), 5),
		MakeCohort(TEXT("MixedMaxRegression"), TEXT("Npc.SongJinBao"), TEXT("PoJun"), TEXT("Epic"), 10)};
	return Matrix;
}

bool FGameXXKRouteBalanceRules::MakeOrthogonalCases(
	TArray<FGameXXKRouteBalanceCase>& OutCases,
	FString* OutError)
{
	TArray<FGameXXKRouteBalanceCase> Cases;
	Cases.Reserve(2520);

	AppendOrthogonalDimension(
		Cases,
		TEXT("Profession"),
		{TEXT("Blade"), TEXT("Guard"), TEXT("Healer"), TEXT("Hunter"), TEXT("Sorcerer"), TEXT("FormationMaster")},
		1100000,
		[](const FName VariantId, FGameXXKRouteBalanceCase& Case)
		{
			Case.CompanionTemplateId = FName(*FString::Printf(TEXT("Companion.%s.01"), *VariantId.ToString()));
		});

	AppendOrthogonalDimension(
		Cases,
		TEXT("EquipmentSet"),
		{TEXT("NoSet"), TEXT("PoJun"), TEXT("XuanJia"), TEXT("QingNang"), TEXT("ZhuiFeng"), TEXT("ShiGu"), TEXT("ShanHe")},
		1200000,
		[](const FName VariantId, FGameXXKRouteBalanceCase& Case)
		{
			Case.EquipmentSetId = VariantId == TEXT("NoSet") ? FName(TEXT("None")) : VariantId;
		});

	AppendOrthogonalDimension(
		Cases,
		TEXT("QuestNpc"),
		{TEXT("TusiChief"), TEXT("SongJinBao"), TEXT("YueBai"), TEXT("ZhouGuangZu"), TEXT("JinGui"), TEXT("QiongMeiEr")},
		1300000,
		[](const FName VariantId, FGameXXKRouteBalanceCase& Case)
		{
			Case.QuestNpcId = FName(*FString::Printf(TEXT("Npc.%s"), *VariantId.ToString()));
		});

	AppendOrthogonalDimension(
		Cases,
		TEXT("Terrain"),
		{TEXT("Plain"), TEXT("Cliff"), TEXT("Forest"), TEXT("WaterShore"), TEXT("Village"), TEXT("Cave")},
		1400000,
		[](const FName VariantId, FGameXXKRouteBalanceCase& Case)
		{
			Case.CompanionTemplateId = TEXT("Companion.FormationMaster.01");
			if (VariantId == TEXT("Plain")) { Case.Terrain = EGameXXKCardTerrain::Plain; }
			else if (VariantId == TEXT("Cliff")) { Case.Terrain = EGameXXKCardTerrain::Cliff; }
			else if (VariantId == TEXT("Forest")) { Case.Terrain = EGameXXKCardTerrain::Forest; }
			else if (VariantId == TEXT("WaterShore")) { Case.Terrain = EGameXXKCardTerrain::WaterShore; }
			else if (VariantId == TEXT("Village")) { Case.Terrain = EGameXXKCardTerrain::Village; }
			else if (VariantId == TEXT("Cave")) { Case.Terrain = EGameXXKCardTerrain::Cave; }
		});

	AppendOrthogonalDimension(
		Cases,
		TEXT("Progression"),
		{TEXT("Early"), TEXT("Mid"), TEXT("Late")},
		1500000,
		[](const FName VariantId, FGameXXKRouteBalanceCase& Case)
		{
			if (VariantId == TEXT("Early"))
			{
				Case.Chapter = 1;
				Case.RouteLevel = 5;
			}
			else if (VariantId == TEXT("Late"))
			{
				Case.Chapter = 3;
				Case.RouteLevel = 15;
			}
		});

	if (Cases.Num() != 2520)
	{
		return SetError(OutError, TEXT("The orthogonal balance matrix did not produce exactly 2,520 cases."));
	}
	OutCases = MoveTemp(Cases);
	return true;
}

bool FGameXXKRouteBalanceRules::ExpandCases(
	const FGameXXKRouteBalanceMatrix& Matrix,
	TArray<FGameXXKRouteBalanceCase>& OutCases,
	FString* OutError)
{
	if (Matrix.SchemaVersion != 1 || Matrix.SeedCount != 100)
	{
		return SetError(OutError, TEXT("The locked full balance matrix must use schema 1 and exactly 100 seeds."));
	}
	if (!IsLockedNodeKinds(Matrix.NodeKinds) || Matrix.Cohorts.Num() != 8)
	{
		return SetError(OutError, TEXT("The locked full balance matrix requires eight cohorts and Battle/Elite/Boss."));
	}

	TArray<FGameXXKRouteBalanceCase> PendingCases;
	PendingCases.Reserve(Matrix.Cohorts.Num() * Matrix.NodeKinds.Num() * Matrix.SeedCount);
	for (int32 CohortIndex = 0; CohortIndex < Matrix.Cohorts.Num(); ++CohortIndex)
	{
		const FGameXXKRouteBalanceCohort& Cohort = Matrix.Cohorts[CohortIndex];
		if (Cohort.CohortId.IsNone() || Cohort.QuestNpcId.IsNone())
		{
			return SetError(OutError, TEXT("Every locked balance cohort needs an identity and a task NPC."));
		}
		for (int32 NodeIndex = 0; NodeIndex < Matrix.NodeKinds.Num(); ++NodeIndex)
		{
			for (int32 SeedOrdinal = 0; SeedOrdinal < Matrix.SeedCount; ++SeedOrdinal)
			{
				const int32 Chapter = 1 + ((SeedOrdinal + CohortIndex) % 3);
				const int32* RouteLevel = Matrix.RouteLevelsByChapter.Find(Chapter);
				if (!RouteLevel)
				{
					return SetError(OutError, TEXT("The locked balance matrix is missing a chapter route level."));
				}
				FGameXXKRouteBalanceCase& Case = PendingCases.Emplace_GetRef();
				Case.CohortId = Cohort.CohortId;
				Case.QuestNpcId = Cohort.QuestNpcId;
				Case.EquipmentSetId = Cohort.EquipmentSetId;
				Case.EquipmentQualityId = Cohort.EquipmentQualityId;
				Case.EnhancementLevel = Cohort.EnhancementLevel;
				Case.NodeKind = Matrix.NodeKinds[NodeIndex];
				Case.Chapter = Chapter;
				Case.RouteLevel = *RouteLevel;
				Case.SeedOrdinal = SeedOrdinal;
				Case.Seed = 900000 + CohortIndex * 10000 + NodeIndex * 1000 + SeedOrdinal;
			}
		}
	}

	OutCases = MoveTemp(PendingCases);
	return true;
}

bool FGameXXKRouteBalanceRules::RunCase(
	const FGameXXKRouteBalanceCase& Case,
	FGameXXKRouteBalanceCaseResult& OutResult,
	FString* OutError,
	const FGameXXKRouteBalanceCalibrationProfile* CalibrationProfile,
	TArray<FGameXXKSimulationTraceEntry>* OutTrace)
{
	FGameXXKRouteBalanceCaseResult PendingResult;
	PendingResult.Case = Case;
	FGameXXKSimulationScenario Scenario;
	if (!BuildScenario(Case, Scenario, CalibrationProfile, &PendingResult.InitialEnemies, OutError))
	{
		return false;
	}
	TArray<FGameXXKSimulationTraceEntry> Trace;
	if (!FGameXXKCombatSimulationRules::RunScenario(Scenario, PendingResult.Metrics, Trace, OutError))
	{
		if (OutError)
		{
			*OutError = FString::Printf(
				TEXT("reason=%s cohort=%s chapter=%d node=%d seed=%d: %s"),
				*PendingResult.Metrics.FailureReason.ToString(),
				*Case.CohortId.ToString(),
				Case.Chapter,
				static_cast<int32>(Case.NodeKind),
				Case.Seed,
				**OutError);
		}
		if (OutTrace)
		{
			*OutTrace = MoveTemp(Trace);
		}
		OutResult = MoveTemp(PendingResult);
		return false;
	}
	if (OutTrace)
	{
		*OutTrace = MoveTemp(Trace);
	}
	OutResult = MoveTemp(PendingResult);
	return true;
}

bool FGameXXKRouteBalanceRules::RunFullMatrix(
	const FGameXXKRouteBalanceMatrix& Matrix,
	FGameXXKRouteBalanceReport& OutReport,
	FString* OutError)
{
	TArray<FGameXXKRouteBalanceCase> Cases;
	if (!ExpandCases(Matrix, Cases, OutError))
	{
		return false;
	}

	FGameXXKRouteBalanceReport PendingReport;
	PendingReport.Results.Reserve(Cases.Num());
	const double StartSeconds = FPlatformTime::Seconds();
	for (const FGameXXKRouteBalanceCase& Case : Cases)
	{
		FGameXXKRouteBalanceCaseResult Result;
		if (!RunCase(Case, Result, OutError))
		{
			return false;
		}
		PendingReport.Results.Add(MoveTemp(Result));
	}
	PendingReport.ElapsedSeconds = FPlatformTime::Seconds() - StartSeconds;
	BuildAggregates(PendingReport);
	OutReport = MoveTemp(PendingReport);
	return true;
}
