#include "UI/GameXXKMetaShopWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "GameXXKEquipmentRules.h"
#include "MVP/GameXXKMVPSubsystem.h"

namespace
{
	constexpr const TCHAR* PaperFrameTexturePath = TEXT("/Game/GameXXK/UI/Town/Textures/Backpack/T_TownBackpack_WindowFrame.T_TownBackpack_WindowFrame");

	UTextBlock* MakeText(UWidgetTree* Tree, const FName Name, const FText& Text, const int32 Size)
	{
		UTextBlock* Block = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Block->SetText(Text);
		Block->SetAutoWrapText(true);
		Block->SetJustification(ETextJustify::Center);
		Block->SetColorAndOpacity(FSlateColor(FLinearColor(0.12f, 0.08f, 0.04f, 1.0f)));
		FSlateFontInfo Font = Block->GetFont();
		Font.Size = Size;
		Block->SetFont(Font);
		return Block;
	}

	void AddCanvas(UCanvasPanel* Canvas, UWidget* Widget, const FVector2D Position, const FVector2D Size, const int32 ZOrder)
	{
		if (UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Widget))
		{
			Slot->SetPosition(Position);
			Slot->SetSize(Size);
			Slot->SetZOrder(ZOrder);
		}
	}

	FSlateBrush SolidBrush(const FLinearColor Color)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Box;
		Brush.TintColor = FSlateColor(Color);
		return Brush;
	}

	FSlateBrush TextureBrush(const TCHAR* Path)
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(LoadObject<UTexture2D>(nullptr, Path));
		Brush.DrawAs = ESlateBrushDrawType::Box;
		Brush.Margin = FMargin(0.08f);
		Brush.TintColor = FSlateColor(FLinearColor::White);
		return Brush;
	}
}

void UGameXXKMetaShopProductButton::Configure(
	UGameXXKMetaShopWidget* InOwner,
	const EGameXXKMetaShopProductId InProductId)
{
	Owner = InOwner;
	ProductId = InProductId;
	OnClicked.RemoveDynamic(this, &UGameXXKMetaShopProductButton::HandleClicked);
	OnClicked.AddDynamic(this, &UGameXXKMetaShopProductButton::HandleClicked);
}

void UGameXXKMetaShopProductButton::HandleClicked()
{
	if (Owner)
	{
		Owner->SelectProduct(ProductId);
	}
}

TSharedRef<SWidget> UGameXXKMetaShopWidget::RebuildWidget()
{
	BuildProgrammaticLayout();
	return Super::RebuildWidget();
}

void UGameXXKMetaShopWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildProgrammaticLayout();
	RefreshFromState();
}

void UGameXXKMetaShopWidget::BuildProgrammaticLayout()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("MetaShopRoot"));
	WidgetTree->RootWidget = RootCanvas;

	UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MetaShopBackdrop"));
	Backdrop->SetBrush(SolidBrush(FLinearColor(0.02f, 0.015f, 0.01f, 0.72f)));
	AddCanvas(RootCanvas, Backdrop, FVector2D::ZeroVector, FVector2D(1920.0f, 1080.0f), 0);

	UBorder* PaperFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MetaShopPaperFrame"));
	PaperFrame->SetBrush(TextureBrush(PaperFrameTexturePath));
	AddCanvas(RootCanvas, PaperFrame, FVector2D(70.0f, 55.0f), FVector2D(1780.0f, 970.0f), 1);

	GoldText = MakeText(WidgetTree, TEXT("MetaShopGoldText"), FText::GetEmpty(), 34);
	AddCanvas(RootCanvas, GoldText, FVector2D(135.0f, 105.0f), FVector2D(430.0f, 60.0f), 2);

	UButton* CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("MetaShopCloseButton"));
	CloseButton->OnClicked.AddDynamic(this, &UGameXXKMetaShopWidget::HandleCloseClicked);
	CloseButton->AddChild(MakeText(WidgetTree, TEXT("MetaShopCloseText"), FText::FromString(TEXT("关闭")), 28));
	AddCanvas(RootCanvas, CloseButton, FVector2D(1640.0f, 100.0f), FVector2D(130.0f, 58.0f), 2);

	ProductGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("MetaShopProductGrid"));
	ProductGrid->SetSlotPadding(FMargin(10.0f));
	AddCanvas(RootCanvas, ProductGrid, FVector2D(125.0f, 200.0f), FVector2D(1070.0f, 730.0f), 2);

	for (int32 Index = 0; Index < 7; ++Index)
	{
		UGameXXKMetaShopProductButton* Card = WidgetTree->ConstructWidget<UGameXXKMetaShopProductButton>(
			UGameXXKMetaShopProductButton::StaticClass(),
			FName(*FString::Printf(TEXT("MetaShopProductCard_%d"), Index)));
		UVerticalBox* CardBody = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		UImage* Image = WidgetTree->ConstructWidget<UImage>(
			UImage::StaticClass(),
			FName(*FString::Printf(TEXT("MetaShopProductImage_%d"), Index)));
		Image->SetDesiredSizeOverride(FVector2D(190.0f, 190.0f));
		UTextBlock* Name = MakeText(
			WidgetTree,
			FName(*FString::Printf(TEXT("MetaShopProductName_%d"), Index)),
			FText::GetEmpty(),
			24);
		UTextBlock* Price = MakeText(
			WidgetTree,
			FName(*FString::Printf(TEXT("MetaShopProductPrice_%d"), Index)),
			FText::GetEmpty(),
			22);
		CardBody->AddChildToVerticalBox(Image);
		CardBody->AddChildToVerticalBox(Name);
		CardBody->AddChildToVerticalBox(Price);
		Card->AddChild(CardBody);
		ProductGrid->AddChildToUniformGrid(Card, Index / 4, Index % 4);
		ProductButtons.Add(Card);
		ProductImages.Add(Image);
		ProductNameTexts.Add(Name);
		ProductPriceTexts.Add(Price);
	}

	UBorder* DetailPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MetaShopDetailPanel"));
	DetailPanel->SetBrush(SolidBrush(FLinearColor(0.93f, 0.86f, 0.69f, 0.94f)));
	DetailPanel->SetPadding(FMargin(28.0f));
	UVerticalBox* DetailBody = WidgetTree->ConstructWidget<UVerticalBox>();
	DetailNameText = MakeText(WidgetTree, TEXT("MetaShopDetailName"), FText::GetEmpty(), 34);
	DetailDescriptionText = MakeText(WidgetTree, TEXT("MetaShopDetailDescription"), FText::GetEmpty(), 25);
	DetailPriceText = MakeText(WidgetTree, TEXT("MetaShopDetailPrice"), FText::GetEmpty(), 28);
	DisabledReasonText = MakeText(WidgetTree, TEXT("MetaShopDisabledReason"), FText::GetEmpty(), 22);
	PurchaseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("MetaShopPurchaseButton"));
	PurchaseButton->OnClicked.AddDynamic(this, &UGameXXKMetaShopWidget::HandlePurchaseClicked);
	PurchaseButton->AddChild(MakeText(WidgetTree, TEXT("MetaShopPurchaseText"), FText::FromString(TEXT("购买")), 30));
	DetailBody->AddChildToVerticalBox(DetailNameText);
	DetailBody->AddChildToVerticalBox(DetailDescriptionText);
	DetailBody->AddChildToVerticalBox(DetailPriceText);
	DetailBody->AddChildToVerticalBox(DisabledReasonText);
	DetailBody->AddChildToVerticalBox(PurchaseButton);
	DetailPanel->SetContent(DetailBody);
	AddCanvas(RootCanvas, DetailPanel, FVector2D(1230.0f, 200.0f), FVector2D(540.0f, 730.0f), 2);

	ConfirmOverlay = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MetaShopConfirmOverlay"));
	ConfirmOverlay->SetBrush(SolidBrush(FLinearColor(0.92f, 0.84f, 0.66f, 0.99f)));
	ConfirmOverlay->SetPadding(FMargin(35.0f));
	UVerticalBox* ConfirmBody = WidgetTree->ConstructWidget<UVerticalBox>();
	ConfirmBody->AddChildToVerticalBox(MakeText(WidgetTree, TEXT("MetaShopConfirmPrompt"), FText::FromString(TEXT("确认购买所选商品？")), 32));
	UHorizontalBox* ConfirmActions = WidgetTree->ConstructWidget<UHorizontalBox>();
	UButton* ConfirmButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("MetaShopConfirmButton"));
	ConfirmButton->OnClicked.AddDynamic(this, &UGameXXKMetaShopWidget::HandleConfirmClicked);
	ConfirmButton->AddChild(MakeText(WidgetTree, TEXT("MetaShopConfirmText"), FText::FromString(TEXT("确认")), 27));
	UButton* CancelButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("MetaShopCancelButton"));
	CancelButton->OnClicked.AddDynamic(this, &UGameXXKMetaShopWidget::HandleCancelClicked);
	CancelButton->AddChild(MakeText(WidgetTree, TEXT("MetaShopCancelText"), FText::FromString(TEXT("取消")), 27));
	ConfirmActions->AddChildToHorizontalBox(ConfirmButton);
	ConfirmActions->AddChildToHorizontalBox(CancelButton);
	ConfirmBody->AddChildToVerticalBox(ConfirmActions);
	ConfirmOverlay->SetContent(ConfirmBody);
	ConfirmOverlay->SetVisibility(ESlateVisibility::Collapsed);
	AddCanvas(RootCanvas, ConfirmOverlay, FVector2D(600.0f, 350.0f), FVector2D(720.0f, 330.0f), 10);

	ResultPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MetaShopResultPanel"));
	ResultPanel->SetBrush(SolidBrush(FLinearColor(0.92f, 0.84f, 0.66f, 0.99f)));
	ResultPanel->SetPadding(FMargin(32.0f));
	ResultText = MakeText(WidgetTree, TEXT("MetaShopResultText"), FText::GetEmpty(), 27);
	ResultPanel->SetContent(ResultText);
	ResultPanel->SetVisibility(ESlateVisibility::Collapsed);
	AddCanvas(RootCanvas, ResultPanel, FVector2D(620.0f, 365.0f), FVector2D(680.0f, 300.0f), 11);
}

void UGameXXKMetaShopWidget::RefreshFromState()
{
	BuildProgrammaticLayout();
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || Subsystem->GetRuntimeState().Screen != EGameXXKScreen::Town)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	CurrentProducts = Subsystem->GetMetaShopProducts();
	ApplyProducts(CurrentProducts);
	GoldText->SetText(FText::FromString(FString::Printf(TEXT("永久金币：%d"), Subsystem->GetRuntimeState().PlayerGold)));
	if (SelectedProductId == EGameXXKMetaShopProductId::Invalid && !CurrentProducts.IsEmpty())
	{
		SelectedProductId = CurrentProducts[0].ProductId;
	}
	UpdateSelectedProduct();
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void UGameXXKMetaShopWidget::ApplyProducts(const TArray<FGameXXKMetaShopProductDefinition>& Products)
{
	for (int32 Index = 0; Index < ProductButtons.Num(); ++Index)
	{
		const bool bValid = Products.IsValidIndex(Index);
		ProductButtons[Index]->SetVisibility(bValid ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		if (!bValid)
		{
			continue;
		}
		const FGameXXKMetaShopProductDefinition& Product = Products[Index];
		ProductButtons[Index]->Configure(this, Product.ProductId);
		ProductNameTexts[Index]->SetText(Product.DisplayName);
		ProductPriceTexts[Index]->SetText(FText::FromString(FString::Printf(TEXT("%d 金币"), Product.Price)));
		if (UTexture2D* Texture = Cast<UTexture2D>(Product.IconSoftPath.TryLoad()))
		{
			ProductImages[Index]->SetBrushFromTexture(Texture, true);
		}
	}
}

bool UGameXXKMetaShopWidget::SelectProduct(const EGameXXKMetaShopProductId ProductId)
{
	if (!CurrentProducts.ContainsByPredicate([ProductId](const FGameXXKMetaShopProductDefinition& Product)
		{ return Product.ProductId == ProductId; }))
	{
		return false;
	}
	SelectedProductId = ProductId;
	UpdateSelectedProduct();
	return true;
}

void UGameXXKMetaShopWidget::UpdateSelectedProduct()
{
	const FGameXXKMetaShopProductDefinition* Product = CurrentProducts.FindByPredicate(
		[this](const FGameXXKMetaShopProductDefinition& Candidate)
		{ return Candidate.ProductId == SelectedProductId; });
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Product || !Subsystem)
	{
		DisabledReason = FText::FromString(TEXT("商品不可用。"));
		PurchaseButton->SetIsEnabled(false);
		return;
	}
	DetailNameText->SetText(Product->DisplayName);
	DetailDescriptionText->SetText(BuildProductDescription(*Product));
	DetailPriceText->SetText(FText::FromString(FString::Printf(TEXT("价格：%d 永久金币"), Product->Price)));
	if (Subsystem->PreviewMetaShopPurchase(SelectedProductId, SelectedPreview))
	{
		DisabledReason = FText::GetEmpty();
	}
	else
	{
		DisabledReason = SelectedPreview.Message;
	}
	DisabledReasonText->SetText(DisabledReason);
	PurchaseButton->SetIsEnabled(SelectedPreview.bAvailable);
}

FText UGameXXKMetaShopWidget::BuildProductDescription(const FGameXXKMetaShopProductDefinition& Product) const
{
	if (Product.Kind == EGameXXKMetaShopProductKind::CompanionPack)
	{
		return FText::FromString(TEXT("获得一名永久伙伴。伙伴上限 12 人；满员时进入替换流程，取消候选不退还金币。"));
	}
	return FText::FromString(TEXT("随机获得该套装的武器、头部、护甲、腰带、鞋子或饰品之一。\n普通 70% / 稀有 25% / 珍稀 5%"));
}

bool UGameXXKMetaShopWidget::RequestPurchase()
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || !Subsystem->PreviewMetaShopPurchase(SelectedProductId, SelectedPreview))
	{
		DisabledReason = SelectedPreview.Message;
		DisabledReasonText->SetText(DisabledReason);
		return false;
	}
	ConfirmOverlay->SetVisibility(ESlateVisibility::Visible);
	ResultPanel->SetVisibility(ESlateVisibility::Collapsed);
	return true;
}

bool UGameXXKMetaShopWidget::ConfirmPurchase()
{
	if (!ConfirmOverlay || ConfirmOverlay->GetVisibility() == ESlateVisibility::Collapsed)
	{
		return false;
	}
	ConfirmOverlay->SetVisibility(ESlateVisibility::Collapsed);
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	LastPurchaseResult = FGameXXKMetaShopPurchaseResult();
	if (!Subsystem || !Subsystem->PurchaseMetaShopProduct(SelectedProductId, LastPurchaseResult))
	{
		DisabledReason = LastPurchaseResult.Message;
		DisabledReasonText->SetText(DisabledReason);
		ResultText->SetText(BuildPurchaseResultText());
		ResultPanel->SetVisibility(ESlateVisibility::Visible);
		return false;
	}
	RefreshFromState();
	ResultText->SetText(BuildPurchaseResultText());
	ResultPanel->SetVisibility(ESlateVisibility::Visible);
	if (LastPurchaseResult.CompanionResult.Outcome == EGameXXKCompanionRecruitOutcome::PendingReplacement)
	{
		CompanionReplacementRequestedDelegate.ExecuteIfBound();
	}
	NotifyPlayerFlowStateChanged();
	return true;
}

FText UGameXXKMetaShopWidget::BuildPurchaseResultText() const
{
	if (!LastPurchaseResult.bPurchased)
	{
		return LastPurchaseResult.Message.IsEmpty() ? FText::FromString(TEXT("购买失败。")) : LastPurchaseResult.Message;
	}
	if (!LastPurchaseResult.GeneratedEquipmentId.IsNone())
	{
		FGameXXKEquipmentTooltipSnapshot Tooltip;
		if (const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
			Subsystem && Subsystem->GetEquipmentTooltipSnapshot(
				LastPurchaseResult.GeneratedEquipmentId,
				FGameXXKEquipmentRules::HeroCharacterId(),
				Tooltip))
		{
			return FText::FromString(FString::Printf(
				TEXT("获得装备：%s\n等级 %d，品质 %d"),
				*Tooltip.BaseEquipmentId.ToString(),
				Tooltip.ItemLevel,
				static_cast<int32>(Tooltip.Quality)));
		}
		return FText::FromString(FString::Printf(TEXT("获得装备：%s"), *LastPurchaseResult.GeneratedEquipmentId.ToString()));
	}
	return FText::FromString(FString::Printf(
		TEXT("伙伴结果：%s"),
		*LastPurchaseResult.CompanionResult.Companion.InstanceId.ToString()));
}

bool UGameXXKMetaShopWidget::CancelPurchase()
{
	if (!ConfirmOverlay || ConfirmOverlay->GetVisibility() == ESlateVisibility::Collapsed)
	{
		return false;
	}
	ConfirmOverlay->SetVisibility(ESlateVisibility::Collapsed);
	return true;
}

void UGameXXKMetaShopWidget::CloseMetaShop()
{
	ConfirmOverlay->SetVisibility(ESlateVisibility::Collapsed);
	ResultPanel->SetVisibility(ESlateVisibility::Collapsed);
	SetVisibility(ESlateVisibility::Collapsed);
	CloseRequestedDelegate.ExecuteIfBound();
}

void UGameXXKMetaShopWidget::SetCloseRequestedDelegate(FSimpleDelegate InDelegate)
{
	CloseRequestedDelegate = MoveTemp(InDelegate);
}

void UGameXXKMetaShopWidget::SetCompanionReplacementRequestedDelegate(FSimpleDelegate InDelegate)
{
	CompanionReplacementRequestedDelegate = MoveTemp(InDelegate);
}

void UGameXXKMetaShopWidget::HandlePurchaseClicked() { RequestPurchase(); }
void UGameXXKMetaShopWidget::HandleConfirmClicked() { ConfirmPurchase(); }
void UGameXXKMetaShopWidget::HandleCancelClicked() { CancelPurchase(); }
void UGameXXKMetaShopWidget::HandleCloseClicked() { CloseMetaShop(); }

bool UGameXXKMetaShopWidget::OpenMetaShopForTest()
{
	ConfirmOverlay->SetVisibility(ESlateVisibility::Collapsed);
	ResultPanel->SetVisibility(ESlateVisibility::Collapsed);
	LastPurchaseResult = FGameXXKMetaShopPurchaseResult();
	RefreshFromState();
	return GetVisibility() != ESlateVisibility::Collapsed && CurrentProducts.Num() == 7;
}

bool UGameXXKMetaShopWidget::SelectProductForTest(const EGameXXKMetaShopProductId ProductId) { return SelectProduct(ProductId); }
bool UGameXXKMetaShopWidget::RequestPurchaseForTest() { return RequestPurchase(); }
bool UGameXXKMetaShopWidget::ConfirmPurchaseForTest() { return ConfirmPurchase(); }
bool UGameXXKMetaShopWidget::CancelPurchaseForTest() { return CancelPurchase(); }
int32 UGameXXKMetaShopWidget::GetProductCardCountForTest() const { return ProductButtons.Num(); }
FText UGameXXKMetaShopWidget::GetDisabledReasonForTest() const { return DisabledReason; }
FGameXXKMetaShopPurchaseResult UGameXXKMetaShopWidget::GetLastPurchaseResultForTest() const { return LastPurchaseResult; }
