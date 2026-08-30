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

bool FGameXXKRouteEncounterCatalog::IsRetiredNpcEncounterId(const FName EncounterId)
{
	static const TSet<FName> RetiredIds = {
		TEXT("Encounter.Event.TusiChief"),
		TEXT("Encounter.Event.SongJinBao"),
		TEXT("Encounter.Event.YueBai"),
		TEXT("Encounter.Event.ZhouGuangZu"),
		TEXT("Encounter.Event.JinGui"),
		TEXT("Encounter.Event.QiongMeiEr"),
		TEXT("Encounter.Event.NiuHuan")};
	return RetiredIds.Contains(EncounterId);
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
