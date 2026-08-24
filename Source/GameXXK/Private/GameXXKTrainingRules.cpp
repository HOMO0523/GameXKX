#include "GameXXKTrainingRules.h"

#include "GameXXKEnemyCatalog.h"

namespace
{
	FText EnemyName(const FName Id)
	{
		if (const FGameXXKEnemyDefinition* Definition = FGameXXKEnemyCatalog::Find(Id))
		{
			return Definition->DisplayName;
		}
		return FText::FromName(Id);
	}

	FString JoinNames(const TArray<FName>& Ids)
	{
		FString Result;
		for (int32 Index = 0; Index < Ids.Num(); ++Index)
		{
			if (Index > 0)
			{
				Result += TEXT("、");
			}
			Result += EnemyName(Ids[Index]).ToString();
		}
		return Result;
	}

	int32 DifficultyIndex(const EGameXXKTrainingDifficulty Difficulty)
	{
		return static_cast<int32>(Difficulty);
	}

	EGameXXKTrainingDifficulty DifficultyFromIndex(const int32 Index)
	{
		return static_cast<EGameXXKTrainingDifficulty>(FMath::Clamp(Index, 0, 2));
	}

	const TCHAR* DifficultyLabel(const EGameXXKTrainingDifficulty Difficulty)
	{
		switch (Difficulty)
		{
		case EGameXXKTrainingDifficulty::Hard: return TEXT("Hard");
		case EGameXXKTrainingDifficulty::Hell: return TEXT("Hell");
		default: return TEXT("Normal");
		}
	}

	FName BossForChapter(const int32 Chapter, const int32 StageNumber)
	{
		if (Chapter == 1)
		{
			// Training deliberately reuses the existing first-chapter pool while
			// assigning the user-frozen identities to the three stage bosses.
			if (StageNumber == 1) return TEXT("Enemy.Ch1.Goat");
			if (StageNumber == 2) return TEXT("Enemy.Ch1.Weasel");
			return TEXT("Enemy.Ch1.BluehornGoatKing");
		}
		if (Chapter == 2)
		{
			return StageNumber == 3 ? TEXT("Enemy.Ch2.BlackBear") : TEXT("Enemy.Ch2.RedtuskBoarKing");
		}
		return StageNumber == 3 ? TEXT("Enemy.Ch3.Tiger") : TEXT("Enemy.Ch3.SpiralHornDeer");
	}

	TArray<FName> NormalPoolForChapter(const int32 Chapter)
	{
		return FGameXXKEnemyCatalog::GetPool(Chapter, EGameXXKEnemyTier::Normal);
	}

	TArray<FName> ElitePoolForChapter(const int32 Chapter)
	{
		return FGameXXKEnemyCatalog::GetPool(Chapter, EGameXXKEnemyTier::Elite);
	}

	TArray<FGameXXKTrainingStageDefinition> BuildStages()
	{
		TArray<FGameXXKTrainingStageDefinition> Stages;
		Stages.Reserve(27);
		for (int32 DifficultyIndexValue = 0; DifficultyIndexValue < 3; ++DifficultyIndexValue)
		{
			const EGameXXKTrainingDifficulty Difficulty = DifficultyFromIndex(DifficultyIndexValue);
			for (int32 StageNumber = 1; StageNumber <= FGameXXKTrainingRules::StagesPerDifficulty; ++StageNumber)
			{
				FGameXXKTrainingStageDefinition Stage;
				Stage.Difficulty = Difficulty;
				Stage.Chapter = ((StageNumber - 1) / 3) + 1;
				Stage.StageNumber = StageNumber;
				Stage.StageId = FGameXXKTrainingRules::MakeStageId(Difficulty, StageNumber);
				Stage.DisplayName = FText::FromString(FString::Printf(
					TEXT("%s %d-%d"),
					Difficulty == EGameXXKTrainingDifficulty::Normal ? TEXT("普通")
						: Difficulty == EGameXXKTrainingDifficulty::Hard ? TEXT("困难") : TEXT("地狱"),
					Stage.Chapter,
					((StageNumber - 1) % 3) + 1));
				if (Stage.Chapter == 1)
				{
					// Four first-chapter enemy identities are split into two ordinary
					// candidates and two sub-elites. The ordinary encounters repeat
					// those two candidates; the elite slots stay deterministic.
					Stage.NormalEnemyPool = {FName(TEXT("Enemy.Ch1.Rooster")), FName(TEXT("Enemy.Ch1.Civet"))};
					Stage.EliteEnemyPool = {FName(TEXT("Enemy.Ch1.Goat")), FName(TEXT("Enemy.Ch1.Weasel"))};
				}
				else
				{
					Stage.NormalEnemyPool = NormalPoolForChapter(Stage.Chapter);
					Stage.EliteEnemyPool = ElitePoolForChapter(Stage.Chapter);
				}
				Stage.BossEnemyId = BossForChapter(Stage.Chapter, ((StageNumber - 1) % 3) + 1);
				Stage.BossDisplayName = EnemyName(Stage.BossEnemyId);
				const int32 TierScale = DifficultyIndexValue + 1;
				Stage.TravelGold = 18 * TierScale + StageNumber * 3;
				Stage.TravelExperience = 10 * TierScale + StageNumber * 2;
				Stage.NormalChestChance = 0.25f;
				Stage.AdvancedChestChance = 0.35f;
				Stages.Add(MoveTemp(Stage));
			}
		}
		return Stages;
	}

	int32 TravelEnemyAttack(const EGameXXKTrainingEncounterKind Kind)
	{
		switch (Kind)
		{
		case EGameXXKTrainingEncounterKind::Elite:
			return 2;
		case EGameXXKTrainingEncounterKind::Boss:
			return 3;
		default:
			return 1;
		}
	}

	void AppendFormationEnemy(TArray<FName>& Formation, const FName EnemyId)
	{
		if (!EnemyId.IsNone() && Formation.Num() < 3)
		{
			Formation.Add(EnemyId);
		}
	}

	TArray<FName> BuildNormalFormation(const FGameXXKTrainingStageDefinition& Stage)
	{
		TArray<FName> Formation;
		if (Stage.NormalEnemyPool.Num() > 0)
		{
			AppendFormationEnemy(Formation, Stage.NormalEnemyPool[0]);
		}
		if (Stage.NormalEnemyPool.Num() > 1)
		{
			AppendFormationEnemy(Formation, Stage.NormalEnemyPool[1]);
		}
		return Formation;
	}

	TArray<FName> BuildCoreFormation(
		const TArray<FName>& FlankPool,
		const FName CoreEnemyId)
	{
		TArray<FName> Formation;
		if (FlankPool.Num() > 0)
		{
			AppendFormationEnemy(Formation, FlankPool[0]);
		}
		AppendFormationEnemy(Formation, CoreEnemyId);
		if (FlankPool.Num() > 1)
		{
			AppendFormationEnemy(Formation, FlankPool[1]);
		}
		return Formation;
	}

	int32 FindNextLivingTravelEnemy(
		const FGameXXKTrainingTravelRuntime& Runtime,
		const int32 FirstIndex)
	{
		for (int32 EnemyIndex = FMath::Max(0, FirstIndex); EnemyIndex < Runtime.Enemies.Num(); ++EnemyIndex)
		{
			if (Runtime.Enemies[EnemyIndex].HP > 0)
			{
				return EnemyIndex;
			}
		}
		return INDEX_NONE;
	}

	int32 FindNextLivingTravelPartyUnit(
		const FGameXXKTrainingTravelRuntime& Runtime,
		const int32 FirstIndex)
	{
		if (Runtime.PartyUnits.IsEmpty())
		{
			return INDEX_NONE;
		}
		const int32 SafeFirstIndex = FMath::Max(0, FirstIndex) % Runtime.PartyUnits.Num();
		for (int32 Offset = 0; Offset < Runtime.PartyUnits.Num(); ++Offset)
		{
			const int32 PartyIndex = (SafeFirstIndex + Offset) % Runtime.PartyUnits.Num();
			if (Runtime.PartyUnits[PartyIndex].HP > 0)
			{
				return PartyIndex;
			}
		}
		return INDEX_NONE;
	}

	void SynchronizeTravelPartyCompatibility(FGameXXKTrainingTravelRuntime& Runtime)
	{
		if (Runtime.PartyUnits.IsEmpty())
		{
			Runtime.PlayerHP = 0;
			Runtime.PlayerMaxHP = 0;
			Runtime.PlayerAttack = 0;
			return;
		}
		const FGameXXKTrainingTravelPartyUnitRuntime& Hero = Runtime.PartyUnits[0];
		Runtime.PlayerHP = Hero.HP;
		Runtime.PlayerMaxHP = Hero.MaxHP;
		Runtime.PlayerAttack = Hero.Attack;
	}

	bool SynchronizeActiveTravelEnemy(FGameXXKTrainingTravelRuntime& Runtime)
	{
		if (!Runtime.Enemies.IsValidIndex(Runtime.ActiveEnemyIndex))
		{
			Runtime.EnemyDefinitionId = NAME_None;
			Runtime.EnemyHP = 0;
			Runtime.EnemyMaxHP = 0;
			Runtime.EnemyAttack = 0;
			return false;
		}

		const FGameXXKTrainingTravelEnemyRuntime& Enemy = Runtime.Enemies[Runtime.ActiveEnemyIndex];
		Runtime.EnemyDefinitionId = Enemy.EnemyDefinitionId;
		Runtime.EnemyHP = Enemy.HP;
		Runtime.EnemyMaxHP = Enemy.MaxHP;
		Runtime.EnemyAttack = Enemy.Attack;
		return !Runtime.EnemyDefinitionId.IsNone();
	}

	EGameXXKTrainingRewardTier ChestTierForEncounter(const EGameXXKTrainingEncounterKind Kind)
	{
		return Kind == EGameXXKTrainingEncounterKind::Normal
			? EGameXXKTrainingRewardTier::NormalChest
			: EGameXXKTrainingRewardTier::AdvancedChest;
	}

	FName ChestItemIdForTierInternal(const EGameXXKTrainingRewardTier Tier)
	{
		switch (Tier)
		{
		case EGameXXKTrainingRewardTier::NormalChest:
			return FName(TEXT("Item.TrainingNormalChest"));
		case EGameXXKTrainingRewardTier::AdvancedChest:
			return FName(TEXT("Item.TrainingAdvancedChest"));
		default:
			return NAME_None;
		}
	}

	float ChestChanceForEncounter(
		const FGameXXKTrainingStageDefinition& Stage,
		const EGameXXKTrainingEncounterKind Kind)
	{
		return Kind == EGameXXKTrainingEncounterKind::Normal
			? Stage.NormalChestChance
			: Stage.AdvancedChestChance;
	}

	uint32 MixRewardSeed(uint32 Value)
	{
		Value ^= Value >> 16;
		Value *= 0x7feb352dU;
		Value ^= Value >> 15;
		Value *= 0x846ca68bU;
		Value ^= Value >> 16;
		return Value;
	}

	float RewardRoll(const FName StageId, const EGameXXKTrainingEncounterKind Kind, const int32 RewardSeed)
	{
		const uint32 KindSalt = static_cast<uint32>(static_cast<uint8>(Kind) + 1U) * 0x9e3779b9U;
		const uint32 StageSalt = FCrc::StrCrc32(*StageId.ToString());
		const uint32 Mixed = MixRewardSeed(static_cast<uint32>(RewardSeed) ^ StageSalt ^ KindSalt);
		return static_cast<float>(Mixed & 0x00ffffffU) / 16777216.0f;
	}
}

bool FGameXXKTrainingRules::AppendChestToken(
	FGameXXKTrainingProgress& InOutProgress,
	const EGameXXKTrainingRewardTier Tier,
	const FName SourceStageId,
	const int32 SourceItemLevel,
	FString* OutError)
{
	if (OutError) OutError->Reset();
	FGameXXKTrainingStageDefinition Stage;
	if ((Tier != EGameXXKTrainingRewardTier::NormalChest && Tier != EGameXXKTrainingRewardTier::AdvancedChest)
		|| SourceStageId.IsNone()
		|| !TryGetStageDefinition(SourceStageId, Stage)
		|| InOutProgress.NextChestAcquisitionOrdinal == MAX_int32)
	{
		if (OutError) *OutError = TEXT("Training chest token source is invalid.");
		return false;
	}
	FGameXXKTrainingProgress Candidate = InOutProgress;
	FGameXXKTrainingChestToken Token;
	Token.Tier = Tier;
	Token.SourceStageId = SourceStageId;
	Token.SourceItemLevel = FMath::Clamp(SourceItemLevel, 1, 100);
	Token.AcquisitionOrdinal = ++Candidate.NextChestAcquisitionOrdinal;
	Candidate.OwnedChestTokens.Add(Token);
	if (!ValidateChestTokens(Candidate, OutError)) return false;
	InOutProgress = MoveTemp(Candidate);
	return true;
}

int32 FGameXXKTrainingRules::CountChestTokens(
	const FGameXXKTrainingProgress& Progress,
	const EGameXXKTrainingRewardTier Tier)
{
	int32 Count = 0;
	for (const FGameXXKTrainingChestToken& Token : Progress.OwnedChestTokens)
	{
		if (Token.Tier == Tier) ++Count;
	}
	return Count;
}

bool FGameXXKTrainingRules::ValidateChestTokens(const FGameXXKTrainingProgress& Progress, FString* OutError)
{
	if (OutError) OutError->Reset();
	if (Progress.NextChestAcquisitionOrdinal < 0 || Progress.NextChestOpenOrdinal < 0)
	{
		if (OutError) *OutError = TEXT("Training chest ordinals cannot be negative.");
		return false;
	}
	int32 PreviousOrdinal = 0;
	for (const FGameXXKTrainingChestToken& Token : Progress.OwnedChestTokens)
	{
		FGameXXKTrainingStageDefinition Stage;
		if ((Token.Tier != EGameXXKTrainingRewardTier::NormalChest && Token.Tier != EGameXXKTrainingRewardTier::AdvancedChest)
			|| Token.SourceItemLevel < 1 || Token.SourceItemLevel > 100
			|| Token.AcquisitionOrdinal <= PreviousOrdinal
			|| Token.AcquisitionOrdinal > Progress.NextChestAcquisitionOrdinal
			|| !TryGetStageDefinition(Token.SourceStageId, Stage))
		{
			if (OutError) *OutError = TEXT("Training chest token ledger is invalid.");
			return false;
		}
		PreviousOrdinal = Token.AcquisitionOrdinal;
	}
	return true;
}

FName FGameXXKTrainingRules::DifficultyId(const EGameXXKTrainingDifficulty Difficulty)
{
	return FName(*FString::Printf(TEXT("Training.Difficulty.%s"), DifficultyLabel(Difficulty)));
}

FName FGameXXKTrainingRules::MakeStageId(const EGameXXKTrainingDifficulty Difficulty, const int32 StageNumber)
{
	const int32 ClampedStageNumber = FMath::Clamp(StageNumber, 1, StagesPerDifficulty);
	const int32 Chapter = ((ClampedStageNumber - 1) / 3) + 1;
	const int32 ChapterStage = ((ClampedStageNumber - 1) % 3) + 1;
	return FName(*FString::Printf(TEXT("Training.%s.%d-%d"), DifficultyLabel(Difficulty), Chapter, ChapterStage));
}

EGameXXKTrainingDifficulty FGameXXKTrainingRules::DifficultyFromStageId(const FName StageId)
{
	const FString Value = StageId.ToString();
	if (Value.Contains(TEXT(".Hard."))) return EGameXXKTrainingDifficulty::Hard;
	if (Value.Contains(TEXT(".Hell."))) return EGameXXKTrainingDifficulty::Hell;
	return EGameXXKTrainingDifficulty::Normal;
}

const TArray<FGameXXKTrainingStageDefinition>& FGameXXKTrainingRules::GetStageDefinitions()
{
	static const TArray<FGameXXKTrainingStageDefinition> Stages = BuildStages();
	return Stages;
}

bool FGameXXKTrainingRules::TryGetStageDefinition(const FName StageId, FGameXXKTrainingStageDefinition& OutDefinition)
{
	const FGameXXKTrainingStageDefinition* Found = GetStageDefinitions().FindByPredicate([StageId](const FGameXXKTrainingStageDefinition& Stage)
	{
		return Stage.StageId == StageId;
	});
	if (!Found)
	{
		return false;
	}
	OutDefinition = *Found;
	return true;
}

TArray<FGameXXKTrainingEncounterDefinition> FGameXXKTrainingRules::BuildEncounterSequence(const FName StageId, const bool bTravelMode)
{
	FGameXXKTrainingStageDefinition Stage;
	TArray<FGameXXKTrainingEncounterDefinition> Encounters;
	if (!TryGetStageDefinition(StageId, Stage))
	{
		return Encounters;
	}

	// Travel and active challenge intentionally share the exact authored
	// encounter health. The only Normal 1-1 exception is progression: it starts
	// cleared so a new player may Travel immediately.
	(void)bTravelMode;
	const TArray<FName> NormalFormation = BuildNormalFormation(Stage);
	for (int32 NormalIndex = 0; NormalIndex < 4; ++NormalIndex)
	{
		FGameXXKTrainingEncounterDefinition Encounter;
		Encounter.EnemyDefinitionIds = NormalFormation;
		Encounter.EnemyDefinitionId = NormalFormation.Num() > 0 ? NormalFormation[0] : NAME_None;
		Encounter.DisplayName = FText::FromString(JoinNames(Encounter.EnemyDefinitionIds));
		Encounter.Kind = EGameXXKTrainingEncounterKind::Normal;
		Encounter.BaseHealth = FMath::Max(1, 20 + Stage.Chapter * 10 + Stage.StageNumber * 3);
		Encounters.Add(MoveTemp(Encounter));
		if (NormalIndex == 1 || NormalIndex == 2)
		{
			const int32 EliteIndex = NormalIndex == 1 ? 0 : 1;
			FGameXXKTrainingEncounterDefinition Elite;
			Elite.EnemyDefinitionId = Stage.EliteEnemyPool.IsValidIndex(EliteIndex) ? Stage.EliteEnemyPool[EliteIndex] : NAME_None;
			Elite.EnemyDefinitionIds = BuildCoreFormation(Stage.NormalEnemyPool, Elite.EnemyDefinitionId);
			Elite.DisplayName = FText::FromString(JoinNames(Elite.EnemyDefinitionIds));
			Elite.Kind = EGameXXKTrainingEncounterKind::Elite;
			Elite.BaseHealth = FMath::Max(1, 55 + Stage.Chapter * 20 + Stage.StageNumber * 5);
			Encounters.Add(MoveTemp(Elite));
		}
	}

	FGameXXKTrainingEncounterDefinition Boss;
	Boss.EnemyDefinitionId = Stage.BossEnemyId;
	// 1-1/1-2 reuse an elite as the stage boss, so they keep the ordinary
	// rooster/civet flanks. A true chapter boss uses the two elite flanks.
	const TArray<FName>& BossFlankPool = Stage.EliteEnemyPool.Contains(Stage.BossEnemyId)
		? Stage.NormalEnemyPool
		: Stage.EliteEnemyPool;
	Boss.EnemyDefinitionIds = BuildCoreFormation(BossFlankPool, Boss.EnemyDefinitionId);
	Boss.DisplayName = FText::FromString(JoinNames(Boss.EnemyDefinitionIds));
	Boss.Kind = EGameXXKTrainingEncounterKind::Boss;
	Boss.BaseHealth = FMath::Max(1, 120 + Stage.Chapter * 50 + Stage.StageNumber * 10);
	Encounters.Add(MoveTemp(Boss));
	return Encounters;
}

void FGameXXKTrainingRules::InitializeNewGame(FGameXXKTrainingProgress& Progress)
{
	Progress = FGameXXKTrainingProgress();
	const FName NormalDifficulty = DifficultyId(EGameXXKTrainingDifficulty::Normal);
	const FName StageOne = MakeStageId(EGameXXKTrainingDifficulty::Normal, 1);
	Progress.UnlockedDifficultyIds.Add(NormalDifficulty);
	Progress.ClearedStageIds.Add(StageOne);
	Progress.SelectedStageId = StageOne;
	Progress.CurrentTravelStageId = StageOne;
	Progress.bTravelActive = true;
	Progress.bRetryOnFailure = true;
	Progress.ActiveTravelEncounterIndex = 0;
	Progress.ChallengeRewardSeed = DefaultChallengeRewardSeed();
}

bool FGameXXKTrainingRules::IsDifficultyUnlocked(const FGameXXKTrainingProgress& Progress, const EGameXXKTrainingDifficulty Difficulty)
{
	return Progress.UnlockedDifficultyIds.Contains(DifficultyId(Difficulty));
}

bool FGameXXKTrainingRules::IsStageCleared(const FGameXXKTrainingProgress& Progress, const FName StageId)
{
	return Progress.ClearedStageIds.Contains(StageId);
}

bool FGameXXKTrainingRules::AreAllStagesCleared(const FGameXXKTrainingProgress& Progress)
{
	for (const FGameXXKTrainingStageDefinition& Stage : GetStageDefinitions())
	{
		if (!IsStageCleared(Progress, Stage.StageId))
		{
			return false;
		}
	}
	return true;
}

bool FGameXXKTrainingRules::CanChallenge(const FGameXXKTrainingProgress& Progress, const FName StageId)
{
	FGameXXKTrainingStageDefinition Stage;
	if (!TryGetStageDefinition(StageId, Stage)
		|| !IsDifficultyUnlocked(Progress, Stage.Difficulty))
	{
		return false;
	}
	if (Stage.StageNumber > 1)
	{
		const FName Previous = MakeStageId(Stage.Difficulty, Stage.StageNumber - 1);
		if (!IsStageCleared(Progress, Previous))
		{
			return false;
		}
	}
	if (Stage.Difficulty != EGameXXKTrainingDifficulty::Normal)
	{
		const EGameXXKTrainingDifficulty PreviousDifficulty = DifficultyFromIndex(DifficultyIndex(Stage.Difficulty) - 1);
		if (!IsDifficultyUnlocked(Progress, PreviousDifficulty))
		{
			return false;
		}
	}
	return true;
}

bool FGameXXKTrainingRules::CanTravel(const FGameXXKTrainingProgress& Progress, const FName StageId)
{
	FGameXXKTrainingStageDefinition Stage;
	return IsStageCleared(Progress, StageId) && TryGetStageDefinition(StageId, Stage);
}

bool FGameXXKTrainingRules::StartChallenge(FGameXXKTrainingProgress& Progress, const FName StageId)
{
	if (!CanChallenge(Progress, StageId))
	{
		return false;
	}
	Progress.bChallengeActive = true;
	Progress.bTravelActive = false;
	Progress.ActiveTravelEncounterIndex = INDEX_NONE;
	Progress.ActiveChallengeStageId = StageId;
	Progress.ActiveChallengeEncounterIndex = 0;
	Progress.bChallengeAutoBattle = false;
	Progress.SelectedStageId = StageId;
	return true;
}

bool FGameXXKTrainingRules::CompleteChallenge(FGameXXKTrainingProgress& Progress, const FName StageId)
{
	FGameXXKTrainingStageDefinition Stage;
	if (!Progress.bChallengeActive || Progress.ActiveChallengeStageId != StageId || !TryGetStageDefinition(StageId, Stage))
	{
		return false;
	}
	Progress.ClearedStageIds.Add(StageId);
	Progress.bChallengeActive = false;
	Progress.ActiveChallengeStageId = NAME_None;
	Progress.ActiveChallengeEncounterIndex = INDEX_NONE;
	if (Stage.StageNumber < StagesPerDifficulty)
	{
		Progress.SelectedStageId = MakeStageId(Stage.Difficulty, Stage.StageNumber + 1);
	}
	else if (Stage.Difficulty != EGameXXKTrainingDifficulty::Hell)
	{
		const EGameXXKTrainingDifficulty Next = DifficultyFromIndex(DifficultyIndex(Stage.Difficulty) + 1);
		Progress.UnlockedDifficultyIds.Add(DifficultyId(Next));
		Progress.SelectedStageId = MakeStageId(Next, 1);
	}
	else
	{
		Progress.SelectedStageId = StageId;
	}
	return true;
}

bool FGameXXKTrainingRules::StartTravel(FGameXXKTrainingProgress& Progress, const FName StageId)
{
	if (Progress.bChallengeActive || !CanTravel(Progress, StageId))
	{
		return false;
	}
	Progress.SelectedStageId = StageId;
	Progress.CurrentTravelStageId = StageId;
	Progress.bTravelActive = true;
	Progress.ActiveTravelEncounterIndex = 0;
	Progress.bTravelPausedAtDefeat = false;
	return true;
}

bool FGameXXKTrainingRules::InitializeTravelRunner(
	const FGameXXKTrainingProgress& Progress,
	FGameXXKTrainingTravelRuntime& OutRuntime,
	const int32 PlayerHP,
	const int32 PlayerMaxHP,
	const int32 PlayerAttack)

{
	const int32 SafeMaxHP = FMath::Max(1, PlayerMaxHP);
	return InitializeTravelRunner(
		Progress,
		OutRuntime,
		{FGameXXKTrainingTravelPartyUnitRuntime(
			FName(TEXT("Hero")),
			FMath::Clamp(PlayerHP, 0, SafeMaxHP),
			SafeMaxHP,
			FMath::Max(1, PlayerAttack))});
}

bool FGameXXKTrainingRules::InitializeTravelRunner(
	const FGameXXKTrainingProgress& Progress,
	FGameXXKTrainingTravelRuntime& OutRuntime,
	const TArray<FGameXXKTrainingTravelPartyUnitRuntime>& PartyUnits)
{
	OutRuntime = FGameXXKTrainingTravelRuntime();
	if (!Progress.bTravelActive || Progress.bChallengeActive || Progress.CurrentTravelStageId.IsNone())
	{
		return false;
	}

	const TArray<FGameXXKTrainingEncounterDefinition> Encounters = BuildEncounterSequence(Progress.CurrentTravelStageId, true);
	if (!Encounters.IsValidIndex(Progress.ActiveTravelEncounterIndex))
	{
		return false;
	}

	const FGameXXKTrainingEncounterDefinition& Encounter = Encounters[Progress.ActiveTravelEncounterIndex];
	OutRuntime.StageId = Progress.CurrentTravelStageId;
	OutRuntime.EncounterIndex = Progress.ActiveTravelEncounterIndex;
	OutRuntime.EncounterKind = Encounter.Kind;
	OutRuntime.Phase = EGameXXKTrainingTravelPhase::Walking;
	OutRuntime.WalkStep = 0;
	OutRuntime.WalkStepsRequired = 2;
	TSet<FName> SeenPartyUnitIds;
	for (const FGameXXKTrainingTravelPartyUnitRuntime& SourceUnit : PartyUnits)
	{
		if (SourceUnit.UnitId.IsNone()
			|| SourceUnit.MaxHP <= 0
			|| SourceUnit.Attack <= 0
			|| SeenPartyUnitIds.Contains(SourceUnit.UnitId)
			|| OutRuntime.PartyUnits.Num() >= 3)
		{
			continue;
		}
		FGameXXKTrainingTravelPartyUnitRuntime& Unit = OutRuntime.PartyUnits.Add_GetRef(SourceUnit);
		Unit.MaxHP = FMath::Max(1, Unit.MaxHP);
		Unit.HP = FMath::Clamp(Unit.HP, 0, Unit.MaxHP);
		Unit.Attack = FMath::Max(1, Unit.Attack);
		SeenPartyUnitIds.Add(Unit.UnitId);
	}
	if (OutRuntime.PartyUnits.IsEmpty())
	{
		return false;
	}
	OutRuntime.ActivePartyIndex = FindNextLivingTravelPartyUnit(OutRuntime, 0);
	OutRuntime.NextEnemyTargetPartyIndex = 0;
	SynchronizeTravelPartyCompatibility(OutRuntime);
	TArray<FName> Formation = Encounter.EnemyDefinitionIds;
	if (Formation.IsEmpty())
	{
		Formation.Add(Encounter.EnemyDefinitionId);
	}
	for (const FName EnemyId : Formation)
	{
		if (EnemyId.IsNone() || OutRuntime.Enemies.Num() >= 3)
		{
			continue;
		}
		FGameXXKTrainingTravelEnemyRuntime Enemy;
		Enemy.EnemyDefinitionId = EnemyId;
		Enemy.MaxHP = FMath::Max(1, Encounter.BaseHealth);
		Enemy.HP = Enemy.MaxHP;
		Enemy.Attack = TravelEnemyAttack(Encounter.Kind);
		OutRuntime.Enemies.Add(MoveTemp(Enemy));
	}
	OutRuntime.ActiveEnemyIndex = FindNextLivingTravelEnemy(OutRuntime, 0);
	if (!SynchronizeActiveTravelEnemy(OutRuntime))
	{
		return false;
	}
	OutRuntime.LastDamageToEnemy = 0;
	OutRuntime.LastDamageToPlayer = 0;
	OutRuntime.LastAttackingPartyIndex = INDEX_NONE;
	OutRuntime.LastDamagedPartyIndex = INDEX_NONE;
	OutRuntime.bAutoBattle = true;
	if (Progress.bTravelPausedAtDefeat)
	{
		OutRuntime.Phase = EGameXXKTrainingTravelPhase::Defeated;
		for (FGameXXKTrainingTravelPartyUnitRuntime& Unit : OutRuntime.PartyUnits)
		{
			Unit.HP = 0;
		}
		OutRuntime.ActivePartyIndex = INDEX_NONE;
		SynchronizeTravelPartyCompatibility(OutRuntime);
	}
	return true;
}

bool FGameXXKTrainingRules::AdvanceTravelRunner(
	FGameXXKTrainingProgress& Progress,
	FGameXXKTrainingTravelRuntime& InOutRuntime,
	bool& bOutEncounterCompleted,
	bool& bOutStageCompleted,
	bool& bOutDefeated,
	FGameXXKTrainingReward& OutReward,
	const int32 ElapsedSeconds)
{
	bOutEncounterCompleted = false;
	bOutStageCompleted = false;
	bOutDefeated = false;
	OutReward = FGameXXKTrainingReward();

	if (!Progress.bTravelActive || Progress.bChallengeActive || Progress.CurrentTravelStageId.IsNone())
	{
		return false;
	}
	if (InOutRuntime.StageId != Progress.CurrentTravelStageId
		|| InOutRuntime.EncounterIndex != Progress.ActiveTravelEncounterIndex)
	{
		return false;
	}
	Progress.TravelNormalChestCooldownRemainingSeconds = AdvanceTravelChestCooldown(
		Progress.TravelNormalChestCooldownRemainingSeconds,
		ElapsedSeconds);
	Progress.TravelAdvancedChestCooldownRemainingSeconds = AdvanceTravelChestCooldown(
		Progress.TravelAdvancedChestCooldownRemainingSeconds,
		ElapsedSeconds);

	if (InOutRuntime.Phase == EGameXXKTrainingTravelPhase::Walking)
	{
		InOutRuntime.LastDamageToEnemy = 0;
		InOutRuntime.LastDamageToPlayer = 0;
		InOutRuntime.LastAttackingPartyIndex = INDEX_NONE;
		InOutRuntime.LastDamagedPartyIndex = INDEX_NONE;
		InOutRuntime.WalkStep = FMath::Min(InOutRuntime.WalkStepsRequired, InOutRuntime.WalkStep + 1);
		if (InOutRuntime.WalkStep >= InOutRuntime.WalkStepsRequired)
		{
			InOutRuntime.Phase = EGameXXKTrainingTravelPhase::Combat;
		}
		return true;
	}

	if (InOutRuntime.Phase != EGameXXKTrainingTravelPhase::Combat)
	{
		return false;
	}
	if (!InOutRuntime.Enemies.IsValidIndex(InOutRuntime.ActiveEnemyIndex)
		|| InOutRuntime.Enemies[InOutRuntime.ActiveEnemyIndex].HP <= 0)
	{
		return false;
	}
	InOutRuntime.ActivePartyIndex = FindNextLivingTravelPartyUnit(
		InOutRuntime,
		FMath::Max(0, InOutRuntime.ActivePartyIndex));
	if (!InOutRuntime.PartyUnits.IsValidIndex(InOutRuntime.ActivePartyIndex))
	{
		InOutRuntime.Phase = EGameXXKTrainingTravelPhase::Defeated;
		Progress.bTravelPausedAtDefeat = true;
		bOutDefeated = true;
		SynchronizeTravelPartyCompatibility(InOutRuntime);
		return true;
	}

	FGameXXKTrainingTravelEnemyRuntime& ActiveEnemy = InOutRuntime.Enemies[InOutRuntime.ActiveEnemyIndex];
	const int32 AttackingPartyIndex = InOutRuntime.ActivePartyIndex;
	const FGameXXKTrainingTravelPartyUnitRuntime& AttackingUnit = InOutRuntime.PartyUnits[AttackingPartyIndex];
	InOutRuntime.LastAttackingPartyIndex = AttackingPartyIndex;
	InOutRuntime.LastDamagedPartyIndex = INDEX_NONE;
	InOutRuntime.LastDamageToEnemy = FMath::Clamp(AttackingUnit.Attack, 0, ActiveEnemy.HP);
	ActiveEnemy.HP = FMath::Max(0, ActiveEnemy.HP - InOutRuntime.LastDamageToEnemy);
	InOutRuntime.EnemyHP = ActiveEnemy.HP;
	InOutRuntime.LastDamageToPlayer = 0;
	const int32 NextAttackingPartyIndex = FindNextLivingTravelPartyUnit(
		InOutRuntime,
		AttackingPartyIndex + 1);
	InOutRuntime.ActivePartyIndex = NextAttackingPartyIndex;
	if (ActiveEnemy.HP <= 0)
	{
		const int32 NextLivingEnemyIndex = FindNextLivingTravelEnemy(
			InOutRuntime,
			InOutRuntime.ActiveEnemyIndex + 1);
		if (NextLivingEnemyIndex != INDEX_NONE)
		{
			// The whole authored formation is one settlement unit. Preserve the
			// defeated slot for the death presentation, then target the next living
			// member without advancing the durable encounter cursor or paying rewards.
			InOutRuntime.ActiveEnemyIndex = NextLivingEnemyIndex;
			return SynchronizeActiveTravelEnemy(InOutRuntime);
		}

		bOutEncounterCompleted = true;
		const TArray<FGameXXKTrainingEncounterDefinition> Encounters = BuildEncounterSequence(Progress.CurrentTravelStageId, true);
		if (!Encounters.IsValidIndex(InOutRuntime.EncounterIndex))
		{
			return false;
		}

		const bool bLastEncounter = InOutRuntime.EncounterIndex == Encounters.Num() - 1;
		OutReward = ResolveTravelReward(
			Progress.CurrentTravelStageId,
			Encounters[InOutRuntime.EncounterIndex].Kind,
			Progress.ChallengeRewardSeed,
			Progress.TravelNormalChestCooldownRemainingSeconds,
			Progress.TravelAdvancedChestCooldownRemainingSeconds,
			0.0f,
			true);
		if (OutReward.bChestRolled)
		{
			if (OutReward.ChestTier == EGameXXKTrainingRewardTier::AdvancedChest)
			{
				Progress.TravelAdvancedChestCooldownRemainingSeconds = TravelAdvancedChestCooldownSeconds;
			}
			else if (OutReward.ChestTier == EGameXXKTrainingRewardTier::NormalChest)
			{
				Progress.TravelNormalChestCooldownRemainingSeconds = TravelNormalChestCooldownSeconds;
			}
		}
		Progress.ChallengeRewardSeed = NextChallengeRewardSeed(Progress.ChallengeRewardSeed);
		if (bLastEncounter)
		{
			++Progress.TravelVictories;
			Progress.ActiveTravelEncounterIndex = 0;
			bOutStageCompleted = true;
		}
		else
		{
			++Progress.ActiveTravelEncounterIndex;
		}

		// Keep every party member's remaining HP and deterministic turn cursors.
		const TArray<FGameXXKTrainingTravelPartyUnitRuntime> PreservedParty = InOutRuntime.PartyUnits;
		const int32 PreservedNextAttacker = InOutRuntime.ActivePartyIndex;
		const int32 PreservedNextEnemyTarget = InOutRuntime.NextEnemyTargetPartyIndex;
		if (!InitializeTravelRunner(Progress, InOutRuntime, PreservedParty))
		{
			return false;
		}
		InOutRuntime.ActivePartyIndex = FindNextLivingTravelPartyUnit(
			InOutRuntime,
			FMath::Max(0, PreservedNextAttacker));
		InOutRuntime.NextEnemyTargetPartyIndex = FMath::Max(0, PreservedNextEnemyTarget);
		SynchronizeTravelPartyCompatibility(InOutRuntime);
		return true;
	}

	// One enemy retaliation occurs only after every currently living party
	// member has taken one action. This keeps all three actors meaningful while
	// retaining the lightweight one-mutation-per-tick desktop simulation.
	const bool bPartyRoundCompleted = NextAttackingPartyIndex == INDEX_NONE
		|| NextAttackingPartyIndex <= AttackingPartyIndex;
	if (bPartyRoundCompleted)
	{
		const int32 DamagedPartyIndex = FindNextLivingTravelPartyUnit(
			InOutRuntime,
			InOutRuntime.NextEnemyTargetPartyIndex);
		if (InOutRuntime.PartyUnits.IsValidIndex(DamagedPartyIndex))
		{
			FGameXXKTrainingTravelPartyUnitRuntime& DamagedUnit = InOutRuntime.PartyUnits[DamagedPartyIndex];
			InOutRuntime.LastDamagedPartyIndex = DamagedPartyIndex;
			InOutRuntime.LastDamageToPlayer = FMath::Min(ActiveEnemy.Attack, DamagedUnit.HP);
			DamagedUnit.HP = FMath::Max(0, DamagedUnit.HP - InOutRuntime.LastDamageToPlayer);
			InOutRuntime.NextEnemyTargetPartyIndex = DamagedPartyIndex + 1;
		}
		InOutRuntime.ActivePartyIndex = FindNextLivingTravelPartyUnit(
			InOutRuntime,
			FMath::Max(0, NextAttackingPartyIndex));
		if (InOutRuntime.ActivePartyIndex == INDEX_NONE)
		{
			InOutRuntime.Phase = EGameXXKTrainingTravelPhase::Defeated;
			Progress.bTravelPausedAtDefeat = true;
			bOutDefeated = true;
		}
	}
	SynchronizeTravelPartyCompatibility(InOutRuntime);
	return true;
}

bool FGameXXKTrainingRules::AdvanceTravelEncounter(
	FGameXXKTrainingProgress& Progress,
	bool& bOutStageCompleted,
	FGameXXKTrainingReward& OutReward)
{
	bOutStageCompleted = false;
	OutReward = FGameXXKTrainingReward();
	if (!Progress.bTravelActive || Progress.bChallengeActive || Progress.CurrentTravelStageId.IsNone())
	{
		return false;
	}
	Progress.TravelNormalChestCooldownRemainingSeconds = AdvanceTravelChestCooldown(
		Progress.TravelNormalChestCooldownRemainingSeconds,
		1);
	Progress.TravelAdvancedChestCooldownRemainingSeconds = AdvanceTravelChestCooldown(
		Progress.TravelAdvancedChestCooldownRemainingSeconds,
		1);
	const TArray<FGameXXKTrainingEncounterDefinition> Encounters = BuildEncounterSequence(Progress.CurrentTravelStageId, true);
	if (!Encounters.IsValidIndex(Progress.ActiveTravelEncounterIndex))
	{
		return false;
	}
	const bool bLastEncounter = Progress.ActiveTravelEncounterIndex == Encounters.Num() - 1;
	OutReward = ResolveTravelReward(
		Progress.CurrentTravelStageId,
		Encounters[Progress.ActiveTravelEncounterIndex].Kind,
		Progress.ChallengeRewardSeed,
		Progress.TravelNormalChestCooldownRemainingSeconds,
		Progress.TravelAdvancedChestCooldownRemainingSeconds,
		0.0f,
		true);
	if (OutReward.bChestRolled)
	{
		if (OutReward.ChestTier == EGameXXKTrainingRewardTier::AdvancedChest)
		{
			Progress.TravelAdvancedChestCooldownRemainingSeconds = TravelAdvancedChestCooldownSeconds;
		}
		else if (OutReward.ChestTier == EGameXXKTrainingRewardTier::NormalChest)
		{
			Progress.TravelNormalChestCooldownRemainingSeconds = TravelNormalChestCooldownSeconds;
		}
	}
	Progress.ChallengeRewardSeed = NextChallengeRewardSeed(Progress.ChallengeRewardSeed);
	if (!bLastEncounter)
	{
		++Progress.ActiveTravelEncounterIndex;
		return true;
	}
	++Progress.TravelVictories;
	Progress.ActiveTravelEncounterIndex = 0;
	bOutStageCompleted = true;
	return true;
}

bool FGameXXKTrainingRules::AdvanceTravelOffline(
	FGameXXKTrainingProgress& Progress,
	FGameXXKTrainingTravelRuntime& InOutRuntime,
	const int32 ElapsedSeconds,
	FGameXXKTrainingOfflineReward& OutReward)
{
	OutReward = FGameXXKTrainingOfflineReward();
	if (!Progress.bTravelActive || Progress.bChallengeActive || ElapsedSeconds <= 0)
	{
		return false;
	}

	const int32 SimulationSeconds = FMath::Clamp(ElapsedSeconds, 1, MaxTravelOfflineSimulationSeconds);
	for (int32 Second = 0; Second < SimulationSeconds; ++Second)
	{
		bool bEncounterCompleted = false;
		bool bStageCompleted = false;
		bool bDefeated = false;
		FGameXXKTrainingReward Reward;
		if (!AdvanceTravelRunner(
			Progress,
			InOutRuntime,
			bEncounterCompleted,
			bStageCompleted,
			bDefeated,
			Reward,
			1))
		{
			return false;
		}

		++OutReward.SimulatedSeconds;
		if (bEncounterCompleted)
		{
			++OutReward.CompletedEncounters;
			OutReward.Gold = FMath::Max(0, OutReward.Gold + Reward.Gold);
			OutReward.Experience = FMath::Max(0, OutReward.Experience + Reward.Experience);
			if (Reward.ChestTier == EGameXXKTrainingRewardTier::NormalChest && Reward.bChestRolled)
			{
				++OutReward.NormalChestCount;
			}
			else if (Reward.ChestTier == EGameXXKTrainingRewardTier::AdvancedChest && Reward.bChestRolled)
			{
				++OutReward.AdvancedChestCount;
			}
		}
		if (bStageCompleted)
		{
			++OutReward.CompletedStages;
		}
		if (bDefeated)
		{
			OutReward.bStoppedAtDefeat = true;
			break;
		}
	}
	return OutReward.SimulatedSeconds > 0;
}

bool FGameXXKTrainingRules::AccumulatePendingTravelReward(
	FGameXXKTrainingProgress& Progress,
	const FGameXXKTrainingOfflineReward& Reward)
{
	if (Reward.Gold < 0
		|| Reward.Experience < 0
		|| Reward.NormalChestCount < 0
		|| Reward.AdvancedChestCount < 0
		|| Reward.CompletedEncounters < 0
		|| Reward.CompletedStages < 0
		|| Reward.SimulatedSeconds < 0)
	{
		return false;
	}
	Progress.PendingTravelGold = FMath::Max(0, Progress.PendingTravelGold + Reward.Gold);
	Progress.PendingTravelExperience = FMath::Max(0, Progress.PendingTravelExperience + Reward.Experience);
	Progress.PendingTravelNormalChestCount = FMath::Max(0, Progress.PendingTravelNormalChestCount + Reward.NormalChestCount);
	Progress.PendingTravelAdvancedChestCount = FMath::Max(0, Progress.PendingTravelAdvancedChestCount + Reward.AdvancedChestCount);
	Progress.PendingTravelCompletedEncounters = FMath::Max(0, Progress.PendingTravelCompletedEncounters + Reward.CompletedEncounters);
	Progress.PendingTravelCompletedStages = FMath::Max(0, Progress.PendingTravelCompletedStages + Reward.CompletedStages);
	Progress.PendingTravelSimulatedSeconds = FMath::Max(0, Progress.PendingTravelSimulatedSeconds + Reward.SimulatedSeconds);
	return true;
}

bool FGameXXKTrainingRules::GetPendingTravelReward(
	const FGameXXKTrainingProgress& Progress,
	FGameXXKTrainingOfflineReward& OutReward)
{
	OutReward = FGameXXKTrainingOfflineReward();
	OutReward.Gold = FMath::Max(0, Progress.PendingTravelGold);
	OutReward.Experience = FMath::Max(0, Progress.PendingTravelExperience);
	OutReward.NormalChestCount = FMath::Max(0, Progress.PendingTravelNormalChestCount);
	OutReward.AdvancedChestCount = FMath::Max(0, Progress.PendingTravelAdvancedChestCount);
	OutReward.CompletedEncounters = FMath::Max(0, Progress.PendingTravelCompletedEncounters);
	OutReward.CompletedStages = FMath::Max(0, Progress.PendingTravelCompletedStages);
	OutReward.SimulatedSeconds = FMath::Max(0, Progress.PendingTravelSimulatedSeconds);
	OutReward.bStoppedAtDefeat = Progress.bTravelPausedAtDefeat;
	return OutReward.Gold > 0
		|| OutReward.Experience > 0
		|| OutReward.NormalChestCount > 0
		|| OutReward.AdvancedChestCount > 0
		|| OutReward.CompletedEncounters > 0
		|| OutReward.CompletedStages > 0
		|| OutReward.SimulatedSeconds > 0;
}

bool FGameXXKTrainingRules::ConsumePendingTravelReward(
	FGameXXKTrainingProgress& Progress,
	FGameXXKTrainingOfflineReward& OutReward)
{
	if (!GetPendingTravelReward(Progress, OutReward))
	{
		return false;
	}
	Progress.PendingTravelGold = 0;
	Progress.PendingTravelExperience = 0;
	Progress.PendingTravelNormalChestCount = 0;
	Progress.PendingTravelAdvancedChestCount = 0;
	Progress.PendingTravelCompletedEncounters = 0;
	Progress.PendingTravelCompletedStages = 0;
	Progress.PendingTravelSimulatedSeconds = 0;
	return true;
}

bool FGameXXKTrainingRules::ResolveTravelFailure(FGameXXKTrainingProgress& Progress)
{
	if (!Progress.bTravelActive || Progress.CurrentTravelStageId.IsNone())
	{
		return false;
	}
	++Progress.TravelFailures;
	if (Progress.bRetryOnFailure)
	{
		Progress.ActiveTravelEncounterIndex = 0;
		Progress.bTravelPausedAtDefeat = false;
		return true;
	}
	FGameXXKTrainingStageDefinition Stage;
	if (!TryGetStageDefinition(Progress.CurrentTravelStageId, Stage))
	{
		return false;
	}
	const int32 PreviousStageNumber = FMath::Max(1, Stage.StageNumber - 1);
	Progress.CurrentTravelStageId = MakeStageId(Stage.Difficulty, PreviousStageNumber);
	Progress.SelectedStageId = Progress.CurrentTravelStageId;
	Progress.bTravelActive = false;
	Progress.ActiveTravelEncounterIndex = INDEX_NONE;
	Progress.bTravelPausedAtDefeat = false;
	return true;
}

FGameXXKTrainingReward FGameXXKTrainingRules::BuildTravelReward(const FName StageId)
{
	FGameXXKTrainingReward Reward;
	FGameXXKTrainingStageDefinition Stage;
	if (!TryGetStageDefinition(StageId, Stage))
	{
		return Reward;
	}
	Reward.Gold = Stage.TravelGold;
	Reward.Experience = Stage.TravelExperience;
	Reward.ChestTier = EGameXXKTrainingRewardTier::None;
	Reward.bChestRolled = false;
	return Reward;
}

FGameXXKTrainingReward FGameXXKTrainingRules::BuildChallengeReward(
	const FName StageId,
	const EGameXXKTrainingEncounterKind EncounterKind,
	const bool bChestRolled,
	const float TalentChestDropBonus)
{
	FGameXXKTrainingReward Reward = BuildTravelReward(StageId);
	FGameXXKTrainingStageDefinition Stage;
	if (!TryGetStageDefinition(StageId, Stage))
	{
		return Reward;
	}
	Reward.Gold = FMath::Max(1, Stage.TravelGold * 2);
	Reward.Experience = FMath::Max(1, Stage.TravelExperience * 2);
	Reward.bChestRolled = bChestRolled;
	if (!bChestRolled)
	{
		return Reward;
	}
	const float Chance = ChestChanceForEncounter(Stage, EncounterKind);
	// This legacy entry point intentionally preserves the old forced-roll
	// contract for compatibility fixtures. Production settlement uses the
	// seeded ResolveChallengeReward/ResolveTravelReward APIs below.
	const float EffectiveChance = FMath::Clamp(Chance + TalentChestDropBonus, 0.0f, 1.0f);
	Reward.ChestTier = EffectiveChance > 0.0f
		? ChestTierForEncounter(EncounterKind)
		: EGameXXKTrainingRewardTier::None;
	Reward.ChestItemId = ChestItemIdForTierInternal(Reward.ChestTier);
	return Reward;
}

FGameXXKTrainingReward FGameXXKTrainingRules::ResolveChallengeReward(
	const FName StageId,
	const EGameXXKTrainingEncounterKind EncounterKind,
	const int32 RewardSeed,
	const float TalentChestDropBonus)
{
	FGameXXKTrainingReward Reward = BuildTravelReward(StageId);
	FGameXXKTrainingStageDefinition Stage;
	if (!TryGetStageDefinition(StageId, Stage))
	{
		return Reward;
	}
	Reward.Gold = FMath::Max(1, Stage.TravelGold * 2);
	Reward.Experience = FMath::Max(1, Stage.TravelExperience * 2);
	Reward.ChestTier = EGameXXKTrainingRewardTier::None;
	Reward.bChestRolled = false;
	const float Chance = FMath::Clamp(ChestChanceForEncounter(Stage, EncounterKind) + TalentChestDropBonus, 0.0f, 1.0f);
	if (Chance <= 0.0f)
	{
		return Reward;
	}
	if (Chance >= 1.0f || RewardRoll(StageId, EncounterKind, RewardSeed == 0 ? DefaultChallengeRewardSeed() : RewardSeed) < Chance)
	{
		Reward.bChestRolled = true;
		Reward.ChestTier = ChestTierForEncounter(EncounterKind);
		Reward.ChestItemId = ChestItemIdForTierInternal(Reward.ChestTier);
	}
	return Reward;
}

FGameXXKTrainingReward FGameXXKTrainingRules::ResolveTravelReward(
	const FName StageId,
	const EGameXXKTrainingEncounterKind EncounterKind,
	const int32 RewardSeed,
	const int32 NormalChestCooldownRemainingSeconds,
	const int32 AdvancedChestCooldownRemainingSeconds,
	const float TalentChestDropBonus,
	const bool bIncludeStageReward)
{
	FGameXXKTrainingReward Reward;
	if (bIncludeStageReward)
	{
		Reward = BuildTravelReward(StageId);
	}
	FGameXXKTrainingStageDefinition Stage;
	if (!TryGetStageDefinition(StageId, Stage))
	{
		return Reward;
	}
	const EGameXXKTrainingRewardTier Tier = ChestTierForEncounter(EncounterKind);
	const int32 CooldownRemaining = Tier == EGameXXKTrainingRewardTier::AdvancedChest
		? FMath::Max(0, AdvancedChestCooldownRemainingSeconds)
		: FMath::Max(0, NormalChestCooldownRemainingSeconds);
	const float Chance = FMath::Clamp(ChestChanceForEncounter(Stage, EncounterKind) + TalentChestDropBonus, 0.0f, 1.0f);
	if (CooldownRemaining > 0 || Chance <= 0.0f)
	{
		return Reward;
	}
	if (Chance >= 1.0f || RewardRoll(StageId, EncounterKind, RewardSeed == 0 ? DefaultChallengeRewardSeed() : RewardSeed) < Chance)
	{
		Reward.bChestRolled = true;
		Reward.ChestTier = Tier;
		Reward.ChestItemId = ChestItemIdForTierInternal(Tier);
	}
	return Reward;
}

int32 FGameXXKTrainingRules::DefaultChallengeRewardSeed()
{
	return 0x13579BDF;
}

int32 FGameXXKTrainingRules::NextChallengeRewardSeed(const int32 RewardSeed)
{
	uint32 Next = static_cast<uint32>(RewardSeed == 0 ? DefaultChallengeRewardSeed() : RewardSeed);
	Next = Next * 1664525U + 1013904223U;
	if (Next == 0U)
	{
		Next = static_cast<uint32>(DefaultChallengeRewardSeed());
	}
	return static_cast<int32>(Next);
}

int32 FGameXXKTrainingRules::AdvanceTravelChestCooldown(const int32 RemainingSeconds, const int32 ElapsedSeconds)
{
	return FMath::Max(0, FMath::Max(0, RemainingSeconds) - FMath::Max(0, ElapsedSeconds));
}

int32 FGameXXKTrainingRules::TravelChestCooldownSeconds(const EGameXXKTrainingRewardTier ChestTier)
{
	switch (ChestTier)
	{
	case EGameXXKTrainingRewardTier::NormalChest:
		return TravelNormalChestCooldownSeconds;
	case EGameXXKTrainingRewardTier::AdvancedChest:
		return TravelAdvancedChestCooldownSeconds;
	default:
		return 0;
	}
}

FName FGameXXKTrainingRules::ChestItemIdForTier(const EGameXXKTrainingRewardTier ChestTier)
{
	return ChestItemIdForTierInternal(ChestTier);
}

FText FGameXXKTrainingRules::BuildStageTooltip(const FGameXXKTrainingProgress& Progress, const FName StageId)
{
	FGameXXKTrainingStageDefinition Stage;
	if (!TryGetStageDefinition(StageId, Stage))
	{
		return FText::FromString(TEXT("历练关卡不存在"));
	}
	const TCHAR* Status = IsStageCleared(Progress, StageId)
		? TEXT("已通关，可游历")
		: CanChallenge(Progress, StageId) ? TEXT("可挑战") : TEXT("未解锁");
	return FText::FromString(FString::Printf(
		TEXT("%s\n状态：%s\n普通：%s\n次级精英：%s\n首领：%s\n悬停可查看实际编制"),
		*Stage.DisplayName.ToString(),
		Status,
		*JoinNames(Stage.NormalEnemyPool),
		*JoinNames(Stage.EliteEnemyPool),
		*Stage.BossDisplayName.ToString()));
}
