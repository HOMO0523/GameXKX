#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Engine/GameInstance.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameXXKCardCatalog.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKCardNameStyle.h"
#include "UI/GameXXKInventoryWindowWidget.h"
#include "UI/GameXXKInRunUiStyle.h"
#include "UI/GameXXKLocalization.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKAllShortCardNamesTest,
    "GameXXK.Localization.Deck.AllNamesFitAndUseAtMostTwoWords",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKAllShortCardNamesTest::RunTest(const FString&)
{
    const FString Previous=GameXXKLocalization::GetLanguage();
    ON_SCOPE_EXIT{GameXXKLocalization::SetLanguage(Previous,false);};
    GameXXKLocalization::SetLanguage(TEXT("en"),false);
    const auto Measure=FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
    const auto Font=FGameXXKInRunUiStyle::TitleFont(30,true);
    FString Report(TEXT("CardId\tEnglish\tWords\tWidthAt30\n"));
    TSet<FString> Names;int32 Count=0;
    for(const auto& Card:FGameXXKCardCatalog::GetAllCardDefinitions())
    {
        const FText Text=GameXXKLocalization::Localize(Card.DisplayName);
        const FString Name=Text.ToString();TArray<FString> Words;Name.ParseIntoArrayWS(Words);
        const float Width=Measure->Measure(Text,Font).X;
        TestTrue(*FString::Printf(TEXT("%s uses one or two words"),*Card.Id.ToString()),Words.Num()>=1&&Words.Num()<=2);
        TestTrue(*FString::Printf(TEXT("%s fits the 190-unit small card title including a 6-unit quality outline allowance (%.2f)"),*Card.Id.ToString(),Width),Width<=184);
        TestFalse(*FString::Printf(TEXT("%s remains distinct"),*Card.Id.ToString()),Names.Contains(Name));
        Names.Add(Name);++Count;
        Report+=FString::Printf(TEXT("%s\t%s\t%d\t%.3f\n"),*Card.Id.ToString(),*Name,Words.Num(),Width);
    }
    TestEqual(TEXT("Includes every Hero, Companion, NPC and Boss reward card"),Count,173);
    TestTrue(TEXT("Write the measured card-name report"),FFileHelper::SaveStringToFile(Report,*FPaths::Combine(FPaths::ProjectDir(),TEXT("Saved/Codex/UIGuidanceLocalization-20260910/all-card-name-widths.tsv"))));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKDeckQualityReflowTest,
    "GameXXK.Localization.Deck.QualityMaterialsSurviveReflow",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKDeckQualityReflowTest::RunTest(const FString&)
{
    const FString Previous=GameXXKLocalization::GetLanguage();
    ON_SCOPE_EXIT{GameXXKLocalization::SetLanguage(Previous,false);};
    auto* MVP=NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
    if(!TestTrue(TEXT("Independent deck fixture starts"),MVP->StartGame()))return false;
    TArray<FName> Characters{TEXT("Player")};
    const auto& State=MVP->GetRuntimeState();
    if(!State.CardRun.CompanionRoster.PermanentCompanions.IsEmpty())Characters.Add(State.CardRun.CompanionRoster.PermanentCompanions[0].InstanceId);
    Characters.Add(TEXT("Npc.TusiChief"));
    for(const FName Character:Characters)
    {
        auto* Inventory=NewObject<UGameXXKInventoryWindowWidget>();Inventory->SetMVPSubsystem(MVP);
        Inventory->ConfigureDesktopTrainingEmbeddedMode(true);Inventory->ConfigureDesktopTrainingCharacter(Character);
        Inventory->TakeWidget();Inventory->OpenFreeInventoryForTest();
        Inventory->OpenCharacterBackpackTabForTest(EGameXXKCharacterBackpackTab::Deck);
        for(const TCHAR* Language:{TEXT("zh-Hans"),TEXT("en")})
        {
            GameXXKLocalization::SetLanguage(Language,false);
            TMap<FString,EGameXXKCardQuality> Qualities;
            for(const auto& Card:FGameXXKCardCatalog::GetAllCardDefinitions())Qualities.Add(GameXXKLocalization::Localize(Card.DisplayName).ToString(),Card.BaseQuality);
            for(int32 Pass=0;Pass<3;++Pass)
            {
                if(Pass==1)Inventory->ToggleDeckDensity();
                if(Pass==2)Inventory->SetDeckExpanded(true);
                int32 Animated=0,SeenNames=0,ExpectedAnimated=0;
                for(const FName Id:Inventory->GetHeroCardBackpackIdsForTest())
                    if(const auto* Card=FGameXXKCardCatalog::FindCardDefinition(Id))
                        ExpectedAnimated+=(Card->BaseQuality==EGameXXKCardQuality::Rare||Card->BaseQuality==EGameXXKCardQuality::Epic)?1:0;
                Inventory->WidgetTree->ForEachWidget([&](UWidget* Widget)
                {
                    auto* Label=Cast<UTextBlock>(Widget);if(!Label)return;
                    auto* Frame=Cast<UImage>(Inventory->WidgetTree->FindWidget(*(Label->GetName()+TEXT("QualityFrame"))));
                    if(!Frame)return;
                    const auto* Quality=Qualities.Find(Label->GetText().ToString());
                    if(!Quality)return;
                    ++SeenNames;
                    if(*Quality!=EGameXXKCardQuality::Rare&&*Quality!=EGameXXKCardQuality::Epic)
                    {
                        TestNull(TEXT("Common cards keep their ordinary name style"),Label->GetFont().FontMaterial);
                        return;
                    }
                    ++Animated;const auto Font=Label->GetFont();
                    TestNotNull(TEXT("Reflow keeps the rarity fill material"),Cast<UMaterialInstanceDynamic>(Font.FontMaterial));
                    TestNotNull(TEXT("Reflow keeps the rarity outline material"),Cast<UMaterialInstanceDynamic>(Font.OutlineSettings.OutlineMaterial));
                    TestNotNull(TEXT("The matching quality frame remains bound"),Frame->GetBrush().GetResourceObject());
                });
                TestEqual(TEXT("Every card face is inspected"),SeenNames,Inventory->GetHeroCardBackpackIdsForTest().Num());
                TestEqual(*FString::Printf(TEXT("%s preserves every actual high-quality card style; base-Common cards are not reclassified"),*Character.ToString()),Animated,ExpectedAnimated);
            }
            Inventory->SetDeckExpanded(false);
        }
    }
    return true;
}
#endif
