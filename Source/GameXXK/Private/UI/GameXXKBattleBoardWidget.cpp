#include "UI/GameXXKBattleBoardWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameXXKMVPRules.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "MVP/GameXXKLevelFlow.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKMVPCommandRouter.h"

namespace
{
	static const FName ResolveBattleVictoryCommand(TEXT("ResolveBattleVictory"));
	static const FName BasicAttackAction(TEXT("BattleBasicAttack"));
	static const FName CraneWingSlashAction(TEXT("BattleCraneWingSlash"));
	static const FName GuiyuanArtAction(TEXT("BattleGuiyuanArt"));
	static const FName DefendAction(TEXT("BattleDefend"));
	static const FName HealingPowderAction(TEXT("BattleHealingPowder"));
}

TSharedRef<SWidget> UGameXXKBattleBoardWidget::RebuildWidget()
{
	BuildProgrammaticLayout();
	return Super::RebuildWidget();
}

void UGameXXKBattleBoardWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildProgrammaticLayout();
	RefreshFromState();
}

FReply UGameXXKBattleBoardWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton && CancelBattleTargeting())
	{
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UGameXXKBattleBoardWidget::RefreshFromState()
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const bool bInBattle = Subsystem && Subsystem->GetRuntimeState().Screen == EGameXXKScreen::Battle;
	if (!bInBattle)
	{
		InteractionMode = EGameXXKBattleInteractionMode::Hidden;
		SelectedPartyIndex = INDEX_NONE;
		TargetingActionName = NAME_None;
	}
	else if (InteractionMode == EGameXXKBattleInteractionMode::Hidden)
	{
		InteractionMode = EGameXXKBattleInteractionMode::Idle;
	}

	BuildProgrammaticLayout();
	RefreshProgrammaticLayout();

	SetVisibility(bInBattle ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

bool UGameXXKBattleBoardWidget::ExecutePrimaryEnemyAction()
{
	return false;
}

bool UGameXXKBattleBoardWidget::ExecuteBasicAttackAction()
{
	return BeginTargetingBattleAction(BasicAttackAction);
}

bool UGameXXKBattleBoardWidget::ExecuteCraneWingSlashAction()
{
	return BeginTargetingBattleAction(CraneWingSlashAction);
}

bool UGameXXKBattleBoardWidget::ExecuteGuiyuanArtAction()
{
	return ExecuteBattleAction(GuiyuanArtAction);
}

bool UGameXXKBattleBoardWidget::ExecuteDefendAction()
{
	return ExecuteBattleAction(DefendAction);
}

bool UGameXXKBattleBoardWidget::ExecuteHealingPowderAction()
{
	return ExecuteBattleAction(HealingPowderAction);
}

bool UGameXXKBattleBoardWidget::OpenCommandMenuForPartyUnit(int32 PartyIndex, FVector2D MenuScreenPosition, FVector2D UnitScreenPosition)
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const FGameXXKRuntimeState* State = Subsystem ? &Subsystem->GetRuntimeState() : nullptr;
	if (!State || State->Screen != EGameXXKScreen::Battle || !State->ActiveBattleParty.IsValidIndex(PartyIndex))
	{
		return false;
	}

	const FGameXXKBattleRuntimeUnit& Unit = State->ActiveBattleParty[PartyIndex];
	if (Unit.bEnemy || Unit.bDefeated || Unit.HP <= 0)
	{
		return false;
	}

	SelectedPartyIndex = PartyIndex;
	CommandMenuAnchor = MenuScreenPosition;
	SelectedPartyScreenPosition = UnitScreenPosition;
	TargetingPointerPosition = UnitScreenPosition;
	TargetingActionName = NAME_None;
	InteractionMode = EGameXXKBattleInteractionMode::CommandMenuOpen;
	RefreshProgrammaticLayout();
	return true;
}

void UGameXXKBattleBoardWidget::UpdateTargetingPointer(FVector2D ScreenPosition)
{
	if (IsTargetingBattleActionForTest())
	{
		TargetingPointerPosition = ScreenPosition;
		InvalidateLayoutAndVolatility();
	}
}

bool UGameXXKBattleBoardWidget::ConfirmTargetingEnemy(int32 EnemyIndex)
{
	if (!IsTargetingBattleActionForTest() || SelectedPartyIndex == INDEX_NONE)
	{
		return false;
	}

	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || Subsystem->GetRuntimeState().Screen != EGameXXKScreen::Battle)
	{
		return false;
	}
	const FGameXXKRuntimeState& State = Subsystem->GetRuntimeState();
	if (!State.ActiveBattleEnemies.IsValidIndex(EnemyIndex))
	{
		return false;
	}
	const FGameXXKBattleRuntimeUnit& Enemy = State.ActiveBattleEnemies[EnemyIndex];
	if (!Enemy.bEnemy || Enemy.bDefeated || Enemy.HP <= 0)
	{
		return false;
	}

	const FName ActionToExecute = TargetingActionName;
	const int32 PartyIndex = SelectedPartyIndex;
	bool bExecuted = false;
	if (ActionToExecute == BasicAttackAction)
	{
		bExecuted = Subsystem->ExecuteBattleBasicAttack(PartyIndex, EnemyIndex);
	}
	else if (ActionToExecute == CraneWingSlashAction)
	{
		bExecuted = Subsystem->ExecuteBattleCraneWingSlash(PartyIndex, EnemyIndex);
	}

	if (bExecuted)
	{
		InteractionMode = EGameXXKBattleInteractionMode::Idle;
		TargetingActionName = NAME_None;
		SelectedPartyIndex = INDEX_NONE;
		if (Subsystem->GetRuntimeState().Screen != EGameXXKScreen::Battle)
		{
			GameXXKLevelFlow::OpenMapForRuntimeState(Subsystem);
		}
		if (!NotifyPlayerFlowStateChanged())
		{
			RefreshFromState();
		}
	}
	return bExecuted;
}

bool UGameXXKBattleBoardWidget::CancelBattleTargeting()
{
	if (IsTargetingBattleActionForTest())
	{
		InteractionMode = EGameXXKBattleInteractionMode::CommandMenuOpen;
		TargetingActionName = NAME_None;
		RefreshProgrammaticLayout();
		return true;
	}
	if (InteractionMode == EGameXXKBattleInteractionMode::CommandMenuOpen)
	{
		InteractionMode = EGameXXKBattleInteractionMode::Idle;
		SelectedPartyIndex = INDEX_NONE;
		TargetingActionName = NAME_None;
		RefreshProgrammaticLayout();
		return true;
	}
	return false;
}

bool UGameXXKBattleBoardWidget::ExecuteBattleAction(FName ActionName)
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || Subsystem->GetRuntimeState().Screen != EGameXXKScreen::Battle)
	{
		return false;
	}
	if (InteractionMode != EGameXXKBattleInteractionMode::CommandMenuOpen || SelectedPartyIndex == INDEX_NONE)
	{
		return false;
	}

	const int32 PartyIndex = SelectedPartyIndex;
	bool bExecuted = false;
	if (ActionName == BasicAttackAction)
	{
		bExecuted = BeginTargetingBattleAction(BasicAttackAction);
	}
	else if (ActionName == CraneWingSlashAction)
	{
		bExecuted = BeginTargetingBattleAction(CraneWingSlashAction);
	}
	else if (ActionName == GuiyuanArtAction)
	{
		bExecuted = Subsystem->ExecuteBattleGuiyuanArt(PartyIndex);
	}
	else if (ActionName == DefendAction)
	{
		bExecuted = Subsystem->ExecuteBattleDefend(PartyIndex);
	}
	else if (ActionName == HealingPowderAction)
	{
		bExecuted = Subsystem->ExecuteBattleHealingPowder(PartyIndex);
	}
	if (!bExecuted && !Subsystem->GetRuntimeState().bHasActiveBattle)
	{
		bExecuted = GameXXKMVPCommandRouter::ExecuteVisibleCommand(Subsystem, ResolveBattleVictoryCommand);
	}
	if (bExecuted)
	{
		if (ActionName != BasicAttackAction && ActionName != CraneWingSlashAction)
		{
			InteractionMode = EGameXXKBattleInteractionMode::Idle;
			SelectedPartyIndex = INDEX_NONE;
			TargetingActionName = NAME_None;
		}
		if (Subsystem->GetRuntimeState().Screen != EGameXXKScreen::Battle)
		{
			GameXXKLevelFlow::OpenMapForRuntimeState(Subsystem);
		}
		if (!NotifyPlayerFlowStateChanged())
		{
			RefreshFromState();
		}
	}
	return bExecuted;
}

bool UGameXXKBattleBoardWidget::IsBattleBoardVisible() const
{
	return GetVisibility() == ESlateVisibility::Visible;
}

int32 UGameXXKBattleBoardWidget::GetEnemySlotCount() const
{
	return 0;
}

int32 UGameXXKBattleBoardWidget::GetPartySlotCount() const
{
	return 0;
}

FString UGameXXKBattleBoardWidget::GetEnemySlotSide() const
{
	return TEXT("Left");
}

FString UGameXXKBattleBoardWidget::GetPartySlotSide() const
{
	return TEXT("Right");
}

FVector2D UGameXXKBattleBoardWidget::GetEnemySlotPositionForTest(int32 SlotIndex) const
{
	return FVector2D::ZeroVector;
}

FVector2D UGameXXKBattleBoardWidget::GetPartySlotPositionForTest(int32 SlotIndex) const
{
	return FVector2D::ZeroVector;
}

FString UGameXXKBattleBoardWidget::GetBattleStatusTextForTest() const
{
	return BuildBattleStatusText();
}

bool UGameXXKBattleBoardWidget::HasBattleActionForTest(FName ActionName, bool bRequireEnabled) const
{
	const UButton* Button = nullptr;
	if (ActionName == BasicAttackAction)
	{
		Button = BasicAttackButton;
	}
	else if (ActionName == CraneWingSlashAction)
	{
		Button = CraneWingSlashButton;
	}
	else if (ActionName == GuiyuanArtAction)
	{
		Button = GuiyuanArtButton;
	}
	else if (ActionName == DefendAction)
	{
		Button = DefendButton;
	}
	else if (ActionName == HealingPowderAction)
	{
		Button = HealingPowderButton;
	}
	return Button && (!bRequireEnabled || (IsCommandMenuVisibleForTest() && Button->GetIsEnabled()));
}

bool UGameXXKBattleBoardWidget::IsCommandMenuVisibleForTest() const
{
	return InteractionMode == EGameXXKBattleInteractionMode::CommandMenuOpen
		|| InteractionMode == EGameXXKBattleInteractionMode::TargetingBasicAttack
		|| InteractionMode == EGameXXKBattleInteractionMode::TargetingCraneWingSlash;
}

bool UGameXXKBattleBoardWidget::IsTargetingBattleActionForTest() const
{
	return InteractionMode == EGameXXKBattleInteractionMode::TargetingBasicAttack
		|| InteractionMode == EGameXXKBattleInteractionMode::TargetingCraneWingSlash;
}

bool UGameXXKBattleBoardWidget::KeepTargetingAfterEmptyClickForTest() const
{
	return IsTargetingBattleActionForTest();
}

int32 UGameXXKBattleBoardWidget::GetSelectedPartyIndexForTest() const
{
	return SelectedPartyIndex;
}

FName UGameXXKBattleBoardWidget::GetTargetingActionNameForTest() const
{
	return TargetingActionName;
}

FVector2D UGameXXKBattleBoardWidget::GetTargetingPointerPositionForTest() const
{
	return TargetingPointerPosition;
}

void UGameXXKBattleBoardWidget::BuildProgrammaticLayout()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("BattleBoardWidgetTree"));
	}
	if (!WidgetTree || RootCanvas || WidgetTree->RootWidget)
	{
		return;
	}

	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("GameXXKBattleBoardRoot"));
	WidgetTree->RootWidget = RootCanvas;

	StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BattleStatusText"));
	StatusText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	StatusText->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.85f));
	StatusText->SetShadowOffset(FVector2D(1.0f, 1.0f));
	FSlateFontInfo StatusFont = StatusText->GetFont();
	StatusFont.Size = 20;
	StatusText->SetFont(StatusFont);
	if (UCanvasPanelSlot* StatusSlot = RootCanvas->AddChildToCanvas(StatusText))
	{
		StatusSlot->SetAnchors(FAnchors(0.02f, 0.04f, 0.46f, 0.04f));
		StatusSlot->SetOffsets(FMargin(0.0f, 0.0f, 0.0f, 180.0f));
		StatusSlot->SetAlignment(FVector2D(0.0f, 0.0f));
	}

	ActionBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("BattleActionBox"));
	if (UCanvasPanelSlot* ActionSlot = RootCanvas->AddChildToCanvas(ActionBox))
	{
		ActionSlot->SetAnchors(FAnchors(0.68f, 0.52f, 0.98f, 0.96f));
		ActionSlot->SetOffsets(FMargin(0.0f, 0.0f, 0.0f, 0.0f));
		ActionSlot->SetAlignment(FVector2D(0.0f, 0.0f));
	}

	BasicAttackButton = AddBattleActionButton(NSLOCTEXT("GameXXKBattle", "BasicAttack", "普攻"), FName(TEXT("BattleBasicAttackButton")));
	CraneWingSlashButton = AddBattleActionButton(NSLOCTEXT("GameXXKBattle", "CraneWingSlash", "鹤羽斩"), FName(TEXT("BattleCraneWingSlashButton")));
	GuiyuanArtButton = AddBattleActionButton(NSLOCTEXT("GameXXKBattle", "GuiyuanArt", "归元术"), FName(TEXT("BattleGuiyuanArtButton")));
	DefendButton = AddBattleActionButton(NSLOCTEXT("GameXXKBattle", "Defend", "防御"), FName(TEXT("BattleDefendButton")));
	HealingPowderButton = AddBattleActionButton(NSLOCTEXT("GameXXKBattle", "HealingPowder", "金疮药"), FName(TEXT("BattleHealingPowderButton")));

	if (BasicAttackButton)
	{
		BasicAttackButton->OnClicked.AddDynamic(this, &UGameXXKBattleBoardWidget::HandleBasicAttackClicked);
	}
	if (CraneWingSlashButton)
	{
		CraneWingSlashButton->OnClicked.AddDynamic(this, &UGameXXKBattleBoardWidget::HandleCraneWingSlashClicked);
	}
	if (GuiyuanArtButton)
	{
		GuiyuanArtButton->OnClicked.AddDynamic(this, &UGameXXKBattleBoardWidget::HandleGuiyuanArtClicked);
	}
	if (DefendButton)
	{
		DefendButton->OnClicked.AddDynamic(this, &UGameXXKBattleBoardWidget::HandleDefendClicked);
	}
	if (HealingPowderButton)
	{
		HealingPowderButton->OnClicked.AddDynamic(this, &UGameXXKBattleBoardWidget::HandleHealingPowderClicked);
	}
}

UButton* UGameXXKBattleBoardWidget::AddBattleActionButton(const FText& Label, FName ButtonName)
{
	if (!ActionBox || !WidgetTree)
	{
		return nullptr;
	}

	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), ButtonName);
	Button->SetBackgroundColor(FLinearColor(0.06f, 0.16f, 0.13f, 0.92f));
	UTextBlock* LabelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	LabelText->SetText(Label);
	LabelText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	LabelText->SetJustification(ETextJustify::Center);
	FSlateFontInfo LabelFont = LabelText->GetFont();
	LabelFont.Size = 22;
	LabelText->SetFont(LabelFont);
	Button->AddChild(LabelText);
	if (UVerticalBoxSlot* ButtonSlot = ActionBox->AddChildToVerticalBox(Button))
	{
		ButtonSlot->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 6.0f));
	}
	return Button;
}

void UGameXXKBattleBoardWidget::RefreshProgrammaticLayout()
{
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(BuildBattleStatusText()));
	}
	if (ActionBox)
	{
		if (UCanvasPanelSlot* ActionSlot = Cast<UCanvasPanelSlot>(ActionBox->Slot))
		{
			ActionSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));
			ActionSlot->SetOffsets(FMargin(CommandMenuAnchor.X, CommandMenuAnchor.Y, 360.0f, 300.0f));
			ActionSlot->SetAlignment(FVector2D(0.0f, 0.0f));
		}
	}
	RefreshActionButtons();
}

void UGameXXKBattleBoardWidget::RefreshActionButtons()
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const FGameXXKRuntimeState* State = Subsystem ? &Subsystem->GetRuntimeState() : nullptr;
	const bool bShowMenu = IsCommandMenuVisibleForTest();
	const bool bCanUseMenu = InteractionMode == EGameXXKBattleInteractionMode::CommandMenuOpen;
	if (ActionBox)
	{
		ActionBox->SetVisibility(bShowMenu ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		ActionBox->SetIsEnabled(bCanUseMenu);
		ActionBox->SetRenderOpacity(IsTargetingBattleActionForTest() ? 0.55f : 1.0f);
	}

	const bool bInBattle = State && State->Screen == EGameXXKScreen::Battle && State->bHasActiveBattle && State->ActiveBattleParty.IsValidIndex(SelectedPartyIndex);
	const bool bHeroReady = bInBattle && !State->ActiveBattleParty[SelectedPartyIndex].bDefeated && State->ActiveBattleParty[SelectedPartyIndex].HP > 0;
	const bool bHasTarget = bInBattle && FindFirstLivingEnemyIndex() != INDEX_NONE;
	const bool bCanHealHero = bHeroReady && State->ActiveBattleParty[SelectedPartyIndex].HP < State->ActiveBattleParty[SelectedPartyIndex].MaxHP;

	if (BasicAttackButton)
	{
		BasicAttackButton->SetIsEnabled(bCanUseMenu && bHeroReady && bHasTarget);
	}
	if (CraneWingSlashButton)
	{
		CraneWingSlashButton->SetIsEnabled(bCanUseMenu && bHeroReady && bHasTarget && State->ActiveBattleParty[SelectedPartyIndex].MP >= 8);
	}
	if (GuiyuanArtButton)
	{
		GuiyuanArtButton->SetIsEnabled(bCanUseMenu && bCanHealHero && State->ActiveBattleParty[SelectedPartyIndex].MP >= 10);
	}
	if (DefendButton)
	{
		DefendButton->SetIsEnabled(bCanUseMenu && bHeroReady);
	}
	if (HealingPowderButton)
	{
		HealingPowderButton->SetIsEnabled(bCanUseMenu && bCanHealHero && UGameXXKMVPRules::GetItemCount(*State, UGameXXKMVPRules::ItemHealingPowder()) > 0);
	}
}

bool UGameXXKBattleBoardWidget::BeginTargetingBattleAction(FName ActionName)
{
	if (InteractionMode != EGameXXKBattleInteractionMode::CommandMenuOpen || SelectedPartyIndex == INDEX_NONE)
	{
		return false;
	}

	if (ActionName == BasicAttackAction)
	{
		InteractionMode = EGameXXKBattleInteractionMode::TargetingBasicAttack;
	}
	else if (ActionName == CraneWingSlashAction)
	{
		InteractionMode = EGameXXKBattleInteractionMode::TargetingCraneWingSlash;
	}
	else
	{
		return false;
	}

	TargetingActionName = ActionName;
	TargetingPointerPosition = SelectedPartyScreenPosition;
	RefreshProgrammaticLayout();
	return true;
}

int32 UGameXXKBattleBoardWidget::FindFirstLivingEnemyIndex() const
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem)
	{
		return INDEX_NONE;
	}

	const FGameXXKRuntimeState& State = Subsystem->GetRuntimeState();
	for (int32 EnemyIndex = 0; EnemyIndex < State.ActiveBattleEnemies.Num(); ++EnemyIndex)
	{
		const FGameXXKBattleRuntimeUnit& Enemy = State.ActiveBattleEnemies[EnemyIndex];
		if (Enemy.bEnemy && !Enemy.bDefeated && Enemy.HP > 0)
		{
			return EnemyIndex;
		}
	}
	return INDEX_NONE;
}

FString UGameXXKBattleBoardWidget::BuildBattleStatusText() const
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || Subsystem->GetRuntimeState().Screen != EGameXXKScreen::Battle)
	{
		return TEXT("");
	}

	const FGameXXKRuntimeState& State = Subsystem->GetRuntimeState();
	TArray<FString> Lines;
	if (State.ActiveBattleParty.IsValidIndex(0))
	{
		const FGameXXKBattleRuntimeUnit& Hero = State.ActiveBattleParty[0];
		Lines.Add(FString::Printf(TEXT("主角 HP %d/%d   MP %d/%d"), Hero.HP, Hero.MaxHP, Hero.MP, Hero.MaxMP));
	}
	if (State.ActiveBattleParty.IsValidIndex(1))
	{
		const FGameXXKBattleRuntimeUnit& Follower = State.ActiveBattleParty[1];
		Lines.Add(FString::Printf(TEXT("队友 HP %d/%d"), Follower.HP, Follower.MaxHP));
	}
	Lines.Add(FString::Printf(TEXT("金疮药 x%d"), UGameXXKMVPRules::GetItemCount(State, UGameXXKMVPRules::ItemHealingPowder())));
	for (const FGameXXKBattleRuntimeUnit& Enemy : State.ActiveBattleEnemies)
	{
		Lines.Add(FString::Printf(
			TEXT("%s HP %d/%d%s"),
			*Enemy.DisplayName.ToString(),
			Enemy.HP,
			Enemy.MaxHP,
			Enemy.bDefeated ? TEXT(" 已倒下") : TEXT("")));
	}
	return FString::Join(Lines, TEXT("\n"));
}

void UGameXXKBattleBoardWidget::HandleBasicAttackClicked()
{
	ExecuteBasicAttackAction();
}

void UGameXXKBattleBoardWidget::HandleCraneWingSlashClicked()
{
	ExecuteCraneWingSlashAction();
}

void UGameXXKBattleBoardWidget::HandleGuiyuanArtClicked()
{
	ExecuteGuiyuanArtAction();
}

void UGameXXKBattleBoardWidget::HandleDefendClicked()
{
	ExecuteDefendAction();
}

void UGameXXKBattleBoardWidget::HandleHealingPowderClicked()
{
	ExecuteHealingPowderAction();
}
