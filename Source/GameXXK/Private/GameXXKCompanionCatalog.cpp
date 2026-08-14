#include "GameXXKCompanionCatalog.h"

#include "GameXXKCardCatalog.h"

namespace
{
	void AddRoleTemplates(
		TArray<FGameXXKCompanionTemplateDefinition>& OutTemplates,
		const EGameXXKCharacterRole Role,
		const TCHAR* RoleKey)
	{
		for (int32 Index = 1; Index <= 4; ++Index)
		{
			FGameXXKCompanionTemplateDefinition& Template = OutTemplates.AddDefaulted_GetRef();
			Template.TemplateId = FName(*FString::Printf(TEXT("Companion.%s.%02d"), RoleKey, Index));
			Template.Role = Role;
			Template.PortraitVariantKey = FName(*FString::Printf(TEXT("Portrait.Companion.%s.%02d"), RoleKey, Index));
			Template.NamePoolKey = FName(*FString::Printf(TEXT("NamePool.%s"), RoleKey));
		}
	}

	TArray<FGameXXKCompanionTemplateDefinition> BuildRecruitTemplates()
	{
		TArray<FGameXXKCompanionTemplateDefinition> Templates;
		Templates.Reserve(24);
		AddRoleTemplates(Templates, EGameXXKCharacterRole::Blade, TEXT("Blade"));
		AddRoleTemplates(Templates, EGameXXKCharacterRole::Guard, TEXT("Guard"));
		AddRoleTemplates(Templates, EGameXXKCharacterRole::Healer, TEXT("Healer"));
		AddRoleTemplates(Templates, EGameXXKCharacterRole::Hunter, TEXT("Hunter"));
		AddRoleTemplates(Templates, EGameXXKCharacterRole::Sorcerer, TEXT("Sorcerer"));
		AddRoleTemplates(Templates, EGameXXKCharacterRole::FormationMaster, TEXT("FormationMaster"));
		return Templates;
	}

	bool NameLess(const FName Left, const FName Right)
	{
		return Left.ToString() < Right.ToString();
	}

	void AddQuestNpcDefinition(
		TArray<FGameXXKQuestNpcDefinition>& OutDefinitions,
		const TCHAR* NpcId,
		const TCHAR* PassiveId,
		const int32 Health,
		const int32 Attack,
		const int32 Defense,
		const int32 Mana,
		const int32 Speed,
		const float HealthGrowth,
		const float AttackGrowth,
		const float DefenseGrowth,
		const float ManaGrowth)
	{
		FGameXXKQuestNpcDefinition& Definition = OutDefinitions.AddDefaulted_GetRef();
		Definition.NpcId = FName(NpcId);
		Definition.PassiveId = FName(PassiveId);
		Definition.PortraitKey = FName(*FString::Printf(TEXT("Portrait.%s"), NpcId));
		Definition.BaseAttributes.Health = Health;
		Definition.BaseAttributes.Attack = Attack;
		Definition.BaseAttributes.Defense = Defense;
		Definition.BaseAttributes.Mana = Mana;
		Definition.BaseAttributes.Speed = FMath::Max(1, Speed);
		Definition.GrowthPerLevel.Health = HealthGrowth;
		Definition.GrowthPerLevel.Attack = AttackGrowth;
		Definition.GrowthPerLevel.Defense = DefenseGrowth;
		Definition.GrowthPerLevel.Mana = ManaGrowth;

		for (const FGameXXKCardDefinition& Card : FGameXXKCardCatalog::GetCardDefinitionsForOwner(Definition.NpcId))
		{
			if (Card.Owner == EGameXXKCardOwner::QuestNpc && Card.NpcId == Definition.NpcId)
			{
				Definition.FixedCardIds.Add(Card.Id);
			}
		}
		Definition.FixedCardIds.Sort(NameLess);
	}

	TArray<FGameXXKQuestNpcDefinition> BuildQuestNpcDefinitions()
	{
		TArray<FGameXXKQuestNpcDefinition> Definitions;
		Definitions.Reserve(6);
		AddQuestNpcDefinition(Definitions, TEXT("Npc.TusiChief"), TEXT("Passive.TusiChief.ZhaiWei"), 115, 14, 10, 24, 10, 11.0f, 1.4f, 1.0f, 1.0f);
		AddQuestNpcDefinition(Definitions, TEXT("Npc.SongJinBao"), TEXT("Passive.SongJinBao.RenQingMian"), 88, 10, 7, 30, 10, 8.0f, 1.0f, 0.6f, 2.0f);
		AddQuestNpcDefinition(Definitions, TEXT("Npc.YueBai"), TEXT("Passive.YueBai.CanJuanXianZhi"), 84, 15, 6, 34, 10, 8.0f, 1.5f, 0.5f, 2.0f);
		AddQuestNpcDefinition(Definitions, TEXT("Npc.ZhouGuangZu"), TEXT("Passive.ZhouGuangZu.CaoMuZhaJi"), 90, 12, 7, 32, 10, 8.0f, 1.2f, 0.7f, 2.0f);
		AddQuestNpcDefinition(Definitions, TEXT("Npc.JinGui"), TEXT("Passive.JinGui.ShiJingMenLu"), 92, 12, 8, 28, 10, 9.0f, 1.1f, 0.8f, 2.0f);
		AddQuestNpcDefinition(Definitions, TEXT("Npc.QiongMeiEr"), TEXT("Passive.QiongMeiEr.MiaoLingYinLu"), 96, 13, 8, 30, 10, 9.0f, 1.3f, 0.8f, 2.0f);
		return Definitions;
	}
}

const TArray<FGameXXKCompanionTemplateDefinition>& FGameXXKCompanionCatalog::GetRecruitTemplates()
{
	static const TArray<FGameXXKCompanionTemplateDefinition> Templates = BuildRecruitTemplates();
	return Templates;
}

const FGameXXKCompanionTemplateDefinition* FGameXXKCompanionCatalog::FindRecruitTemplate(const FName TemplateId)
{
	return GetRecruitTemplates().FindByPredicate([TemplateId](const FGameXXKCompanionTemplateDefinition& Template)
	{
		return Template.TemplateId == TemplateId;
	});
}

const TArray<FGameXXKQuestNpcDefinition>& FGameXXKCompanionCatalog::GetQuestNpcDefinitions()
{
	static const TArray<FGameXXKQuestNpcDefinition> Definitions = BuildQuestNpcDefinitions();
	return Definitions;
}

const FGameXXKQuestNpcDefinition* FGameXXKCompanionCatalog::FindQuestNpcDefinition(const FName NpcId)
{
	return GetQuestNpcDefinitions().FindByPredicate([NpcId](const FGameXXKQuestNpcDefinition& Definition)
	{
		return Definition.NpcId == NpcId;
	});
}
