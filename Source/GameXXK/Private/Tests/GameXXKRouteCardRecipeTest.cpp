#include "GameXXKRouteCardRecipe.h"

#include "GameXXKCardCatalog.h"
#include "GameXXKCardRules.h"
#include "GameXXKCompanionCatalog.h"
#include "GameXXKCompanionRules.h"
#include "GameXXKMVPRules.h"

#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr int32 RecipeRouteSeed = 0x2468ACE;
	const FName HeroUnitId(TEXT("Player"));
	const FName CompanionInstanceId(TEXT("Companion.Test.RouteRecipe"));
	const FName QuestNpcId(TEXT("Npc.TusiChief"));

	const TArray<FName> MissingPartyFillCards = {
		TEXT("Route.General.QingShenQuShi"),
		TEXT("Route.General.TuNaJue"),
		TEXT("Route.General.ZhiXueSan"),
		TEXT("Route.General.FeiZhen"),
		TEXT("Route.General.YanDun"),
		TEXT("Route.General.TieJiLi"),
		TEXT("Route.General.LinZhenMoRen"),
		TEXT("Route.Terrain.XingJunBuZhen")};

	const TArray<FName> FixedRouteCards = {
		TEXT("Route.General.PoJiaTuCi"),
		TEXT("Route.General.ShouShiHuiYuan")};

	bool AreEntriesIdentical(
		const TArray<FGameXXKRouteCardEntry>& Left,
		const TArray<FGameXXKRouteCardEntry>& Right)
	{
		if (Left.Num() != Right.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < Left.Num(); ++Index)
		{
			if (!FGameXXKRouteCardEntry::StaticStruct()->CompareScriptStruct(
				&Left[Index],
				&Right[Index],
				PPF_None))
			{
				return false;
			}
		}
		return true;
	}

	FGameXXKCardRunState MakeRecipeRun(const bool bWithCompanion, const bool bWithQuestNpc)
	{
		FGameXXKCardRunState Run;
		for (const FGameXXKCardDefinition& Definition : FGameXXKCardCatalog::GetAllCardDefinitions())
		{
			if (Definition.Owner == EGameXXKCardOwner::Hero)
			{
				Run.HeroUnlockedCardIds.Add(Definition.Id);
				if (Run.HeroSelectedCardIds.Num() < 8)
				{
					Run.HeroSelectedCardIds.Add(Definition.Id);
				}
			}
		}

		if (bWithCompanion)
		{
			FGameXXKPermanentCompanion& Companion = Run.CompanionRoster.PermanentCompanions.AddDefaulted_GetRef();
			Companion.InstanceId = CompanionInstanceId;
			Companion.Role = EGameXXKCharacterRole::Blade;
			Companion.bIsActive = true;
			for (const FGameXXKCardDefinition& Definition : FGameXXKCardCatalog::GetAllCardDefinitions())
			{
				if (Definition.Owner == EGameXXKCardOwner::Profession
					&& Definition.Role == Companion.Role
					&& Companion.SelectedCardIds.Num() < 5)
				{
					Companion.SelectedCardIds.Add(Definition.Id);
				}
			}
			Companion.PersonalCardIds = Companion.SelectedCardIds;
			Companion.UnlockedPersonalCardIds = Companion.SelectedCardIds;
			Run.PartySelection.ActivePermanentCompanionInstanceId = Companion.InstanceId;
		}

		if (bWithQuestNpc)
		{
			const FGameXXKQuestNpcDefinition* Definition = FGameXXKCompanionCatalog::FindQuestNpcDefinition(QuestNpcId);
			if (Definition)
			{
				Run.ActiveTemporaryQuestNpcId = QuestNpcId;
				Run.PartySelection.QuestNpc.NpcId = QuestNpcId;
				FGameXXKCompanionRules::BuildQuestNpcRouteCardSelection(
					QuestNpcId,
					RecipeRouteSeed,
					Run.PartySelection.QuestNpc.SelectedCardIds,
					nullptr);
			}
		}

		return Run;
	}

	TArray<FName> BuildExpectedCardIds(const FGameXXKCardRunState& Run, const bool bWithCompanion, const bool bWithQuestNpc)
	{
		TArray<FName> Expected = Run.HeroSelectedCardIds;
		int32 MissingFillIndex = 0;
		if (bWithCompanion)
		{
			Expected.Append(Run.CompanionRoster.PermanentCompanions[0].SelectedCardIds);
		}
		else
		{
			for (int32 Index = 0; Index < 5; ++Index)
			{
				Expected.Add(MissingPartyFillCards[MissingFillIndex++]);
			}
		}

		if (bWithQuestNpc)
		{
			Expected.Append(Run.PartySelection.QuestNpc.SelectedCardIds);
		}
		else
		{
			for (int32 Index = 0; Index < 3; ++Index)
			{
				Expected.Add(MissingPartyFillCards[MissingFillIndex++]);
			}
		}
		Expected.Append(FixedRouteCards);
		return Expected;
	}

	struct FExpectedRouteCard
	{
		const TCHAR* Id;
		const TCHAR* DisplayName;
		EGameXXKCardRarity Rarity;
		EGameXXKCardQuality Quality;
		int32 EnergyCost;
		int32 ManaCost;
		EGameXXKCardTargetMode TargetMode;
		const TCHAR* FrameKey;
		const TCHAR* AcquisitionKey;
		TArray<FGameXXKCardEffect> Effects;
	};

	FGameXXKCardEffectCondition ExpectedTargetHasStatus(const EGameXXKCardStatus Status)
	{
		FGameXXKCardEffectCondition Result;
		Result.Type = EGameXXKCardEffectConditionType::TargetHasStatus;
		Result.Status = Status;
		Result.MinimumStatusStacks = 1;
		return Result;
	}

	FGameXXKCardEffectCondition ExpectedOwnerArmorAtLeast(const int32 MinimumArmor)
	{
		FGameXXKCardEffectCondition Result;
		Result.Type = EGameXXKCardEffectConditionType::OwnerArmorAtLeast;
		Result.MinimumArmor = MinimumArmor;
		return Result;
	}

	FGameXXKCardEffectCondition ExpectedOwnerHealthBelow(const float Percent, const bool bNegate = false)
	{
		FGameXXKCardEffectCondition Result;
		Result.Type = EGameXXKCardEffectConditionType::OwnerHealthBelowPercent;
		Result.HealthPercentThreshold = Percent;
		Result.bNegate = bNegate;
		return Result;
	}

	FGameXXKCardEffectCondition ExpectedTerrainIs(
		const EGameXXKCardTerrain Terrain,
		const EGameXXKCardTerrain AlternateTerrain = EGameXXKCardTerrain::Invalid,
		const bool bNegate = false)
	{
		FGameXXKCardEffectCondition Result;
		Result.Type = EGameXXKCardEffectConditionType::TerrainIsAny;
		Result.Terrain = Terrain;
		Result.AlternateTerrain = AlternateTerrain;
		Result.bNegate = bNegate;
		return Result;
	}

	FGameXXKCardEffectCondition ExpectedConsumeTargetStatus(
		const EGameXXKCardStatus Status,
		const int32 MaximumStacks)
	{
		FGameXXKCardEffectCondition Result = ExpectedTargetHasStatus(Status);
		Result.bConsumeStatus = true;
		Result.MaxConsumedStatusStacks = MaximumStacks;
		Result.bScaleMagnitudeByConsumedStacks = true;
		return Result;
	}

	FGameXXKCardEffect ExpectedEffect(
		const EGameXXKCardEffectType Type,
		const EGameXXKCardEffectTarget Target,
		const int32 Magnitude,
		const EGameXXKCardStatus Status = EGameXXKCardStatus::None,
		const FGameXXKCardEffectCondition& Condition = FGameXXKCardEffectCondition())
	{
		FGameXXKCardEffect Result;
		Result.Type = Type;
		Result.Target = Target;
		Result.Magnitude = Magnitude;
		Result.Status = Status;
		Result.Condition = Condition;
		return Result;
	}

	FGameXXKCardEffect ExpectedAttack(
		const int32 Percent,
		const EGameXXKCardEffectTarget Target,
		const FGameXXKCardEffectCondition& Condition = FGameXXKCardEffectCondition())
	{
		return ExpectedEffect(EGameXXKCardEffectType::DamagePercentAttack, Target, Percent, EGameXXKCardStatus::None, Condition);
	}

	FGameXXKCardEffect ExpectedModifier(
		const EGameXXKCardBattleModifierTrigger Trigger,
		const EGameXXKCardEffectType EffectType,
		const EGameXXKCardEffectTarget EffectTarget,
		const int32 Magnitude,
		const int32 RemainingTriggers,
		const FGameXXKCardEffectCondition& Condition = FGameXXKCardEffectCondition(),
		const EGameXXKCardModifierRecipientScope RecipientScope = EGameXXKCardModifierRecipientScope::CardOwner,
		const EGameXXKCardEffectTarget RecipientTarget = EGameXXKCardEffectTarget::CardOwner)
	{
		FGameXXKCardEffect Result = ExpectedEffect(EGameXXKCardEffectType::ApplyBattleModifier, RecipientTarget, 0);
		Result.Modifier.Trigger = Trigger;
		Result.Modifier.EffectType = EffectType;
		Result.Modifier.Target = EffectTarget;
		Result.Modifier.RecipientScope = RecipientScope;
		Result.Modifier.RecipientTarget = RecipientTarget;
		Result.Modifier.Expiry = EGameXXKCardModifierExpiry::AfterTriggerCount;
		Result.Modifier.TriggeredAttackTargetScope = EGameXXKCardTriggeredAttackTargetScope::AnyTarget;
		Result.Modifier.Magnitude = Magnitude;
		Result.Modifier.RemainingTriggers = RemainingTriggers;
		Result.Modifier.bPersistent = true;
		Result.Modifier.Condition = Condition;
		return Result;
	}

	FGameXXKCardTargetSpec ExpectedTargetSpec(const EGameXXKCardTargetMode Mode)
	{
		FGameXXKCardTargetSpec Result;
		Result.Mode = Mode;
		Result.RequiredUnitState = Mode == EGameXXKCardTargetMode::None
			? EGameXXKCardUnitState::Any
			: EGameXXKCardUnitState::Living;
		switch (Mode)
		{
		case EGameXXKCardTargetMode::None:
			Result.Presentation = EGameXXKCardTargetPresentation::NoSelection;
			break;
		case EGameXXKCardTargetMode::Self:
			Result.Presentation = EGameXXKCardTargetPresentation::Self;
			break;
		case EGameXXKCardTargetMode::SingleEnemy:
		case EGameXXKCardTargetMode::SingleAlly:
			Result.Presentation = EGameXXKCardTargetPresentation::PlayerSelectsUnit;
			break;
		case EGameXXKCardTargetMode::AllEnemies:
		case EGameXXKCardTargetMode::AllAllies:
			Result.Presentation = EGameXXKCardTargetPresentation::Group;
			break;
		default:
			break;
		}
		return Result;
	}

	TArray<FExpectedRouteCard> BuildExpectedRouteCardContracts()
	{
		using R = EGameXXKCardRarity;
		using Q = EGameXXKCardQuality;
		using M = EGameXXKCardTargetMode;
		using T = EGameXXKCardEffectType;
		using G = EGameXXKCardEffectTarget;
		using S = EGameXXKCardStatus;
		using L = EGameXXKCardTerrain;

		return {
			{TEXT("Route.General.PoJiaTuCi"), TEXT("破甲突刺"), R::Common, Q::Common, 1, 0, M::SingleEnemy, TEXT("Style.Route.General"), TEXT("Route.General"), {ExpectedAttack(100, G::SelectedTarget), ExpectedEffect(T::ApplyStatus, G::SelectedTarget, 1, S::Vulnerability)}},
			{TEXT("Route.General.ShouShiHuiYuan"), TEXT("守势回元"), R::Common, Q::Common, 1, 0, M::Self, TEXT("Style.Route.General"), TEXT("Route.General"), {ExpectedEffect(T::AddArmor, G::CardOwner, 8), ExpectedEffect(T::GainMana, G::CardOwner, 3)}},
			{TEXT("Route.General.QingShenQuShi"), TEXT("轻身取势"), R::Common, Q::Common, 0, 0, M::Self, TEXT("Style.Route.General"), TEXT("Route.General"), {ExpectedEffect(T::ApplyStatus, G::CardOwner, 1, S::Agility)}},
			{TEXT("Route.General.TuNaJue"), TEXT("吐纳诀"), R::Common, Q::Common, 0, 0, M::Self, TEXT("Style.Route.General"), TEXT("Route.General"), {ExpectedEffect(T::GainMana, G::CardOwner, 5)}},
			{TEXT("Route.General.ZhiXueSan"), TEXT("止血散"), R::Common, Q::Common, 1, 0, M::SingleAlly, TEXT("Style.Route.General"), TEXT("Route.General"), {ExpectedEffect(T::Heal, G::SelectedTarget, 12), ExpectedEffect(T::RemoveStatus, G::SelectedTarget, 1, S::Bleed)}},
			{TEXT("Route.General.FeiZhen"), TEXT("飞针"), R::Common, Q::Common, 1, 0, M::SingleEnemy, TEXT("Style.Route.General"), TEXT("Route.General"), {ExpectedAttack(70, G::SelectedTarget), ExpectedEffect(T::ApplyStatus, G::SelectedTarget, 1, S::Mark)}},
			{TEXT("Route.General.YanDun"), TEXT("烟遁"), R::Common, Q::Common, 1, 0, M::SingleAlly, TEXT("Style.Route.General"), TEXT("Route.General"), {ExpectedEffect(T::ApplyStatus, G::SelectedTarget, 1, S::Agility), ExpectedEffect(T::AddArmor, G::SelectedTarget, 4)}},
			{TEXT("Route.General.TieJiLi"), TEXT("铁蒺藜"), R::Common, Q::Common, 1, 0, M::SingleEnemy, TEXT("Style.Route.General"), TEXT("Route.General"), {ExpectedEffect(T::ApplyStatus, G::SelectedTarget, 2, S::Poison), ExpectedEffect(T::ApplyStatus, G::SelectedTarget, 1, S::Vulnerability)}},
			{TEXT("Route.General.LinZhenMoRen"), TEXT("临阵磨刃"), R::Common, Q::Common, 1, 0, M::SingleAlly, TEXT("Style.Route.General"), TEXT("Route.General"), {ExpectedModifier(EGameXXKCardBattleModifierTrigger::OnNextAttack, T::BonusDamagePercent, G::PlayedCard, 25, 2, FGameXXKCardEffectCondition(), EGameXXKCardModifierRecipientScope::SelectedTarget, G::SelectedTarget)}},
			{TEXT("Route.General.HeJiLing"), TEXT("合击令"), R::Common, Q::Common, 2, 6, M::SingleEnemy, TEXT("Style.Route.General"), TEXT("Route.General"), {ExpectedEffect(T::EachLivingAllyAttackSelectedTarget, G::SelectedTarget, 50)}},

			{TEXT("Route.Terrain.DuanYaLuoShi"), TEXT("断崖落石"), R::Common, Q::Common, 2, 0, M::SingleEnemy, TEXT("Style.Route.Terrain"), TEXT("Route.Terrain"), {ExpectedAttack(130, G::SelectedTarget, ExpectedTerrainIs(L::Cliff, L::Invalid, true)), ExpectedEffect(T::ApplyStatus, G::SelectedTarget, 1, S::Vulnerability, ExpectedTerrainIs(L::Cliff, L::Invalid, true)), ExpectedAttack(180, G::SelectedTarget, ExpectedTerrainIs(L::Cliff)), ExpectedEffect(T::ApplyStatus, G::SelectedTarget, 2, S::Vulnerability, ExpectedTerrainIs(L::Cliff))}},
			{TEXT("Route.Terrain.LinYingFuXi"), TEXT("林影伏袭"), R::Common, Q::Common, 1, 0, M::SingleEnemy, TEXT("Style.Route.Terrain"), TEXT("Route.Terrain"), {ExpectedEffect(T::ApplyStatus, G::SelectedTarget, 2, S::Mark), ExpectedEffect(T::ApplyStatus, G::CardOwner, 1, S::Agility, ExpectedTerrainIs(L::Forest)), ExpectedEffect(T::DrawCards, G::CardOwner, 1, S::None, ExpectedTerrainIs(L::Forest))}},
			{TEXT("Route.Terrain.DuKouHuiLiu"), TEXT("渡口回流"), R::Common, Q::Common, 1, 0, M::AllAllies, TEXT("Style.Route.Terrain"), TEXT("Route.Terrain"), {ExpectedEffect(T::GainMana, G::AllAllies, 3), ExpectedEffect(T::GainMana, G::AllAllies, 3, S::None, ExpectedTerrainIs(L::WaterShore, L::Ferry))}},
			{TEXT("Route.Terrain.ZhaiHuoYuanShou"), TEXT("寨火援手"), R::Common, Q::Common, 1, 0, M::AllAllies, TEXT("Style.Route.Terrain"), TEXT("Route.Terrain"), {ExpectedEffect(T::AddArmor, G::AllAllies, 5), ExpectedEffect(T::Heal, G::AllAllies, 6, S::None, ExpectedTerrainIs(L::Village))}},
			{TEXT("Route.Terrain.DongHuoZhaoMing"), TEXT("洞火照明"), R::Common, Q::Common, 1, 0, M::AllEnemies, TEXT("Style.Route.Terrain"), TEXT("Route.Terrain"), {ExpectedEffect(T::ApplyStatus, G::AllEnemies, 1, S::Mark), ExpectedEffect(T::ApplyStatus, G::AllEnemies, 2, S::Burn, ExpectedTerrainIs(L::Cave)), ExpectedEffect(T::DrawCards, G::CardOwner, 1, S::None, ExpectedTerrainIs(L::Cave))}},
			{TEXT("Route.Terrain.JieShiTuXi"), TEXT("借势突袭"), R::Common, Q::Common, 2, 0, M::SingleEnemy, TEXT("Style.Route.Terrain"), TEXT("Route.Terrain"), {ExpectedAttack(115, G::SelectedTarget), ExpectedEffect(T::ApplyStatus, G::SelectedTarget, 2, S::Vulnerability, ExpectedTerrainIs(L::Cliff)), ExpectedEffect(T::ApplyStatus, G::SelectedTarget, 2, S::Mark, ExpectedTerrainIs(L::Forest)), ExpectedEffect(T::GainMana, G::CardOwner, 4, S::None, ExpectedTerrainIs(L::WaterShore, L::Ferry)), ExpectedEffect(T::AddArmor, G::AllAllies, 4, S::None, ExpectedTerrainIs(L::Village)), ExpectedEffect(T::ApplyStatus, G::SelectedTarget, 2, S::Poison, ExpectedTerrainIs(L::Cave)), ExpectedEffect(T::DrawCards, G::CardOwner, 1, S::None, ExpectedTerrainIs(L::Plain))}},
			{TEXT("Route.Terrain.XingJunBuZhen"), TEXT("行军布阵"), R::Common, Q::Common, 1, 0, M::AllAllies, TEXT("Style.Route.Terrain"), TEXT("Route.Terrain"), {ExpectedEffect(T::AddArmor, G::AllAllies, 4), ExpectedEffect(T::ApplyStatus, G::CardOwner, 1, S::NextTerrainCardEnergyReduction)}},
			{TEXT("Route.Terrain.DiMaiHuiXiang"), TEXT("地脉回响"), R::Common, Q::Common, 0, 0, M::None, TEXT("Style.Route.Terrain"), TEXT("Route.Terrain"), {ExpectedEffect(T::DoubleTerrainBonus, G::CardOwner, 2)}},
			{TEXT("Route.Terrain.LinShiZhaYing"), TEXT("临时扎营"), R::Common, Q::Common, 2, 0, M::AllAllies, TEXT("Style.Route.Terrain"), TEXT("Route.Terrain"), {ExpectedEffect(T::Heal, G::AllAllies, 8), ExpectedEffect(T::GainMana, G::AllAllies, 2), ExpectedEffect(T::Heal, G::AllAllies, 4, S::None, ExpectedTerrainIs(L::Forest, L::Village))}},
			{TEXT("Route.Terrain.XianLuTuWei"), TEXT("险路突围"), R::Common, Q::Common, 2, 6, M::AllAllies, TEXT("Style.Route.Terrain"), TEXT("Route.Terrain"), {ExpectedEffect(T::ApplyStatus, G::AllAllies, 1, S::Agility), ExpectedEffect(T::DrawCards, G::CardOwner, 1), ExpectedEffect(T::AddArmor, G::LowestHealthAlly, 8, S::None, ExpectedTerrainIs(L::Cliff, L::Forest))}},

			{TEXT("Route.Rare.GuJuanCanZhang"), TEXT("古卷残章"), R::Rare, Q::Rare, 0, 0, M::None, TEXT("Style.Route.Rare"), TEXT("Route.Rare"), {ExpectedEffect(T::Insight, G::CardOwner, 3), ExpectedEffect(T::DiscoverCards, G::CardOwner, 1), ExpectedEffect(T::ReorderCards, G::CardOwner, 3)}},
			{TEXT("Route.Rare.TieYiYiJue"), TEXT("铁衣遗诀"), R::Rare, Q::Rare, 2, 0, M::Self, TEXT("Style.Route.Rare"), TEXT("Route.Rare"), {ExpectedEffect(T::AddArmor, G::CardOwner, 18), ExpectedModifier(EGameXXKCardBattleModifierTrigger::EndOfRound, T::GainEnergy, G::CardOwner, 1, 1, ExpectedOwnerArmorAtLeast(10))}},
			{TEXT("Route.Rare.LingQuanYiYin"), TEXT("灵泉一饮"), R::Rare, Q::Rare, 1, 0, M::SingleAlly, TEXT("Style.Route.Rare"), TEXT("Route.Rare"), {ExpectedEffect(T::Heal, G::SelectedTarget, 20), ExpectedEffect(T::GainMana, G::SelectedTarget, 5), ExpectedEffect(T::RemoveAnyDamageOverTime, G::SelectedTarget, 1)}},
			{TEXT("Route.Rare.JueJingFanJi"), TEXT("绝境反击"), R::Rare, Q::Rare, 2, 0, M::SingleEnemy, TEXT("Style.Route.Rare"), TEXT("Route.Rare"), {ExpectedEffect(T::AddArmor, G::CardOwner, 10), ExpectedAttack(220, G::SelectedTarget, ExpectedOwnerHealthBelow(30.0f)), ExpectedAttack(120, G::SelectedTarget, ExpectedOwnerHealthBelow(30.0f, true))}},
			{TEXT("Route.Rare.TongXinHeBi"), TEXT("同心合璧"), R::Rare, Q::Rare, 2, 8, M::AllAllies, TEXT("Style.Route.Rare"), TEXT("Route.Rare"), {ExpectedEffect(T::ApplyStatus, G::AllAllies, 1, S::Momentum), ExpectedEffect(T::GainMana, G::AllAllies, 3), ExpectedEffect(T::DrawCards, G::CardOwner, 1)}},

			{TEXT("Route.Boss.XiongPiPiJia"), TEXT("熊罴皮甲"), R::Boss, Q::Epic, 2, 0, M::Self, TEXT("Style.Route.Boss"), TEXT("Route.Boss.BlackBear"), {ExpectedEffect(T::AddArmor, G::CardOwner, 18), ExpectedModifier(EGameXXKCardBattleModifierTrigger::FirstDirectDamageReceivedThisRound, T::DamagePercentAttack, G::Attacker, 50, 1)}},
			{TEXT("Route.Boss.HanDiYiShi"), TEXT("撼地遗势"), R::Boss, Q::Epic, 3, 10, M::SingleEnemy, TEXT("Style.Route.Boss"), TEXT("Route.Boss.BlackBear"), {ExpectedAttack(180, G::SelectedTarget), ExpectedEffect(T::ApplyStatus, G::SelectedTarget, 3, S::Vulnerability), ExpectedEffect(T::AddArmor, G::CardOwner, 10)}},
			{TEXT("Route.Boss.HuPoZhenDan"), TEXT("虎魄镇胆"), R::Boss, Q::Epic, 2, 8, M::AllAllies, TEXT("Style.Route.Boss"), TEXT("Route.Boss.Tiger"), {ExpectedEffect(T::AddArmor, G::AllAllies, 10), ExpectedEffect(T::RemoveAnyDamageOverTime, G::AllAllies, 1)}},
			{TEXT("Route.Boss.DuKouLieFeng"), TEXT("渡口猎风"), R::Boss, Q::Epic, 2, 6, M::SingleEnemy, TEXT("Style.Route.Boss"), TEXT("Route.Boss.Tiger"), {ExpectedAttack(140, G::SelectedTarget), ExpectedEffect(T::BonusDamagePercent, G::SelectedTarget, 80, S::None, ExpectedTargetHasStatus(S::Mark))}},
			{TEXT("Route.Boss.FuHuDuanJiang"), TEXT("伏虎断江"), R::Boss, Q::Epic, 3, 14, M::SingleEnemy, TEXT("Style.Route.Boss"), TEXT("Route.Boss.Tiger"), {ExpectedAttack(230, G::SelectedTarget), ExpectedEffect(T::BonusDamagePercentPerConsumedStatus, G::SelectedTarget, 25, S::None, ExpectedConsumeTargetStatus(S::Vulnerability, 3))}},
		};
	}

	void AddRouteRuntimeStatus(
		FGameXXKCardCombatUnit& Unit,
		const EGameXXKCardStatus Status,
		const int32 Stacks)
	{
		FGameXXKCardStatusStack& Stack = Unit.Statuses.AddDefaulted_GetRef();
		Stack.Status = Status;
		Stack.Stacks = Stacks;
	}

	FGameXXKCardCombatUnit MakeRouteRuntimeUnit(
		const FName UnitId,
		const EGameXXKCardTargetSide Side,
		const EGameXXKCharacterRole Role,
		const int32 StableSortOrder,
		const bool bLowHealth = false)
	{
		FGameXXKCardCombatUnit Unit;
		Unit.UnitId = UnitId;
		Unit.Side = Side;
		Unit.Role = Role;
		Unit.bLiving = true;
		Unit.MaxHP = Side == EGameXXKCardTargetSide::Enemy ? 50000 : 500;
		Unit.HP = bLowHealth ? 100 : Unit.MaxHP;
		Unit.Mana = Side == EGameXXKCardTargetSide::Party ? 200 : 0;
		Unit.MaxMana = Unit.Mana;
		Unit.Attack = Side == EGameXXKCardTargetSide::Enemy ? 10 : 30 - StableSortOrder;
		Unit.Defense = 0;
		Unit.Speed = 1;
		Unit.Armor = Side == EGameXXKCardTargetSide::Party ? 20 : 0;
		Unit.StableSortOrder = StableSortOrder;
		if (Side == EGameXXKCardTargetSide::Party)
		{
			AddRouteRuntimeStatus(Unit, EGameXXKCardStatus::Bleed, 2);
			AddRouteRuntimeStatus(Unit, EGameXXKCardStatus::Poison, 2);
			AddRouteRuntimeStatus(Unit, EGameXXKCardStatus::Burn, 2);
			AddRouteRuntimeStatus(Unit, EGameXXKCardStatus::DamageOverTime, 2);
		}
		else
		{
			AddRouteRuntimeStatus(Unit, EGameXXKCardStatus::Mark, 1);
			AddRouteRuntimeStatus(Unit, EGameXXKCardStatus::Vulnerability, 3);
		}
		return Unit;
	}

	FGameXXKCardInstance MakeRouteRuntimeCard(
		const FName InstanceId,
		const FName CardId,
		const EGameXXKCardQuality Quality,
		const int32 AcquisitionOrdinal)
	{
		FGameXXKCardInstance Card;
		Card.InstanceId = InstanceId;
		Card.CardId = CardId;
		Card.CurrentQuality = Quality;
		Card.OwnerUnitId = TEXT("Player");
		Card.SourceEntryId = FName(*FString::Printf(TEXT("Route.Runtime.Source.%d"), AcquisitionOrdinal));
		Card.AcquisitionOrdinal = AcquisitionOrdinal;
		return Card;
	}

	bool BuildRouteRuntime(
		FAutomationTestBase& Test,
		const FGameXXKCardDefinition& Definition,
		const EGameXXKCardTerrain Terrain,
		const bool bLowHealthOwner,
		FGameXXKCardBattleRuntime& OutRuntime,
		FName& OutPlayedInstanceId)
	{
		TArray<FGameXXKCardInstance> Cards;
		OutPlayedInstanceId = FName(*FString::Printf(TEXT("Route.Played.%s"), *Definition.Id.ToString()));
		Cards.Add(MakeRouteRuntimeCard(OutPlayedInstanceId, Definition.Id, Definition.BaseQuality, 0));
		const TCHAR* FillerCardIds[] = {
			TEXT("Hero.Generic.QingFengYiShi"),
			TEXT("Hero.Generic.HeYuZhan"),
			TEXT("Hero.Generic.FengShenBu"),
			TEXT("Hero.Generic.SuiYanJi"),
			TEXT("Hero.Generic.GuiYuanShu"),
			TEXT("Hero.Generic.HengJianShouShi"),
			TEXT("Hero.Generic.NingShenTuNa"),
			TEXT("Hero.Generic.GuanXi")};
		for (int32 Index = 0; Index < static_cast<int32>(UE_ARRAY_COUNT(FillerCardIds)); ++Index)
		{
			Cards.Add(MakeRouteRuntimeCard(
				FName(*FString::Printf(TEXT("Route.Filler.%d"), Index)),
				FName(FillerCardIds[Index]),
				EGameXXKCardQuality::Common,
				Index + 1));
		}

		TArray<FGameXXKCardCombatUnit> Units = {
			MakeRouteRuntimeUnit(TEXT("Player"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 0, bLowHealthOwner),
			MakeRouteRuntimeUnit(TEXT("Ally.A"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Guard, 1),
			MakeRouteRuntimeUnit(TEXT("Ally.B"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hunter, 2),
			MakeRouteRuntimeUnit(TEXT("Enemy.A"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 10),
			MakeRouteRuntimeUnit(TEXT("Enemy.B"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 11)};
		FString Error;
		if (!GameXXKCardRules::InitializeCardBattleRuntime(
			OutRuntime,
			Cards,
			Units,
			Terrain,
			61000 + Definition.Id.GetComparisonIndex().ToUnstableInt(),
			&Error))
		{
			Test.AddError(FString::Printf(TEXT("%s runtime initialization failed: %s"), *Definition.Id.ToString(), *Error));
			return false;
		}
		OutRuntime.Deck.Hand.Reset();
		OutRuntime.Deck.DrawPile.Reset();
		OutRuntime.Deck.DiscardPile.Reset();
		OutRuntime.Deck.ExhaustPile.Reset();
		for (const FGameXXKCardInstance& Card : Cards)
		{
			(Card.InstanceId == OutPlayedInstanceId ? OutRuntime.Deck.Hand : OutRuntime.Deck.DrawPile).Add(Card);
		}
		OutRuntime.Deck.SharedEnergy = 20;
		if (!GameXXKCardRules::ValidateCardBattleRuntime(OutRuntime, &Error))
		{
			Test.AddError(FString::Printf(TEXT("%s runtime fixture is invalid: %s"), *Definition.Id.ToString(), *Error));
			return false;
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteCardAll30CatalogContractsTest,
	"GameXXK.Route.CardRecipe.All30CatalogContracts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteCardAll30CatalogContractsTest::RunTest(const FString& Parameters)
{
	const TArray<FExpectedRouteCard> ExpectedCards = BuildExpectedRouteCardContracts();
	TestEqual(TEXT("the exact route-card contract table contains thirty rows"), ExpectedCards.Num(), 30);

	TArray<const FGameXXKCardDefinition*> RouteDefinitions;
	for (const FGameXXKCardDefinition& Definition : FGameXXKCardCatalog::GetAllCardDefinitions())
	{
		if (Definition.Owner == EGameXXKCardOwner::Route)
		{
			RouteDefinitions.Add(&Definition);
		}
	}
	TestEqual(TEXT("the live catalog contains exactly thirty route cards"), RouteDefinitions.Num(), 30);

	TSet<FName> SeenIds;
	for (const FExpectedRouteCard& Expected : ExpectedCards)
	{
		const FName CardId(Expected.Id);
		TestFalse(FString::Printf(TEXT("expected route CardId is unique: %s"), Expected.Id), SeenIds.Contains(CardId));
		SeenIds.Add(CardId);

		const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(CardId);
		if (!TestNotNull(FString::Printf(TEXT("route card exists: %s"), Expected.Id), Definition))
		{
			continue;
		}
		TestEqual(FString::Printf(TEXT("display name: %s"), Expected.Id), Definition->DisplayName.ToString(), FString(Expected.DisplayName));
		TestEqual(FString::Printf(TEXT("owner: %s"), Expected.Id), Definition->Owner, EGameXXKCardOwner::Route);
		TestEqual(FString::Printf(TEXT("rarity: %s"), Expected.Id), Definition->Rarity, Expected.Rarity);
		TestEqual(FString::Printf(TEXT("quality: %s"), Expected.Id), Definition->BaseQuality, Expected.Quality);
		TestEqual(FString::Printf(TEXT("role: %s"), Expected.Id), Definition->Role, EGameXXKCharacterRole::Route);
		TestEqual(FString::Printf(TEXT("owner id: %s"), Expected.Id), Definition->OwnerId, FName(TEXT("Route")));
		TestTrue(FString::Printf(TEXT("NPC id is empty: %s"), Expected.Id), Definition->NpcId.IsNone());
		TestEqual(FString::Printf(TEXT("energy cost: %s"), Expected.Id), Definition->EnergyCost, Expected.EnergyCost);
		TestEqual(FString::Printf(TEXT("mana cost: %s"), Expected.Id), Definition->ManaCost, Expected.ManaCost);

		const FGameXXKCardTargetSpec ExpectedTarget = ExpectedTargetSpec(Expected.TargetMode);
		TestTrue(
			FString::Printf(TEXT("complete target contract: %s"), Expected.Id),
			FGameXXKCardTargetSpec::StaticStruct()->CompareScriptStruct(&Definition->TargetSpec, &ExpectedTarget, PPF_None));
		TestEqual(FString::Printf(TEXT("effect count: %s"), Expected.Id), Definition->Effects.Num(), Expected.Effects.Num());
		for (int32 EffectIndex = 0; EffectIndex < FMath::Min(Definition->Effects.Num(), Expected.Effects.Num()); ++EffectIndex)
		{
			TestTrue(
				FString::Printf(TEXT("effect %d contract: %s"), EffectIndex, Expected.Id),
				FGameXXKCardEffect::StaticStruct()->CompareScriptStruct(
					&Definition->Effects[EffectIndex],
					&Expected.Effects[EffectIndex],
					PPF_None));
		}

		TestEqual(
			FString::Printf(TEXT("art key: %s"), Expected.Id),
			Definition->VisualArtKey,
			FName(*FString::Printf(TEXT("Art.%s"), Expected.Id)));
		TestEqual(FString::Printf(TEXT("frame key: %s"), Expected.Id), Definition->FrameKey, FName(Expected.FrameKey));
		TestEqual(FString::Printf(TEXT("acquisition key: %s"), Expected.Id), Definition->AcquisitionKey, FName(Expected.AcquisitionKey));
		TestFalse(FString::Printf(TEXT("route card is not a profession core: %s"), Expected.Id), Definition->bCoreProfessionCard);
		TestTrue(FString::Printf(TEXT("route card has no profession archetype: %s"), Expected.Id), Definition->ProfessionArchetypeIds.IsEmpty());
		TestFalse(FString::Printf(TEXT("route card identity is not NPC-locked: %s"), Expected.Id), Definition->bIdentityLocked);
		TestEqual(FString::Printf(TEXT("route card has no linked profession: %s"), Expected.Id), Definition->LinkedRole, EGameXXKCharacterRole::Invalid);
		TestEqual(FString::Printf(TEXT("route card has no hero unlock level: %s"), Expected.Id), Definition->HeroUnlockLevel, 0);
		TestFalse(FString::Printf(TEXT("route card does not exhaust on play: %s"), Expected.Id), Definition->bExhaustOnPlay);
		TestTrue(FString::Printf(TEXT("route card has no Charge payload: %s"), Expected.Id), Definition->ChargeEffects.IsEmpty());
		TestTrue(FString::Printf(TEXT("route card has no Finish payload: %s"), Expected.Id), Definition->FinishEffects.IsEmpty());
		TestTrue(FString::Printf(TEXT("route card has no task-NPC reward payload: %s"), Expected.Id), Definition->TaskNpcRewardEffects.IsEmpty());
		TestEqual(FString::Printf(TEXT("route card has no spell-task reward: %s"), Expected.Id), Definition->SpellTaskReward, EGameXXKHeroSpellTaskReward::None);
	}

	for (const FGameXXKCardDefinition* Definition : RouteDefinitions)
	{
		TestTrue(
			FString::Printf(TEXT("every live route CardId is present in the exact table: %s"), *Definition->Id.ToString()),
			SeenIds.Contains(Definition->Id));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteCardAll30RuntimeTest,
	"GameXXK.Route.CardRecipe.All30BaseResolution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteCardAll30RuntimeTest::RunTest(const FString& Parameters)
{
	const TArray<EGameXXKCardTerrain> EveryTerrain = {
		EGameXXKCardTerrain::Plain,
		EGameXXKCardTerrain::Cliff,
		EGameXXKCardTerrain::Forest,
		EGameXXKCardTerrain::WaterShore,
		EGameXXKCardTerrain::Ferry,
		EGameXXKCardTerrain::Village,
		EGameXXKCardTerrain::Cave};
	TSet<FName> ExecutedCardIds;
	int32 ResolutionCount = 0;
	for (const FGameXXKCardDefinition& Definition : FGameXXKCardCatalog::GetAllCardDefinitions())
	{
		if (Definition.Owner != EGameXXKCardOwner::Route)
		{
			continue;
		}
		const bool bTerrainCard = Definition.AcquisitionKey == FName(TEXT("Route.Terrain"));
		const TArray<EGameXXKCardTerrain> Terrains = bTerrainCard
			? EveryTerrain
			: TArray<EGameXXKCardTerrain>{EGameXXKCardTerrain::Plain};
		for (const EGameXXKCardTerrain Terrain : Terrains)
		{
			FGameXXKCardBattleRuntime Runtime;
			FName PlayedInstanceId;
			if (!BuildRouteRuntime(*this, Definition, Terrain, false, Runtime, PlayedInstanceId))
			{
				continue;
			}

			FGameXXKCardPlayPreview Preview;
			FString Error;
			const FString Context = FString::Printf(TEXT("%s terrain=%d"), *Definition.Id.ToString(), static_cast<int32>(Terrain));
			if (!TestTrue(
				FString::Printf(TEXT("preview succeeds: %s (%s)"), *Context, *Error),
				GameXXKCardRules::BuildCardPlayPreview(Runtime, PlayedInstanceId, Preview, &Error)))
			{
				continue;
			}
			TestTrue(FString::Printf(TEXT("preview is playable: %s"), *Context), Preview.bCanPlay);
			TestEqual(FString::Printf(TEXT("preview CardId: %s"), *Context), Preview.CardId, Definition.Id);
			TestEqual(FString::Printf(TEXT("preview energy: %s"), *Context), Preview.EffectiveEnergyCost, Definition.EnergyCost);
			TestEqual(FString::Printf(TEXT("preview mana: %s"), *Context), Preview.EffectiveManaCost, Definition.ManaCost);

			const bool bExpectedManual = Definition.TargetSpec.Mode == EGameXXKCardTargetMode::SingleEnemy
				|| Definition.TargetSpec.Mode == EGameXXKCardTargetMode::SingleAlly;
			TestEqual(
				FString::Printf(TEXT("manual-target contract: %s"), *Context),
				Preview.TargetRequest.bRequiresManualSelection,
				bExpectedManual);
			FName SelectedTarget = NAME_None;
			if (bExpectedManual)
			{
				const FName DesiredTarget = Definition.TargetSpec.Mode == EGameXXKCardTargetMode::SingleEnemy
					? FName(TEXT("Enemy.A"))
					: FName(TEXT("Ally.A"));
				const FGameXXKCardTargetCandidateView* Candidate = Preview.TargetRequest.CandidateViews.FindByPredicate(
					[DesiredTarget](const FGameXXKCardTargetCandidateView& View)
					{
						return View.UnitId == DesiredTarget && View.bCanSelect;
					});
				if (!TestNotNull(FString::Printf(TEXT("desired target is legal: %s"), *Context), Candidate))
				{
					continue;
				}
				SelectedTarget = DesiredTarget;
			}

			FGameXXKCardPlayResult Result;
			Error.Reset();
			if (!TestTrue(
				FString::Printf(TEXT("base resolution succeeds: %s (%s)"), *Context, *Error),
				GameXXKCardRules::ResolveCardPlay(Runtime, PlayedInstanceId, SelectedTarget, Result, &Error)))
			{
				continue;
			}
			TestEqual(FString::Printf(TEXT("result CardId: %s"), *Context), Result.CardId, Definition.Id);
			TestEqual(FString::Printf(TEXT("result owner: %s"), *Context), Result.OwnerUnitId, FName(TEXT("Player")));
			TestEqual(FString::Printf(TEXT("active origin: %s"), *Context), Result.ResolutionOrigin, EGameXXKCardResolutionOrigin::ActivePlay);
			TestEqual(FString::Printf(TEXT("one active card counted: %s"), *Context), Runtime.ActiveCardsPlayedThisRound, 1);
			TestTrue(
				FString::Printf(TEXT("runtime remains valid: %s (%s)"), *Context, *Error),
				GameXXKCardRules::ValidateCardBattleRuntime(Runtime, &Error));
			ExecutedCardIds.Add(Definition.Id);
			++ResolutionCount;
		}

		if (Definition.Id == FName(TEXT("Route.Rare.JueJingFanJi")))
		{
			FGameXXKCardBattleRuntime Runtime;
			FName PlayedInstanceId;
			if (BuildRouteRuntime(*this, Definition, EGameXXKCardTerrain::Plain, true, Runtime, PlayedInstanceId))
			{
				FGameXXKCardPlayResult Result;
				FString Error;
				TestTrue(
					FString::Printf(TEXT("low-health route branch resolves: %s"), *Error),
					GameXXKCardRules::ResolveCardPlay(Runtime, PlayedInstanceId, TEXT("Enemy.A"), Result, &Error));
				++ResolutionCount;
			}
		}
	}
	TestEqual(TEXT("all thirty route CardIds execute"), ExecutedCardIds.Num(), 30);
	TestEqual(TEXT("ten terrain cards cover seven terrains and every other route card covers Plain plus the low-health branch"), ResolutionCount, 91);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteCardRecipeCombinationsTest,
	"GameXXK.Route.CardRecipe.Combinations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteCardRecipeCombinationsTest::RunTest(const FString& Parameters)
{
	for (const bool bWithCompanion : {false, true})
	{
		for (const bool bWithQuestNpc : {false, true})
		{
			const FString CaseLabel = FString::Printf(
				TEXT("companion=%s npc=%s"),
				bWithCompanion ? TEXT("yes") : TEXT("no"),
				bWithQuestNpc ? TEXT("yes") : TEXT("no"));
			const FGameXXKCardRunState Run = MakeRecipeRun(bWithCompanion, bWithQuestNpc);
			const FGameXXKCardRunState Before = Run;
			TArray<FGameXXKRouteCardEntry> Entries;
			FString Error;
			if (!TestTrue(
				FString::Printf(TEXT("the base recipe builds (%s): %s"), *CaseLabel, *Error),
				FGameXXKRouteCardRecipe::BuildBaseEntries(Run, RecipeRouteSeed, Entries, &Error)))
			{
				continue;
			}

			TestEqual(FString::Printf(TEXT("the recipe has exactly 18 entries (%s)"), *CaseLabel), Entries.Num(), 18);
			TestTrue(
				FString::Printf(TEXT("building never changes the source run (%s)"), *CaseLabel),
				FGameXXKCardRunState::StaticStruct()->CompareScriptStruct(&Run, &Before, PPF_None));
			if (Entries.Num() != 18)
			{
				continue;
			}

			const TArray<FName> ExpectedCardIds = BuildExpectedCardIds(Run, bWithCompanion, bWithQuestNpc);
			TSet<FName> EntryIds;
			for (int32 Index = 0; Index < Entries.Num(); ++Index)
			{
				const FGameXXKRouteCardEntry& Entry = Entries[Index];
				const FGameXXKCardDefinition* CardDefinition = FGameXXKCardCatalog::FindCardDefinition(Entry.CardId);
				FName ExpectedEntryId = NAME_None;
				TestTrue(
					FString::Printf(TEXT("the stable helper accepts base ordinal %d (%s)"), Index, *CaseLabel),
					FGameXXKRouteCardRecipe::MakeStableEntryId(RecipeRouteSeed, Index, ExpectedEntryId, &Error));
				TestEqual(FString::Printf(TEXT("card order %d (%s)"), Index, *CaseLabel), Entry.CardId, ExpectedCardIds[Index]);
				TestEqual(FString::Printf(TEXT("entry id %d (%s)"), Index, *CaseLabel), Entry.EntryId, ExpectedEntryId);
				TestFalse(FString::Printf(TEXT("entry id is unique %d (%s)"), Index, *CaseLabel), EntryIds.Contains(Entry.EntryId));
				EntryIds.Add(Entry.EntryId);
				TestEqual(FString::Printf(TEXT("ordinal %d (%s)"), Index, *CaseLabel), Entry.AcquisitionOrdinal, Index);
				TestFalse(FString::Printf(TEXT("base entry consumes no capacity %d (%s)"), Index, *CaseLabel), Entry.bConsumesRouteCapacity);
				TestNotNull(FString::Printf(TEXT("card exists %d (%s)"), Index, *CaseLabel), CardDefinition);
				if (CardDefinition)
				{
					TestEqual(
						FString::Printf(TEXT("quality comes from catalog %d (%s)"), Index, *CaseLabel),
						Entry.CurrentQuality,
						CardDefinition->BaseQuality);
				}

				if (Index < 8)
				{
					TestEqual(FString::Printf(TEXT("hero owner %d (%s)"), Index, *CaseLabel), Entry.OwnerUnitId, HeroUnitId);
					TestEqual(FString::Printf(TEXT("hero source %d (%s)"), Index, *CaseLabel), Entry.SourceKind, EGameXXKRouteCardSourceKind::HeroBase);
					TestFalse(FString::Printf(TEXT("hero base is durable %d (%s)"), Index, *CaseLabel), Entry.bTemporaryRouteCard);
				}
				else if (Index < 13 && bWithCompanion)
				{
					TestEqual(FString::Printf(TEXT("companion owner %d (%s)"), Index, *CaseLabel), Entry.OwnerUnitId, CompanionInstanceId);
					TestEqual(FString::Printf(TEXT("companion source %d (%s)"), Index, *CaseLabel), Entry.SourceKind, EGameXXKRouteCardSourceKind::CompanionBase);
					TestFalse(FString::Printf(TEXT("companion base is durable %d (%s)"), Index, *CaseLabel), Entry.bTemporaryRouteCard);
				}
				else if (Index >= 13 && Index < 16 && bWithQuestNpc)
				{
					TestEqual(FString::Printf(TEXT("npc owner %d (%s)"), Index, *CaseLabel), Entry.OwnerUnitId, QuestNpcId);
					TestEqual(FString::Printf(TEXT("npc source %d (%s)"), Index, *CaseLabel), Entry.SourceKind, EGameXXKRouteCardSourceKind::QuestNpcBase);
					TestFalse(FString::Printf(TEXT("npc base is durable %d (%s)"), Index, *CaseLabel), Entry.bTemporaryRouteCard);
				}
				else
				{
					TestEqual(FString::Printf(TEXT("filler/fixed owner %d (%s)"), Index, *CaseLabel), Entry.OwnerUnitId, HeroUnitId);
					TestEqual(FString::Printf(TEXT("filler/fixed source %d (%s)"), Index, *CaseLabel), Entry.SourceKind, EGameXXKRouteCardSourceKind::RouteBase);
					TestTrue(FString::Printf(TEXT("filler/fixed is temporary %d (%s)"), Index, *CaseLabel), Entry.bTemporaryRouteCard);
				}
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteCardRecipeStableIdentityTest,
	"GameXXK.Route.CardRecipe.StableIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteCardRecipeStableIdentityTest::RunTest(const FString& Parameters)
{
	const FGameXXKCardRunState Run = MakeRecipeRun(true, true);
	const FGameXXKCardRunState RunBefore = Run;
	TArray<FGameXXKRouteCardEntry> First;
	TArray<FGameXXKRouteCardEntry> Replay;
	TArray<FGameXXKRouteCardEntry> DifferentSeed;
	FString Error;
	TestTrue(TEXT("the first deterministic recipe builds"), FGameXXKRouteCardRecipe::BuildBaseEntries(Run, 42, First, &Error));
	TestTrue(TEXT("the deterministic recipe replays"), FGameXXKRouteCardRecipe::BuildBaseEntries(Run, 42, Replay, &Error));
	TestTrue(TEXT("a different-seed recipe builds"), FGameXXKRouteCardRecipe::BuildBaseEntries(Run, 43, DifferentSeed, &Error));
	TestTrue(TEXT("same input and seed reproduce every entry"), AreEntriesIdentical(First, Replay));
	TestTrue(TEXT("run overload remains pure"), FGameXXKCardRunState::StaticStruct()->CompareScriptStruct(&Run, &RunBefore, PPF_None));
	if (First.Num() == 18 && DifferentSeed.Num() == 18)
	{
		for (int32 Index = 0; Index < First.Num(); ++Index)
		{
			TestNotEqual(FString::Printf(TEXT("a different route seed changes id %d"), Index), First[Index].EntryId, DifferentSeed[Index].EntryId);
			TestEqual(FString::Printf(TEXT("a different route seed preserves card order %d"), Index), First[Index].CardId, DifferentSeed[Index].CardId);
		}
	}

	FGameXXKRuntimeState Runtime;
	Runtime.CardRun = Run;
	Runtime.RouteSeed = 777;
	Runtime.CardRun.RouteRandomSeed = 888;
	Runtime.ActiveBattleNodeId = 9001;
	const FGameXXKRuntimeState RuntimeBefore = Runtime;
	TArray<FGameXXKRouteCardEntry> RuntimeEntries;
	TestTrue(TEXT("runtime overload builds from its card-run state and explicit seed"), FGameXXKRouteCardRecipe::BuildBaseEntries(Runtime, 42, RuntimeEntries, &Error));
	TestTrue(TEXT("runtime and card-run overloads agree"), AreEntriesIdentical(First, RuntimeEntries));
	TestTrue(TEXT("runtime overload remains pure"), FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(&Runtime, &RuntimeBefore, PPF_None));

	FName AcquiredEntryId = NAME_None;
	TestTrue(TEXT("the stable helper accepts acquired ordinal 18"), FGameXXKRouteCardRecipe::MakeStableEntryId(42, 18, AcquiredEntryId, &Error));
	TestEqual(TEXT("the stable id uses fixed-width normalized seed and ordinal"), AcquiredEntryId, FName(TEXT("RouteEntry.0000002A.00000012")));
	FName ZeroSeedEntryId = NAME_None;
	FName ZeroSeedReplayId = NAME_None;
	TestTrue(TEXT("zero seed uses a deterministic fallback"), FGameXXKRouteCardRecipe::MakeStableEntryId(0, 18, ZeroSeedEntryId, &Error));
	TestTrue(TEXT("zero seed fallback replays"), FGameXXKRouteCardRecipe::MakeStableEntryId(0, 18, ZeroSeedReplayId, &Error));
	TestEqual(TEXT("zero seed fallback is stable"), ZeroSeedEntryId, ZeroSeedReplayId);
	TestEqual(TEXT("zero seed fallback is explicitly normalized"), ZeroSeedEntryId, FName(TEXT("RouteEntry.13579BDF.00000012")));

	FName RejectedId(TEXT("Sentinel.Entry"));
	TestFalse(TEXT("negative ordinals are rejected"), FGameXXKRouteCardRecipe::MakeStableEntryId(42, -1, RejectedId, &Error));
	TestEqual(TEXT("negative-ordinal rejection preserves the caller output"), RejectedId, FName(TEXT("Sentinel.Entry")));
	TestTrue(TEXT("negative-ordinal rejection reports an error"), !Error.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteCardRecipeAtomicFailureTest,
	"GameXXK.Route.CardRecipe.AtomicFailure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteCardRecipeAtomicFailureTest::RunTest(const FString& Parameters)
{
	const auto MakeSentinelEntries = []()
	{
		TArray<FGameXXKRouteCardEntry> Entries;
		FGameXXKRouteCardEntry& Sentinel = Entries.AddDefaulted_GetRef();
		Sentinel.EntryId = TEXT("Sentinel.Entry");
		Sentinel.CardId = TEXT("Route.General.PoJiaTuCi");
		Sentinel.AcquisitionOrdinal = 77;
		return Entries;
	};

	FString Error;
	FGameXXKCardRunState BrokenHero = MakeRecipeRun(true, true);
	BrokenHero.HeroSelectedCardIds.Pop();
	const FGameXXKCardRunState BrokenHeroBefore = BrokenHero;
	TArray<FGameXXKRouteCardEntry> HeroOutput = MakeSentinelEntries();
	const TArray<FGameXXKRouteCardEntry> HeroOutputBefore = HeroOutput;
	TestFalse(TEXT("a broken hero loadout fails"), FGameXXKRouteCardRecipe::BuildBaseEntries(BrokenHero, RecipeRouteSeed, HeroOutput, &Error));
	TestTrue(TEXT("broken hero failure preserves output"), AreEntriesIdentical(HeroOutput, HeroOutputBefore));
	TestTrue(TEXT("broken hero failure preserves input"), FGameXXKCardRunState::StaticStruct()->CompareScriptStruct(&BrokenHero, &BrokenHeroBefore, PPF_None));
	TestTrue(TEXT("broken hero failure reports an error"), !Error.IsEmpty());

	FGameXXKCardRunState BrokenCompanion = MakeRecipeRun(true, true);
	BrokenCompanion.CompanionRoster.PermanentCompanions[0].SelectedCardIds.Pop();
	const FGameXXKCardRunState BrokenCompanionBefore = BrokenCompanion;
	TArray<FGameXXKRouteCardEntry> CompanionOutput = MakeSentinelEntries();
	const TArray<FGameXXKRouteCardEntry> CompanionOutputBefore = CompanionOutput;
	TestFalse(TEXT("a broken companion loadout fails"), FGameXXKRouteCardRecipe::BuildBaseEntries(BrokenCompanion, RecipeRouteSeed, CompanionOutput, &Error));
	TestTrue(TEXT("broken companion failure preserves output"), AreEntriesIdentical(CompanionOutput, CompanionOutputBefore));
	TestTrue(TEXT("broken companion failure preserves input"), FGameXXKCardRunState::StaticStruct()->CompareScriptStruct(&BrokenCompanion, &BrokenCompanionBefore, PPF_None));

	FGameXXKCardRunState LockedCompanionCard = MakeRecipeRun(true, true);
	FGameXXKPermanentCompanion& LockedCardCompanion = LockedCompanionCard.CompanionRoster.PermanentCompanions[0];
	FName LockedSameRoleCardId = NAME_None;
	for (const FGameXXKCardDefinition& Definition : FGameXXKCardCatalog::GetAllCardDefinitions())
	{
		if (Definition.Owner == EGameXXKCardOwner::Profession
			&& Definition.Role == LockedCardCompanion.Role
			&& !LockedCardCompanion.UnlockedPersonalCardIds.Contains(Definition.Id))
		{
			LockedSameRoleCardId = Definition.Id;
			break;
		}
	}
	TestFalse(TEXT("the corrupt-loadout fixture finds a locked same-role card"), LockedSameRoleCardId.IsNone());
	if (!LockedSameRoleCardId.IsNone())
	{
		LockedCardCompanion.SelectedCardIds.Last() = LockedSameRoleCardId;
		const FGameXXKCardRunState LockedCompanionCardBefore = LockedCompanionCard;
		TArray<FGameXXKRouteCardEntry> LockedCardOutput = MakeSentinelEntries();
		const TArray<FGameXXKRouteCardEntry> LockedCardOutputBefore = LockedCardOutput;
		TestFalse(
			TEXT("a same-role card outside the companion's unlocked personal pool fails"),
			FGameXXKRouteCardRecipe::BuildBaseEntries(LockedCompanionCard, RecipeRouteSeed, LockedCardOutput, &Error));
		TestTrue(TEXT("locked companion-card failure preserves output"), AreEntriesIdentical(LockedCardOutput, LockedCardOutputBefore));
		TestTrue(
			TEXT("locked companion-card failure preserves input"),
			FGameXXKCardRunState::StaticStruct()->CompareScriptStruct(&LockedCompanionCard, &LockedCompanionCardBefore, PPF_None));
	}

	FGameXXKCardRunState BrokenNpc = MakeRecipeRun(true, true);
	BrokenNpc.PartySelection.QuestNpc.SelectedCardIds[2] = TEXT("Card.Does.Not.Exist");
	const FGameXXKCardRunState BrokenNpcBefore = BrokenNpc;
	TArray<FGameXXKRouteCardEntry> NpcOutput = MakeSentinelEntries();
	const TArray<FGameXXKRouteCardEntry> NpcOutputBefore = NpcOutput;
	TestFalse(TEXT("a late broken NPC card fails"), FGameXXKRouteCardRecipe::BuildBaseEntries(BrokenNpc, RecipeRouteSeed, NpcOutput, &Error));
	TestTrue(TEXT("late NPC failure atomically preserves output"), AreEntriesIdentical(NpcOutput, NpcOutputBefore));
	TestTrue(TEXT("late NPC failure preserves input"), FGameXXKCardRunState::StaticStruct()->CompareScriptStruct(&BrokenNpc, &BrokenNpcBefore, PPF_None));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteCardRecipeStateContractTest,
	"GameXXK.Route.CardRecipe.StateContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteCardRecipeStateContractTest::RunTest(const FString& Parameters)
{
	const FGameXXKCardRunState DefaultRun;
	TestEqual(TEXT("next route-card entry ordinal defaults to zero"), DefaultRun.NextRouteCardEntryOrdinal, 0);
	TestEqual(TEXT("legacy reward ordinal still defaults independently"), DefaultRun.NextRewardOrdinal, 0);

	FGameXXKCardRunState IndependentRun;
	IndependentRun.NextRewardOrdinal = 37;
	TestEqual(TEXT("changing reward ordinal does not change route-card ordinal"), IndependentRun.NextRouteCardEntryOrdinal, 0);
	IndependentRun.NextRouteCardEntryOrdinal = 18;
	TestEqual(TEXT("changing route-card ordinal does not change reward ordinal"), IndependentRun.NextRewardOrdinal, 37);

	const FProperty* Property = FindFProperty<FProperty>(
		FGameXXKCardRunState::StaticStruct(),
		GET_MEMBER_NAME_CHECKED(FGameXXKCardRunState, NextRouteCardEntryOrdinal));
	TestNotNull(TEXT("next route-card entry ordinal is reflected"), Property);
	if (Property)
	{
		TestTrue(TEXT("next route-card entry ordinal is SaveGame state"), Property->HasAnyPropertyFlags(CPF_SaveGame));
		TestTrue(TEXT("next route-card entry ordinal is an int32"), Property->IsA<FIntProperty>());
	}
	return true;
}

#endif
