#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "MVP/GameXXKSaveGame.h"
#include "MVP/GameXXKSaveMigration.h"
#include "Narrative/GameXXKMainStorySubsystem.h"
#include "GameXXKTalentCatalog.h"
#include "GameXXKTalentRules.h"
#include "GameXXKEquipmentEconomyRules.h"
#include "GameXXKCardBattleAdapter.h"
#include "GameXXKRelicRules.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKMainStoryAfterTalentTravelTest,
	"GameXXK.MainStory.EntryAfterTalentTravelPersists",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKMainStoryAfterTalentTravelTest::RunTest(const FString&)
{
	auto* Instance=NewObject<UGameInstance>();auto* MVP=NewObject<UGameXXKMVPSubsystem>(Instance);
	if(!TestTrue(TEXT("valid game starts"),MVP->StartGame()))return false;
	auto& State=MVP->GetMutableRuntimeState();
	State.Talents.NodeRanks.Add(TEXT("Talent.Root"),1);
	State.Talents.NodeRanks.Add(TEXT("Talent.Entry.Combat"),1);
	const int32 BaselineMax=State.PlayerMaxHP;
	if(!TestTrue(TEXT("ordinary idle travel starts with health talent"),MVP->StartTrainingTravel(TEXT("Training.Normal.1-1"))))return false;
	bool Encounter=false,Stage=false,Defeated=false;FGameXXKTrainingReward Reward;
	if(!TestTrue(TEXT("idle travel publishes its first health snapshot"),MVP->AdvanceTrainingTravelStep(Encounter,Stage,Defeated,Reward,1)))return false;
	TestEqual(TEXT("persistent maximum stays the equipment-only baseline"),MVP->GetRuntimeState().PlayerMaxHP,BaselineMax);
	TestEqual(TEXT("current health includes the real five-point talent"),MVP->GetRuntimeState().PlayerHP,BaselineMax+5);
	FString Error;
	TestTrue(*FString::Printf(TEXT("talented idle state remains saveable: %s"),*Error),FGameXXKSaveMigration::ValidateRuntimeState(MVP->GetRuntimeState(),Error));
	TArray<uint8> Bytes;int32 Writes=0;
	MVP->SetSaveSlotWriteDelegateForTest(FGameXXKSaveSlotWriteDelegate::CreateLambda([&](USaveGame* Save,const FString&,int32)
	{++Writes;return UGameplayStatics::SaveGameToMemory(Save,Bytes);}));
	auto* Story=NewObject<UGameXXKMainStorySubsystem>(Instance);Story->SetMVPForTest(MVP);
	TestTrue(TEXT("clicking the available first chapter succeeds after idle travel"),Story->OpenChapter(TEXT("S00")));
	if(Bytes.IsEmpty()){AddError(Story->Feedback().ToString());return false;}
	TestEqual(TEXT("opening records one durable viewed-chapter transaction"),Writes,1);
	auto* Saved=Cast<UGameXXKSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
	if(!TestNotNull(TEXT("checkpoint round-trip loads"),Saved))return false;
	FGameXXKSaveState Restored;FGameXXKSaveMigrationReport Migration;
	TestTrue(TEXT("the saved talent health survives normal migration"),FGameXXKSaveMigration::MigrateToCurrent(Saved->SaveState,Restored,Migration));
	TestEqual(TEXT("migration does not cut away the earned health bonus"),Restored.RuntimeState.PlayerHP,BaselineMax+5);
	TestTrue(TEXT("the chapter viewed flag survives loading"),Restored.RuntimeState.NarrativeProgress.MainStory.SeenChapters.Contains(TEXT("S00")));
	auto Invalid=Restored.RuntimeState;Invalid.PlayerHP=BaselineMax+6;
	TestFalse(TEXT("health above the actual talented maximum is still rejected"),FGameXXKSaveMigration::ValidateRuntimeState(Invalid,Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKTalentHealthDeficitBoundaryTest,
	"GameXXK.MainStory.TalentHealthDeficitSurvivesProjection",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKTalentHealthDeficitBoundaryTest::RunTest(const FString&)
{
	auto* MVP=NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if(!MVP->StartGame())return false;
	auto State=MVP->GetRuntimeStateCopy();
	for(const auto& Node:FGameXXKTalentCatalog::GetDefinitions())
		if(Node.bRoot||Node.Branch==EGameXXKTalentBranch::Combat)State.Talents.NodeRanks.Add(Node.Id,Node.MaxRank);
	State.PlayerHP=50;
	const int32 BaselineMax=State.PlayerMaxHP;
	TestTrue(TEXT("equipment mirror synchronization accepts talented health"),FGameXXKEquipmentEconomyRules::SynchronizeRuntimeMirrors(State));
	TestEqual(TEXT("mirror synchronization preserves existing damage"),State.PlayerHP,50);
	TestEqual(TEXT("permanent maxima are not double-counted"),State.PlayerMaxHP,BaselineMax);
	FGameXXKBattleRuntimeUnit Enemy;Enemy.Id=TEXT("Enemy.Health.Target.P1");Enemy.DisplayName=FText::FromString(TEXT("木桩"));
	Enemy.HP=10000;Enemy.MaxHP=10000;Enemy.Attack=1;Enemy.Defense=0;Enemy.Speed=1;Enemy.bEnemy=true;
	Enemy.EnemyDefinitionId=TEXT("Enemy.Ch1.Rooster");Enemy.BattleSlotNumber=1;Enemy.CombatLevel=1;
	State.ActiveBattleEnemies={Enemy};State.bHasActiveBattle=true;State.ActiveBattleNodeId=INDEX_NONE;
	FString Error;
	if(!TestTrue(TEXT("talented next battle begins"),FGameXXKCardBattleAdapter::BeginCardBattle(State,EGameXXKNodeKind::Battle,EGameXXKCardTerrain::Plain,0x5511,&Error)))return false;
	const auto* Hero=State.CardRun.ActiveBattle.Units.FindByPredicate([](const auto& Unit){return Unit.UnitId==TEXT("Player");});
	if(!TestNotNull(TEXT("hero exists"),Hero))return false;
	TestEqual(TEXT("a new battle does not heal damage by reapplying health talents"),Hero->HP,50);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKTalentRouteHealingBoundaryTest,
	"GameXXK.MainStory.TalentHealthSurvivesRouteHealing",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKTalentRouteHealingBoundaryTest::RunTest(const FString&)
{
	auto* MVP=NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if(!MVP->StartGame())return false;
	auto State=MVP->GetRuntimeStateCopy();
	State.Talents.NodeRanks.Add(TEXT("Talent.Root"),1);
	State.Talents.NodeRanks.Add(TEXT("Talent.Entry.Combat"),1);
	const int32 Maximum=State.PlayerMaxHP+5;
	State.PlayerHP=Maximum-2;
	FString Error;
	if(!TestTrue(TEXT("route healing relic acquired"),FGameXXKRelicRules::AcquireRelic(State,TEXT("Relic.HerbBasket"),&Error)))return false;
	FGameXXKRelicRules::ApplyRouteNodeCompletedNonCurrency(State);
	TestEqual(TEXT("route healing restores the talent portion instead of lowering current health"),State.PlayerHP,Maximum);
	FGameXXKRelicRules::ApplyRouteNodeCompletedNonCurrency(State);
	TestEqual(TEXT("healing at full talent health keeps the full value"),State.PlayerHP,Maximum);
	FGameXXKRelicRules::ClearRouteRelics(State);
	TestEqual(TEXT("route relic cleanup retains permanent talent health"),State.PlayerHP,Maximum);
	return true;
}
#endif
