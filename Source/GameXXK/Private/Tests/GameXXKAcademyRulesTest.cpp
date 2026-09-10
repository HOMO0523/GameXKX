#include "Guide/GameXXKAcademyRules.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "GameXXKCardCatalog.h"
#include "GameXXKCardBattleAdapter.h"
#include "GameXXKMetaShopRules.h"
#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "GameXXKEquipmentRules.h"
#include "UI/GameXXKLocalization.h"
#include "Misc/ScopeExit.h"
#include "../MVP/GameXXKAcademyStateBuilder.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKAcademyBilingualTest,"GameXXK.Academy.ConciseBilingualMechanisms",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKAcademyBilingualTest::RunTest(const FString&)
{
    const FString Original=GameXXKLocalization::GetLanguage();ON_SCOPE_EXIT{GameXXKLocalization::SetLanguage(Original,false);};
    int32 Lessons=0;
    for(const auto& Course:FGameXXKAcademyRules::Courses())
    {
        for(const TCHAR* Language:{TEXT("en"),TEXT("zh-Hans")})
        {
            GameXXKLocalization::SetLanguage(Language,false);
            TArray<FText> Copy={Course.Title,Course.Summary};
            for(const auto& Lesson:Course.Lessons)
            {
                Copy.Add(Lesson.Title);Copy.Add(Lesson.Instruction);
                TestTrue(Course.Id.ToString()+TEXT(" explanation is concise"),GameXXKLocalization::Localize(Lesson.Instruction).ToString().Len()<=100);
                for(const auto& Goal:Lesson.Goals)Copy.Add(Goal.Text);
            }
            for(const auto& Item:Copy)
            {
                const FString Text=GameXXKLocalization::Localize(Item).ToString();
                TestFalse(TEXT("all teaching copy exists"),Text.IsEmpty()||Text.StartsWith(TEXT("Academy.")));
                if(GameXXKLocalization::IsEnglish())for(TCHAR Character:Text)if(Character>=0x3400&&Character<=0x9fff){AddError(Course.Id.ToString()+TEXT(" untranslated: ")+Text);break;}
            }
        }
        Lessons+=Course.Lessons.Num();
    }
    TestEqual(TEXT("all hero, six partner and six NPC lessons covered"),Lessons,29);return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKAcademyGuidedObjectivesTest,"GameXXK.Academy.GuidedObjectives",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKAcademyGuidedObjectivesTest::RunTest(const FString&)
{
    for(const auto& Course:FGameXXKAcademyRules::Courses())for(int32 Index=0;Index<Course.Lessons.Num();++Index)
    {
        FGameXXKRuntimeState State;FName Focus;FString Error;FGameXXKAcademyEvidence Evidence;
        if(!UGameXXKMVPSubsystem::BuildAcademyBattleState(Course,Index,State,Focus,Error)){AddError(Error);continue;}
        for(const auto& Unit:State.CardRun.ActiveBattle.Units)if(Unit.Side==EGameXXKCardTargetSide::Enemy)
        {
            TestTrue(TEXT("every tutorial enemy has at most 300 HP"),Unit.MaxHP>0&&Unit.MaxHP<=300);
            TestTrue(TEXT("tutorial attacks are mild"),Unit.Attack<=12);
        }
        else TestEqual(TEXT("every borrowed hero, partner and NPC has 300 max HP"),Unit.MaxHP,300);
        for(int32 Step=0;Step<400;++Step)
        {
            const auto Before=State.CardRun.ActiveBattle;
            if(Before.Phase==EGameXXKCardBattlePhase::Victory||Before.Phase==EGameXXKCardBattlePhase::Defeat)break;
            TArray<FGameXXKCardDamageResult> Damage;TArray<FGameXXKCardPlayResult> Resumed;FName Played;
            const auto& Pending=Before.Deck.PendingChoice;
            if(Pending.Kind==EGameXXKCardPendingChoiceKind::ForcedDiscard)
            {
                TArray<FName> Ids;for(const auto& Card:Pending.Candidates)if(Ids.Num()<Pending.RequiredCount)Ids.Add(Card.InstanceId);
                FGameXXKCardBattleAdapter::SubmitForcedDiscard(State,Ids,&Error,&Resumed);
            }
            else if(Pending.Kind==EGameXXKCardPendingChoiceKind::InsightChooseToHand)FGameXXKCardBattleAdapter::CancelInsight(State,&Error,&Resumed);
            else if(Pending.Kind!=EGameXXKCardPendingChoiceKind::Invalid&&!Pending.Candidates.IsEmpty())FGameXXKCardBattleAdapter::SubmitHeroTaskSearchChoice(State,Pending.Candidates[0].InstanceId,Resumed,&Error);
            else if(Before.Phase==EGameXXKCardBattlePhase::Enemy)FGameXXKCardBattleAdapter::ResolveEnemyPhase(State,Damage,&Error);
            else
            {
                    FName Target;bool EndTurn=false;
                    FGameXXKAcademyRules::Recommend(State,Focus,Course.Lessons[Index],Evidence,NAME_None,Played,Target,EndTurn);
                    if(EndTurn)FGameXXKCardBattleAdapter::EndPlayerCardPhase(State,Damage,&Error);
                    else
                    {
                        FGameXXKCardPlayResult Result;
                        if(!FGameXXKCardBattleAdapter::ResolveCardPlay(State,Played,Target,Result,&Error)){AddError(TEXT("Visible tutorial recommends an illegal play: ")+Error);break;}
                        FGameXXKAcademyRules::ObserveCommittedResult(Result,Focus,Evidence);Damage=MoveTemp(Result.DamageResults);
                    }
            }
            for(const auto& Result:Resumed){FGameXXKAcademyRules::ObserveCommittedResult(Result,Focus,Evidence);Damage.Append(Result.DamageResults);}
            FGameXXKAcademyRules::Observe(Before,State.CardRun.ActiveBattle,Damage,Played,Focus,Evidence);
        }
        FString Missing;for(const auto& Goal:Course.Lessons[Index].Goals)if(Evidence.Counts.FindRef(Goal.Kind)<Goal.Required)Missing+=Goal.Text.ToString()+TEXT("; ");
        TestTrue(FString::Printf(TEXT("visible guidance %s/%d: missing=%s, phase=%d, error=%s"),*Course.Id.ToString(),Index,*Missing,int32(State.CardRun.ActiveBattle.Phase),*Error),
            Evidence.Satisfies(Course.Lessons[Index])&&State.CardRun.ActiveBattle.Phase==EGameXXKCardBattlePhase::Victory);
        if(Course.Role==EGameXXKCharacterRole::Hero)
        {
            TestTrue(FString::Printf(TEXT("hero lesson %d finishes within three rounds (actual %d)"),Index+1,State.CardRun.ActiveBattle.RoundNumber),State.CardRun.ActiveBattle.RoundNumber<=3);
            UE_LOG(LogTemp,Display,TEXT("[AcademyPacing] Hero lesson=%d rounds=%d victory=%d goals=%d"),Index+1,
                State.CardRun.ActiveBattle.RoundNumber,State.CardRun.ActiveBattle.Phase==EGameXXKCardBattlePhase::Victory,Evidence.Satisfies(Course.Lessons[Index]));
        }
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKAcademyCatalogTest,"GameXXK.Academy.CatalogAndLoadouts",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKAcademyCatalogTest::RunTest(const FString& Parameters)
{
	const auto& Courses=FGameXXKAcademyRules::Courses();TestEqual(TEXT("13 independent courses"),Courses.Num(),13);
	TSet<FName> Ids;
	for(const auto& C:Courses)
	{
		TestFalse(TEXT("unique course"),Ids.Contains(C.Id));Ids.Add(C.Id);
		for(int32 I=0;I<C.Lessons.Num();++I)
		{
			for(FName Card:C.Lessons[I].Cards)TestNotNull(*Card.ToString(),FGameXXKCardCatalog::FindCardDefinition(Card));
			FGameXXKRuntimeState State;FName Focus;FString Error;
			TestTrue(*FString::Printf(TEXT("%s lesson %d builds: %s"),*C.Id.ToString(),I,*Error),UGameXXKMVPSubsystem::BuildAcademyBattleState(C,I,State,Focus,Error));
			if(!Error.IsEmpty())AddError(C.Id.ToString()+TEXT(": ")+Error);
            TestTrue(TEXT("course battle owns its map identity"),State.CurrentMapId==TEXT("AcademyBattle"));
            TestFalse(TEXT("course never starts a Training challenge"),State.Training.bChallengeActive);
            TestTrue(TEXT("course has no Training source stage"),State.Training.ActiveChallengeStageId.IsNone());
            TestFalse(TEXT("course never imports difficulty-scaled stage attributes"),State.CardRun.ActiveBattle.bEnemyAttributesIncludeDifficulty);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKAcademyIndependentEncounterTest,"GameXXK.Academy.IndependentEncounter",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKAcademyIndependentEncounterTest::RunTest(const FString&)
{
    const auto& Course=FGameXXKAcademyRules::Courses()[0];FGameXXKRuntimeState Base;FName Focus;FString Error;
    if(!GameXXKAcademyStateBuilder::BuildLoadout(Course,0,Base,Focus,Error))return false;
    auto A=Base;auto B=Base;
    B.Training.bDevelopmentUnlockAllStages=true;B.Training.bChallengeActive=true;
    B.Training.SelectedStageId=TEXT("Training.Hell.3-4");B.Training.ActiveChallengeStageId=TEXT("Missing.Stage");
    B.Training.bTravelActive=true;B.Training.CurrentTravelStageId=TEXT("Missing.Travel");
    TestTrue(TEXT("ordinary course encounter builds"),GameXXKAcademyStateBuilder::BuildEncounter(Course,0,A,Error));
    TestTrue(TEXT("unrelated invalid Training state cannot determine a course encounter"),GameXXKAcademyStateBuilder::BuildEncounter(Course,0,B,Error));
    const auto& Left=A.CardRun.ActiveBattle;const auto& Right=B.CardRun.ActiveBattle;
    TestEqual(TEXT("same lesson creates the same unit count"),Left.Units.Num(),Right.Units.Num());
    TestEqual(TEXT("lesson difficulty does not come from Hell"),Right.EnemyDifficulty,EGameXXKEnemyDifficulty::Normal);
    for(int32 Index=0;Index<FMath::Min(Left.Units.Num(),Right.Units.Num());++Index)
    {
        TestEqual(TEXT("same course unit identity"),Left.Units[Index].UnitId,Right.Units[Index].UnitId);
        TestEqual(TEXT("same course HP"),Left.Units[Index].HP,Right.Units[Index].HP);
        TestEqual(TEXT("same course ATK"),Left.Units[Index].Attack,Right.Units[Index].Attack);
    }
    TestTrue(TEXT("no invalid stage identifier is retained"),B.Training.ActiveChallengeStageId.IsNone());
    TestFalse(TEXT("course does not start idle travel"),B.Training.bTravelActive);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKAcademyRewardTest,"GameXXK.Academy.FirstClearReward",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKAcademyRewardTest::RunTest(const FString& Parameters)
{
	FGameXXKGuideProgress Progress;int32 Gold=0,Award=0;FString Error;
	for(const auto& C:FGameXXKAcademyRules::Courses())for(int32 I=0;I<C.Lessons.Num();++I)
	{
		FGameXXKAcademyEvidence Evidence;
		TestFalse(TEXT("victory without objectives rejected"),FGameXXKAcademyRules::CompleteLesson(C.Id,I,true,Evidence,Progress,Gold,Award,Error));
		for(const auto& G:C.Lessons[I].Goals)Evidence.Record(G.Kind,G.Required);
		TestFalse(TEXT("loss cannot complete course"),FGameXXKAcademyRules::CompleteLesson(C.Id,I,false,Evidence,Progress,Gold,Award,Error));
		TestTrue(TEXT("eligible completion"),FGameXXKAcademyRules::CompleteLesson(C.Id,I,true,Evidence,Progress,Gold,Award,Error));
		TestEqual(TEXT("only final lesson awards"),Award,I+1==C.Lessons.Num()?100000:0);
		const int32 Before=Gold;
		TestTrue(TEXT("repeated completion harmless"),FGameXXKAcademyRules::CompleteLesson(C.Id,I,true,Evidence,Progress,Gold,Award,Error));
		TestEqual(TEXT("no duplicate reward"),Gold,Before);
	}
	TestEqual(TEXT("all courses total"),Gold,1300000);
    FGameXXKGuideProgress Legacy;Legacy.AcademyCompletedLessons.Add(TEXT("Academy.Basic"),2);Legacy.AcademyRewardedCourses.Add(TEXT("Academy.Basic"));
    FGameXXKAcademyEvidence Extra;const auto* Hero=FGameXXKAcademyRules::Find(TEXT("Academy.Basic"));
    for(const auto& Goal:Hero->Lessons[2].Goals)Extra.Record(Goal.Kind,Goal.Required);
    const int32 PreviousGold=Gold;
    TestTrue(TEXT("old rewarded players can finish the new Hero lesson"),FGameXXKAcademyRules::CompleteLesson(TEXT("Academy.Basic"),2,true,Extra,Legacy,Gold,Award,Error));
    TestEqual(TEXT("new lesson cannot duplicate the old course reward"),Gold,PreviousGold);
    TestEqual(TEXT("old progress advances without reset"),Legacy.AcademyCompletedLessons.FindRef(TEXT("Academy.Basic")),3);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKDesktopShopCatalogTest,"GameXXK.MetaShop.DesktopCatalog",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKDesktopShopCatalogTest::RunTest(const FString& Parameters)
{
	const auto& P=FGameXXKMetaShopRules::GetDesktopProducts();TestEqual(TEXT("ten products including travel money"),P.Num(),10);
	for(const auto& Item:P)TestEqual(TEXT("approved price"),Item.Price,Item.Kind==EGameXXKMetaShopProductKind::EquipmentPack?100000:Item.ProductId==EGameXXKMetaShopProductId::NormalChest?25000:100000);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKAcademyPlayableObjectivesTest,"GameXXK.Academy.PlayableObjectives",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKAcademyPlayableObjectivesTest::RunTest(const FString& Parameters)
{
	for(const auto& Course:FGameXXKAcademyRules::Courses())for(int32 L=0;L<Course.Lessons.Num();++L)
	{
		FGameXXKRuntimeState State;FName Focus;FString Error;FGameXXKAcademyEvidence Evidence;
		if(!UGameXXKMVPSubsystem::BuildAcademyBattleState(Course,L,State,Focus,Error)){AddError(Error);continue;}
		for(int32 Step=0;Step<400;++Step)
		{
			const auto Before=State.CardRun.ActiveBattle;
			if(Before.Phase==EGameXXKCardBattlePhase::Victory || Before.Phase==EGameXXKCardBattlePhase::Defeat)break;
			TArray<FGameXXKCardDamageResult> Damage;FName Played;
			const auto Pending=Before.Deck.PendingChoice;TArray<FGameXXKCardPlayResult> Resumed;
			if(Pending.Kind==EGameXXKCardPendingChoiceKind::ForcedDiscard)
			{
				TArray<FName> Ids;for(const auto& Card:Pending.Candidates)if(Ids.Num()<Pending.RequiredCount)Ids.Add(Card.InstanceId);
				FGameXXKCardBattleAdapter::SubmitForcedDiscard(State,Ids,&Error,&Resumed);
			}
			else if(Pending.Kind==EGameXXKCardPendingChoiceKind::InsightChooseToHand)
				FGameXXKCardBattleAdapter::CancelInsight(State,&Error,&Resumed);
			else if(Pending.Kind!=EGameXXKCardPendingChoiceKind::Invalid && !Pending.Candidates.IsEmpty())
				FGameXXKCardBattleAdapter::SubmitHeroTaskSearchChoice(State,Pending.Candidates[0].InstanceId,Resumed,&Error);
			else if(Before.Phase==EGameXXKCardBattlePhase::Enemy)
				FGameXXKCardBattleAdapter::ResolveEnemyPhase(State,Damage,&Error);
			else
			{
				auto Hand=Before.Deck.Hand;
				Hand.StableSort([&](const auto& A,const auto& B){const int32 PA=(A.OwnerUnitId==Focus?100:0)+(Evidence.ActiveCardIds.Contains(A.CardId)?0:10);const int32 PB=(B.OwnerUnitId==Focus?100:0)+(Evidence.ActiveCardIds.Contains(B.CardId)?0:10);return PA>PB;});
				for(const auto& Card:Hand)
				{
					if(Card.OwnerUnitId!=Focus && !Evidence.Satisfies(Course.Lessons[L]))continue;
					FGameXXKCardPlayPreview Preview;if(!FGameXXKCardBattleAdapter::BuildCardPlayPreview(State,Card.InstanceId,Preview,&Error)||!Preview.bCanPlay)continue;
					TArray<FName> Targets={NAME_None};for(const auto& Unit:Before.Units)if(Unit.bLiving)Targets.Add(Unit.UnitId);
					for(FName Target:Targets){FGameXXKCardPlayResult Result;if(FGameXXKCardBattleAdapter::ResolveCardPlay(State,Card.InstanceId,Target,Result,&Error)){FGameXXKAcademyRules::ObserveCommittedResult(Result,Focus,Evidence);Played=Card.InstanceId;Damage=MoveTemp(Result.DamageResults);break;}}
					if(!Played.IsNone())break;
				}
				if(Played.IsNone())FGameXXKCardBattleAdapter::EndPlayerCardPhase(State,Damage,&Error);
			}
			for(const auto& R:Resumed){FGameXXKAcademyRules::ObserveCommittedResult(R,Focus,Evidence);Damage.Append(R.DamageResults);}
			FGameXXKAcademyRules::Observe(Before,State.CardRun.ActiveBattle,Damage,Played,Focus,Evidence);
		}
		FString Missing;for(const auto& Goal:Course.Lessons[L].Goals)if(Evidence.Counts.FindRef(Goal.Kind)<Goal.Required)Missing+=Goal.Text.ToString()+TEXT("; ");
		const bool bWon=State.CardRun.ActiveBattle.Phase==EGameXXKCardBattlePhase::Victory;
		TestTrue(*FString::Printf(TEXT("%s/%d won=%d missing=%s error=%s"),*Course.Id.ToString(),L,bWon,*Missing,*Error),bWon && Missing.IsEmpty());
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKShopBatchTest,"GameXXK.MetaShop.BatchAtomicity",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKShopBatchTest::RunTest(const FString& Parameters)
{
	auto* MVP=NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());if(!MVP->StartGame())return false;
	auto Base=MVP->GetRuntimeState();Base.Screen=EGameXXKScreen::Town;Base.PlayerGold=3000000;
	for(const auto& Product:FGameXXKMetaShopRules::GetDesktopProducts())for(int32 Quantity:{1,10})
	{
		auto State=Base;TArray<FGameXXKMetaShopPurchaseResult> Results;FText Message;
		TestTrue(*Product.DisplayName.ToString(),FGameXXKMetaShopRules::PurchaseBatch(State,Product.ProductId,Quantity,Results,Message));
		TestEqual(TEXT("one result per purchased item"),Results.Num(),Quantity);
		TestEqual(TEXT("exact batch cost"),State.PlayerGold,Base.PlayerGold-Product.Price*Quantity);
		if(Product.Kind==EGameXXKMetaShopProductKind::EquipmentPack)TestEqual(TEXT("exact equipment count"),State.EquipmentCollection.WarehouseInstanceIds.Num()-Base.EquipmentCollection.WarehouseInstanceIds.Num(),Quantity);
		if(Product.Kind==EGameXXKMetaShopProductKind::TrainingChest)TestEqual(TEXT("exact chest count"),State.Training.OwnedChestTokens.Num()-Base.Training.OwnedChestTokens.Num(),Quantity);
		if(Product.Kind==EGameXXKMetaShopProductKind::GemPack){int32 Delta=0;for(const auto& Pair:State.Inventory)Delta+=Pair.Value-Base.Inventory.FindRef(Pair.Key);TestEqual(TEXT("one gem per pack"),Delta,Quantity);}
	}
	for(int32 Quantity:{0,11,3})
	{
		auto State=Base;State.PlayerGold=200001;const auto Before=State;TArray<FGameXXKMetaShopPurchaseResult> Results;FText Message;
		TestFalse(TEXT("invalid or unaffordable batch rejected"),FGameXXKMetaShopRules::PurchaseBatch(State,EGameXXKMetaShopProductId::PoJunPack,Quantity,Results,Message));
		TestTrue(TEXT("no partial grant or debit"),FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(&State,&Before,PPF_None));
		TestTrue(TEXT("no success results on failure"),Results.IsEmpty());
	}
	MVP->GetMutableRuntimeState()=Base;
	MVP->SetSaveSlotWriteDelegateForTest(FGameXXKSaveSlotWriteDelegate::CreateLambda([](USaveGame*,const FString&,int32){return false;}));
	TArray<FGameXXKMetaShopPurchaseResult> Results;FText Message;
	TestFalse(TEXT("save failure rejects batch"),MVP->PurchaseMetaShopProducts(EGameXXKMetaShopProductId::PoJunPack,10,Results,Message));
	TestTrue(TEXT("save failure leaves real state unchanged"),FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(&MVP->GetRuntimeState(),&Base,PPF_None));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKShopToolLevelTest,"GameXXK.MetaShop.ToolLevel",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKShopToolLevelTest::RunTest(const FString& Parameters)
{
	auto* MVP=NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());if(!MVP->StartGame())return false;
	for(bool Unlocked:{false,true})
	{
		auto State=MVP->GetRuntimeState();State.Screen=EGameXXKScreen::Town;State.PlayerGold=1000000;
		State.ToolProgress.Level=7;State.ToolProgress.SelectedCraftingLevel=1;
		if(Unlocked){State.Talents.NodeRanks.Add(TEXT("Talent.Root"),1);State.Talents.NodeRanks.Add(TEXT("Talent.Entry.Tools"),1);}
		TArray<FGameXXKMetaShopPurchaseResult> Results;FText Message;
		TestTrue(TEXT("tool-level purchase succeeds"),FGameXXKMetaShopRules::PurchaseBatch(State,EGameXXKMetaShopProductId::PoJunPack,2,Results,Message));
		for(const auto& Result:Results)if(const auto* Item=FGameXXKEquipmentRules::FindInstance(State.EquipmentCollection,Result.GeneratedEquipmentId))TestEqual(TEXT("equipment follows tool level with or without the bonus talent branch"),Item->ItemLevel,7);
	}
	return true;
}
#endif
