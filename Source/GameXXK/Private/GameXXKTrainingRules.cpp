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
				Stage.bOneHealthTravelException = Difficulty == EGameXXKTrainingDifficulty::Normal && StageNumber == 1;
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

	const bool bOneHealth = bTravelMode && Stage.bOneHealthTravelException;
	for (int32 NormalIndex = 0; NormalIndex < 4; ++NormalIndex)
	{
		const FName EnemyId = Stage.NormalEnemyPool.IsValidIndex(NormalIndex)
			? Stage.NormalEnemyPool[NormalIndex]
			: NAME_None;
		FGameXXKTrainingEncounterDefinition Encounter;
		Encounter.EnemyDefinitionId = EnemyId;
		Encounter.DisplayName = EnemyName(EnemyId);
		Encounter.Kind = EGameXXKTrainingEncounterKind::Normal;
		Encounter.BaseHealth = bOneHealth ? 1 : FMath::Max(1, 20 + Stage.Chapter * 10 + Stage.StageNumber * 3);
		Encounters.Add(MoveTemp(Encounter));
		if (NormalIndex == 1 || NormalIndex == 2)
		{
			const int32 EliteIndex = NormalIndex == 1 ? 0 : 1;
			FGameXXKTrainingEncounterDefinition Elite;
			Elite.EnemyDefinitionId = Stage.EliteEnemyPool.IsValidIndex(EliteIndex) ? Stage.EliteEnemyPool[EliteIndex] : NAME_None;
			Elite.DisplayName = EnemyName(Elite.EnemyDefinitionId);
			Elite.Kind = EGameXXKTrainingEncounterKind::Elite;
			Elite.BaseHealth = bOneHealth ? 1 : FMath::Max(1, 55 + Stage.Chapter * 20 + Stage.StageNumber * 5);
			Encounters.Add(MoveTemp(Elite));
		}
	}

	FGameXXKTrainingEncounterDefinition Boss;
	Boss.EnemyDefinitionId = Stage.BossEnemyId;
	Boss.DisplayName = Stage.BossDisplayName;
	Boss.Kind = EGameXXKTrainingEncounterKind::Boss;
	Boss.BaseHealth = bOneHealth ? 1 : FMath::Max(1, 120 + Stage.Chapter * 50 + Stage.StageNumber * 10);
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
		|| !IsDifficultyUnlocked(Progress, Stage.Difficulty)
		|| IsStageCleared(Progress, StageId))
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
	return true;
}

bool FGameXXKTrainingRules::InitializeTravelRunner(
	const FGameXXKTrainingProgress& Progress,
	FGameXXKTrainingTravelRuntime& OutRuntime,
	const int32 PlayerHP,
	const int32 PlayerMaxHP,
	const int32 PlayerAttack)
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
	OutRuntime.EnemyDefinitionId = Encounter.EnemyDefinitionId;
	OutRuntime.EncounterKind = Encounter.Kind;
	OutRuntime.Phase = EGameXXKTrainingTravelPhase::Walking;
	OutRuntime.WalkStep = 0;
	OutRuntime.WalkStepsRequired = 2;
	OutRuntime.PlayerMaxHP = FMath::Max(1, PlayerMaxHP);
	OutRuntime.PlayerHP = FMath::Clamp(PlayerHP, 0, OutRuntime.PlayerMaxHP);
	OutRuntime.PlayerAttack = FMath::Max(1, PlayerAttack);
	OutRuntime.EnemyMaxHP = FMath::Max(1, Encounter.BaseHealth);
	OutRuntime.EnemyHP = OutRuntime.EnemyMaxHP;
	OutRuntime.EnemyAttack = TravelEnemyAttack(Encounter.Kind);
	OutRuntime.LastDamageToEnemy = 0;
	OutRuntime.LastDamageToPlayer = 0;
	OutRuntime.bAutoBattle = true;
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

	InOutRuntime.LastDamageToEnemy = FMath::Clamp(InOutRuntime.PlayerAttack, 0, InOutRuntime.EnemyHP);
	InOutRuntime.EnemyHP = FMath::Max(0, InOutRuntime.EnemyHP - InOutRuntime.LastDamageToEnemy);
	InOutRuntime.LastDamageToPlayer = 0;
	if (InOutRuntime.EnemyHP <= 0)
	{
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
			bLastEncounter);
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

		// Keep the player's remaining HP and restart the next loop at its walking
		// phase.  This is the deterministic low-cost loop consumed by the desktop
		// strip; the UI can render the new enemy without inventing another state.
		return InitializeTravelRunner(
			Progress,
			InOutRuntime,
			InOutRuntime.PlayerHP,
			InOutRuntime.PlayerMaxHP,
			InOutRuntime.PlayerAttack);
	}

	InOutRuntime.LastDamageToPlayer = FMath::Min(InOutRuntime.EnemyAttack, InOutRuntime.PlayerHP);
	InOutRuntime.PlayerHP = FMath::Max(0, InOutRuntime.PlayerHP - InOutRuntime.LastDamageToPlayer);
	if (InOutRuntime.PlayerHP <= 0)
	{
		InOutRuntime.Phase = EGameXXKTrainingTravelPhase::Defeated;
		bOutDefeated = true;
	}
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
	if (!bLastEncounter)
	{
		++Progress.ActiveTravelEncounterIndex;
		return true;
	}

	OutReward = ResolveTravelReward(
		Progress.CurrentTravelStageId,
		Encounters[Progress.ActiveTravelEncounterIndex].Kind,
		Progress.ChallengeRewardSeed,
		Progress.TravelNormalChestCooldownRemainingSeconds,
		Progress.TravelAdvancedChestCooldownRemainingSeconds,
		0.0f,
		bLastEncounter);
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
	++Progress.TravelVictories;
	Progress.ActiveTravelEncounterIndex = 0;
	bOutStageCompleted = true;
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
	// Normal 1-1 is the explicit low-cost onboarding exception: Travel uses
	// one-health encounters there and grants only gold/experience.  It must not
	// silently inherit the normal/advanced chest resolver even when the caller
	// supplies a guaranteed chance or a talent bonus.
	if (Stage.bOneHealthTravelException)
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
