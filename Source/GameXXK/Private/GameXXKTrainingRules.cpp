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
}

bool FGameXXKTrainingRules::IsDifficultyUnlocked(const FGameXXKTrainingProgress& Progress, const EGameXXKTrainingDifficulty Difficulty)
{
	return Progress.UnlockedDifficultyIds.Contains(DifficultyId(Difficulty));
}

bool FGameXXKTrainingRules::IsStageCleared(const FGameXXKTrainingProgress& Progress, const FName StageId)
{
	return Progress.ClearedStageIds.Contains(StageId);
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

	OutReward = BuildTravelReward(Progress.CurrentTravelStageId);
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
	const float Chance = EncounterKind == EGameXXKTrainingEncounterKind::Boss
		? Stage.AdvancedChestChance
		: Stage.NormalChestChance;
	// The actual random roll remains outside this pure resolver.  The bonus is
	// clamped here so future talent trees cannot produce an invalid probability.
	const float EffectiveChance = FMath::Clamp(Chance + TalentChestDropBonus, 0.0f, 1.0f);
	Reward.ChestTier = EncounterKind == EGameXXKTrainingEncounterKind::Boss && EffectiveChance > 0.0f
		? EGameXXKTrainingRewardTier::AdvancedChest
		: (EffectiveChance > 0.0f ? EGameXXKTrainingRewardTier::NormalChest : EGameXXKTrainingRewardTier::None);
	return Reward;
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
