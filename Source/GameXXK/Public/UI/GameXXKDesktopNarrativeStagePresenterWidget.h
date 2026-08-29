#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Narrative/GameXXKDesktopNarrativeExecutor.h"

#include "GameXXKDesktopNarrativeStagePresenterWidget.generated.h"

class UTextBlock;
class UVerticalBox;

/**
 * Programmatic fallback content for one semantic desktop-narrative stage slot.
 * Asset-backed rendering may replace the text content later without changing
 * the executor or semantic slot contract.
 */
UCLASS()
class GAMEXXK_API UGameXXKDesktopNarrativeStagePresenterWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;

	void ConfigureSlot(EGameXXKDesktopNarrativeSlot InSlot);
	void PresentRole(
		FName RoleId,
		FName ResourceId,
		EGameXXKDesktopNarrativeFacing Facing,
		EGameXXKDesktopNarrativeRoleActionState ActionState,
		FName ActionId);
	void ClearRole();
	void PresentProp(FName ResourceId);
	void PresentVfx(FName ResourceId);
	void PresentFlash(FName ResourceId);
	void PresentToast(FName ResourceId);
	void ResetPresentation();

	EGameXXKDesktopNarrativeSlot GetSemanticSlot() const { return SemanticSlot; }
	bool IsPresentationReady() const;
	bool HasAnyPresentation() const;
	bool IsRolePresented() const { return !PresentedRoleId.IsNone(); }
	bool IsPropPresented() const { return !PropResourceId.IsNone(); }
	bool IsVfxPresented() const { return !VfxResourceId.IsNone(); }
	bool IsFlashPresented() const { return !FlashResourceId.IsNone(); }
	bool IsToastPresented() const { return !ToastResourceId.IsNone(); }
	FName GetPresentedRoleId() const { return PresentedRoleId; }
	FName GetRoleResourceId() const { return RoleResourceId; }
	FName GetRoleActionId() const { return RoleActionId; }
	FName GetPropResourceId() const { return PropResourceId; }
	FName GetVfxResourceId() const { return VfxResourceId; }
	FName GetFlashResourceId() const { return FlashResourceId; }
	FName GetToastResourceId() const { return ToastResourceId; }
	EGameXXKDesktopNarrativeFacing GetRoleFacing() const { return RoleFacing; }
	EGameXXKDesktopNarrativeRoleActionState GetRoleActionState() const
	{
		return RoleActionState;
	}
	UTextBlock* GetRoleContentNode() const { return RoleContentNode; }
	UTextBlock* GetRoleActionNode() const { return RoleActionNode; }
	UTextBlock* GetPropContentNode() const { return PropContentNode; }
	UTextBlock* GetVfxContentNode() const { return VfxContentNode; }
	UTextBlock* GetFlashContentNode() const { return FlashContentNode; }
	UTextBlock* GetToastContentNode() const { return ToastContentNode; }

private:
	void BuildProgrammaticLayout();
	void RefreshPresentation();
	static void ApplyResourceNode(UTextBlock* Node, FName ResourceId);

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> RootBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RoleContentNode;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RoleActionNode;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PropContentNode;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> VfxContentNode;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> FlashContentNode;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ToastContentNode;

	EGameXXKDesktopNarrativeSlot SemanticSlot = EGameXXKDesktopNarrativeSlot::Left;
	FName PresentedRoleId;
	FName RoleResourceId;
	FName RoleActionId;
	FName PropResourceId;
	FName VfxResourceId;
	FName FlashResourceId;
	FName ToastResourceId;
	EGameXXKDesktopNarrativeFacing RoleFacing = EGameXXKDesktopNarrativeFacing::Right;
	EGameXXKDesktopNarrativeRoleActionState RoleActionState =
		EGameXXKDesktopNarrativeRoleActionState::Idle;
};
