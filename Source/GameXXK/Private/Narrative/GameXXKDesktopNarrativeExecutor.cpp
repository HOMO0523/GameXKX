#include "Narrative/GameXXKDesktopNarrativeExecutor.h"

#include "UI/GameXXKDesktopNarrativeLayerWidget.h"

namespace GameXXKDesktopNarrativeExecutorPrivate
{
	bool SetError(FString* OutError, const FString& Error)
	{
		if (OutError)
		{
			*OutError = Error;
		}
		return false;
	}

	void ClearError(FString* OutError)
	{
		if (OutError)
		{
			OutError->Reset();
		}
	}

	bool ParseCommandType(
		const FName CommandType,
		EGameXXKDesktopNarrativeCommandType& OutType)
	{
		const TPair<FName, EGameXXKDesktopNarrativeCommandType> Types[] = {
			{TEXT("stageShowRole"), EGameXXKDesktopNarrativeCommandType::StageShowRole},
			{TEXT("stageHideRole"), EGameXXKDesktopNarrativeCommandType::StageHideRole},
			{TEXT("stageMoveRole"), EGameXXKDesktopNarrativeCommandType::StageMoveRole},
			{TEXT("stageSetFacing"), EGameXXKDesktopNarrativeCommandType::StageSetFacing},
			{TEXT("stageShowProp"), EGameXXKDesktopNarrativeCommandType::StageShowProp},
			{TEXT("stageHideProp"), EGameXXKDesktopNarrativeCommandType::StageHideProp},
			{TEXT("stagePlayAction"), EGameXXKDesktopNarrativeCommandType::StagePlayAction},
			{TEXT("stagePlayVfx"), EGameXXKDesktopNarrativeCommandType::StagePlayVfx},
			{TEXT("stageFlash"), EGameXXKDesktopNarrativeCommandType::StageFlash},
			{TEXT("showToast"), EGameXXKDesktopNarrativeCommandType::ShowToast},
			{TEXT("dialogue"), EGameXXKDesktopNarrativeCommandType::Dialogue}};
		for (const TPair<FName, EGameXXKDesktopNarrativeCommandType>& Pair : Types)
		{
			if (Pair.Key == CommandType)
			{
				OutType = Pair.Value;
				return true;
			}
		}
		return false;
	}

	bool ParseSlot(const FString& Value, EGameXXKDesktopNarrativeSlot& OutSlot)
	{
		const TPair<const TCHAR*, EGameXXKDesktopNarrativeSlot> Slots[] = {
			{TEXT("Left"), EGameXXKDesktopNarrativeSlot::Left},
			{TEXT("Center"), EGameXXKDesktopNarrativeSlot::Center},
			{TEXT("Right"), EGameXXKDesktopNarrativeSlot::Right},
			{TEXT("Prop"), EGameXXKDesktopNarrativeSlot::Prop},
			{TEXT("Vfx"), EGameXXKDesktopNarrativeSlot::Vfx}};
		for (const TPair<const TCHAR*, EGameXXKDesktopNarrativeSlot>& Pair : Slots)
		{
			if (Value.Equals(Pair.Key, ESearchCase::CaseSensitive))
			{
				OutSlot = Pair.Value;
				return true;
			}
		}
		return false;
	}

	bool ParseFacing(const FString& Value, EGameXXKDesktopNarrativeFacing& OutFacing)
	{
		if (Value == TEXT("Left"))
		{
			OutFacing = EGameXXKDesktopNarrativeFacing::Left;
			return true;
		}
		if (Value == TEXT("Right"))
		{
			OutFacing = EGameXXKDesktopNarrativeFacing::Right;
			return true;
		}
		return false;
	}

	FName SlotName(const EGameXXKDesktopNarrativeSlot Slot)
	{
		switch (Slot)
		{
		case EGameXXKDesktopNarrativeSlot::Left:
			return TEXT("Left");
		case EGameXXKDesktopNarrativeSlot::Center:
			return TEXT("Center");
		case EGameXXKDesktopNarrativeSlot::Right:
			return TEXT("Right");
		case EGameXXKDesktopNarrativeSlot::Prop:
			return TEXT("Prop");
		case EGameXXKDesktopNarrativeSlot::Vfx:
			return TEXT("Vfx");
		default:
			return NAME_None;
		}
	}

	bool IsValidSlot(const EGameXXKDesktopNarrativeSlot Slot)
	{
		return !SlotName(Slot).IsNone();
	}

	bool IsValidFacing(const EGameXXKDesktopNarrativeFacing Facing)
	{
		return Facing == EGameXXKDesktopNarrativeFacing::Left
			|| Facing == EGameXXKDesktopNarrativeFacing::Right;
	}

	bool IsValidResourceKind(const EGameXXKDesktopNarrativeResourceKind Kind)
	{
		switch (Kind)
		{
		case EGameXXKDesktopNarrativeResourceKind::RoleVisual:
		case EGameXXKDesktopNarrativeResourceKind::Prop:
		case EGameXXKDesktopNarrativeResourceKind::Action:
		case EGameXXKDesktopNarrativeResourceKind::Vfx:
		case EGameXXKDesktopNarrativeResourceKind::Toast:
		case EGameXXKDesktopNarrativeResourceKind::Dialogue:
			return true;
		default:
			return false;
		}
	}

	bool IsValidCommandType(const EGameXXKDesktopNarrativeCommandType Type)
	{
		switch (Type)
		{
		case EGameXXKDesktopNarrativeCommandType::StageShowRole:
		case EGameXXKDesktopNarrativeCommandType::StageHideRole:
		case EGameXXKDesktopNarrativeCommandType::StageMoveRole:
		case EGameXXKDesktopNarrativeCommandType::StageSetFacing:
		case EGameXXKDesktopNarrativeCommandType::StageShowProp:
		case EGameXXKDesktopNarrativeCommandType::StageHideProp:
		case EGameXXKDesktopNarrativeCommandType::StagePlayAction:
		case EGameXXKDesktopNarrativeCommandType::StagePlayVfx:
		case EGameXXKDesktopNarrativeCommandType::StageFlash:
		case EGameXXKDesktopNarrativeCommandType::ShowToast:
		case EGameXXKDesktopNarrativeCommandType::Dialogue:
			return true;
		default:
			return false;
		}
	}

	bool IsRoleSlot(const EGameXXKDesktopNarrativeSlot Slot)
	{
		return Slot == EGameXXKDesktopNarrativeSlot::Left
			|| Slot == EGameXXKDesktopNarrativeSlot::Center
			|| Slot == EGameXXKDesktopNarrativeSlot::Right;
	}

	bool ReadNameArgument(
		const FGameXXKNarrativeCommandDefinition& Source,
		const FName Key,
		FName& OutValue,
		FString* OutError)
	{
		const FString* const Value = Source.Arguments.Find(Key);
		if (!Value || Value->IsEmpty())
		{
			return SetError(OutError, FString::Printf(
				TEXT("Desktop narrative command %s requires argument %s."),
				*Source.CommandId.ToString(),
				*Key.ToString()));
		}
		OutValue = FName(**Value);
		return true;
	}

	bool CheckArgumentKeys(
		const FGameXXKNarrativeCommandDefinition& Source,
		const TSet<FName>& Allowed,
		FString* OutError)
	{
		for (const TPair<FName, FString>& Argument : Source.Arguments)
		{
			if (!Allowed.Contains(Argument.Key))
			{
				return SetError(OutError, FString::Printf(
					TEXT("Desktop narrative command %s has unsupported argument %s."),
					*Source.CommandId.ToString(),
					*Argument.Key.ToString()));
			}
		}
		return true;
	}

	bool ValidateResource(
		const FName ResourceId,
		const EGameXXKDesktopNarrativeResourceKind ExpectedKind,
		const FGameXXKDesktopNarrativeResourceDeclarations& Declarations,
		FString& OutError)
	{
		if (ResourceId.IsNone())
		{
			OutError = TEXT("Desktop narrative resource id cannot be NAME_None.");
			return false;
		}
		const EGameXXKDesktopNarrativeResourceKind* const ActualKind =
			Declarations.ResourceKindById.Find(ResourceId);
		if (!ActualKind || *ActualKind != ExpectedKind)
		{
			OutError = FString::Printf(
				TEXT("Desktop narrative resource %s is undeclared or has the wrong semantic kind."),
				*ResourceId.ToString());
			return false;
		}
		return true;
	}

	bool ValidateRole(
		const FName RoleId,
		const FGameXXKDesktopNarrativeResourceDeclarations& Declarations,
		FString& OutError)
	{
		if (RoleId.IsNone())
		{
			OutError = TEXT("Desktop narrative role id cannot be NAME_None.");
			return false;
		}
		const FName* const RoleResource = Declarations.RoleResourceByRole.Find(RoleId);
		if (!RoleResource)
		{
			OutError = FString::Printf(
				TEXT("Desktop narrative role %s is undeclared."),
				*RoleId.ToString());
			return false;
		}
		return ValidateResource(
			*RoleResource,
			EGameXXKDesktopNarrativeResourceKind::RoleVisual,
			Declarations,
			OutError);
	}
}

bool FGameXXKDesktopNarrativeRoleState::operator==(
	const FGameXXKDesktopNarrativeRoleState& Other) const
{
	return ResourceId == Other.ResourceId
		&& Slot == Other.Slot
		&& Facing == Other.Facing
		&& ActionState == Other.ActionState
		&& ActiveActionId == Other.ActiveActionId
		&& bVisible == Other.bVisible;
}

bool FGameXXKDesktopNarrativePresentationState::operator==(
	const FGameXXKDesktopNarrativePresentationState& Other) const
{
	if (VisiblePropId != Other.VisiblePropId
		|| ActiveVfxId != Other.ActiveVfxId
		|| ActiveFlashId != Other.ActiveFlashId
		|| ActiveToastId != Other.ActiveToastId
		|| ActiveDialogueId != Other.ActiveDialogueId
		|| Roles.Num() != Other.Roles.Num())
	{
		return false;
	}
	for (const TPair<FName, FGameXXKDesktopNarrativeRoleState>& Pair : Roles)
	{
		const FGameXXKDesktopNarrativeRoleState* const OtherRole = Other.Roles.Find(Pair.Key);
		if (!OtherRole || !(Pair.Value == *OtherRole))
		{
			return false;
		}
	}
	return true;
}

FGameXXKDesktopNarrativeExecutor::FGameXXKDesktopNarrativeExecutor(
	UGameXXKDesktopNarrativeLayerWidget* InLayer,
	const FGameXXKDesktopNarrativeCompiledSegment& InSegment)
	: Layer(InLayer)
	, Segment(InSegment)
{
	RestoreBaseline(true);
}

FGameXXKDesktopNarrativeExecutor::~FGameXXKDesktopNarrativeExecutor()
{
	Shutdown();
}

bool FGameXXKDesktopNarrativeCompiler::CompileSegment(
	const TArray<FGameXXKNarrativeCommandDefinition>& SourceCommands,
	const FGameXXKDesktopNarrativeResourceDeclarations& Declarations,
	FGameXXKDesktopNarrativeCompiledSegment& OutSegment,
	FString* OutError)
{
	using namespace GameXXKDesktopNarrativeExecutorPrivate;
	FGameXXKDesktopNarrativeCompiledSegment Candidate;
	Candidate.Declarations = Declarations;
	for (const FGameXXKNarrativeCommandDefinition& Source : SourceCommands)
	{
		if (Source.CommandId.IsNone() || Source.CommandType.IsNone())
		{
			return SetError(OutError, TEXT("Desktop narrative commands require stable ids and types."));
		}
		if (Candidate.Commands.Contains(Source.CommandId))
		{
			return SetError(OutError, FString::Printf(
				TEXT("Duplicate desktop narrative command id: %s."),
				*Source.CommandId.ToString()));
		}

		FGameXXKDesktopNarrativeCompiledCommand Compiled;
		Compiled.CommandId = Source.CommandId;
		Compiled.SourceCommandType = Source.CommandType;
		Compiled.bOptional = Source.bOptional;
		if (!ParseCommandType(Source.CommandType, Compiled.Type))
		{
			return SetError(OutError, FString::Printf(
				TEXT("Unsupported desktop narrative command type: %s."),
				*Source.CommandType.ToString()));
		}

		TSet<FName> AllowedArguments;
		switch (Compiled.Type)
		{
		case EGameXXKDesktopNarrativeCommandType::StageShowRole:
		case EGameXXKDesktopNarrativeCommandType::StageMoveRole:
			AllowedArguments = {TEXT("role"), TEXT("slot")};
			if (!ReadNameArgument(Source, TEXT("role"), Compiled.RoleId, OutError))
			{
				return false;
			}
			Compiled.bHasSlot = true;
			break;

		case EGameXXKDesktopNarrativeCommandType::StageHideRole:
			AllowedArguments = {TEXT("role")};
			if (!ReadNameArgument(Source, TEXT("role"), Compiled.RoleId, OutError))
			{
				return false;
			}
			break;

		case EGameXXKDesktopNarrativeCommandType::StageSetFacing:
			AllowedArguments = {TEXT("role"), TEXT("facing")};
			if (!ReadNameArgument(Source, TEXT("role"), Compiled.RoleId, OutError))
			{
				return false;
			}
			Compiled.bHasFacing = true;
			break;

		case EGameXXKDesktopNarrativeCommandType::StageShowProp:
		case EGameXXKDesktopNarrativeCommandType::StagePlayVfx:
		case EGameXXKDesktopNarrativeCommandType::StageFlash:
			AllowedArguments = {TEXT("resource"), TEXT("slot")};
			if (!ReadNameArgument(Source, TEXT("resource"), Compiled.ResourceId, OutError))
			{
				return false;
			}
			Compiled.bHasSlot = true;
			break;

		case EGameXXKDesktopNarrativeCommandType::StageHideProp:
		case EGameXXKDesktopNarrativeCommandType::ShowToast:
		case EGameXXKDesktopNarrativeCommandType::Dialogue:
			AllowedArguments = {TEXT("resource")};
			if (!ReadNameArgument(Source, TEXT("resource"), Compiled.ResourceId, OutError))
			{
				return false;
			}
			break;

		case EGameXXKDesktopNarrativeCommandType::StagePlayAction:
			AllowedArguments = {TEXT("role"), TEXT("resource")};
			if (!ReadNameArgument(Source, TEXT("role"), Compiled.RoleId, OutError)
				|| !ReadNameArgument(Source, TEXT("resource"), Compiled.ResourceId, OutError))
			{
				return false;
			}
			break;
		}
		if (!CheckArgumentKeys(Source, AllowedArguments, OutError))
		{
			return false;
		}
		if (Compiled.bHasSlot)
		{
			const FString* const SlotValue = Source.Arguments.Find(TEXT("slot"));
			if (!SlotValue || !ParseSlot(*SlotValue, Compiled.Slot))
			{
				return SetError(OutError, FString::Printf(
					TEXT("Desktop narrative command %s has an invalid semantic slot."),
					*Source.CommandId.ToString()));
			}
		}
		if (Compiled.bHasFacing)
		{
			const FString* const FacingValue = Source.Arguments.Find(TEXT("facing"));
			if (!FacingValue || !ParseFacing(*FacingValue, Compiled.Facing))
			{
				return SetError(OutError, FString::Printf(
					TEXT("Desktop narrative command %s has an invalid semantic facing."),
					*Source.CommandId.ToString()));
			}
		}
		Candidate.Commands.Add(Compiled.CommandId, MoveTemp(Compiled));
	}
	TArray<FString> ValidationErrors;
	if (!ValidateSegment(Candidate, &ValidationErrors))
	{
		return SetError(
			OutError,
			ValidationErrors.IsEmpty()
				? TEXT("Desktop narrative compiled segment validation failed.")
				: ValidationErrors[0]);
	}
	OutSegment = MoveTemp(Candidate);
	ClearError(OutError);
	return true;
}

bool FGameXXKDesktopNarrativeCompiler::ValidateSegment(
	const FGameXXKDesktopNarrativeCompiledSegment& Segment,
	TArray<FString>* OutErrors)
{
	using namespace GameXXKDesktopNarrativeExecutorPrivate;
	if (OutErrors)
	{
		OutErrors->Reset();
	}
	bool bValid = true;
	auto Reject = [&bValid, OutErrors](FString Error)
	{
		bValid = false;
		if (OutErrors)
		{
			OutErrors->Add(MoveTemp(Error));
		}
	};
	for (const EGameXXKDesktopNarrativeSlot Slot : Segment.Declarations.DeclaredSlots)
	{
		if (!IsValidSlot(Slot))
		{
			Reject(TEXT("Desktop narrative declarations contain an invalid semantic slot."));
		}
	}
	for (const TPair<FName, EGameXXKDesktopNarrativeResourceKind>& Resource :
		Segment.Declarations.ResourceKindById)
	{
		if (Resource.Key.IsNone() || !IsValidResourceKind(Resource.Value))
		{
			Reject(TEXT("Desktop narrative resource declarations contain an invalid id or kind."));
		}
	}
	for (const TPair<FName, FName>& Role : Segment.Declarations.RoleResourceByRole)
	{
		if (Role.Key.IsNone() || Role.Value.IsNone())
		{
			Reject(TEXT("Desktop narrative role declarations cannot use NAME_None."));
			continue;
		}
		FString RoleError;
		if (!ValidateRole(Role.Key, Segment.Declarations, RoleError))
		{
			Reject(MoveTemp(RoleError));
		}
	}
	for (const TPair<FName, FGameXXKDesktopNarrativeCompiledCommand>& Pair : Segment.Commands)
	{
		if (Pair.Key.IsNone()
			|| Pair.Value.CommandId.IsNone()
			|| Pair.Key != Pair.Value.CommandId)
		{
			Reject(TEXT("Desktop narrative command map key must match its non-empty embedded id."));
			continue;
		}
		EGameXXKDesktopNarrativeCommandType ParsedType;
		if (!IsValidCommandType(Pair.Value.Type)
			|| !ParseCommandType(Pair.Value.SourceCommandType, ParsedType)
			|| ParsedType != Pair.Value.Type)
		{
			Reject(FString::Printf(
				TEXT("Desktop narrative command %s has mismatched source and typed command kinds."),
				*Pair.Key.ToString()));
			continue;
		}
		FString Error;
		if (!ValidateCommand(Pair.Value, Segment.Declarations, Error))
		{
			Reject(MoveTemp(Error));
		}
	}
	return bValid;
}

FGameXXKNarrativeCommandResult FGameXXKDesktopNarrativeExecutor::ExecuteCommand(
	const FName CompiledCommandId)
{
	if (bShutdown)
	{
		FGameXXKNarrativeCommandResult Result;
		Result.Status = EGameXXKNarrativeCommandStatus::Failed;
		Result.Error = TEXT("Desktop narrative executor is shut down.");
		return Result;
	}
	const FGameXXKDesktopNarrativeCompiledCommand* const Compiled =
		Segment.Commands.Find(CompiledCommandId);
	if (!Compiled)
	{
		return FailCommand(
			TEXT("Desktop narrative command is absent from the compiled segment."),
			false);
	}
	FString ValidationError;
	if (!FGameXXKDesktopNarrativeCompiler::ValidateCommand(
		*Compiled,
		Segment.Declarations,
		ValidationError))
	{
		return FailCommand(ValidationError, Compiled->bOptional);
	}
	UGameXXKDesktopNarrativeLayerWidget* const LayerWidget = Layer.Get();
	if (!LayerWidget || !LayerWidget->IsPresentationReady())
	{
		return FailCommand(
			TEXT("Desktop narrative presentation layer is unavailable."),
			Compiled->bOptional);
	}

	FGameXXKNarrativeCommandResult Result;
	Result.Status = EGameXXKNarrativeCommandStatus::Completed;
	switch (Compiled->Type)
	{
	case EGameXXKDesktopNarrativeCommandType::StageShowRole:
		OccupyRoleSlot(Compiled->RoleId, Compiled->Slot);
		PresentationState.Roles.FindChecked(Compiled->RoleId).bVisible = true;
		break;

	case EGameXXKDesktopNarrativeCommandType::StageHideRole:
		if (PendingRoleId == Compiled->RoleId)
		{
			CancelPending();
		}
		PresentationState.Roles.FindChecked(Compiled->RoleId).bVisible = false;
		break;

	case EGameXXKDesktopNarrativeCommandType::StageMoveRole:
		OccupyRoleSlot(Compiled->RoleId, Compiled->Slot);
		break;

	case EGameXXKDesktopNarrativeCommandType::StageSetFacing:
		PresentationState.Roles.FindChecked(Compiled->RoleId).Facing = Compiled->Facing;
		break;

	case EGameXXKDesktopNarrativeCommandType::StageShowProp:
		PresentationState.VisiblePropId = Compiled->ResourceId;
		break;

	case EGameXXKDesktopNarrativeCommandType::StageHideProp:
		if (PresentationState.VisiblePropId == Compiled->ResourceId)
		{
			PresentationState.VisiblePropId = NAME_None;
		}
		break;

	case EGameXXKDesktopNarrativeCommandType::StagePlayAction:
		if (PendingGeneration != 0
			&& PendingRoleId == Compiled->RoleId
			&& PendingActionId == Compiled->ResourceId)
		{
			Result.Status = EGameXXKNarrativeCommandStatus::Pending;
			return Result;
		}
		if (PendingGeneration != 0)
		{
			CancelPending();
		}
		PendingRoleId = Compiled->RoleId;
		PendingActionId = Compiled->ResourceId;
		PendingGeneration = ++GenerationCounter;
		PendingCompletionDelegate = CompletionDelegate;
		PresentationState.Roles.FindChecked(Compiled->RoleId).ActionState =
			EGameXXKDesktopNarrativeRoleActionState::Pending;
		PresentationState.Roles.FindChecked(Compiled->RoleId).ActiveActionId =
			Compiled->ResourceId;
		Result.Status = EGameXXKNarrativeCommandStatus::Pending;
		break;

	case EGameXXKDesktopNarrativeCommandType::StagePlayVfx:
		PresentationState.ActiveVfxId = Compiled->ResourceId;
		break;

	case EGameXXKDesktopNarrativeCommandType::StageFlash:
		PresentationState.ActiveFlashId = Compiled->ResourceId;
		break;

	case EGameXXKDesktopNarrativeCommandType::ShowToast:
		PresentationState.ActiveToastId = Compiled->ResourceId;
		break;

	case EGameXXKDesktopNarrativeCommandType::Dialogue:
		PresentationState.ActiveDialogueId = Compiled->ResourceId;
		break;
	}
	ApplyPresentationToLayer();
	return Result;
}

void FGameXXKDesktopNarrativeExecutor::CancelPending()
{
	++GenerationCounter;
	RestorePendingRoleToIdle();
	PendingGeneration = 0;
	PendingRoleId = NAME_None;
	PendingActionId = NAME_None;
	PendingCompletionDelegate.Unbind();
	ApplyPresentationToLayer();
}

void FGameXXKDesktopNarrativeExecutor::SetAbortRequestedDelegate(
	FGameXXKDesktopNarrativeAbortRequested InDelegate)
{
	AbortRequestedDelegate = MoveTemp(InDelegate);
}

void FGameXXKDesktopNarrativeExecutor::SetPendingCompletionDelegate(
	FGameXXKDesktopNarrativePendingCompletion InDelegate)
{
	CompletionDelegate = MoveTemp(InDelegate);
}

void FGameXXKDesktopNarrativeExecutor::ResetPresentation()
{
	if (bShutdown)
	{
		return;
	}
	CancelPending();
	RestoreBaseline(true);
}

bool FGameXXKDesktopNarrativeExecutor::DrivePendingAction()
{
	return PendingGeneration != 0 && CompletePendingAction(PendingGeneration);
}

bool FGameXXKDesktopNarrativeExecutor::CompletePendingAction(
	const uint64 CompletionGeneration)
{
	if (bShutdown
		|| CompletionGeneration == 0
		|| CompletionGeneration != PendingGeneration)
	{
		return false;
	}
	FGameXXKDesktopNarrativePendingCompletion Callback =
		MoveTemp(PendingCompletionDelegate);
	RestorePendingRoleToIdle();
	PendingGeneration = 0;
	PendingRoleId = NAME_None;
	PendingActionId = NAME_None;
	ApplyPresentationToLayer();
	if (Callback.IsBound())
	{
		Callback.Execute(EGameXXKNarrativeCommandStatus::Completed);
	}
	return true;
}

void FGameXXKDesktopNarrativeExecutor::Shutdown()
{
	if (bShutdown)
	{
		return;
	}
	CancelPending();
	bShutdown = true;
	AbortRequestedDelegate.Unbind();
	CompletionDelegate.Unbind();
	PendingCompletionDelegate.Unbind();
	Layer.Reset();
}

bool FGameXXKDesktopNarrativeCompiler::ValidateCommand(
	const FGameXXKDesktopNarrativeCompiledCommand& Command,
	const FGameXXKDesktopNarrativeResourceDeclarations& Declarations,
	FString& OutError)
{
	using namespace GameXXKDesktopNarrativeExecutorPrivate;
	EGameXXKDesktopNarrativeCommandType ParsedType;
	if (Command.CommandId.IsNone()
		|| !IsValidCommandType(Command.Type)
		|| !ParseCommandType(Command.SourceCommandType, ParsedType)
		|| ParsedType != Command.Type)
	{
		OutError = TEXT("Desktop narrative command has an invalid id or mismatched typed kind.");
		return false;
	}
	if (Command.bHasSlot
		&& (!IsValidSlot(Command.Slot)
			|| !Declarations.DeclaredSlots.Contains(Command.Slot)))
	{
		OutError = FString::Printf(
			TEXT("Desktop narrative slot %s is not declared for this segment."),
			*SlotName(Command.Slot).ToString());
		return false;
	}
	switch (Command.Type)
	{
	case EGameXXKDesktopNarrativeCommandType::StageShowRole:
	case EGameXXKDesktopNarrativeCommandType::StageMoveRole:
		if (Command.RoleId.IsNone()
			|| !Command.ResourceId.IsNone()
			|| !Command.bHasSlot
			|| Command.bHasFacing)
		{
			OutError = TEXT("Desktop narrative role placement command has invalid typed fields.");
			return false;
		}
		if (!IsRoleSlot(Command.Slot))
		{
			OutError = TEXT("A narrative role may use only Left, Center, or Right.");
			return false;
		}
		return ValidateRole(Command.RoleId, Declarations, OutError);

	case EGameXXKDesktopNarrativeCommandType::StageHideRole:
		if (Command.RoleId.IsNone()
			|| !Command.ResourceId.IsNone()
			|| Command.bHasSlot
			|| Command.bHasFacing)
		{
			OutError = TEXT("Desktop narrative hide-role command has invalid typed fields.");
			return false;
		}
		return ValidateRole(Command.RoleId, Declarations, OutError);

	case EGameXXKDesktopNarrativeCommandType::StageSetFacing:
		if (Command.RoleId.IsNone()
			|| !Command.ResourceId.IsNone()
			|| Command.bHasSlot
			|| !Command.bHasFacing)
		{
			OutError = TEXT("Desktop narrative facing command has invalid typed fields.");
			return false;
		}
		if (!IsValidFacing(Command.Facing))
		{
			OutError = TEXT("Desktop narrative facing command contains an invalid facing enum.");
			return false;
		}
		return ValidateRole(Command.RoleId, Declarations, OutError);

	case EGameXXKDesktopNarrativeCommandType::StageShowProp:
		if (!Command.RoleId.IsNone()
			|| Command.ResourceId.IsNone()
			|| !Command.bHasSlot
			|| Command.bHasFacing)
		{
			OutError = TEXT("Desktop narrative show-prop command has invalid typed fields.");
			return false;
		}
		if (Command.Slot != EGameXXKDesktopNarrativeSlot::Prop)
		{
			OutError = TEXT("A narrative prop may use only the Prop slot.");
			return false;
		}
		return ValidateResource(
			Command.ResourceId,
			EGameXXKDesktopNarrativeResourceKind::Prop,
			Declarations,
			OutError);

	case EGameXXKDesktopNarrativeCommandType::StageHideProp:
		if (!Command.RoleId.IsNone()
			|| Command.ResourceId.IsNone()
			|| Command.bHasSlot
			|| Command.bHasFacing)
		{
			OutError = TEXT("Desktop narrative hide-prop command has invalid typed fields.");
			return false;
		}
		return ValidateResource(
			Command.ResourceId,
			EGameXXKDesktopNarrativeResourceKind::Prop,
			Declarations,
			OutError);

	case EGameXXKDesktopNarrativeCommandType::StagePlayAction:
		if (Command.RoleId.IsNone()
			|| Command.ResourceId.IsNone()
			|| Command.bHasSlot
			|| Command.bHasFacing)
		{
			OutError = TEXT("Desktop narrative action command has invalid typed fields.");
			return false;
		}
		return ValidateRole(Command.RoleId, Declarations, OutError)
			&& ValidateResource(
				Command.ResourceId,
				EGameXXKDesktopNarrativeResourceKind::Action,
				Declarations,
				OutError);

	case EGameXXKDesktopNarrativeCommandType::StagePlayVfx:
	case EGameXXKDesktopNarrativeCommandType::StageFlash:
		if (!Command.RoleId.IsNone()
			|| Command.ResourceId.IsNone()
			|| !Command.bHasSlot
			|| Command.bHasFacing)
		{
			OutError = TEXT("Desktop narrative visual-effect command has invalid typed fields.");
			return false;
		}
		if (Command.Slot != EGameXXKDesktopNarrativeSlot::Vfx)
		{
			OutError = TEXT("A narrative visual effect may use only the Vfx slot.");
			return false;
		}
		return ValidateResource(
			Command.ResourceId,
			EGameXXKDesktopNarrativeResourceKind::Vfx,
			Declarations,
			OutError);

	case EGameXXKDesktopNarrativeCommandType::ShowToast:
		if (!Command.RoleId.IsNone()
			|| Command.ResourceId.IsNone()
			|| Command.bHasSlot
			|| Command.bHasFacing)
		{
			OutError = TEXT("Desktop narrative toast command has invalid typed fields.");
			return false;
		}
		return ValidateResource(
			Command.ResourceId,
			EGameXXKDesktopNarrativeResourceKind::Toast,
			Declarations,
			OutError);

	case EGameXXKDesktopNarrativeCommandType::Dialogue:
		if (!Command.RoleId.IsNone()
			|| Command.ResourceId.IsNone()
			|| Command.bHasSlot
			|| Command.bHasFacing)
		{
			OutError = TEXT("Desktop narrative dialogue cue has invalid typed fields.");
			return false;
		}
		return ValidateResource(
			Command.ResourceId,
			EGameXXKDesktopNarrativeResourceKind::Dialogue,
			Declarations,
			OutError);
	}
	OutError = TEXT("Desktop narrative command has no typed behavior.");
	return false;
}

FGameXXKNarrativeCommandResult FGameXXKDesktopNarrativeExecutor::FailCommand(
	const FString& Error,
	const bool bOptional)
{
	FGameXXKNarrativeCommandResult Result;
	Result.Status = EGameXXKNarrativeCommandStatus::Failed;
	Result.Error = Error;
	if (bOptional || bShutdown)
	{
		return Result;
	}
	CancelPending();
	RestoreBaseline(false);
	if (!bAbortRequested)
	{
		bAbortRequested = true;
		FGameXXKDesktopNarrativeAbortRequested Callback = AbortRequestedDelegate;
		if (Callback.IsBound())
		{
			Callback.Execute(Error);
		}
	}
	return Result;
}

void FGameXXKDesktopNarrativeExecutor::RestoreBaseline(const bool bResetAbortLatch)
{
	PresentationState = FGameXXKDesktopNarrativePresentationState();
	for (const TPair<FName, FName>& Role : Segment.Declarations.RoleResourceByRole)
	{
		FGameXXKDesktopNarrativeRoleState RoleState;
		RoleState.ResourceId = Role.Value;
		PresentationState.Roles.Add(Role.Key, MoveTemp(RoleState));
	}
	if (bResetAbortLatch)
	{
		bAbortRequested = false;
	}
	ApplyPresentationToLayer();
}

void FGameXXKDesktopNarrativeExecutor::ApplyPresentationToLayer()
{
	UGameXXKDesktopNarrativeLayerWidget* const LayerWidget = Layer.Get();
	if (!LayerWidget)
	{
		return;
	}
	LayerWidget->ResetStagePresentation();
	for (const TPair<FName, FGameXXKDesktopNarrativeRoleState>& Role : PresentationState.Roles)
	{
		if (!Role.Value.bVisible)
		{
			continue;
		}
		LayerWidget->ApplyStageRolePresentation(
			Role.Key,
			Role.Value.ResourceId,
			Role.Value.Slot,
			Role.Value.Facing,
			Role.Value.ActionState,
			Role.Value.ActiveActionId,
			Role.Value.bVisible);
	}
	LayerWidget->ApplyStagePropPresentation(PresentationState.VisiblePropId);
	LayerWidget->ApplyStageVfxPresentation(PresentationState.ActiveVfxId);
	LayerWidget->ApplyStageFlashPresentation(PresentationState.ActiveFlashId);
	LayerWidget->ApplyStageToastPresentation(PresentationState.ActiveToastId);
}

void FGameXXKDesktopNarrativeExecutor::OccupyRoleSlot(
	const FName RoleId,
	const EGameXXKDesktopNarrativeSlot Slot)
{
	for (TPair<FName, FGameXXKDesktopNarrativeRoleState>& Role : PresentationState.Roles)
	{
		if (Role.Key != RoleId && Role.Value.bVisible && Role.Value.Slot == Slot)
		{
			Role.Value.bVisible = false;
		}
	}
	PresentationState.Roles.FindChecked(RoleId).Slot = Slot;
}

void FGameXXKDesktopNarrativeExecutor::RestorePendingRoleToIdle()
{
	if (FGameXXKDesktopNarrativeRoleState* const Role =
		PresentationState.Roles.Find(PendingRoleId))
	{
		Role->ActionState = EGameXXKDesktopNarrativeRoleActionState::Idle;
		Role->ActiveActionId = NAME_None;
	}
}
