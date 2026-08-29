#include "Misc/AutomationTest.h"

#include "Dialogue/GameXXKDialogueAsset.h"
#include "Dialogue/GameXXKDialogueCoordinator.h"
#include "Components/TextBlock.h"
#include "Narrative/GameXXKDesktopNarrativeExecutor.h"
#include "UI/GameXXKDesktopNarrativeLayerWidget.h"
#include "UI/GameXXKDesktopNarrativeStagePresenterWidget.h"
#include "UI/GameXXKDialogueHistoryWidget.h"
#include "UI/GameXXKDialoguePanelWidget.h"

#include <type_traits>

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKDesktopNarrativeExecutorTestPrivate
{
	FGameXXKNarrativeCommandDefinition MakeCommand(
		const FName CommandId,
		const FName CommandType,
		const bool bOptional = false)
	{
		FGameXXKNarrativeCommandDefinition Result;
		Result.CommandId = CommandId;
		Result.CommandType = CommandType;
		Result.bOptional = bOptional;
		return Result;
	}

	TArray<FGameXXKNarrativeCommandDefinition> MakeAllCommands()
	{
		TArray<FGameXXKNarrativeCommandDefinition> Commands;

		FGameXXKNarrativeCommandDefinition ShowRole =
			MakeCommand(TEXT("show_hero"), TEXT("stageShowRole"));
		ShowRole.Arguments.Add(TEXT("role"), TEXT("Hero"));
		ShowRole.Arguments.Add(TEXT("slot"), TEXT("Left"));
		Commands.Add(ShowRole);

		FGameXXKNarrativeCommandDefinition SetFacing =
			MakeCommand(TEXT("face_hero_left"), TEXT("stageSetFacing"));
		SetFacing.Arguments.Add(TEXT("role"), TEXT("Hero"));
		SetFacing.Arguments.Add(TEXT("facing"), TEXT("Left"));
		Commands.Add(SetFacing);

		FGameXXKNarrativeCommandDefinition MoveRole =
			MakeCommand(TEXT("move_hero"), TEXT("stageMoveRole"));
		MoveRole.Arguments.Add(TEXT("role"), TEXT("Hero"));
		MoveRole.Arguments.Add(TEXT("slot"), TEXT("Center"));
		Commands.Add(MoveRole);

		FGameXXKNarrativeCommandDefinition PlayAction =
			MakeCommand(TEXT("hero_bow"), TEXT("stagePlayAction"));
		PlayAction.Arguments.Add(TEXT("role"), TEXT("Hero"));
		PlayAction.Arguments.Add(TEXT("resource"), TEXT("Action.Hero.Bow"));
		Commands.Add(PlayAction);

		FGameXXKNarrativeCommandDefinition HideRole =
			MakeCommand(TEXT("hide_hero"), TEXT("stageHideRole"));
		HideRole.Arguments.Add(TEXT("role"), TEXT("Hero"));
		Commands.Add(HideRole);

		FGameXXKNarrativeCommandDefinition ShowProp =
			MakeCommand(TEXT("show_scroll"), TEXT("stageShowProp"));
		ShowProp.Arguments.Add(TEXT("resource"), TEXT("Prop.MapScroll"));
		ShowProp.Arguments.Add(TEXT("slot"), TEXT("Prop"));
		Commands.Add(ShowProp);

		FGameXXKNarrativeCommandDefinition HideProp =
			MakeCommand(TEXT("hide_scroll"), TEXT("stageHideProp"));
		HideProp.Arguments.Add(TEXT("resource"), TEXT("Prop.MapScroll"));
		Commands.Add(HideProp);

		FGameXXKNarrativeCommandDefinition PlayVfx =
			MakeCommand(TEXT("play_wind"), TEXT("stagePlayVfx"));
		PlayVfx.Arguments.Add(TEXT("resource"), TEXT("Vfx.Wind"));
		PlayVfx.Arguments.Add(TEXT("slot"), TEXT("Vfx"));
		Commands.Add(PlayVfx);

		FGameXXKNarrativeCommandDefinition Flash =
			MakeCommand(TEXT("flash_white"), TEXT("stageFlash"));
		Flash.Arguments.Add(TEXT("resource"), TEXT("Vfx.Flash.White"));
		Flash.Arguments.Add(TEXT("slot"), TEXT("Vfx"));
		Commands.Add(Flash);

		FGameXXKNarrativeCommandDefinition Toast =
			MakeCommand(TEXT("toast_ready"), TEXT("showToast"));
		Toast.Arguments.Add(TEXT("resource"), TEXT("Toast.Ready"));
		Commands.Add(Toast);

		FGameXXKNarrativeCommandDefinition Dialogue =
			MakeCommand(TEXT("dialogue_intro"), TEXT("dialogue"));
		Dialogue.Arguments.Add(TEXT("resource"), TEXT("Dialogue.XuXiake.Intro"));
		Commands.Add(Dialogue);

		return Commands;
	}

	FGameXXKDesktopNarrativeResourceDeclarations MakeDeclarations()
	{
		FGameXXKDesktopNarrativeResourceDeclarations Result;
		Result.DeclaredSlots = {
			EGameXXKDesktopNarrativeSlot::Left,
			EGameXXKDesktopNarrativeSlot::Center,
			EGameXXKDesktopNarrativeSlot::Right,
			EGameXXKDesktopNarrativeSlot::Prop,
			EGameXXKDesktopNarrativeSlot::Vfx};
		Result.RoleResourceByRole.Add(TEXT("Hero"), TEXT("Character.Hero"));
		Result.ResourceKindById.Add(
			TEXT("Character.Hero"), EGameXXKDesktopNarrativeResourceKind::RoleVisual);
		Result.ResourceKindById.Add(
			TEXT("Action.Hero.Bow"), EGameXXKDesktopNarrativeResourceKind::Action);
		Result.ResourceKindById.Add(
			TEXT("Prop.MapScroll"), EGameXXKDesktopNarrativeResourceKind::Prop);
		Result.ResourceKindById.Add(
			TEXT("Vfx.Wind"), EGameXXKDesktopNarrativeResourceKind::Vfx);
		Result.ResourceKindById.Add(
			TEXT("Vfx.Flash.White"), EGameXXKDesktopNarrativeResourceKind::Vfx);
		Result.ResourceKindById.Add(
			TEXT("Toast.Ready"), EGameXXKDesktopNarrativeResourceKind::Toast);
		Result.ResourceKindById.Add(
			TEXT("Dialogue.XuXiake.Intro"), EGameXXKDesktopNarrativeResourceKind::Dialogue);
		return Result;
	}

	FGameXXKDesktopNarrativeResourceDeclarations MakeTwoRoleDeclarations(
		const bool bReverseInsertion)
	{
		FGameXXKDesktopNarrativeResourceDeclarations Result;
		Result.DeclaredSlots = {
			EGameXXKDesktopNarrativeSlot::Left,
			EGameXXKDesktopNarrativeSlot::Center,
			EGameXXKDesktopNarrativeSlot::Right};
		Result.ResourceKindById.Add(
			TEXT("Character.Hero"), EGameXXKDesktopNarrativeResourceKind::RoleVisual);
		Result.ResourceKindById.Add(
			TEXT("Character.Guide"), EGameXXKDesktopNarrativeResourceKind::RoleVisual);
		if (bReverseInsertion)
		{
			Result.RoleResourceByRole.Add(TEXT("Guide"), TEXT("Character.Guide"));
			Result.RoleResourceByRole.Add(TEXT("Hero"), TEXT("Character.Hero"));
		}
		else
		{
			Result.RoleResourceByRole.Add(TEXT("Hero"), TEXT("Character.Hero"));
			Result.RoleResourceByRole.Add(TEXT("Guide"), TEXT("Character.Guide"));
		}
		return Result;
	}

	TArray<FGameXXKNarrativeCommandDefinition> MakeTwoRoleCommands()
	{
		FGameXXKNarrativeCommandDefinition ShowHero =
			MakeCommand(TEXT("show_hero_left"), TEXT("stageShowRole"));
		ShowHero.Arguments.Add(TEXT("role"), TEXT("Hero"));
		ShowHero.Arguments.Add(TEXT("slot"), TEXT("Left"));
		FGameXXKNarrativeCommandDefinition ShowGuide =
			MakeCommand(TEXT("show_guide_left"), TEXT("stageShowRole"));
		ShowGuide.Arguments.Add(TEXT("role"), TEXT("Guide"));
		ShowGuide.Arguments.Add(TEXT("slot"), TEXT("Left"));
		return {ShowHero, ShowGuide};
	}

	UGameXXKDesktopNarrativeLayerWidget* MakeLayer()
	{
		UGameXXKDesktopNarrativeLayerWidget* const Layer =
			NewObject<UGameXXKDesktopNarrativeLayerWidget>();
		Layer->ConstructForTest();
		Layer->TakeWidget();
		return Layer;
	}

	UGameXXKDialogueAsset* MakeBlockingDialogueAsset()
	{
		UGameXXKDialogueAsset* const Asset = NewObject<UGameXXKDialogueAsset>();
		Asset->DialogueId = TEXT("Dialogue.Test.StageIsolation");
		Asset->DialogueVersion = 1;
		Asset->EntryNodeId = TEXT("line");
		FGameXXKDialogueNodeDefinition Line;
		Line.NodeId = TEXT("line");
		Line.Type = EGameXXKDialogueNodeType::Line;
		Line.Presentation = EGameXXKDialoguePresentation::DialoguePanel;
		Line.SpeakerId = TEXT("Character.Hero");
		Line.TextId = TEXT("Text.Test.StageIsolation");
		Line.Text = FText::FromString(TEXT("blocking dialogue remains owned"));
		Line.NextNodeId = TEXT("end");
		Asset->Nodes.Add(Line);
		FGameXXKDialogueNodeDefinition End;
		End.NodeId = TEXT("end");
		End.Type = EGameXXKDialogueNodeType::End;
		End.EndOutcomeId = TEXT("Outcome.Done");
		Asset->Nodes.Add(End);
		return Asset;
	}

	bool CompileValidSegment(
		FGameXXKDesktopNarrativeCompiledSegment& OutSegment,
		FString& OutError)
	{
		return FGameXXKDesktopNarrativeCompiler::CompileSegment(
			MakeAllCommands(),
			MakeDeclarations(),
			OutSegment,
			&OutError);
	}

	FGameXXKDesktopNarrativeCompiledSegment MakeSentinelSegment()
	{
		FGameXXKDesktopNarrativeCompiledSegment Result;
		Result.Declarations.DeclaredSlots.Add(EGameXXKDesktopNarrativeSlot::Right);
		FGameXXKDesktopNarrativeCompiledCommand Sentinel;
		Sentinel.CommandId = TEXT("sentinel");
		Sentinel.SourceCommandType = TEXT("showToast");
		Sentinel.Type = EGameXXKDesktopNarrativeCommandType::ShowToast;
		Sentinel.ResourceId = TEXT("Toast.Sentinel");
		Result.Commands.Add(Sentinel.CommandId, Sentinel);
		return Result;
	}

	bool IsSentinelSegment(const FGameXXKDesktopNarrativeCompiledSegment& Segment)
	{
		return Segment.Commands.Num() == 1
			&& Segment.Commands.Contains(TEXT("sentinel"))
			&& Segment.Declarations.DeclaredSlots.Num() == 1
			&& Segment.Declarations.DeclaredSlots.Contains(
				EGameXXKDesktopNarrativeSlot::Right);
	}

	bool ExecuteSegment(
		FGameXXKDesktopNarrativeExecutor& Executor,
		const TArray<FGameXXKNarrativeCommandDefinition>& Commands,
		FAutomationTestBase& Test)
	{
		for (const FGameXXKNarrativeCommandDefinition& Command : Commands)
		{
			const FGameXXKNarrativeCommandResult Result =
				Executor.ExecuteCommand(Command.CommandId);
			if (Command.CommandType == TEXT("stagePlayAction"))
			{
				if (!Test.TestEqual(
					TEXT("stagePlayAction reports Pending"),
					Result.Status,
					EGameXXKNarrativeCommandStatus::Pending)
					|| !Test.TestTrue(TEXT("pending action is driven"), Executor.DrivePendingAction()))
				{
					return false;
				}
			}
			else if (!Test.TestEqual(
				FString::Printf(TEXT("%s completes synchronously"), *Command.CommandType.ToString()),
				Result.Status,
				EGameXXKNarrativeCommandStatus::Completed))
			{
				return false;
			}
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopNarrativeCompilerAtomicValidationTest,
	"GameXXK.DesktopNarrative.Executor.CompilerAtomicValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopNarrativeCompilerAtomicValidationTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKDesktopNarrativeExecutorTestPrivate;
	auto ExpectRejectedWithoutPublishing = [this](
		const TCHAR* Label,
		const TArray<FGameXXKNarrativeCommandDefinition>& Commands,
		const FGameXXKDesktopNarrativeResourceDeclarations& Declarations)
	{
		FGameXXKDesktopNarrativeCompiledSegment Published = MakeSentinelSegment();
		FString Error;
		TestFalse(Label, FGameXXKDesktopNarrativeCompiler::CompileSegment(
			Commands, Declarations, Published, &Error));
		TestTrue(FString::Printf(TEXT("%s leaves OutSegment atomic"), Label),
			IsSentinelSegment(Published));
		TestTrue(FString::Printf(TEXT("%s reports a compiler error"), Label),
			!Error.IsEmpty());
	};

	FGameXXKDesktopNarrativeResourceDeclarations Declarations = MakeDeclarations();
	FGameXXKNarrativeCommandDefinition UnknownRole =
		MakeCommand(TEXT("unknown_role"), TEXT("stageShowRole"));
	UnknownRole.Arguments.Add(TEXT("role"), TEXT("Unknown"));
	UnknownRole.Arguments.Add(TEXT("slot"), TEXT("Left"));
	ExpectRejectedWithoutPublishing(TEXT("unknown role rejects compile"),
		{UnknownRole}, Declarations);

	FGameXXKNarrativeCommandDefinition UndeclaredSlot =
		MakeCommand(TEXT("undeclared_slot"), TEXT("stageMoveRole"));
	UndeclaredSlot.Arguments.Add(TEXT("role"), TEXT("Hero"));
	UndeclaredSlot.Arguments.Add(TEXT("slot"), TEXT("Right"));
	FGameXXKDesktopNarrativeResourceDeclarations MissingRight = Declarations;
	MissingRight.DeclaredSlots.Remove(EGameXXKDesktopNarrativeSlot::Right);
	ExpectRejectedWithoutPublishing(TEXT("undeclared slot rejects compile"),
		{UndeclaredSlot}, MissingRight);

	FGameXXKNarrativeCommandDefinition UnknownResource =
		MakeCommand(TEXT("unknown_prop"), TEXT("stageShowProp"));
	UnknownResource.Arguments.Add(TEXT("resource"), TEXT("Prop.Unknown"));
	UnknownResource.Arguments.Add(TEXT("slot"), TEXT("Prop"));
	ExpectRejectedWithoutPublishing(TEXT("unknown resource rejects compile"),
		{UnknownResource}, Declarations);
	UnknownResource.bOptional = true;
	ExpectRejectedWithoutPublishing(TEXT("invalid optional resource still rejects compile"),
		{UnknownResource}, Declarations);

	FGameXXKNarrativeCommandDefinition WrongKind =
		MakeCommand(TEXT("wrong_kind"), TEXT("stageShowProp"));
	WrongKind.Arguments.Add(TEXT("resource"), TEXT("Action.Hero.Bow"));
	WrongKind.Arguments.Add(TEXT("slot"), TEXT("Prop"));
	ExpectRejectedWithoutPublishing(TEXT("wrong resource kind rejects compile"),
		{WrongKind}, Declarations);

	FGameXXKDesktopNarrativeResourceDeclarations NoneDeclaration = Declarations;
	NoneDeclaration.RoleResourceByRole.Add(NAME_None, TEXT("Character.Hero"));
	ExpectRejectedWithoutPublishing(TEXT("NAME_None declaration rejects compile"),
		{MakeAllCommands()[0]}, NoneDeclaration);

	FGameXXKNarrativeCommandDefinition MissingArgument =
		MakeCommand(TEXT("missing_slot"), TEXT("stageShowRole"));
	MissingArgument.Arguments.Add(TEXT("role"), TEXT("Hero"));
	ExpectRejectedWithoutPublishing(TEXT("missing argument rejects compile"),
		{MissingArgument}, Declarations);

	FGameXXKNarrativeCommandDefinition ExtraneousArgument = MakeAllCommands()[0];
	ExtraneousArgument.CommandId = TEXT("extraneous_argument");
	ExtraneousArgument.Arguments.Add(TEXT("x"), TEXT("120"));
	ExpectRejectedWithoutPublishing(TEXT("extraneous argument rejects compile"),
		{ExtraneousArgument}, Declarations);

	FGameXXKNarrativeCommandDefinition DuplicateA = MakeAllCommands()[0];
	DuplicateA.CommandId = TEXT("duplicate");
	FGameXXKNarrativeCommandDefinition DuplicateB = MakeAllCommands()[1];
	DuplicateB.CommandId = TEXT("duplicate");
	ExpectRejectedWithoutPublishing(TEXT("duplicate command id rejects compile"),
		{DuplicateA, DuplicateB}, Declarations);

	FGameXXKNarrativeCommandDefinition NoneId = MakeAllCommands()[0];
	NoneId.CommandId = NAME_None;
	ExpectRejectedWithoutPublishing(TEXT("NAME_None command id rejects compile"),
		{NoneId}, Declarations);

	FGameXXKDesktopNarrativeCompiledSegment Valid;
	FString Error;
	TestTrue(TEXT("defensive validation fixture compiles"),
		FGameXXKDesktopNarrativeCompiler::CompileSegment(
			{MakeAllCommands()[0]}, Declarations, Valid, &Error));
	FGameXXKDesktopNarrativeCompiledSegment WrongKey = Valid;
	FGameXXKDesktopNarrativeCompiledCommand Embedded =
		WrongKey.Commands.FindAndRemoveChecked(TEXT("show_hero"));
	WrongKey.Commands.Add(TEXT("different_key"), Embedded);
	TestFalse(TEXT("defensive validation rejects key versus embedded id mismatch"),
		FGameXXKDesktopNarrativeCompiler::ValidateSegment(WrongKey, nullptr));
	FGameXXKDesktopNarrativeCompiledSegment WrongType = Valid;
	WrongType.Commands.FindChecked(TEXT("show_hero")).Type =
		EGameXXKDesktopNarrativeCommandType::StageHideRole;
	TestFalse(TEXT("defensive validation rejects source versus typed command mismatch"),
		FGameXXKDesktopNarrativeCompiler::ValidateSegment(WrongType, nullptr));
	FGameXXKDesktopNarrativeCompiledSegment NoneEmbeddedId = Valid;
	NoneEmbeddedId.Commands.FindChecked(TEXT("show_hero")).CommandId = NAME_None;
	TestFalse(TEXT("defensive validation rejects NAME_None embedded id"),
		FGameXXKDesktopNarrativeCompiler::ValidateSegment(NoneEmbeddedId, nullptr));

	FGameXXKDesktopNarrativeCompiledSegment InvalidFacing;
	TestTrue(TEXT("invalid facing fixture compiles before tamper"),
		FGameXXKDesktopNarrativeCompiler::CompileSegment(
			{MakeAllCommands()[1]}, Declarations, InvalidFacing, &Error));
	InvalidFacing.Commands.FindChecked(TEXT("face_hero_left")).Facing =
		static_cast<EGameXXKDesktopNarrativeFacing>(255);
	TestFalse(TEXT("defensive validation rejects forged facing enum"),
		FGameXXKDesktopNarrativeCompiler::ValidateSegment(InvalidFacing, nullptr));

	FGameXXKDesktopNarrativeCompiledSegment InvalidResourceKind = Valid;
	InvalidResourceKind.Declarations.ResourceKindById.Add(
		TEXT("Unused.Invalid"),
		static_cast<EGameXXKDesktopNarrativeResourceKind>(255));
	TestFalse(TEXT("defensive validation rejects forged unused resource kind"),
		FGameXXKDesktopNarrativeCompiler::ValidateSegment(InvalidResourceKind, nullptr));

	FGameXXKDesktopNarrativeCompiledSegment InvalidSlot = Valid;
	InvalidSlot.Declarations.DeclaredSlots.Add(
		static_cast<EGameXXKDesktopNarrativeSlot>(255));
	TestFalse(TEXT("defensive validation rejects forged slot enum"),
		FGameXXKDesktopNarrativeCompiler::ValidateSegment(InvalidSlot, nullptr));

	FGameXXKDesktopNarrativeCompiledSegment InvalidCommandType = Valid;
	InvalidCommandType.Commands.FindChecked(TEXT("show_hero")).Type =
		static_cast<EGameXXKDesktopNarrativeCommandType>(255);
	TestFalse(TEXT("defensive validation rejects forged command enum"),
		FGameXXKDesktopNarrativeCompiler::ValidateSegment(InvalidCommandType, nullptr));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopNarrativeMultiRoleOrderTest,
	"GameXXK.DesktopNarrative.Executor.MultiRoleOrderIndependentReplacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopNarrativeMultiRoleOrderTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKDesktopNarrativeExecutorTestPrivate;
	const TArray<FGameXXKNarrativeCommandDefinition> Commands = MakeTwoRoleCommands();
	auto RunInsertionOrder = [this, &Commands](
		const bool bReverseInsertion,
		FName& OutAfterHero,
		FName& OutAfterReplacement,
		FName& OutAfterReplay)
	{
		FGameXXKDesktopNarrativeCompiledSegment Segment;
		FString Error;
		if (!TestTrue(TEXT("two-role segment compiles"),
			FGameXXKDesktopNarrativeCompiler::CompileSegment(
				Commands,
				MakeTwoRoleDeclarations(bReverseInsertion),
				Segment,
				&Error)))
		{
			return;
		}
		UGameXXKDesktopNarrativeLayerWidget* const Layer = MakeLayer();
		FGameXXKDesktopNarrativeExecutor Executor(Layer, Segment);
		TestEqual(TEXT("Hero show completes with second role still invisible"),
			Executor.ExecuteCommand(TEXT("show_hero_left")).Status,
			EGameXXKNarrativeCommandStatus::Completed);
		UGameXXKDesktopNarrativeStagePresenterWidget* const Left =
			Layer->GetStagePresenter(EGameXXKDesktopNarrativeSlot::Left);
		OutAfterHero = Left ? Left->GetPresentedRoleId() : NAME_None;
		TestEqual(TEXT("invisible default role cannot erase visible Hero"),
			OutAfterHero, FName(TEXT("Hero")));

		TestEqual(TEXT("Guide same-slot replacement completes"),
			Executor.ExecuteCommand(TEXT("show_guide_left")).Status,
			EGameXXKNarrativeCommandStatus::Completed);
		OutAfterReplacement = Left ? Left->GetPresentedRoleId() : NAME_None;
		TestEqual(TEXT("visible Guide deterministically replaces Hero"),
			OutAfterReplacement, FName(TEXT("Guide")));

		Executor.ResetPresentation();
		Executor.ExecuteCommand(TEXT("show_hero_left"));
		Executor.ExecuteCommand(TEXT("show_guide_left"));
		OutAfterReplay = Left ? Left->GetPresentedRoleId() : NAME_None;
		TestEqual(TEXT("reset and replay retain deterministic replacement"),
			OutAfterReplay, FName(TEXT("Guide")));
	};

	FName ForwardHero;
	FName ForwardReplacement;
	FName ForwardReplay;
	FName ReverseHero;
	FName ReverseReplacement;
	FName ReverseReplay;
	RunInsertionOrder(false, ForwardHero, ForwardReplacement, ForwardReplay);
	RunInsertionOrder(true, ReverseHero, ReverseReplacement, ReverseReplay);
	TestEqual(TEXT("Hero presentation is insertion-order independent"),
		ForwardHero, ReverseHero);
	TestEqual(TEXT("replacement is insertion-order independent"),
		ForwardReplacement, ReverseReplacement);
	TestEqual(TEXT("replay is insertion-order independent"),
		ForwardReplay, ReverseReplay);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopNarrativeCommandVocabularyTest,
	"GameXXK.DesktopNarrative.Executor.CommandVocabulary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopNarrativeCommandVocabularyTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKDesktopNarrativeExecutorTestPrivate;
	const TArray<FGameXXKNarrativeCommandDefinition> Commands = MakeAllCommands();
	FGameXXKDesktopNarrativeCompiledSegment Segment;
	FString Error;
	TestTrue(FString::Printf(TEXT("all eleven commands compile: %s"), *Error),
		FGameXXKDesktopNarrativeCompiler::CompileSegment(
			Commands, MakeDeclarations(), Segment, &Error));
	TArray<FString> ValidationErrors;
	TestTrue(TEXT("all eleven compiled commands validate"),
		FGameXXKDesktopNarrativeCompiler::ValidateSegment(Segment, &ValidationErrors));
	TestEqual(TEXT("compiled segment retains all eleven commands"), Segment.Commands.Num(), 11);

	UGameXXKDesktopNarrativeLayerWidget* const Layer = MakeLayer();
	UGameXXKDesktopNarrativeStagePresenterWidget* const LeftPresenter =
		Layer->GetStagePresenter(EGameXXKDesktopNarrativeSlot::Left);
	UGameXXKDesktopNarrativeStagePresenterWidget* const CenterPresenter =
		Layer->GetStagePresenter(EGameXXKDesktopNarrativeSlot::Center);
	UGameXXKDesktopNarrativeStagePresenterWidget* const PropPresenter =
		Layer->GetStagePresenter(EGameXXKDesktopNarrativeSlot::Prop);
	UGameXXKDesktopNarrativeStagePresenterWidget* const VfxPresenter =
		Layer->GetStagePresenter(EGameXXKDesktopNarrativeSlot::Vfx);
	TestNotNull(TEXT("layer owns a real Left stage presenter"), LeftPresenter);
	TestNotNull(TEXT("layer owns a real Center stage presenter"), CenterPresenter);
	TestNotNull(TEXT("layer owns a real Prop stage presenter"), PropPresenter);
	TestNotNull(TEXT("layer owns a real Vfx stage presenter"), VfxPresenter);
	TSharedRef<FGameXXKDesktopNarrativeExecutor> Executor =
		MakeShared<FGameXXKDesktopNarrativeExecutor>(Layer, Segment);
	int32 PendingCompletionCount = 0;
	Executor->SetPendingCompletionDelegate(
		FGameXXKDesktopNarrativePendingCompletion::CreateLambda(
			[&PendingCompletionCount](EGameXXKNarrativeCommandStatus Status)
			{
				if (Status == EGameXXKNarrativeCommandStatus::Completed)
				{
					++PendingCompletionCount;
				}
			}));
	TestEqual(TEXT("stageShowRole completes"),
		Executor->ExecuteCommand(Commands[0].CommandId).Status,
		EGameXXKNarrativeCommandStatus::Completed);
	const FGameXXKDesktopNarrativeRoleState* Hero =
		Executor->GetPresentationState().Roles.Find(TEXT("Hero"));
	TestNotNull(TEXT("declared Hero has typed stage state"), Hero);
	if (Hero)
	{
		TestTrue(TEXT("stageShowRole makes Hero visible"), Hero->bVisible);
		TestEqual(TEXT("stageShowRole places Hero in semantic Left"),
			Hero->Slot, EGameXXKDesktopNarrativeSlot::Left);
	}
	TestTrue(TEXT("stageShowRole mutates the real Left child presenter"),
		LeftPresenter && LeftPresenter->IsRolePresented());
	TestEqual(TEXT("real Left child content resolves the role resource"),
		LeftPresenter ? LeftPresenter->GetRoleResourceId() : NAME_None,
		FName(TEXT("Character.Hero")));

	TestEqual(TEXT("stageSetFacing completes"),
		Executor->ExecuteCommand(Commands[1].CommandId).Status,
		EGameXXKNarrativeCommandStatus::Completed);
	Hero = Executor->GetPresentationState().Roles.Find(TEXT("Hero"));
	TestTrue(TEXT("stageSetFacing stores semantic Left"),
		Hero && Hero->Facing == EGameXXKDesktopNarrativeFacing::Left);
	TestTrue(TEXT("stageSetFacing mutates the real child transform"),
		LeftPresenter
			&& LeftPresenter->GetRoleContentNode()
			&& LeftPresenter->GetRoleContentNode()->GetRenderTransform().Scale.X < 0.0f);

	TestEqual(TEXT("stageMoveRole completes"),
		Executor->ExecuteCommand(Commands[2].CommandId).Status,
		EGameXXKNarrativeCommandStatus::Completed);
	Hero = Executor->GetPresentationState().Roles.Find(TEXT("Hero"));
	TestTrue(TEXT("stageMoveRole targets semantic Center"),
		Hero && Hero->Slot == EGameXXKDesktopNarrativeSlot::Center);
	TestFalse(TEXT("stageMoveRole clears the real Left child"),
		LeftPresenter && LeftPresenter->IsRolePresented());
	TestTrue(TEXT("stageMoveRole mutates the real Center child"),
		CenterPresenter && CenterPresenter->IsRolePresented());

	TestEqual(TEXT("stagePlayAction reports Pending"),
		Executor->ExecuteCommand(Commands[3].CommandId).Status,
		EGameXXKNarrativeCommandStatus::Pending);
	Hero = Executor->GetPresentationState().Roles.Find(TEXT("Hero"));
	TestTrue(TEXT("stagePlayAction marks the declared action pending"),
		Hero
			&& Hero->ActionState == EGameXXKDesktopNarrativeRoleActionState::Pending
			&& Hero->ActiveActionId == TEXT("Action.Hero.Bow"));
	TestTrue(TEXT("stagePlayAction mutates the real role action node"),
		CenterPresenter
			&& CenterPresenter->GetRoleActionState()
				== EGameXXKDesktopNarrativeRoleActionState::Pending
			&& CenterPresenter->GetRoleActionId() == TEXT("Action.Hero.Bow"));
	TestTrue(TEXT("stagePlayAction completes when driven"), Executor->DrivePendingAction());
	Hero = Executor->GetPresentationState().Roles.Find(TEXT("Hero"));
	TestTrue(TEXT("completed stagePlayAction restores idle"),
		Hero && Hero->ActionState == EGameXXKDesktopNarrativeRoleActionState::Idle);
	TestTrue(TEXT("completed action restores the real role child to idle"),
		CenterPresenter
			&& CenterPresenter->GetRoleActionState()
				== EGameXXKDesktopNarrativeRoleActionState::Idle);
	TestEqual(TEXT("pending action completes exactly once"), PendingCompletionCount, 1);

	TestEqual(TEXT("stageHideRole completes"),
		Executor->ExecuteCommand(Commands[4].CommandId).Status,
		EGameXXKNarrativeCommandStatus::Completed);
	Hero = Executor->GetPresentationState().Roles.Find(TEXT("Hero"));
	TestTrue(TEXT("stageHideRole hides Hero"), Hero && !Hero->bVisible);
	TestFalse(TEXT("stageHideRole clears the real Center child"),
		CenterPresenter && CenterPresenter->IsRolePresented());

	TestEqual(TEXT("stageShowProp completes"),
		Executor->ExecuteCommand(Commands[5].CommandId).Status,
		EGameXXKNarrativeCommandStatus::Completed);
	TestEqual(TEXT("stageShowProp records the declared prop"),
		Executor->GetPresentationState().VisiblePropId, FName(TEXT("Prop.MapScroll")));
	TestTrue(TEXT("stageShowProp mutates the real Prop child"),
		PropPresenter && PropPresenter->IsPropPresented());
	TestEqual(TEXT("real Prop child content resolves the prop resource"),
		PropPresenter ? PropPresenter->GetPropResourceId() : NAME_None,
		FName(TEXT("Prop.MapScroll")));

	TestEqual(TEXT("stageHideProp completes"),
		Executor->ExecuteCommand(Commands[6].CommandId).Status,
		EGameXXKNarrativeCommandStatus::Completed);
	TestTrue(TEXT("stageHideProp clears the semantic prop"),
		Executor->GetPresentationState().VisiblePropId.IsNone());
	TestFalse(TEXT("stageHideProp clears the real Prop child"),
		PropPresenter && PropPresenter->IsPropPresented());

	TestEqual(TEXT("stagePlayVfx completes"),
		Executor->ExecuteCommand(Commands[7].CommandId).Status,
		EGameXXKNarrativeCommandStatus::Completed);
	TestEqual(TEXT("stagePlayVfx records declared VFX"),
		Executor->GetPresentationState().ActiveVfxId, FName(TEXT("Vfx.Wind")));
	TestTrue(TEXT("stagePlayVfx mutates its dedicated real node"),
		VfxPresenter && VfxPresenter->IsVfxPresented());

	TestEqual(TEXT("stageFlash completes"),
		Executor->ExecuteCommand(Commands[8].CommandId).Status,
		EGameXXKNarrativeCommandStatus::Completed);
	TestEqual(TEXT("stageFlash records declared flash"),
		Executor->GetPresentationState().ActiveFlashId, FName(TEXT("Vfx.Flash.White")));
	TestTrue(TEXT("stageFlash mutates its dedicated real node"),
		VfxPresenter && VfxPresenter->IsFlashPresented());

	TestEqual(TEXT("showToast completes"),
		Executor->ExecuteCommand(Commands[9].CommandId).Status,
		EGameXXKNarrativeCommandStatus::Completed);
	TestEqual(TEXT("showToast records declared toast"),
		Executor->GetPresentationState().ActiveToastId, FName(TEXT("Toast.Ready")));
	TestTrue(TEXT("showToast mutates its dedicated real node"),
		VfxPresenter && VfxPresenter->IsToastPresented());

	TestEqual(TEXT("dialogue completes"),
		Executor->ExecuteCommand(Commands[10].CommandId).Status,
		EGameXXKNarrativeCommandStatus::Completed);
	TestEqual(TEXT("dialogue records declared dialogue"),
		Executor->GetPresentationState().ActiveDialogueId,
		FName(TEXT("Dialogue.XuXiake.Intro")));
	TestTrue(TEXT("typed dialogue cue does not claim DialogueCoordinator's panel"),
		Layer->GetDialoguePanel()
			&& Layer->GetDialoguePanel()->GetVisibility() == ESlateVisibility::Collapsed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopNarrativeValidationTest,
	"GameXXK.DesktopNarrative.Executor.SemanticValidationAndFailSafe",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopNarrativeValidationTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKDesktopNarrativeExecutorTestPrivate;
	FGameXXKNarrativeCommandDefinition RequiredResource = MakeAllCommands()[5];
	RequiredResource.CommandId = TEXT("required_prop");
	FGameXXKDesktopNarrativeCompiledSegment Segment;
	FString Error;
	TestTrue(TEXT("required resource fixture compiles while available"),
		FGameXXKDesktopNarrativeCompiler::CompileSegment(
			{RequiredResource}, MakeDeclarations(), Segment, &Error));
	Segment.Declarations.ResourceKindById.Remove(TEXT("Prop.MapScroll"));
	TArray<FString> ValidationErrors;
	TestFalse(TEXT("defensive validation detects a resource that became unavailable"),
		FGameXXKDesktopNarrativeCompiler::ValidateSegment(Segment, &ValidationErrors));

	UGameXXKDesktopNarrativeLayerWidget* const Layer = MakeLayer();
	TSharedRef<FGameXXKDesktopNarrativeExecutor> Executor =
		MakeShared<FGameXXKDesktopNarrativeExecutor>(Layer, Segment);
	int32 AbortCount = 0;
	Executor->SetAbortRequestedDelegate(
		FGameXXKDesktopNarrativeAbortRequested::CreateLambda(
			[&AbortCount](const FString&) { ++AbortCount; }));
	TestEqual(TEXT("unknown required resource fails safely"),
		Executor->ExecuteCommand(RequiredResource.CommandId).Status,
		EGameXXKNarrativeCommandStatus::Failed);
	TestEqual(TEXT("required unknown resource signals fail-safe exactly once"), AbortCount, 1);
	TestEqual(TEXT("re-entry cannot signal the same fail-safe twice"),
		Executor->ExecuteCommand(RequiredResource.CommandId).Status,
		EGameXXKNarrativeCommandStatus::Failed);
	TestEqual(TEXT("required unknown resource remains exactly once after re-entry"), AbortCount, 1);

	FGameXXKNarrativeCommandDefinition OptionalResource = RequiredResource;
	OptionalResource.CommandId = TEXT("optional_prop");
	OptionalResource.bOptional = true;
	FGameXXKDesktopNarrativeCompiledSegment OptionalSegment;
	TestTrue(TEXT("optional resource fixture compiles while available"),
		FGameXXKDesktopNarrativeCompiler::CompileSegment(
			{OptionalResource}, MakeDeclarations(), OptionalSegment, &Error));
	OptionalSegment.Declarations.ResourceKindById.Remove(TEXT("Prop.MapScroll"));
	TSharedRef<FGameXXKDesktopNarrativeExecutor> OptionalExecutor =
		MakeShared<FGameXXKDesktopNarrativeExecutor>(Layer, OptionalSegment);
	OptionalExecutor->SetAbortRequestedDelegate(
		FGameXXKDesktopNarrativeAbortRequested::CreateLambda(
			[&AbortCount](const FString&) { ++AbortCount; }));
	TestEqual(TEXT("optional resource unavailable at runtime still fails"),
		OptionalExecutor->ExecuteCommand(OptionalResource.CommandId).Status,
		EGameXXKNarrativeCommandStatus::Failed);
	TestEqual(TEXT("optional runtime failure suppresses abort request"), AbortCount, 1);

	FGameXXKNarrativeCommandDefinition CoordinateSmuggling =
		MakeCommand(TEXT("numeric_move"), TEXT("stageMoveRole"));
	CoordinateSmuggling.Arguments.Add(TEXT("role"), TEXT("Hero"));
	CoordinateSmuggling.Arguments.Add(TEXT("slot"), TEXT("Left"));
	CoordinateSmuggling.Arguments.Add(TEXT("x"), TEXT("120"));
	FGameXXKDesktopNarrativeCompiledSegment RejectedSegment;
	TestFalse(TEXT("numeric coordinate payload is not part of the command vocabulary"),
		FGameXXKDesktopNarrativeCompiler::CompileSegment(
			{CoordinateSmuggling}, MakeDeclarations(), RejectedSegment, &Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopNarrativeReplayTest,
	"GameXXK.DesktopNarrative.Executor.ResetReplayIdempotent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopNarrativeReplayTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKDesktopNarrativeExecutorTestPrivate;
	const TArray<FGameXXKNarrativeCommandDefinition> Commands = MakeAllCommands();
	FGameXXKDesktopNarrativeCompiledSegment Segment;
	FString Error;
	TestTrue(TEXT("replay fixture compiles"), CompileValidSegment(Segment, Error));
	UGameXXKDesktopNarrativeLayerWidget* const Layer = MakeLayer();
	TSharedRef<FGameXXKDesktopNarrativeExecutor> Executor =
		MakeShared<FGameXXKDesktopNarrativeExecutor>(Layer, Segment);
	TestTrue(TEXT("first deterministic playback succeeds"),
		ExecuteSegment(*Executor, Commands, *this));
	const FGameXXKDesktopNarrativePresentationState First = Executor->GetPresentationState();
	TestTrue(TEXT("repeating commands without reset is idempotent"),
		ExecuteSegment(*Executor, Commands, *this));
	TestTrue(TEXT("idempotent playback preserves exact typed state"),
		Executor->GetPresentationState() == First);

	Executor->ResetPresentation();
	bool bEveryRealPresenterReset = true;
	for (const EGameXXKDesktopNarrativeSlot Slot : {
		EGameXXKDesktopNarrativeSlot::Left,
		EGameXXKDesktopNarrativeSlot::Center,
		EGameXXKDesktopNarrativeSlot::Right,
		EGameXXKDesktopNarrativeSlot::Prop,
		EGameXXKDesktopNarrativeSlot::Vfx})
	{
		const UGameXXKDesktopNarrativeStagePresenterWidget* const Presenter =
			Layer->GetStagePresenter(Slot);
		bEveryRealPresenterReset = bEveryRealPresenterReset
			&& Presenter
			&& !Presenter->HasAnyPresentation();
	}
	TestTrue(TEXT("reset clears every real semantic child presenter"),
		bEveryRealPresenterReset);
	TestTrue(TEXT("replay after reset succeeds"),
		ExecuteSegment(*Executor, Commands, *this));
	TestTrue(TEXT("reset plus replay reproduces exact typed state"),
		Executor->GetPresentationState() == First);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopNarrativePendingCancellationTest,
	"GameXXK.DesktopNarrative.Executor.PendingCancellation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopNarrativePendingCancellationTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKDesktopNarrativeExecutorTestPrivate;
	const TArray<FGameXXKNarrativeCommandDefinition> Commands = MakeAllCommands();
	FGameXXKDesktopNarrativeCompiledSegment Segment;
	FString Error;
	TestTrue(TEXT("pending fixture compiles"), CompileValidSegment(Segment, Error));
	UGameXXKDesktopNarrativeLayerWidget* const Layer = MakeLayer();
	TSharedRef<FGameXXKDesktopNarrativeExecutor> Executor =
		MakeShared<FGameXXKDesktopNarrativeExecutor>(Layer, Segment);
	int32 CompletionCount = 0;
	Executor->SetPendingCompletionDelegate(
		FGameXXKDesktopNarrativePendingCompletion::CreateLambda(
			[&CompletionCount](EGameXXKNarrativeCommandStatus) { ++CompletionCount; }));
	TestEqual(TEXT("show role completes"), Executor->ExecuteCommand(Commands[0].CommandId).Status,
		EGameXXKNarrativeCommandStatus::Completed);
	TestEqual(TEXT("action reports Pending"), Executor->ExecuteCommand(Commands[3].CommandId).Status,
		EGameXXKNarrativeCommandStatus::Pending);
	const uint64 CancelledGeneration = Executor->GetPendingGeneration();
	TestTrue(TEXT("pending generation is nonzero"), CancelledGeneration > 0);
	Executor->CancelPending();
	const FGameXXKDesktopNarrativeRoleState* Hero =
		Executor->GetPresentationState().Roles.Find(TEXT("Hero"));
	TestNotNull(TEXT("cancelled role remains declared"), Hero);
	if (Hero)
	{
		TestTrue(TEXT("cancel preserves the affected role presentation"), Hero->bVisible);
		TestEqual(TEXT("cancel restores affected role to idle"),
			Hero->ActionState, EGameXXKDesktopNarrativeRoleActionState::Idle);
	}
	TestFalse(TEXT("cancelled generation cannot complete"),
		Executor->CompletePendingAction(CancelledGeneration));
	TestFalse(TEXT("cancel leaves no current pending action to drive"), Executor->DrivePendingAction());
	TestEqual(TEXT("cancel suppresses completion callback"), CompletionCount, 0);

	TestEqual(TEXT("a fresh replay may start the action again"),
		Executor->ExecuteCommand(Commands[3].CommandId).Status,
		EGameXXKNarrativeCommandStatus::Pending);
	const uint64 FreshGeneration = Executor->GetPendingGeneration();
	TestTrue(TEXT("fresh action invalidates the old generation"), FreshGeneration > CancelledGeneration);
	TestFalse(TEXT("stale generation cannot steal fresh completion"),
		Executor->CompletePendingAction(CancelledGeneration));
	TestTrue(TEXT("fresh generation completes when driven"),
		Executor->CompletePendingAction(FreshGeneration));
	TestFalse(TEXT("same generation completes at most once"),
		Executor->CompletePendingAction(FreshGeneration));
	TestEqual(TEXT("fresh completion callback fires exactly once"), CompletionCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopNarrativeDialogueOwnershipIsolationTest,
	"GameXXK.DesktopNarrative.Executor.DialogueOwnershipIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopNarrativeDialogueOwnershipIsolationTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKDesktopNarrativeExecutorTestPrivate;
	UGameXXKDesktopNarrativeLayerWidget* const Layer = MakeLayer();
	UGameXXKDialoguePanelWidget* const Panel = Layer->GetDialoguePanel();
	UGameXXKDialogueHistoryWidget* const History = Layer->GetDialogueHistory();
	if (!TestNotNull(TEXT("dialogue isolation owns the real panel"), Panel)
		|| !TestNotNull(TEXT("dialogue isolation owns the real history"), History))
	{
		return false;
	}
	FGameXXKDialogueSessionState DialogueSession;
	UGameXXKDialogueCoordinator* const DialogueCoordinator =
		NewObject<UGameXXKDialogueCoordinator>();
	DialogueCoordinator->Bind(DialogueSession, Panel, nullptr, History);
	FGameXXKDialogueStartContext Context;
	Context.StoryId = TEXT("Story.Test.StageIsolation");
	Context.TaskId = TEXT("Task.Test.StageIsolation");
	Context.StepId = TEXT("Step.Test.StageIsolation");
	Context.SequenceId = TEXT("Sequence.Test.StageIsolation");
	Context.StageContractId = TEXT("Stage.Desktop.Narrative");
	FString Error;
	if (!TestTrue(TEXT("real DialogueCoordinator starts blocking panel"),
		DialogueCoordinator->StartDialogue(
			*MakeBlockingDialogueAsset(),
			Context,
			FGameXXKDialogueFinished(),
			&Error)))
	{
		return false;
	}
	TestTrue(TEXT("real DialogueCoordinator owns blocking presentation"),
		DialogueCoordinator->IsBlockingPresentation());
	const FText SpeakerBefore = Panel->GetSpeakerTextForTest();
	const FText BodyBefore = Panel->GetBodyTextForTest();
	const ESlateVisibility VisibilityBefore = Panel->GetVisibility();
	auto TestDialogueUnchanged = [this, DialogueCoordinator, Panel,
		SpeakerBefore, BodyBefore, VisibilityBefore](const TCHAR* Phase)
	{
		TestTrue(FString::Printf(TEXT("%s keeps dialogue blocking"), Phase),
			DialogueCoordinator->IsBlockingPresentation());
		TestEqual(FString::Printf(TEXT("%s preserves speaker"), Phase),
			Panel->GetSpeakerTextForTest(), SpeakerBefore);
		TestEqual(FString::Printf(TEXT("%s preserves body"), Phase),
			Panel->GetBodyTextForTest(), BodyBefore);
		TestEqual(FString::Printf(TEXT("%s preserves panel visibility"), Phase),
			Panel->GetVisibility(), VisibilityBefore);
	};

	FGameXXKDesktopNarrativeCompiledSegment Segment;
	TestTrue(TEXT("dialogue isolation stage segment compiles"),
		CompileValidSegment(Segment, Error));
	TSharedRef<FGameXXKDesktopNarrativeExecutor> Executor =
		MakeShared<FGameXXKDesktopNarrativeExecutor>(Layer, Segment);
	TestDialogueUnchanged(TEXT("executor construction"));
	Executor->ResetPresentation();
	TestDialogueUnchanged(TEXT("stage reset"));
	TestEqual(TEXT("non-dialogue stage command executes"),
		Executor->ExecuteCommand(TEXT("show_hero")).Status,
		EGameXXKNarrativeCommandStatus::Completed);
	TestDialogueUnchanged(TEXT("non-dialogue stage command"));
	TestEqual(TEXT("typed dialogue cue executes without presenting"),
		Executor->ExecuteCommand(TEXT("dialogue_intro")).Status,
		EGameXXKNarrativeCommandStatus::Completed);
	TestDialogueUnchanged(TEXT("typed dialogue cue"));
	Executor->Shutdown();
	TestDialogueUnchanged(TEXT("executor shutdown"));
	DialogueCoordinator->PauseAndExit();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopNarrativeLifetimeAndApiTest,
	"GameXXK.DesktopNarrative.Executor.LifetimeAndWorldFreeApi",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopNarrativeLifetimeAndApiTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKDesktopNarrativeExecutorTestPrivate;
	static_assert(
		!TIsDerivedFrom<
			FGameXXKDesktopNarrativeExecutor,
			IGameXXKNarrativeCommandExecutor>::Value,
		"Desktop presentation executor must not inherit the gameplay-state command interface.");
	static_assert(!std::is_copy_constructible_v<FGameXXKDesktopNarrativeExecutor>,
		"Desktop narrative executor must have exactly-one ownership semantics.");
	static_assert(!std::is_copy_assignable_v<FGameXXKDesktopNarrativeExecutor>,
		"Desktop narrative executor must not be copy assignable.");
	static_assert(!std::is_move_constructible_v<FGameXXKDesktopNarrativeExecutor>,
		"Desktop narrative executor move construction must be explicitly disabled.");
	static_assert(!std::is_move_assignable_v<FGameXXKDesktopNarrativeExecutor>,
		"Desktop narrative executor move assignment must be explicitly disabled.");
	using FExecuteCommandSignature = FGameXXKNarrativeCommandResult
		(FGameXXKDesktopNarrativeExecutor::*)(FName);
	FExecuteCommandSignature ExecuteCommandSignature =
		&FGameXXKDesktopNarrativeExecutor::ExecuteCommand;
	TestTrue(TEXT("runtime executor exposes only stable compiled-command execution"),
		ExecuteCommandSignature != nullptr);

	FGameXXKDesktopNarrativeCompiledSegment Segment;
	FString Error;
	TestTrue(TEXT("lifetime fixture compiles"), CompileValidSegment(Segment, Error));
	const TArray<FGameXXKNarrativeCommandDefinition> Commands = MakeAllCommands();
	int32 AbortCount = 0;
	TSharedRef<FGameXXKDesktopNarrativeExecutor> NullExecutor =
		MakeShared<FGameXXKDesktopNarrativeExecutor>(nullptr, Segment);
	NullExecutor->SetAbortRequestedDelegate(
		FGameXXKDesktopNarrativeAbortRequested::CreateLambda(
			[&AbortCount](const FString&) { ++AbortCount; }));
	TestEqual(TEXT("null presenter fails safely"),
		NullExecutor->ExecuteCommand(Commands[0].CommandId).Status,
		EGameXXKNarrativeCommandStatus::Failed);
	TestEqual(TEXT("null required presenter requests one fail-safe"), AbortCount, 1);
	NullExecutor->Shutdown();
	TestEqual(TEXT("shutdown rejects re-entry"),
		NullExecutor->ExecuteCommand(Commands[0].CommandId).Status,
		EGameXXKNarrativeCommandStatus::Failed);
	TestEqual(TEXT("shutdown unbinds fail-safe delegate"), AbortCount, 1);

	UGameXXKDesktopNarrativeLayerWidget* Layer = MakeLayer();
	TSharedRef<FGameXXKDesktopNarrativeExecutor> DestroyedExecutor =
		MakeShared<FGameXXKDesktopNarrativeExecutor>(Layer, Segment);
	DestroyedExecutor->SetAbortRequestedDelegate(
		FGameXXKDesktopNarrativeAbortRequested::CreateLambda(
			[&AbortCount](const FString&) { ++AbortCount; }));
	Layer->MarkAsGarbage();
	TestEqual(TEXT("destroyed presenter fails safely"),
		DestroyedExecutor->ExecuteCommand(Commands[0].CommandId).Status,
		EGameXXKNarrativeCommandStatus::Failed);
	TestEqual(TEXT("destroyed required presenter requests one fail-safe"), AbortCount, 2);
	return true;
}

#endif
