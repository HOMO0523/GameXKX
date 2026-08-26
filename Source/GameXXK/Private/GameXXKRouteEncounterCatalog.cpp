#include "GameXXKRouteEncounterCatalog.h"

namespace
{
	FGameXXKRouteEncounterChoiceDefinition AttributeChoice(const TCHAR* Label, EGameXXKRouteAttributeKind Kind, int32 Magnitude)
	{
		FGameXXKRouteEncounterChoiceDefinition Choice;
		Choice.Label = FText::FromString(Label);
		Choice.RewardKind = EGameXXKRouteEncounterRewardKind::RouteAttribute;
		Choice.AttributeKind = Kind;
		Choice.Magnitude = Magnitude;
		return Choice;
	}

	FGameXXKRouteEncounterChoiceDefinition NpcSupportChoice(const TCHAR* Label, const TCHAR* QuestNpcId)
	{
		FGameXXKRouteEncounterChoiceDefinition Choice;
		Choice.Label = FText::FromString(Label);
		Choice.RewardKind = EGameXXKRouteEncounterRewardKind::TemporaryNpcSupport;
		Choice.QuestNpcId = FName(QuestNpcId);
		return Choice;
	}

	FGameXXKRouteEncounterChoiceDefinition RelicChoice(const TCHAR* Label)
	{
		FGameXXKRouteEncounterChoiceDefinition Choice;
		Choice.Label = FText::FromString(Label);
		Choice.RewardKind = EGameXXKRouteEncounterRewardKind::Relic;
		return Choice;
	}

	FGameXXKRouteEncounterDefinition Event(
		const TCHAR* Id,
		const TCHAR* Speaker,
		const TCHAR* Body,
		const TCHAR* EventNpcId,
		FGameXXKRouteEncounterChoiceDefinition A,
		FGameXXKRouteEncounterChoiceDefinition B)
	{
		FGameXXKRouteEncounterDefinition Definition;
		Definition.Id = FName(Id);
		Definition.Kind = EGameXXKRouteEncounterKind::Event;
		Definition.Title = FText::FromString(TEXT("山路奇遇"));
		Definition.Speaker = FText::FromString(Speaker);
		Definition.Body = FText::FromString(Body);
		Definition.EventNpcId = FName(EventNpcId);
		Definition.Choices = {
			MoveTemp(A),
			MoveTemp(B),
			AttributeChoice(TEXT("稳住根基：最大气血+5"), EGameXXKRouteAttributeKind::MaxHealth, 5)};
		return Definition;
	}

	FGameXXKRouteEncounterDefinition Chest(const TCHAR* Id, const TCHAR* Speaker, const TCHAR* Body)
	{
		FGameXXKRouteEncounterDefinition Definition;
		Definition.Id = FName(Id);
		Definition.Kind = EGameXXKRouteEncounterKind::Chest;
		Definition.Title = FText::FromString(TEXT("遗物三选一"));
		Definition.Speaker = FText::FromString(Speaker);
		Definition.Body = FText::FromString(Body);
		Definition.Choices = {RelicChoice(TEXT("选择遗物一")), RelicChoice(TEXT("选择遗物二")), RelicChoice(TEXT("选择遗物三"))};
		return Definition;
	}

	TArray<FGameXXKRouteEncounterDefinition> BuildDefinitions()
	{
		using A = EGameXXKRouteAttributeKind;
		return {
			Event(TEXT("Encounter.Event.TusiChief"), TEXT("土司首领"), TEXT("土司首领以山地行军之法指点你稳住根基。"), TEXT("Npc.TusiChief"), AttributeChoice(TEXT("调息稳脉：最大气血+8"), A::MaxHealth, 8), AttributeChoice(TEXT("演练守势：防御+2"), A::Defense, 2)),
			Event(TEXT("Encounter.Event.SongJinBao"), TEXT("宋金宝"), TEXT("宋金宝辨出一条捷径，并传授借势赶路的诀窍。"), TEXT("Npc.SongJinBao"), AttributeChoice(TEXT("疾行练步：速度+2"), A::Speed, 2), AttributeChoice(TEXT("沿途运气：最大内力+4"), A::MaxMana, 4)),
			Event(TEXT("Encounter.Event.YueBai"), TEXT("月白"), TEXT("月白以清心法门相赠，让你选择一种修行。"), TEXT("Npc.YueBai"), AttributeChoice(TEXT("清心纳气：最大内力+5"), A::MaxMana, 5), NpcSupportChoice(TEXT("邀请月白同行"), TEXT("Npc.YueBai"))),
			Event(TEXT("Encounter.Event.ZhouGuangZu"), TEXT("周光祖"), TEXT("周光祖演示一套发力诀窍，邀你反复练习。"), TEXT("Npc.ZhouGuangZu"), AttributeChoice(TEXT("凝劲发力：攻击+2"), A::Attack, 2), AttributeChoice(TEXT("扎稳马步：最大气血+10"), A::MaxHealth, 10)),
			Event(TEXT("Encounter.Event.JinGui"), TEXT("金贵"), TEXT("金贵整理好护具，教你如何卸力与移步。"), TEXT("Npc.JinGui"), AttributeChoice(TEXT("借甲卸力：防御+2"), A::Defense, 2), AttributeChoice(TEXT("轻装移步：速度+2"), A::Speed, 2)),
			Event(TEXT("Encounter.Event.QiongMeiEr"), TEXT("琼么儿"), TEXT("琼么儿带来高原草药与轻身诀窍。"), TEXT("Npc.QiongMeiEr"), AttributeChoice(TEXT("习得轻身：速度+2"), A::Speed, 2), AttributeChoice(TEXT("服下草药：最大气血+8"), A::MaxHealth, 8)),
			Event(TEXT("Encounter.Event.NiuHuan"), TEXT("牛欢"), TEXT("牛欢与你切磋片刻，让你从两种修行方向中择一。"), TEXT("Npc.Event.NiuHuan"), AttributeChoice(TEXT("练力：攻击+3"), A::Attack, 3), AttributeChoice(TEXT("练步：速度+3"), A::Speed, 3)),
			Event(TEXT("Encounter.Event.MountainSpring"), TEXT("无名山泉"), TEXT("清冽山泉可洗脉养气，但一次只能选择一种调息方式。"), TEXT("Event.Attribute.MountainSpring"), AttributeChoice(TEXT("养血：最大气血+12"), A::MaxHealth, 12), AttributeChoice(TEXT("养气：最大内力+6"), A::MaxMana, 6)),
			Chest(TEXT("Encounter.Chest.Bamboo"), TEXT("竹编秘匣"), TEXT("匣中三件旧物各有灵性，选择一件带入本次路线。")),
			Chest(TEXT("Encounter.Chest.Bronze"), TEXT("铜锁宝箱"), TEXT("铜锁自行脱落，三件遗物只能带走其一。")),
			Chest(TEXT("Encounter.Chest.Shrine"), TEXT("山神供盒"), TEXT("供盒中浮现三件遗物的影子，选定后其余便会散去。")),
			Chest(TEXT("Encounter.Chest.Traveller"), TEXT("遗落行囊"), TEXT("行囊里保存着三件完好的旧物，只能挑选一件。"))
		};
	}
}

const TArray<FGameXXKRouteEncounterDefinition>& FGameXXKRouteEncounterCatalog::GetAllDefinitions()
{
	static const TArray<FGameXXKRouteEncounterDefinition> Definitions = BuildDefinitions();
	return Definitions;
}

const FGameXXKRouteEncounterDefinition* FGameXXKRouteEncounterCatalog::FindDefinition(const FName EncounterId)
{
	return GetAllDefinitions().FindByPredicate([EncounterId](const FGameXXKRouteEncounterDefinition& Definition){ return Definition.Id == EncounterId; });
}

TArray<const FGameXXKRouteEncounterDefinition*> FGameXXKRouteEncounterCatalog::GetDefinitionsOfKind(EGameXXKRouteEncounterKind Kind)
{
	TArray<const FGameXXKRouteEncounterDefinition*> Result;
	for (const FGameXXKRouteEncounterDefinition& Definition : GetAllDefinitions()) if (Definition.Kind == Kind) Result.Add(&Definition);
	return Result;
}

const FGameXXKRouteEncounterDefinition* FGameXXKRouteEncounterCatalog::ChooseDeterministic(EGameXXKRouteEncounterKind Kind, int32 ChoiceSeed)
{
	const TArray<const FGameXXKRouteEncounterDefinition*> Candidates = GetDefinitionsOfKind(Kind);
	if (Candidates.IsEmpty()) return nullptr;
	uint32 Value = static_cast<uint32>(ChoiceSeed);
	Value ^= Value << 13;
	Value ^= Value >> 17;
	Value ^= Value << 5;
	return Candidates[static_cast<int32>(Value % static_cast<uint32>(Candidates.Num()))];
}
