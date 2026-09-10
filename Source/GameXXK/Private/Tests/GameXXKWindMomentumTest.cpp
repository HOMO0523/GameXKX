#include "Misc/AutomationTest.h"
#include "GameXXKAffixCatalog.h"
#include "GameXXKCardRules.h"
#include "GameXXKEquipmentRules.h"
#include "GameXXKEquipmentCatalog.h"
#include "GameXXKEquipmentEconomyRules.h"
#include "GameXXKMVPRules.h"
#include "MVP/GameXXKSaveMigration.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "Engine/GameInstance.h"
#include "UI/GameXXKEquipmentTooltipPresentation.h"
#include "UI/GameXXKLocalization.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace WindTest
{
	const FName WindId(TEXT("Affix.ZhuiFeng.LateCardDamage"));
	FGameXXKEquipmentAffixRoll Roll(const TCHAR* Id, EGameXXKAffixTier Tier = EGameXXKAffixTier::Common)
	{
		FGameXXKEquipmentAffixRoll R; R.AffixId = Id; R.Tier = Tier;
		R.Unit = FGameXXKAffixCatalog::FindDefinition(R.AffixId)->Unit;
		R.Magnitude = FGameXXKAffixCatalog::GetMagnitudeRange(R.AffixId, Tier).Maximum;
		return R;
	}
	bool Build(FAutomationTestBase& Test, FGameXXKCardBattleRuntime& R, const TCHAR* CardId = TEXT("Hero.Generic.HeYuZhan"))
	{
		TArray<FGameXXKCardInstance> Cards;
		for (int32 I=0; I<8; ++I)
		{
			auto& C=Cards.AddDefaulted_GetRef(); C.InstanceId=FName(*FString::Printf(TEXT("Wind.Card%d"),I));
			C.CardId=CardId; C.OwnerUnitId=TEXT("Hero"); C.CurrentQuality=EGameXXKCardQuality::Common;
			C.SourceEntryId=C.InstanceId; C.AcquisitionOrdinal=I;
		}
		TArray<FGameXXKCardCombatUnit> Units;
		for (int32 I=0; I<3; ++I)
		{
			auto& U=Units.AddDefaulted_GetRef(); U.UnitId=I==0?TEXT("Hero"):I==1?TEXT("Ally"):TEXT("Enemy");
			U.Side=I<2?EGameXXKCardTargetSide::Party:EGameXXKCardTargetSide::Enemy;
			U.Role=I<2?EGameXXKCharacterRole::Hero:EGameXXKCharacterRole::Invalid;
			U.HP=U.MaxHP=100000; U.Mana=U.MaxMana=I<2?100:0; U.Attack=1000;
			U.Defense=0; U.Speed=1; U.StableSortOrder=I+1; U.bLiving=true;
		}
		FString Error;
		if (!Test.TestTrue(TEXT("valid live battle fixture"),GameXXKCardRules::InitializeCardBattleRuntime(R,Cards,Units,EGameXXKCardTerrain::Plain,711,&Error)))
		{Test.AddError(Error);return false;}
		R.Deck.Hand=Cards;R.Deck.DrawPile.Reset();R.Deck.DiscardPile.Reset();R.Deck.ExhaustPile.Reset();R.Deck.SharedEnergy=20;
		return true;
	}
	void Add(FGameXXKCardBattleRuntime& R,int32 Copies,FName Owner=TEXT("Hero"))
	{
		auto& Entry=R.EquipmentEffects.AddDefaulted_GetRef(); auto& E=Entry.ActiveEffect;
		E.EffectId=FName(*FString::Printf(TEXT("EquipmentAffixAggregate.%d.%d"),static_cast<int32>(EGameXXKEquipmentSet::ZhuiFeng),static_cast<int32>(EGameXXKEquipmentModifierKind::ZhuiFengLateCardDamage)));
		E.Set=EGameXXKEquipmentSet::ZhuiFeng;E.ModifierKind=EGameXXKEquipmentModifierKind::ZhuiFengLateCardDamage;
		E.SourceCharacterId=Entry.SourceCharacterId=Owner;E.Scope=EGameXXKEquipmentSetBonusScope::Owner;
		E.Hook=EGameXXKEquipmentSetBonusHook::Passive;E.Unit=EGameXXKEquipmentMagnitudeUnit::BasisPoints;E.Magnitude=100*Copies;
	}
	bool Play(FAutomationTestBase& T,FGameXXKCardBattleRuntime& R,FName Id,FGameXXKCardPlayResult& Result)
	{
		FString Error; FGameXXKCardPlayPreview Preview;
		if(!GameXXKCardRules::BuildCardPlayPreview(R,Id,Preview,&Error)){T.AddError(Error);return false;}
		const FName Target=Preview.TargetRequest.bRequiresManualSelection
			?FName(Preview.TargetRequest.EffectiveMode==EGameXXKCardTargetMode::SingleAlly?TEXT("Ally"):TEXT("Enemy")):NAME_None;
		const bool Ok=GameXXKCardRules::ResolveCardPlay(R,Id,Target,Result,&Error);
		T.TestTrue(TEXT("actual card resolves"),Ok);if(!Ok)T.AddError(Error);return Ok;
	}
	void Compare(FAutomationTestBase& T,const FGameXXKCardPlayResult& Base,const FGameXXKCardPlayResult& Wind,int32 Percent)
	{
		T.TestEqual(TEXT("same number of damage packets"),Wind.DamageResults.Num(),Base.DamageResults.Num());
		int32 Direct=0;
		for(int32 I=0;I<Base.DamageResults.Num()&&Wind.DamageResults.IsValidIndex(I);++I)
		{
			const auto& A=Base.DamageResults[I];const auto& B=Wind.DamageResults[I];
			const bool Attack=A.Cause==EGameXXKCardDamageCause::DirectAttack;
			if(Attack)++Direct;
			T.TestEqual(TEXT("only direct card attacks gain exact additive percent"),B.BaseRequestedDamage,
				Attack?static_cast<int32>(static_cast<int64>(A.BaseRequestedDamage)*(100+Percent)/100):A.BaseRequestedDamage);
		}
		T.TestTrue(TEXT("comparison contains actual direct damage"),Direct>0);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKWindMomentumRuntimeTest,"GameXXK.Equipment.WindMomentum.Runtime",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKWindMomentumRuntimeTest::RunTest(const FString&)
{
	using namespace WindTest;
	for(int32 Copies:{1,2})
	{
		FGameXXKCardBattleRuntime Base;if(!Build(*this,Base))return false;
		auto Wind=Base;Add(Wind,Copies);
		for(int32 I=0;I<5;++I)
		{
			const FName Id(*FString::Printf(TEXT("Wind.Card%d"),I));FGameXXKCardPlayResult A,B;
			if(!Play(*this,Base,Id,A)||!Play(*this,Wind,Id,B))return false;
			Compare(*this,A,B,FMath::Max(0,I-1)*Copies);
			TestEqual(TEXT("affix adds no fake active-card count"),Wind.ActiveCardsPlayedThisRound,I+1);
		}
		FGameXXKResolvedCardSnapshot Snapshot;Snapshot.CardId=TEXT("Hero.Generic.HeYuZhan");
		Snapshot.Quality=EGameXXKCardQuality::Common;Snapshot.OwnerUnitId=TEXT("Hero");Snapshot.OriginalTargetUnitIds={TEXT("Enemy")};
		for(auto* R:{&Base,&Wind}){auto& Q=R->AutomaticResolutionQueue;Q.bActive=true;Q.Origin=EGameXXKCardResolutionOrigin::AutomaticReplay;Q.PendingCards={Snapshot,Snapshot};}
		TArray<FGameXXKCardPlayResult> A,B;FString Error;
		if(!TestTrue(TEXT("baseline replays"),GameXXKCardRules::ResumeAutomaticResolutionQueue(Base,A,&Error))
			||!TestTrue(TEXT("wind replays"),GameXXKCardRules::ResumeAutomaticResolutionQueue(Wind,B,&Error))){AddError(Error);return false;}
		TestEqual(TEXT("two replays resolve"),B.Num(),2);
		for(int32 I=0;I<A.Num()&&B.IsValidIndex(I);++I)Compare(*this,A[I],B[I],3*Copies);
		TestEqual(TEXT("replays do not advance count"),Wind.ActiveCardsPlayedThisRound,5);
		TArray<FGameXXKCardDamageResult> End;
		for(auto* R:{&Base,&Wind})
		{
			TestTrue(TEXT("end phase"),GameXXKCardRules::EndPlayerCardPhase(*R,End,&Error));
			TestTrue(TEXT("next round"),GameXXKCardRules::BeginNextPlayerCardRound(*R,End,&Error));
		}
		TestEqual(TEXT("new round resets count"),Wind.ActiveCardsPlayedThisRound,0);
		FGameXXKCardPlayResult NextA,NextB;
		if(!Base.Deck.Hand.IsEmpty()&&Play(*this,Base,Base.Deck.Hand[0].InstanceId,NextA)&&Play(*this,Wind,Wind.Deck.Hand[0].InstanceId,NextB))Compare(*this,NextA,NextB,0);
	}
	for(const TCHAR* Id:{TEXT("Hero.Generic.QingFengYiShi"),TEXT("Hero.Mage.YanXuLiaoYuan"),TEXT("Hero.Guard.XuanJiaZhenYue"),TEXT("Hero.Mage.LeiXuYinTing"),TEXT("Hero.Hunter.LieYuLianShi")})
	{
		FGameXXKCardBattleRuntime Base;if(!Build(*this,Base,Id))return false;Base.ActiveCardsPlayedThisRound=2;
		Base.Units[1].Armor=20; // The Frost card consumes the chosen ally's armor.
		if(FName(Id)==FName(TEXT("Hero.Hunter.LieYuLianShi")))GameXXKCardRules::AddCombatStatus(Base.Units[0],EGameXXKCardStatus::Charge,2);
		auto Wind=Base;Add(Wind,2);auto Other=Base;Add(Other,2,TEXT("Ally"));FGameXXKCardPlayResult A,B,C;
		if(!Play(*this,Base,TEXT("Wind.Card0"),A)||!Play(*this,Wind,TEXT("Wind.Card0"),B)||!Play(*this,Other,TEXT("Wind.Card0"),C))return false;
		Compare(*this,A,B,2);Compare(*this,A,C,0);
	}
	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKWindMomentumGearTest,"GameXXK.Equipment.WindMomentum.GearAndReforge",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKWindMomentumGearTest::RunTest(const FString&)
{
	using namespace WindTest;
	FString Error;FGameXXKEquipmentCollectionState Col;FGameXXKCompanionRosterState Roster;
	for(const auto Slot:{EGameXXKEquipmentSlot::Weapon,EGameXXKEquipmentSlot::Head})
	{
		FGameXXKEquipmentCreateRequest Request;Request.Set=EGameXXKEquipmentSet::ZhuiFeng;
		Request.Quality=EGameXXKEquipmentQuality::Common;Request.bForceSlot=true;Request.ForcedSlot=Slot;
		FName Id;if(!TestTrue(TEXT("create physical Wind item"),FGameXXKEquipmentRules::CreateRolledInstance(Col,Request,Id,&Error)))return false;
		auto* Item=Col.EquipmentInstances.FindByPredicate([&](const auto& I){return I.InstanceId==Id;});
		Item->RolledAffixes={Roll(TEXT("Affix.ZhuiFeng.LateCardDamage"))};
		TestTrue(TEXT("equip Wind item"),FGameXXKEquipmentRules::EquipInstance(Col,Roster,FGameXXKEquipmentRules::HeroCharacterId(),Slot,Id).bSucceeded);
	}
	FGameXXKEquipmentLoadoutSnapshot Snapshot;FGameXXKCharacterStats Bare;Bare.MaxHealth=100;Bare.MaxMana=30;Bare.Attack=10;Bare.Defense=10;
	if(!TestTrue(TEXT("real loadout projects"),FGameXXKEquipmentRules::BuildLoadoutSnapshot(Col,FGameXXKEquipmentRules::HeroCharacterId(),Bare,Snapshot,&Error))){AddError(Error);return false;}
	const auto* Affix=Snapshot.ActivePersonalEffects.FindByPredicate([](const auto& E){return E.ModifierKind==EGameXXKEquipmentModifierKind::ZhuiFengLateCardDamage;});
	TestNotNull(TEXT("projected affix exists"),Affix);if(Affix)TestEqual(TEXT("two real equipped copies aggregate to 200 basis points"),Affix->Magnitude,200);
	for(int32 Rank=1;Rank<=10;++Rank)
	{
		auto State=UGameXXKMVPRules::CreateNewGame();State.Inventory.Add(UGameXXKMVPRules::ItemRefinementSand(),1000);
		FGameXXKEquipmentCreateRequest Request;Request.Set=EGameXXKEquipmentSet::ZhuiFeng;
		Request.Quality=FGameXXKEquipmentQualityRules::EquipmentQualityFromRank(Rank);FName Id;
		if(!TestTrue(TEXT("each gear quality still rolls valid distinct affixes"),FGameXXKEquipmentRules::CreateRolledInstance(State.EquipmentCollection,Request,Id,&Error)))return false;
		const auto Item=*FGameXXKEquipmentRules::FindInstance(State.EquipmentCollection,Id);
		for(int32 I=0;I<Item.RolledAffixes.Num();++I)
		{
			const auto& R=Item.RolledAffixes[I];TestFalse(TEXT("new gear has no retired resource affix"),FGameXXKAffixCatalog::IsRetiredResourceAffix(R.AffixId));
			if(R.AffixId==WindId)TestEqual(TEXT("actual rolled Wind magnitude is fixed"),R.Magnitude,100);
			auto Attempt=State;FGameXXKEquipmentTransactionResult Result;
			const bool Ok=FGameXXKEquipmentEconomyRules::BeginReforge(Attempt,Id,I,Result);
			const bool FixedFullPool=R.AffixId==WindId&&Item.RolledAffixes.Num()==5;
			TestEqual(TEXT("full pool permits numeric rerolls but does not waste sand on a fixed-only roll"),Ok,!FixedFullPool);
			if(Ok)
			{
				const auto& Candidate=Attempt.EquipmentCollection.PendingReforge.CandidateAffix;
				TestFalse(TEXT("reforge never reintroduces retired resources"),FGameXXKAffixCatalog::IsRetiredResourceAffix(Candidate.AffixId));
				if(Candidate.AffixId==WindId)TestEqual(TEXT("reforged Wind stays fixed"),Candidate.Magnitude,100);
			}
			else TestTrue(TEXT("unavailable fixed reforge changes no state"),FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(&State,&Attempt,PPF_None));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKWindMomentumMigrationTest,"GameXXK.Equipment.WindMomentum.Migration",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKWindMomentumMigrationTest::RunTest(const FString&)
{
	using namespace WindTest;
	auto* MVP=NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if(!TestTrue(TEXT("saved fixture has an initialized legal party"),MVP->StartGame()))return false;
	auto State=MVP->GetRuntimeStateCopy();FString Error;FName Id;
	FGameXXKEquipmentCreateRequest Request;Request.Set=EGameXXKEquipmentSet::ZhuiFeng;Request.Quality=EGameXXKEquipmentQuality::Treasure;Request.ItemLevel=10;
	if(!TestTrue(TEXT("create old-style item fixture"),FGameXXKEquipmentRules::CreateRolledInstance(State.EquipmentCollection,Request,Id,&Error)))return false;
	auto* Item=State.EquipmentCollection.EquipmentInstances.FindByPredicate([&](const auto& I){return I.InstanceId==Id;});
	Item->EnhancementLevel=3;Item->RolledAffixes={Roll(TEXT("Affix.ZhuiFeng.Draw")),Roll(TEXT("Affix.ZhuiFeng.SharedEnergy")),Roll(TEXT("Affix.ZhuiFeng.TemporaryCostReduction")),Roll(TEXT("Affix.ZhuiFeng.ComboCount")),Roll(TEXT("Affix.Universal.Attack"))};
	Item->SocketedGems[0]={EGameXXKGemType::Attack,EGameXXKGemQuality::Rare};
	const auto Original=*Item;
	auto& Pending=State.EquipmentCollection.PendingReforge;Pending.bActive=true;Pending.InstanceId=Id;Pending.AffixIndex=0;
	Pending.OriginalAffix=Item->RolledAffixes[0];Pending.CandidateAffix=Roll(TEXT("Affix.ZhuiFeng.LowCostBonus"));Pending.PaidRefinementSand=5;
	// Let the fixture use the project's required paid amount rather than an invented one.
	Pending.PaidRefinementSand=FGameXXKEquipmentCatalog::GetReforgeSandCost(Item->Quality);
	Pending.ConsumedReforgeOrdinal=State.EquipmentCollection.NextReforgeOrdinal++;
	if(!TestTrue(TEXT("legacy collection validates"),FGameXXKEquipmentRules::ValidateCollectionState(State.EquipmentCollection,&Error))){AddError(Error);return false;}
	FGameXXKSaveState Migrated;FGameXXKSaveMigrationReport Report;
	if(!TestTrue(TEXT("current-version old rolls migrate on normal load"),FGameXXKSaveMigration::MigrateToCurrent(UGameXXKMVPRules::MakeSaveState(State),Migrated,Report))){AddError(Report.Error);return false;}
	const auto& Col=Migrated.RuntimeState.EquipmentCollection;const auto* Updated=FGameXXKEquipmentRules::FindInstance(Col,Id);
	TestNotNull(TEXT("stable item identity preserved"),Updated);if(!Updated)return false;
	TestEqual(TEXT("enhancement preserved"),Updated->EnhancementLevel,Original.EnhancementLevel);
	auto Expected=Original;Expected.RolledAffixes=Updated->RolledAffixes;
	TestTrue(TEXT("gems, ownership, quality and every unrelated item field are preserved"),FGameXXKEquipmentInstance::StaticStruct()->CompareScriptStruct(&Expected,Updated,PPF_None));
	TestEqual(TEXT("specifically retired cost discount becomes fixed 1 percent"),Updated->RolledAffixes[2].AffixId,WindId);
	TestEqual(TEXT("fixed 1 percent value"),Updated->RolledAffixes[2].Magnitude,100);
	TestTrue(TEXT("unrelated attack roll preserved"),FGameXXKEquipmentAffixRoll::StaticStruct()->CompareScriptStruct(&Updated->RolledAffixes[4],&Original.RolledAffixes[4],PPF_None));
	for(const auto& R:Updated->RolledAffixes)TestFalse(TEXT("retired rolls removed"),FGameXXKAffixCatalog::IsRetiredResourceAffix(R.AffixId));
	TestEqual(TEXT("paid reforge not charged twice"),Col.PendingReforge.PaidRefinementSand,Pending.PaidRefinementSand);
	auto Twice=Col;TestTrue(TEXT("repeat normalization"),FGameXXKEquipmentRules::NormalizeRetiredResourceAffixes(Twice,&Error));
	TestTrue(TEXT("migration is idempotent"),FGameXXKEquipmentCollectionState::StaticStruct()->CompareScriptStruct(&Col,&Twice,PPF_None));
	for(bool Keep:{false,true}){auto Choice=Migrated.RuntimeState;FGameXXKEquipmentTransactionResult Result;TestTrue(TEXT("both paid-preview choices remain usable"),FGameXXKEquipmentEconomyRules::ResolvePendingReforge(Choice,Keep,Result));}
	const FString Language=GameXXKLocalization::GetLanguage();ON_SCOPE_EXIT{GameXXKLocalization::SetLanguage(Language,false);};
	for(const TCHAR* Lang:{TEXT("zh-Hans"),TEXT("en")}){GameXXKLocalization::SetLanguage(Lang,false);const auto Text=GameXXKEquipmentTooltipPresentation::AffixLine(Updated->RolledAffixes[2]);TestTrue(TEXT("tooltip explains incremental 1 percent"),Text.Contains(TEXT("1%")));if(GameXXKLocalization::IsEnglish())for(TCHAR C:Text)if(C>=0x3400&&C<=0x9fff)AddError(TEXT("Untranslated wind tooltip"));}
	return true;
}
#endif
