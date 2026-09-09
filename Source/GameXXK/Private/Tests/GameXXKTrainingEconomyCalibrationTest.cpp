#include "Misc/AutomationTest.h"
#include "GameXXKTalentCatalog.h"
#include "GameXXKTalentRules.h"
#include "GameXXKTrainingRules.h"
#include "GameXXKMVPRules.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKTrainingEconomyCalibrationTest,
	"GameXXK.Training.Economy.Gold105AndFortyFiveDayTalents",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTrainingEconomyCalibrationTest::RunTest(const FString& Parameters)
{
	const auto& Nodes = FGameXXKTalentCatalog::GetDefinitions();
	const FName StageId = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Hell, 9);
	const int32 BaseGold = FGameXXKTrainingRules::BuildTravelReward(StageId).Gold;
	TestEqual(TEXT("the approved 1.05 curve ends at 1083 gold"), BaseGold, 1083);
	FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
	State.PlayerGold = 0;
	State.Talents = FGameXXKTalentProgress();
	int64 Waves = 0;
	int64 Spent = 0;
	int32 PurchasedRanks = 0;
	int32 TargetRanks = 0;
	for (const auto& Node : Nodes) TargetRanks += Node.MaxRank;
	const auto Buy = [&](const FGameXXKTalentNodeDefinition& Node) -> bool
	{
		FGameXXKTalentProjection Projection;
		if (!FGameXXKTalentRules::BuildProjection(State.Talents, Projection)) return false;
		const int64 Cost = FGameXXKTalentRules::GetRankPrice(Node, State.Talents.NodeRanks.FindRef(Node.Id));
		const int32 WaveGold = FMath::RoundToInt(BaseGold * Projection.GetOnlineGoldMultiplier());
		const int64 Missing = FMath::Max<int64>(0, Cost - State.PlayerGold);
		const int64 WaitWaves = (Missing + WaveGold - 1) / WaveGold;
		Waves += WaitWaves;
		const int64 Funded = State.PlayerGold + WaitWaves * WaveGold;
		if (Funded > MAX_int32) return false;
		State.PlayerGold = static_cast<int32>(Funded);
		FGameXXKTalentPurchaseResult Result;
		if (!FGameXXKTalentRules::Purchase(State, Node.Id, Result))
		{
			AddError(FString::Printf(TEXT("calibration cannot buy %s: %s"), *Node.Id.ToString(), *Result.Message.ToString()));
			return false;
		}
		Spent += Result.Price;
		++PurchasedRanks;
		return true;
	};
	const auto Cheapest = [&](bool GoldOnly) -> const FGameXXKTalentNodeDefinition*
	{
		const FGameXXKTalentNodeDefinition* Best = nullptr;
		int64 BestPrice = MAX_int64;
		for (const auto& Node : Nodes)
		{
			const int32 Rank = State.Talents.NodeRanks.FindRef(Node.Id);
			if (Rank >= Node.MaxRank || (GoldOnly && Node.Effect != EGameXXKTalentEffect::OnlineGoldPercent)
				|| !FGameXXKTalentRules::ArePrerequisitesMet(State.Talents, Node)) continue;
			const int64 Price = FGameXXKTalentRules::GetRankPrice(Node, Rank);
			if (!Best || Price < BestPrice || (Price == BestPrice && Node.Id.ToString() < Best->Id.ToString()))
			{
				Best = &Node;
				BestPrice = Price;
			}
		}
		return Best;
	};
	const auto* Root = FGameXXKTalentCatalog::Find(TEXT("Talent.Root"));
	const auto* IdleEntry = FGameXXKTalentCatalog::Find(TEXT("Talent.Entry.IdleOffline"));
	if (!Root || !IdleEntry || !Buy(*Root) || !Buy(*IdleEntry)) return false;
	while (const auto* Next = Cheapest(true)) if (!Buy(*Next)) return false;
	const int64 GoldMaxWaves = Waves;
	FGameXXKTalentProjection Projection;
	FGameXXKTalentRules::BuildProjection(State.Talents, Projection);
	TestEqual(TEXT("online gold reaches its actual 350-percent cap"), Projection.OnlineGoldPercent, 350);
	while (PurchasedRanks < TargetRanks)
	{
		const auto* Next = Cheapest(false);
		if (!TestNotNull(TEXT("a remaining talent can be reached with the real prerequisite/capacity rules"), Next) || !Buy(*Next)) return false;
	}
	TestEqual(TEXT("the calibration covers the current complete tree"), Nodes.Num(), 453);
	TestEqual(TEXT("every authored rank is bought"), PurchasedRanks, 2229);
	TestEqual(TEXT("the authoritative full-tree bill matches calibration"), Spent, int64(1880754900));
	TestEqual(TEXT("the dynamic-income simulation matches the independent model"), Waves, int64(388128));
	const double Days = Waves * 10.0 / 86400.0;
	TestTrue(TEXT("the fixed benchmark stays within a quarter-day of 45 days"), FMath::Abs(Days - 45.0) < 0.25);

	const auto Json = MakeShared<FJsonObject>();
	Json->SetNumberField(TEXT("schema_version"), 1);
	Json->SetNumberField(TEXT("gold_growth"), 1.05);
	Json->SetNumberField(TEXT("price_base"), 229);
	Json->SetNumberField(TEXT("nodes"), Nodes.Num());
	Json->SetNumberField(TEXT("ranks"), PurchasedRanks);
	Json->SetNumberField(TEXT("total_gold"), static_cast<double>(Spent));
	Json->SetNumberField(TEXT("waves"), static_cast<double>(Waves));
	Json->SetNumberField(TEXT("days"), Days);
	Json->SetNumberField(TEXT("gold_talent_max_days"), GoldMaxWaves * 10.0 / 86400.0);
	TArray<TSharedPtr<FJsonValue>> Prices;
	for (int32 Tier = 0; Tier <= FGameXXKTalentRules::MaximumCostTier; ++Tier)
	{
		const auto Row = MakeShared<FJsonObject>();
		Row->SetNumberField(TEXT("tier"), Tier);
		Row->SetNumberField(TEXT("price"), static_cast<double>(FGameXXKTalentRules::GetPriceForCostTier(Tier)));
		Prices.Add(MakeShared<FJsonValueObject>(Row));
	}
	Json->SetArrayField(TEXT("prices"), Prices);
	TArray<TSharedPtr<FJsonValue>> Stages;
	for (const auto Difficulty : {EGameXXKTrainingDifficulty::Normal, EGameXXKTrainingDifficulty::Hard, EGameXXKTrainingDifficulty::Hell})
		for (int32 Index = 1; Index <= 9; ++Index)
		{
			const FName Id = FGameXXKTrainingRules::MakeStageId(Difficulty, Index);
			const auto Reward = FGameXXKTrainingRules::BuildTravelReward(Id);
			const auto Row = MakeShared<FJsonObject>();
			Row->SetStringField(TEXT("id"), Id.ToString());
			Row->SetNumberField(TEXT("gold"), Reward.Gold);
			Row->SetNumberField(TEXT("experience"), Reward.Experience);
			Stages.Add(MakeShared<FJsonValueObject>(Row));
		}
	Json->SetArrayField(TEXT("stages"), Stages);
	FString Text;
	FJsonSerializer::Serialize(Json, TJsonWriterFactory<>::Create(&Text));
	const FString Folder = FPaths::Combine(FPaths::ProjectDir(), TEXT("Saved/Diagnostics/TrainingEconomy105"));
	IFileManager::Get().MakeDirectory(*Folder, true);
	TestTrue(TEXT("the verified runtime budget is exported"), FFileHelper::SaveStringToFile(Text,
		*FPaths::Combine(Folder, TEXT("runtime-calibration.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
	return true;
}
#endif
