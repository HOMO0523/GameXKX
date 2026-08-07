#include "GameXXKCardCatalog.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCardCatalogTest,
	"GameXXK.Data.CardCatalog",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
	bool CanResolveSelectedTarget(const EGameXXKCardTargetMode Mode)
	{
		switch (Mode)
		{
		case EGameXXKCardTargetMode::SingleEnemy:
		case EGameXXKCardTargetMode::SingleAlly:
		case EGameXXKCardTargetMode::OtherAlly:
		case EGameXXKCardTargetMode::RandomEnemy:
		case EGameXXKCardTargetMode::LowestHealthAlly:
		case EGameXXKCardTargetMode::LowestHealthOtherAlly:
		case EGameXXKCardTargetMode::AnyLivingUnit:
			return true;
		default:
			return false;
		}
	}

	const FGameXXKCardDefinition* RequireCard(FAutomationTestBase& Test, const TCHAR* CardId)
	{
		const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(FName(CardId));
		Test.TestNotNull(FString::Printf(TEXT("catalog contains %s"), CardId), Definition);
		return Definition;
	}

	bool HasEffect(
		const FGameXXKCardDefinition& Definition,
		const EGameXXKCardEffectType Type,
		const EGameXXKCardEffectTarget Target,
		const int32 Magnitude)
	{
		return Definition.Effects.ContainsByPredicate([Type, Target, Magnitude](const FGameXXKCardEffect& Effect)
		{
			return Effect.Type == Type && Effect.Target == Target && Effect.Magnitude == Magnitude;
		});
	}

	int32 CountEffects(const FGameXXKCardDefinition& Definition, const EGameXXKCardEffectType Type)
	{
		int32 Count = 0;
		for (const FGameXXKCardEffect& Effect : Definition.Effects)
		{
			if (Effect.Type == Type)
			{
				++Count;
			}
		}
		return Count;
	}

	void TestDefinitionIsRejected(FAutomationTestBase& Test, const TCHAR* TestName, const FGameXXKCardDefinition& Definition)
	{
		FString Error;
		Test.TestFalse(TestName, FGameXXKCardCatalog::ValidateCardDefinition(Definition, Error));
		Test.TestFalse(FString::Printf(TEXT("%s reports a validation error"), TestName), Error.IsEmpty());
	}

	void AddAllAlliesTerrainOverride(FGameXXKCardDefinition& Definition)
	{
		FGameXXKCardTargetModeOverride Override;
		Override.ConditionType = EGameXXKCardTargetModeOverrideConditionType::TerrainIsAny;
		Override.Terrain = EGameXXKCardTerrain::Forest;
		Override.Mode = EGameXXKCardTargetMode::AllAllies;
		Override.Presentation = EGameXXKCardTargetPresentation::Group;
		Definition.TargetSpec.ModeOverrides.Add(Override);
	}
}

bool FGameXXKCardCatalogTest::RunTest(const FString& Parameters)
{
	const TArray<FGameXXKCardDefinition>& Definitions = FGameXXKCardCatalog::GetAllCardDefinitions();
	TestEqual(TEXT("catalog contains the approved 174 card definitions"), Definitions.Num(), 174);
	TestEqual(TEXT("target mode invalid remains serialized as zero"), static_cast<uint8>(EGameXXKCardTargetMode::Invalid), static_cast<uint8>(0));
	TestEqual(TEXT("target mode terminal value remains stable"), static_cast<uint8>(EGameXXKCardTargetMode::AnyLivingUnit), static_cast<uint8>(12));
	TestEqual(TEXT("target presentation invalid remains serialized as zero"), static_cast<uint8>(EGameXXKCardTargetPresentation::Invalid), static_cast<uint8>(0));
	TestEqual(TEXT("target presentation terminal value remains stable"), static_cast<uint8>(EGameXXKCardTargetPresentation::Group), static_cast<uint8>(5));
	TestEqual(TEXT("card owner invalid remains serialized as zero"), static_cast<uint8>(EGameXXKCardOwner::Invalid), static_cast<uint8>(0));
	TestEqual(TEXT("card rarity invalid remains serialized as zero"), static_cast<uint8>(EGameXXKCardRarity::Invalid), static_cast<uint8>(0));
	TestEqual(TEXT("character role invalid remains serialized as zero"), static_cast<uint8>(EGameXXKCharacterRole::Invalid), static_cast<uint8>(0));
	TestEqual(TEXT("card state invalid remains serialized as zero"), static_cast<uint8>(EGameXXKCardState::Invalid), static_cast<uint8>(0));
	TestEqual(TEXT("unit state invalid remains serialized as zero"), static_cast<uint8>(EGameXXKCardUnitState::Invalid), static_cast<uint8>(0));
	TestEqual(TEXT("card status invalid remains serialized as zero"), static_cast<uint8>(EGameXXKCardStatus::Invalid), static_cast<uint8>(0));
	TestEqual(TEXT("card terrain invalid remains serialized as zero"), static_cast<uint8>(EGameXXKCardTerrain::Invalid), static_cast<uint8>(0));
	TestEqual(TEXT("effect target invalid remains serialized as zero"), static_cast<uint8>(EGameXXKCardEffectTarget::Invalid), static_cast<uint8>(0));
	TestEqual(TEXT("effect type invalid remains serialized as zero"), static_cast<uint8>(EGameXXKCardEffectType::Invalid), static_cast<uint8>(0));
	TestEqual(TEXT("effect condition none remains serialized as zero"), static_cast<uint8>(EGameXXKCardEffectConditionType::None), static_cast<uint8>(0));
	TestEqual(TEXT("effect condition terminal value remains stable"), static_cast<uint8>(EGameXXKCardEffectConditionType::OwnerHasDamageOverTime), static_cast<uint8>(8));
	TestEqual(TEXT("modifier trigger invalid remains serialized as zero"), static_cast<uint8>(EGameXXKCardBattleModifierTrigger::Invalid), static_cast<uint8>(0));
	int32 StrongNpcCardCount = 0;
	for (const FGameXXKCardDefinition& Definition : Definitions)
	{
		if (Definition.Owner != EGameXXKCardOwner::QuestNpc || Definition.EnergyCost < 2)
		{
			continue;
		}
		++StrongNpcCardCount;
		TestTrue(
			FString::Printf(TEXT("strong NPC card %s spends owner mana instead of remaining free"), *Definition.Id.ToString()),
			Definition.ManaCost >= 6);
	}
	TestTrue(TEXT("the NPC catalog contains strong cards covered by the mana-cost rule"), StrongNpcCardCount > 0);
	TestEqual(TEXT("guard redirect policy invalid remains serialized as zero"), static_cast<uint8>(EGameXXKCardGuardRedirectPolicy::Invalid), static_cast<uint8>(0));
	TestEqual(TEXT("modifier recipient scope invalid remains serialized as zero"), static_cast<uint8>(EGameXXKCardModifierRecipientScope::Invalid), static_cast<uint8>(0));
	TestEqual(TEXT("modifier expiry invalid remains serialized as zero"), static_cast<uint8>(EGameXXKCardModifierExpiry::Invalid), static_cast<uint8>(0));
	TestEqual(TEXT("triggered attack target scope invalid remains serialized as zero"), static_cast<uint8>(EGameXXKCardTriggeredAttackTargetScope::Invalid), static_cast<uint8>(0));
	TestEqual(TEXT("target override condition invalid remains serialized as zero"), static_cast<uint8>(EGameXXKCardTargetModeOverrideConditionType::Invalid), static_cast<uint8>(0));

	int32 HeroCount = 0;
	int32 ProfessionCount = 0;
	int32 QuestNpcCount = 0;
	int32 RouteCount = 0;
	int32 IdentityLockedCount = 0;
	TSet<FName> UniqueIds;

	for (const FGameXXKCardDefinition& Definition : Definitions)
	{
		TestFalse(FString::Printf(TEXT("card id %s is unique"), *Definition.Id.ToString()), UniqueIds.Contains(Definition.Id));
		UniqueIds.Add(Definition.Id);
		TestTrue(FString::Printf(TEXT("card %s explicitly declares a target mode"), *Definition.Id.ToString()), Definition.TargetSpec.Mode != EGameXXKCardTargetMode::Invalid);
		TestTrue(FString::Printf(TEXT("card %s has a non-negative target status stack minimum"), *Definition.Id.ToString()), Definition.TargetSpec.RequiredStatusMinimumStacks >= 0);

		switch (Definition.Owner)
		{
		case EGameXXKCardOwner::Hero:
			++HeroCount;
			break;
		case EGameXXKCardOwner::Profession:
			++ProfessionCount;
			break;
		case EGameXXKCardOwner::QuestNpc:
			++QuestNpcCount;
			break;
		case EGameXXKCardOwner::Route:
			++RouteCount;
			break;
		default:
			AddError(FString::Printf(TEXT("card %s has an invalid catalog owner"), *Definition.Id.ToString()));
			break;
		}

		if (Definition.Owner == EGameXXKCardOwner::Hero || Definition.Owner == EGameXXKCardOwner::QuestNpc)
		{
			TestTrue(FString::Printf(TEXT("identity-bound card %s has an identity lock"), *Definition.Id.ToString()), Definition.bIdentityLocked);
			++IdentityLockedCount;
		}

		for (const FGameXXKCardEffect& Effect : Definition.Effects)
		{
			if (Effect.Target == EGameXXKCardEffectTarget::SelectedTarget)
			{
				TestTrue(
					FString::Printf(TEXT("SelectedTarget effect on %s is compatible with its target mode"), *Definition.Id.ToString()),
					CanResolveSelectedTarget(Definition.TargetSpec.Mode));
			}
		}
	}

	TestEqual(TEXT("hero card count"), HeroCount, 12);
	TestEqual(TEXT("profession card count"), ProfessionCount, 108);
	TestEqual(TEXT("quest NPC card count"), QuestNpcCount, 24);
	TestEqual(TEXT("route reward card count"), RouteCount, 30);
	TestEqual(TEXT("hero and NPC identity locks"), IdentityLockedCount, 36);
	TestEqual(TEXT("all card ids are unique"), UniqueIds.Num(), Definitions.Num());
	TestEqual(TEXT("hero owner query preserves the hero deck size"), FGameXXKCardCatalog::GetCardDefinitionsForOwner(FName(TEXT("Hero"))).Num(), 12);

	FGameXXKCardDefinition MissingCard;
	TestFalse(TEXT("missing card lookup returns false"), FGameXXKCardCatalog::FindCardDefinition(FName(TEXT("Missing.Card")), MissingCard));
	TestNull(TEXT("pointer lookup returns null for a missing card"), FGameXXKCardCatalog::FindCardDefinition(FName(TEXT("Missing.Card"))));

	const TArray<FGameXXKCardVisualDefinition>& Visuals = FGameXXKCardCatalog::GetCardVisualDefinitions();
	TestEqual(TEXT("each card has one visual recipe"), Visuals.Num(), 174);
	for (const FGameXXKCardDefinition& Definition : Definitions)
	{
		const FGameXXKCardVisualDefinition* Visual = FGameXXKCardCatalog::FindCardVisualDefinition(Definition.Id);
		TestNotNull(FString::Printf(TEXT("visual recipe exists for %s"), *Definition.Id.ToString()), Visual);
		if (Visual)
		{
			TestEqual(FString::Printf(TEXT("visual art key stays aligned for %s"), *Definition.Id.ToString()), Visual->ArtKey, Definition.VisualArtKey);
			TestEqual(FString::Printf(TEXT("visual frame key stays aligned for %s"), *Definition.Id.ToString()), Visual->FrameKey, Definition.FrameKey);
			TestEqual(FString::Printf(TEXT("visual identity lock stays aligned for %s"), *Definition.Id.ToString()), Visual->bIdentityLocked, Definition.bIdentityLocked);
			TestFalse(FString::Printf(TEXT("visual source art key exists for %s"), *Definition.Id.ToString()), Visual->SourceArtKey.IsNone());
			TestFalse(FString::Printf(TEXT("visual overlay key exists for %s"), *Definition.Id.ToString()), Visual->OverlayKey.IsNone());
			if (Definition.Owner == EGameXXKCardOwner::Hero || Definition.Owner == EGameXXKCardOwner::QuestNpc)
			{
				TestFalse(FString::Printf(TEXT("identity-bound visual has a stable subject key for %s"), *Definition.Id.ToString()), Visual->IdentitySubjectKey.IsNone());
				TestFalse(FString::Printf(TEXT("identity-bound visual source is not copied from its card art key for %s"), *Definition.Id.ToString()), Visual->SourceArtKey == Definition.VisualArtKey);
			}
		}
	}

	FString ValidationError;
	TestTrue(TEXT("the approved card catalog validates"), FGameXXKCardCatalog::ValidateCardDefinitions(ValidationError));
	TestTrue(TEXT("successful validation has no error text"), ValidationError.IsEmpty());

	if (const FGameXXKCardDefinition* QingFengYiShi = RequireCard(*this, TEXT("Hero.QingFengYiShi")))
	{
		FGameXXKCardDefinition InvalidHitCount = *QingFengYiShi;
		InvalidHitCount.Effects[0].HitCount = 0;
		TestDefinitionIsRejected(*this, TEXT("public validator rejects a non-positive hit count"), InvalidHitCount);

		FGameXXKCardDefinition InvalidTargetPresentation = *QingFengYiShi;
		InvalidTargetPresentation.TargetSpec.Presentation = EGameXXKCardTargetPresentation::Group;
		TestDefinitionIsRejected(*this, TEXT("public validator rejects an incompatible target presentation"), InvalidTargetPresentation);

		FGameXXKCardDefinition InvalidRequireDifferent = *QingFengYiShi;
		InvalidRequireDifferent.TargetSpec.bRequireDifferentFromOwner = true;
		TestDefinitionIsRejected(*this, TEXT("public validator rejects incompatible require-different targeting"), InvalidRequireDifferent);

		FGameXXKCardDefinition InvalidForbiddenStatus = *QingFengYiShi;
		InvalidForbiddenStatus.TargetSpec.ForbiddenStatus = EGameXXKCardStatus::Invalid;
		TestDefinitionIsRejected(*this, TEXT("public validator rejects an invalid forbidden target status"), InvalidForbiddenStatus);

		FGameXXKCardDefinition InvalidMinimumHealth = *QingFengYiShi;
		InvalidMinimumHealth.TargetSpec.MinimumHealthPercent = -1.0f;
		TestDefinitionIsRejected(*this, TEXT("public validator rejects a target health minimum below zero"), InvalidMinimumHealth);

		FGameXXKCardDefinition InvalidMaximumHealth = *QingFengYiShi;
		InvalidMaximumHealth.TargetSpec.MaximumHealthPercent = 101.0f;
		TestDefinitionIsRejected(*this, TEXT("public validator rejects a target health maximum above one hundred"), InvalidMaximumHealth);

		FGameXXKCardDefinition InvertedTargetHealthRange = *QingFengYiShi;
		InvertedTargetHealthRange.TargetSpec.MinimumHealthPercent = 80.0f;
		InvertedTargetHealthRange.TargetSpec.MaximumHealthPercent = 20.0f;
		TestDefinitionIsRejected(*this, TEXT("public validator rejects an inverted target health range"), InvertedTargetHealthRange);

		FGameXXKCardDefinition AlternateTerrainWithoutPrimary = *QingFengYiShi;
		AlternateTerrainWithoutPrimary.TargetSpec.AlternateRequiredTerrain = EGameXXKCardTerrain::Forest;
		TestDefinitionIsRejected(*this, TEXT("public validator rejects an alternate target terrain without a primary terrain"), AlternateTerrainWithoutPrimary);

		FGameXXKCardDefinition GroupOverrideForSelectedTarget = *QingFengYiShi;
		FGameXXKCardTargetModeOverride GroupOverride;
		GroupOverride.ConditionType = EGameXXKCardTargetModeOverrideConditionType::TerrainIsAny;
		GroupOverride.Terrain = EGameXXKCardTerrain::Forest;
		GroupOverride.Mode = EGameXXKCardTargetMode::AllAllies;
		GroupOverride.Presentation = EGameXXKCardTargetPresentation::Group;
		GroupOverrideForSelectedTarget.TargetSpec.ModeOverrides.Add(GroupOverride);
		TestDefinitionIsRejected(*this, TEXT("public validator rejects an active SelectedTarget effect under an all-allies override"), GroupOverrideForSelectedTarget);
	}

	if (const FGameXXKCardDefinition* FengShenBu = RequireCard(*this, TEXT("Hero.FengShenBu")))
	{
		FGameXXKCardDefinition MissingAppliedStatus = *FengShenBu;
		MissingAppliedStatus.Effects[0].Status = EGameXXKCardStatus::None;
		TestDefinitionIsRejected(*this, TEXT("public validator rejects an apply-status effect without a status"), MissingAppliedStatus);
	}

	if (const FGameXXKCardDefinition* DiMaiJieLi = RequireCard(*this, TEXT("Profession.FormationMaster.DiMaiJieLi")))
	{
		FGameXXKCardDefinition InvalidTerrainCondition = *DiMaiJieLi;
		InvalidTerrainCondition.Effects[2].Condition.Terrain = EGameXXKCardTerrain::Invalid;
		TestDefinitionIsRejected(*this, TEXT("public validator rejects TerrainIsAny without a terrain"), InvalidTerrainCondition);
	}

	if (const FGameXXKCardDefinition* PoYunYiShan = RequireCard(*this, TEXT("Hero.PoYunYiShan")))
	{
		FGameXXKCardDefinition InapplicableStatusConsumption = *PoYunYiShan;
		InapplicableStatusConsumption.Effects[1].Condition.Type = EGameXXKCardEffectConditionType::TerrainIsAny;
		InapplicableStatusConsumption.Effects[1].Condition.Terrain = EGameXXKCardTerrain::Plain;
		TestDefinitionIsRejected(*this, TEXT("public validator rejects status consumption on a non-status condition"), InapplicableStatusConsumption);

		FGameXXKCardDefinition ConsumptionResultBeforeProducer = *PoYunYiShan;
		ConsumptionResultBeforeProducer.Effects.Swap(1, 2);
		TestDefinitionIsRejected(*this, TEXT("public validator rejects a consumption result before its producer"), ConsumptionResultBeforeProducer);
	}

	if (const FGameXXKCardDefinition* YinShuiHuiYuan = RequireCard(*this, TEXT("Profession.FormationMaster.YinShuiHuiYuan")))
	{
		FGameXXKCardDefinition InvalidTargetOverride = *YinShuiHuiYuan;
		InvalidTargetOverride.TargetSpec.ModeOverrides[0].Mode = EGameXXKCardTargetMode::SingleAlly;
		TestDefinitionIsRejected(*this, TEXT("public validator derives target override presentation from its mode"), InvalidTargetOverride);
	}

	if (const FGameXXKCardDefinition* TieYiYiJue = RequireCard(*this, TEXT("Route.Rare.TieYiYiJue")))
	{
		FGameXXKCardDefinition NestedModifierRecipientTarget = *TieYiYiJue;
		NestedModifierRecipientTarget.TargetSpec.Mode = EGameXXKCardTargetMode::SingleAlly;
		NestedModifierRecipientTarget.TargetSpec.Presentation = EGameXXKCardTargetPresentation::PlayerSelectsUnit;
		NestedModifierRecipientTarget.Effects[1].Target = EGameXXKCardEffectTarget::CardOwner;
		NestedModifierRecipientTarget.Effects[1].Modifier.RecipientTarget = EGameXXKCardEffectTarget::SelectedTarget;
		AddAllAlliesTerrainOverride(NestedModifierRecipientTarget);
		TestDefinitionIsRejected(*this, TEXT("public validator rejects a nested modifier recipient target under an all-allies override"), NestedModifierRecipientTarget);

		FGameXXKCardDefinition NestedModifierTarget = *TieYiYiJue;
		NestedModifierTarget.TargetSpec.Mode = EGameXXKCardTargetMode::SingleAlly;
		NestedModifierTarget.TargetSpec.Presentation = EGameXXKCardTargetPresentation::PlayerSelectsUnit;
		NestedModifierTarget.Effects[1].Target = EGameXXKCardEffectTarget::CardOwner;
		NestedModifierTarget.Effects[1].Modifier.Target = EGameXXKCardEffectTarget::SelectedTarget;
		AddAllAlliesTerrainOverride(NestedModifierTarget);
		TestDefinitionIsRejected(*this, TEXT("public validator rejects a nested modifier effect target under an all-allies override"), NestedModifierTarget);
	}

	if (const FGameXXKCardDefinition* HuZhu = RequireCard(*this, TEXT("Profession.Guard.HuZhu")))
	{
		FGameXXKCardDefinition NestedGuardProtectedTarget = *HuZhu;
		for (FGameXXKCardEffect& Effect : NestedGuardProtectedTarget.Effects)
		{
			if (Effect.Type == EGameXXKCardEffectType::ApplyGuardLink)
			{
				Effect.Target = EGameXXKCardEffectTarget::CardOwner;
				Effect.GuardLink.ProtectedUnit = EGameXXKCardEffectTarget::SelectedTarget;
				break;
			}
		}
		NestedGuardProtectedTarget.Effects[1].Target = EGameXXKCardEffectTarget::CardOwner;
		AddAllAlliesTerrainOverride(NestedGuardProtectedTarget);
		TestDefinitionIsRejected(*this, TEXT("public validator rejects a nested guard protected target under an all-allies override"), NestedGuardProtectedTarget);

		FGameXXKCardDefinition NestedGuardGuardianTarget = *HuZhu;
		for (FGameXXKCardEffect& Effect : NestedGuardGuardianTarget.Effects)
		{
			if (Effect.Type == EGameXXKCardEffectType::ApplyGuardLink)
			{
				Effect.Target = EGameXXKCardEffectTarget::CardOwner;
				Effect.GuardLink.Guardian = EGameXXKCardEffectTarget::SelectedTarget;
				Effect.GuardLink.ProtectedUnit = EGameXXKCardEffectTarget::CardOwner;
				break;
			}
		}
		NestedGuardGuardianTarget.Effects[1].Target = EGameXXKCardEffectTarget::CardOwner;
		AddAllAlliesTerrainOverride(NestedGuardGuardianTarget);
		TestDefinitionIsRejected(*this, TEXT("public validator rejects a nested guard guardian target under an all-allies override"), NestedGuardGuardianTarget);
	}

	for (const FGameXXKCardDefinition& Definition : Definitions)
	{
		const bool bUsesBareGuardStatus = Definition.Effects.ContainsByPredicate([](const FGameXXKCardEffect& Effect)
		{
			return (Effect.Type == EGameXXKCardEffectType::ApplyStatus && Effect.Status == EGameXXKCardStatus::Guard)
				|| (Effect.Type == EGameXXKCardEffectType::ApplyBattleModifier && Effect.Modifier.EffectType == EGameXXKCardEffectType::ApplyStatus && Effect.Modifier.Status == EGameXXKCardStatus::Guard);
		});
		TestFalse(FString::Printf(TEXT("%s does not encode guard as a bare status"), *Definition.Id.ToString()), bUsesBareGuardStatus);
	}

	const auto TestTwoLayerPartyGuardLink = [this](const TCHAR* CardId)
	{
		if (const FGameXXKCardDefinition* Definition = RequireCard(*this, CardId))
		{
			TestEqual(FString::Printf(TEXT("%s has exactly one guard link"), CardId), CountEffects(*Definition, EGameXXKCardEffectType::ApplyGuardLink), 1);
			const FGameXXKCardEffect* GuardLink = Definition->Effects.FindByPredicate([](const FGameXXKCardEffect& Effect)
			{
				return Effect.Type == EGameXXKCardEffectType::ApplyGuardLink;
			});
			TestNotNull(FString::Printf(TEXT("%s represents its two-layer guard as a link"), CardId), GuardLink);
			if (GuardLink)
			{
				TestEqual(FString::Printf(TEXT("%s uses the card owner as guardian"), CardId), GuardLink->GuardLink.Guardian, EGameXXKCardEffectTarget::CardOwner);
				TestEqual(FString::Printf(TEXT("%s protects all other allies"), CardId), GuardLink->GuardLink.ProtectedUnit, EGameXXKCardEffectTarget::AllOtherAllies);
				TestEqual(FString::Printf(TEXT("%s retains two guard layers"), CardId), GuardLink->GuardLink.Stacks, 2);
				TestEqual(FString::Printf(TEXT("%s redirects the next single-target direct attack"), CardId), GuardLink->GuardLink.RedirectPolicy, EGameXXKCardGuardRedirectPolicy::RedirectNextSingleTargetDirectAttackToGuardian);
			}
		}
	};
	TestTwoLayerPartyGuardLink(TEXT("Profession.Guard.BuDongRuShan"));
	TestTwoLayerPartyGuardLink(TEXT("Profession.Guard.YiFuDangGuan"));

	if (const FGameXXKCardDefinition* YuanHuBu = RequireCard(*this, TEXT("Profession.Guard.YuanHuBu")))
	{
		TestEqual(TEXT("YuanHuBu uses the automatic lowest-health ally target mode"), YuanHuBu->TargetSpec.Mode, EGameXXKCardTargetMode::LowestHealthAlly);
		TestFalse(TEXT("YuanHuBu may protect its card owner when it is the lowest-health ally"), YuanHuBu->TargetSpec.bRequireDifferentFromOwner);
		TestEqual(TEXT("YuanHuBu has exactly one guard link"), CountEffects(*YuanHuBu, EGameXXKCardEffectType::ApplyGuardLink), 1);
		const FGameXXKCardEffect* GuardLink = YuanHuBu->Effects.FindByPredicate([](const FGameXXKCardEffect& Effect)
		{
			return Effect.Type == EGameXXKCardEffectType::ApplyGuardLink;
		});
		TestNotNull(TEXT("YuanHuBu represents a guard link instead of only a guard status"), GuardLink);
		if (GuardLink)
		{
			TestEqual(TEXT("YuanHuBu guard link uses the card owner as guardian"), GuardLink->GuardLink.Guardian, EGameXXKCardEffectTarget::CardOwner);
			TestEqual(TEXT("YuanHuBu guard link protects the automatic lowest-health ally"), GuardLink->GuardLink.ProtectedUnit, EGameXXKCardEffectTarget::LowestHealthAlly);
			TestEqual(TEXT("YuanHuBu guard link has one stack"), GuardLink->GuardLink.Stacks, 1);
			TestEqual(TEXT("YuanHuBu guard link redirects the next single-target direct attack"), GuardLink->GuardLink.RedirectPolicy, EGameXXKCardGuardRedirectPolicy::RedirectNextSingleTargetDirectAttackToGuardian);
		}
	}

	if (const FGameXXKCardDefinition* HuZhu = RequireCard(*this, TEXT("Profession.Guard.HuZhu")))
	{
		TestEqual(TEXT("HuZhu has exactly one guard link"), CountEffects(*HuZhu, EGameXXKCardEffectType::ApplyGuardLink), 1);
		const FGameXXKCardEffect* GuardLink = HuZhu->Effects.FindByPredicate([](const FGameXXKCardEffect& Effect)
		{
			return Effect.Type == EGameXXKCardEffectType::ApplyGuardLink;
		});
		TestNotNull(TEXT("HuZhu uses a data-only guard link"), GuardLink);
		if (GuardLink)
		{
			TestEqual(TEXT("HuZhu guard link binds the owner as guardian"), GuardLink->GuardLink.Guardian, EGameXXKCardEffectTarget::CardOwner);
			TestEqual(TEXT("HuZhu guard link binds the selected ally as protected"), GuardLink->GuardLink.ProtectedUnit, EGameXXKCardEffectTarget::SelectedTarget);
			TestEqual(TEXT("HuZhu guard link has one stack"), GuardLink->GuardLink.Stacks, 1);
			TestEqual(TEXT("HuZhu guard link redirects the next single-target direct attack"), GuardLink->GuardLink.RedirectPolicy, EGameXXKCardGuardRedirectPolicy::RedirectNextSingleTargetDirectAttackToGuardian);
		}
	}

	if (const FGameXXKCardDefinition* HeJiLing = RequireCard(*this, TEXT("Route.General.HeJiLing")))
	{
		TestTrue(TEXT("HeJiLing represents each living ally attacking the selected target as a data effect"), HasEffect(*HeJiLing, EGameXXKCardEffectType::EachLivingAllyAttackSelectedTarget, EGameXXKCardEffectTarget::SelectedTarget, 50));
	}

	if (const FGameXXKCardDefinition* SheLingHuo = RequireCard(*this, TEXT("Profession.Sorcerer.SheLingHuo")))
	{
		const FGameXXKCardEffect* ConsumeBurnEffect = SheLingHuo->Effects.FindByPredicate([](const FGameXXKCardEffect& Effect)
		{
			return Effect.Type == EGameXXKCardEffectType::GainManaPerConsumedStatus
				&& Effect.Target == EGameXXKCardEffectTarget::CardOwner
				&& Effect.Magnitude == 2
				&& Effect.Condition.Type == EGameXXKCardEffectConditionType::TargetHasStatus
				&& Effect.Condition.Status == EGameXXKCardStatus::Burn
				&& Effect.Condition.bConsumeStatus
				&& Effect.Condition.MaxConsumedStatusStacks == 4
				&& Effect.Condition.bScaleMagnitudeByConsumedStacks;
		});
		TestNotNull(TEXT("SheLingHuo consumes up to four burn stacks through its effect condition"), ConsumeBurnEffect);
	}

	if (const FGameXXKCardDefinition* FuHuDuanJiang = RequireCard(*this, TEXT("Route.Boss.FuHuDuanJiang")))
	{
		const FGameXXKCardEffect* ConsumeVulnerabilityEffect = FuHuDuanJiang->Effects.FindByPredicate([](const FGameXXKCardEffect& Effect)
		{
			return Effect.Type == EGameXXKCardEffectType::BonusDamagePercentPerConsumedStatus
				&& Effect.Target == EGameXXKCardEffectTarget::SelectedTarget
				&& Effect.Magnitude == 25
				&& Effect.Condition.Type == EGameXXKCardEffectConditionType::TargetHasStatus
				&& Effect.Condition.Status == EGameXXKCardStatus::Vulnerability
				&& Effect.Condition.bConsumeStatus
				&& Effect.Condition.MaxConsumedStatusStacks == 3
				&& Effect.Condition.bScaleMagnitudeByConsumedStacks;
		});
		TestNotNull(TEXT("FuHuDuanJiang consumes up to three vulnerability stacks through its effect condition"), ConsumeVulnerabilityEffect);
	}

	if (const FGameXXKCardDefinition* XiongPiPiJia = RequireCard(*this, TEXT("Route.Boss.XiongPiPiJia")))
	{
		TestEqual(TEXT("XiongPiPiJia has exactly one modifier"), CountEffects(*XiongPiPiJia, EGameXXKCardEffectType::ApplyBattleModifier), 1);
		const FGameXXKCardEffect* RetaliationModifier = XiongPiPiJia->Effects.FindByPredicate([](const FGameXXKCardEffect& Effect)
		{
			return Effect.Type == EGameXXKCardEffectType::ApplyBattleModifier
				&& Effect.Modifier.Trigger == EGameXXKCardBattleModifierTrigger::FirstDirectDamageReceivedThisRound
				&& Effect.Modifier.EffectType == EGameXXKCardEffectType::DamagePercentAttack
				&& Effect.Modifier.Target == EGameXXKCardEffectTarget::Attacker
				&& Effect.Modifier.Magnitude == 50
				&& Effect.Modifier.RemainingTriggers == 1
				&& Effect.Modifier.bPersistent;
		});
		TestNotNull(TEXT("XiongPiPiJia carries its persistent retaliation trigger as data"), RetaliationModifier);
	}

	if (const FGameXXKCardDefinition* YiNuoQianJin = RequireCard(*this, TEXT("Npc.SongJinBao.YiNuoQianJin")))
	{
		TestEqual(TEXT("YiNuoQianJin has exactly one modifier"), CountEffects(*YiNuoQianJin, EGameXXKCardEffectType::ApplyBattleModifier), 1);
		const FGameXXKCardEffect* EnergyModifier = YiNuoQianJin->Effects.FindByPredicate([](const FGameXXKCardEffect& Effect)
		{
			return Effect.Type == EGameXXKCardEffectType::ApplyBattleModifier
				&& Effect.Modifier.Trigger == EGameXXKCardBattleModifierTrigger::OnCardPlayed
				&& Effect.Modifier.EffectType == EGameXXKCardEffectType::ModifyEnergyCost
				&& Effect.Modifier.Target == EGameXXKCardEffectTarget::PlayedCard
				&& Effect.Modifier.Magnitude == -1
				&& Effect.Modifier.MinimumResult == 0
				&& Effect.Modifier.RemainingTriggers == 2
				&& Effect.Modifier.RecipientScope == EGameXXKCardModifierRecipientScope::SharedDeck
				&& Effect.Modifier.RecipientTarget == EGameXXKCardEffectTarget::PlayedCard
				&& Effect.Modifier.Expiry == EGameXXKCardModifierExpiry::AfterTriggerCount
				&& Effect.Modifier.bPersistent;
		});
		TestNotNull(TEXT("YiNuoQianJin carries its two-card energy modifier as data"), EnergyModifier);
	}

	const auto FindModifier = [](const FGameXXKCardDefinition& Definition) -> const FGameXXKCardEffect*
	{
		return Definition.Effects.FindByPredicate([](const FGameXXKCardEffect& Effect)
		{
			return Effect.Type == EGameXXKCardEffectType::ApplyBattleModifier;
		});
	};

	if (const FGameXXKCardDefinition* TieYiYiJue = RequireCard(*this, TEXT("Route.Rare.TieYiYiJue")))
	{
		TestEqual(TEXT("TieYiYiJue has exactly one modifier"), CountEffects(*TieYiYiJue, EGameXXKCardEffectType::ApplyBattleModifier), 1);
		const FGameXXKCardEffect* Modifier = FindModifier(*TieYiYiJue);
		TestNotNull(TEXT("TieYiYiJue has an end-of-round armor-gated modifier"), Modifier);
		if (Modifier)
		{
			TestEqual(TEXT("TieYiYiJue uses the owner-armor condition"), Modifier->Modifier.Condition.Type, EGameXXKCardEffectConditionType::OwnerArmorAtLeast);
			TestEqual(TEXT("TieYiYiJue uses the independent minimum armor field"), Modifier->Modifier.Condition.MinimumArmor, 10);
			TestEqual(TEXT("TieYiYiJue does not overload the status stack minimum for armor"), Modifier->Modifier.Condition.MinimumStatusStacks, 0);
		}
	}

	if (const FGameXXKCardDefinition* LinZhenMoRen = RequireCard(*this, TEXT("Route.General.LinZhenMoRen")))
	{
		TestEqual(TEXT("LinZhenMoRen has exactly one modifier"), CountEffects(*LinZhenMoRen, EGameXXKCardEffectType::ApplyBattleModifier), 1);
		const FGameXXKCardEffect* Modifier = FindModifier(*LinZhenMoRen);
		TestNotNull(TEXT("LinZhenMoRen has a modifier"), Modifier);
		if (Modifier)
		{
			TestEqual(TEXT("LinZhenMoRen grants its modifier to the selected ally"), Modifier->Modifier.RecipientScope, EGameXXKCardModifierRecipientScope::SelectedTarget);
			TestEqual(TEXT("LinZhenMoRen stores the selected ally binding"), Modifier->Modifier.RecipientTarget, EGameXXKCardEffectTarget::SelectedTarget);
			TestEqual(TEXT("LinZhenMoRen lasts for two attacks"), Modifier->Modifier.RemainingTriggers, 2);
		}
	}

	const auto TestRoleBoundAttackModifier = [this, &FindModifier](const TCHAR* CardId, const EGameXXKCharacterRole RequiredRole, const TCHAR* RequiredOwnerId, const int32 Magnitude)
	{
		if (const FGameXXKCardDefinition* Definition = RequireCard(*this, CardId))
		{
			TestEqual(FString::Printf(TEXT("%s has exactly one modifier"), CardId), CountEffects(*Definition, EGameXXKCardEffectType::ApplyBattleModifier), 1);
			const FGameXXKCardEffect* Modifier = FindModifier(*Definition);
			TestNotNull(FString::Printf(TEXT("%s has its role-bound next-attack modifier"), CardId), Modifier);
			if (Modifier)
			{
				TestEqual(FString::Printf(TEXT("%s restricts the triggered role"), CardId), Modifier->Modifier.RequiredTriggeredRole, RequiredRole);
				TestEqual(FString::Printf(TEXT("%s restricts the triggered owner"), CardId), Modifier->Modifier.RequiredTriggeredOwnerId, FName(RequiredOwnerId));
				TestEqual(FString::Printf(TEXT("%s binds the modifier to its owner"), CardId), Modifier->Modifier.RecipientScope, EGameXXKCardModifierRecipientScope::CardOwner);
				TestEqual(FString::Printf(TEXT("%s applies its approved attack bonus"), CardId), Modifier->Modifier.Magnitude, Magnitude);
				TestEqual(FString::Printf(TEXT("%s accepts any attack target"), CardId), Modifier->Modifier.TriggeredAttackTargetScope, EGameXXKCardTriggeredAttackTargetScope::AnyTarget);
			}
		}
	};
	TestRoleBoundAttackModifier(TEXT("Profession.Blade.JieShiHuiFeng"), EGameXXKCharacterRole::Blade, TEXT("Profession.Blade"), 40);
	TestRoleBoundAttackModifier(TEXT("Profession.Blade.ZhuYing"), EGameXXKCharacterRole::Blade, TEXT("Profession.Blade"), 50);
	TestRoleBoundAttackModifier(TEXT("Profession.Blade.HuiFengJiaShi"), EGameXXKCharacterRole::Blade, TEXT("Profession.Blade"), 40);
	TestRoleBoundAttackModifier(TEXT("Profession.Hunter.LieHunBiao"), EGameXXKCharacterRole::Hunter, TEXT("Profession.Hunter"), 40);

	if (const FGameXXKCardDefinition* ZhuYing = RequireCard(*this, TEXT("Profession.Blade.ZhuYing")))
	{
		TestEqual(TEXT("ZhuYing has exactly one modifier"), CountEffects(*ZhuYing, EGameXXKCardEffectType::ApplyBattleModifier), 1);
		const FGameXXKCardEffect* Modifier = FindModifier(*ZhuYing);
		TestNotNull(TEXT("ZhuYing has its next-attack modifier"), Modifier);
		if (Modifier)
		{
			TestEqual(TEXT("ZhuYing requires agility before the attack bonus"), Modifier->Modifier.Condition.Type, EGameXXKCardEffectConditionType::OwnerHasStatus);
			TestEqual(TEXT("ZhuYing consumes agility for the attack bonus"), Modifier->Modifier.Condition.Status, EGameXXKCardStatus::Agility);
			TestEqual(TEXT("ZhuYing requires one agility stack"), Modifier->Modifier.Condition.MinimumStatusStacks, 1);
			TestTrue(TEXT("ZhuYing consumes its agility condition"), Modifier->Modifier.Condition.bConsumeStatus);
			TestEqual(TEXT("ZhuYing consumes exactly one agility stack"), Modifier->Modifier.Condition.MaxConsumedStatusStacks, 1);
		}
	}

	if (const FGameXXKCardDefinition* ZhenQiGuWu = RequireCard(*this, TEXT("Profession.FormationMaster.ZhenQiGuWu")))
	{
		TestEqual(TEXT("ZhenQiGuWu has exactly one modifier"), CountEffects(*ZhenQiGuWu, EGameXXKCardEffectType::ApplyBattleModifier), 1);
		const FGameXXKCardEffect* Modifier = FindModifier(*ZhenQiGuWu);
		TestNotNull(TEXT("ZhenQiGuWu has a party modifier"), Modifier);
		if (Modifier)
		{
			TestEqual(TEXT("ZhenQiGuWu covers all allies"), Modifier->Modifier.RecipientScope, EGameXXKCardModifierRecipientScope::AllAllies);
			TestEqual(TEXT("ZhenQiGuWu does not restrict a profession"), Modifier->Modifier.RequiredTriggeredRole, EGameXXKCharacterRole::Invalid);
		}
	}

	if (const FGameXXKCardDefinition* WuWeiTiaoHe = RequireCard(*this, TEXT("Profession.Healer.WuWeiTiaoHe")))
	{
		TestEqual(TEXT("WuWeiTiaoHe has exactly one modifier"), CountEffects(*WuWeiTiaoHe, EGameXXKCardEffectType::ApplyBattleModifier), 1);
		const FGameXXKCardEffect* Modifier = FindModifier(*WuWeiTiaoHe);
		TestNotNull(TEXT("WuWeiTiaoHe has a healing modifier"), Modifier);
		if (Modifier)
		{
			TestEqual(TEXT("WuWeiTiaoHe modifies healing instead of damage"), Modifier->Modifier.EffectType, EGameXXKCardEffectType::ModifyHealingPercent);
			TestEqual(TEXT("WuWeiTiaoHe increases healing by fifty percent"), Modifier->Modifier.Magnitude, 50);
			TestEqual(TEXT("WuWeiTiaoHe expires at the end of this round"), Modifier->Modifier.Expiry, EGameXXKCardModifierExpiry::EndOfCurrentRound);
			TestEqual(TEXT("WuWeiTiaoHe covers the party"), Modifier->Modifier.RecipientScope, EGameXXKCardModifierRecipientScope::AllAllies);
		}
	}

	const auto TestConsumptionResult = [this](const TCHAR* CardId, const EGameXXKCardEffectType ProducerType, const EGameXXKCardEffectType ConsumerType)
	{
		if (const FGameXXKCardDefinition* Definition = RequireCard(*this, CardId))
		{
			const FGameXXKCardEffect* Producer = Definition->Effects.FindByPredicate([ProducerType](const FGameXXKCardEffect& Effect)
			{
				return Effect.Type == ProducerType && Effect.Condition.bConsumeStatus;
			});
			const FGameXXKCardEffect* Consumer = Definition->Effects.FindByPredicate([ConsumerType](const FGameXXKCardEffect& Effect)
			{
				return Effect.Type == ConsumerType && !Effect.ConsumedStackResultRef.IsNone();
			});
			TestNotNull(FString::Printf(TEXT("%s has a consumption producer"), CardId), Producer);
			TestNotNull(FString::Printf(TEXT("%s has a consumption result consumer"), CardId), Consumer);
			if (Producer && Consumer)
			{
				TestFalse(FString::Printf(TEXT("%s has a named consumption group"), CardId), Producer->ConsumptionGroupId.IsNone());
				TestEqual(FString::Printf(TEXT("%s shares its consumed-stack result"), CardId), Consumer->ConsumedStackResultRef, Producer->ConsumptionGroupId);
			}
		}
	};
	TestConsumptionResult(TEXT("Profession.Sorcerer.XingHuoHuiShou"), EGameXXKCardEffectType::GainManaPerConsumedStatus, EGameXXKCardEffectType::BonusDamagePercentPerConsumedStatus);
	TestConsumptionResult(TEXT("Hero.PoYunYiShan"), EGameXXKCardEffectType::BonusDamagePercent, EGameXXKCardEffectType::DrawCards);
	TestConsumptionResult(TEXT("Profession.Blade.DaoYiShouShu"), EGameXXKCardEffectType::GainManaPerConsumedStatus, EGameXXKCardEffectType::DrawCards);

	const auto TestTerrainTargetOverride = [this](const TCHAR* CardId, const EGameXXKCardTerrain Terrain, const EGameXXKCardTerrain AlternateTerrain)
	{
		if (const FGameXXKCardDefinition* Definition = RequireCard(*this, CardId))
		{
			const FGameXXKCardTargetModeOverride* Override = Definition->TargetSpec.ModeOverrides.FindByPredicate([Terrain, AlternateTerrain](const FGameXXKCardTargetModeOverride& Candidate)
			{
				return Candidate.ConditionType == EGameXXKCardTargetModeOverrideConditionType::TerrainIsAny
					&& Candidate.Terrain == Terrain
					&& Candidate.AlternateTerrain == AlternateTerrain;
			});
			TestNotNull(FString::Printf(TEXT("%s declares its terrain target override"), CardId), Override);
			if (Override)
			{
				TestEqual(FString::Printf(TEXT("%s becomes an all-allies card on its terrain"), CardId), Override->Mode, EGameXXKCardTargetMode::AllAllies);
				TestEqual(FString::Printf(TEXT("%s becomes group presentation on its terrain"), CardId), Override->Presentation, EGameXXKCardTargetPresentation::Group);
			}
		}
	};
	TestTerrainTargetOverride(TEXT("Npc.QiongMeiEr.TengQiaoFeiDu"), EGameXXKCardTerrain::Cliff, EGameXXKCardTerrain::Forest);
	TestTerrainTargetOverride(TEXT("Profession.FormationMaster.YinShuiHuiYuan"), EGameXXKCardTerrain::WaterShore, EGameXXKCardTerrain::Ferry);
	TestTerrainTargetOverride(TEXT("Profession.FormationMaster.LinYingMiZong"), EGameXXKCardTerrain::Forest, EGameXXKCardTerrain::Invalid);
	TestTerrainTargetOverride(TEXT("Profession.FormationMaster.LinFengFuZhen"), EGameXXKCardTerrain::Forest, EGameXXKCardTerrain::Invalid);

	const auto TestSingleAllyTarget = [this](const TCHAR* CardId)
	{
		if (const FGameXXKCardDefinition* Definition = RequireCard(*this, CardId))
		{
			TestEqual(FString::Printf(TEXT("%s permits one ally including its owner"), CardId), Definition->TargetSpec.Mode, EGameXXKCardTargetMode::SingleAlly);
			TestFalse(FString::Printf(TEXT("%s does not exclude its owner"), CardId), Definition->TargetSpec.bRequireDifferentFromOwner);
		}
	};
	TestSingleAllyTarget(TEXT("Profession.Guard.HuZhu"));
	TestSingleAllyTarget(TEXT("Profession.Guard.YuanJunBiLei"));
	TestSingleAllyTarget(TEXT("Profession.Healer.HuiQiXiang"));
	TestSingleAllyTarget(TEXT("Profession.FormationMaster.YinShuiHuiYuan"));
	TestSingleAllyTarget(TEXT("Profession.FormationMaster.LinYingMiZong"));
	TestSingleAllyTarget(TEXT("Profession.FormationMaster.YiWeiZhen"));
	TestSingleAllyTarget(TEXT("Profession.FormationMaster.ShuiJingZheGuang"));
	TestSingleAllyTarget(TEXT("Profession.FormationMaster.LinFengFuZhen"));
	TestSingleAllyTarget(TEXT("Route.General.YanDun"));
	TestSingleAllyTarget(TEXT("Route.General.LinZhenMoRen"));

	const auto TestWaterShoreAndFerryCondition = [this](const TCHAR* CardId)
	{
		if (const FGameXXKCardDefinition* Definition = RequireCard(*this, CardId))
		{
			const FGameXXKCardEffect* WaterMana = Definition->Effects.FindByPredicate([](const FGameXXKCardEffect& Effect)
			{
				return Effect.Type == EGameXXKCardEffectType::GainMana
					&& Effect.Target == EGameXXKCardEffectTarget::CardOwner
					&& Effect.Magnitude == 4
					&& Effect.Condition.Type == EGameXXKCardEffectConditionType::TerrainIsAny
					&& Effect.Condition.Terrain == EGameXXKCardTerrain::WaterShore;
			});
			TestNotNull(FString::Printf(TEXT("%s has its water-terrain mana effect"), CardId), WaterMana);
			if (WaterMana)
			{
				TestEqual(FString::Printf(TEXT("%s also recognizes Ferry terrain"), CardId), WaterMana->Condition.AlternateTerrain, EGameXXKCardTerrain::Ferry);
			}
		}
	};
	TestWaterShoreAndFerryCondition(TEXT("Profession.FormationMaster.DiMaiJieLi"));
	TestWaterShoreAndFerryCondition(TEXT("Route.Terrain.JieShiTuXi"));

	const auto TestNoUnitTarget = [this](const TCHAR* CardId)
	{
		if (const FGameXXKCardDefinition* Definition = RequireCard(*this, CardId))
		{
			TestEqual(FString::Printf(TEXT("%s has no unit target"), CardId), Definition->TargetSpec.Mode, EGameXXKCardTargetMode::None);
			TestEqual(FString::Printf(TEXT("%s has no-selection presentation"), CardId), Definition->TargetSpec.Presentation, EGameXXKCardTargetPresentation::NoSelection);
			TestEqual(FString::Printf(TEXT("%s accepts any unit state because it selects none"), CardId), Definition->TargetSpec.RequiredUnitState, EGameXXKCardUnitState::Any);
		}
	};
	TestNoUnitTarget(TEXT("Npc.SongJinBao.YiNuoQianJin"));
	TestNoUnitTarget(TEXT("Npc.YueBai.CanJuanPiZhu"));
	TestNoUnitTarget(TEXT("Route.Terrain.DiMaiHuiXiang"));
	TestNoUnitTarget(TEXT("Route.Rare.GuJuanCanZhang"));

	const auto TestAnyDotCondition = [this](const TCHAR* CardId)
	{
		if (const FGameXXKCardDefinition* Definition = RequireCard(*this, CardId))
		{
			const FGameXXKCardEffect* RemoveDot = Definition->Effects.FindByPredicate([](const FGameXXKCardEffect& Effect)
			{
				return Effect.Type == EGameXXKCardEffectType::RemoveAnyDamageOverTime
					&& Effect.Condition.Type == EGameXXKCardEffectConditionType::TargetHasAnyDamageOverTime;
			});
			TestNotNull(FString::Printf(TEXT("%s recognizes bleed, poison, or burn as damage over time"), CardId), RemoveDot);
		}
	};
	TestAnyDotCondition(TEXT("Profession.Healer.XingQiZhen"));
	TestAnyDotCondition(TEXT("Profession.Healer.HuiQiXiang"));

	TMap<FName, int32> ProfessionCardCounts;
	TMap<FName, int32> ProfessionCoreCounts;
	TMap<FName, int32> NpcCardCounts;
	int32 GeneralRouteCount = 0;
	int32 TerrainRouteCount = 0;
	int32 RareRouteCount = 0;
	int32 BossRouteCount = 0;
	for (const FGameXXKCardDefinition& Definition : Definitions)
	{
		const FString OwnerKey = Definition.OwnerId.ToString();
		const FString AcquisitionKey = Definition.AcquisitionKey.ToString();
		switch (Definition.Owner)
		{
		case EGameXXKCardOwner::Hero:
			TestTrue(FString::Printf(TEXT("hero golden metadata is valid for %s"), *Definition.Id.ToString()), Definition.Role == EGameXXKCharacterRole::Hero && Definition.Rarity == EGameXXKCardRarity::Permanent && Definition.OwnerId == FName(TEXT("Hero")) && AcquisitionKey.StartsWith(TEXT("Unlock.")));
			break;
		case EGameXXKCardOwner::Profession:
			++ProfessionCardCounts.FindOrAdd(Definition.OwnerId);
			if (Definition.bCoreProfessionCard)
			{
				++ProfessionCoreCounts.FindOrAdd(Definition.OwnerId);
			}
			TestTrue(FString::Printf(TEXT("profession golden metadata is valid for %s"), *Definition.Id.ToString()), Definition.Rarity == EGameXXKCardRarity::Permanent && OwnerKey.StartsWith(TEXT("Profession.")) && AcquisitionKey == FString::Printf(TEXT("Pool.%s"), *OwnerKey));
			break;
		case EGameXXKCardOwner::QuestNpc:
			++NpcCardCounts.FindOrAdd(Definition.OwnerId);
			TestTrue(FString::Printf(TEXT("quest NPC golden metadata is valid for %s"), *Definition.Id.ToString()), Definition.Rarity == EGameXXKCardRarity::Permanent && Definition.NpcId == Definition.OwnerId && OwnerKey.StartsWith(TEXT("Npc.")) && Definition.AcquisitionKey == Definition.OwnerId);
			break;
		case EGameXXKCardOwner::Route:
			TestTrue(FString::Printf(TEXT("route golden owner metadata is valid for %s"), *Definition.Id.ToString()), Definition.Role == EGameXXKCharacterRole::Route && Definition.OwnerId == FName(TEXT("Route")) && AcquisitionKey.StartsWith(TEXT("Route.")));
			if (AcquisitionKey == TEXT("Route.General"))
			{
				++GeneralRouteCount;
				TestEqual(TEXT("general route cards are common"), Definition.Rarity, EGameXXKCardRarity::Common);
			}
			else if (AcquisitionKey == TEXT("Route.Terrain"))
			{
				++TerrainRouteCount;
				TestEqual(TEXT("terrain route cards are common"), Definition.Rarity, EGameXXKCardRarity::Common);
			}
			else if (AcquisitionKey == TEXT("Route.Rare"))
			{
				++RareRouteCount;
				TestEqual(TEXT("rare route cards are rare"), Definition.Rarity, EGameXXKCardRarity::Rare);
			}
			else if (AcquisitionKey.StartsWith(TEXT("Route.Boss.")))
			{
				++BossRouteCount;
				TestEqual(TEXT("boss route cards are boss rarity"), Definition.Rarity, EGameXXKCardRarity::Boss);
			}
			else
			{
				AddError(FString::Printf(TEXT("route card has an unknown acquisition key: %s"), *Definition.Id.ToString()));
			}
			break;
		default:
			break;
		}
	}

	const TArray<FName> ProfessionOwnerIds = {
		FName(TEXT("Profession.Blade")), FName(TEXT("Profession.Guard")), FName(TEXT("Profession.Healer")),
		FName(TEXT("Profession.Hunter")), FName(TEXT("Profession.Sorcerer")), FName(TEXT("Profession.FormationMaster"))
	};
	for (const FName OwnerId : ProfessionOwnerIds)
	{
		TestEqual(FString::Printf(TEXT("%s has exactly eighteen profession cards"), *OwnerId.ToString()), ProfessionCardCounts.FindRef(OwnerId), 18);
		TestEqual(FString::Printf(TEXT("%s has exactly four core cards"), *OwnerId.ToString()), ProfessionCoreCounts.FindRef(OwnerId), 4);
	}

	const TArray<FName> NpcOwnerIds = {
		FName(TEXT("Npc.TusiChief")), FName(TEXT("Npc.SongJinBao")), FName(TEXT("Npc.YueBai")),
		FName(TEXT("Npc.ZhouGuangZu")), FName(TEXT("Npc.JinGui")), FName(TEXT("Npc.QiongMeiEr"))
	};
	for (const FName OwnerId : NpcOwnerIds)
	{
		TestEqual(FString::Printf(TEXT("%s has exactly four NPC cards"), *OwnerId.ToString()), NpcCardCounts.FindRef(OwnerId), 4);
	}
	TestEqual(TEXT("route general category has ten cards"), GeneralRouteCount, 10);
	TestEqual(TEXT("route terrain category has ten cards"), TerrainRouteCount, 10);
	TestEqual(TEXT("route rare category has five cards"), RareRouteCount, 5);
	TestEqual(TEXT("route boss category has five cards"), BossRouteCount, 5);

	return true;
}

#endif
