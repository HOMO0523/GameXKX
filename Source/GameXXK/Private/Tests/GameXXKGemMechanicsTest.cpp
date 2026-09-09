#include "Misc/AutomationTest.h"
#include "GameXXKCombatGemRules.h"
#include "GameXXKGemRules.h"
#include "GameXXKEquipmentBonusRules.h"
#include "GameXXKEquipmentRules.h"
#include "GameXXKCardBattleAdapter.h"
#include "GameXXKPartyFormationRules.h"
#include "GameXXKSorcererPartnerRuntimeTestUtils.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "MVP/GameXXKSaveGame.h"
#include "MVP/GameXXKSaveMigration.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/TextBlock.h"
#include "UI/GameXXKEquipmentTooltipPresentation.h"
#include "UI/GameXXKCharacterDetailedAttributes.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
	using namespace GameXXKSorcererPartnerRuntimeTestUtils;
	bool RuntimeFixture(FAutomationTestBase& Test,FGameXXKCardBattleRuntime& Runtime,FName CardId=TEXT("Profession.Sorcerer.JuLing"))
	{
		TArray<FName> Ids={CardId};
		for(FName Id:{FName(TEXT("Profession.Sorcerer.JuLing")),FName(TEXT("Profession.Sorcerer.LiHuoYin")),FName(TEXT("Profession.Sorcerer.FenMaiFu")),FName(TEXT("Profession.Sorcerer.ChiXiaoFenXing")),FName(TEXT("Profession.Sorcerer.LingYanLianDan"))})if(Ids.Num()<5)Ids.AddUnique(Id);
		TArray<FGameXXKCardInstance> Cards;for(int32 I=0;I<Ids.Num();++I)Cards.Add(MakeCard(Ids[I],I));
		return BuildRuntime(Test,Cards,
			{MakeUnit(SorcererId,EGameXXKCardTargetSide::Party,EGameXXKCharacterRole::Sorcerer,1,1000),
			 MakeUnit(AllyId,EGameXXKCardTargetSide::Party,EGameXXKCharacterRole::Healer,2,1000),
			 MakeUnit(EnemyAId,EGameXXKCardTargetSide::Enemy,EGameXXKCharacterRole::Invalid,10)},57913,Runtime);
	}
	TArray<UTextBlock*> Texts(UWidget* Root)
	{
		TArray<UTextBlock*> Result;TArray<UWidget*> Pending={Root};
		while(!Pending.IsEmpty())
		{
			auto* Widget=Pending.Pop();if(auto* Text=Cast<UTextBlock>(Widget))Result.Add(Text);
			if(auto* Panel=Cast<UPanelWidget>(Widget))Pending.Append(Panel->GetAllChildren());
		}
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKGemCurveAndPoolTest,"GameXXK.Equipment.Gems.Mechanics.CurveAndPools",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKGemCurveAndPoolTest::RunTest(const FString& Parameters)
{
	const int32 Steps[]={1,2,4,8,16,32,58,95,137,172};
	for(int32 Rank=1;Rank<=10;++Rank)
	{
		const auto Q=FGameXXKGemRules::QualityFromRank(Rank);
		for(int32 Type=4;Type<=17;++Type)
			TestEqual(TEXT("approved nominal values for every percentage gem"),FGameXXKGemRules::GetBonusBasisPoints(static_cast<EGameXXKGemType>(Type),Q),Steps[Rank-1]*(Type<=6?25:50));
	}
	TestEqual(TEXT("stat percent single cosmic rounds at the final result"),FGameXXKGemRules::ApplyBonus(1000,4300),1273);
	TestEqual(TEXT("mechanic cosmic single gem"),FGameXXKGemRules::ApplyBonus(1000,8600),1401);
	TestEqual(TEXT("two applicable mechanic gems share one pool"),FGameXXKGemRules::ApplyBonus(1000,17200),1522);
	TestEqual(TEXT("overflow safely saturates"),FGameXXKGemRules::ApplyBonus(MAX_int32,MAX_int32),MAX_int32);
	TestTrue(TEXT("the soft limit remains below 75 percent"),FGameXXKGemRules::GetEffectiveBonusBasisPoints(MAX_int32)<7500.0);
	FGameXXKEquipmentArmorBonus Armor;Armor.RawAffixBasisPoints=21000;Armor.GemBasisPoints=8600;Armor.FixedBasisPoints=1000;
	TestEqual(TEXT("half-affix and full gem share one armor pool before the fixed set"),Armor.ApplyTo(1000),1639);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKGemDamageOwnershipTest,"GameXXK.Equipment.Gems.Mechanics.DirectElementAndCounter",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKGemDamageOwnershipTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKSorcererPartnerRuntimeTestUtils;
	FGameXXKCardBattleRuntime Base;if(!RuntimeFixture(*this,Base))return false;
	auto* Owner=FindUnit(Base,SorcererId);
	for(auto Type:{EGameXXKGemType::DirectDamage,EGameXXKGemType::CounterDamage,EGameXXKGemType::FireDamage,EGameXXKGemType::FrostDamage,EGameXXKGemType::LightningDamage})Owner->GemBonusBasisPoints.Add(Type,8600);
	FGameXXKCardDamageContext Context;Context.SourceUnitId=SorcererId;Context.Kind=EGameXXKCardDamageKind::SingleTargetAttack;Context.ResolutionOrigin=EGameXXKCardResolutionOrigin::ActivePlay;
	auto Damage=[&](FGameXXKCardDamageContext C,int32 Expected,int32 Talent=0)
	{
		auto Runtime=Base;Runtime.TalentFinalDamagePercent=Talent;FGameXXKCardDamageResult Result;FString Error;
		if(!TestTrue(*Error,GameXXKCardRules::ApplyPlayerCardDirectDamage(Runtime,C,EnemyAId,1000,Result,&Error)))return;
		TestEqual(TEXT("real packet receives only its source's applicable gem pool"),Result.RequestedDamage,Expected);
	};
	Damage(Context,1401);Damage(Context,1681,20);
	for(auto Element:{EGameXXKCardDamageElement::Fire,EGameXXKCardDamageElement::Frost,EGameXXKCardDamageElement::Lightning})
	{
		Context.Element=Element;Damage(Context,1401);
	}
	Context.SourceUnitId=AllyId;Damage(Context,1000);
	Context.SourceUnitId=SorcererId;Context.Element=EGameXXKCardDamageElement::None;Context.ResolutionOrigin=EGameXXKCardResolutionOrigin::Reaction;Damage(Context,1522);
	Context.ResolutionOrigin=EGameXXKCardResolutionOrigin::Equipment;Damage(Context,1000);Context.bEquipmentCounter=true;Damage(Context,1522);
	Context.bEquipmentCounter=false;Context.ResolutionOrigin=EGameXXKCardResolutionOrigin::TerrainListener;Damage(Context,1000);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKGemDotOwnershipTest,"GameXXK.Equipment.Gems.Mechanics.DotSourcesAndSave",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKGemDotOwnershipTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKSorcererPartnerRuntimeTestUtils;
	FGameXXKCardBattleRuntime Runtime;if(!RuntimeFixture(*this,Runtime))return false;
	FindUnit(Runtime,SorcererId)->GemBonusBasisPoints.Add(EGameXXKGemType::DamageOverTime,8600);
	FindUnit(Runtime,SorcererId)->GemBonusBasisPoints.Add(EGameXXKGemType::FireDamage,8600);
	auto* Target=FindUnit(Runtime,EnemyAId);
	GameXXKCardRules::AddCombatStatus(*Target,EGameXXKCardStatus::Poison,100,SorcererId);
	GameXXKCardRules::AddCombatStatus(*Target,EGameXXKCardStatus::Poison,100,AllyId);
	GameXXKCardRules::AddCombatStatus(*Target,EGameXXKCardStatus::Poison,100);
	int32 Damage=0;FString Error;auto Natural=Runtime;
	TestTrue(TEXT("mixed-source natural poison resolves"),GameXXKCardRules::ApplyCombatEndPhaseDot(Natural.Units,Natural.GuardLinks,EnemyAId,Damage,&Error));
	TestEqual(TEXT("only the applier's hundred stacks are amplified, unknown hundred stays base"),Damage,340);
	GameXXKCardRules::ConsumeCombatStatus(*Target,EGameXXKCardStatus::Poison,150);
	Natural=Runtime;
	TestTrue(TEXT("partial cleansing keeps a valid source ledger"),GameXXKCardRules::ValidateCardBattleRuntime(Runtime,&Error));
	TestTrue(TEXT("remaining natural poison resolves"),GameXXKCardRules::ApplyCombatEndPhaseDot(Natural.Units,Natural.GuardLinks,EnemyAId,Damage,&Error));
	TestEqual(TEXT("partial removal retains proportional ownership"),Damage,170);
	TArray<FGameXXKCardDamageResult> Results;auto Triggered=Runtime;
	TestTrue(TEXT("unbonused ally actively triggers stored poison"),GameXXKCardRules::ResolveToxicExplosion(Triggered,AllyId,EnemyAId,true,Results,&Error));
	TestEqual(TEXT("one original status means one damage packet"),Results.Num(),1);
	if(!Results.IsEmpty())TestEqual(TEXT("active explosion uses triggerer and raw reservoir"),Results[0].RequestedDamage,150);
	Triggered=Runtime;
	TestTrue(TEXT("bonused owner actively triggers stored poison"),GameXXKCardRules::ResolveToxicExplosion(Triggered,SorcererId,EnemyAId,true,Results,&Error));
	if(!Results.IsEmpty())TestEqual(TEXT("active trigger does not amplify the already-amplified natural tick"),Results[0].RequestedDamage,210);
	GameXXKCardRules::AddCombatStatus(*Target,EGameXXKCardStatus::Burn,100,SorcererId);
	GameXXKCardRules::AddCombatStatus(*Target,EGameXXKCardStatus::DamageOverTime,100,SorcererId);
	TestEqual(TEXT("fire and dot share a single burn pool"),FGameXXKCombatGemRules::NaturalDot(Runtime.Units,*Target,EGameXXKCardStatus::Burn,100),152);
	TestEqual(TEXT("independent rot remains fixed"),FGameXXKCombatGemRules::TriggeredDot(Runtime.Units,SorcererId,EGameXXKCardStatus::DamageOverTime,100),100);
	auto* Owner=FindUnit(Runtime,SorcererId);Owner->HP=0;Owner->bLiving=false;
	Natural=Runtime;TestTrue(TEXT("dead applier does not cancel its existing poison"),GameXXKCardRules::ApplyCombatEndPhaseDot(Natural.Units,Natural.GuardLinks,EnemyAId,Damage,&Error));TestEqual(TEXT("dead source retains attribution"),Damage,170);
	auto* Save=NewObject<UGameXXKSaveGame>();Save->SaveState.SaveVersion=FGameXXKSaveMigration::CurrentSaveVersion;Save->SaveState.RuntimeState.CardRun.ActiveBattle=Runtime;
	TArray<uint8> Bytes;TestTrue(TEXT("new combat fields serialize through the real SaveGame API"),UGameplayStatics::SaveGameToMemory(Save,Bytes));
	auto* Loaded=Cast<UGameXXKSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));if(!TestNotNull(TEXT("save reloads"),Loaded))return false;
	auto& Restored=Loaded->SaveState.RuntimeState.CardRun.ActiveBattle;
	TestTrue(TEXT("round-trip source ledger validates"),GameXXKCardRules::ValidateCardBattleRuntime(Restored,&Error));
	TestEqual(TEXT("round-trip preserves effective dot and dead source bonus"),FGameXXKCombatGemRules::NaturalDot(Restored.Units,*FindUnit(Restored,EnemyAId),EGameXXKCardStatus::Poison,150),170);
	GameXXKCardRules::ClearAllDotReservoirs(*FindUnit(Restored,EnemyAId));TestTrue(TEXT("clearing reservoirs removes their source records"),FindUnit(Restored,EnemyAId)->Statuses.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKGemCardMechanismTest,"GameXXK.Equipment.Gems.Mechanics.AuthoredCardsKeepCountsAndResources",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKGemCardMechanismTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKSorcererPartnerRuntimeTestUtils;
	FGameXXKCardBattleRuntime Lightning;if(!RuntimeFixture(*this,Lightning,TEXT("Profession.Sorcerer.NingYanChengRen")))return false;
	FindUnit(Lightning,SorcererId)->GemBonusBasisPoints.Add(EGameXXKGemType::LightningDamage,8600);
	GameXXKCardRules::AddCombatStatus(*FindUnit(Lightning,EnemyAId),EGameXXKCardStatus::Mark,3);
	FGameXXKCardPlayResult Result;FString Error;
	const FName LightningId=Lightning.Deck.Hand.FindByPredicate([](const auto& Card){return Card.CardId==FName(TEXT("Profession.Sorcerer.NingYanChengRen"));})->InstanceId;
	const bool LightningPlayed=GameXXKCardRules::ResolveCardPlay(Lightning,LightningId,NAME_None,Result,&Error);
	if(!LightningPlayed)AddError(TEXT("Lightning fixture: ")+Error);TestTrue(TEXT("actual marked lightning card resolves"),LightningPlayed);
	TestEqual(TEXT("three locked marks remain three strikes"),Result.DamageResults.Num(),3);
	for(const auto& Hit:Result.DamageResults)TestEqual(TEXT("each actual lightning hit receives the bonus"),Hit.RequestedDamage,770);
	FGameXXKCardBattleRuntime Armor;if(!RuntimeFixture(*this,Armor,TEXT("Profession.Sorcerer.LingYanLianDan")))return false;
	FindUnit(Armor,SorcererId)->Armor=60;FindUnit(Armor,SorcererId)->GemBonusBasisPoints.Add(EGameXXKGemType::ArmorGain,8600);
	const FName ArmorId=Armor.Deck.Hand.FindByPredicate([](const auto& Card){return Card.CardId==FName(TEXT("Profession.Sorcerer.LingYanLianDan"));})->InstanceId;
	const bool ArmorPlayed=GameXXKCardRules::ResolveCardPlay(Armor,ArmorId,NAME_None,Result,&Error);
	if(!ArmorPlayed)AddError(TEXT("Armor fixture: ")+Error);TestTrue(TEXT("armor-copy card resolves"),ArmorPlayed);
	TestEqual(TEXT("copied armor is not multiplied again"),FindUnit(Armor,SorcererId)->Armor,120);
	TestEqual(TEXT("active healing gets the diminishing bonus once"),FGameXXKCombatGemRules::ApplyHealing(*FindUnit(Armor,SorcererId),1000,EGameXXKCardResolutionOrigin::ActivePlay),1000);
	FindUnit(Armor,SorcererId)->GemBonusBasisPoints.Add(EGameXXKGemType::Healing,8600);
	TestEqual(TEXT("active healing uses integer settlement"),FGameXXKCombatGemRules::ApplyHealing(*FindUnit(Armor,SorcererId),1000,EGameXXKCardResolutionOrigin::ActivePlay),1400);
	TestEqual(TEXT("fixed equipment restoration stays unchanged"),FGameXXKCombatGemRules::ApplyHealing(*FindUnit(Armor,SorcererId),1000,EGameXXKCardResolutionOrigin::Equipment),1000);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKGemQualityTooltipTest,"GameXXK.Equipment.Gems.Presentation.SharedQualityAndMixedSockets",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKGemQualityTooltipTest::RunTest(const FString& Parameters)
{
	auto* Tree=NewObject<UWidgetTree>();auto* Button=Tree->ConstructWidget<UButton>();auto* Overlay=Tree->ConstructWidget<UOverlay>();auto* Art=Tree->ConstructWidget<UImage>();Overlay->AddChildToOverlay(Art);Button->SetContent(Overlay);Tree->RootWidget=Button;
	for(int32 Rank=1;Rank<=10;++Rank)
	{
		const auto Q=FGameXXKGemRules::QualityFromRank(Rank);const auto Id=FGameXXKGemRules::MakeItemId(EGameXXKGemType::FireDamage,Q);
		TestTrue(TEXT("gem card uses shared equipment quality and tooltip"),GameXXKEquipmentTooltipPresentation::ApplyGem(Tree,Button,Id,FVector2D(64,64),Art));
		TestEqual(TEXT("quality changes do not accumulate visual layers"),Overlay->GetChildrenCount(),4);
		bool Found=false;
		for(auto* Label:Texts(Button->GetToolTip()))if(Label->GetText().EqualTo(FGameXXKGemRules::GetDisplayName(EGameXXKGemType::FireDamage,Q)))
		{
			Found=true;TestEqual(TEXT("gem name follows equipment quality outline"),Label->GetFont().OutlineSettings.OutlineSize,Rank==1?0:Rank>=6?3:2);
		}
		TestTrue(TEXT("typed and quality-specific gem name is present"),Found);
	}
	auto* Sub=NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());if(!Sub->StartGame())return false;
	auto& Collection=Sub->GetMutableRuntimeState().EquipmentCollection;FGameXXKEquipmentCreateRequest Request;Request.Set=EGameXXKEquipmentSet::ShanHe;Request.Quality=EGameXXKEquipmentQuality::Cosmic;Request.ItemLevel=1;FName Id;
	if(!FGameXXKEquipmentRules::CreateRolledInstance(Collection,Request,Id))return false;
	auto* Item=Collection.EquipmentInstances.FindByPredicate([Id](const auto& E){return E.InstanceId==Id;});
	Item->SocketedGems={{EGameXXKGemType::Attack,EGameXXKGemQuality::Common},{EGameXXKGemType::DefensePercent,EGameXXKGemQuality::Rare},{EGameXXKGemType::FrostDamage,EGameXXKGemQuality::Treasure},{EGameXXKGemType::LightningDamage,EGameXXKGemQuality::Cosmic},{},{}};
	auto* Tooltip=GameXXKEquipmentTooltipPresentation::Build(Tree,Sub,Id);const auto Labels=Texts(Tooltip);
	for(const auto& Gem:Item->SocketedGems)
	{
		if(Gem.IsEmpty())continue;bool Found=false;
		for(auto* Label:Labels)if(Label->GetText().EqualTo(FGameXXKGemRules::GetSocketText(Gem.Type,Gem.Quality)))
		{
			Found=true;const int32 Rank=FGameXXKGemRules::GetQualityRank(Gem.Quality);
			TestEqual(TEXT("each socket text follows its gem, not its cosmic host"),Label->GetFont().OutlineSettings.OutlineSize,Rank==1?0:Rank>=6?3:2);
		}
		TestTrue(TEXT("every mixed-quality socket has its own styled label"),Found);
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKGemEquipmentProjectionTest,"GameXXK.Equipment.Gems.Mechanics.EquipmentAndPartyProjection",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKGemEquipmentProjectionTest::RunTest(const FString& Parameters)
{
	auto* Sub=NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());if(!Sub->StartGame())return false;
	auto& State=Sub->GetMutableRuntimeState();const FName HeroId=FGameXXKEquipmentRules::HeroCharacterId();
	FGameXXKSaveState Legacy=UGameXXKMVPRules::MakeSaveState(State);Legacy.SaveVersion=38;
	FGameXXKSaveState Migrated;FGameXXKSaveMigrationReport Report;
	TestTrue(TEXT("previous-version save migrates without inventing gem bonuses"),FGameXXKSaveMigration::MigrateToCurrent(Legacy,Migrated,Report));
	TestEqual(TEXT("migration retains the player's gold"),Migrated.RuntimeState.PlayerGold,State.PlayerGold);
	TArray<FName> Owners={HeroId};for(const auto& Companion:State.CardRun.CompanionRoster.PermanentCompanions)Owners.Add(Companion.InstanceId);
	if(!TestTrue(TEXT("party projection has two permanent companions"),Owners.Num()>=3))return false;
	State.CardRun.OrderedFormation.Members={{EGameXXKPartyMemberKind::Hero,HeroId},{EGameXXKPartyMemberKind::PermanentCompanion,Owners[1]},{EGameXXKPartyMemberKind::PermanentCompanion,Owners[2]}};
	FGameXXKPartyFormationRules::ProjectCompatibility(State);
	FName HeroItem;
	for(const FName Owner:Owners)
	{
		FGameXXKEquipmentCreateRequest Request;Request.Set=EGameXXKEquipmentSet::ShanHe;Request.Quality=EGameXXKEquipmentQuality::Cosmic;Request.ItemLevel=1;Request.bForceSlot=true;Request.ForcedSlot=EGameXXKEquipmentSlot::Weapon;
		FName Id;FString Error;if(!FGameXXKEquipmentRules::CreateRolledInstance(State.EquipmentCollection,Request,Id,&Error)){AddError(Error);return false;}
		auto* Item=State.EquipmentCollection.EquipmentInstances.FindByPredicate([Id](const auto& E){return E.InstanceId==Id;});
		Item->SocketedGems[0]={EGameXXKGemType::Healing,EGameXXKGemQuality::Cosmic};
		if(Owner==HeroId){HeroItem=Id;for(auto& Gem:Item->SocketedGems)Gem={};}
		if(!TestTrue(TEXT("gems equip through the normal loadout transaction"),FGameXXKEquipmentRules::EquipInstance(State.EquipmentCollection,State.CardRun.CompanionRoster,Owner,EGameXXKEquipmentSlot::Weapon,Id).bSucceeded))return false;
	}
	FGameXXKEquipmentLoadoutSnapshot Before,After;if(!Sub->GetEquipmentLoadoutSnapshot(HeroId,Before))return false;
	auto* Item=State.EquipmentCollection.EquipmentInstances.FindByPredicate([HeroItem](const auto& E){return E.InstanceId==HeroItem;});
	Item->SocketedGems={{EGameXXKGemType::Attack,EGameXXKGemQuality::Cosmic},{EGameXXKGemType::AttackPercent,EGameXXKGemQuality::Cosmic},
		{EGameXXKGemType::DefensePercent,EGameXXKGemQuality::Cosmic},{EGameXXKGemType::MaxHealthPercent,EGameXXKGemQuality::Cosmic},
		{EGameXXKGemType::DirectDamage,EGameXXKGemQuality::Cosmic},{EGameXXKGemType::ArmorGain,EGameXXKGemQuality::Cosmic}};
	if(!Sub->GetEquipmentLoadoutSnapshot(HeroId,After))return false;
	const double StatMultiplier=1.0+0.75*43.0/118.0;
	TestEqual(TEXT("attack percent includes naked, equipment and fixed gem attack"),After.AttributesBeforeRoute.Attack,static_cast<decltype(After.AttributesBeforeRoute.Attack)>(static_cast<int32>(FMath::RoundToInt((Before.AttributesBeforeRoute.Attack+172)*StatMultiplier))));
	TestEqual(TEXT("defense percent has an independent pool"),After.AttributesBeforeRoute.Defense,static_cast<decltype(After.AttributesBeforeRoute.Defense)>(static_cast<int32>(FMath::RoundToInt(Before.AttributesBeforeRoute.Defense*StatMultiplier))));
	TestEqual(TEXT("health percent has an independent pool"),After.AttributesBeforeRoute.MaxHealth,static_cast<decltype(After.AttributesBeforeRoute.MaxHealth)>(static_cast<int32>(FMath::RoundToInt(Before.AttributesBeforeRoute.MaxHealth*StatMultiplier))));
	TestEqual(TEXT("mechanic gem does not inflate the base Attack pool"),After.SocketGemBasisPoints.FindRef(EGameXXKGemType::DirectDamage),8600);
	FGameXXKBattleRuntimeUnit Enemy;Enemy.Id=TEXT("GemProjection.Enemy");Enemy.DisplayName=FText::FromString(TEXT("测试敌人"));Enemy.HP=Enemy.MaxHP=10000;Enemy.Attack=1;Enemy.Speed=1;Enemy.bEnemy=true;
	State.ActiveBattleParty.Reset();State.ActiveBattleEnemies={Enemy};State.bHasActiveBattle=true;State.ActiveBattleNodeId=INDEX_NONE;FString Error;
	if(!TestTrue(TEXT("normal battle adapter materializes gem snapshots"),FGameXXKCardBattleAdapter::BeginCardBattle(State,EGameXXKNodeKind::Battle,EGameXXKCardTerrain::Plain,24613,&Error))){AddError(Error);return false;}
	int32 Wearers=0;
	for(const auto& Unit:State.CardRun.ActiveBattle.Units)
	{
		if(Unit.Side!=EGameXXKCardTargetSide::Party)continue;++Wearers;
		if(Unit.UnitId==HeroId)
		{
			TestEqual(TEXT("battle attack is the authoritative gem projection"),Unit.Attack,After.AttributesBeforeRoute.Attack);
			TestEqual(TEXT("hero keeps its direct gem"),Unit.GemBonusBasisPoints.FindRef(EGameXXKGemType::DirectDamage),8600);
			TestEqual(TEXT("hero does not borrow companion healing"),Unit.GemBonusBasisPoints.FindRef(EGameXXKGemType::Healing),0);
		}
		else TestEqual(TEXT("each deployed companion including the second owns its healing gem"),Unit.GemBonusBasisPoints.FindRef(EGameXXKGemType::Healing),8600);
	}
	TestEqual(TEXT("all three deployed characters were checked"),Wearers,3);
	return true;
}
#endif
