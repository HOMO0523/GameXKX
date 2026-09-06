#include "UI/GameXXKTrainingSettlementWidget.h"
#include "UI/GameXXKInRunUiStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "MVP/GameXXKMVPPlayerController.h"
#include "TimerManager.h"

namespace
{
	void Place(UCanvasPanel* Canvas, UWidget* Widget, FVector2D Position, FVector2D Size, int32 Z = 0)
	{
		if (auto* Slot = Canvas->AddChildToCanvas(Widget)) { Slot->SetPosition(Position); Slot->SetSize(Size); Slot->SetZOrder(Z); }
	}
	UTextBlock* Text(UWidgetTree* Tree, const FName Name, const FString& Value, int32 Size, bool Display = false, bool Bold = false)
	{
		auto* Block = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Block->SetText(FText::FromString(Value)); Block->SetFont(FGameXXKInRunUiStyle::Font(Size, true, Bold));
		Block->SetColorAndOpacity(FSlateColor(FGameXXKInRunUiStyle::Ink()));
		Block->SetVisibility(ESlateVisibility::HitTestInvisible); Block->SetAutoWrapText(true);
		return Block;
	}
}

TSharedRef<SWidget> UGameXXKTrainingSettlementWidget::RebuildWidget() { EnsureLayout(); return Super::RebuildWidget(); }
void UGameXXKTrainingSettlementWidget::NativeConstruct() { Super::NativeConstruct(); EnsureLayout(); RefreshReceipt(); }

void UGameXXKTrainingSettlementWidget::SetReceipt(const FGameXXKTrainingSettlementReceipt& InReceipt)
{
	if (Receipt.ReceiptId != InReceipt.ReceiptId) bConfirmed = false;
	Receipt = InReceipt; EnsureLayout(); RefreshReceipt();
}

void UGameXXKTrainingSettlementWidget::EnsureLayout()
{
	if (Page) return;
	if (!WidgetTree) WidgetTree = NewObject<UWidgetTree>(this, TEXT("TrainingSettlementTree"));
	auto* Viewport = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("TrainingSettlementViewport"));
	WidgetTree->RootWidget = Viewport;
	auto* Shade = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TrainingSettlementBackdrop"));
	Shade->SetBrushColor(FLinearColor(0.018f, 0.023f, 0.018f, 0.86f));
	auto* ShadeSlot = Viewport->AddChildToCanvas(Shade);
	ShadeSlot->SetAnchors(FAnchors(0, 0, 1, 1)); ShadeSlot->SetOffsets(FMargin(0));
	auto* Scale = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("TrainingSettlementResponsiveScale"));
	Scale->SetStretch(EStretch::ScaleToFit);
	auto* ScaleSlot = Viewport->AddChildToCanvas(Scale);
	ScaleSlot->SetAnchors(FAnchors(0, 0, 1, 1)); ScaleSlot->SetOffsets(FMargin(0)); ScaleSlot->SetZOrder(1);
	auto* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("TrainingSettlementDesignSize"));
	Size->SetWidthOverride(1600); Size->SetHeightOverride(900); Scale->SetContent(Size);
	Page = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("TrainingSettlementPage")); Size->SetContent(Page);
	auto* Paper = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TrainingSettlementPaper"));
	Paper->SetBrush(FGameXXKInRunUiStyle::Paper(FVector2D(1472, 812))); Paper->SetBrushColor(FLinearColor::White);
	Paper->SetVisibility(ESlateVisibility::HitTestInvisible); Place(Page, Paper, {64, 42}, {1472, 812});
	Place(Page, Text(WidgetTree, TEXT("TrainingSettlementTitle"), TEXT("历练告捷"), 44, true), {144, 89}, {870, 72});
	StageText = Text(WidgetTree, TEXT("TrainingSettlementStage"), TEXT(""), 22); Place(Page, StageText, {148, 166}, {960, 40});
	FirstClearText = Text(WidgetTree, TEXT("TrainingSettlementFirstClear"), TEXT(""), 28, true);
	FirstClearText->SetColorAndOpacity(FSlateColor(FGameXXKInRunUiStyle::Vermilion())); Place(Page, FirstClearText, {1248, 110}, {190, 56});
	for (int32 I = 0; I < 3; ++I)
	{
		auto* RewardPaper = WidgetTree->ConstructWidget<UBorder>();
		FSlateBrush Brush; Brush.DrawAs = ESlateBrushDrawType::Box;
		RewardPaper->SetBrush(Brush); RewardPaper->SetBrushColor(FLinearColor(1, .93f, .77f, .38f));
		RewardPaper->SetVisibility(ESlateVisibility::HitTestInvisible); Place(Page, RewardPaper, {144.f + I * 448.f, 240}, {416, 151});
	}
	auto* Coin = WidgetTree->ConstructWidget<UImage>();
	Coin->SetBrushFromTexture(LoadObject<UTexture2D>(nullptr, TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_Ingot.T_MasterV2_Ingot")));
	Coin->SetVisibility(ESlateVisibility::HitTestInvisible); Place(Page, Coin, {169, 284}, {65, 62});
	Place(Page, Text(WidgetTree, TEXT("TrainingSettlementGoldCaption"), TEXT("通关所得金币"), 18), {255, 258}, {287, 30});
	GoldText = Text(WidgetTree, TEXT("TrainingSettlementGold"), TEXT(""), 32, false, true); Place(Page, GoldText, {255, 299}, {287, 47});
	GoldDetail = Text(WidgetTree, TEXT("TrainingSettlementGoldDetail"), TEXT(""), 15); Place(Page, GoldDetail, {166, 357}, {373, 25});
	Place(Page, Text(WidgetTree, TEXT("TrainingSettlementExperienceCaption"), TEXT("每位出战成员经验"), 18), {619, 259}, {355, 30});
	ExperienceText = Text(WidgetTree, TEXT("TrainingSettlementExperience"), TEXT(""), 32, false, true); Place(Page, ExperienceText, {619, 300}, {354, 48});
	Place(Page, Text(WidgetTree, TEXT("TrainingSettlementExperienceHint"), TEXT("成员的实际成长见下方"), 16), {619, 354}, {355, 28});
	ChestImage = WidgetTree->ConstructWidget<UImage>(); ChestImage->SetVisibility(ESlateVisibility::HitTestInvisible); Place(Page, ChestImage, {1064, 272}, {86, 86});
	Place(Page, Text(WidgetTree, TEXT("TrainingSettlementChestCaption"), TEXT("通关宝箱"), 18), {1170, 258}, {272, 30});
	ChestText = Text(WidgetTree, TEXT("TrainingSettlementChest"), TEXT(""), 25, false, true); Place(Page, ChestText, {1170, 304}, {272, 71});
	Place(Page, Text(WidgetTree, TEXT("TrainingSettlementMembersTitle"), TEXT("同行成长"), 27, true), {145, 418}, {450, 43});
	for (int32 I = 0; I < 3; ++I)
	{
		const float X = 145 + I * 448;
		auto* Name = Text(WidgetTree, *FString::Printf(TEXT("TrainingSettlementMember%d"), I), TEXT(""), 23, false, true);
		auto* Level = Text(WidgetTree, NAME_None, TEXT(""), 20);
		auto* Xp = Text(WidgetTree, NAME_None, TEXT(""), 18);
		MemberNames.Add(Name); MemberLevels.Add(Level); MemberExperience.Add(Xp);
		Place(Page, Name, {X, 476}, {242, 37}); Place(Page, Level, {X + 236, 478}, {180, 34}); Place(Page, Xp, {X, 523}, {410, 34});
		auto* Bar = WidgetTree->ConstructWidget<UProgressBar>(); Bar->SetFillColorAndOpacity(FGameXXKInRunUiStyle::Jade());
		Bar->SetVisibility(ESlateVisibility::HitTestInvisible); MemberBars.Add(Bar); Place(Page, Bar, {X, 568}, {400, 8});
	}
	Place(Page, Text(WidgetTree, TEXT("TrainingSettlementStatsTitle"), TEXT("Boss战表现"), 26, true), {145, 610}, {450, 42});
	StatsText = Text(WidgetTree, TEXT("TrainingSettlementStats"), TEXT(""), 19); Place(Page, StatsText, {148, 663}, {1284, 71});
	UnlockText = Text(WidgetTree, TEXT("TrainingSettlementUnlock"), TEXT(""), 20); UnlockText->SetColorAndOpacity(FSlateColor(FGameXXKInRunUiStyle::Jade())); Place(Page, UnlockText, {148, 759}, {850, 60});
	ConfirmButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("TrainingSettlementConfirm"));
	ConfirmButton->SetStyle(FGameXXKInRunUiStyle::Action({306, 65}));
	auto* Label = Text(WidgetTree, NAME_None, TEXT("确认 · 返回挂机"), 24, false, true);
	Label->SetAutoWrapText(false); Label->SetJustification(ETextJustify::Center); Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	ConfirmButton->SetContent(Label); ConfirmButton->OnClicked.AddDynamic(this, &UGameXXKTrainingSettlementWidget::HandleConfirm);
	Place(Page, ConfirmButton, {1131, 747}, {306, 65});
	ErrorText = Text(WidgetTree, TEXT("TrainingSettlementError"), TEXT(""), 16);
	ErrorText->SetColorAndOpacity(FSlateColor(FGameXXKInRunUiStyle::Vermilion())); Place(Page, ErrorText, {148, 812}, {1269, 28});
}

void UGameXXKTrainingSettlementWidget::RefreshReceipt()
{
	if (!Page) return;
	StageText->SetText(Receipt.StageDisplayName); FirstClearText->SetText(FText::FromString(Receipt.bFirstClear ? TEXT("首次通关") : TEXT("再战告捷")));
	GoldText->SetText(FText::FromString(TEXT("+") + FText::AsNumber(Receipt.Gold).ToString()));
	GoldDetail->SetText(FText::FromString(FString::Printf(TEXT("含行旅钱折算金币 +%d"), Receipt.RouteGold)));
	ExperienceText->SetText(FText::FromString(TEXT("+") + FText::AsNumber(Receipt.Experience).ToString()));
	const bool Advanced = Receipt.AdvancedChestCount > 0;
	ChestImage->SetBrushFromTexture(LoadObject<UTexture2D>(nullptr, Advanced
		? TEXT("/Game/GameXXK/UI/Items/T_Item_TrainingAdvancedChest.T_Item_TrainingAdvancedChest")
		: TEXT("/Game/GameXXK/UI/Items/T_Item_TrainingNormalChest.T_Item_TrainingNormalChest")));
	const int32 ChestCount = Receipt.NormalChestCount + Receipt.AdvancedChestCount;
	ChestImage->SetRenderOpacity(ChestCount > 0 ? 1.f : .25f);
	ChestText->SetText(FText::FromString(ChestCount > 0 ? FString::Printf(TEXT("%s宝箱 × %d"), Advanced ? TEXT("高级") : TEXT("普通"), ChestCount) : TEXT("本次未掉落")));
	for (int32 I = 0; I < MemberNames.Num(); ++I)
	{
		if (!Receipt.Members.IsValidIndex(I)) continue;
		const auto& M = Receipt.Members[I]; MemberNames[I]->SetText(M.DisplayName);
		MemberLevels[I]->SetText(FText::FromString(M.LevelAfter > M.LevelBefore ? FString::Printf(TEXT("Lv.%d → %d"), M.LevelBefore, M.LevelAfter) : FString::Printf(TEXT("Lv.%d"), M.LevelAfter)));
		MemberExperience[I]->SetText(FText::FromString(M.LevelAfter >= 100 ? TEXT("已达满级") : FString::Printf(TEXT("经验 +%d    %d / %d"), M.ExperienceGained, M.ExperienceAfter, UGameXXKMVPRules::GetPlayerExperienceRequiredForNextLevel(M.LevelAfter))));
		const int32 Needed = UGameXXKMVPRules::GetPlayerExperienceRequiredForNextLevel(M.LevelAfter);
		MemberBars[I]->SetPercent(Needed > 0 ? FMath::Clamp(static_cast<float>(M.ExperienceAfter) / Needed, 0.f, 1.f) : 1.f);
	}
	const auto& S = Receipt.Stats;
	StatsText->SetText(FText::FromString(S.bComplete
		? FString::Printf(TEXT("%d 回合    ·    主动出牌 %d 张    ·    存活 %d / 3\n敌方损失气血 %lld    我方损失气血 %lld    有效治疗 %lld    获得护甲 %lld"), S.Rounds, S.ActiveCardsPlayed, S.SurvivingPartyUnits, S.PartyDamageDealt, S.PartyDamageTaken, S.HealingDone, S.ArmorGenerated)
		: TEXT("该场战斗的完整统计未记录。")));
	FGameXXKTrainingStageDefinition Next;
	UnlockText->SetText(FText::FromString(FGameXXKTrainingRules::TryGetStageDefinition(Receipt.UnlockedStageId, Next)
		? TEXT("已解锁：") + Next.DisplayName.ToString() : TEXT("本关已通关，可继续挂机历练")));
	ConfirmButton->SetIsEnabled(Receipt.ReceiptId.IsValid() && !bConfirmed);
}

bool UGameXXKTrainingSettlementWidget::ConfirmForTest()
{
	if (bConfirmed || !Receipt.ReceiptId.IsValid()) return false;
	auto* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || !Subsystem->ConfirmTrainingSettlement(Receipt.ReceiptId))
	{
		if (ErrorText) ErrorText->SetText(Subsystem ? Subsystem->GetLastSaveLoadError() : FText::FromString(TEXT("无法读取结算，请重试。")));
		return false;
	}
	bConfirmed = true; ConfirmButton->SetIsEnabled(false);
	if (UWorld* World = GetWorld())
	{
		TWeakObjectPtr<UGameXXKTrainingSettlementWidget> WeakThis(this);
		World->GetTimerManager().SetTimerForNextTick([WeakThis]() { if (WeakThis.IsValid()) WeakThis->NotifyPlayerFlowStateChanged(); });
	}
	else NotifyPlayerFlowStateChanged();
	return true;
}

void UGameXXKTrainingSettlementWidget::HandleConfirm() { ConfirmForTest(); }
