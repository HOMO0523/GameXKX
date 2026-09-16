#include "UI/GameXXKInterfaceHelpWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Framework/Application/SlateApplication.h"
#include "Guide/GameXXKGuideTargetRegistry.h"
#include "InputCoreTypes.h"
#include "UI/GameXXKDesktopTrainingLayout.h"
#include "UI/GameXXKGuideOverlayWidget.h"
#include "UI/GameXXKInRunUiStyle.h"
#include "UI/GameXXKLocalization.h"
#include "UI/GameXXKPartyDeckUiStyle.h"
#include "Widgets/SWindow.h"
#include "Brushes/SlateRoundedBoxBrush.h"

namespace
{
	bool VisibleWithParents(const UWidget* Widget)
	{
		for (const UWidget* Current = Widget; Current; Current = Current->GetParent())
			if (Current->GetVisibility() == ESlateVisibility::Collapsed || Current->GetVisibility() == ESlateVisibility::Hidden) return false;
		return Widget != nullptr;
	}
}

TSharedRef<SWidget> UGameXXKInterfaceHelpWidget::RebuildWidget() { Build(); return Super::RebuildWidget(); }

void UGameXXKInterfaceHelpWidget::Build()
{
	if (HelpCanvas) return;
	if (!WidgetTree) WidgetTree = NewObject<UWidgetTree>(this, TEXT("InterfaceHelpTree"));
	SetIsFocusable(true);
	HelpCanvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = HelpCanvas;
	Spotlight = WidgetTree->ConstructWidget<UGameXXKGuideSpotlightWidget>();
	auto* SpotlightSlot = HelpCanvas->AddChildToCanvas(Spotlight);
	SpotlightSlot->SetAnchors(FAnchors(0, 0, 1, 1)); SpotlightSlot->SetOffsets(FMargin(0));
	ReadingPanel = WidgetTree->ConstructWidget<UBorder>();
	FSlateBrush NoBacking; NoBacking.DrawAs=ESlateBrushDrawType::NoDrawType;
	ReadingPanel->SetBrush(NoBacking);
	ReadingPanel->SetPadding(FMargin(12));
	ReadingPanel->SetVisibility(ESlateVisibility::Visible);
	auto* ReadingSlot = HelpCanvas->AddChildToCanvas(ReadingPanel);
	ReadingSlot->SetZOrder(2); ReadingSlot->SetSize(FVector2D(336, 160));
	auto* Stack = WidgetTree->ConstructWidget<UVerticalBox>(); ReadingPanel->SetContent(Stack);
	auto* Header = WidgetTree->ConstructWidget<UHorizontalBox>();Stack->AddChildToVerticalBox(Header);
	Heading = WidgetTree->ConstructWidget<UTextBlock>();
	Heading->SetFont(FGameXXKInRunUiStyle::TitleFont(20)); Heading->SetColorAndOpacity(FLinearColor::White);
	Header->AddChildToHorizontalBox(Heading)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	Counter = WidgetTree->ConstructWidget<UTextBlock>();
	Counter->SetFont(FGameXXKInRunUiStyle::BodyFont(12)); Counter->SetColorAndOpacity(FLinearColor(1,1,1,.75f));
	Header->AddChildToHorizontalBox(Counter)->SetVerticalAlignment(VAlign_Center);
	BodySize = WidgetTree->ConstructWidget<USizeBox>(); BodySize->SetHeightOverride(64);
	Stack->AddChildToVerticalBox(BodySize)->SetPadding(FMargin(0,8,0,0));
	Body = WidgetTree->ConstructWidget<UTextBlock>(); Body->SetAutoWrapText(true);
	Body->SetFont(FGameXXKInRunUiStyle::BodyFont(18)); Body->SetColorAndOpacity(FLinearColor::White);
	BodySize->SetContent(Body);
	auto* Actions = WidgetTree->ConstructWidget<UHorizontalBox>();
	Stack->AddChildToVerticalBox(Actions)->SetPadding(FMargin(0, 4, 0, 0));
	auto AddButton = [&](const TCHAR* Key)
	{
		auto* Button = WidgetTree->ConstructWidget<UButton>();
        FButtonStyle Style;Style.SetNormal(FSlateRoundedBoxBrush(FLinearColor(1,1,1,.10f),3.f));
        Style.SetHovered(FSlateRoundedBoxBrush(FLinearColor(1,1,1,.2f),3.f));Style.SetPressed(FSlateRoundedBoxBrush(FLinearColor(1,1,1,.3f),3.f));Button->SetStyle(Style);
		auto* Label = WidgetTree->ConstructWidget<UTextBlock>(); Label->SetText(GameXXKLocalization::Text(Key));
		Label->SetFont(FGameXXKInRunUiStyle::BodyFont(16)); Label->SetColorAndOpacity(FLinearColor::White); Label->SetJustification(ETextJustify::Center);
		Button->SetContent(Label);
		auto* ButtonSize = WidgetTree->ConstructWidget<USizeBox>(); ButtonSize->SetWidthOverride(96); ButtonSize->SetHeightOverride(30); ButtonSize->SetContent(Button);
		Actions->AddChildToHorizontalBox(ButtonSize)->SetPadding(FMargin(0, 0, 6, 0));
		return Button;
	};
	PreviousButton = AddButton(TEXT("Common.Previous")); PreviousButton->OnClicked.AddDynamic(this, &UGameXXKInterfaceHelpWidget::Previous);
	NextButton = AddButton(TEXT("Common.Next")); NextButton->OnClicked.AddDynamic(this, &UGameXXKInterfaceHelpWidget::Next);
	PreviousLabel=Cast<UTextBlock>(PreviousButton->GetContent());NextLabel=Cast<UTextBlock>(NextButton->GetContent());
	auto* CloseButton = AddButton(TEXT("Common.Close")); CloseButton->OnClicked.AddDynamic(this, &UGameXXKInterfaceHelpWidget::CloseClicked);

	SetVisibility(ESlateVisibility::Collapsed);
}

#include "UI/GameXXKTalentTreeWidget.h"
#include "UI/GameXXKDesktopTrainingWorkbenchWidget.h"

UWidget* UGameXXKInterfaceHelpWidget::FindTarget(const FStep& Step) const
{
	if (Step.DirectTarget.IsValid()) return Step.DirectTarget.Get();
    if(Step.bNoTarget)return nullptr;
	UUserWidget* Host = TargetHost.Get();
	if (!Host || !Host->WidgetTree) return nullptr;
    if(bTeachingChestGuide)if(auto* Workbench=Cast<UGameXXKDesktopTrainingWorkbenchWidget>(Host))
        for(FName Name:Step.WidgetNames)if(Name.ToString().StartsWith(TEXT("TeachingTarget."))||Name.ToString().StartsWith(TEXT("StarterItem."))||Name.ToString().StartsWith(TEXT("StarterSlot.")))
            if(auto* Target=Workbench->ResolveTeachingChestTarget(Name))return Target;
	UWidget* Result = nullptr;
	// UE's ForEachWidgetAndDescendants descends into a UUserWidget instead of
	// visiting that wrapper. Whole embedded panels are guide targets too.
	Host->WidgetTree->ForEachWidget([&](UWidget* Candidate)
	{
		if (!Result && Step.WidgetNames.Contains(Candidate->GetFName()) && VisibleWithParents(Candidate)) Result = Candidate;
	});
	Host->WidgetTree->ForEachWidgetAndDescendants([&](UWidget* Candidate)
	{
		if (!Result && Step.WidgetNames.Contains(Candidate->GetFName()) && VisibleWithParents(Candidate)) Result = Candidate;
        if(!Result && VisibleWithParents(Candidate) && Step.WidgetNames.Contains(FName(*Candidate->GetFName().GetPlainNameString())))Result=Candidate;
        if (!Result && VisibleWithParents(Candidate))
            if (auto* Node = Cast<UGameXXKTalentNodeButton>(Candidate))
                if (Step.WidgetNames.Contains(Node->GetNodeId())) Result = Candidate;
	});
	return Result;
}

bool UGameXXKInterfaceHelpWidget::ResolveTargetRect(const FStep& Step, FSlateRect& Rect) const
{
	if (!Step.RegisteredTarget.IsNone())
		return FGameXXKGuideTargetRegistry::Get().ResolveTargetRect(Step.RegisteredTarget, *this, Rect);
	const UWidget* Target = FindTarget(Step);
	if (!Target || Target->GetCachedGeometry().GetLocalSize().IsNearlyZero()) return false;
	const FGeometry& TargetGeometry = Target->GetCachedGeometry();
	const FVector2D Min = GetCachedGeometry().AbsoluteToLocal(TargetGeometry.LocalToAbsolute(FVector2D::ZeroVector));
	const FVector2D Max = GetCachedGeometry().AbsoluteToLocal(TargetGeometry.LocalToAbsolute(TargetGeometry.GetLocalSize()));
	Rect = FSlateRect(Min.X, Min.Y, Max.X, Max.Y);
	return Rect.Right > Rect.Left && Rect.Bottom > Rect.Top;
}

void UGameXXKInterfaceHelpWidget::AddVisibleStep(const TCHAR* Key, TArray<FName> Names, const FName RegisteredTarget)
{
	FStep Step{Key, FString(Key) + TEXT(".Body"), MoveTemp(Names), RegisteredTarget};
	if (bBrowseAllInterfaces || (!RegisteredTarget.IsNone() && FGameXXKGuideTargetRegistry::Get().IsTargetRegistered(RegisteredTarget)) || FindTarget(Step))
		Steps.Add(MoveTemp(Step));
}

void UGameXXKInterfaceHelpWidget::ShowForHost(UUserWidget* Host, const FName Surface, const bool bDesktopFrame, const int32 HudPercent)
{
    bTeachingChestGuide=false;
	Build();ClearTargetBinding();bTutorial=false; TargetHost = Host; bUseDesktopFrame = bDesktopFrame; DesktopHudPercent = HudPercent;
	bBrowseAllInterfaces = Surface == TEXT("InterfaceCatalog");
	Steps.Reset(); StepIndex = 0;
	const FString Overview = FString(TEXT("Help.")) + Surface.ToString();
	Steps.Add({Overview, Overview + TEXT(".Body"), {}, NAME_None});
	if (bDesktopFrame)
	{
		AddVisibleStep(TEXT("Help.Inventory"), {TEXT("WarehousePanel"), TEXT("EmbeddedApprovedBackpack")});
		AddVisibleStep(TEXT("Help.Deck"), {TEXT("EmbeddedApprovedBackpack")});
		AddVisibleStep(TEXT("Help.Formation"), {TEXT("FormationPanel")});
		AddVisibleStep(TEXT("Help.Talents"), {TEXT("TalentsPanel")});
		AddVisibleStep(TEXT("Help.Tools"), {TEXT("ToolsPanel")});
		AddVisibleStep(TEXT("Help.Training"), {TEXT("TrainingMapPanel")});
		AddVisibleStep(TEXT("Help.Shop"), {TEXT("ShopPaper")});
		AddVisibleStep(Surface == TEXT("Academy") ? TEXT("Help.Academy") : TEXT("Help.Story"), {TEXT("StoryTaskPanel")});
		if(bBrowseAllInterfaces)AddVisibleStep(TEXT("Help.Academy"), {TEXT("StoryTaskPanel")});
	}
	if (Surface == TEXT("Battle") || bBrowseAllInterfaces)
	{
		AddVisibleStep(TEXT("Help.Battle.Resources"), {}, TEXT("Battle.Hud.PartyQi"));
		AddVisibleStep(TEXT("Help.Battle.Intent"), {}, TEXT("Battle.Enemy.Intent"));
		AddVisibleStep(TEXT("Help.Battle.Cards"), {}, TEXT("Battle.Hand.FirstPlayableTargetedCard"));
		AddVisibleStep(TEXT("Help.Battle.Turn"), {}, TEXT("Battle.EndTurn"));
	}
	if (Surface == TEXT("Route") || bBrowseAllInterfaces)
	{
		AddVisibleStep(TEXT("Help.Route.Choice"), {}, TEXT("Route.Tutorial.NextNode"));
		AddVisibleStep(TEXT("Help.Route.Summary"), {TEXT("GameXXKRouteMapFixedSummary")});
		AddVisibleStep(TEXT("Help.Route.Legend"), {TEXT("RouteLegendContainer")});
	}
	bOpen = true; UpdateReadingLayout(); SetVisibility(ESlateVisibility::SelfHitTestInvisible); RefreshPage();
	if (GetCachedWidget().IsValid() && FSlateApplication::IsInitialized()
		&& FSlateApplication::Get().FindWidgetWindow(GetCachedWidget().ToSharedRef()).IsValid()) SetKeyboardFocus();
}

void UGameXXKInterfaceHelpWidget::ShowTutorial(UUserWidget* Host,const TSet<FName>& Completed,int32 HudPercent,
    TFunction<void(FName)> PrepareContext,TFunction<bool(FName)> RecordCompletion)
{
    bTeachingChestGuide=false;
    Build();ClearTargetBinding();TargetHost=Host;bUseDesktopFrame=true;DesktopHudPercent=HudPercent;
    PrepareContextCallback=MoveTemp(PrepareContext);RecordCompletionCallback=MoveTemp(RecordCompletion);
    bTutorial=true;bBrowseAllInterfaces=false;bAdvancePending=false;Steps.Reset();
    const auto Add=[&](const TCHAR* Id,const TCHAR* Chapter,const TCHAR* Target,const TCHAR* Context,bool Action=true,bool Hover=false)
    {
        FStep Step;Step.HeadingKey=FString(TEXT("UI.Guide."))+Chapter;Step.BodyKey=FString(TEXT("UI.Step."))+Id;
        Step.WidgetNames.Add(FName(Target));Step.CompletionId=FName(*(FString(TEXT("UI.Basics.V1."))+Id));
        Step.Context=FName(Context);Step.bAction=Action;Step.bHover=Hover;Steps.Add(MoveTemp(Step));
    };
    Add(TEXT("Attributes"),TEXT("Bag"),TEXT("InventoryCharacterTab_0"),TEXT("Bag.Equipment"));
    Add(TEXT("Details"),TEXT("Bag"),TEXT("InventoryDetailedAttributesButton"),TEXT("Bag.Attributes"));
    Add(TEXT("Equipment"),TEXT("Bag"),TEXT("InventoryCharacterTab_1"),TEXT("Bag.Attributes"));
    Add(TEXT("Deck"),TEXT("Cards"),TEXT("InventoryCharacterTab_2"),TEXT("Bag.Equipment"));
    Add(TEXT("ExpandCards"),TEXT("Cards"),TEXT("InventoryDeckExpandButton"),TEXT("Bag.Deck"));
    Add(TEXT("InspectCard"),TEXT("Cards"),TEXT("InventoryHeroDeckCard_00"),TEXT("Bag.ExpandedDeck"),false,true);
    Add(TEXT("BackToBag"),TEXT("Cards"),TEXT("InventoryDeckCollapseButton"),TEXT("Bag.ExpandedDeck"));
    Add(TEXT("Storage"),TEXT("Items"),TEXT("BottomNavigationButton_0"),TEXT("Bag.Equipment"));
    Add(TEXT("StorageUse"),TEXT("Items"),TEXT("WarehousePanel"),TEXT("Storage"),false);
    Add(TEXT("Party"),TEXT("Party"),TEXT("BottomNavigationButton_1"),TEXT("Bag.Equipment"));
    Add(TEXT("PersonalDeck"),TEXT("Party"),TEXT("FormationEditDeck_0"),TEXT("Party"));
    Add(TEXT("BackToParty"),TEXT("Party"),TEXT("FormationDeckBack"),TEXT("Party.Deck"));
    Add(TEXT("Talents"),TEXT("Growth"),TEXT("BottomNavigationButton_2"),TEXT("Party"));
    Add(TEXT("TalentUse"),TEXT("Growth"),TEXT("PermanentTalentTreeWidget"),TEXT("Talents"),false);
    Add(TEXT("Tools"),TEXT("Tools"),TEXT("BottomNavigationButton_3"),TEXT("Bag.Equipment"));
    Add(TEXT("ToolMenu"),TEXT("Tools"),TEXT("ToolModeDropdownButton"),TEXT("Tools"));
    Add(TEXT("Enhance"),TEXT("Tools"),TEXT("ToolButton_2"),TEXT("Tools.Menu"));
    Add(TEXT("ToolUse"),TEXT("Tools"),TEXT("ToolInputSlot_0"),TEXT("Tools.Enhance"),false);
    Add(TEXT("Map"),TEXT("Map"),TEXT("BottomNavigationButton_4"),TEXT("Bag.Equipment"));
    Add(TEXT("MapUse"),TEXT("Map"),TEXT("TrainingMapPanel"),TEXT("Map"),false);
    Add(TEXT("Settings"),TEXT("Settings"),TEXT("TopToolbarSettings"),TEXT("Bag.Equipment"));
    Add(TEXT("Language"),TEXT("Settings"),TEXT("DesktopHudSettingsPanel"),TEXT("Settings"),false);
    Add(TEXT("Done"),TEXT("Settings"),TEXT("HudSettingsCloseButton"),TEXT("Settings"));
    StepIndex=0;while(Steps.IsValidIndex(StepIndex)&&Completed.Contains(Steps[StepIndex].CompletionId))++StepIndex;
    bReplay=StepIndex>=Steps.Num();if(bReplay)StepIndex=0;
    bOpen=true;bPreparePending=true;HoverSeconds=0;bHoverObserved=false;SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    RefreshPage();UpdateReadingLayout();
}

void UGameXXKInterfaceHelpWidget::ShowProgressionTutorial(UUserWidget* Host, FName Group,
    const TSet<FName>& Completed, int32 HudPercent, TFunction<void(FName)> PrepareContext,
    TFunction<bool(FName)> RecordCompletion)
{
    Build(); ClearTargetBinding(); TargetHost=Host; bUseDesktopFrame=true; DesktopHudPercent=HudPercent;
    PrepareContextCallback=MoveTemp(PrepareContext); RecordCompletionCallback=MoveTemp(RecordCompletion);
    bTutorial=true; bBrowseAllInterfaces=false; bReplay=false; bAdvancePending=false; Steps.Reset();
    const FString Name=Group.ToString();
    bTeachingChestGuide=Name.StartsWith(TEXT("Teaching."));
    if(bTeachingChestGuide)
    {
        TArray<FString> Parts;Name.ParseIntoArray(Parts,TEXT("."));
        if(Parts.Num()!=3){Dismiss();return;}
        const int32 Stage=FCString::Atoi(*Parts[1]);const FString Part=Parts[2];
        const auto Add=[&](const TCHAR* Target,const TCHAR* Body,bool StateDriven=false,bool Action=true)
        {
            FStep Step;Step.HeadingKey=FString::Printf(TEXT("TeachingChest.Stage.%d"),Stage);Step.BodyKey=Body;
            Step.WidgetNames.Add(Target);Step.Context=Group;Step.bAction=Action;Step.bStateDriven=StateDriven;
            Step.CompletionId=FName(*(Name+FString::Printf(TEXT(".%d"),Steps.Num())));Steps.Add(MoveTemp(Step));
        };
        if(Stage==0)
        {
            if(Part==TEXT("Open"))Add(TEXT("TeachingTarget.Chest"),TEXT("StarterGear.Open"),true);
            else if(Part==TEXT("Map"))Add(TEXT("BottomNavigationButton_4"),TEXT("StarterGear.Map"));
            else if(Part==TEXT("Retry"))Add(TEXT("TrainingTravelButton"),TEXT("StarterGear.Retry"),true);
            else
            {
                Add(*(TEXT("StarterItem.")+Part),TEXT("StarterGear.Pick"),true);
                Add(*(TEXT("StarterSlot.")+Part),TEXT("StarterGear.Equip"),true);
            }
            for(auto& Step:Steps)Step.HeadingKey=TEXT("StarterGear.Title");
        }
        else if(Part==TEXT("Open"))Add(TEXT("TeachingTarget.Chest"),TEXT("TeachingChest.Open"),true);
        else if(Part==TEXT("Equip")){Add(TEXT("TeachingTarget.Item"),TEXT("TeachingChest.Equip"),true);Add(TEXT("TeachingTarget.Equipped"),TEXT("TeachingChest.EquipPlace"),true);}
        else if(Part==TEXT("Unequip"))Add(TEXT("TeachingTarget.Equipped"),TEXT("TeachingChest.Unequip"),true);
        else if(Part==TEXT("Tools"))Add(TEXT("BottomNavigationButton_3"),TEXT("TeachingChest.Tools"));
        else if(Part==TEXT("Mode"))
        {
            Add(TEXT("ToolModeDropdownButton"),TEXT("TeachingChest.ModeMenu"));
            const int32 Mode=Stage==2?4:Stage==3?2:Stage==4?3:Stage==5?0:1;
            Add(*FString::Printf(TEXT("ToolButton_%d"),Mode),TEXT("TeachingChest.Mode"));
        }
        else if(Part==TEXT("Target")){Add(TEXT("TeachingTarget.Item"),TEXT("TeachingChest.PickEquipment"),true);Add(TEXT("ToolInputSlot_0"),TEXT("TeachingChest.PlaceEquipment"),true);}
        else if(Part==TEXT("Gem")){Add(TEXT("TeachingTarget.Gem"),TEXT("TeachingChest.PickGem"),true);Add(TEXT("ToolInputSlot_1"),TEXT("TeachingChest.PlaceGem"),true);}
        else if(Part==TEXT("Generate"))Add(TEXT("ToolConfirmButton"),TEXT("TeachingChest.Reforge"),true);
        else if(Part==TEXT("Resolve"))Add(TEXT("TeachingReforgeChoices"),TEXT("TeachingChest.Resolve"),true);
        else if(Part==TEXT("Commit"))Add(TEXT("ToolConfirmButton"),Stage==3?TEXT("TeachingChest.Enhance"):TEXT("TeachingChest.Dismantle"),true);
        else if(Part==TEXT("Materials"))Add(TEXT("TeachingTarget.Chest"),TEXT("TeachingChest.Materials"),true);
        else if(Part==TEXT("Fill"))Add(TEXT("ToolAutoFill"),TEXT("TeachingChest.Fill"));
        else if(Part==TEXT("Review")&&Stage==3)
        {Add(TEXT("TeachingTarget.EnhancedItem"),TEXT("TeachingChest.HoverEnhanced"),false,false);Steps.Last().bHover=true;Steps.Last().bCloseCompletes=true;Steps.Last().HeadingKey=TEXT("TeachingChest.InspectHeading");}
        else if(Part==TEXT("Intro")&&Stage==5)
        {Add(TEXT("ToolModeLabel"),TEXT("TeachingChest.DismantleIntro"),false,false);Steps.Last().bCloseCompletes=true;}
        else if(Part==TEXT("Review"))Add(TEXT("ToolAutoFill"),TEXT("TeachingChest.Review"),false,false);
        StepIndex=0;bOpen=!Steps.IsEmpty();bPreparePending=bOpen;HoverSeconds=0;bHoverObserved=false;
        SetVisibility(bOpen?ESlateVisibility::SelfHitTestInvisible:ESlateVisibility::Collapsed);
        if(bOpen){RefreshPage();UpdateReadingLayout();}return;
    }
    if(Name==TEXT("FirstChallenge12"))
    {
        if(!Completed.Contains(TEXT("UI.Progression.V1.FirstChallenge12.Commit")))
        for(int32 I=0;I<2;++I)
        {
            FStep Step;Step.HeadingKey=TEXT("UI.Progression.FirstChallenge12.Heading");
            Step.BodyKey=I==0?TEXT("UI.Progression.FirstChallenge12.Select"):TEXT("UI.Progression.FirstChallenge12.Start");
            Step.CompletionId=I==0?TEXT("UI.Progression.V1.FirstChallenge12.Select"):TEXT("UI.Progression.V1.FirstChallenge12.Commit");
            Step.WidgetNames.Add(I==0?FName(TEXT("TrainingNode_2")):FName(TEXT("TrainingChallengeButton")));
            Step.Context=TEXT("Progression.FirstChallenge12");Step.bAction=true;Steps.Add(MoveTemp(Step));
        }
        StepIndex=0;bOpen=!Steps.IsEmpty();bPreparePending=bOpen;HoverSeconds=0;bHoverObserved=false;
        SetVisibility(bOpen?ESlateVisibility::SelfHitTestInvisible:ESlateVisibility::Collapsed);
        if(bOpen){RefreshPage();UpdateReadingLayout();}return;
    }
    const bool Companion=Name.StartsWith(TEXT("Companion"));
    const bool Unlock=Name.EndsWith(TEXT("Unlock"));
    if(Name.EndsWith(TEXT("Formation")))
    {
        FStep Step;Step.CompletionId=FName(*(TEXT("UI.Progression.V1.")+Name+TEXT(".Commit")));
        if(!Completed.Contains(Step.CompletionId))
        {
            Step.HeadingKey=Companion?TEXT("UI.Progression.Companion"):TEXT("UI.Progression.Npc");
            Step.BodyKey=TEXT("UI.Progression.OpenFormation");Step.WidgetNames.Add(TEXT("BottomNavigationButton_1"));
            Step.Context=TEXT("Progression.Formation");Step.bAction=true;Steps.Add(MoveTemp(Step));
        }
        StepIndex=0;bOpen=!Steps.IsEmpty();bPreparePending=bOpen;HoverSeconds=0;bHoverObserved=false;
        SetVisibility(bOpen?ESlateVisibility::SelfHitTestInvisible:ESlateVisibility::Collapsed);
        if(bOpen){RefreshPage();UpdateReadingLayout();}return;
    }
    if(Name.StartsWith(TEXT("Story")))
    {
        const bool Reward=Name.StartsWith(TEXT("StoryReward"));
        const TCHAR* Targets[2]={TEXT("StoryQuestButton"),TEXT("MainStoryChapter_0")};
        const TCHAR* Bodies[2]={TEXT("UI.Progression.Story.Open"),TEXT("UI.Progression.Story.Chapter")};
        if(Name==TEXT("StoryFirst")){Targets[0]=TEXT("StoryNode_S00-01");Targets[1]=TEXT("StoryStartTask");Bodies[0]=TEXT("UI.Progression.Story.First");Bodies[1]=TEXT("UI.Progression.Story.Start");}
        if(Name==TEXT("StoryMeet")){Targets[0]=TEXT("StoryContinueMainline");Targets[1]=TEXT("StoryStartTask");Bodies[0]=TEXT("UI.Progression.Story.Continue");Bodies[1]=TEXT("UI.Progression.Story.Meet");}
        if(Reward){Targets[0]=TEXT("StoryClaimReward");Bodies[0]=TEXT("UI.Progression.Story.Reward");}
        for(int32 I=0;I<(Reward?1:2);++I)
        {
            FStep Step;Step.CompletionId=FName(*(TEXT("UI.Progression.V1.")+Name+((Reward||I==1)?TEXT(".Commit"):TEXT(".Select"))));
            if(Completed.Contains(Step.CompletionId))continue;
            Step.HeadingKey=TEXT("UI.Progression.Story.Heading");Step.BodyKey=Bodies[I];Step.WidgetNames.Add(Targets[I]);
            Step.Context=FName(*(TEXT("Progression.")+Name));Step.bAction=true;Steps.Add(MoveTemp(Step));
        }
        StepIndex=0;bOpen=!Steps.IsEmpty();bPreparePending=bOpen;HoverSeconds=0;bHoverObserved=false;
        SetVisibility(bOpen?ESlateVisibility::SelfHitTestInvisible:ESlateVisibility::Collapsed);
        if(bOpen){RefreshPage();UpdateReadingLayout();}return;
    }
    const FString Context=FString(Unlock?TEXT("Progression.Talent."):TEXT("Progression.Party."))+(Companion?TEXT("Companion"):TEXT("Npc"));
    for(int32 I=0;I<2;++I)
    {
        FStep Step;
        Step.CompletionId=FName(*(TEXT("UI.Progression.V1.")+Name+(I==0?TEXT(".Select"):TEXT(".Commit"))));
        if(Completed.Contains(Step.CompletionId))continue;
        Step.HeadingKey=Companion?TEXT("UI.Progression.Companion"):TEXT("UI.Progression.Npc");
        Step.BodyKey=Unlock?(I==0?TEXT("UI.Progression.SelectTalent"):TEXT("UI.Progression.Pay"))
            :(I==0?TEXT("UI.Progression.SelectMember"):TEXT("UI.Progression.Deploy"));
        Step.Context=FName(*Context); Step.bAction=true;
        Step.WidgetNames.Add(Unlock?(I==0?FName(Companion?TEXT("Talent.Party.CompanionSlot"):TEXT("Talent.Party.NpcSlot")):FName(TEXT("TalentPurchaseButton")))
            :FName(I==0?TEXT("FormationCandidateButton_0"):TEXT("FormationApplyButton")));
        Steps.Add(MoveTemp(Step));
    }
    StepIndex=0; bOpen=!Steps.IsEmpty(); bPreparePending=bOpen; HoverSeconds=0; bHoverObserved=false;
    SetVisibility(bOpen?ESlateVisibility::SelfHitTestInvisible:ESlateVisibility::Collapsed);
    if(bOpen){RefreshPage();UpdateReadingLayout();}
}

FName UGameXXKInterfaceHelpWidget::GetCurrentCompletionIdForTest() const
{
    return Steps.IsValidIndex(StepIndex)?Steps[StepIndex].CompletionId:NAME_None;
}
UWidget* UGameXXKInterfaceHelpWidget::GetCurrentTargetForTest() const{return Steps.IsValidIndex(StepIndex)?FindTarget(Steps[StepIndex]):nullptr;}

void UGameXXKInterfaceHelpWidget::ClearTargetBinding()
{
    if(BoundTargetButton)BoundTargetButton->OnClicked.RemoveDynamic(this,&UGameXXKInterfaceHelpWidget::TargetClicked);
    BoundTargetButton=nullptr;
}

void UGameXXKInterfaceHelpWidget::PrepareCurrentStep()
{
    bPreparePending=false;ClearTargetBinding();
    if(!bTutorial||!Steps.IsValidIndex(StepIndex)||!PrepareContextCallback)return;
    TGuardValue<bool> Guard(bPreparing,true);PrepareContextCallback(Steps[StepIndex].Context);
}

void UGameXXKInterfaceHelpWidget::TargetClicked()
{
    // The gameplay handler can disable the button after a successful purchase.
    // Validate the resulting state in RecordCompletion rather than its new enabled state.
    if(bOpen&&bTutorial&&!bPreparing&&BoundTargetButton)bAdvancePending=true;
}

bool UGameXXKInterfaceHelpWidget::CompleteCurrentStep()
{
    if(!bOpen||!Steps.IsValidIndex(StepIndex))return false;
    if(bTutorial&&RecordCompletionCallback&&!RecordCompletionCallback(Steps[StepIndex].CompletionId))
    {
        if(!bTeachingChestGuide)Body->SetText(GameXXKLocalization::Text(TEXT("UI.Guide.SaveFailed")));return false;
    }
    ClearTargetBinding();bAdvancePending=false;HoverSeconds=0;bHoverObserved=false;++StepIndex;
    if(!Steps.IsValidIndex(StepIndex)){Dismiss();return true;}
    RefreshPage();return true;
}

void UGameXXKInterfaceHelpWidget::RecoverTarget(){if(bOpen&&bTutorial){if(bTeachingChestGuide){StepIndex=0;RefreshPage();}bPreparePending=true;HoverSeconds=0;}}
void UGameXXKInterfaceHelpWidget::ConfirmReadingForTest(){Next();}

void UGameXXKInterfaceHelpWidget::RefreshPage()
{
	if (!Steps.IsValidIndex(StepIndex)) { Dismiss(); return; }
	Heading->SetText(GameXXKLocalization::Text(*Steps[StepIndex].HeadingKey));
	Body->SetText(Steps[StepIndex].OverrideText.IsEmpty()?GameXXKLocalization::Text(*Steps[StepIndex].BodyKey):Steps[StepIndex].OverrideText);
    if(BodySize)BodySize->SetHeightOverride(64.f);
	FFormatNamedArguments Arguments; Arguments.Add(TEXT("Current"), StepIndex + 1); Arguments.Add(TEXT("Total"), Steps.Num());
	Counter->SetText(GameXXKLocalization::Format(TEXT("UI.Guide.Progress"), Arguments));
	Counter->SetVisibility(Steps[StepIndex].DirectTarget.IsValid()?ESlateVisibility::Collapsed:ESlateVisibility::HitTestInvisible);
    PreviousLabel->SetText(GameXXKLocalization::Text(TEXT("UI.Guide.Back")));
    NextLabel->SetText(GameXXKLocalization::Text(bTutorial?TEXT("UI.Guide.GotIt"):TEXT("Common.Next")));
    PreviousButton->GetParent()->SetVisibility(bTutorial?ESlateVisibility::Collapsed:ESlateVisibility::Visible);
    NextButton->GetParent()->SetVisibility(bTutorial&&(Steps[StepIndex].bAction||(Steps[StepIndex].bHover&&!bHoverObserved))?ESlateVisibility::Collapsed:ESlateVisibility::Visible);
    if(Steps[StepIndex].bCloseCompletes)NextButton->GetParent()->SetVisibility(ESlateVisibility::Collapsed);
	PreviousButton->SetIsEnabled(StepIndex > 0); NextButton->SetIsEnabled(bTutorial||StepIndex + 1 < Steps.Num());
    PresentedLanguageRevision=GameXXKLocalization::GetRevision();
    // Includes the close label, which remains visible across language switches.
    WidgetTree->ForEachWidget([](UWidget* Widget){if(auto* Label=Cast<UTextBlock>(Widget))Label->SetText(GameXXKLocalization::Localize(Label->GetText()));});
}

void UGameXXKInterfaceHelpWidget::UpdateReadingLayout()
{
	FVector2D HostSize = GetCachedGeometry().GetLocalSize();
	if (HostSize.IsNearlyZero()) HostSize = FVector2D(1920, 1080);
	if (auto* ReadingSlot = Cast<UCanvasPanelSlot>(ReadingPanel->Slot))
	{
        const float HostScale=FMath::Max(.25f,GetCachedGeometry().GetAccumulatedLayoutTransform().GetScale());
        const float ReadingScale=bUseDesktopFrame?1.f/HostScale:1.f;
        FVector2D SafeMin(12/HostScale,12/HostScale),SafeMax=HostSize-SafeMin;
        if(FSlateApplication::IsInitialized()&&GetCachedWidget().IsValid())
            if(const auto Window=FSlateApplication::Get().FindWidgetWindow(GetCachedWidget().ToSharedRef()))
            {
                SafeMin=GetCachedGeometry().AbsoluteToLocal(Window->GetPositionInScreen())+FVector2D(12/HostScale);
                SafeMax=GetCachedGeometry().AbsoluteToLocal(Window->GetPositionInScreen()+Window->GetSizeInScreen())-FVector2D(12/HostScale);
            }
        const FVector2D Size(336,160),Extent=Size*ReadingScale;
        FSlateRect Target;const bool HasTarget=Steps.IsValidIndex(StepIndex)&&ResolveTargetRect(Steps[StepIndex],Target);
        TArray<FVector2D> Candidates;
        if(HasTarget)
        {
            Candidates={{Target.Left-Extent.X-18,Target.Top},{Target.Right+18,Target.Top},
                {Target.Left,Target.Bottom+18},{Target.Left,Target.Top-Extent.Y-18}};
        }
        Candidates.Add(FVector2D(SafeMax.X-Extent.X,SafeMin.Y));
        FVector2D Best=Candidates.Last();double BestScore=TNumericLimits<double>::Max();
        for(FVector2D Candidate:Candidates)
        {
            Candidate.X=FMath::Clamp(Candidate.X,SafeMin.X,FMath::Max(SafeMin.X,SafeMax.X-Extent.X));
            Candidate.Y=FMath::Clamp(Candidate.Y,SafeMin.Y,FMath::Max(SafeMin.Y,SafeMax.Y-Extent.Y));
            const double Overlap=HasTarget?FMath::Max(0.0,FMath::Min(Candidate.X+Extent.X,double(Target.Right))-FMath::Max(Candidate.X,double(Target.Left)))
                *FMath::Max(0.0,FMath::Min(Candidate.Y+Extent.Y,double(Target.Bottom))-FMath::Max(Candidate.Y,double(Target.Top))):0;
            if(Overlap<BestScore){Best=Candidate;BestScore=Overlap;}
        }
        ReadingSlot->SetPosition(Best);ReadingSlot->SetSize(Size);
        ReadingPanel->SetRenderTransformPivot(FVector2D::ZeroVector);ReadingPanel->SetRenderScale(FVector2D(ReadingScale));
        const FVector4 NewRect(Best.X,Best.Y,Extent.X,Extent.Y);
        if(!ReadingRect.Equals(NewRect,.1f)){ReadingRect=NewRect;LayoutChanged.ExecuteIfBound();}
	}
}

void UGameXXKInterfaceHelpWidget::NativeTick(const FGeometry& Geometry, const float DeltaTime)
{
	Super::NativeTick(Geometry, DeltaTime);
	if (!bOpen) return;
	if (!TargetHost.IsValid() || !VisibleWithParents(TargetHost.Get())) { Dismiss(); return; }
    if(bPreparePending)PrepareCurrentStep();
    if(bAdvancePending){bAdvancePending=false;CompleteCurrentStep();if(!bOpen)return;}
    if(PresentedLanguageRevision!=GameXXKLocalization::GetRevision())RefreshPage();
    if(bTeachingChestGuide&&Steps.IsValidIndex(StepIndex)&&Steps[StepIndex].bStateDriven)
    {
        HoverSeconds+=FMath::Max(0.f,DeltaTime);
        if(HoverSeconds>=.15f)
        {
            HoverSeconds=0;
            if(RecordCompletionCallback&&RecordCompletionCallback(Steps[StepIndex].CompletionId))
            {CompleteCurrentStep();if(!bOpen)return;}
        }
    }
    if(bTutorial&&Steps.IsValidIndex(StepIndex))
    {
        const auto& Step=Steps[StepIndex];UWidget* Target=FindTarget(Step);
        if(Step.bAction)
        {
            auto* Button=Cast<UButton>(Target);
            if(BoundTargetButton!=Button)
            {
                ClearTargetBinding();BoundTargetButton=Button;
            }
            // Workbench rebuilds reuse the same button UObject and Configure()
            // clears its delegates; pointer equality does not mean we are bound.
            if(Button)Button->OnClicked.AddUniqueDynamic(this,&UGameXXKInterfaceHelpWidget::TargetClicked);
        }
        if(Step.bHover)
        {
            HoverSeconds=Target&&Target->IsHovered()?HoverSeconds+FMath::Max(0.f,DeltaTime):0;
            if(HoverSeconds>=1.0f)
            {
                bHoverObserved=true;NextButton->GetParent()->SetVisibility(Step.bCloseCompletes?ESlateVisibility::Collapsed:ESlateVisibility::Visible);
                NextLabel->SetText(GameXXKLocalization::Text(TEXT("UI.Guide.GotIt")));
            }
        }
        if(!Target&&!Step.bNoTarget)
        {
            NextButton->GetParent()->SetVisibility(ESlateVisibility::Visible);
            NextLabel->SetText(GameXXKLocalization::Text(TEXT("UI.Guide.Show")));
            Body->SetText(GameXXKLocalization::Text(TEXT("UI.Guide.Recover")));
        }
        else if(NextLabel->GetText().ToString()==GameXXKLocalization::Text(TEXT("UI.Guide.Show")).ToString())RefreshPage();
    }
	UpdateReadingLayout();
	FSlateRect TargetRect;
	TArray<FSlateRect> Cutouts;
	if (Steps.IsValidIndex(StepIndex) && ResolveTargetRect(Steps[StepIndex], TargetRect))
	{
		Cutouts.Add(TargetRect);
	}
	FGameXXKGuideOutput Output; Output.bActive=true; Output.InputPolicy=EGameXXKGuideInputPolicy::Soft;
	Spotlight->PresentSpotlight(Output,Cutouts);
}

void UGameXXKInterfaceHelpWidget::Previous() { if (!bTutorial&&StepIndex > 0) { --StepIndex; RefreshPage(); } }
void UGameXXKInterfaceHelpWidget::Next()
{
    if(bTutorial)
    {
        if(!Steps.IsValidIndex(StepIndex))return;
        if(Steps[StepIndex].bCloseCompletes)return;
        if(!Steps[StepIndex].bNoTarget&&!FindTarget(Steps[StepIndex])){RecoverTarget();return;}
        if(!Steps[StepIndex].bAction&&(!Steps[StepIndex].bHover||bHoverObserved))CompleteCurrentStep();
    }
    else if(StepIndex + 1 < Steps.Num()){++StepIndex;RefreshPage();}
}
void UGameXXKInterfaceHelpWidget::CloseClicked()
{
    if(bOpen && Steps.IsValidIndex(StepIndex) && Steps[StepIndex].DirectTarget.IsValid()
        && !Steps[StepIndex].bAction && (!Steps[StepIndex].bHover || bHoverObserved))
    {
        // Reading and then closing a battle explanation completes that explanation,
        // not the rest of the battle's lessons. Failed saves keep the current prompt.
        CompleteCurrentStep();return;
    }
    if(bOpen&&bTeachingChestGuide&&Steps.IsValidIndex(StepIndex)&&Steps[StepIndex].bCloseCompletes)
    {if(CompleteCurrentStep())return;}
    Dismiss();
}

void UGameXXKInterfaceHelpWidget::ShowBattleStep(UUserWidget* Host,FName Topic,UWidget* Target,const FText& Text,
	bool Action,bool Hover,TFunction<bool(FName)> RecordCompletion,FSimpleDelegate OnDismiss)
{
	Build();ClearTargetBinding();TargetHost=Host;bUseDesktopFrame=false;DesktopHudPercent=100;
	bTeachingChestGuide=false;bTutorial=true;bBrowseAllInterfaces=false;bReplay=false;bAdvancePending=false;
	bPreparePending=false;PrepareContextCallback=nullptr;RecordCompletionCallback=MoveTemp(RecordCompletion);Dismissed=MoveTemp(OnDismiss);
	Steps.Reset();FStep Step;Step.HeadingKey=TEXT("FirstBattle.Heading");Step.OverrideText=Text;
	Step.CompletionId=Topic;Step.DirectTarget=Target;Step.bAction=Action;Step.bHover=Hover;
	Steps.Add(MoveTemp(Step));StepIndex=0;HoverSeconds=0;bHoverObserved=false;bOpen=true;
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);RefreshPage();UpdateReadingLayout();
}
void UGameXXKInterfaceHelpWidget::Dismiss()
{
	const bool bWasOpen = bOpen; bOpen = false; SetVisibility(ESlateVisibility::Collapsed);
    if(bWasOpen&&bTeachingChestGuide&&StepIndex<Steps.Num())
        if(auto* Host=Cast<UGameXXKDesktopTrainingWorkbenchWidget>(TargetHost.Get()))Host->RecordTeachingChestStep(TEXT("Teaching.Dismiss"));
    ClearTargetBinding();bAdvancePending=false;bPreparePending=false;HoverSeconds=0;
	if (Spotlight) Spotlight->DismissSpotlight();
	if (bWasOpen) Dismissed.ExecuteIfBound();
}

FReply UGameXXKInterfaceHelpWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (bOpen && (Event.GetKey() == EKeys::Escape || Event.GetKey() == EKeys::F1 || Event.GetKey() == EKeys::F10))
	{
		CloseClicked(); if (Event.GetKey() != EKeys::F10) return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(Geometry, Event);
}
