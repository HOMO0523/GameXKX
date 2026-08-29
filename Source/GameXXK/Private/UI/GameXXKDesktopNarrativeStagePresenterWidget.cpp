#include "UI/GameXXKDesktopNarrativeStagePresenterWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"

TSharedRef<SWidget> UGameXXKDesktopNarrativeStagePresenterWidget::RebuildWidget()
{
	BuildProgrammaticLayout();
	return Super::RebuildWidget();
}

void UGameXXKDesktopNarrativeStagePresenterWidget::ConfigureSlot(
	const EGameXXKDesktopNarrativeSlot InSlot)
{
	SemanticSlot = InSlot;
}

void UGameXXKDesktopNarrativeStagePresenterWidget::PresentRole(
	const FName RoleId,
	const FName ResourceId,
	const EGameXXKDesktopNarrativeFacing Facing,
	const EGameXXKDesktopNarrativeRoleActionState ActionState,
	const FName ActionId)
{
	if (Facing != EGameXXKDesktopNarrativeFacing::Left
		&& Facing != EGameXXKDesktopNarrativeFacing::Right)
	{
		return;
	}
	BuildProgrammaticLayout();
	PresentedRoleId = RoleId;
	RoleResourceId = ResourceId;
	RoleFacing = Facing;
	RoleActionState = ActionState;
	RoleActionId = ActionState == EGameXXKDesktopNarrativeRoleActionState::Pending
		? ActionId
		: NAME_None;
	RefreshPresentation();
}

void UGameXXKDesktopNarrativeStagePresenterWidget::ClearRole()
{
	PresentedRoleId = NAME_None;
	RoleResourceId = NAME_None;
	RoleActionId = NAME_None;
	RoleFacing = EGameXXKDesktopNarrativeFacing::Right;
	RoleActionState = EGameXXKDesktopNarrativeRoleActionState::Idle;
	RefreshPresentation();
}

void UGameXXKDesktopNarrativeStagePresenterWidget::PresentProp(const FName ResourceId)
{
	BuildProgrammaticLayout();
	PropResourceId = ResourceId;
	RefreshPresentation();
}

void UGameXXKDesktopNarrativeStagePresenterWidget::PresentVfx(const FName ResourceId)
{
	BuildProgrammaticLayout();
	VfxResourceId = ResourceId;
	RefreshPresentation();
}

void UGameXXKDesktopNarrativeStagePresenterWidget::PresentFlash(const FName ResourceId)
{
	BuildProgrammaticLayout();
	FlashResourceId = ResourceId;
	RefreshPresentation();
}

void UGameXXKDesktopNarrativeStagePresenterWidget::PresentToast(const FName ResourceId)
{
	BuildProgrammaticLayout();
	ToastResourceId = ResourceId;
	RefreshPresentation();
}

void UGameXXKDesktopNarrativeStagePresenterWidget::ResetPresentation()
{
	BuildProgrammaticLayout();
	PresentedRoleId = NAME_None;
	RoleResourceId = NAME_None;
	RoleActionId = NAME_None;
	PropResourceId = NAME_None;
	VfxResourceId = NAME_None;
	FlashResourceId = NAME_None;
	ToastResourceId = NAME_None;
	RoleFacing = EGameXXKDesktopNarrativeFacing::Right;
	RoleActionState = EGameXXKDesktopNarrativeRoleActionState::Idle;
	RefreshPresentation();
}

bool UGameXXKDesktopNarrativeStagePresenterWidget::IsPresentationReady() const
{
	return RootBox
		&& RoleContentNode
		&& RoleActionNode
		&& PropContentNode
		&& VfxContentNode
		&& FlashContentNode
		&& ToastContentNode
		&& RoleContentNode->GetParent() == RootBox
		&& RoleActionNode->GetParent() == RootBox
		&& PropContentNode->GetParent() == RootBox
		&& VfxContentNode->GetParent() == RootBox
		&& FlashContentNode->GetParent() == RootBox
		&& ToastContentNode->GetParent() == RootBox;
}

bool UGameXXKDesktopNarrativeStagePresenterWidget::HasAnyPresentation() const
{
	return !PresentedRoleId.IsNone()
		|| !PropResourceId.IsNone()
		|| !VfxResourceId.IsNone()
		|| !FlashResourceId.IsNone()
		|| !ToastResourceId.IsNone();
}

void UGameXXKDesktopNarrativeStagePresenterWidget::BuildProgrammaticLayout()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("DesktopNarrativeStagePresenterTree"));
	}
	if (!WidgetTree)
	{
		return;
	}
	if (RootBox
		&& RoleContentNode
		&& RoleActionNode
		&& PropContentNode
		&& VfxContentNode
		&& FlashContentNode
		&& ToastContentNode)
	{
		RefreshPresentation();
		return;
	}

	RootBox = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("StagePresenterRoot"));
	RoleContentNode = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("StageRoleResource"));
	RoleActionNode = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("StageRoleAction"));
	PropContentNode = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("StagePropResource"));
	VfxContentNode = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("StageVfxResource"));
	FlashContentNode = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("StageFlashResource"));
	ToastContentNode = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("StageToastResource"));
	if (!RootBox
		|| !RoleContentNode
		|| !RoleActionNode
		|| !PropContentNode
		|| !VfxContentNode
		|| !FlashContentNode
		|| !ToastContentNode)
	{
		return;
	}
	RootBox->AddChildToVerticalBox(RoleContentNode);
	RootBox->AddChildToVerticalBox(RoleActionNode);
	RootBox->AddChildToVerticalBox(PropContentNode);
	RootBox->AddChildToVerticalBox(VfxContentNode);
	RootBox->AddChildToVerticalBox(FlashContentNode);
	RootBox->AddChildToVerticalBox(ToastContentNode);
	WidgetTree->RootWidget = RootBox;
	RefreshPresentation();
}

void UGameXXKDesktopNarrativeStagePresenterWidget::RefreshPresentation()
{
	if (!RootBox)
	{
		return;
	}
	ApplyResourceNode(RoleContentNode, RoleResourceId);
	if (RoleContentNode)
	{
		FWidgetTransform Transform = RoleContentNode->GetRenderTransform();
		Transform.Scale = FVector2D(
			RoleFacing == EGameXXKDesktopNarrativeFacing::Left ? -1.0f : 1.0f,
			1.0f);
		RoleContentNode->SetRenderTransform(Transform);
	}
	ApplyResourceNode(
		RoleActionNode,
		RoleActionState == EGameXXKDesktopNarrativeRoleActionState::Pending
			? RoleActionId
			: NAME_None);
	ApplyResourceNode(PropContentNode, PropResourceId);
	ApplyResourceNode(VfxContentNode, VfxResourceId);
	ApplyResourceNode(FlashContentNode, FlashResourceId);
	ApplyResourceNode(ToastContentNode, ToastResourceId);
}

void UGameXXKDesktopNarrativeStagePresenterWidget::ApplyResourceNode(
	UTextBlock* Node,
	const FName ResourceId)
{
	if (!Node)
	{
		return;
	}
	Node->SetText(ResourceId.IsNone() ? FText::GetEmpty() : FText::FromName(ResourceId));
	Node->SetVisibility(
		ResourceId.IsNone()
			? ESlateVisibility::Collapsed
			: ESlateVisibility::HitTestInvisible);
}
