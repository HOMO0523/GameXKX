#include "GameXXKTrainingRules.h"

#include "GameXXKCardBattleAdapter.h"
#include "GameXXKEnemyCatalog.h"
#include "GameXXKMVPRules.h"
#include "GameXXKRelicRules.h"
#include "GameXXKRouteEconomyRules.h"

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
			if (StageNumber == 1) return TEXT("Enemy.Ch1.IronfeatherRooster");
			if (StageNumber == 2) return TEXT("Enemy.Ch1.BluehornGoatKing");
			return TEXT("Enemy.Ch1.MoneyRat");
		}
		if (Chapter == 2)
		{
			if (StageNumber == 1) return TEXT("Enemy.Ch2.GraymaneWolfKing");
			if (StageNumber == 2) return TEXT("Enemy.Ch2.RedtuskBoarKing");
			return TEXT("Enemy.Ch2.BlackBear");
		}
		if (StageNumber == 1) return TEXT("Enemy.Ch3.WhiteApe");
		if (StageNumber == 2) return TEXT("Enemy.Ch3.SpiralHornDeer");
		return TEXT("Enemy.Ch3.Tiger");
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
				Stage.CombatLevel = (DifficultyIndexValue * FGameXXKTrainingRules::StagesPerDifficulty + StageNumber) * 5;
				Stage.StageId = FGameXXKTrainingRules::MakeStageId(Difficulty, StageNumber);
				Stage.DisplayName = FText::FromString(FString::Printf(
					TEXT("%s %d-%d"),
					Difficulty == EGameXXKTrainingDifficulty::Normal ? TEXT("普通")
						: Difficulty == EGameXXKTrainingDifficulty::Hard ? TEXT("困难") : TEXT("地狱"),
					Stage.Chapter,
					((StageNumber - 1) % 3) + 1));
				Stage.NormalEnemyPool = NormalPoolForChapter(Stage.Chapter);
				Stage.EliteEnemyPool = ElitePoolForChapter(Stage.Chapter);
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

	const TCHAR* const ApprovedFormationRows[27][7] = {
		{TEXT("R-G-C"), TEXT("W-R-G"), TEXT("W-C-R"), TEXT("G-W-C"), TEXT("R-B-G"), TEXT("W-B-C"), TEXT("W-I-C")},
		{TEXT("W-R-C"), TEXT("R-G-R"), TEXT("C-G-W"), TEXT("W-G-R"), TEXT("R-I-C"), TEXT("W-I-G"), TEXT("W-B-C")},
		{TEXT("W-R-G"), TEXT("C-R-G"), TEXT("W-C-R"), TEXT("R-G-C"), TEXT("W-I-R"), TEXT("C-B-G"), TEXT("I-M-B")},
		{TEXT("Ma-Wf-Bo"), TEXT("Wf-P-Bo"), TEXT("Ma-P-Bo"), TEXT("Ma-Wf-P"), TEXT("Ma-Rt-Bo"), TEXT("Wf-Rt-P"), TEXT("Wf-Gm-P")},
		{TEXT("Ma-Wf-P"), TEXT("Wf-Bo-P"), TEXT("Ma-Wf-Bo"), TEXT("Ma-Bo-P"), TEXT("Ma-Gm-P"), TEXT("Wf-Gm-Bo"), TEXT("Wf-Rt-P")},
		{TEXT("Wf-Wf-P"), TEXT("Ma-Bo-P"), TEXT("Ma-Wf-Bo"), TEXT("Wf-Bo-Bo"), TEXT("Ma-Gm-P"), TEXT("Wf-Rt-Bo"), TEXT("Gm-Bb-Rt")},
		{TEXT("S-C-T"), TEXT("V-C-T"), TEXT("V-S-T"), TEXT("V-S-C"), TEXT("S-D-T"), TEXT("V-D-C"), TEXT("V-A-T")},
		{TEXT("V-C-T"), TEXT("S-S-T"), TEXT("C-C-V"), TEXT("V-S-C"), TEXT("V-A-C"), TEXT("S-A-T"), TEXT("S-D-C")},
		{TEXT("V-S-C"), TEXT("S-C-T"), TEXT("V-S-T"), TEXT("V-C-T"), TEXT("V-A-C"), TEXT("S-D-T"), TEXT("A-Ti-D")},

		{TEXT("W-R-R"), TEXT("C-R-G"), TEXT("W-G-C"), TEXT("R-G-R"), TEXT("W-B-R"), TEXT("C-B-G"), TEXT("W-I-G")},
		{TEXT("W-R-C"), TEXT("C-G-G"), TEXT("W-G-R"), TEXT("C-R-R"), TEXT("W-I-R"), TEXT("C-I-G"), TEXT("C-B-W")},
		{TEXT("W-R-G"), TEXT("C-R-G"), TEXT("W-C-R"), TEXT("W-C-G"), TEXT("W-I-C"), TEXT("R-B-G"), TEXT("I-M-B")},
		{TEXT("Ma-Wf-P"), TEXT("Wf-Wf-Bo"), TEXT("Ma-Bo-P"), TEXT("Wf-Bo-P"), TEXT("Ma-Rt-P"), TEXT("Wf-Rt-Bo"), TEXT("Ma-Gm-P")},
		{TEXT("Ma-Wf-Bo"), TEXT("Wf-Wf-P"), TEXT("Ma-Wf-P"), TEXT("Ma-Bo-Bo"), TEXT("Ma-Gm-P"), TEXT("Wf-Gm-Bo"), TEXT("Wf-Rt-P")},
		{TEXT("Ma-Wf-P"), TEXT("Wf-Bo-P"), TEXT("Ma-Wf-Bo"), TEXT("Wf-Bo-Bo"), TEXT("Ma-Gm-P"), TEXT("Wf-Rt-P"), TEXT("Gm-Bb-Rt")},
		{TEXT("V-C-T"), TEXT("S-C-T"), TEXT("V-S-C"), TEXT("S-S-T"), TEXT("V-D-T"), TEXT("S-D-C"), TEXT("V-A-T")},
		{TEXT("V-S-T"), TEXT("C-C-V"), TEXT("S-C-T"), TEXT("V-S-C"), TEXT("V-A-C"), TEXT("S-A-T"), TEXT("S-D-C")},
		{TEXT("V-S-C"), TEXT("S-C-T"), TEXT("V-C-T"), TEXT("V-S-T"), TEXT("V-A-C"), TEXT("S-D-T"), TEXT("A-Ti-D")},

		{TEXT("W-R-R"), TEXT("C-R-R"), TEXT("W-G-R"), TEXT("C-G-R"), TEXT("W-B-R"), TEXT("C-B-G"), TEXT("W-I-C")},
		{TEXT("W-G-G"), TEXT("C-G-G"), TEXT("W-R-C"), TEXT("W-G-C"), TEXT("W-I-R"), TEXT("C-I-G"), TEXT("C-B-W")},
		{TEXT("W-R-G"), TEXT("C-R-G"), TEXT("W-C-R"), TEXT("W-C-G"), TEXT("W-I-C"), TEXT("C-B-G"), TEXT("I-M-B")},
		{TEXT("Wf-Wf-P"), TEXT("Ma-Wf-P"), TEXT("Wf-Bo-P"), TEXT("Ma-Bo-Bo"), TEXT("Ma-Rt-P"), TEXT("Wf-Rt-Bo"), TEXT("Ma-Gm-P")},
		{TEXT("Ma-Wf-Bo"), TEXT("Wf-Wf-P"), TEXT("Ma-Wf-P"), TEXT("Wf-Bo-Bo"), TEXT("Ma-Gm-P"), TEXT("Wf-Gm-Bo"), TEXT("Wf-Rt-P")},
		{TEXT("Ma-Wf-P"), TEXT("Wf-Bo-P"), TEXT("Ma-Wf-Bo"), TEXT("Wf-Wf-Bo"), TEXT("Ma-Gm-P"), TEXT("Wf-Rt-P"), TEXT("Gm-Bb-Rt")},
		{TEXT("V-C-T"), TEXT("S-S-T"), TEXT("V-S-C"), TEXT("S-C-T"), TEXT("V-D-C"), TEXT("S-D-T"), TEXT("V-A-T")},
		{TEXT("V-S-T"), TEXT("C-C-V"), TEXT("S-C-T"), TEXT("V-S-C"), TEXT("V-A-C"), TEXT("S-A-T"), TEXT("S-D-C")},
		{TEXT("V-S-C"), TEXT("S-C-T"), TEXT("V-C-T"), TEXT("V-S-T"), TEXT("V-A-C"), TEXT("S-D-T"), TEXT("A-Ti-D")}
	};

	FName ResolveFormationToken(const int32 Chapter, const FString& Token)
	{
		if (Chapter == 1)
		{
			if (Token == TEXT("R")) return TEXT("Enemy.Ch1.Rooster");
			if (Token == TEXT("G")) return TEXT("Enemy.Ch1.Goat");
			if (Token == TEXT("W")) return TEXT("Enemy.Ch1.Weasel");
			if (Token == TEXT("C")) return TEXT("Enemy.Ch1.Civet");
			if (Token == TEXT("I")) return TEXT("Enemy.Ch1.IronfeatherRooster");
			if (Token == TEXT("B")) return TEXT("Enemy.Ch1.BluehornGoatKing");
			if (Token == TEXT("M")) return TEXT("Enemy.Ch1.MoneyRat");
		}
		else if (Chapter == 2)
		{
			if (Token == TEXT("Wf")) return TEXT("Enemy.Ch2.GrayWolf");
			if (Token == TEXT("Bo")) return TEXT("Enemy.Ch2.Boar");
			if (Token == TEXT("Ma")) return TEXT("Enemy.Ch2.Macaque");
			if (Token == TEXT("P")) return TEXT("Enemy.Ch2.Porcupine");
			if (Token == TEXT("Gm")) return TEXT("Enemy.Ch2.GraymaneWolfKing");
			if (Token == TEXT("Rt")) return TEXT("Enemy.Ch2.RedtuskBoarKing");
			if (Token == TEXT("Bb")) return TEXT("Enemy.Ch2.BlackBear");
		}
		else if (Chapter == 3)
		{
			if (Token == TEXT("S")) return TEXT("Enemy.Ch3.VenomSnake");
			if (Token == TEXT("C")) return TEXT("Enemy.Ch3.Wildcat");
			if (Token == TEXT("V")) return TEXT("Enemy.Ch3.Vulture");
			if (Token == TEXT("T")) return TEXT("Enemy.Ch3.GiantToad");
			if (Token == TEXT("A")) return TEXT("Enemy.Ch3.WhiteApe");
			if (Token == TEXT("D")) return TEXT("Enemy.Ch3.SpiralHornDeer");
			if (Token == TEXT("Ti")) return TEXT("Enemy.Ch3.Tiger");
		}
		return NAME_None;
	}

	TArray<FName> ParseApprovedFormation(const int32 Chapter, const TCHAR* Notation)
	{
		TArray<FString> Tokens;
		FString(Notation).ParseIntoArray(Tokens, TEXT("-"), true);
		TArray<FName> Formation;
		for (const FString& Token : Tokens)
		{
			Formation.Add(ResolveFormationToken(Chapter, Token));
		}
		return Formation;
	}

	FName ResolveOpeningIntent(
		const TArray<FName>& Formation,
		const int32 SlotIndex)
	{
		const FName EnemyId = Formation.IsValidIndex(SlotIndex) ? Formation[SlotIndex] : NAME_None;
		int32 TotalCopies = 0;
		for (const FName CandidateId : Formation)
		{
			TotalCopies += CandidateId == EnemyId ? 1 : 0;
		}
		int32 EarlierCopies = 0;
		for (int32 Index = 0; Index < SlotIndex; ++Index)
		{
			EarlierCopies += Formation[Index] == EnemyId ? 1 : 0;
		}
		if (EnemyId == TEXT("Enemy.Ch1.Rooster"))
		{
			if (TotalCopies > 1 && EarlierCopies == 0) return TEXT("Crow");
			return Formation.Contains(TEXT("Enemy.Ch1.Weasel")) ? FName(TEXT("DoublePeck")) : FName(TEXT("Peck"));
		}
		if (EnemyId == TEXT("Enemy.Ch1.Goat")) return TotalCopies > 1 && EarlierCopies > 0 ? FName(TEXT("Stomp")) : FName(TEXT("Horn"));
		if (EnemyId == TEXT("Enemy.Ch1.Weasel")) return TEXT("Harass");
		if (EnemyId == TEXT("Enemy.Ch1.Civet")) return TEXT("Feint");
		if (EnemyId == TEXT("Enemy.Ch1.IronfeatherRooster")) return TEXT("RapidPeck");
		if (EnemyId == TEXT("Enemy.Ch1.BluehornGoatKing")) return TEXT("Pierce");
		if (EnemyId == TEXT("Enemy.Ch1.MoneyRat")) return TEXT("GreedyMark");
		if (EnemyId == TEXT("Enemy.Ch2.GrayWolf")) return TotalCopies > 1 && EarlierCopies == 0 ? FName(TEXT("CallPack")) : FName(TEXT("Bite"));
		if (EnemyId == TEXT("Enemy.Ch2.Boar"))
		{
			if (TotalCopies > 1 && EarlierCopies > 0) return TEXT("Bristle");
			return Formation.Contains(TEXT("Enemy.Ch2.GrayWolf")) || Formation.Contains(TEXT("Enemy.Ch2.GraymaneWolfKing"))
				? FName(TEXT("ArmorBreakCharge")) : FName(TEXT("Tusk"));
		}
		if (EnemyId == TEXT("Enemy.Ch2.Macaque")) return TEXT("Snatch");
		if (EnemyId == TEXT("Enemy.Ch2.Porcupine")) return TEXT("Quill");
		if (EnemyId == TEXT("Enemy.Ch2.GraymaneWolfKing")) return TEXT("HuntMark");
		if (EnemyId == TEXT("Enemy.Ch2.RedtuskBoarKing")) return TEXT("Earthquake");
		if (EnemyId == TEXT("Enemy.Ch2.BlackBear")) return TEXT("Rend");
		if (EnemyId == TEXT("Enemy.Ch3.VenomSnake")) return TotalCopies > 1 && EarlierCopies > 0 ? FName(TEXT("Coil")) : FName(TEXT("VenomBite"));
		if (EnemyId == TEXT("Enemy.Ch3.Wildcat")) return TotalCopies > 1 && EarlierCopies == 0 ? FName(TEXT("Stalk")) : FName(TEXT("Rake"));
		if (EnemyId == TEXT("Enemy.Ch3.Vulture")) return TEXT("Gaze");
		if (EnemyId == TEXT("Enemy.Ch3.GiantToad")) return TEXT("PoisonFog");
		if (EnemyId == TEXT("Enemy.Ch3.WhiteApe")) return TEXT("ThrowRock");
		if (EnemyId == TEXT("Enemy.Ch3.SpiralHornDeer")) return TEXT("TerrainBless");
		if (EnemyId == TEXT("Enemy.Ch3.Tiger")) return TEXT("MarkPrey");
		return NAME_None;
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
	(void)bTravelMode;
	const int32 RowIndex = DifficultyIndex(Stage.Difficulty) * StagesPerDifficulty + Stage.StageNumber - 1;
	if (RowIndex < 0 || RowIndex >= UE_ARRAY_COUNT(ApprovedFormationRows))
	{
		return Encounters;
	}
	Encounters.Reserve(7);
	for (int32 EncounterIndex = 0; EncounterIndex < 7; ++EncounterIndex)
	{
		FGameXXKTrainingEncounterDefinition Encounter;
		Encounter.Kind = EncounterIndex < 4
			? EGameXXKTrainingEncounterKind::Normal
			: EncounterIndex < 6
				? EGameXXKTrainingEncounterKind::Elite
				: EGameXXKTrainingEncounterKind::Boss;
		Encounter.CombatLevel = Stage.CombatLevel;
		Encounter.EnemyDefinitionIds = ParseApprovedFormation(
			Stage.Chapter,
			ApprovedFormationRows[RowIndex][EncounterIndex]);
		if (Encounter.EnemyDefinitionIds.Num() != 3
			|| Encounter.EnemyDefinitionIds.Contains(NAME_None))
		{
			return {};
		}
		Encounter.EnemyDefinitionId = Encounter.EnemyDefinitionIds[1];
		Encounter.DisplayName = FText::FromString(JoinNames(Encounter.EnemyDefinitionIds));
		int64 FirstPhaseHealth = 0;
		for (int32 SlotIndex = 0; SlotIndex < Encounter.EnemyDefinitionIds.Num(); ++SlotIndex)
		{
			FGameXXKTrainingEnemySlotDefinition& Slot = Encounter.EnemySlots.AddDefaulted_GetRef();
			Slot.EnemyDefinitionId = Encounter.EnemyDefinitionIds[SlotIndex];
			Slot.OpeningIntentId = ResolveOpeningIntent(Encounter.EnemyDefinitionIds, SlotIndex);
			FirstPhaseHealth += FGameXXKEnemyCatalog::ComputeStats(
				Slot.EnemyDefinitionId,
				Stage.CombatLevel).MaxHP;
		}
		Encounter.BaseHealth = static_cast<int32>(FMath::Clamp<int64>(FirstPhaseHealth, 1, MAX_int32));
		Encounters.Add(MoveTemp(Encounter));
	}
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
	Progress.ActiveChallengeRouteNodeId = INDEX_NONE;
	Progress.ChallengeRouteNodeEncounterIndices.Reset();
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
	Progress.ActiveChallengeRouteNodeId = INDEX_NONE;
	Progress.ChallengeRouteNodeEncounterIndices.Reset();
	if (Stage.StageNumber == StagesPerDifficulty
		&& Stage.Difficulty != EGameXXKTrainingDifficulty::Hell)
	{
		const EGameXXKTrainingDifficulty Next = DifficultyFromIndex(DifficultyIndex(Stage.Difficulty) + 1);
		Progress.UnlockedDifficultyIds.Add(DifficultyId(Next));
	}
	// A successful Challenge immediately becomes the new desktop Travel target.
	// This is deliberately the same restart contract as pressing Travel again:
	// encounter zero, a fresh walking delay, and a newly initialized full-health
	// party runtime owned by the subsystem.
	Progress.SelectedStageId = StageId;
	Progress.CurrentTravelStageId = StageId;
	Progress.bTravelActive = true;
	Progress.ActiveTravelEncounterIndex = 0;
	Progress.bTravelPausedAtDefeat = false;
	return true;
}

void FGameXXKTrainingRules::GenerateChallengeRouteMap(FGameXXKRuntimeState& State, const FName StageId, const int32 Seed)
{
	State.bHasGeneratedRouteMap = false;
	State.RouteMapNodes.Reset();
	State.RouteMapEdges.Reset();
	State.VisitedRouteNodeIds.Reset();
	State.ReachableRouteNodeIds.Reset();
	State.CurrentRouteNodeId = INDEX_NONE;
	State.PendingRouteNodeId = INDEX_NONE;
	State.DungeonNodeIndex = 0;
	State.BattleEntryCheckpoint = FGameXXKBattleEntryCheckpoint{};
	State.Training.ChallengeRouteNodeEncounterIndices.Reset();
	State.Training.ActiveChallengeRouteNodeId = INDEX_NONE;

	const TArray<FGameXXKTrainingEncounterDefinition> Encounters = BuildEncounterSequence(StageId, false);
	if (Encounters.Num() < 7)
	{
		return;
	}

	FString Error;
	if (!FGameXXKCardBattleAdapter::EnsureCardRunInitialized(State, &Error))
	{
		return;
	}
	// Clear route-local progress without touching the desktop party selection
	// (Challenge never accepts or alters the town quest/party).
	State.CardRun.PendingEvent = FGameXXKPendingRouteEvent();
	State.CardRun.RouteMerchant = FGameXXKRouteMerchantState();
	FGameXXKRelicRules::ClearRouteRelics(State);
	FGameXXKCardBattleAdapter::ClearActiveCardBattle(State);
	FGameXXKRouteEconomyRules::ClearRouteEconomy(State.CardRun);
	State.CardRun.RouteProgress.CurrentChapter = 1;
	State.CardRun.bLoadoutLockedForRoute = true;
	State.bDungeonActive = true;
	State.Screen = EGameXXKScreen::DungeonMap;
	State.CurrentMapId = TEXT("DesktopTrainingHUD");
	State.TownPanelMode = EGameXXKTownPanelMode::None;

	// The Challenge map is the canonical generated route map: Start node, mixed
	// Battle / Elite / Event / Camp / Chest / Merchant layers, and a final Boss.
	// Players keep the full route economy, one-time node rewards and tiered
	// battle-reward offers; only battle-entry enemies are authored by Training.
	UGameXXKMVPRules::GenerateRouteMapForSeed(State, Seed);
	State.CardRun.RouteRandomSeed = State.RouteSeed;
	if (!FGameXXKRouteEconomyRules::InitializeRoute(State.CardRun, 60, &Error))
	{
		State.bHasGeneratedRouteMap = false;
		State.RouteMapNodes.Reset();
		State.RouteMapEdges.Reset();
		State.ReachableRouteNodeIds.Reset();
		return;
	}

	// The bottom campfire is the player's current camp, not a selectable reward
	// room.  Training challenges therefore begin with Start already visited and
	// expose its outgoing rooms immediately.  The marker remains in the graph so
	// the route map can render the point of origin, but it can never dispatch.
	const FGameXXKRouteMapNode* StartNode = State.RouteMapNodes.FindByPredicate(
		[](const FGameXXKRouteMapNode& Node)
		{
			return Node.NodeKind == EGameXXKNodeKind::Start;
		});
	if (!StartNode || StartNode->OutgoingNodeIds.IsEmpty())
	{
		State.bHasGeneratedRouteMap = false;
		State.RouteMapNodes.Reset();
		State.RouteMapEdges.Reset();
		State.VisitedRouteNodeIds.Reset();
		State.ReachableRouteNodeIds.Reset();
		return;
	}
	State.VisitedRouteNodeIds = {StartNode->NodeId};
	State.ReachableRouteNodeIds.Reset();
	for (const int32 OutgoingNodeId : StartNode->OutgoingNodeIds)
	{
		State.ReachableRouteNodeIds.AddUnique(OutgoingNodeId);
	}
	State.CurrentRouteNodeId = State.ReachableRouteNodeIds[0];
	State.PendingRouteNodeId = INDEX_NONE;
	State.DungeonNodeIndex = State.VisitedRouteNodeIds.Num();

	// Authored challenge sequence is [0 N, 1 N, 2 E0, 3 N, 4 E1, 5 N, 6 B].
	// Generated battle-kind nodes consume those encounters in map order and
	// cycle when the generated map offers more nodes than the authored pool.
	const int32 NormalEncounterIndices[4] = {0, 1, 3, 5};
	const int32 EliteEncounterIndices[2] = {2, 4};
	int32 NormalCursor = 0;
	int32 EliteCursor = 0;
	for (const FGameXXKRouteMapNode& Node : State.RouteMapNodes)
	{
		if (Node.NodeKind == EGameXXKNodeKind::Battle)
		{
			State.Training.ChallengeRouteNodeEncounterIndices.Add(
				Node.NodeId,
				NormalEncounterIndices[NormalCursor % 4]);
			++NormalCursor;
		}
		else if (Node.NodeKind == EGameXXKNodeKind::Elite)
		{
			State.Training.ChallengeRouteNodeEncounterIndices.Add(
				Node.NodeId,
				EliteEncounterIndices[EliteCursor % 2]);
			++EliteCursor;
		}
		else if (Node.NodeKind == EGameXXKNodeKind::Boss)
		{
			State.Training.ChallengeRouteNodeEncounterIndices.Add(Node.NodeId, 6);
		}
	}
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
	FGameXXKTrainingStageDefinition Stage;
	if (!TryGetStageDefinition(Progress.CurrentTravelStageId, Stage))
	{
		return false;
	}
	const EGameXXKEnemyDifficulty EnemyDifficulty = Stage.Difficulty == EGameXXKTrainingDifficulty::Hell
		? EGameXXKEnemyDifficulty::Hell
		: Stage.Difficulty == EGameXXKTrainingDifficulty::Hard
			? EGameXXKEnemyDifficulty::Hard
			: EGameXXKEnemyDifficulty::Normal;
	OutRuntime.StageId = Progress.CurrentTravelStageId;
	OutRuntime.EncounterIndex = Progress.ActiveTravelEncounterIndex;
	OutRuntime.EncounterKind = Encounter.Kind;
	OutRuntime.Phase = EGameXXKTrainingTravelPhase::Walking;
	OutRuntime.WalkStep = 0;
	OutRuntime.WalkStepsRequired = TravelEncounterSpawnDelaySeconds;
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
		Unit.HP = Unit.MaxHP;
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
	for (const FGameXXKTrainingEnemySlotDefinition& Slot : Encounter.EnemySlots)
	{
		const FName EnemyId = Slot.EnemyDefinitionId;
		if (EnemyId.IsNone() || OutRuntime.Enemies.Num() >= 3)
		{
			continue;
		}
		FGameXXKTrainingTravelEnemyRuntime Enemy;
		Enemy.EnemyDefinitionId = EnemyId;
		const FGameXXKEnemyComputedStats Stats = FGameXXKEnemyCatalog::ComputeStats(
			EnemyId,
			Encounter.CombatLevel);
		const FGameXXKEnemyDefinition* Definition = FGameXXKEnemyCatalog::Find(EnemyId);
		const int32 TotalPhases = Definition
			? FGameXXKEnemyCatalog::ResolveTotalPhases(Definition->Tier, EnemyDifficulty)
			: 1;
		Enemy.MaxHP = static_cast<int32>(FMath::Clamp<int64>(
			static_cast<int64>(FMath::Max(1, Stats.MaxHP)) * TotalPhases,
			1,
			MAX_int32));
		Enemy.HP = Enemy.MaxHP;
		Enemy.Attack = FMath::Max(1, Stats.Attack);
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

		// Keep party identities/stats and deterministic turn cursors; the next
		// encounter initializer intentionally restores every member to MaxHP.
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
	const float EffectiveChance = ResolveRelativeChestChance(Chance, TalentChestDropBonus);
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
	const float Chance = ResolveRelativeChestChance(
		ChestChanceForEncounter(Stage, EncounterKind),
		TalentChestDropBonus);
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
	const float Chance = ResolveRelativeChestChance(
		ChestChanceForEncounter(Stage, EncounterKind),
		TalentChestDropBonus);
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

float FGameXXKTrainingRules::ResolveRelativeChestChance(
	const float BaseChance,
	const float RelativeTalentBonus)
{
	return FMath::Clamp(
		FMath::Clamp(BaseChance, 0.0f, 1.0f)
			* (1.0f + FMath::Max(0.0f, RelativeTalentBonus)),
		0.0f,
		1.0f);
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
