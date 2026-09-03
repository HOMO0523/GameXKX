#include "UI/GameXXKCardTooltipWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "GameXXKCardCatalog.h"
#include "UI/GameXXKCardTooltipPresentation.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/SWindow.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

namespace
{
	constexpr float CardTooltipWidth = 360.0f;
	constexpr const TCHAR* TooltipPaperTexturePath =
		TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_ItemSlot.T_MasterV2_ItemSlot");

	FSlateBrush MakeTooltipPaperBrush()
	{
		FSlateBrush Brush;
		if (UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, TooltipPaperTexturePath))
		{
			Brush.SetResourceObject(Texture);
			Brush.DrawAs = ESlateBrushDrawType::Box;
			Brush.ImageSize = FVector2D(CardTooltipWidth, 180.0f);
			Brush.Margin = FMargin(0.065f);
		}
		else
		{
			Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
			Brush.TintColor = FSlateColor(FLinearColor(0.88f, 0.81f, 0.68f, 1.0f));
		}
		return Brush;
	}
}

TSharedRef<SWidget> UGameXXKCardTooltipWidget::RebuildWidget()
{
	BuildProgrammaticLayout();
	return Super::RebuildWidget();
}

bool UGameXXKCardTooltipWidget::IsPhysicalShiftDown()
{
#if PLATFORM_WINDOWS
	return (::GetAsyncKeyState(VK_LSHIFT) & 0x8000) != 0
		|| (::GetAsyncKeyState(VK_RSHIFT) & 0x8000) != 0;
#else
	return FSlateApplication::IsInitialized()
		&& FSlateApplication::Get().GetModifierKeys().IsShiftDown();
#endif
}

bool UGameXXKCardTooltipWidget::IsPhysicalControlDown()
{
#if PLATFORM_WINDOWS
	return (::GetAsyncKeyState(VK_LCONTROL) & 0x8000) != 0
		|| (::GetAsyncKeyState(VK_RCONTROL) & 0x8000) != 0;
#else
	return FSlateApplication::IsInitialized()
		&& FSlateApplication::Get().GetModifierKeys().IsControlDown();
#endif
}

bool UGameXXKCardTooltipWidget::IsPhysicalEscapeDown()
{
#if PLATFORM_WINDOWS
	return (::GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
#else
	return false;
#endif
}

bool UGameXXKCardTooltipWidget::IsOwnerWindowActive(const UWidget* Owner)
{
	if (!Owner || !FSlateApplication::IsInitialized() || !FSlateApplication::Get().IsActive())
	{
		return false;
	}
	const TSharedPtr<SWidget> CachedWidget = Owner->GetCachedWidget();
	const TSharedPtr<SWindow> Window = CachedWidget.IsValid()
		? FSlateApplication::Get().FindWidgetWindow(CachedWidget.ToSharedRef()) : nullptr;
	return Window.IsValid() && Window->IsActive();
}

void UGameXXKCardTooltipWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildProgrammaticLayout();
	RefreshPresentation(true);
}

void UGameXXKCardTooltipWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshPresentation(false);
}

void UGameXXKCardTooltipWidget::ConfigureCard(
	const FGameXXKCardDefinition& Definition,
	const EGameXXKCardQuality Quality,
	const FGameXXKCardPlayPreview* Preview,
	const FGameXXKCardTooltipContext& Context)
{
	if (ConfiguredCardId != Definition.Id || ConfiguredQuality != Quality)
	{
		Inspection.Reset();
	}
	ConfiguredCardId = Definition.Id;
	ConfiguredQuality = Quality;
	ConfiguredTitle = Definition.DisplayName;
	CompactBody = GameXXKCardText::DescribeCompactTooltipBody(
		Definition,
		Quality,
		Preview,
		Context);
	ExpandedBody = GameXXKCardText::DescribeExpandedTooltipBody(Definition, Quality, Preview, Context);
	PillBody = GameXXKCardText::DescribePillTooltipBody(Definition, Quality, Context);
	RefreshPresentation(true);
}

void UGameXXKCardTooltipWidget::ConfigureDirect(
	const FText& InTitle,
	const FString& InCompactBody,
	const FString& InExpandedBody)
{
	Inspection.Reset();
	ConfiguredCardId = NAME_None;
	ConfiguredQuality = EGameXXKCardQuality::Invalid;
	PillBody.Reset();
	ConfiguredTitle = InTitle;
	CompactBody = RemoveLeadingTitleLine(InTitle.ToString(), InCompactBody);
	ExpandedBody = RemoveLeadingTitleLine(
		InTitle.ToString(),
		InExpandedBody.IsEmpty() ? InCompactBody : InExpandedBody);
	RefreshPresentation(true);
}

float UGameXXKCardTooltipWidget::GetFixedWidthForTest() const
{
	return CardTooltipWidth;
}

FString UGameXXKCardTooltipWidget::GetDisplayedTextForTest() const
{
	const FString Body = bExpanded ? ExpandedBody : bPillHelpDisplayed ? PillBody : CompactBody;
	return ConfiguredTitle.IsEmpty()
		? Body
		: ConfiguredTitle.ToString() + TEXT("\n") + Body;
}

FString UGameXXKCardTooltipWidget::GetRenderedTextForTest() const
{
	TArray<FString> BodyLines;
	if (BodyBox)
	{
		for (UWidget* RowWidget : BodyBox->GetAllChildren())
		{
			const UHorizontalBox* Row = Cast<UHorizontalBox>(RowWidget);
			if (!Row)
			{
				continue;
			}
			FString RowText;
			for (UWidget* Cell : Row->GetAllChildren())
			{
				if (const UTextBlock* Text = Cast<UTextBlock>(Cell))
				{
					RowText += Text->GetText().ToString();
				}
				else if (const UBorder* Pill = Cast<UBorder>(Cell))
				{
					if (const UTextBlock* PillText = Cast<UTextBlock>(Pill->GetContent()))
					{
						RowText += PillText->GetText().ToString();
					}
				}
			}
			BodyLines.Add(RowText);
		}
	}
	const FString Body = FString::Join(BodyLines, TEXT("\n"));
	return ConfiguredTitle.IsEmpty()
		? Body
		: ConfiguredTitle.ToString() + TEXT("\n") + Body;
}

TArray<FString> UGameXXKCardTooltipWidget::GetPillTextsForTest() const
{
	TArray<FString> Result;
	if (!BodyBox)
	{
		return Result;
	}
	for (UWidget* RowWidget : BodyBox->GetAllChildren())
	{
		const UHorizontalBox* Row = Cast<UHorizontalBox>(RowWidget);
		if (!Row)
		{
			continue;
		}
		for (UWidget* Cell : Row->GetAllChildren())
		{
			const UBorder* Pill = Cast<UBorder>(Cell);
			const UTextBlock* PillText = Pill ? Cast<UTextBlock>(Pill->GetContent()) : nullptr;
			if (PillText)
			{
				Result.Add(PillText->GetText().ToString());
			}
		}
	}
	return Result;
}

float UGameXXKCardTooltipWidget::GetPillFontSizeForTest(const FString& PillText) const
{
	if (!BodyBox)
	{
		return 0.0f;
	}
	for (UWidget* RowWidget : BodyBox->GetAllChildren())
	{
		const UHorizontalBox* Row = Cast<UHorizontalBox>(RowWidget);
		if (!Row)
		{
			continue;
		}
		for (UWidget* Cell : Row->GetAllChildren())
		{
			const UBorder* Pill = Cast<UBorder>(Cell);
			const UTextBlock* Text = Pill ? Cast<UTextBlock>(Pill->GetContent()) : nullptr;
			if (Text && Text->GetText().ToString() == PillText)
			{
				return Text->GetFont().Size;
			}
		}
	}
	return 0.0f;
}

bool UGameXXKCardTooltipWidget::IsExpandedForTest() const
{
	return bExpanded;
}

void UGameXXKCardTooltipWidget::SetExpandedForTest(const bool bInExpanded)
{
	bUseExpandedOverrideForTest = true;
	bExpandedOverrideForTest = bInExpanded;
	RefreshPresentation(true);
}

void UGameXXKCardTooltipWidget::SetExpandedFromOwner(const bool bInExpanded)
{
	if (!bUseExpandedOverrideForTest
		&& bUseOwnerExpandedState
		&& bOwnerExpandedState == bInExpanded
		&& bExpanded == bInExpanded)
	{
		return;
	}
	bUseExpandedOverrideForTest = false;
	bUseOwnerExpandedState = true;
	bOwnerExpandedState = bInExpanded;
	RefreshPresentation(true);
}

void UGameXXKCardTooltipWidget::UpdateInspectionFromOwner(
	const bool bHovered, const bool bShiftDown, const bool bControlDown, const bool bEscapeDown)
{
	bUseExpandedOverrideForTest = false;
	bUseOwnerExpandedState = true;
	bOwnerExpandedState = bHovered && bShiftDown;
	Inspection.Update(bHovered, bShiftDown, bControlDown, bEscapeDown);
	RefreshPresentation(false);
}

void UGameXXKCardTooltipWidget::BuildProgrammaticLayout()
{
	if (!WidgetTree || RootSizeBox)
	{
		return;
	}

	RootSizeBox = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(),
		TEXT("CardTooltipFixedWidth"));
	RootSizeBox->SetWidthOverride(CardTooltipWidth);
	RootSizeBox->SetVisibility(ESlateVisibility::HitTestInvisible);
	WidgetTree->RootWidget = RootSizeBox;

	PaperFrame = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(),
		TEXT("CardTooltipPaper"));
	PaperFrame->SetBrush(MakeTooltipPaperBrush());
	PaperFrame->SetBrushColor(FLinearColor::White);
	PaperFrame->SetPadding(FMargin(16.0f, 12.0f));
	PaperFrame->SetVisibility(ESlateVisibility::HitTestInvisible);
	RootSizeBox->AddChild(PaperFrame);

	UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(),
		TEXT("CardTooltipStack"));
	PaperFrame->SetContent(Stack);

	TitleText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("CardTooltipTitle"));
	TitleText->SetColorAndOpacity(FSlateColor(FLinearColor(0.08f, 0.06f, 0.04f, 1.0f)));
	TitleText->SetJustification(ETextJustify::Left);
	FSlateFontInfo TitleFont = TitleText->GetFont();
	TitleFont.Size = 22;
	TitleFont.TypefaceFontName = TEXT("Bold");
	TitleFont.OutlineSettings.OutlineSize = 1;
	TitleFont.OutlineSettings.OutlineColor = FLinearColor(0.08f, 0.06f, 0.04f, 1.0f);
	TitleText->SetFont(TitleFont);
	TitleText->SetVisibility(ESlateVisibility::HitTestInvisible);
	Stack->AddChildToVerticalBox(TitleText);

	BodyBox = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(),
		TEXT("CardTooltipBody"));
	BodyBox->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UVerticalBoxSlot* BodySlot = Stack->AddChildToVerticalBox(BodyBox))
	{
		BodySlot->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 0.0f));
	}
	RefreshPresentation(true);
}

void UGameXXKCardTooltipWidget::RefreshPresentation(const bool bForce)
{
	const bool bResolvedExpanded = ResolveExpandedState();
	const bool bResolvedPills = !bResolvedExpanded && !PillBody.IsEmpty()
		&& Inspection.GetMode() == EGameXXKCardTooltipMode::Pills;
	if (!bForce && bExpanded == bResolvedExpanded && bPillHelpDisplayed == bResolvedPills)
	{
		return;
	}
	bExpanded = bResolvedExpanded;
	bPillHelpDisplayed = bResolvedPills;
	if (!TitleText || !BodyBox || !WidgetTree)
	{
		return;
	}
	TitleText->SetText(ConfiguredTitle);
	const FString& Body = bExpanded ? ExpandedBody : bPillHelpDisplayed ? PillBody : CompactBody;
	FGameXXKCardTooltipPresentationStyle Style;
	Style.bPillHelp = bPillHelpDisplayed;
	GameXXKCardTooltipPresentation::PopulateBody(
		WidgetTree,
		BodyBox,
		ConfiguredTitle.ToString(),
		Body,
		Style);
}

bool UGameXXKCardTooltipWidget::ResolveExpandedState() const
{
	if (bUseExpandedOverrideForTest)
	{
		return bExpandedOverrideForTest;
	}
	if (bUseOwnerExpandedState)
	{
		return bOwnerExpandedState;
	}
	return IsPhysicalShiftDown();
}

FString UGameXXKCardTooltipWidget::RemoveLeadingTitleLine(
	const FString& Title,
	const FString& Body)
{
	const FString Prefix = Title + TEXT("\n");
	return !Title.IsEmpty() && Body.StartsWith(Prefix)
		? Body.Mid(Prefix.Len())
		: Body;
}
