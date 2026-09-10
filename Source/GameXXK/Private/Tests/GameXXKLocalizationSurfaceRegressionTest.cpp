#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/GameInstance.h"
#include "GameXXKGemRules.h"
#include "GameXXKEquipmentCatalog.h"
#include "GameXXKTalentCatalog.h"
#include "GameXXKTalentRules.h"
#include "GameXXKCardCatalog.h"
#include "UI/GameXXKCardTooltipWidget.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKDesktopTrainingWorkbenchWidget.h"
#include "UI/GameXXKBattleUnitResourceWidget.h"
#include "UI/GameXXKLocalization.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
    bool HasChinese(const FString& Text){for(TCHAR C:Text)if(C>=0x3400&&C<=0x9fff)return true;return false;}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKLocalizedSurfaceLabelsTest,
    "GameXXK.Localization.Surfaces.GemsVitalsAndShortActions",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKLocalizedSurfaceLabelsTest::RunTest(const FString&)
{
    const FString Previous=GameXXKLocalization::GetLanguage();
    ON_SCOPE_EXIT{GameXXKLocalization::SetLanguage(Previous,false);};
    GameXXKLocalization::SetLanguage(TEXT("en"),false);
    TestEqual(TEXT("Auto keeps its short label while off"),GameXXKLocalization::Compact(GameXXKLocalization::Source(TEXT("自动战斗：关"))).ToString(),FString(TEXT("Auto")));
    TestEqual(TEXT("Auto keeps its short label while on"),GameXXKLocalization::Compact(GameXXKLocalization::Source(TEXT("自动战斗：开"))).ToString(),FString(TEXT("Auto")));
    TestEqual(TEXT("Map page uses Maps"),GameXXKLocalization::Source(TEXT("历练地图")).ToString(),FString(TEXT("Maps")));
    TestEqual(TEXT("Map navigation remains singular"),GameXXKLocalization::Source(TEXT("历练")).ToString(),FString(TEXT("Map")));
    TestEqual(TEXT("Flattened equipment names are covered too"),GameXXKLocalization::Source(TEXT("破军鞋履")).ToString(),FString(TEXT("War Boots")));
    TestEqual(TEXT("Actual enemy intent title is translated"),GameXXKLocalization::Source(TEXT("双重啄击")).ToString(),FString(TEXT("Twin Peck")));
    auto* Resource=NewObject<UGameXXKBattleUnitResourceWidget>();Resource->PrepareForScreenSpaceEmbedding();
    Resource->SetUnitVitals(TEXT("敌 2P"),FText::FromString(TEXT("黄鼬")),66,66,0,0,false);
    TestEqual(TEXT("Live HP label never reverts to untranslated Chinese on refresh"),Resource->GetHealthDisplayTextForTest(),FString(TEXT("HP 66 / 66")));
    Resource->SetUnitVitals(TEXT("我 2P"),FText::FromString(TEXT("主角")),814,835,30,30,true);
    TestEqual(TEXT("MP label uses the canonical abbreviation"),Resource->GetManaDisplayTextForTest(),FString(TEXT("MP 30 / 30")));
    Resource->WidgetTree->ForEachWidget([this](UWidget* Widget){if(auto* T=Cast<UTextBlock>(Widget))TestFalse(TEXT("Unit identity and both resource rows are translated"),HasChinese(T->GetText().ToString()));});
    int32 Count=0;
    for(int32 Type=1;Type<=17;++Type)for(int32 Quality=1;Quality<=10;++Quality)
    {
        const auto T=static_cast<EGameXXKGemType>(Type);const auto Q=static_cast<EGameXXKGemQuality>(Quality);
        const FText Name=FGameXXKGemRules::GetDisplayName(T,Q);const FText Description=FGameXXKGemRules::GetDescription(T,Q);
        if(Name.IsEmpty())continue;
        ++Count;
        TestFalse(*FString::Printf(TEXT("Gem name %d/%d"),Type,Quality),HasChinese(Name.ToString()));
        TestFalse(*FString::Printf(TEXT("Complete gem description %d/%d"),Type,Quality),HasChinese(Description.ToString()));
        TestFalse(*FString::Printf(TEXT("Socket row %d/%d"),Type,Quality),HasChinese(FGameXXKGemRules::GetSocketText(T,Q).ToString()));
    }
    TestEqual(TEXT("All modern and compatible gem names are covered"),Count,170);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKToolDropdownSurfaceTest,
    "GameXXK.Localization.Surfaces.ToolDropdownModesAndClose",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKToolDropdownSurfaceTest::RunTest(const FString&)
{
    const FString Previous=GameXXKLocalization::GetLanguage();
    ON_SCOPE_EXIT{GameXXKLocalization::SetLanguage(Previous,false);};
    GameXXKLocalization::SetLanguage(TEXT("en"),false);
    auto* MVP=NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
    if(!TestTrue(TEXT("Independent fixture starts"),MVP->StartGame()))return false;
    auto* Widget=NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();Widget->SetMVPSubsystem(MVP);
    Widget->ConstructForTest();Widget->OpenWorkbench();Widget->OpenBackpack();Widget->HandleDesktopActionForTest(3);Widget->TickForTest(0);
    const auto Measure=FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
    for(int32 Selected=0;Selected<5;++Selected)
    {
        auto* Selector=Cast<UButton>(Widget->WidgetTree->FindWidget(TEXT("ToolModeDropdownButton")));
        if(!TestNotNull(TEXT("Separate square disclosure button is visible"),Selector))return false;
        const auto* Slot=Cast<UCanvasPanelSlot>(Selector->Slot);
        TestTrue(TEXT("Disclosure is a square, separate from the translated mode"),Slot&&Slot->GetSize().Equals(FVector2D(42,42)));
        auto* ModeLabel=Cast<UTextBlock>(Widget->WidgetTree->FindWidget(TEXT("ToolModeLabel")));
        TestTrue(TEXT("Current mode name translates as well as the menu rows"),ModeLabel&&!HasChinese(ModeLabel->GetText().ToString()));
        auto* Icon=Cast<UImage>(Selector->GetContent());
        TestTrue(TEXT("Disclosure is an image rather than a font glyph or text"),Icon&&Icon->GetBrush().GetResourceObject());
        TestNull(TEXT("Closed selector hides the option row"),Widget->WidgetTree->FindWidget(TEXT("ToolButton_0")));
        Widget->HandleDesktopActionForTest(664);Widget->TickForTest(0);
        for(int32 Option=0;Option<5;++Option)
        {
            auto* B=Cast<UButton>(Widget->WidgetTree->FindWidget(*FString::Printf(TEXT("ToolButton_%d"),Option)));
            if(!TestNotNull(TEXT("Every mode is available in the dropdown"),B))return false;
            auto* T=Cast<UTextBlock>(B->GetContent());if(!TestNotNull(TEXT("Mode has text"),T))return false;
            TestFalse(TEXT("Mode name is fully translated"),HasChinese(T->GetText().ToString()));
            TestTrue(TEXT("Full mode name fits without squeezing"),Measure->Measure(T->GetText(),T->GetFont()).X<220);
        }
        Widget->HandleDesktopActionForTest(30+Selected);Widget->TickForTest(0);
        TestNull(TEXT("Selection closes the dropdown"),Widget->WidgetTree->FindWidget(TEXT("ToolModeDropdownMenu")));
    }
    Widget->HandleDesktopActionForTest(664);Widget->TickForTest(0);
    Widget->HandleDesktopActionForTest(664);Widget->TickForTest(0);
    TestNull(TEXT("Toggling the selector closes it without changing the page"),Widget->WidgetTree->FindWidget(TEXT("ToolModeDropdownMenu")));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKWarehouseFooterSurfaceTest,
    "GameXXK.Localization.Surfaces.WarehouseActionsReplaceVerboseFooter",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKWarehouseFooterSurfaceTest::RunTest(const FString&)
{
    const FString Previous=GameXXKLocalization::GetLanguage();
    ON_SCOPE_EXIT{GameXXKLocalization::SetLanguage(Previous,false);};
    auto* MVP=NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
    if(!TestTrue(TEXT("Independent fixture starts"),MVP->StartGame()))return false;
    auto* Widget=NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();Widget->SetMVPSubsystem(MVP);
    Widget->ConstructForTest();Widget->OpenWorkbench();Widget->OpenBackpack();
    for(const TCHAR* Language:{TEXT("zh-Hans"),TEXT("en")})
    {
        GameXXKLocalization::SetLanguage(Language,false);
        Widget->HandleDesktopActionForTest(0);Widget->TickForTest(0);
        TestNull(TEXT("No redundant page/slot footer"),Widget->WidgetTree->FindWidget(TEXT("WarehousePageSummaryText")));
        TestNull(TEXT("No redundant item/character-card footer"),Widget->WidgetTree->FindWidget(TEXT("WarehouseFooterText")));
        const TCHAR* Names[]={TEXT("WarehouseBatchToBackpackButton"),TEXT("BackpackBatchToWarehouseButton")};
        const TCHAR* ExpectedEn[]={TEXT("Take"),TEXT("Store")};
        for(int32 Index=0;Index<2;++Index)
        {
            auto* Button=Cast<UButton>(Widget->WidgetTree->FindWidget(Names[Index]));
            if(!TestNotNull(TEXT("Transfer action remains available"),Button))return false;
            auto* Label=Cast<UTextBlock>(Button->GetContent());if(!TestNotNull(TEXT("Transfer action has text"),Label))return false;
            if(GameXXKLocalization::IsEnglish())TestEqual(TEXT("Short unambiguous transfer label"),Label->GetText().ToString(),FString(ExpectedEn[Index]));
            TestFalse(TEXT("Full transfer scope remains available on hover"),Button->GetToolTipText().IsEmpty());
        }
        Widget->HandleDesktopActionForTest(0);Widget->TickForTest(0);
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKFullTalentEnglishTest,
    "GameXXK.Localization.Surfaces.FullTalentNamesAndEffects",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKFullTalentEnglishTest::RunTest(const FString&)
{
    const FString Previous=GameXXKLocalization::GetLanguage();
    ON_SCOPE_EXIT{GameXXKLocalization::SetLanguage(Previous,false);};
    GameXXKLocalization::SetLanguage(TEXT("en"),false);
    for(const auto& Node:FGameXXKTalentCatalog::GetDefinitions())
    {
        TestFalse(*FString::Printf(TEXT("Talent name %s"),*Node.Id.ToString()),HasChinese(GameXXKLocalization::Localize(Node.DisplayName).ToString()));
        TestFalse(*FString::Printf(TEXT("Talent description %s"),*Node.Id.ToString()),HasChinese(GameXXKLocalization::Localize(Node.Description).ToString()));
        TestFalse(*FString::Printf(TEXT("Talent effect %s"),*Node.Id.ToString()),HasChinese(GameXXKLocalization::Localize(FGameXXKTalentRules::DescribeEffect(Node)).ToString()));
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKPlainEnglishPillTest,
    "GameXXK.Localization.Surfaces.PlainEnglishPills",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKPlainEnglishPillTest::RunTest(const FString&)
{
    const FString Previous=GameXXKLocalization::GetLanguage();
    ON_SCOPE_EXIT{GameXXKLocalization::SetLanguage(Previous,false);};
    GameXXKLocalization::SetLanguage(TEXT("en"),false);
    for(const TCHAR* Id:{TEXT("Hero.Generic.SuiYanJi"),TEXT("Hero.Generic.HengJianShouShi")})
    {
        const auto* Card=FGameXXKCardCatalog::FindCardDefinition(Id);
        if(!TestNotNull(TEXT("User-reported card exists"),Card))return false;
        auto* Tooltip=NewObject<UGameXXKCardTooltipWidget>();Tooltip->Initialize();
        Tooltip->SetExpandedForTest(false);Tooltip->ConfigureCard(*Card,EGameXXKCardQuality::Common,nullptr,{});Tooltip->TakeWidget();
        const auto Pills=Tooltip->GetPillTextsForTest();
        TestTrue(TEXT("The status colour pills remain present"),!Pills.IsEmpty());
        Tooltip->WidgetTree->ForEachWidget([&](UWidget* Widget)
        {
            if(auto* Text=Cast<UTextBlock>(Widget))if(Pills.Contains(Text->GetText().ToString()))
            {
                TestFalse(TEXT("Pill names are English"),HasChinese(Text->GetText().ToString()));
                TestEqual(TEXT("Pill text has no stroke outline"),Text->GetFont().OutlineSettings.OutlineSize,0);
            }
        });
        TestFalse(*FString::Printf(TEXT("Whole displayed tooltip is English for %s"),Id),HasChinese(Tooltip->GetRenderedTextForTest()));
    }
    return true;
}
#endif
