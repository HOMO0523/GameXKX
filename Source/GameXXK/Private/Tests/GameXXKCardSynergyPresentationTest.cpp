#include "Misc/AutomationTest.h"
#include "GameXXKCardCatalog.h"
#include "GameXXKCardRules.h"
#include "UI/GameXXKCardSynergyPresentation.h"
#include "UI/GameXXKCardVisualEffects.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
	using K = EGameXXKCardSynergyKind;
	FGameXXKCardBattleRuntime Runtime()
	{
		FGameXXKCardBattleRuntime R; R.Phase=EGameXXKCardBattlePhase::Player; R.RoundNumber=1;
		FGameXXKCardCombatUnit U; U.UnitId=TEXT("Owner"); U.Side=EGameXXKCardTargetSide::Party;
		U.bLiving=true; U.HP=50; U.MaxHP=100; U.Attack=10; R.Units.Add(U); return R;
	}
	FGameXXKCardInstance Instance(const FGameXXKCardDefinition& D)
	{
		FGameXXKCardInstance C; C.InstanceId=TEXT("Card.Instance"); C.CardId=D.Id; C.OwnerUnitId=TEXT("Owner"); C.CurrentQuality=D.BaseQuality; return C;
	}
	bool Has(const FGameXXKCardBattleRuntime& R,const FGameXXKCardInstance& C,const FGameXXKCardDefinition& D,K Kind)
	{
		return GameXXKCardSynergyPresentation::Build(R,C,D).ContainsByPredicate([Kind](const auto& Cue){return Cue.Kind==Kind;});
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKFormulaCueTest,"GameXXK.MVP.UI.CardEffects.FormulaOwnerAndOpening",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKFormulaCueTest::RunTest(const FString&)
{
	const auto* D=FGameXXKCardCatalog::GetAllCardDefinitions().FindByPredicate([](const auto& X){return X.HealerRule.FormulaKind!=EGameXXKHealerFormulaKind::None;});
	if(!TestNotNull(TEXT("real formula card exists"),D))return false;
	auto R=Runtime(); auto C=Instance(*D);
	TestTrue(TEXT("unopened formula is visibly signalled"),Has(R,C,*D,K::FormulaOpening));
	FGameXXKHealerFormulaRuntime F; F.OwnerUnitId=TEXT("Other");F.SourceCardId=D->Id;F.Kind=D->HealerRule.FormulaKind;R.HealerFormulas.Add(F);
	TestTrue(TEXT("another owner's formula does not suppress this opening"),Has(R,C,*D,K::FormulaOpening));
	R.HealerFormulas[0].OwnerUnitId=C.OwnerUnitId;
	TestFalse(TEXT("opened formula stops claiming to be unopened"),Has(R,C,*D,K::FormulaOpening));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKSpellCueTest,"GameXXK.MVP.UI.CardEffects.SpellTaskProgress",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKSpellCueTest::RunTest(const FString&)
{
	const auto* D=FGameXXKCardCatalog::GetAllCardDefinitions().FindByPredicate([](const auto& X){return X.Owner==EGameXXKCardOwner::Profession && X.Role==EGameXXKCharacterRole::Sorcerer;});
	if(!D)return false;auto R=Runtime();auto C=Instance(*D);
	FGameXXKSorcererPartnerTaskRuntime T;T.bActive=true;T.OwnerUnitId=C.OwnerUnitId;T.LockedCardIds.Add(D->Id);R.SorcererPartnerTasks.Add(T);
	TestTrue(TEXT("unfinished carried spell glows"),Has(R,C,*D,K::SpellTask));
	R.SorcererPartnerTasks[0].CompletedCardIds.Add(D->Id);
	TestFalse(TEXT("recorded spell stops glowing for that task"),Has(R,C,*D,K::SpellTask));
	R.SorcererPartnerTasks[0].CompletedCardIds.Reset();C.bTemporary=true;
	TestFalse(TEXT("temporary partner spells do not advance the five-card task"),Has(R,C,*D,K::SpellTask));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKBladeCueTest,"GameXXK.MVP.UI.CardEffects.BladeAndNpcTiming",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKBladeCueTest::RunTest(const FString&)
{
	const auto* D=FGameXXKCardCatalog::FindCardDefinition(TEXT("Npc.TusiChief.TuSiJunLing"));
	if(!D)return false;auto R=Runtime();auto C=Instance(*D);
	TestTrue(TEXT("NPC first-card opening uses the same cue"),Has(R,C,*D,K::BladeOpening));
	R.ActiveCardsPlayedThisRound=1;
	TestFalse(TEXT("opening closes after first active play"),Has(R,C,*D,K::BladeOpening));
	TestTrue(TEXT("unplayed finisher remains only a candidate"),Has(R,C,*D,K::BladeFinishCandidate));
	TestTrue(TEXT("no actual last card means no registered finisher"),GameXXKCardSynergyPresentation::FinisherHint(R).IsEmpty());
	R.LastActiveCard.CardId=D->Id;R.LastActiveCard.OwnerUnitId=C.OwnerUnitId;
	TestFalse(TEXT("end turn identifies the real last-active finisher"),GameXXKCardSynergyPresentation::FinisherHint(R).IsEmpty());
	R.Phase=EGameXXKCardBattlePhase::Enemy;
	TestTrue(TEXT("enemy phase clears actionable hand cues"),GameXXKCardSynergyPresentation::Build(R,C,*D).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKResourceCueTest,"GameXXK.MVP.UI.CardEffects.ArmorTerrainAndNpcResources",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKResourceCueTest::RunTest(const FString&)
{
	auto R=Runtime();FGameXXKCardDefinition D;D.Id=TEXT("Test.ArmorConversion");D.Role=EGameXXKCharacterRole::Guard;
	FGameXXKCardEffect E;E.Type=EGameXXKCardEffectType::DamagePercentAttackPlusArmor;E.Source=EGameXXKCardEffectSource::CardOwner;D.Effects.Add(E);auto C=Instance(D);
	TestFalse(TEXT("zero armor offers no conversion benefit"),Has(R,C,D,K::ArmorConversion));
	R.Units[0].Armor=20;TestTrue(TEXT("actual source armor enables blue conversion cue"),Has(R,C,D,K::ArmorConversion));
	const auto* Terrain=FGameXXKCardCatalog::FindCardDefinition(TEXT("Profession.FormationMaster.GuanShi"));
	if(!Terrain)return false;C=Instance(*Terrain);R.Terrain=EGameXXKCardTerrain::Plain;
	TestTrue(TEXT("terrain payload is described on the actual card"),Has(R,C,*Terrain,K::Terrain));
	GameXXKCardRules::AddCombatStatus(R.Units[0],EGameXXKCardStatus::NextTerrainCardFree,1);
	TestTrue(TEXT("ready terrain discount increases the cue"),Has(R,C,*Terrain,K::TerrainEnhanced));
	const auto* Npc=FGameXXKCardCatalog::FindCardDefinition(TEXT("Npc.QiongMeiEr.TengQiaoFeiDu"));
	if(!Npc)return false;C=Instance(*Npc);
	TestTrue(TEXT("NPC grant-then-heavy-arrow is represented before the play"),Has(R,C,*Npc,K::HeavyArrow));
	const auto* Zhou=FGameXXKCardCatalog::FindCardDefinition(TEXT("Npc.ZhouGuangZu.YiCaoBianShi"));
	if(!Zhou)return false;C=Instance(*Zhou);
	TestFalse(TEXT("NPC medicine does not invent a formula opening"),Has(R,C,*Zhou,K::FormulaOpening));
	TestTrue(TEXT("NPC medicine-healing synergy is visible"),Has(R,C,*Zhou,K::MedicineReady));
	R.Units[0].bLiving=false;TestTrue(TEXT("dead owners leave no glow behind"),GameXXKCardSynergyPresentation::Build(R,C,*Zhou).IsEmpty());
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKCardMotionEndpointTest,"GameXXK.MVP.UI.CardEffects.MotionEndsFullyVisible",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKCardMotionEndpointTest::RunTest(const FString&)
{
	for(int32 I=0;I<5;++I)
	{
		const auto Start=GameXXKCardVisualEffects::Deal(0,I);
		const auto End=GameXXKCardVisualEffects::Deal(2,I);
		TestTrue(TEXT("new cards start outside their final hand position"),!Start.Offset.IsNearlyZero());
		TestTrue(TEXT("settled cards have no residual motion or transparency"),End.Offset.IsNearlyZero(0.001) && End.Scale.Equals(FVector2D(1,1),0.001) && End.Opacity==1);
		const auto Back=GameXXKCardVisualEffects::Flip(0,I);
		const auto Face=GameXXKCardVisualEffects::Flip(2,I);
		TestFalse(TEXT("rewards initially show their back"),Back.bFrontFace);
		TestTrue(TEXT("revealed rewards restore full width and opacity"),Face.bFrontFace && Face.Scale.Equals(FVector2D(1,1),0.001) && Face.Opacity==1);
	}
	const auto Middle=GameXXKCardVisualEffects::Flip(0.22f,0);
	TestTrue(TEXT("flip has a finite edge-on midpoint"),Middle.Scale.X>0 && Middle.Scale.X<0.1f);
	return true;
}
#endif
