#include "Misc/AutomationTest.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/Image.h"
#include "GameXXKRelicCatalog.h"
#include "UI/GameXXKCardNameStyle.h"
#include "UI/GameXXKInRunUiStyle.h"
#include "UI/GameXXKCardTooltipPresentation.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKCardNameQualityStyleTest,
	"GameXXK.MVP.UI.CardEffects.UpgradeNameMaterialsAndReset", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGameXXKCardNameQualityStyleTest::RunTest(const FString&)
{
	UWidgetTree* Tree = NewObject<UWidgetTree>();
	UCanvasPanel* Face = Tree->ConstructWidget<UCanvasPanel>();
	Tree->RootWidget = Face;
	UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),TEXT("QualityTestTitle"));
	Face->AddChild(Text);
	GameXXKCardNameStyle::AttachFrame(Tree,Face,Text);
	Text->SetFont(FGameXXKInRunUiStyle::TitleFont(24,true));
	Text->SetColorAndOpacity(FLinearColor(0.1f,0.07f,0.04f,1));
	GameXXKCardNameStyle::Apply(Text,EGameXXKCardQuality::Rare);
	const FSlateFontInfo RareFont = Text->GetFont();
	TestNotNull(TEXT("rare cards have a flowing glyph fill"), RareFont.FontMaterial.Get());
	TestNotNull(TEXT("rare cards have a separate flowing outline"), RareFont.OutlineSettings.OutlineMaterial.Get());
	UImage* Frame = Cast<UImage>(Tree->FindWidget(TEXT("QualityTestTitleQualityFrame")));
	TestTrue(TEXT("rarity frame never intercepts card input"), Frame && Frame->GetVisibility() == ESlateVisibility::HitTestInvisible);
	TestTrue(TEXT("rarity frame has its own animated material"), Frame && Frame->GetBrush().GetResourceObject());
	GameXXKCardNameStyle::Apply(Text,EGameXXKCardQuality::Epic);
	const FSlateFontInfo EpicFont = Text->GetFont();
	TestNotNull(TEXT("epic fill material exists"), EpicFont.FontMaterial.Get());
	TestNotEqual(TEXT("the second upgrade uses a different warm palette"), EpicFont.FontMaterial, RareFont.FontMaterial);
	TestTrue(TEXT("the second upgrade has the stronger outline"), EpicFont.OutlineSettings.OutlineSize > RareFont.OutlineSettings.OutlineSize);
	GameXXKCardNameStyle::Apply(Text,EGameXXKCardQuality::Common);
	TestNull(TEXT("a reused common title clears animated fill"), Text->GetFont().FontMaterial.Get());
	TestNull(TEXT("a reused common title clears animated outline"), Text->GetFont().OutlineSettings.OutlineMaterial.Get());
	TestEqual(TEXT("a common title restores the unoutlined card name"), Text->GetFont().OutlineSettings.OutlineSize, 0);
	TestTrue(TEXT("reusing a card as common hides the rarity frame"), Frame && Frame->GetVisibility() == ESlateVisibility::Collapsed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKCompactRelicTooltipWidthTest,
	"GameXXK.UI.CardTooltip.ShortRelicWidthBounds", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGameXXKCompactRelicTooltipWidthTest::RunTest(const FString&)
{
	const float Short = GameXXKCardTooltipPresentation::CompactWidth(TEXT("月玉璧"), TEXT("完成路线节点时，\n本路线最大内力提高1点。"));
	const float Long = GameXXKCardTooltipPresentation::CompactWidth(TEXT("保命护符"), TEXT("战斗中任一角色气血将降至50%以下时，令其至少保留1点气血，消耗此遗物并使全队恢复30%最大气血。"));
	TestTrue(TEXT("short relic copy no longer inherits the wide card-rule minimum"), Short >= 280 && Short < 520);
	TestTrue(TEXT("a long relic explanation grows within its own maximum"), Long > Short && Long <= 560);
	UWidgetTree* Tree = NewObject<UWidgetTree>();
	const FGameXXKRelicDefinition* Relic = FGameXXKRelicCatalog::FindDefinition(TEXT("Relic.LifeSavingTalisman"));
	if (Relic)
	{
		Tree->RootWidget = GameXXKCardTooltipPresentation::BuildCompactTooltip(Tree, Relic->DisplayName, Relic->Description.ToString());
		Tree->ForEachWidget([this](UWidget* Widget)
		{
			if (const UTextBlock* Text = Cast<UTextBlock>(Widget))
				TestNotEqual(TEXT("a punctuation mark never owns a separate tooltip row"), Text->GetText().ToString().TrimStartAndEnd(), FString(TEXT("。")));
		});
	}
	return true;
}
#endif
