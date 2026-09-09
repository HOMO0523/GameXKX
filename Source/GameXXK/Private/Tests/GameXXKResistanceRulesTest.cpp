#include "Misc/AutomationTest.h"
#include "GameXXKResistanceRules.h"
#include "GameXXKGemRules.h"
#include "GameXXKCombatGemRules.h"
#include "GameXXKCardRules.h"
#include "GameXXKEnemyCatalog.h"
#include "GameXXKSorcererPartnerRuntimeTestUtils.h"
#include "UI/GameXXKBattleAnimationPresentation.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
	FGameXXKCardCombatUnit Unit(FName Id,EGameXXKCardTargetSide Side,int32 Order)
	{
		FGameXXKCardCombatUnit U;U.UnitId=Id;U.Side=Side;U.bLiving=true;U.HP=U.MaxHP=10000;
		U.Attack=1000;U.Mana=U.MaxMana=20;U.StableSortOrder=Order;return U;
	}
	void SetResistance(FGameXXKCardCombatUnit& U,int32 Fire,int32 Frost,int32 Lightning)
	{
		U.InnateResistanceBasisPoints={{EGameXXKCardDamageElement::Fire,Fire},{EGameXXKCardDamageElement::Frost,Frost},{EGameXXKCardDamageElement::Lightning,Lightning}};
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKResistanceProfileTest,"GameXXK.Equipment.Resistance.ProfilesAndMarginalFormula",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKResistanceProfileTest::RunTest(const FString& Parameters)
{
	const auto Keys=FGameXXKResistanceRules::GetProfileKeys();TestEqual(TEXT("13 character templates and 21 monsters"),Keys.Num(),34);
	for(const auto& Enemy:FGameXXKEnemyCatalog::GetAllDefinitions())TestTrue(TEXT("every enemy has an explicit resistance profile"),Keys.Contains(Enemy.Id));
	const auto Hero=FGameXXKResistanceRules::GetBaseProfile(TEXT("Hero"),EGameXXKCharacterRole::Hero);
	TestEqual(TEXT("hero base fire resistance"),Hero.FireBasisPoints,1000);
	const auto Ghost=FGameXXKResistanceRules::GetBaseProfile(TEXT("Npc.YueBai"),EGameXXKCharacterRole::QuestNpc);
	TestEqual(TEXT("named ghost overrides the generic role"),Ghost.FireBasisPoints,2500);TestEqual(TEXT("ghost lightning weak side"),Ghost.LightningBasisPoints,500);
	const auto Ape=FGameXXKResistanceRules::GetBaseProfile(NAME_None,EGameXXKCharacterRole::Invalid,TEXT("Enemy.Ch3.WhiteApe"));
	TestEqual(TEXT("white ape fire weakness"),Ape.FireBasisPoints,-1000);TestEqual(TEXT("white ape frost specialty"),Ape.FrostBasisPoints,4000);
	TestTrue(TEXT("base20 plus nominal20 is 34.6667 percent"),FMath::IsNearlyEqual(FGameXXKResistanceRules::ResolveBasisPoints(2000,2000),3466.6666666667,0.000001));
	TestTrue(TEXT("reduction is subtracted after diminishing"),FMath::IsNearlyEqual(FGameXXKResistanceRules::ResolveBasisPoints(2000,2000,1500),1966.6666666667,0.000001));
	TestEqual(TEXT("upper ceiling including a zero gap"),FGameXXKResistanceRules::ResolveBasisPoints(7500,0),7500.0);
	TestEqual(TEXT("negative resistance lower bound"),FGameXXKResistanceRules::ResolveBasisPoints(0,0,MAX_int32),-2500.0);
	TestTrue(TEXT("huge bonuses approach but do not exceed the ceiling"),FGameXXKResistanceRules::ResolveBasisPoints(1000,MAX_int32)<7500.0);
	TestTrue(TEXT("cosmic resistance gem depends on the wearer's base"),FMath::IsNearlyEqual(FGameXXKResistanceRules::ResolveBasisPoints(1000,8600),4701.9867549669,0.000001));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKResistanceDamageChannelsTest,"GameXXK.Equipment.Resistance.ChannelsNegativeResistanceAndArmor",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKResistanceDamageChannelsTest::RunTest(const FString& Parameters)
{
	auto Source=Unit(TEXT("Source"),EGameXXKCardTargetSide::Party,0);auto Target=Unit(TEXT("Target"),EGameXXKCardTargetSide::Enemy,1);
	Target.Defense=300;Target.Armor=150;SetResistance(Target,2000,4000,-1000);
	const int32 Expected[]={550,650,450,950};
	for(int32 Index=0;Index<4;++Index)
	{
		auto Units=TArray<FGameXXKCardCombatUnit>{Source,Target};TArray<FGameXXKCardGuardLinkRuntime> Links;
		FGameXXKCardDamageContext Context;Context.SourceUnitId=Source.UnitId;Context.Kind=EGameXXKCardDamageKind::SingleTargetAttack;Context.ResolutionOrigin=EGameXXKCardResolutionOrigin::ActivePlay;Context.Element=static_cast<EGameXXKCardDamageElement>(Index);
		FGameXXKCardDamageResult Result;FString Error;
		if(!TestTrue(TEXT("real damage channel resolves"),GameXXKCardRules::ApplyCombatDirectDamage(Units,Links,Context,Target.UnitId,1000,Result,&Error))){AddError(Error);return false;}
		TestEqual(TEXT("corresponding resistance and armor determine actual HP loss"),Result.HealthDamage,Expected[Index]);
		TestEqual(TEXT("temporary armor still absorbs 150"),Result.ArmorAbsorbed,150);
		TestEqual(TEXT("spell packets do not subtract defense"),Result.DamageAfterDefense,Index==0?700:1000);
	}
	SetResistance(Target,7500,0,0);TestEqual(TEXT("75 percent resistance leaves a quarter"),FGameXXKResistanceRules::ApplyDamage(Target,EGameXXKCardDamageElement::Fire,1000),250);
	TestEqual(TEXT("zero cannot produce phantom damage"),FGameXXKResistanceRules::ApplyDamage(Target,EGameXXKCardDamageElement::Fire,0),0);
	SetResistance(Target,-2500,0,0);TestEqual(TEXT("damage overflow clamps safely"),FGameXXKResistanceRules::ApplyDamage(Target,EGameXXKCardDamageElement::Fire,MAX_int32),MAX_int32);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKResistanceBurnAndSaveTest,"GameXXK.Equipment.Resistance.BurnAndUnitSnapshotSave",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKResistanceBurnAndSaveTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKSorcererPartnerRuntimeTestUtils;
	FGameXXKCardBattleRuntime Runtime;
	if(!BuildRuntime(*this,{MakeCard(TEXT("Profession.Sorcerer.JuLing"),0)},
		{MakeUnit(SorcererId,EGameXXKCardTargetSide::Party,EGameXXKCharacterRole::Sorcerer,0),MakeUnit(EnemyAId,EGameXXKCardTargetSide::Enemy,EGameXXKCharacterRole::Invalid,1)},9782,Runtime))return false;
	auto* Caster=FindUnit(Runtime,SorcererId);Caster->GemBonusBasisPoints={{EGameXXKGemType::FireDamage,8600},{EGameXXKGemType::DamageOverTime,8600}};
	auto* Target=FindUnit(Runtime,EnemyAId);SetResistance(*Target,2000,0,0);Target->Armor=500;Target->Defense=1000;
	GameXXKCardRules::AddCombatStatus(*Target,EGameXXKCardStatus::Burn,1000,SorcererId);
	TArray<FGameXXKCardDamageResult> Results;FString Error;
	if(!TestTrue(TEXT("active burn trigger uses the normal rule path"),GameXXKCardRules::TriggerCombatDamageOverTime(Runtime,SorcererId,EnemyAId,EGameXXKCardStatus::Burn,1,Results,&Error))){AddError(Error);return false;}
	if(!TestEqual(TEXT("one burn trigger remains one packet"),Results.Num(),1))return false;
	TestEqual(TEXT("fire and DOT combine before the target's fire resistance"),Results[0].HealthDamage,1218);
	TestEqual(TEXT("burn keeps its original armor-bypass behavior"),FindUnit(Runtime,EnemyAId)->Armor,500);
	TestEqual(TEXT("burn status layers are not boosted or consumed"),GameXXKCardRules::GetCombatStatusStacks(*FindUnit(Runtime,EnemyAId),EGameXXKCardStatus::Burn),1000);
	FindUnit(Runtime,SorcererId)->GemBonusBasisPoints.Add(EGameXXKGemType::FireResistance,8600);
	FGameXXKResistanceRules::InitializeUnitProfile(*FindUnit(Runtime,SorcererId));
	TArray<uint8> Bytes;FMemoryWriter Writer(Bytes,true);FObjectAndNameAsStringProxyArchive WriteArchive(Writer,false);WriteArchive.ArIsSaveGame=true;
	FGameXXKCardBattleRuntime::StaticStruct()->SerializeItem(WriteArchive,&Runtime,nullptr);
	FGameXXKCardBattleRuntime Loaded;FMemoryReader Reader(Bytes,true);FObjectAndNameAsStringProxyArchive ReadArchive(Reader,false);ReadArchive.ArIsSaveGame=true;
	FGameXXKCardBattleRuntime::StaticStruct()->SerializeItem(ReadArchive,&Loaded,nullptr);
	TestTrue(TEXT("saved battle with source and resistance data validates"),GameXXKCardRules::ValidateCardBattleRuntime(Loaded,&Error));
	TestEqual(TEXT("innate fire profile survives"),FindUnit(Loaded,SorcererId)->InnateResistanceBasisPoints.FindRef(EGameXXKCardDamageElement::Fire),2000);
	TestEqual(TEXT("new resistance gem survives"),FindUnit(Loaded,SorcererId)->GemBonusBasisPoints.FindRef(EGameXXKGemType::FireResistance),8600);
	TestEqual(TEXT("DOT source survives alongside resistance"),FindUnit(Loaded,EnemyAId)->Statuses[0].Sources[0].SourceUnitId,SorcererId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKResistanceElementInheritanceTest,"GameXXK.Equipment.Resistance.SnapshotElementInheritanceAndClips",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKResistanceElementInheritanceTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKSorcererPartnerRuntimeTestUtils;
	const EGameXXKSorcererCardFamily Families[]={EGameXXKSorcererCardFamily::Fire,EGameXXKSorcererCardFamily::Ice,EGameXXKSorcererCardFamily::Lightning};
	for(int32 Index=0;Index<3;++Index)
	{
		FGameXXKCardBattleRuntime Runtime;
		if(!BuildRuntime(*this,{MakeCard(TEXT("Profession.Sorcerer.YanMuHuTi"),0)},
			{MakeUnit(SorcererId,EGameXXKCardTargetSide::Party,EGameXXKCharacterRole::Sorcerer,0,100),MakeUnit(EnemyAId,EGameXXKCardTargetSide::Enemy,EGameXXKCharacterRole::Invalid,1)},8791+Index,Runtime))return false;
		FGameXXKCardPlayResult Result;
		if(!ResolveAutomaticSnapshot(*this,Runtime,TEXT("Profession.Sorcerer.YanMuHuTi"),2,Families[Index],EGameXXKSorcererTaskBranch::Normal,Result))return false;
		if(!TestEqual(TEXT("inherited element keeps one attack packet"),Result.DamageResults.Num(),1))return false;
		const auto Element=static_cast<EGameXXKCardDamageElement>(Index+1);
		TestEqual(TEXT("replay uses its previous-card snapshot element"),Result.DamageResults[0].Element,Element);
		TestEqual(TEXT("the established sequence multiplier remains 85 percent"),Result.DamageResults[0].RequestedDamage,85);
		const auto Events=FGameXXKBattleAnimationPresentation::BuildPresentationEvents(Runtime,NAME_None,Result.DamageResults);
		if(!TestEqual(TEXT("one attack produces one presentation event"),Events.Num(),1))return false;
		TestEqual(TEXT("element survives the event boundary"),Events[0].Element,Element);
		const auto Clip=FGameXXKBattleAnimationPresentation::ResolveElementalHitClip(Element);TestTrue(TEXT("element has a valid real clip"),Clip.IsValid());TestEqual(TEXT("six frames per elemental impact"),Clip.FrameCount,6);
		TestNotNull(TEXT("the elemental atlas is imported"),Clip.TexturePath.TryLoad());
	}
	return true;
}
#endif
