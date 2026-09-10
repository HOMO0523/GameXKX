#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameXXKCardPillText.h"
#include "GameXXKPermanentPartyTestFixtures.h"
#include "UI/GameXXKLocalization.h"
#include "UI/GameXXKInventoryWindowWidget.h"
#include "UI/GameXXKDesktopTrainingLayout.h"
#include "UI/GameXXKInRunUiStyle.h"
#include "UI/GameXXKCardNameStyle.h"
#include "UI/GameXXKCardTooltipWidget.h"
#include "UI/GameXXKDesktopTrainingWorkbenchWidget.h"
#include "GameXXKEquipmentCatalog.h"
#include "GameXXKEquipmentSetCatalog.h"
#include "GameXXKCardCatalog.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKEnglishCompactTabsTest,
    "GameXXK.Localization.Presentation.CompactTabsFitAndKeepFullHelp",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKEnglishCompactTabsTest::RunTest(const FString&)
{
    const FString Previous=GameXXKLocalization::GetLanguage();
    ON_SCOPE_EXIT{GameXXKLocalization::SetLanguage(Previous,false);};
    GameXXKLocalization::SetLanguage(TEXT("zh-Hans"),false);
    auto* MVP=NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
    if(!TestTrue(TEXT("Independent character fixture starts"),MVP->StartGame()))return false;
    auto* Inventory=NewObject<UGameXXKInventoryWindowWidget>();
    Inventory->SetMVPSubsystem(MVP);Inventory->ConfigureDesktopTrainingEmbeddedMode(true);
    Inventory->TakeWidget();Inventory->OpenFreeInventoryForTest();
    GameXXKLocalization::SetLanguage(TEXT("en"),false);
    const TCHAR* Short[]={TEXT("Attr"),TEXT("Equip"),TEXT("Deck")};
    const TCHAR* Full[]={TEXT("Attributes"),TEXT("Equipment"),TEXT("Deck")};
    const auto Measure=FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
    for(int32 Index=0;Index<3;++Index)
    {
        auto* Button=Cast<UButton>(Inventory->WidgetTree->FindWidget(*FString::Printf(TEXT("InventoryCharacterTab_%d"),Index)));
        if(!TestNotNull(TEXT("The real narrow tab exists"),Button))return false;
        auto* Label=Cast<UTextBlock>(Button->GetContent());
        if(!TestNotNull(TEXT("The real tab has readable text"),Label))return false;
        TestEqual(TEXT("English compact caption"),Label->GetText().ToString(),FString(Short[Index]));
        TestEqual(TEXT("Hover keeps the full English name"),Button->GetToolTipText().ToString(),FString(Full[Index]));
        const auto Rect=GameXXKDesktopTrainingLayout::GetEmbeddedCharacterTabRect(Index);
        TestTrue(TEXT("Actual Jianghu glyph width fits the tab with padding"),Measure->Measure(Label->GetText(),Label->GetFont()).X<=Rect.Z-12);
        TestTrue(TEXT("Tab uses the selected Jianghu font"),Label->GetFont().FontObject&&Label->GetFont().FontObject->GetPathName().Contains(TEXT("JiangHuGuFeng")));
    }
    GameXXKLocalization::SetLanguage(TEXT("zh-Hans"),false);
    auto* Attr=Cast<UButton>(Inventory->WidgetTree->FindWidget(TEXT("InventoryCharacterTab_0")));
    TestEqual(TEXT("The same tab switches back without losing its caption"),Cast<UTextBlock>(Attr->GetContent())->GetText().ToString(),FString(TEXT("属性")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKEnglishPillBoundaryTest,
    "GameXXK.Localization.Presentation.PillAliasesHaveWordBoundaries",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKEnglishPillBoundaryTest::RunTest(const FString&)
{
    TestEqual(TEXT("Opener has a different identity from Charge"),GameXXKCardPillText::CanonicalName(TEXT("Opener")),FString(TEXT("冲锋")));
    TestEqual(TEXT("Charge means the Heavy Arrow resource"),GameXXKCardPillText::CanonicalName(TEXT("Charge")),FString(TEXT("蓄力")));
    TestEqual(TEXT("Adjective maps to the original status"),GameXXKCardPillText::CanonicalName(TEXT("Poisoned")),FString(TEXT("中毒")));
    TestFalse(TEXT("Mark cannot match Market"),GameXXKCardPillText::MatchesAt(TEXT("Market"),0,TEXT("Mark")));
    TestFalse(TEXT("Mark cannot match a suffix inside another word"),GameXXKCardPillText::MatchesAt(TEXT("landmark"),4,TEXT("Mark")));
    TestTrue(TEXT("A complete keyword next to punctuation matches"),GameXXKCardPillText::MatchesAt(TEXT("2 Mark,"),2,TEXT("Mark")));
    TestTrue(TEXT("Native semantic keyword list stays Chinese"),GameXXKCardPillText::InlineNames().Contains(TEXT("冲锋"))&&!GameXXKCardPillText::InlineNames().Contains(TEXT("Opener")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKEnglishTooltipRefreshTest,
    "GameXXK.Localization.Presentation.OpenTooltipRefreshesLanguageAndFont",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKEnglishTooltipRefreshTest::RunTest(const FString&)
{
    const FString Previous=GameXXKLocalization::GetLanguage();
    ON_SCOPE_EXIT{GameXXKLocalization::SetLanguage(Previous,false);};
    GameXXKLocalization::SetLanguage(TEXT("zh-Hans"),false);
    auto* Tooltip=NewObject<UGameXXKCardTooltipWidget>();Tooltip->TakeWidget();
    Tooltip->ConfigureDirect(GameXXKLocalization::Source(TEXT("属性")),TEXT("冲锋：此牌是本回合第一张主动牌打出时，出牌者获得2层气势\n攻击 +17"));
    GameXXKLocalization::SetLanguage(TEXT("en"),false);
    Tooltip->UpdateInspectionFromOwner(true,false,false,false);
    const FString Rendered=Tooltip->GetRenderedTextForTest();
    TestTrue(TEXT("The already open tooltip is translated"),Rendered.Contains(TEXT("Opener"))&&Rendered.Contains(TEXT("Momentum"))&&Rendered.Contains(TEXT("17")));
    TestFalse(TEXT("No original Chinese words remain in this covered example"),Rendered.Contains(TEXT("冲锋"))||Rendered.Contains(TEXT("气势"))||Rendered.Contains(TEXT("攻击")));
    const auto Pills=Tooltip->GetPillTextsForTest();
    TestTrue(TEXT("English mechanics are actual Pills"),Pills.Contains(TEXT("Opener"))&&Pills.Contains(TEXT("Momentum")));
    Tooltip->WidgetTree->ForEachWidget([this](UWidget* Widget)
    {
        if(const auto* Text=Cast<UTextBlock>(Widget);Text&&!Text->GetText().IsEmpty())
        {
            TestTrue(TEXT("Every rendered tooltip row uses Jianghu"),Text->GetFont().FontObject&&Text->GetFont().FontObject->GetPathName().Contains(TEXT("JiangHuGuFeng")));
            TestEqual(TEXT("No nonexistent Bold face overrides Jianghu"),Text->GetFont().TypefaceFontName,FName(TEXT("Default")));
        }
    });
    Tooltip->ConfigureDirect(FText::FromString(TEXT("Wrap")),TEXT("antidisestablishmentarianism antidisestablishmentarianism"));
    TArray<FString> Words;Tooltip->GetRenderedTextForTest().ParseIntoArrayWS(Words);
    TestEqual(TEXT("Wrapping keeps both complete English words"),Words.FilterByPredicate([](const FString& Word){return Word==TEXT("antidisestablishmentarianism");}).Num(),2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKEnglishMaterialAspectTest,
    "GameXXK.Localization.Presentation.NameMaterialUsesNewLanguageWidth",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKEnglishMaterialAspectTest::RunTest(const FString&)
{
    const FString Previous=GameXXKLocalization::GetLanguage();
    ON_SCOPE_EXIT{GameXXKLocalization::SetLanguage(Previous,false);};
    GameXXKLocalization::SetLanguage(TEXT("zh-Hans"),false);
    auto* Text=NewObject<UTextBlock>();Text->SetFont(FGameXXKInRunUiStyle::Font(24,true));
    Text->SetText(GameXXKLocalization::Source(TEXT("装备等级 37")));
    GameXXKCardNameStyle::Apply(Text,EGameXXKCardQuality::Rare);
    auto* Fill=Cast<UMaterialInstanceDynamic>(Text->GetFont().FontMaterial.Get());
    if(!TestNotNull(TEXT("Real quality material exists"),Fill))return false;
    float Before=0,After=0;Fill->GetScalarParameterValue(FMaterialParameterInfo(TEXT("TextAspect")),Before);
    GameXXKLocalization::SetLanguage(TEXT("en"),false);
    Fill->GetScalarParameterValue(FMaterialParameterInfo(TEXT("TextAspect")),After);
    const auto Measure=FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
    const float Expected=FMath::Max(1.0f,static_cast<float>(Measure->Measure(Text->GetText(),Text->GetFont()).X)/Measure->GetMaxCharacterHeight(Text->GetFont()));
    TestTrue(TEXT("Existing MID receives the measured English width"),FMath::IsNearlyEqual(After,Expected,0.01f));
    TestTrue(TEXT("The language width actually changed"),!FMath::IsNearlyEqual(After,Before,0.01f));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKEnglishRosterTermsTest,
    "GameXXK.Localization.Presentation.RosterUsesShortContextualEnglish",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKEnglishRosterTermsTest::RunTest(const FString&)
{
    const FString Previous=GameXXKLocalization::GetLanguage();
    ON_SCOPE_EXIT{GameXXKLocalization::SetLanguage(Previous,false);};
    GameXXKLocalization::SetLanguage(TEXT("en"),false);
    TestEqual(TEXT("Map navigation has the approved label"),GameXXKLocalization::Source(TEXT("历练")).ToString(),FString(TEXT("Map")));
    TestEqual(TEXT("Tool navigation has the approved label"),GameXXKLocalization::Source(TEXT("工具")).ToString(),FString(TEXT("Tool")));
    TestEqual(TEXT("Current view never enters the combat When template"),GameXXKLocalization::Source(TEXT("当前查看：主角")).ToString(),FString(TEXT("Viewing: Hero")));
    auto* MVP=NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
    if(!TestTrue(TEXT("Independent roster fixture starts"),MVP->StartGame()))return false;
    auto* Workbench=NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
    Workbench->SetMVPSubsystem(MVP);Workbench->ConstructForTest();Workbench->OpenWorkbench();Workbench->OpenBackpack();
    Workbench->HandleDesktopActionForTest(81);Workbench->TickForTest(0);
    const auto Measure=FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
    const TCHAR* Expected[]={TEXT("Blade"),TEXT("Guard"),TEXT("Healer"),TEXT("Archer"),TEXT("Mage"),TEXT("Tactician")};
    for(int32 Index=0;Index<6;++Index)
    {
        auto* Name=Cast<UTextBlock>(Workbench->WidgetTree->FindWidget(*FString::Printf(TEXT("CharacterPickerName_%d"),Index)));
        if(!TestNotNull(TEXT("All six role labels exist on their actual cards"),Name))return false;
        TestEqual(TEXT("Each class has a readable English name"),Name->GetText().ToString(),FString(Expected[Index]));
        TestTrue(TEXT("Class name fits its 119.5-wide card title"),Measure->Measure(Name->GetText(),Name->GetFont()).X<=119.5f);
    }
    auto* Back=Cast<UButton>(Workbench->WidgetTree->FindWidget(TEXT("CharacterPickerBack")));
    if(!TestNotNull(TEXT("Back control exists"),Back))return false;
    auto* Caption=Cast<UTextBlock>(Back->GetContent());
    if(!TestNotNull(TEXT("Back has a label"),Caption))return false;
    TestEqual(TEXT("The narrow return button uses Back"),Caption->GetText().ToString(),FString(TEXT("Back")));
    TestEqual(TEXT("Hover retains the full destination"),Back->GetToolTipText().ToString(),FString(TEXT("Back to bag")));
    TestTrue(TEXT("Back fits with padding"),Measure->Measure(Caption->GetText(),Caption->GetFont()).X<=124);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKEnglishNamesAndResourcesTest,
    "GameXXK.Localization.Presentation.CatalogNamesAndResourceTerms",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKEnglishNamesAndResourcesTest::RunTest(const FString&)
{
    const FString Previous=GameXXKLocalization::GetLanguage();
    ON_SCOPE_EXIT{GameXXKLocalization::SetLanguage(Previous,false);};
    GameXXKLocalization::SetLanguage(TEXT("en"),false);
    const auto ContainsChinese=[](const FString& Text){for(TCHAR C:Text)if(C>=0x3400&&C<=0x9fff)return true;return false;};
    int32 CardCount=0;
    for(const auto& Card:FGameXXKCardCatalog::GetAllCardDefinitions())
    {
        ++CardCount;
        TestFalse(*FString::Printf(TEXT("Catalogue card name is translated: %s"),*Card.Id.ToString()),ContainsChinese(Card.DisplayName.ToString()));
    }
    TestEqual(TEXT("Every approved card is covered"),CardCount,173);
    const auto* Bone=FGameXXKEquipmentCatalog::FindDefinition(TEXT("Equipment.ShiGu.Accessory"));
    if(!TestNotNull(TEXT("Bone accessory exists"),Bone))return false;
    TestEqual(TEXT("Equipment name is exactly set plus slot"),Bone->DisplayName.ToString(),FString(TEXT("Bone Accessory")));
    for(const auto& Bonus:FGameXXKEquipmentSetCatalog::GetDefinitions())
        TestFalse(*FString::Printf(TEXT("Complete set rule is translated: %s"),*Bonus.Id.ToString()),ContainsChinese(GameXXKLocalization::Localize(Bonus.Description).ToString()));
    TestEqual(TEXT("Two-line card costs use AP and MP"),GameXXKLocalization::Source(TEXT("1气\n0内")).ToString(),FString(TEXT("1 AP\n0 MP")));
    TestEqual(TEXT("Full AP explanation uses Action Points"),GameXXKLocalization::Text(TEXT("Resource.ActionPoints")).ToString(),FString(TEXT("Action Points")));
    GameXXKLocalization::SetLanguage(TEXT("zh-Hans"),false);
    TestEqual(TEXT("The same equipment name returns to Chinese"),Bone->DisplayName.ToString(),FString(TEXT("蚀骨饰品")));
    return true;
}
#endif
