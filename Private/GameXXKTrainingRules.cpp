#include "GameXXKTrainingRules.h"

#include "GameXXKEnemyCatalog.h"

namespace
{
	const FName NormalId(TEXT("Normal"));
	const FName HardId(TEXT("Hard"));
	const FName HellId(TEXT("Hell"));

	const FName RoosterId(TEXT("Enemy.Ch1.Rooster"));
	const FName GoatId(TEXT("Enemy.Ch1.Goat"));
	const FName WeaselId(TEXT("Enemy.Ch1.Weasel"));
	const FName CivetId(TEXT("Enemy.Ch1.Civet"));
	const FName IronfeatherId(TEXT("Enemy.Ch1.IronfeatherRooster"));
	const FName BluehornId(TEXT("Enemy.Ch1.BluehornGoatKing"));
	const FName MoneyRatId(TEXT("Enemy.Ch1.MoneyRat"));

	const FName GrayWolfId(TEXT("Enemy.Ch2.GrayWolf"));
	const FName BoarId(TEXT("Enemy.Ch2.Boar"));
	const FName MacaqueId(TEXT("Enemy.Ch2.Macaque"));
	const FName PorcupineId(TEXT("Enemy.Ch2.Porcupine"));
	const FName GraymaneId(TEXT("Enemy.Ch2.GraymaneWolfKing"));
	const FName RedtuskId(TEXT("Enemy.Ch2.RedtuskBoarKing"));
	const FName BlackBearId(TEXT("Enemy.Ch2.BlackBear"));

	const FName SnakeId(TEXT("Enemy.Ch3.VenomSnake"));
	const FName WildcatId(TEXT("Enemy.Ch3.Wildcat"));
	const FName VultureId(TEXT("Enemy.Ch3.Vulture"));
	const FName ToadId(TEXT("Enemy.Ch3.GiantToad"));
	const FName WhiteApeId(TEXT("Enemy.Ch3.WhiteApe"));
	const FName DeerId(TEXT("Enemy.Ch3.SpiralHornDeer"));
	const FName TigerId(TEXT("Enemy.Ch3.Tiger"));

	FText EnemyDisplayName(const FName EnemyId)
	{
		if (const FGameXXKEnemyDefinition* Definition = FGameXXKEnemyCatalog::Find(EnemyId))
		{
			return Definition->DisplayName;
		}
		return FText::FromName(EnemyId);
	}

	int32 DifficultyMultiplier(const EGameXXKTrainingDifficulty Difficulty)
	{
		switch (Difficulty)
		{
		case EGameXXKTrainingDifficulty::Hard: return 2;
		case EGameXXKTrainingDifficulty::Hell: return 3;
		default: return 1;
		}
	}

	FName StageIdFor(const EGameXXKTrainingDifficulty Difficulty, const int32 StageNumber)
	{
		return FGameXXKTrainingRules::MakeStageId(Difficulty, StageNumber);
	}

	FGameXXKTrainingStageDefinition MakeStage(
		const EGameXXKTrainingDifficulty Difficulty,
		const int32 StageNumber)
	{
		FGameXXKTrainingStageDefinition Definition;
		Definition.Difficulty = Difficulty;
		Definition.StageNumber = FMath::Clamp(StageNumber, 1, FGameXXKTrainingRules::StagesPerDifficulty);
		Definition.Chapter = ((Definition.StageNumber - 1) / 3) + 1;
		Definition.StageId = StageIdFor(Difficulty, Definition.StageNumber);
		Definition.DisplayName = FText::FromString(FString::Printf(TEXT("%s %d-%d"),
			*FGameXXKTrainingRules::DifficultyId(Difficulty).ToString(),
			Definition.Chapter,
			((Definition.StageNumber - 1) % 3) + 1));
		Definition.TravelGold = (10 + Definition.Chapter * 4) * DifficultyMultiplier(Difficulty);
		Definition.TravelExperience = (6 + Definition.Chapter * 3) * DifficultyMultiplier(Difficulty);
		Definition.bOneHealthTravelException = Difficulty == EGameXXKTrainingDifficulty::Normal
			&& Definition.StageNumber == 1;
		Definition.NormalChestChance = 0.25f;
		Definition.AdvancedChestChance = 0.50f;

		switch (Definition.Chapter)
		{
		case 1:
			Definition.NormalEnemyPool = {RoosterId, GoatId, WeaselId, CivetId};
			// The requested first-chapter sub-elite policy intentionally uses the
			// existing Goat/Weasel definitions while keeping the tier distinct in
			// the Training route and tooltip.
			Definition.EliteEnemyPool = {GoatId, WeaselId};
			Definition.BossEnemyId = Definition.StageNumber == 1
				? BluehornId
				: Definition.StageNumber == 2 ? IronfeatherId : MoneyRatId;
			break;
		case 2:
			Definition.NormalEnemyPool = {GrayWolfId, BoarId, MacaqueId, PorcupineId};
			Definition.EliteEnemyPool = {GraymaneId, RedtuskId};
			Definition.BossEnemyId = BlackBearId;
			break;
		default:
			Definition.NormalEnemyPool = {SnakeId, WildcatId, VultureId, ToadId};
			Definition.EliteEnemyPool = {WhiteApeId, DeerId};
			Definition.BossEnemyId = TigerId;
			break;
		}
		Definition.BossDisplayName = EnemyDisplayName(Definition.BossEnemyId);
		return Definition;
	}

	FName PreviousStageId(const FName StageId)
	{
		FGameXXKTrainingStageDefinition Definition;
		if (!FGameXXKTrainingRules::TryGetStageDefinition(StageId, Definition))
		{
			return NAME_None;
		}
		if (Definition.StageNumber > 1)
		{
			return FGameXXKTrainingRules::MakeStageId(Definition.Difficulty, Definition.StageNumber - 1);
		}
		if (Definition.Difficulty == EGameXXKTrainingDifficulty::Hard)
		{
			return FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 9);
		}
		if (Definition.Difficulty == EGameXXKTrainingDifficulty::Hell)
		{
			return FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Hard, 9);
		}
		return StageId;
	}
}

FName FGameXXKTrainingRules::DifficultyId(const EGameXXKTrainingDifficulty Difficulty)
{
	switch (Difficulty)
	{
	case EGameXXKTrainingDifficulty::Hard: return HardId;
	case EGameXXKTrainingDifficulty::Hell: return HellId;
	default: return NormalId;
	}
}

FName FGameXXKTrainingRules::MakeStageId(const EGameXXKTrainingDifficulty Difficulty, const int32 StageNumber)
{
	return FName(*FString::Printf(TEXT("Training.%s.%d-%d"),
		*DifficultyId(Difficulty).ToString(),
		((FMath::Clamp(StageNumber, 1, StagesPerDifficulty) - 1) / 3) + 1,
		((FMath::Clamp(StageNumber, 1, StagesPerDifficulty) - 1) % 3) + 1));
}

EGameXXKTrainingDifficulty FGameXXKTrainingRules::DifficultyFromStageId(const FName StageId)
{
	const FString Value = StageId.ToString();
	if (Value.Contains(TEXT(".Hell."))) return EGameXXKTrainingDifficulty::Hell;
	if (Value.Contains(TEXT(".Hard."))) return EGameXXKTrainingDifficulty::Hard;
	return EGameXXKTrainingDifficulty::Normal;
}

TArray<FGameXXKTrainingStageDefinition> FGameXXKTrainingRules::GetStageDefinitions()
{
	TArray<FGameXXKTrainingStageDefinition> Definitions;
	Definitions.Reserve(3 * StagesPerDifficulty);
	for (const EGameXXKTrainingDifficulty Difficulty : {
		EGameXXKTrainingDifficulty::Normal,
		EGameXXKTrainingDifficulty::Hard,
		EGameXXKTrainingDifficulty::Hell})
	{
		for (int32 StageNumber = 1; StageNumber <= StagesPerDifficulty; ++StageNumber)
		{
			Definitions.Add(MakeStage(Difficulty, StageNumber));
		}
	}
	return Definitions;
}

bool FGameXXKTrainingRules::TryGetStageDefinition(const FName StageId, FGameXXKTrainingStageDefinition& OutDefinition)
{
	const TArray<FGameXXKTrainingStageDefinition> Definitions = GetStageDefinitions();
	const FGameXXKTrainingStageDefinition* Match = Definitions.FindByPredicate([StageId](const FGameXXKTrainingStageDefinition& Candidate)
	{
		return Candidate.StageId == StageId;
	});
	if (!Match)
	{
		return false;
	}
	OutDefinition = *Match;
	return true;
}

TArray<FGameXXKTrainingEncounterDefinition> FGameXXKTrainingRules::BuildEncounterSequence(const FName StageId)
{
	TArray<FGameXXKTrainingEncounterDefinition> Encounters;
	FGameXXKTrainingStageDefinition Definition;
	if (!TryGetStageDefinition(StageId, Definition))
	{
		return Encounters;
	}
	const int32 NormalHealth = Definition.bOneHealthTravelException ? 1 : 0;
	const int32 BaseHealth = FMath::Max(1, 46 * DifficultyMultiplier(Definition.Difficulty));
	for (int32 Index = 0; Index < Definition.NormalEnemyPool.Num(); ++Index)
	{
		FGameXXKTrainingEncounterDefinition Encounter;
		Encounter.EnemyDefinitionId = Definition.NormalEnemyPool[Index];
		Encounter.DisplayName = EnemyDisplayName(Encounter.EnemyDefinitionId);
		Encounter.Kind = EGameXXKTrainingEncounterKind::Normal;
		Encounter.BaseHealth = NormalHealth > 0 ? NormalHealth : BaseHealth;
		Encounters.Add(Encounter);
		if (Index == 1 && Definition.EliteEnemyPool.IsValidIndex(0))
		{
			FGameXXKTrainingEncounterDefinition Elite;
			Elite.EnemyDefinitionId = Definition.EliteEnemyPool[0];
			Elite.DisplayName = EnemyDisplayName(Elite.EnemyDefinitionId);
			Elite.Kind = EGameXXKTrainingEncounterKind::Elite;
			Elite.BaseHealth = NormalHealth > 0 ? NormalHealth : BaseHealth * 2;
			Encounters.Add(Elite);
		}
		if (Index == 2 && Definition.EliteEnemyPool.IsValidIndex(1))
		{
			FGameXXKTrainingEncounterDefinition Elite;
			Elite.EnemyDefinitionId = Definition.EliteEnemyPool[1];
			Elite.DisplayName = EnemyDisplayName(Elite.EnemyDefinitionId);
			Elite.Kind = EGameXXKTrainingEncounterKind::Elite;
			Elite.BaseHealth = NormalHealth > 0 ? NormalHealth : BaseHealth * 2;
			Encounters.Add(Elite);
		}
	}
	FGameXXKTrainingEncounterDefinition Boss;
	Boss.EnemyDefinitionId = Definition.BossEnemyId;
	Boss.DisplayName = Definition.BossDisplayName;
	Boss.Kind = EGameXXKTrainingEncounterKind::Boss;
	Boss.BaseHealth = NormalHealth > 0 ? NormalHealth : BaseHealth * 3;
	Encounters.Add(Boss);
	return Encounters;
}

void FGameXXKTrainingRules::InitializeNewGame(FGameXXKTrainingProgress& Progress)
{
	Progress = FGameXXKTrainingProgress();
	Progress.UnlockedDifficultyIds.Add(DifficultyId(EGameXXKTrainingDifficulty::Normal));
	Progress.ClearedStageIds.Add(MakeStageId(EGameXXKTrainingDifficulty::Normal, 1));
	Progress.SelectedStageId = MakeStageId(EGameXXKTrainingDifficulty::Normal, 1);
	Progress.CurrentTravelStageId = Progress.SelectedStageId;
	Progress.bTravelActive = true;
	Progress.bRetryOnFailure = true;
}

bool FGameXXKTrainingRules::IsDifficultyUnlocked(const FGameXXKTrainingProgress& Progress, const EGameXXKTrainingDifficulty Difficulty)
{
	return Progress.UnlockedDifficultyIds.Contains(DifficultyId(Difficulty));
}

bool FGameXXKTrainingRules::IsStageCleared(const FGameXXKTrainingProgress& Progress, const FName StageId)
{
	return Progress.ClearedStageIds.Contains(StageId);
}

bool FGameXXKTrainingRules::AreAllStagesCleared(const FGameXXKTrainingProgress& Progress, const EGameXXKTrainingDifficulty Difficulty)
{
	if (!IsDifficultyUnlocked(Progress, Difficulty))
	{
		return false;
	}
	for (int32 StageNumber = 1; StageNumber <= StagesPerDifficulty; ++StageNumber)
	{
		if (!IsStageCleared(Progress, MakeStageId(Difficulty, StageNumber)))
		{
			return false;
		}
	}
	return true;
}

bool FGameXXKTrainingRules::CanChallenge(const FGameXXKTrainingProgress& Progress, const FName StageId, FString* OutError)
{
	FGameXXKTrainingStageDefinition Definition;
	if (!TryGetStageDefinition(StageId, Definition))
	{
		SetError(OutError, TEXT("历练关卡不存在。"));
		return false;
	}
	if (!IsDifficultyUnlocked(Progress, Definition.Difficulty))
	{
		SetError(OutError, TEXT("请先完成前一难度的九关。"));
		return false;
	}
	if (IsStageCleared(Progress, StageId))
	{
		SetError(OutError, TEXT("该关卡已通关；游历可直接挂机。"));
		return false;
	}
	if (Definition.StageNumber > 1 && !IsStageCleared(Progress, MakeStageId(Definition.Difficulty, Definition.StageNumber - 1)))
	{
		SetError(OutError, TEXT("请先完成前置关卡。"));
		return false;
	}
	return true;
}

bool FGameXXKTrainingRules::CanTravel(const FGameXXKTrainingProgress& Progress, const FName StageId, FString* OutError)
{
	FGameXXKTrainingStageDefinition Definition;
	if (!TryGetStageDefinition(StageId, Definition))
	{
		SetError(OutError, TEXT("历练关卡不存在。"));
		return false;
	}
	if (!IsStageCleared(Progress, StageId))
	{
		SetError(OutError, TEXT("请先挑战通关后再游历。"));
		return false;
	}
	return true;
}

bool FGameXXKTrainingRules::StartChallenge(FGameXXKTrainingProgress& Progress, const FName StageId, FString* OutError)
{
	if (Progress.bChallengeActive || !CanChallenge(Progress, StageId, OutError))
	{
		if (Progress.bChallengeActive) SetError(OutError, TEXT("当前已有挑战进行中。"));
		return false;
	}
	Progress.bChallengeActive = true;
	Progress.ActiveChallengeStageId = StageId;
	Progress.ActiveChallengeEncounterIndex = 0;
	Progress.bChallengeAutoBattle = false;
	Progress.SelectedStageId = StageId;
	return true;
}

bool FGameXXKTrainingRules::CompleteChallenge(FGameXXKTrainingProgress& Progress, const FName StageId, FString* OutError)
{
	if (!Progress.bChallengeActive || Progress.ActiveChallengeStageId != StageId)
	{
		SetError(OutError, TEXT("没有匹配的挑战会话。"));
		return false;
	}
	Progress.ClearedStageIds.Add(StageId);
	Progress.bChallengeActive = false;
	Progress.ActiveChallengeStageId = NAME_None;
	Progress.ActiveChallengeEncounterIndex = INDEX_NONE;
	Progress.bChallengeAutoBattle = false;
	const EGameXXKTrainingDifficulty Difficulty = DifficultyFromStageId(StageId);
	if (AreAllStagesCleared(Progress, Difficulty))
	{
		if (Difficulty == EGameXXKTrainingDifficulty::Normal)
		{
			Progress.UnlockedDifficultyIds.Add(DifficultyId(EGameXXKTrainingDifficulty::Hard));
		}
		else if (Difficulty == EGameXXKTrainingDifficulty::Hard)
		{
			Progress.UnlockedDifficultyIds.Add(DifficultyId(EGameXXKTrainingDifficulty::Hell));
		}
	}
	Progress.SelectedStageId = StageId;
	return true;
}

bool FGameXXKTrainingRules::StartTravel(FGameXXKTrainingProgress& Progress, const FName StageId, FString* OutError)
{
	if (Progress.bChallengeActive || !CanTravel(Progress, StageId, OutError))
	{
		if (Progress.bChallengeActive) SetError(OutError, TEXT("挑战进行中，暂不能切换游历。"));
		return false;
	}
	Progress.SelectedStageId = StageId;
	Progress.CurrentTravelStageId = StageId;
	Progress.bTravelActive = true;
	return true;
}

bool FGameXXKTrainingRules::ResolveTravelFailure(FGameXXKTrainingProgress& Progress)
{
	if (!Progress.bTravelActive || Progress.CurrentTravelStageId.IsNone())
	{
		return false;
	}
	++Progress.TravelFailures;
	if (!Progress.bRetryOnFailure)
	{
		Progress.CurrentTravelStageId = PreviousStageId(Progress.CurrentTravelStageId);
		Progress.SelectedStageId = Progress.CurrentTravelStageId;
	}
	return true;
}

FGameXXKTrainingReward FGameXXKTrainingRules::BuildTravelReward(const FName StageId)
{
	FGameXXKTrainingReward Reward;
	FGameXXKTrainingStageDefinition Definition;
	if (TryGetStageDefinition(StageId, Definition))
	{
		Reward.Gold = Definition.TravelGold;
		Reward.Experience = Definition.TravelExperience;
		// The normal 1-1 low-cost exception explicitly has no chest.
		Reward.ChestTier = EGameXXKTrainingRewardTier::None;
		Reward.bChestRolled = false;
	}
	return Reward;
}

FGameXXKTrainingReward FGameXXKTrainingRules::BuildChallengeReward(
	const FName StageId,
	const EGameXXKTrainingEncounterKind EncounterKind,
	const bool bChestRolled,
	const float TalentChestBonus)
{
	FGameXXKTrainingReward Reward;
	FGameXXKTrainingStageDefinition Definition;
	if (!TryGetStageDefinition(StageId, Definition))
	{
		return Reward;
	}
	const float Bonus = FMath::Max(0.0f, TalentChestBonus);
	Reward.Gold = Definition.TravelGold * (EncounterKind == EGameXXKTrainingEncounterKind::Boss ? 3 : 1);
	Reward.Experience = Definition.TravelExperience * (EncounterKind == EGameXXKTrainingEncounterKind::Boss ? 3 : 1);
	Reward.bChestRolled = bChestRolled && Bonus >= 0.0f;
	if (Reward.bChestRolled)
	{
		Reward.ChestTier = EncounterKind == EGameXXKTrainingEncounterKind::Boss
			? EGameXXKTrainingRewardTier::AdvancedChest
			: EGameXXKTrainingRewardTier::NormalChest;
	}
	return Reward;
}

FText FGameXXKTrainingRules::BuildStageTooltip(const FGameXXKTrainingProgress& Progress, const FName StageId)
{
	FGameXXKTrainingStageDefinition Definition;
	if (!TryGetStageDefinition(StageId, Definition))
	{
		return FText::FromString(TEXT("历练关卡不存在"));
	}
	const bool bCleared = IsStageCleared(Progress, StageId);
	const bool bChallengeable = CanChallenge(Progress, StageId);
	const FString Status = bCleared
		? TEXT("已通关，可游历")
		: bChallengeable ? TEXT("可挑战，首次通关后解锁游历") : TEXT("已锁定，请先完成前置关卡");
	const FString NormalNames = FString::JoinBy(Definition.NormalEnemyPool, TEXT("、"), [](const FName& Id)
	{
		return EnemyDisplayName(Id).ToString();
	});
	const FString EliteNames = FString::JoinBy(Definition.EliteEnemyPool, TEXT("、"), [](const FName& Id)
	{
		return EnemyDisplayName(Id).ToString();
	});
	return FText::FromString(FString::Printf(TEXT("%s\n%s\n普通：%s\n次级精英：%s\n首领：%s"),
		*Definition.DisplayName.ToString(), *Status, *NormalNames, *EliteNames, *Definition.BossDisplayName.ToString()));
}

void FGameXXKTrainingRules::SetError(FString* OutError, const TCHAR* Message)
{
	if (OutError)
	{
		*OutError = Message;
	}
}
