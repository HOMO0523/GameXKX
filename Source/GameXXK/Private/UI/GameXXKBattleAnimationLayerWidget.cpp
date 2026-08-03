#include "UI/GameXXKBattleAnimationLayerWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateBrush.h"

namespace
{
	constexpr float BattleDimOpacity = 0.5f;
	const FVector2D UnitImageSize(820.0f, 820.0f);
	const FVector2D ImpactImageSize(360.0f, 360.0f);
	constexpr float UnitHorizontalCenterOffset = 370.0f;

	void ConfigureCenteredCanvasSlot(
		UCanvasPanelSlot* Slot,
		const FVector2D& Size,
		const FVector2D& Position,
		const int32 ZOrder)
	{
		if (!Slot)
		{
			return;
		}
		Slot->SetAnchors(FAnchors(0.5f, 0.5f));
		Slot->SetAlignment(FVector2D(0.5f, 0.5f));
		Slot->SetPosition(Position);
		Slot->SetSize(Size);
		Slot->SetZOrder(ZOrder);
	}
}

TSharedRef<SWidget> UGameXXKBattleAnimationLayerWidget::RebuildWidget()
{
	BuildProgrammaticLayout();
	return Super::RebuildWidget();
}

void UGameXXKBattleAnimationLayerWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	AdvancePresentation(InDeltaTime);
}

void UGameXXKBattleAnimationLayerWidget::NativeDestruct()
{
	ResetPresentation();
	Super::NativeDestruct();
}

void UGameXXKBattleAnimationLayerWidget::QueueCombatSequence(
	const FName AttackerUnitId,
	const bool bAttackerEnemy,
	const FName TargetUnitId,
	const bool bTargetEnemy,
	const bool bTargetDefeated)
{
	FQueuedSequence Combat;
	const FGameXXKBattleAnimationClipDescriptor AttackerClip = FGameXXKBattleAnimationPresentation::ResolveClip(
		AttackerUnitId, bAttackerEnemy, EGameXXKBattleAnimationAction::Attack);
	const FGameXXKBattleAnimationClipDescriptor TargetClip = FGameXXKBattleAnimationPresentation::ResolveClip(
		TargetUnitId, bTargetEnemy, EGameXXKBattleAnimationAction::Hit);
	if (bAttackerEnemy)
	{
		Combat.LeftClip = AttackerClip.IsValid()
			? AttackerClip
			: FGameXXKBattleAnimationPresentation::ResolveClip(AttackerUnitId, bAttackerEnemy, EGameXXKBattleAnimationAction::Idle);
		Combat.RightClip = TargetClip;
	}
	else
	{
		Combat.LeftClip = TargetClip;
		Combat.RightClip = AttackerClip;
	}
	Combat.bHasLeft = Combat.LeftClip.IsValid();
	Combat.bHasRight = Combat.RightClip.IsValid();
	Combat.bShowImpact = true;

	if (bPresentationActive)
	{
		QueuedSequences.Add(Combat);
	}
	else
	{
		StartSequence(Combat);
	}

	if (bTargetDefeated)
	{
		FQueuedSequence Death;
		const FGameXXKBattleAnimationClipDescriptor DeathClip = FGameXXKBattleAnimationPresentation::ResolveClip(
			TargetUnitId, bTargetEnemy, EGameXXKBattleAnimationAction::Death);
		if (bTargetEnemy)
		{
			Death.LeftClip = DeathClip;
			Death.bHasLeft = DeathClip.IsValid();
		}
		else
		{
			Death.RightClip = DeathClip;
			Death.bHasRight = DeathClip.IsValid();
		}
		QueuedSequences.Add(Death);
	}
}

void UGameXXKBattleAnimationLayerWidget::QueueStatusSequence(
	const FName UnitId,
	const bool bEnemy,
	const bool bBuff)
{
	FQueuedSequence Status;
	const FGameXXKBattleAnimationClipDescriptor IdleClip = FGameXXKBattleAnimationPresentation::ResolveClip(
		UnitId, bEnemy, EGameXXKBattleAnimationAction::Idle);
	const FGameXXKBattleAnimationClipDescriptor EffectClip = FGameXXKBattleAnimationPresentation::ResolveGenericClip(
		bBuff ? EGameXXKBattleAnimationAction::Buff : EGameXXKBattleAnimationAction::Debuff);
	if (bEnemy)
	{
		Status.LeftClip = IdleClip;
		Status.RightClip = EffectClip;
	}
	else
	{
		Status.LeftClip = EffectClip;
		Status.RightClip = IdleClip;
	}
	Status.bHasLeft = Status.LeftClip.IsValid();
	Status.bHasRight = Status.RightClip.IsValid();
	if (bPresentationActive)
	{
		QueuedSequences.Add(Status);
	}
	else
	{
		StartSequence(Status);
	}
}

void UGameXXKBattleAnimationLayerWidget::BuildProgrammaticLayout()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("BattleAnimationLayerWidgetTree"));
	}
	if (!WidgetTree || RootCanvas || WidgetTree->RootWidget)
	{
		return;
	}

	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("BattleAnimationLayerRoot"));
	WidgetTree->RootWidget = RootCanvas;
	RootCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	Dimmer = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BattleAnimationDimmer"));
	Dimmer->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, BattleDimOpacity));
	Dimmer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (UCanvasPanelSlot* DimmerSlot = RootCanvas->AddChildToCanvas(Dimmer))
	{
		DimmerSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		DimmerSlot->SetOffsets(FMargin(0.0f));
		DimmerSlot->SetZOrder(0);
	}

	AttackerImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("BattleAnimationLeftUnit"));
	TargetImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("BattleAnimationRightUnit"));
	ImpactImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("BattleAnimationImpact"));
	for (UImage* Image : {AttackerImage.Get(), TargetImage.Get(), ImpactImage.Get()})
	{
		Image->SetVisibility(ESlateVisibility::Hidden);
	}
	ConfigureCenteredCanvasSlot(
		RootCanvas->AddChildToCanvas(AttackerImage), UnitImageSize, FVector2D(-UnitHorizontalCenterOffset, 0.0f), 1);
	ConfigureCenteredCanvasSlot(
		RootCanvas->AddChildToCanvas(TargetImage), UnitImageSize, FVector2D(UnitHorizontalCenterOffset, 0.0f), 1);
	ConfigureCenteredCanvasSlot(
		RootCanvas->AddChildToCanvas(ImpactImage), ImpactImageSize, FVector2D::ZeroVector, 2);
	SetVisibility(ESlateVisibility::Collapsed);
}

void UGameXXKBattleAnimationLayerWidget::StartSequence(const FQueuedSequence& Sequence)
{
	// A programmatically constructed child can be queued in the same frame that
	// its owning Board is built, before Slate asks the child to RebuildWidget().
	BuildProgrammaticLayout();
	if (!AttackerImage || !TargetImage || !ImpactImage)
	{
		QueuedSequences.Insert(Sequence, 0);
		return;
	}
	ActiveSequence = Sequence;
	PresentationElapsed = 0.0f;
	ImpactFrame = 0;
	bImpactStarted = false;
	bPresentationActive = true;
	PresentationDuration = FMath::Max(
		FGameXXKBattleAnimationPresentation::GetRuntimeDuration(ActiveSequence.LeftClip),
		FGameXXKBattleAnimationPresentation::GetRuntimeDuration(ActiveSequence.RightClip));
	PresentationDuration = FMath::Max(PresentationDuration, 0.01f);
	LeftTexture = LoadClipTexture(ActiveSequence.LeftClip);
	RightTexture = LoadClipTexture(ActiveSequence.RightClip);
	ImpactClip = FGameXXKBattleAnimationPresentation::ResolveGenericClip(EGameXXKBattleAnimationAction::Impact);
	ImpactTexture = ActiveSequence.bShowImpact ? LoadClipTexture(ImpactClip) : nullptr;
	AttackerImage->SetVisibility(ActiveSequence.bHasLeft ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Hidden);
	TargetImage->SetVisibility(ActiveSequence.bHasRight ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Hidden);
	ImpactImage->SetVisibility(ESlateVisibility::Hidden);
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	AdvancePresentation(0.0f);
}

void UGameXXKBattleAnimationLayerWidget::StartNextSequence()
{
	if (QueuedSequences.IsEmpty())
	{
		FinishPresentation();
		return;
	}
	const FQueuedSequence Next = QueuedSequences[0];
	QueuedSequences.RemoveAt(0);
	StartSequence(Next);
}

void UGameXXKBattleAnimationLayerWidget::AdvancePresentation(const float DeltaSeconds)
{
	if (!bPresentationActive)
	{
		return;
	}
	PresentationElapsed += FMath::Max(0.0f, DeltaSeconds);
	if (PresentationElapsed >= PresentationDuration)
	{
		StartNextSequence();
		return;
	}

	if (ActiveSequence.bHasLeft)
	{
		UpdateImageFrame(
			AttackerImage,
			LeftTexture,
			ActiveSequence.LeftClip,
			FGameXXKBattleAnimationPresentation::CalculateFrameIndex(ActiveSequence.LeftClip, PresentationElapsed, false));
	}
	if (ActiveSequence.bHasRight)
	{
		UpdateImageFrame(
			TargetImage,
			RightTexture,
			ActiveSequence.RightClip,
			FGameXXKBattleAnimationPresentation::CalculateFrameIndex(ActiveSequence.RightClip, PresentationElapsed, false));
	}

	const float ImpactStart = FGameXXKBattleAnimationPresentation::GetImpactRuntimeSeconds();
	const float ImpactElapsed = PresentationElapsed - ImpactStart;
	const float ImpactDuration = FGameXXKBattleAnimationPresentation::GetRuntimeDuration(ImpactClip);
	if (ActiveSequence.bShowImpact && ImpactElapsed >= 0.0f && ImpactElapsed < ImpactDuration)
	{
		if (!bImpactStarted)
		{
			bImpactStarted = true;
		}
		ImpactFrame = FGameXXKBattleAnimationPresentation::CalculateFrameIndex(ImpactClip, ImpactElapsed, false);
		UpdateImageFrame(ImpactImage, ImpactTexture, ImpactClip, ImpactFrame);
		ImpactImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	else
	{
		ImpactImage->SetVisibility(ESlateVisibility::Hidden);
	}
}

void UGameXXKBattleAnimationLayerWidget::FinishPresentation()
{
	bPresentationActive = false;
	bImpactStarted = false;
	PresentationElapsed = 0.0f;
	PresentationDuration = 0.0f;
	if (AttackerImage)
	{
		AttackerImage->SetBrush(FSlateBrush());
		AttackerImage->SetVisibility(ESlateVisibility::Hidden);
	}
	if (TargetImage)
	{
		TargetImage->SetBrush(FSlateBrush());
		TargetImage->SetVisibility(ESlateVisibility::Hidden);
	}
	if (ImpactImage)
	{
		ImpactImage->SetBrush(FSlateBrush());
		ImpactImage->SetVisibility(ESlateVisibility::Hidden);
	}
	LeftTexture = nullptr;
	RightTexture = nullptr;
	ImpactTexture = nullptr;
	SetVisibility(ESlateVisibility::Collapsed);
}

void UGameXXKBattleAnimationLayerWidget::ResetPresentation()
{
	QueuedSequences.Reset();
	FinishPresentation();
	ActiveSequence = FQueuedSequence();
	ImpactClip = FGameXXKBattleAnimationClipDescriptor();
	ImpactFrame = 0;
}

void UGameXXKBattleAnimationLayerWidget::UpdateImageFrame(
	UImage* Image,
	UTexture2D* Texture,
	const FGameXXKBattleAnimationClipDescriptor& Clip,
	const int32 FrameIndex)
{
	if (!Image || !Texture || !Clip.IsValid())
	{
		return;
	}
	FSlateBrush Brush;
	Brush.SetResourceObject(Texture);
	Brush.ImageSize = FVector2D(512.0f, 512.0f);
	Brush.DrawAs = ESlateBrushDrawType::Image;
	Brush.SetUVRegion(FGameXXKBattleAnimationPresentation::CalculateUvRegion(Clip, FrameIndex));
	Image->SetBrush(Brush);
}

UTexture2D* UGameXXKBattleAnimationLayerWidget::LoadClipTexture(
	const FGameXXKBattleAnimationClipDescriptor& Clip) const
{
	return Clip.IsValid() ? Cast<UTexture2D>(Clip.TexturePath.TryLoad()) : nullptr;
}

UCanvasPanel* UGameXXKBattleAnimationLayerWidget::GetRootCanvasForTest() const { return RootCanvas; }
UBorder* UGameXXKBattleAnimationLayerWidget::GetDimmerForTest() const { return Dimmer; }
UImage* UGameXXKBattleAnimationLayerWidget::GetAttackerImageForTest() const { return AttackerImage; }
UImage* UGameXXKBattleAnimationLayerWidget::GetTargetImageForTest() const { return TargetImage; }
UImage* UGameXXKBattleAnimationLayerWidget::GetImpactImageForTest() const { return ImpactImage; }
float UGameXXKBattleAnimationLayerWidget::GetDimOpacityForTest() const { return BattleDimOpacity; }
FVector2D UGameXXKBattleAnimationLayerWidget::GetUnitImageSizeForTest() const { return UnitImageSize; }
FVector2D UGameXXKBattleAnimationLayerWidget::GetImpactAnchorForTest() const { return FVector2D(0.5f, 0.5f); }
bool UGameXXKBattleAnimationLayerWidget::IsPresentationActiveForTest() const { return bPresentationActive; }
bool UGameXXKBattleAnimationLayerWidget::IsImpactVisibleForTest() const
{
	return ImpactImage && ImpactImage->GetVisibility() != ESlateVisibility::Hidden;
}
int32 UGameXXKBattleAnimationLayerWidget::GetImpactFrameForTest() const { return ImpactFrame; }
int32 UGameXXKBattleAnimationLayerWidget::GetQueuedSequenceCountForTest() const { return QueuedSequences.Num(); }
void UGameXXKBattleAnimationLayerWidget::AdvanceAnimationForTest(const float DeltaSeconds) { AdvancePresentation(DeltaSeconds); }
