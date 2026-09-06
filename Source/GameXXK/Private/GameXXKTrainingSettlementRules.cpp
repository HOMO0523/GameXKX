#include "GameXXKTrainingSettlementRules.h"
#include "GameXXKMVPRules.h"
#include "GameXXKCompanionCatalog.h"
#include "GameXXKCompanionRules.h"
#include "GameXXKEquipmentRules.h"
#include "GameXXKRouteSettlementRules.h"

namespace
{
	bool Fail(FString* Error, const TCHAR* Message) { if (Error) *Error = Message; return false; }
	FText NpcName(const FName Id)
	{
		if (Id == TEXT("Npc.TusiChief")) return FText::FromString(TEXT("土司首领"));
		if (Id == TEXT("Npc.SongJinBao")) return FText::FromString(TEXT("宋金宝"));
		if (Id == TEXT("Npc.YueBai")) return FText::FromString(TEXT("月白"));
		if (Id == TEXT("Npc.ZhouGuangZu")) return FText::FromString(TEXT("周光祖"));
		if (Id == TEXT("Npc.JinGui")) return FText::FromString(TEXT("金贵"));
		if (Id == TEXT("Npc.QiongMeiEr")) return FText::FromString(TEXT("琼梅儿"));
		return FText::FromString(TEXT("NPC"));
	}
	void Accumulate(int64& Total, int64 Value) { Total += FMath::Min<int64>(FMath::Max<int64>(0, Value), MAX_int64 - Total); }
	int64 TotalExperience(const int32 Level, const int32 Experience, const bool bCompanion)
	{
		int64 Total = FMath::Max(0, Experience);
		for (int32 L = 1; L < FMath::Clamp(Level, 1, 100); ++L)
			Total += bCompanion ? FGameXXKCompanionRules::GetExperienceRequiredForNextLevel(L)
				: UGameXXKMVPRules::GetPlayerExperienceRequiredForNextLevel(L);
		return Total;
	}
	void AddMember(FGameXXKTrainingSettlementReceipt& Receipt, const FName Id, const FText& Name,
		const int32 BeforeLevel, const int32 BeforeXp, const int32 AfterLevel, const int32 AfterXp,
		const bool bCompanion = false)
	{
		auto& Member = Receipt.Members.AddDefaulted_GetRef();
		Member.MemberId = Id; Member.DisplayName = Name;
		Member.LevelBefore = BeforeLevel; Member.LevelAfter = AfterLevel;
		Member.ExperienceBefore = BeforeXp; Member.ExperienceAfter = AfterXp;
		Member.ExperienceGained = static_cast<int32>(FMath::Clamp<int64>(
			TotalExperience(AfterLevel, AfterXp, bCompanion) - TotalExperience(BeforeLevel, BeforeXp, bCompanion), 0, MAX_int32));
	}
}

FGameXXKBattleSessionStats FGameXXKTrainingSettlementRules::CaptureBattleStats(const FGameXXKCardBattleRuntime& Battle)
{
	FGameXXKBattleSessionStats Stats = Battle.SessionStats;
	Stats.Rounds = FMath::Max(0, Battle.RoundNumber);
	Stats.PartyDamageDealt = Stats.PartyDamageTaken = Stats.HealingDone = Stats.ArmorGenerated = 0;
	Stats.SurvivingPartyUnits = 0; Stats.PartyEndingHealth = Stats.PartyEndingMaxHealth = 0;
	for (const auto& Unit : Battle.Units)
	{
		if (Unit.Side == EGameXXKCardTargetSide::Enemy) Accumulate(Stats.PartyDamageDealt, Unit.SettlementHealthLost);
		if (Unit.Side != EGameXXKCardTargetSide::Party) continue;
		Accumulate(Stats.PartyDamageTaken, Unit.SettlementHealthLost);
		Accumulate(Stats.HealingDone, Unit.SettlementHealingReceived);
		Accumulate(Stats.ArmorGenerated, Unit.SettlementArmorGenerated);
		Stats.SurvivingPartyUnits += Unit.bLiving ? 1 : 0;
		Stats.PartyEndingHealth += FMath::Max(0, Unit.HP);
		Stats.PartyEndingMaxHealth += FMath::Max(0, Unit.MaxHP);
	}
	return Stats;
}

bool FGameXXKTrainingSettlementRules::CaptureAppliedResult(const FGameXXKRuntimeState& Before,
	FGameXXKRuntimeState& After, const FGameXXKTrainingReward& Reward, FString* OutError)
{
	if (After.Training.PendingSettlement.ReceiptId.IsValid()) return ValidatePending(After, OutError);
	FGameXXKTrainingStageDefinition Stage;
	if (!Before.Training.bChallengeActive
		|| Before.CardRun.ActiveBattle.Phase != EGameXXKCardBattlePhase::Victory
		|| !FGameXXKTrainingRules::TryGetStageDefinition(Before.Training.ActiveChallengeStageId, Stage)
		|| !After.Training.ClearedStageIds.Contains(Stage.StageId)
		|| After.PlayerGold < Before.PlayerGold)
		return Fail(OutError, TEXT("通关结果与当前关卡不一致，尚未生成结算。"));
	FGameXXKRouteSettlementReceipt RouteReceipt;
	if (!FGameXXKRouteSettlementRules::Preview(Before, EGameXXKRouteTerminalOutcome::Cleared, RouteReceipt, OutError)) return false;
	FGameXXKTrainingSettlementReceipt Receipt;
	Receipt.ReceiptId = FGuid::NewGuid(); Receipt.StageId = Stage.StageId; Receipt.StageDisplayName = Stage.DisplayName;
	Receipt.Gold = After.PlayerGold - Before.PlayerGold;
	Receipt.RouteGold = RouteReceipt.PermanentGoldAward;
	Receipt.SourceTravelMoney = RouteReceipt.SourceTravelMoney;
	Receipt.Experience = Reward.Experience;
	Receipt.bFirstClear = !Before.Training.ClearedStageIds.Contains(Stage.StageId);
	Receipt.bUnlockedNextDifficulty = After.Training.UnlockedDifficultyIds.Num() > Before.Training.UnlockedDifficultyIds.Num();
	Receipt.Stats = CaptureBattleStats(Before.CardRun.ActiveBattle);
	for (const auto& CandidateStage : FGameXXKTrainingRules::GetStageDefinitions())
	{
		if (CandidateStage.StageId != Stage.StageId
			&& !FGameXXKTrainingRules::CanChallenge(Before.Training, CandidateStage.StageId)
			&& FGameXXKTrainingRules::CanChallenge(After.Training, CandidateStage.StageId))
		{ Receipt.UnlockedStageId = CandidateStage.StageId; break; }
	}
	for (const auto& Token : After.Training.OwnedChestTokens)
	{
		if (Before.Training.OwnedChestTokens.ContainsByPredicate([&Token](const auto& Old){return Old.AcquisitionOrdinal == Token.AcquisitionOrdinal;})) continue;
		Receipt.NormalChestCount += Token.Tier == EGameXXKTrainingRewardTier::NormalChest ? 1 : 0;
		Receipt.AdvancedChestCount += Token.Tier == EGameXXKTrainingRewardTier::AdvancedChest ? 1 : 0;
		Receipt.ChestItemLevel = FMath::Max(Receipt.ChestItemLevel, Token.SourceItemLevel);
	}
	AddMember(Receipt, FGameXXKEquipmentRules::HeroCharacterId(), FText::FromString(TEXT("主角")), Before.PlayerLevel, Before.PlayerXP, After.PlayerLevel, After.PlayerXP);
	const FName CompanionId = Before.CardRun.PartySelection.ActivePermanentCompanionInstanceId;
	const auto* OldCompanion = Before.CardRun.CompanionRoster.PermanentCompanions.FindByPredicate([CompanionId](const auto& C){return C.InstanceId == CompanionId;});
	const auto* NewCompanion = After.CardRun.CompanionRoster.PermanentCompanions.FindByPredicate([CompanionId](const auto& C){return C.InstanceId == CompanionId;});
	if (!OldCompanion || !NewCompanion) return Fail(OutError, TEXT("结算缺少出战伙伴的成长记录。"));
	AddMember(Receipt, CompanionId, FText::FromString(FGameXXKCompanionRules::GetCompanionDisplayName(NewCompanion->Role, NewCompanion->NameSeed)), OldCompanion->Level, OldCompanion->Experience, NewCompanion->Level, NewCompanion->Experience, true);
	const FName NpcId = Before.CardRun.PartySelection.QuestNpc.NpcId;
	const auto* Npc = FGameXXKCompanionCatalog::FindQuestNpcDefinition(NpcId);
	const auto* OldNpc = Before.CardRun.PartySelection.QuestNpcProgressions.Find(NpcId);
	const auto* NewNpc = After.CardRun.PartySelection.QuestNpcProgressions.Find(NpcId);
	if (!Npc || !OldNpc || !NewNpc) return Fail(OutError, TEXT("结算缺少出战NPC的成长记录。"));
	AddMember(Receipt, NpcId, NpcName(NpcId), OldNpc->Level, OldNpc->Experience, NewNpc->Level, NewNpc->Experience);
	After.Training.PendingSettlement = MoveTemp(Receipt);
	After.Training.LastAppliedSettlementId = After.Training.PendingSettlement.ReceiptId;
	After.Training.bTravelActive = false;
	After.Training.ActiveTravelEncounterIndex = INDEX_NONE;
	After.Training.bTravelPausedAtDefeat = false;
	After.Training.TravelLastUpdatedUnixSeconds = 0;
	return ValidatePending(After, OutError);
}

bool FGameXXKTrainingSettlementRules::ValidatePending(const FGameXXKRuntimeState& State, FString* OutError)
{
	const auto& R = State.Training.PendingSettlement;
	if (!R.ReceiptId.IsValid())
	{
		return R.StageId.IsNone() && R.Gold == 0 && R.Experience == 0 && R.Members.IsEmpty()
			? true : Fail(OutError, TEXT("无效的通关凭据包含未确认数据。"));
	}
	FGameXXKTrainingStageDefinition Stage;
	if (R.ReceiptId != State.Training.LastAppliedSettlementId || !FGameXXKTrainingRules::TryGetStageDefinition(R.StageId, Stage)
		|| !State.Training.ClearedStageIds.Contains(R.StageId) || State.Screen != EGameXXKScreen::Town
		|| State.CurrentMapId != TEXT("DesktopTrainingHUD") || State.bDungeonActive || State.Training.bChallengeActive || State.Training.bTravelActive
		|| R.Gold < 0 || R.RouteGold < 0 || R.RouteGold > R.Gold || R.Experience < 0 || R.SourceTravelMoney < 0
		|| R.NormalChestCount < 0 || R.AdvancedChestCount < 0 || R.ChestItemLevel < 0 || R.Members.Num() != 3
		|| R.Stats.Rounds < 0 || R.Stats.ActiveCardsPlayed < 0 || R.Stats.PartyDamageDealt < 0 || R.Stats.PartyDamageTaken < 0
		|| R.Stats.HealingDone < 0 || R.Stats.ArmorGenerated < 0 || R.Stats.SurvivingPartyUnits < 0 || R.Stats.SurvivingPartyUnits > 3
		|| R.Stats.PartyEndingHealth < 0 || R.Stats.PartyEndingHealth > R.Stats.PartyEndingMaxHealth)
		return Fail(OutError, TEXT("通关凭据无效或已脱离本次结算状态。"));
	TSet<FName> Ids;
	for (const auto& Member : R.Members)
	{
		if (Member.MemberId.IsNone() || Ids.Contains(Member.MemberId) || Member.LevelBefore < 1 || Member.LevelAfter < Member.LevelBefore
			|| Member.LevelAfter > 100 || Member.ExperienceBefore < 0 || Member.ExperienceAfter < 0 || Member.ExperienceGained < 0)
			return Fail(OutError, TEXT("通关凭据的队伍成长记录无效。"));
		Ids.Add(Member.MemberId);
	}
	return true;
}

bool FGameXXKTrainingSettlementRules::Acknowledge(FGameXXKRuntimeState& State, FGuid ReceiptId, FString* OutError)
{
	if (!ReceiptId.IsValid() || State.Training.PendingSettlement.ReceiptId != ReceiptId || !ValidatePending(State, OutError))
		return Fail(OutError, TEXT("该通关结果已确认或不属于当前页面。"));
	State.Training.PendingSettlement = FGameXXKTrainingSettlementReceipt();
	return true;
}
