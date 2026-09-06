#include "Misc/AutomationTest.h"
#include "Dev/GameXXKDevToolsSubsystem.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "GameXXKCombatSimulationRules.h"
#include "GameXXKEquipmentRules.h"
#include "GameXXKEquipmentCatalog.h"
#include "GameXXKDesktopInventoryRules.h"
#include "GameXXKCompanionCatalog.h"
#include "../Dev/GameXXKDevFixtures.h"
#include "Engine/GameInstance.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "JsonObjectConverter.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
namespace
{
	struct FDevFixture
	{
		UGameXXKMVPSubsystem* MVP;
		UGameXXKDevToolsSubsystem* Dev;
		int32 Writes=0;
		FDevFixture()
		{
			auto* GI=NewObject<UGameInstance>();MVP=NewObject<UGameXXKMVPSubsystem>(GI);
			MVP->SetSaveSlotWriteDelegateForTest(FGameXXKSaveSlotWriteDelegate::CreateLambda([this](USaveGame*,const FString&,int32){++Writes;return true;}));
			MVP->StartGame();Dev=NewObject<UGameXXKDevToolsSubsystem>(GI);Dev->SetMVPForTest(MVP);Writes=0;
		}
		~FDevFixture() { MVP->ResetSaveSlotWriteDelegateForTest(); }
		TSharedPtr<FJsonObject> Call(const FString& Request)
		{
			const auto Text=Dev->ExecuteJson(Request);TSharedPtr<FJsonObject> Result;
			auto R=TJsonReaderFactory<>::Create(Text);FJsonSerializer::Deserialize(R,Result);return Result;
		}
		bool OK(const FString& Request) { auto R=Call(Request);return R && R->GetBoolField(TEXT("ok")); }
	};
	FString StateText(const FGameXXKRuntimeState& S)
	{ FString Text;FJsonObjectConverter::UStructToJsonObjectString(S,Text);return Text; }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKDevSessionTest,"GameXXK.Development.SessionAndTransactions",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKDevSessionTest::RunTest(const FString&)
{
	FDevFixture F;const FString Original=StateText(F.MVP->GetRuntimeState());const int32 Gold=F.MVP->GetRuntimeState().PlayerGold;
	TestFalse(TEXT("invalid quantity is rejected"),F.OK(TEXT("{\"command\":\"item.give\",\"args\":{\"id\":\"Currency.Gold\",\"quantity\":0}}")));
	TestFalse(TEXT("failed command did not open a test session"),F.Dev->IsSessionActive());
	TestFalse(TEXT("misspelled parameter is rejected"),F.OK(TEXT("{\"command\":\"item.give\",\"args\":{\"id\":\"Currency.Gold\",\"quanity\":123}}")));
	TestTrue(TEXT("give actual currency"),F.OK(TEXT("{\"command\":\"item.give\",\"args\":{\"id\":\"Currency.Gold\",\"quantity\":123}}")));
	TestEqual(TEXT("authoritative gold updated"),F.MVP->GetRuntimeState().PlayerGold,Gold+123);
	TestTrue(TEXT("normal save succeeds within sandbox"),F.MVP->SaveCurrentGame());
	TestEqual(TEXT("sandbox bypasses every player disk-write seam"),F.Writes,0);
	TestTrue(TEXT("restore original session"),F.OK(TEXT("{\"command\":\"session.restore\"}")));
	TestEqual(TEXT("original state restored"),StateText(F.MVP->GetRuntimeState()),Original);
	TestFalse(TEXT("suppression cleared after restore"),F.MVP->AreDevelopmentWritesSuppressed());
	TestTrue(TEXT("set hero level"),F.OK(TEXT("{\"command\":\"character.level\",\"args\":{\"level\":100}}")));
	const FString Before=StateText(F.MVP->GetRuntimeState());
	TestFalse(TEXT("invalid sixth set does not partially grant first five"),F.OK(TEXT("{\"command\":\"equipment.loadout\",\"args\":{\"sets\":[\"XuanJia\",\"XuanJia\",\"XuanJia\",\"XuanJia\",\"XuanJia\",\"NoSuchSet\"]}}")));
	TestEqual(TEXT("failed loadout is atomic"),StateText(F.MVP->GetRuntimeState()),Before);
	auto Result=F.Call(TEXT("{\"command\":\"equipment.loadout\",\"args\":{\"sets\":[\"XuanJia\",\"XuanJia\",\"XuanJia\",\"XuanJia\",\"XuanJia\",\"XuanJia\"],\"level\":100,\"quality\":6,\"enhance\":10,\"gem\":\"balanced\",\"affix\":\"mid\"}}"));
	TestTrue(FString::Printf(TEXT("real six-piece loadout: %s"),*Result->GetStringField(TEXT("message"))),Result->GetBoolField(TEXT("ok")));
	FGameXXKEquipmentLoadoutSnapshot Snapshot;TestTrue(TEXT("inspect actual equipped stats"),F.MVP->GetEquipmentLoadoutSnapshot(TEXT("Player"),Snapshot));
	TestTrue(TEXT("equipment contributes to final attack"),Snapshot.AttributesBeforeRoute.Attack>Snapshot.BareStats.Attack);
	Result=F.Call(TEXT("{\"command\":\"equipment.loadout\",\"args\":{\"character\":\"Npc.TusiChief\",\"character_level\":100,\"sets\":[\"XuanJia\",\"XuanJia\",\"XuanJia\",\"XuanJia\",\"XuanJia\",\"XuanJia\"],\"level\":100,\"quality\":6}}"));
	TestTrue(FString::Printf(TEXT("NPC loadout and level are atomic: %s"),*Result->GetStringField(TEXT("message"))),Result->GetBoolField(TEXT("ok")));
	int32 HeroPieces=0,NpcPieces=0;
	for(const auto& Item:F.MVP->GetRuntimeState().EquipmentCollection.EquipmentInstances) {HeroPieces+=Item.OwnerCharacterId==TEXT("Player");NpcPieces+=Item.OwnerCharacterId==TEXT("Npc.TusiChief");}
	TestEqual(TEXT("hero kept six equipped pieces"),HeroPieces,6);TestEqual(TEXT("NPC owns its own six pieces"),NpcPieces,6);
	TestEqual(TEXT("gear transaction never writes original save"),F.Writes,0);
	F.OK(TEXT("{\"command\":\"session.restore\"}"));return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKDevBattleTest,"GameXXK.Development.AuthoredBattleAndSimulation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKDevBattleTest::RunTest(const FString&)
{
	FDevFixture F;FGameXXKRuntimeState Source=F.MVP->GetRuntimeState();const auto Quest=Source.QuestState;const FString LiveBefore=StateText(Source);
	FString Error;
	if (!TestTrue(FString::Printf(TEXT("authored training boss fixture: %s"),*Error),UGameXXKMVPSubsystem::BuildDevelopmentTrainingBattle(Source,TEXT("Training.Normal.1-1"),6,20260906,Error)))
	{ AddError(Error);return false; }
	TestTrue(TEXT("real full-screen battle state"),Source.CardRun.bHasActiveCardBattle);
	TestEqual(TEXT("story quest unchanged"),Source.QuestState,Quest);
	TestEqual(TEXT("requested encounter selected"),Source.Training.ActiveChallengeEncounterIndex,6);
	const FString Initial=StateText(Source);
	FGameXXKRuntimeState Hell=F.MVP->GetRuntimeState();
	TestTrue(FString::Printf(TEXT("locked higher-difficulty stages can be tested: %s"),*Error),UGameXXKMVPSubsystem::BuildDevelopmentTrainingBattle(Hell,TEXT("Training.Hell.3-1"),6,20260906,Error));
	TestEqual(TEXT("Hell uses actual 150 percent enemy damage"),Hell.CardRun.ActiveBattle.EnemyDifficultyDamagePercent,150);
	FGameXXKSimulationScenario Scenario;Scenario.InitialRuntimeState=Source;Scenario.bResumeActiveBattle=true;Scenario.MaxRounds=30;Scenario.MaxDecisions=1000;
	FGameXXKSimulationMetrics A,B;TArray<FGameXXKSimulationTraceEntry> TA,TB;
	const bool First=FGameXXKCombatSimulationRules::RunScenario(Scenario,A,TA,&Error);
	TestTrue(FString::Printf(TEXT("first real-rule simulation: %s"),*Error),First);
	const bool Second=FGameXXKCombatSimulationRules::RunScenario(Scenario,B,TB,&Error);
	TestEqual(TEXT("same scenario termination"),First,Second);
	FString MA,MB;FJsonObjectConverter::UStructToJsonObjectString(A,MA);FJsonObjectConverter::UStructToJsonObjectString(B,MB);
	TestEqual(TEXT("same seed same metrics"),MA,MB);TestEqual(TEXT("same trace length"),TA.Num(),TB.Num());
	for(int32 I=0;I<FMath::Min(TA.Num(),TB.Num());++I) {FString XA,XB;FJsonObjectConverter::UStructToJsonObjectString(TA[I],XA);FJsonObjectConverter::UStructToJsonObjectString(TB[I],XB);TestEqual(FString::Printf(TEXT("same committed trace entry %d"),I),XA,XB);}
	TestEqual(TEXT("simulation input unchanged"),StateText(Scenario.InitialRuntimeState),Initial);
	TestEqual(TEXT("simulation did not change live game"),StateText(F.MVP->GetRuntimeState()),LiveBefore);
	TestEqual(TEXT("simulation wrote no player saves"),F.Writes,0);return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKDevSnapshotTest,"GameXXK.Development.SnapshotRoundtrip",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKDevSnapshotTest::RunTest(const FString&)
{
	FDevFixture F;const FString Name=TEXT("automation_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
	TestFalse(TEXT("snapshot path traversal rejected"),F.OK(TEXT("{\"command\":\"snapshot.save\",\"args\":{\"name\":\"../player\"}}")));
	const int32 Gold=F.MVP->GetRuntimeState().PlayerGold;
	TestTrue(TEXT("save a named Dev snapshot"),F.OK(FString::Printf(TEXT("{\"command\":\"snapshot.save\",\"args\":{\"name\":\"%s\"}}"),*Name)));
	F.OK(TEXT("{\"command\":\"item.give\",\"args\":{\"id\":\"Currency.Gold\",\"quantity\":77}}"));
	auto R=F.Call(FString::Printf(TEXT("{\"command\":\"snapshot.load\",\"args\":{\"name\":\"%s\"}}"),*Name));
	TestTrue(FString::Printf(TEXT("roundtrip validated snapshot: %s"),*R->GetStringField(TEXT("message"))),R->GetBoolField(TEXT("ok")));
	TestEqual(TEXT("snapshot restored gold"),F.MVP->GetRuntimeState().PlayerGold,Gold);
	TestEqual(TEXT("snapshot used no player-slot writes"),F.Writes,0);
	IFileManager::Get().Delete(*(F.Dev->GetStorageDirectory()/TEXT("snapshots")/(Name+TEXT(".json"))),false,true);
	F.OK(TEXT("{\"command\":\"session.restore\"}"));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKDevRecommendedTest,"GameXXK.Development.RecommendedAll",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKDevRecommendedTest::RunTest(const FString&)
{
	FDevFixture F;const FString Original=StateText(F.MVP->GetRuntimeState());
	auto R=F.Call(TEXT("{\"command\":\"equipment.recommend_all\"}"));
	if(!TestTrue(R->GetStringField(TEXT("message")),R->GetBoolField(TEXT("ok"))))return false;
	const auto& S=F.MVP->GetRuntimeState();
	TestEqual(TEXT("hero level"),S.PlayerLevel,100);TestEqual(TEXT("hero refilled"),S.PlayerHP,S.PlayerMaxHP);
	TArray<TPair<FName,EGameXXKEquipmentSet>> Owners;Owners.Add({TEXT("Player"),EGameXXKEquipmentSet::PoJun});
	for(const auto& C:S.CardRun.CompanionRoster.PermanentCompanions){TestEqual(TEXT("partner level"),C.Level,100);Owners.Add({C.InstanceId,GameXXKDevFixtures::RecommendedSet(C.Role)});}
	for(const auto& N:FGameXXKCompanionCatalog::GetQuestNpcDefinitions())Owners.Add({N.NpcId,GameXXKDevFixtures::NpcRecommendedSet(N.NpcId)});
	for(const auto& Owner:Owners)
	{
		int32 Pieces=0;TMap<EGameXXKGemType,int32> Gems;
		for(const auto& E:S.EquipmentCollection.EquipmentInstances)if(E.OwnerCharacterId==Owner.Key)
		{
			++Pieces;const auto* D=FGameXXKEquipmentCatalog::FindDefinition(E.BaseEquipmentId);
			TestTrue(TEXT("profession set mapping"),D&&D->Set==Owner.Value);TestEqual(TEXT("equipment level"),E.ItemLevel,100);TestEqual(TEXT("enhancement"),E.EnhancementLevel,10);
			for(const auto& G:E.SocketedGems){++Gems.FindOrAdd(G.Type);TestEqual(TEXT("gem quality"),G.Quality,EGameXXKGemQuality::Treasure);}
		}
		TestEqual(Owner.Key.ToString()+TEXT(" has six pieces"),Pieces,6);
		for(auto Type:{EGameXXKGemType::Attack,EGameXXKGemType::Defense,EGameXXKGemType::MaxHealth})TestEqual(TEXT("balanced four gems"),Gems.FindRef(Type),4);
	}
	int32 Spare=0;
	for(const auto& E:S.EquipmentCollection.EquipmentInstances)
	{
		const auto* D=FGameXXKEquipmentCatalog::FindDefinition(E.BaseEquipmentId);
		if(D&&D->Set==EGameXXKEquipmentSet::ShiGu&&E.OwnerKind==EGameXXKEquipmentOwnerKind::Warehouse)
		{++Spare;TestTrue(TEXT("ShiGu in physical backpack"),FGameXXKDesktopInventoryRules::FindEntrySlot(S,EGameXXKDesktopItemContainer::Backpack,FGameXXKDesktopInventoryRules::MakeEquipmentEntry(E.InstanceId))!=INDEX_NONE);}
	}
	TestEqual(TEXT("six spare ShiGu pieces"),Spare,6);const int32 Count=S.EquipmentCollection.EquipmentInstances.Num();
	TestTrue(TEXT("repeat recommendation succeeds"),F.OK(TEXT("{\"command\":\"equipment.recommend_all\"}")));
	TestEqual(TEXT("repeat does not generate duplicate sets"),F.MVP->GetRuntimeState().EquipmentCollection.EquipmentInstances.Num(),Count);
	const FString BeforeInvalid=StateText(F.MVP->GetRuntimeState());
	TestFalse(TEXT("invalid set rejected"),F.OK(TEXT("{\"command\":\"equipment.recommend_all\",\"args\":{\"hero_set\":\"Legacy\"}}")));
	TestEqual(TEXT("invalid recommendation is atomic"),StateText(F.MVP->GetRuntimeState()),BeforeInvalid);
	TestEqual(TEXT("no player save writes"),F.Writes,0);F.OK(TEXT("{\"command\":\"session.restore\"}"));
	TestEqual(TEXT("original restored"),StateText(F.MVP->GetRuntimeState()),Original);return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKDevTelemetryTest,"GameXXK.Development.BenchmarkTelemetry",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKDevTelemetryTest::RunTest(const FString&)
{
	FDevFixture F;FString Error;
	for(auto Role:{EGameXXKCharacterRole::Blade,EGameXXKCharacterRole::Guard,EGameXXKCharacterRole::Healer,EGameXXKCharacterRole::Hunter,EGameXXKCharacterRole::Sorcerer,EGameXXKCharacterRole::FormationMaster})
	{
		FGameXXKRuntimeState S;
		if(!GameXXKDevFixtures::BuildBenchmark(Role,TEXT("Npc.TusiChief"),TEXT("Blade"),3,S,Error)){AddError(Error);return false;}
		TestEqual(TEXT("legal hero deck"),S.CardRun.HeroSelectedCardIds.Num(),8);
		if(!UGameXXKMVPSubsystem::BuildDevelopmentTrainingBattle(S,TEXT("Training.Hell.3-1"),6,20260906,Error)){AddError(Error);return false;}
		const FString Input=StateText(S);FGameXXKSimulationScenario Scenario;Scenario.InitialRuntimeState=S;Scenario.bResumeActiveBattle=true;Scenario.MaxRounds=60;Scenario.Terrain=S.CardRun.ActiveBattle.Terrain;
		FGameXXKSimulationMetrics M;TArray<FGameXXKSimulationTraceEntry> Trace;
		if(!FGameXXKCombatSimulationRules::RunScenario(Scenario,M,Trace,&Error)&&M.FailureReason!=TEXT("Simulation.MaxRounds")){AddError(Error);return false;}
		int64 Packets=0,Taken=0,Ledger=0,Healing=0,Armor=0;
		for(const auto& T:Trace)
		{
			Ledger+=T.EffectiveDamage;Healing+=T.EffectiveHealing;Armor+=T.GeneratedArmor;
			for(const auto& D:T.DamagePackets)
			{
				const auto* Target=T.UnitsBefore.FindByPredicate([&D](const auto& U){return U.UnitId==D.ResolvedTargetUnitId;});
				if(Target&&Target->Side==EGameXXKCardTargetSide::Enemy)Packets+=D.HealthDamage;else Taken+=D.HealthDamage;
			}
		}
		TestEqual(TEXT("damage ledger matches exported outgoing packets"),M.DamageLedgerDifference,int64(0));
		TestEqual(TEXT("outgoing damage excludes enemy attacks"),M.DamageDealt,Packets);TestEqual(TEXT("incoming separately audited"),M.DamageTaken,Taken);
		TestEqual(TEXT("trace sums to effective damage"),M.DamageDealt,Ledger);TestEqual(TEXT("trace sums to healing"),M.HealingGenerated,Healing);TestEqual(TEXT("trace sums to armor"),M.ArmorGenerated,Armor);
		TestEqual(TEXT("input unchanged"),StateText(S),Input);
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKDevBalanceExportTest,"GameXXK.Development.ExportBalanceManifest",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKDevBalanceExportTest::RunTest(const FString&)
{
	FString ManifestPath;if(!FParse::Value(FCommandLine::Get(),TEXT("GameXXKBalanceManifest="),ManifestPath))return true;
	FString Input;TSharedPtr<FJsonObject> Manifest;
	if(!FFileHelper::LoadFileToString(Input,*ManifestPath)||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Input),Manifest))return false;
	const FString Directory=Manifest->GetStringField(TEXT("directory"));IFileManager::Get().MakeDirectory(*Directory,true);
	FDevFixture F;const FString Live=StateText(F.MVP->GetRuntimeState());int32 Completed=0,Errors=0;
	auto Encode=[](const TSharedPtr<FJsonObject>& O){FString Text;FJsonSerializer::Serialize(O.ToSharedRef(),TJsonWriterFactory<>::Create(&Text));return Text;};
	auto Save=[&](const TSharedPtr<FJsonObject>& O,const FString& Path){return FFileHelper::SaveStringToFile(Encode(O),*Path,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);};
	for(const auto& V:Manifest->GetArrayField(TEXT("cases")))
	{
		const auto Case=V->AsObject();const FString Id=Case->GetStringField(TEXT("id"));
		if(Id.Contains(TEXT("/"))||Id.Contains(TEXT("\\"))||Id.Contains(TEXT("..")))return false;
		const FString CaseDir=Directory/Id;IFileManager::Get().MakeDirectory(*CaseDir,true);
		auto Request=MakeShared<FJsonObject>();Request->SetStringField(TEXT("command"),TEXT("benchmark.prepare"));Request->SetObjectField(TEXT("args"),Case->GetObjectField(TEXT("fixture")));
		auto Prepared=F.Call(Encode(Request));
		if(!Prepared->GetBoolField(TEXT("ok"))){Save(Prepared,CaseDir/TEXT("prepare-error.json"));AddError(Prepared->GetStringField(TEXT("message")));return false;}
		auto Scene=Prepared->GetObjectField(TEXT("data"));
		if(Completed==0){auto Catalog=MakeShared<FJsonObject>();Catalog->SetArrayField(TEXT("cards"),Scene->GetArrayField(TEXT("card_catalog")));if(!Save(Catalog,Directory/TEXT("catalog.json")))return false;}
		Scene->RemoveField(TEXT("card_catalog"));if(!Save(Scene,CaseDir/TEXT("source.json")))return false;
		for(const auto& Seed:Case->GetArrayField(TEXT("seeds")))
		{
			auto Args=MakeShared<FJsonObject>();Args->SetObjectField(TEXT("scene"),Scene);Args->SetNumberField(TEXT("seed"),Seed->AsNumber());Args->SetStringField(TEXT("stage"),Case->GetStringField(TEXT("stage")));Args->SetNumberField(TEXT("encounter"),Case->GetNumberField(TEXT("encounter")));Args->SetNumberField(TEXT("max_rounds"),60);
			Request->SetStringField(TEXT("command"),TEXT("simulate.run"));Request->SetObjectField(TEXT("args"),Args);
			auto Response=F.Call(Encode(Request));const FString Output=CaseDir/FString::Printf(TEXT("seed-%d.json"),static_cast<int32>(Seed->AsNumber()));
			if(!Save(Response,Output))return false;
			++Completed;if(!Response->GetBoolField(TEXT("ok"))||!Response->GetObjectField(TEXT("data"))->GetBoolField(TEXT("ok")))++Errors;
		}
		auto Progress=MakeShared<FJsonObject>();Progress->SetNumberField(TEXT("completed"),Completed);Progress->SetNumberField(TEXT("errors"),Errors);Progress->SetStringField(TEXT("last_case"),Id);Save(Progress,Directory/TEXT("progress.json"));
		UE_LOG(LogTemp,Display,TEXT("Dev balance: %s, %d completed, %d errors"),*Id,Completed,Errors);
	}
	TestEqual(TEXT("matrix never mutates live state"),StateText(F.MVP->GetRuntimeState()),Live);TestEqual(TEXT("matrix never writes player saves"),F.Writes,0);
	if(Errors)AddWarning(FString::Printf(TEXT("%d simulation errors are recorded separately in the report."),Errors));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKDevResumedPhaseTest,"GameXXK.Development.ResumedPhaseRegression",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKDevResumedPhaseTest::RunTest(const FString&)
{
	struct FCase{EGameXXKCharacterRole Role;FName Npc;int32 Omit;int32 Seed;};
	for(const auto& C:TArray<FCase>{{EGameXXKCharacterRole::Guard,TEXT("Npc.ZhouGuangZu"),3,20260908},{EGameXXKCharacterRole::Healer,TEXT("Npc.JinGui"),0,20260909},{EGameXXKCharacterRole::Healer,TEXT("Npc.TusiChief"),3,20260908}})
	{
		FGameXXKRuntimeState S;FString Error;
		if(!GameXXKDevFixtures::BuildBenchmark(C.Role,C.Npc,TEXT("Mage"),C.Omit,S,Error)||!UGameXXKMVPSubsystem::BuildDevelopmentTrainingBattle(S,TEXT("Training.Hell.3-1"),6,C.Seed,Error)){AddError(Error);return false;}
		FGameXXKSimulationScenario Scenario;Scenario.InitialRuntimeState=S;Scenario.bResumeActiveBattle=true;Scenario.Seed=C.Seed;Scenario.MaxRounds=60;Scenario.Terrain=S.CardRun.ActiveBattle.Terrain;
		FGameXXKSimulationMetrics Metrics;TArray<FGameXXKSimulationTraceEntry> Trace;
		const bool OK=FGameXXKCombatSimulationRules::RunScenario(Scenario,Metrics,Trace,&Error);
		TestTrue(Error,OK||Metrics.FailureReason==TEXT("Simulation.MaxRounds"));
		TestTrue(TEXT("regression covers automatic continuation through forced discard"),Trace.ContainsByPredicate([](const auto& T){return T.Action==TEXT("ForcedDiscard");}));
		TestEqual(TEXT("resumed packets reconcile to the settlement ledger"),Metrics.DamageLedgerDifference,int64(0));
	}
	return true;
}
#endif
