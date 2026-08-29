#pragma once

#include "CoreMinimal.h"
#include "Narrative/GameXXKNarrativeCommandExecutor.h"

class UGameXXKDesktopNarrativeLayerWidget;

enum class EGameXXKDesktopNarrativeSlot : uint8
{
	Left,
	Center,
	Right,
	Prop,
	Vfx
};

enum class EGameXXKDesktopNarrativeFacing : uint8
{
	Left,
	Right
};

enum class EGameXXKDesktopNarrativeResourceKind : uint8
{
	RoleVisual,
	Prop,
	Action,
	Vfx,
	Toast,
	Dialogue
};

enum class EGameXXKDesktopNarrativeCommandType : uint8
{
	StageShowRole,
	StageHideRole,
	StageMoveRole,
	StageSetFacing,
	StageShowProp,
	StageHideProp,
	StagePlayAction,
	StagePlayVfx,
	StageFlash,
	ShowToast,
	Dialogue
};

enum class EGameXXKDesktopNarrativeRoleActionState : uint8
{
	Idle,
	Pending
};

struct GAMEXXK_API FGameXXKDesktopNarrativeResourceDeclarations
{
	TSet<EGameXXKDesktopNarrativeSlot> DeclaredSlots;
	TMap<FName, FName> RoleResourceByRole;
	TMap<FName, EGameXXKDesktopNarrativeResourceKind> ResourceKindById;
};

struct GAMEXXK_API FGameXXKDesktopNarrativeCompiledCommand
{
	FName CommandId;
	FName SourceCommandType;
	EGameXXKDesktopNarrativeCommandType Type =
		EGameXXKDesktopNarrativeCommandType::StageShowRole;
	FName RoleId;
	FName ResourceId;
	EGameXXKDesktopNarrativeSlot Slot = EGameXXKDesktopNarrativeSlot::Left;
	EGameXXKDesktopNarrativeFacing Facing = EGameXXKDesktopNarrativeFacing::Right;
	bool bHasSlot = false;
	bool bHasFacing = false;
	bool bOptional = false;
};

struct GAMEXXK_API FGameXXKDesktopNarrativeCompiledSegment
{
	FGameXXKDesktopNarrativeResourceDeclarations Declarations;
	TMap<FName, FGameXXKDesktopNarrativeCompiledCommand> Commands;
};

struct GAMEXXK_API FGameXXKDesktopNarrativeRoleState
{
	FName ResourceId;
	EGameXXKDesktopNarrativeSlot Slot = EGameXXKDesktopNarrativeSlot::Left;
	EGameXXKDesktopNarrativeFacing Facing = EGameXXKDesktopNarrativeFacing::Right;
	EGameXXKDesktopNarrativeRoleActionState ActionState =
		EGameXXKDesktopNarrativeRoleActionState::Idle;
	FName ActiveActionId;
	bool bVisible = false;

	bool operator==(const FGameXXKDesktopNarrativeRoleState& Other) const;
};

struct GAMEXXK_API FGameXXKDesktopNarrativePresentationState
{
	TMap<FName, FGameXXKDesktopNarrativeRoleState> Roles;
	FName VisiblePropId;
	FName ActiveVfxId;
	FName ActiveFlashId;
	FName ActiveToastId;
	FName ActiveDialogueId;

	bool operator==(const FGameXXKDesktopNarrativePresentationState& Other) const;
};

DECLARE_DELEGATE_OneParam(
	FGameXXKDesktopNarrativeAbortRequested,
	const FString&);
DECLARE_DELEGATE_OneParam(
	FGameXXKDesktopNarrativePendingCompletion,
	EGameXXKNarrativeCommandStatus);

/** Converts authored source commands into validated semantic desktop commands. */
class GAMEXXK_API FGameXXKDesktopNarrativeCompiler final
{
public:
	static bool CompileSegment(
		const TArray<FGameXXKNarrativeCommandDefinition>& SourceCommands,
		const FGameXXKDesktopNarrativeResourceDeclarations& Declarations,
		FGameXXKDesktopNarrativeCompiledSegment& OutSegment,
		FString* OutError = nullptr);
	static bool ValidateSegment(
		const FGameXXKDesktopNarrativeCompiledSegment& Segment,
		TArray<FString>* OutErrors = nullptr);
	static bool ValidateCommand(
		const FGameXXKDesktopNarrativeCompiledCommand& Command,
		const FGameXXKDesktopNarrativeResourceDeclarations& Declarations,
		FString& OutError);
};

/**
 * Deterministic executor for an already-compiled desktop narrative segment.
 * Its only presentation dependency is the semantic Task 7 narrative layer.
 */
class GAMEXXK_API FGameXXKDesktopNarrativeExecutor final
{
public:
	FGameXXKDesktopNarrativeExecutor(
		UGameXXKDesktopNarrativeLayerWidget* InLayer,
		const FGameXXKDesktopNarrativeCompiledSegment& InSegment);
	~FGameXXKDesktopNarrativeExecutor();
	FGameXXKDesktopNarrativeExecutor(const FGameXXKDesktopNarrativeExecutor&) = delete;
	FGameXXKDesktopNarrativeExecutor& operator=(const FGameXXKDesktopNarrativeExecutor&) = delete;
	FGameXXKDesktopNarrativeExecutor(FGameXXKDesktopNarrativeExecutor&&) = delete;
	FGameXXKDesktopNarrativeExecutor& operator=(FGameXXKDesktopNarrativeExecutor&&) = delete;

	FGameXXKNarrativeCommandResult ExecuteCommand(FName CompiledCommandId);
	void CancelPending();

	void SetAbortRequestedDelegate(FGameXXKDesktopNarrativeAbortRequested InDelegate);
	void SetPendingCompletionDelegate(FGameXXKDesktopNarrativePendingCompletion InDelegate);
	void ResetPresentation();
	bool DrivePendingAction();
	bool CompletePendingAction(uint64 CompletionGeneration);
	uint64 GetPendingGeneration() const { return PendingGeneration; }
	const FGameXXKDesktopNarrativePresentationState& GetPresentationState() const
	{
		return PresentationState;
	}
	void Shutdown();

private:
	FGameXXKNarrativeCommandResult FailCommand(
		const FString& Error,
		bool bOptional);
	void RestoreBaseline(bool bResetAbortLatch);
	void ApplyPresentationToLayer();
	void OccupyRoleSlot(FName RoleId, EGameXXKDesktopNarrativeSlot Slot);
	void RestorePendingRoleToIdle();

	TWeakObjectPtr<UGameXXKDesktopNarrativeLayerWidget> Layer;
	FGameXXKDesktopNarrativeCompiledSegment Segment;
	FGameXXKDesktopNarrativePresentationState PresentationState;
	FGameXXKDesktopNarrativeAbortRequested AbortRequestedDelegate;
	FGameXXKDesktopNarrativePendingCompletion CompletionDelegate;
	FGameXXKDesktopNarrativePendingCompletion PendingCompletionDelegate;
	FName PendingRoleId;
	FName PendingActionId;
	uint64 GenerationCounter = 0;
	uint64 PendingGeneration = 0;
	bool bAbortRequested = false;
	bool bShutdown = false;
};
