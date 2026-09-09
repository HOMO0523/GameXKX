#include "Misc/AutomationTest.h"
#include "GameXXKCardRules.h"
#include "GameXXKGemRules.h"
#include "GameXXKEnemyCatalog.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKSpellChannelRedTest,
	"GameXXK.Equipment.Resistance.SpellBypassesDefenseButConsumesArmor",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKSpellChannelRedTest::RunTest(const FString& Parameters)
{
	FGameXXKCardCombatUnit Attacker;Attacker.UnitId=TEXT("SpellTest.Source");Attacker.Side=EGameXXKCardTargetSide::Party;Attacker.bLiving=true;Attacker.HP=Attacker.MaxHP=100;Attacker.StableSortOrder=0;Attacker.Attack=100;
	FGameXXKCardCombatUnit Target;Target.UnitId=TEXT("SpellTest.Target");Target.Side=EGameXXKCardTargetSide::Enemy;Target.bLiving=true;Target.HP=Target.MaxHP=100;Target.StableSortOrder=1;Target.Defense=30;Target.Armor=20;
	for(auto Element:{EGameXXKCardDamageElement::None,EGameXXKCardDamageElement::Fire,EGameXXKCardDamageElement::Frost,EGameXXKCardDamageElement::Lightning})
	{
		TArray<FGameXXKCardCombatUnit> Units={Attacker,Target};TArray<FGameXXKCardGuardLinkRuntime> Guards;
		FGameXXKCardDamageContext Context;Context.SourceUnitId=Attacker.UnitId;Context.Kind=EGameXXKCardDamageKind::SingleTargetAttack;Context.ResolutionOrigin=EGameXXKCardResolutionOrigin::ActivePlay;Context.Element=Element;
		FGameXXKCardDamageResult Result;FString Error;
		if(!TestTrue(TEXT("a normal damage packet resolves"),GameXXKCardRules::ApplyCombatDirectDamage(Units,Guards,Context,Target.UnitId,100,Result,&Error))){AddError(Error);return false;}
		TestEqual(TEXT("temporary armor absorbs either channel"),Result.ArmorAbsorbed,20);
		TestEqual(TEXT("only physical damage subtracts defense"),Result.HealthDamage,Element==EGameXXKCardDamageElement::None?50:80);
	}
	TestEqual(TEXT("seventeen gem types expose all ten qualities"),FGameXXKGemRules::GetAllItemIds().Num(),170);
	for (const auto& Spec : TArray<TPair<FName,int32>>{{TEXT("Enemy.Ch1.MoneyRat"),8},{TEXT("Enemy.Ch3.GiantToad"),6}})
	{
		const auto* Definition = FGameXXKEnemyCatalog::Find(Spec.Key); bool Found = false;
		if (Definition) for (const auto& Phase : Definition->Phases) for (const auto& Card : Phase.Intents) for (const auto& Effect : Card.Effects)
			if (static_cast<int32>(Effect.Type) == 20)
				Found |= Effect.ResourceAmountByDifficulty.Normal == Spec.Value && Effect.ResourceAmountByDifficulty.Hard == Spec.Value && Effect.ResourceAmountByDifficulty.Hell == Spec.Value;
		TestTrue(TEXT("the selected monster exposes the requested fixed mana siphon at every difficulty"), Found);
	}
	return true;
}
#endif
