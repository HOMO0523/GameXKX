#include "UI/GameXXKCompanionRosterWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "GameXXKCardCatalog.h"
#include "GameXXKCardText.h"
#include "GameXXKCompanionRules.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKPartyDeckUiStyle.h"

namespace
{
	constexpr int32 RosterSlotCount = 12;
	constexpr int32 RosterColumnCount = 3;
	constexpr int32 PersonalCardColumnCount = 3;
	const FVector2D RosterWindowSize(1240.0f, 704.0f);
	const FVector2D RosterSlotSize(96.0f, 78.0f);
	const FVector2D PersonalCardSize(113.0f, 129.0f);
	const FVector2D CardTooltipSize(402.0f, 244.0f);
	const FVector2D ActionButtonSize(220.0f, 44.0f);
	const FMargin SlotFrameMargin(5.0f / 61.0f, 5.0f / 56.0f, 5.0f / 61.0f, 5.0f / 56.0f);
	const FMargin ActionFrameMargin(5.0f / 73.0f, 5.0f / 31.0f, 5.0f / 73.0f, 5.0f / 31.0f);

	static constexpr const TCHAR* WindowFrameTexturePath = TEXT("/Game/GameXXK/UI/Town/Textures/PSD/Backgrounds/T_TownPsd_Background_Companion.T_TownPsd_Background_Companion");
	static constexpr const TCHAR* PanelFrameTexturePath = TEXT("/Game/GameXXK/UI/Town/Textures/Backpack/T_TownBackpack_WindowFrame.T_TownBackpack_WindowFrame");
	static constexpr const TCHAR* RosterSlotTexturePath = TEXT("/Game/GameXXK/UI/Town/Textures/PSD/Companion/T_TownPsd_CompanionCardFrame.T_TownPsd_CompanionCardFrame");
	static constexpr const TCHAR* ActionButtonTexturePath = TEXT("/Game/GameXXK/UI/Town/Textures/PSD/Controls/T_TownPsd_ButtonPrimary.T_TownPsd_ButtonPrimary");
	static constexpr const TCHAR* CardFrameTexturePath = TEXT("/Game/GameXXK/UI/Cards/Textures/T_CardFrame_PSD057.T_CardFrame_PSD057");
	static constexpr const TCHAR* HeroCardPortraitTexturePath = TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Hero.T_CardPortrait_Hero");
	static constexpr const TCHAR* TusiChiefCardPortraitTexturePath = TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Npc_TusiChief.T_CardPortrait_Npc_TusiChief");
	static constexpr const TCHAR* SongJinBaoCardPortraitTexturePath = TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Npc_SongJinBao.T_CardPortrait_Npc_SongJinBao");
	static constexpr const TCHAR* YueBaiCardPortraitTexturePath = TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Npc_YueBai.T_CardPortrait_Npc_YueBai");
	static constexpr const TCHAR* ZhouGuangZuCardPortraitTexturePath = TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Npc_ZhouGuangZu.T_CardPortrait_Npc_ZhouGuangZu");
	static constexpr const TCHAR* JinGuiCardPortraitTexturePath = TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Npc_JinGui.T_CardPortrait_Npc_JinGui");
	static constexpr const TCHAR* QiongMeiErCardPortraitTexturePath = TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Npc_QiongMeiEr.T_CardPortrait_Npc_QiongMeiEr");
	static constexpr const TCHAR* BladeCardPortraitTexturePath = TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Role_Blade.T_CardPortrait_Role_Blade");
	static constexpr const TCHAR* GuardCardPortraitTexturePath = TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Role_Guard.T_CardPortrait_Role_Guard");
	static constexpr const TCHAR* HealerCardPortraitTexturePath = TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Role_Healer.T_CardPortrait_Role_Healer");
	static constexpr const TCHAR* HunterCardPortraitTexturePath = TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Role_Hunter.T_CardPortrait_Role_Hunter");
	static constexpr const TCHAR* SorcererCardPortraitTexturePath = TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Role_Sorcerer.T_CardPortrait_Role_Sorcerer");
	static constexpr const TCHAR* FormationMasterCardPortraitTexturePath = TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Role_FormationMaster.T_CardPortrait_Role_FormationMaster");

	UTexture2D* LoadTexture(const TCHAR* Path)
	{
		return Path ? LoadObject<UTexture2D>(nullptr, Path) : nullptr;
	}

	FSlateBrush MakeTextureBrush(const TCHAR* Path, const FVector2D& ImageSize, const FLinearColor& Tint = FLinearColor::White)
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(LoadTexture(Path));
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.ImageSize = ImageSize;
		Brush.TintColor = FSlateColor(Tint);
		return Brush;
	}

	FSlateBrush MakeBoxTextureBrush(const TCHAR* Path, const FVector2D& ImageSize, const FMargin& Margin = FMargin(0.065f))
	{
		FSlateBrush Brush = MakeTextureBrush(Path, ImageSize);
		Brush.DrawAs = ESlateBrushDrawType::Box;
		Brush.Margin = Margin;
		return Brush;
	}

	FButtonStyle MakeBoxTextureButtonStyle(const TCHAR* Path, const FVector2D& ImageSize, const FMargin& Margin)
	{
		FButtonStyle Style;
		FSlateBrush Brush = MakeBoxTextureBrush(Path, ImageSize, Margin);
		Style.SetNormal(Brush);
		Style.SetHovered(Brush);
		Style.SetPressed(Brush);
		Style.SetDisabled(Brush);
		return Style;
	}

	FButtonStyle MakeCardButtonStyle()
	{
		FButtonStyle Style;
		const FSlateBrush FrameBrush = MakeTextureBrush(CardFrameTexturePath, PersonalCardSize);
		Style.SetNormal(FrameBrush);
		Style.SetHovered(FrameBrush);
		Style.SetPressed(FrameBrush);
		Style.SetDisabled(FrameBrush);
		return Style;
	}

	UTextBlock* MakeText(
		UWidgetTree* WidgetTree,
		const FText& Text,
		const int32 FontSize = 16,
		const FLinearColor& Color = FLinearColor(0.12f, 0.09f, 0.06f, 1.0f),
		const FName WidgetName = NAME_None)
	{
		if (!WidgetTree)
		{
			return nullptr;
		}

		UTextBlock* TextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), WidgetName);
		TextBlock->SetText(Text);
		TextBlock->SetColorAndOpacity(FSlateColor(Color));
		TextBlock->SetAutoWrapText(true);
		FSlateFontInfo Font = TextBlock->GetFont();
		Font.Size = FontSize;
		TextBlock->SetFont(Font);
		return TextBlock;
	}

	void AddCanvasChild(
		UCanvasPanel* Canvas,
		UWidget* Child,
		const FVector2D& Position,
		const FVector2D& Size,
		const FAnchors& Anchors = FAnchors(0.0f, 0.0f),
		const FVector2D& Alignment = FVector2D::ZeroVector)
	{
		if (!Canvas || !Child)
		{
			return;
		}

		if (UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Child))
		{
			Slot->SetAnchors(Anchors);
			Slot->SetAlignment(Alignment);
			Slot->SetPosition(Position);
			Slot->SetSize(Size);
		}
	}

	FText GetRoleDisplayName(const EGameXXKCharacterRole Role)
	{
		switch (Role)
		{
		case EGameXXKCharacterRole::Blade: return NSLOCTEXT("GameXXKCompanionRoster", "RoleBlade", "刀客");
		case EGameXXKCharacterRole::Guard: return NSLOCTEXT("GameXXKCompanionRoster", "RoleGuard", "护卫");
		case EGameXXKCharacterRole::Healer: return NSLOCTEXT("GameXXKCompanionRoster", "RoleHealer", "医师");
		case EGameXXKCharacterRole::Hunter: return NSLOCTEXT("GameXXKCompanionRoster", "RoleHunter", "猎手");
		case EGameXXKCharacterRole::Sorcerer: return NSLOCTEXT("GameXXKCompanionRoster", "RoleSorcerer", "术士");
		case EGameXXKCharacterRole::FormationMaster: return NSLOCTEXT("GameXXKCompanionRoster", "RoleFormationMaster", "阵师");
		default: return NSLOCTEXT("GameXXKCompanionRoster", "RoleUnknown", "未知职业");
		}
	}

	FLinearColor ResolveCardInfoStripTint(const FGameXXKCardDefinition& Definition)
	{
		if (Definition.Owner == EGameXXKCardOwner::Hero)
		{
			return FLinearColor(0.945f, 0.894f, 0.800f, 1.0f);
		}
		if (Definition.Owner == EGameXXKCardOwner::QuestNpc)
		{
			return FLinearColor(0.145f, 0.137f, 0.129f, 1.0f);
		}

		switch (Definition.Role)
		{
		case EGameXXKCharacterRole::Blade: return FLinearColor(0.714f, 0.282f, 0.247f, 1.0f);
		case EGameXXKCharacterRole::Guard: return FLinearColor(0.145f, 0.302f, 0.302f, 1.0f);
		case EGameXXKCharacterRole::Healer: return FLinearColor(0.353f, 0.576f, 0.427f, 1.0f);
		case EGameXXKCharacterRole::Hunter: return FLinearColor(0.604f, 0.408f, 0.200f, 1.0f);
		case EGameXXKCharacterRole::Sorcerer: return FLinearColor(0.251f, 0.318f, 0.553f, 1.0f);
		case EGameXXKCharacterRole::FormationMaster: return FLinearColor(0.502f, 0.384f, 0.475f, 1.0f);
		default: return FLinearColor(0.882f, 0.827f, 0.722f, 1.0f);
		}
	}

	FLinearColor ResolveCardInfoInkTint(const FGameXXKCardDefinition& Definition)
	{
		if (Definition.Owner == EGameXXKCardOwner::Hero || Definition.Owner == EGameXXKCardOwner::Route)
		{
			return FLinearColor(0.137f, 0.118f, 0.098f, 1.0f);
		}
		if (Definition.Owner == EGameXXKCardOwner::QuestNpc)
		{
			return FLinearColor(0.722f, 0.706f, 0.671f, 1.0f);
		}
		return FLinearColor(0.953f, 0.941f, 0.914f, 1.0f);
	}

	FString ResolveCardPortraitResourcePath(const FGameXXKCardDefinition& Definition)
	{
		if (Definition.Owner == EGameXXKCardOwner::Hero)
		{
			return HeroCardPortraitTexturePath;
		}
		if (Definition.Owner == EGameXXKCardOwner::QuestNpc)
		{
			const FName NpcId = Definition.NpcId.IsNone() ? Definition.OwnerId : Definition.NpcId;
			if (NpcId == TEXT("Npc.TusiChief")) return TusiChiefCardPortraitTexturePath;
			if (NpcId == TEXT("Npc.SongJinBao")) return SongJinBaoCardPortraitTexturePath;
			if (NpcId == TEXT("Npc.YueBai")) return YueBaiCardPortraitTexturePath;
			if (NpcId == TEXT("Npc.ZhouGuangZu")) return ZhouGuangZuCardPortraitTexturePath;
			if (NpcId == TEXT("Npc.JinGui")) return JinGuiCardPortraitTexturePath;
			if (NpcId == TEXT("Npc.QiongMeiEr")) return QiongMeiErCardPortraitTexturePath;
			return FString();
		}
		if (Definition.Owner == EGameXXKCardOwner::Profession)
		{
			switch (Definition.Role)
			{
			case EGameXXKCharacterRole::Blade: return BladeCardPortraitTexturePath;
			case EGameXXKCharacterRole::Guard: return GuardCardPortraitTexturePath;
			case EGameXXKCharacterRole::Healer: return HealerCardPortraitTexturePath;
			case EGameXXKCharacterRole::Hunter: return HunterCardPortraitTexturePath;
			case EGameXXKCharacterRole::Sorcerer: return SorcererCardPortraitTexturePath;
			case EGameXXKCharacterRole::FormationMaster: return FormationMasterCardPortraitTexturePath;
			default: return FString();
			}
		}

		return FString();
	}

	FString BuildCardSummary(const TArray<FName>& CardIds)
	{
		TArray<FString> DisplayNames;
		DisplayNames.Reserve(CardIds.Num());
		for (const FName CardId : CardIds)
		{
			const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(CardId);
			DisplayNames.Add(Definition ? Definition->DisplayName.ToString() : CardId.ToString());
		}
		return FString::Join(DisplayNames, TEXT("、"));
	}

	void BuildCardFace(
		UWidgetTree* WidgetTree,
		UGameXXKCompanionRosterCardButton* CardButton,
		const FGameXXKCardDefinition* Definition,
		const bool bSelected,
		const bool bUnlocked)
	{
		if (!WidgetTree || !CardButton)
		{
			return;
		}

		CardButton->SetStyle(MakeCardButtonStyle());
		CardButton->SetBackgroundColor(FLinearColor::White);
		UCanvasPanel* FaceCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
		CardButton->AddChild(FaceCanvas);

		UImage* Portrait = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		const FString PortraitPath = Definition ? ResolveCardPortraitResourcePath(*Definition) : FString();
		if (UTexture2D* PortraitTexture = PortraitPath.IsEmpty() ? nullptr : LoadObject<UTexture2D>(nullptr, *PortraitPath))
		{
			Portrait->SetBrushFromTexture(PortraitTexture, true);
			Portrait->SetColorAndOpacity(FLinearColor::White);
			Portrait->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			Portrait->SetVisibility(ESlateVisibility::Collapsed);
		}
		AddCanvasChild(FaceCanvas, Portrait, FVector2D(16.0f, 14.0f), FVector2D(81.0f, 68.0f));

		UBorder* InfoStrip = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		InfoStrip->SetPadding(FMargin(2.0f, 1.0f, 2.0f, 1.0f));
		InfoStrip->SetHorizontalAlignment(HAlign_Center);
		InfoStrip->SetVerticalAlignment(VAlign_Center);
		InfoStrip->SetBrushColor(Definition ? ResolveCardInfoStripTint(*Definition) : FLinearColor(0.882f, 0.827f, 0.722f, 1.0f));
		AddCanvasChild(FaceCanvas, InfoStrip, FVector2D(12.0f, 87.0f), FVector2D(89.0f, 27.0f));

		const FString DisplayName = Definition ? Definition->DisplayName.ToString() : TEXT("未知牌");
		const FString StateLabel = bSelected ? TEXT("已编入") : (bUnlocked ? TEXT("候选") : TEXT("未解锁"));
		UTextBlock* Label = MakeText(
			WidgetTree,
			FText::FromString(FString::Printf(TEXT("%s\n%s"), *DisplayName, *StateLabel)),
			10,
			Definition ? ResolveCardInfoInkTint(*Definition) : FLinearColor(0.137f, 0.118f, 0.098f, 1.0f));
		if (Label)
		{
			Label->SetJustification(ETextJustify::Center);
			Label->SetAutoWrapText(false);
			InfoStrip->SetContent(Label);
		}
	}
}

void UGameXXKCompanionRosterSlotButton::Configure(UGameXXKCompanionRosterWidget* InOwner, const int32 InSlotIndex)
{
	Owner = InOwner;
	SlotIndex = InSlotIndex;
	OnClicked.AddDynamic(this, &UGameXXKCompanionRosterSlotButton::HandleClicked);
}

void UGameXXKCompanionRosterSlotButton::HandleClicked()
{
	if (Owner)
	{
		Owner->HandleConfiguredRosterSlotClicked(SlotIndex);
	}
}

void UGameXXKCompanionRosterCardButton::Configure(
	UGameXXKCompanionRosterWidget* InOwner,
	const FName InCardId,
	const bool bInHeroDeck)
{
	Owner = InOwner;
	CardId = InCardId;
	bHeroDeckCard = bInHeroDeck;
	OnClicked.RemoveDynamic(this, &UGameXXKCompanionRosterCardButton::HandleClicked);
	OnClicked.AddDynamic(this, &UGameXXKCompanionRosterCardButton::HandleClicked);
	OnHovered.RemoveDynamic(this, &UGameXXKCompanionRosterCardButton::HandleHovered);
	OnHovered.AddDynamic(this, &UGameXXKCompanionRosterCardButton::HandleHovered);
	OnUnhovered.RemoveDynamic(this, &UGameXXKCompanionRosterCardButton::HandleUnhovered);
	OnUnhovered.AddDynamic(this, &UGameXXKCompanionRosterCardButton::HandleUnhovered);
}

void UGameXXKCompanionRosterCardButton::HandleClicked()
{
	if (Owner)
	{
		if (bHeroDeckCard)
		{
			Owner->HandleConfiguredHeroCardClicked(CardId);
		}
		else
		{
			Owner->HandleConfiguredPersonalCardClicked(CardId);
		}
	}
}

void UGameXXKCompanionRosterCardButton::HandleHovered()
{
	if (Owner && !CardId.IsNone())
	{
		Owner->HandleConfiguredCardHoverChanged(CardId, bHeroDeckCard, true);
	}
}

void UGameXXKCompanionRosterCardButton::HandleUnhovered()
{
	if (Owner && !CardId.IsNone())
	{
		Owner->HandleConfiguredCardHoverChanged(CardId, bHeroDeckCard, false);
	}
}

TSharedRef<SWidget> UGameXXKCompanionRosterWidget::RebuildWidget()
{
	BuildProgrammaticLayout();
	return Super::RebuildWidget();
}

void UGameXXKCompanionRosterWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildProgrammaticLayout();
	RefreshFromState();
}

void UGameXXKCompanionRosterWidget::RefreshFromState()
{
	ClearCardTooltipHoverState();
	BuildProgrammaticLayout();
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (Subsystem)
	{
		// A new save reaches town before any battle setup has touched CardRun.  Prepare the fixed
		// twelve-card hero pool before this presentation layer performs its read-only snapshots.
		Subsystem->PrepareCompanionRosterForTown();
	}
	CachedRoster = Subsystem ? Subsystem->GetPermanentCompanionViews() : TArray<FGameXXKPermanentCompanion>();
	HeroCardSummary = Subsystem ? Subsystem->GetHeroCardLoadout() : TArray<FName>();
	TaskNpcCardSummary = Subsystem ? Subsystem->GetQuestNpcCardLoadout() : FGameXXKQuestNpcCardSelection();
	RefreshHeroCardData();
	bLoadoutReadOnly = !Subsystem || Subsystem->IsCompanionLoadoutMutationLocked();
	bRecruitmentActionsReadOnly = !Subsystem
		|| bLoadoutReadOnly
		|| Subsystem->GetRuntimeState().Screen != EGameXXKScreen::Town;
	RosterCapacity = Subsystem ? FMath::Clamp(Subsystem->GetPermanentCompanionRosterCapacity(), 0, RosterSlotCount) : RosterSlotCount;
	PendingRecruitmentCandidate = FGameXXKPermanentCompanion();
	SigilCount = Subsystem ? Subsystem->GetPermanentCompanionSigilCount() : 0;
	if (Subsystem)
	{
		Subsystem->TryGetPendingPermanentCompanionRecruitment(PendingRecruitmentCandidate);
	}

	if (!IsCurrentSelectedCompanion(SelectedCompanionId))
	{
		SelectedCompanionId = CachedRoster.IsEmpty() ? NAME_None : CachedRoster[0].InstanceId;
	}

	VisibleRosterSlotInstanceIds.Init(NAME_None, RosterSlotCount);
	for (int32 RosterIndex = 0; RosterIndex < CachedRoster.Num() && RosterIndex < RosterSlotCount; ++RosterIndex)
	{
		VisibleRosterSlotInstanceIds[RosterIndex] = CachedRoster[RosterIndex].InstanceId;
	}
	RefreshSelectedCompanionData();
	RefreshProgrammaticLayout();
}

bool UGameXXKCompanionRosterWidget::SelectCompanion(const FName InstanceId)
{
	if (!IsCurrentSelectedCompanion(InstanceId))
	{
		return false;
	}

	ClearCardTooltipHoverState();
	SelectedCompanionId = InstanceId;
	bEditingHeroDeck = false;
	RefreshSelectedCompanionData();
	RefreshProgrammaticLayout();
	return true;
}

bool UGameXXKCompanionRosterWidget::ToggleSelectedCompanionCard(const FName CardId)
{
	if (bLoadoutReadOnly || SelectedCompanionId.IsNone() || CardId.IsNone() || !UnlockedPersonalCardIds.Contains(CardId))
	{
		return false;
	}

	if (PendingPersonalCardIds.Contains(CardId))
	{
		PendingPersonalCardIds.RemoveSingle(CardId);
	}
	else
	{
		if (PendingPersonalCardIds.Num() >= 5)
		{
			return false;
		}
		PendingPersonalCardIds.Add(CardId);
	}

	RefreshProgrammaticLayout();
	return true;
}

bool UGameXXKCompanionRosterWidget::ApplySelectedCompanionCardLoadout()
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (bLoadoutReadOnly || !Subsystem || SelectedCompanionId.IsNone() || PendingPersonalCardIds.Num() != 5)
	{
		return false;
	}

	if (!Subsystem->SetPermanentCompanionCardLoadout(SelectedCompanionId, PendingPersonalCardIds))
	{
		return false;
	}

	RefreshFromState();
	return true;
}

bool UGameXXKCompanionRosterWidget::SetSelectedCompanionAsActive()
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (bLoadoutReadOnly || !Subsystem || SelectedCompanionId.IsNone())
	{
		return false;
	}

	if (!Subsystem->SetActivePermanentCompanion(SelectedCompanionId))
	{
		return false;
	}

	RefreshFromState();
	return true;
}

bool UGameXXKCompanionRosterWidget::ClearActivePermanentCompanion()
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (bRecruitmentActionsReadOnly || !Subsystem)
	{
		return false;
	}

	if (!Subsystem->ClearActivePermanentCompanion())
	{
		RecruitmentFeedback = TEXT("暂不编入未完成：该操作仅可在城镇的可编辑状态执行。");
		RefreshProgrammaticLayout();
		return false;
	}

	RecruitmentFeedback = TEXT("已暂不编入永久伙伴；本次路线可仅由主角与任务 NPC 出战。");
	RefreshFromState();
	return true;
}

bool UGameXXKCompanionRosterWidget::OpenHeroDeckEditor()
{
	if (VisibleHeroCardIds.IsEmpty())
	{
		return false;
	}

	bEditingHeroDeck = true;
	RefreshProgrammaticLayout();
	return true;
}

bool UGameXXKCompanionRosterWidget::ToggleHeroCard(const FName CardId)
{
	if (bLoadoutReadOnly || !bEditingHeroDeck || CardId.IsNone()
		|| !VisibleHeroCardIds.Contains(CardId)
		|| !UnlockedHeroCardIds.Contains(CardId))
	{
		return false;
	}

	if (PendingHeroCardIds.Contains(CardId))
	{
		PendingHeroCardIds.RemoveSingle(CardId);
	}
	else
	{
		if (PendingHeroCardIds.Num() >= 8)
		{
			return false;
		}
		PendingHeroCardIds.Add(CardId);
	}

	RefreshProgrammaticLayout();
	return true;
}

bool UGameXXKCompanionRosterWidget::ApplyHeroCardLoadout()
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (bLoadoutReadOnly || !bEditingHeroDeck || !Subsystem || PendingHeroCardIds.Num() != 8)
	{
		return false;
	}

	if (!Subsystem->SetHeroCardLoadout(PendingHeroCardIds))
	{
		return false;
	}

	RefreshFromState();
	return true;
}

bool UGameXXKCompanionRosterWidget::BeginRandomRecruitment()
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (bRecruitmentActionsReadOnly || !Subsystem)
	{
		return false;
	}

	FGameXXKCompanionRecruitResult Result;
	if (!Subsystem->StartRandomPermanentCompanionRecruitment(Result))
	{
		return false;
	}

	switch (Result.Outcome)
	{
	case EGameXXKCompanionRecruitOutcome::Recruited:
		SelectedCompanionId = Result.Companion.InstanceId;
		bEditingHeroDeck = false;
		RecruitmentFeedback = FString::Printf(TEXT("已招募：%s · 已自动选中，可配置个人牌组。"), *GetRoleDisplayName(Result.Companion.Role).ToString());
		break;
	case EGameXXKCompanionRecruitOutcome::DuplicateSigil:
		RecruitmentFeedback = FString::Printf(TEXT("招募重复：%s 转化为 1 枚升星印（现有 %d 枚）。"),
			*GetRoleDisplayName(Result.Companion.Role).ToString(),
			Subsystem->GetPermanentCompanionSigilCount());
		break;
	case EGameXXKCompanionRecruitOutcome::PendingReplacement:
		RecruitmentFeedback = TEXT("名册已满：新伙伴已固定保存，请选择替换对象或放弃候选。");
		break;
	default:
		RecruitmentFeedback = TEXT("招贤未能完成，请稍后重试。");
		break;
	}

	RefreshFromState();
	return true;
}

bool UGameXXKCompanionRosterWidget::ResolvePendingRecruitmentWithSelectedCompanion()
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (bRecruitmentActionsReadOnly || !Subsystem || SelectedCompanionId.IsNone() || PendingRecruitmentCandidate.InstanceId.IsNone())
	{
		return false;
	}

	// The player has explicitly selected the replaced slot. If that slot is currently deployed, the
	// candidate visibly inherits that one optional party position instead of leaving an invalid party.
	const FName ActiveAfterReplacement = SelectedCompanionProfile.bIsActive
		? PendingRecruitmentCandidate.InstanceId
		: NAME_None;
	if (!Subsystem->ResolvePendingPermanentCompanionReplacement(SelectedCompanionId, ActiveAfterReplacement))
	{
		RecruitmentFeedback = TEXT("替换未完成：请先卸下装备并清空已投入经验，再替换该伙伴。");
		RefreshProgrammaticLayout();
		return false;
	}

	SelectedCompanionId = PendingRecruitmentCandidate.InstanceId;
	bEditingHeroDeck = false;
	RecruitmentFeedback = TEXT("已完成替换，并自动选中新伙伴。");
	RefreshFromState();
	return true;
}

bool UGameXXKCompanionRosterWidget::DiscardPendingRecruitment()
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (bRecruitmentActionsReadOnly || !Subsystem || PendingRecruitmentCandidate.InstanceId.IsNone())
	{
		return false;
	}

	if (!Subsystem->DiscardPendingPermanentCompanionRecruitment())
	{
		RecruitmentFeedback = TEXT("放弃候选未完成，请稍后重试。");
		RefreshProgrammaticLayout();
		return false;
	}

	RecruitmentFeedback = TEXT("已放弃本次候选；下次招贤将继续后续卡池顺序。");
	RefreshFromState();
	return true;
}

bool UGameXXKCompanionRosterWidget::PromoteSelectedCompanionStar()
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (bRecruitmentActionsReadOnly || !Subsystem || SelectedCompanionId.IsNone() || SigilCount <= 0)
	{
		return false;
	}

	if (!Subsystem->PromotePermanentCompanionStar(SelectedCompanionId))
	{
		return false;
	}

	RefreshFromState();
	return true;
}

FGameXXKCompanionRosterProfileView UGameXXKCompanionRosterWidget::GetSelectedCompanionProfile() const
{
	return SelectedCompanionProfile;
}

TArray<FName> UGameXXKCompanionRosterWidget::GetVisiblePersonalCardIds() const
{
	return VisiblePersonalCardIds;
}

TArray<FName> UGameXXKCompanionRosterWidget::GetPendingPersonalCardIds() const
{
	return PendingPersonalCardIds;
}

TArray<FName> UGameXXKCompanionRosterWidget::GetHeroCardSummary() const
{
	return HeroCardSummary;
}

TArray<FName> UGameXXKCompanionRosterWidget::GetVisibleHeroCardIds() const
{
	return VisibleHeroCardIds;
}

TArray<FName> UGameXXKCompanionRosterWidget::GetPendingHeroCardIds() const
{
	return PendingHeroCardIds;
}

FGameXXKQuestNpcCardSelection UGameXXKCompanionRosterWidget::GetTaskNpcCardSummary() const
{
	return TaskNpcCardSummary;
}

void UGameXXKCompanionRosterWidget::HandleConfiguredRosterSlotClicked(const int32 SlotIndex)
{
	if (VisibleRosterSlotInstanceIds.IsValidIndex(SlotIndex))
	{
		SelectCompanion(VisibleRosterSlotInstanceIds[SlotIndex]);
	}
}

void UGameXXKCompanionRosterWidget::HandleConfiguredPersonalCardClicked(const FName CardId)
{
	ToggleSelectedCompanionCard(CardId);
}

void UGameXXKCompanionRosterWidget::HandleConfiguredHeroCardClicked(const FName CardId)
{
	ToggleHeroCard(CardId);
}

void UGameXXKCompanionRosterWidget::HandleConfiguredCardHoverChanged(
	const FName CardId,
	const bool bInHeroDeck,
	const bool bHovered)
{
	if (bHovered)
	{
		HoveredCardTooltipId = CardId;
		bHoveredCardTooltipIsHeroDeck = bInHeroDeck;
	}
	else if (HoveredCardTooltipId == CardId && bHoveredCardTooltipIsHeroDeck == bInHeroDeck)
	{
		HoveredCardTooltipId = NAME_None;
		bHoveredCardTooltipIsHeroDeck = false;
	}
	RefreshCardTooltip();
}

int32 UGameXXKCompanionRosterWidget::GetRosterSlotCountForTest() const
{
	return RosterSlotButtons.Num();
}

int32 UGameXXKCompanionRosterWidget::GetRosterColumnCountForTest() const
{
	return RosterColumnCount;
}

FString UGameXXKCompanionRosterWidget::GetWindowFrameResourcePathForTest() const
{
	return WindowFrameTexturePath;
}

FString UGameXXKCompanionRosterWidget::GetRosterSlotResourcePathForTest() const
{
	return RosterSlotTexturePath;
}

FString UGameXXKCompanionRosterWidget::GetPersonalCardFrameResourcePathForTest() const
{
	return CardFrameTexturePath;
}

FName UGameXXKCompanionRosterWidget::GetSelectedCompanionIdForTest() const
{
	return SelectedCompanionId;
}

bool UGameXXKCompanionRosterWidget::IsLoadoutReadOnlyForTest() const
{
	return bLoadoutReadOnly;
}

bool UGameXXKCompanionRosterWidget::HasPersonalCardScrollBoxForTest() const
{
	return PersonalCardScroll != nullptr;
}

FString UGameXXKCompanionRosterWidget::GetPersonalCardScrollTrackResourcePathForTest() const
{
	if (!PersonalCardScroll)
	{
		return FString();
	}

	const UObject* Resource = PersonalCardScroll->GetWidgetBarStyle().VerticalBackgroundImage.GetResourceObject();
	return Resource ? Resource->GetPathName() : FString();
}

FString UGameXXKCompanionRosterWidget::GetPersonalCardScrollThumbResourcePathForTest() const
{
	if (!PersonalCardScroll)
	{
		return FString();
	}

	const UObject* Resource = PersonalCardScroll->GetWidgetBarStyle().NormalThumbImage.GetResourceObject();
	return Resource ? Resource->GetPathName() : FString();
}

bool UGameXXKCompanionRosterWidget::HasPendingRecruitmentForTest() const
{
	return !PendingRecruitmentCandidate.InstanceId.IsNone();
}

FName UGameXXKCompanionRosterWidget::GetPendingRecruitmentCandidateIdForTest() const
{
	return PendingRecruitmentCandidate.InstanceId;
}

FString UGameXXKCompanionRosterWidget::GetRecruitmentStatusForTest() const
{
	return RecruitmentStatusText ? RecruitmentStatusText->GetText().ToString() : FString();
}

bool UGameXXKCompanionRosterWidget::IsHeroDeckEditorOpenForTest() const
{
	return bEditingHeroDeck;
}

FString UGameXXKCompanionRosterWidget::GetCardTooltipTextForTest() const
{
	return CardTooltipText ? CardTooltipText->GetText().ToString() : FString();
}

bool UGameXXKCompanionRosterWidget::IsCardTooltipVisibleForTest() const
{
	return CardTooltipPanel
		&& CardTooltipPanel->GetVisibility() != ESlateVisibility::Collapsed
		&& CardTooltipPanel->GetVisibility() != ESlateVisibility::Hidden;
}

bool UGameXXKCompanionRosterWidget::IsCardTooltipHitTestInvisibleForTest() const
{
	return CardTooltipPanel && CardTooltipPanel->GetVisibility() == ESlateVisibility::HitTestInvisible;
}

void UGameXXKCompanionRosterWidget::BuildProgrammaticLayout()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("CompanionRosterWidgetTree"));
	}
	if (!WidgetTree || RootCanvas)
	{
		return;
	}

	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CompanionRosterRoot"));
	WidgetTree->RootWidget = RootCanvas;

	WindowFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CompanionRosterWindowFrame"));
	WindowFrame->SetBrush(MakeBoxTextureBrush(WindowFrameTexturePath, RosterWindowSize));
	WindowFrame->SetBrushColor(FLinearColor::White);
	WindowFrame->SetPadding(FMargin(34.0f, 30.0f, 34.0f, 30.0f));
	AddCanvasChild(RootCanvas, WindowFrame, FVector2D::ZeroVector, RosterWindowSize, FAnchors(0.5f, 0.5f), FVector2D(0.5f, 0.5f));

	FrameCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CompanionRosterFrameCanvas"));
	WindowFrame->AddChild(FrameCanvas);

	UTextBlock* Title = MakeText(WidgetTree, NSLOCTEXT("GameXXKCompanionRoster", "Title", "伙伴行囊"), 28, FLinearColor(0.08f, 0.06f, 0.04f, 1.0f), TEXT("CompanionRosterTitle"));
	AddCanvasChild(FrameCanvas, Title, FVector2D(16.0f, 4.0f), FVector2D(250.0f, 42.0f));
	RosterCountText = MakeText(WidgetTree, FText::GetEmpty(), 15, FLinearColor(0.18f, 0.13f, 0.08f, 1.0f), TEXT("CompanionRosterCount"));
	AddCanvasChild(FrameCanvas, RosterCountText, FVector2D(270.0f, 10.0f), FVector2D(180.0f, 30.0f));

	UBorder* RosterPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CompanionRosterBackpackPanel"));
	RosterPanel->SetBrush(MakeBoxTextureBrush(PanelFrameTexturePath, FVector2D(336.0f, 580.0f)));
	RosterPanel->SetBrushColor(FLinearColor::White);
	RosterPanel->SetPadding(FMargin(16.0f));
	AddCanvasChild(FrameCanvas, RosterPanel, FVector2D(16.0f, 62.0f), FVector2D(336.0f, 580.0f));

	UCanvasPanel* RosterPanelCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CompanionRosterBackpackCanvas"));
	RosterPanel->AddChild(RosterPanelCanvas);
	UTextBlock* RosterCaption = MakeText(WidgetTree, NSLOCTEXT("GameXXKCompanionRoster", "RosterCaption", "永久伙伴（最多 12 名）"), 17, FLinearColor(0.10f, 0.07f, 0.04f, 1.0f));
	AddCanvasChild(RosterPanelCanvas, RosterCaption, FVector2D(0.0f, 0.0f), FVector2D(290.0f, 32.0f));
	RosterGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("CompanionRosterGrid"));
	AddCanvasChild(RosterPanelCanvas, RosterGrid, FVector2D(0.0f, 38.0f), FVector2D(300.0f, 320.0f));

	for (int32 SlotIndex = 0; SlotIndex < RosterSlotCount; ++SlotIndex)
	{
		UGameXXKCompanionRosterSlotButton* SlotButton = WidgetTree->ConstructWidget<UGameXXKCompanionRosterSlotButton>(UGameXXKCompanionRosterSlotButton::StaticClass(), *FString::Printf(TEXT("CompanionRosterSlot_%02d"), SlotIndex));
		SlotButton->Configure(this, SlotIndex);
		SlotButton->SetStyle(MakeBoxTextureButtonStyle(RosterSlotTexturePath, RosterSlotSize, SlotFrameMargin));
		SlotButton->SetBackgroundColor(FLinearColor::White);

		UOverlay* SlotOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		UTextBlock* SlotLabel = MakeText(WidgetTree, FText::GetEmpty(), 12, FLinearColor(0.12f, 0.09f, 0.06f, 1.0f));
		SlotLabel->SetJustification(ETextJustify::Center);
		SlotOverlay->AddChildToOverlay(SlotLabel);
		UBorder* SelectionBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		SelectionBorder->SetBrushColor(FLinearColor(0.94f, 0.75f, 0.31f, 0.35f));
		SelectionBorder->SetVisibility(ESlateVisibility::Collapsed);
		if (UOverlaySlot* SelectionSlot = SlotOverlay->AddChildToOverlay(SelectionBorder))
		{
			SelectionSlot->SetHorizontalAlignment(HAlign_Fill);
			SelectionSlot->SetVerticalAlignment(VAlign_Fill);
		}
		SlotButton->AddChild(SlotOverlay);

		USizeBox* SlotSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *FString::Printf(TEXT("CompanionRosterSlotSize_%02d"), SlotIndex));
		SlotSizeBox->SetWidthOverride(RosterSlotSize.X);
		SlotSizeBox->SetHeightOverride(RosterSlotSize.Y);
		SlotSizeBox->AddChild(SlotButton);
		if (UUniformGridSlot* GridSlot = RosterGrid->AddChildToUniformGrid(SlotSizeBox, SlotIndex / RosterColumnCount, SlotIndex % RosterColumnCount))
		{
			GridSlot->SetHorizontalAlignment(HAlign_Center);
			GridSlot->SetVerticalAlignment(VAlign_Center);
		}
		RosterSlotButtons.Add(SlotButton);
		RosterSlotLabels.Add(SlotLabel);
		RosterSlotSelectionBorders.Add(SelectionBorder);
	}

	RecruitButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CompanionRosterRecruitAction"));
	RecruitButton->SetStyle(MakeBoxTextureButtonStyle(ActionButtonTexturePath, FVector2D(304.0f, 36.0f), ActionFrameMargin));
	RecruitButton->SetBackgroundColor(FLinearColor::White);
	RecruitButton->OnClicked.AddDynamic(this, &UGameXXKCompanionRosterWidget::HandleRecruitClicked);
	UTextBlock* RecruitButtonText = MakeText(WidgetTree, NSLOCTEXT("GameXXKCompanionRoster", "RecruitAction", "招贤 · 随机伙伴"), 15, FLinearColor(0.10f, 0.08f, 0.05f, 1.0f));
	RecruitButtonText->SetJustification(ETextJustify::Center);
	RecruitButton->AddChild(RecruitButtonText);
	AddCanvasChild(RosterPanelCanvas, RecruitButton, FVector2D(0.0f, 368.0f), FVector2D(304.0f, 36.0f));

	RecruitmentStatusText = MakeText(WidgetTree, FText::GetEmpty(), 12, FLinearColor(0.24f, 0.17f, 0.10f, 1.0f), TEXT("CompanionRosterRecruitmentStatus"));
	AddCanvasChild(RosterPanelCanvas, RecruitmentStatusText, FVector2D(0.0f, 410.0f), FVector2D(304.0f, 48.0f));

	ReplacePendingButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CompanionRosterReplacePendingAction"));
	ReplacePendingButton->SetStyle(MakeBoxTextureButtonStyle(ActionButtonTexturePath, FVector2D(147.0f, 34.0f), ActionFrameMargin));
	ReplacePendingButton->SetBackgroundColor(FLinearColor::White);
	ReplacePendingButton->OnClicked.AddDynamic(this, &UGameXXKCompanionRosterWidget::HandleReplacePendingClicked);
	UTextBlock* ReplacePendingText = MakeText(WidgetTree, NSLOCTEXT("GameXXKCompanionRoster", "ReplacePendingAction", "替换已选伙伴"), 13, FLinearColor(0.10f, 0.08f, 0.05f, 1.0f));
	ReplacePendingText->SetJustification(ETextJustify::Center);
	ReplacePendingButton->AddChild(ReplacePendingText);
	AddCanvasChild(RosterPanelCanvas, ReplacePendingButton, FVector2D(0.0f, 464.0f), FVector2D(147.0f, 34.0f));

	DiscardPendingButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CompanionRosterDiscardPendingAction"));
	DiscardPendingButton->SetStyle(MakeBoxTextureButtonStyle(ActionButtonTexturePath, FVector2D(147.0f, 34.0f), ActionFrameMargin));
	DiscardPendingButton->SetBackgroundColor(FLinearColor::White);
	DiscardPendingButton->OnClicked.AddDynamic(this, &UGameXXKCompanionRosterWidget::HandleDiscardPendingClicked);
	UTextBlock* DiscardPendingText = MakeText(WidgetTree, NSLOCTEXT("GameXXKCompanionRoster", "DiscardPendingAction", "放弃候选"), 13, FLinearColor(0.10f, 0.08f, 0.05f, 1.0f));
	DiscardPendingText->SetJustification(ETextJustify::Center);
	DiscardPendingButton->AddChild(DiscardPendingText);
	AddCanvasChild(RosterPanelCanvas, DiscardPendingButton, FVector2D(157.0f, 464.0f), FVector2D(147.0f, 34.0f));

	UBorder* ProfilePanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CompanionRosterProfilePanel"));
	ProfilePanel->SetBrush(MakeBoxTextureBrush(PanelFrameTexturePath, FVector2D(315.0f, 580.0f)));
	ProfilePanel->SetBrushColor(FLinearColor::White);
	ProfilePanel->SetPadding(FMargin(18.0f));
	AddCanvasChild(FrameCanvas, ProfilePanel, FVector2D(370.0f, 62.0f), FVector2D(315.0f, 580.0f));

	UVerticalBox* ProfileBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CompanionRosterProfileBox"));
	ProfilePanel->AddChild(ProfileBox);
	ProfileTitleText = MakeText(WidgetTree, NSLOCTEXT("GameXXKCompanionRoster", "NoCompanion", "尚未招募伙伴"), 21, FLinearColor(0.08f, 0.06f, 0.04f, 1.0f), TEXT("CompanionRosterProfileTitle"));
	ProfileBox->AddChildToVerticalBox(ProfileTitleText);
	ProfileDetailText = MakeText(WidgetTree, FText::GetEmpty(), 15, FLinearColor(0.12f, 0.09f, 0.06f, 1.0f), TEXT("CompanionRosterProfileDetail"));
	if (UVerticalBoxSlot* DetailSlot = ProfileBox->AddChildToVerticalBox(ProfileDetailText))
	{
		DetailSlot->SetPadding(FMargin(0.0f, 12.0f, 0.0f, 10.0f));
	}
	LoadoutStatusText = MakeText(WidgetTree, FText::GetEmpty(), 14, FLinearColor(0.30f, 0.20f, 0.10f, 1.0f), TEXT("CompanionRosterLoadoutStatus"));
	if (UVerticalBoxSlot* StatusSlot = ProfileBox->AddChildToVerticalBox(LoadoutStatusText))
	{
		StatusSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
	}

	ApplyLoadoutButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CompanionRosterApplyLoadout"));
	ApplyLoadoutButton->SetStyle(MakeBoxTextureButtonStyle(ActionButtonTexturePath, ActionButtonSize, ActionFrameMargin));
	ApplyLoadoutButton->SetBackgroundColor(FLinearColor::White);
	ApplyLoadoutButton->OnClicked.AddDynamic(this, &UGameXXKCompanionRosterWidget::HandleApplyLoadoutClicked);
	ApplyLoadoutButtonText = MakeText(WidgetTree, NSLOCTEXT("GameXXKCompanionRoster", "ApplyLoadout", "确认编入 5 张"), 16, FLinearColor(0.10f, 0.08f, 0.05f, 1.0f));
	ApplyLoadoutButtonText->SetJustification(ETextJustify::Center);
	ApplyLoadoutButton->AddChild(ApplyLoadoutButtonText);
	if (UVerticalBoxSlot* ApplySlot = ProfileBox->AddChildToVerticalBox(ApplyLoadoutButton))
	{
		ApplySlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
		ApplySlot->SetHorizontalAlignment(HAlign_Center);
	}

	SetActiveButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CompanionRosterSetActive"));
	SetActiveButton->SetStyle(MakeBoxTextureButtonStyle(ActionButtonTexturePath, ActionButtonSize, ActionFrameMargin));
	SetActiveButton->SetBackgroundColor(FLinearColor::White);
	SetActiveButton->OnClicked.AddDynamic(this, &UGameXXKCompanionRosterWidget::HandleSetActiveClicked);
	SetActiveButtonText = MakeText(WidgetTree, NSLOCTEXT("GameXXKCompanionRoster", "SetActive", "编入出战队伍"), 16, FLinearColor(0.10f, 0.08f, 0.05f, 1.0f));
	SetActiveButtonText->SetJustification(ETextJustify::Center);
	SetActiveButton->AddChild(SetActiveButtonText);
	if (UVerticalBoxSlot* ActiveSlot = ProfileBox->AddChildToVerticalBox(SetActiveButton))
	{
		ActiveSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
		ActiveSlot->SetHorizontalAlignment(HAlign_Center);
	}

	ClearActiveButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CompanionRosterClearActive"));
	ClearActiveButton->SetStyle(MakeBoxTextureButtonStyle(ActionButtonTexturePath, ActionButtonSize, ActionFrameMargin));
	ClearActiveButton->SetBackgroundColor(FLinearColor::White);
	ClearActiveButton->OnClicked.AddDynamic(this, &UGameXXKCompanionRosterWidget::HandleClearActiveClicked);
	ClearActiveButtonText = MakeText(WidgetTree, NSLOCTEXT("GameXXKCompanionRoster", "ClearActive", "暂不编入"), 16, FLinearColor(0.10f, 0.08f, 0.05f, 1.0f));
	ClearActiveButtonText->SetJustification(ETextJustify::Center);
	ClearActiveButton->AddChild(ClearActiveButtonText);
	if (UVerticalBoxSlot* ClearActiveSlot = ProfileBox->AddChildToVerticalBox(ClearActiveButton))
	{
		ClearActiveSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
		ClearActiveSlot->SetHorizontalAlignment(HAlign_Center);
	}

	PromoteStarButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CompanionRosterPromoteStarAction"));
	PromoteStarButton->SetStyle(MakeBoxTextureButtonStyle(ActionButtonTexturePath, ActionButtonSize, ActionFrameMargin));
	PromoteStarButton->SetBackgroundColor(FLinearColor::White);
	PromoteStarButton->OnClicked.AddDynamic(this, &UGameXXKCompanionRosterWidget::HandlePromoteStarClicked);
	PromoteStarButtonText = MakeText(WidgetTree, NSLOCTEXT("GameXXKCompanionRoster", "PromoteStar", "消耗升星印 · 升星"), 16, FLinearColor(0.10f, 0.08f, 0.05f, 1.0f));
	PromoteStarButtonText->SetJustification(ETextJustify::Center);
	PromoteStarButton->AddChild(PromoteStarButtonText);
	if (UVerticalBoxSlot* PromoteSlot = ProfileBox->AddChildToVerticalBox(PromoteStarButton))
	{
		PromoteSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));
		PromoteSlot->SetHorizontalAlignment(HAlign_Center);
	}

	HeroDeckSummaryText = MakeText(WidgetTree, FText::GetEmpty(), 13, FLinearColor(0.12f, 0.09f, 0.06f, 1.0f), TEXT("CompanionRosterHeroDeckSummary"));
	if (UVerticalBoxSlot* HeroSlot = ProfileBox->AddChildToVerticalBox(HeroDeckSummaryText))
	{
		HeroSlot->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 6.0f));
	}
	TaskNpcDeckSummaryText = MakeText(WidgetTree, FText::GetEmpty(), 13, FLinearColor(0.12f, 0.09f, 0.06f, 1.0f), TEXT("CompanionRosterTaskNpcDeckSummary"));
	ProfileBox->AddChildToVerticalBox(TaskNpcDeckSummaryText);

	UBorder* PersonalDeckPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CompanionRosterPersonalDeckPanel"));
	PersonalDeckPanel->SetBrush(MakeBoxTextureBrush(PanelFrameTexturePath, FVector2D(453.0f, 580.0f)));
	PersonalDeckPanel->SetBrushColor(FLinearColor::White);
	PersonalDeckPanel->SetPadding(FMargin(16.0f));
	AddCanvasChild(FrameCanvas, PersonalDeckPanel, FVector2D(703.0f, 62.0f), FVector2D(453.0f, 580.0f));

	UCanvasPanel* PersonalDeckCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CompanionRosterPersonalDeckCanvas"));
	PersonalDeckPanel->AddChild(PersonalDeckCanvas);
	DeckCaptionText = MakeText(WidgetTree, NSLOCTEXT("GameXXKCompanionRoster", "PersonalDeckCaption", "个人牌组（12 张，编入 5 张）"), 17, FLinearColor(0.10f, 0.07f, 0.04f, 1.0f), TEXT("CompanionRosterDeckCaption"));
	AddCanvasChild(PersonalDeckCanvas, DeckCaptionText, FVector2D::ZeroVector, FVector2D(225.0f, 32.0f));
	HeroDeckToggleButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CompanionRosterHeroDeckToggle"));
	HeroDeckToggleButton->SetStyle(MakeBoxTextureButtonStyle(ActionButtonTexturePath, FVector2D(170.0f, 32.0f), ActionFrameMargin));
	HeroDeckToggleButton->SetBackgroundColor(FLinearColor::White);
	HeroDeckToggleButton->OnClicked.AddDynamic(this, &UGameXXKCompanionRosterWidget::HandleHeroDeckToggleClicked);
	HeroDeckToggleButtonText = MakeText(WidgetTree, NSLOCTEXT("GameXXKCompanionRoster", "OpenHeroDeck", "编辑主角牌组"), 13, FLinearColor(0.10f, 0.08f, 0.05f, 1.0f));
	HeroDeckToggleButtonText->SetJustification(ETextJustify::Center);
	HeroDeckToggleButton->AddChild(HeroDeckToggleButtonText);
	AddCanvasChild(PersonalDeckCanvas, HeroDeckToggleButton, FVector2D(235.0f, 0.0f), FVector2D(170.0f, 32.0f));
	PersonalCardScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("CompanionRosterPersonalCardScroll"));
	PersonalCardScroll->SetOrientation(Orient_Vertical);
	PersonalCardScroll->SetScrollBarVisibility(ESlateVisibility::Visible);
	FGameXXKPartyDeckUiStyle::ApplyPaperInkScrollBar(PersonalCardScroll);
	AddCanvasChild(PersonalDeckCanvas, PersonalCardScroll, FVector2D(0.0f, 40.0f), FVector2D(408.0f, 440.0f));
	PersonalCardGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("CompanionRosterPersonalCardGrid"));
	PersonalCardGrid->SetSlotPadding(FMargin(5.0f));
	PersonalCardScroll->AddChild(PersonalCardGrid);
	ApplyHeroLoadoutButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CompanionRosterApplyHeroLoadout"));
	ApplyHeroLoadoutButton->SetStyle(MakeBoxTextureButtonStyle(ActionButtonTexturePath, FVector2D(196.0f, 36.0f), ActionFrameMargin));
	ApplyHeroLoadoutButton->SetBackgroundColor(FLinearColor::White);
	ApplyHeroLoadoutButton->OnClicked.AddDynamic(this, &UGameXXKCompanionRosterWidget::HandleApplyHeroLoadoutClicked);
	ApplyHeroLoadoutButtonText = MakeText(WidgetTree, NSLOCTEXT("GameXXKCompanionRoster", "ApplyHeroLoadout", "确认编入 8 张"), 14, FLinearColor(0.10f, 0.08f, 0.05f, 1.0f));
	ApplyHeroLoadoutButtonText->SetJustification(ETextJustify::Center);
	ApplyHeroLoadoutButton->AddChild(ApplyHeroLoadoutButtonText);
	ApplyHeroLoadoutButton->SetVisibility(ESlateVisibility::Collapsed);
	AddCanvasChild(PersonalDeckCanvas, ApplyHeroLoadoutButton, FVector2D(0.0f, 488.0f), FVector2D(196.0f, 36.0f));
	HeroDeckStatusText = MakeText(WidgetTree, FText::GetEmpty(), 13, FLinearColor(0.30f, 0.20f, 0.10f, 1.0f), TEXT("CompanionRosterHeroDeckStatus"));
	HeroDeckStatusText->SetJustification(ETextJustify::Right);
	HeroDeckStatusText->SetVisibility(ESlateVisibility::Collapsed);
	AddCanvasChild(PersonalDeckCanvas, HeroDeckStatusText, FVector2D(204.0f, 492.0f), FVector2D(201.0f, 28.0f));

	CardTooltipPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CompanionRosterCardTooltipPanel"));
	CardTooltipPanel->SetBrush(MakeBoxTextureBrush(PanelFrameTexturePath, CardTooltipSize));
	CardTooltipPanel->SetBrushColor(FLinearColor::White);
	CardTooltipPanel->SetPadding(FMargin(20.0f, 16.0f, 20.0f, 14.0f));
	CardTooltipPanel->SetVisibility(ESlateVisibility::Collapsed);
	CardTooltipText = MakeText(
		WidgetTree,
		FText::GetEmpty(),
		14,
		FLinearColor(0.12f, 0.09f, 0.06f, 1.0f),
		TEXT("CompanionRosterCardTooltipText"));
	CardTooltipText->SetJustification(ETextJustify::Left);
	CardTooltipPanel->SetContent(CardTooltipText);
	if (UCanvasPanelSlot* TooltipSlot = RootCanvas->AddChildToCanvas(CardTooltipPanel))
	{
		TooltipSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		TooltipSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		TooltipSlot->SetPosition(FVector2D(330.0f, -144.0f));
		TooltipSlot->SetSize(CardTooltipSize);
		TooltipSlot->SetZOrder(50);
	}
}

void UGameXXKCompanionRosterWidget::RefreshProgrammaticLayout()
{
	BuildProgrammaticLayout();
	RefreshRosterSlots();
	RefreshProfilePanel();
	RefreshRecruitmentPanel();
	RefreshDeckSummaries();
	RefreshPersonalCards();
	RefreshDeckEditorControls();
	RefreshCardTooltip();
}

void UGameXXKCompanionRosterWidget::RefreshSelectedCompanionData()
{
	SelectedCompanionProfile = FGameXXKCompanionRosterProfileView();
	VisiblePersonalCardIds.Reset();
	UnlockedPersonalCardIds.Reset();
	PendingPersonalCardIds.Reset();

	const FGameXXKPermanentCompanion* SelectedCompanion = CachedRoster.FindByPredicate([this](const FGameXXKPermanentCompanion& Companion)
	{
		return Companion.InstanceId == SelectedCompanionId;
	});
	if (!SelectedCompanion)
	{
		return;
	}

	SelectedCompanionProfile.InstanceId = SelectedCompanion->InstanceId;
	SelectedCompanionProfile.Role = SelectedCompanion->Role;
	SelectedCompanionProfile.Level = SelectedCompanion->Level;
	SelectedCompanionProfile.Star = SelectedCompanion->Star;
	SelectedCompanionProfile.Experience = SelectedCompanion->Experience;
	SelectedCompanionProfile.ExperienceRequiredForNextLevel = FGameXXKCompanionRules::GetExperienceRequiredForNextLevel(SelectedCompanion->Level);
	SelectedCompanionProfile.bIsActive = SelectedCompanion->bIsActive;
	FGameXXKCompanionRules::GetCompanionAttributes(
		SelectedCompanion->Role,
		SelectedCompanion->Level,
		SelectedCompanion->Star,
		FGameXXKCompanionAttributes(),
		SelectedCompanionProfile.Attributes,
		nullptr);
	VisiblePersonalCardIds = SelectedCompanion->PersonalCardIds;
	UnlockedPersonalCardIds = SelectedCompanion->UnlockedPersonalCardIds;
	PendingPersonalCardIds = SelectedCompanion->SelectedCardIds;
}

void UGameXXKCompanionRosterWidget::RefreshHeroCardData()
{
	VisibleHeroCardIds.Reset();
	UnlockedHeroCardIds.Reset();
	PendingHeroCardIds.Reset();

	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem)
	{
		return;
	}

	for (const FGameXXKCardDefinition& Definition : FGameXXKCardCatalog::GetCardDefinitionsForOwner(FName(TEXT("Hero"))))
	{
		VisibleHeroCardIds.Add(Definition.Id);
	}
	UnlockedHeroCardIds = Subsystem->GetRuntimeState().CardRun.HeroUnlockedCardIds;
	PendingHeroCardIds = HeroCardSummary;
}

void UGameXXKCompanionRosterWidget::RefreshRosterSlots()
{
	for (int32 SlotIndex = 0; SlotIndex < RosterSlotButtons.Num(); ++SlotIndex)
	{
		const FName InstanceId = VisibleRosterSlotInstanceIds.IsValidIndex(SlotIndex) ? VisibleRosterSlotInstanceIds[SlotIndex] : NAME_None;
		const FGameXXKPermanentCompanion* Companion = CachedRoster.FindByPredicate([InstanceId](const FGameXXKPermanentCompanion& Candidate)
		{
			return Candidate.InstanceId == InstanceId;
		});
		if (UGameXXKCompanionRosterSlotButton* SlotButton = RosterSlotButtons[SlotIndex])
		{
			SlotButton->SetIsEnabled(Companion != nullptr);
		}
		if (UTextBlock* Label = RosterSlotLabels.IsValidIndex(SlotIndex) ? RosterSlotLabels[SlotIndex].Get() : nullptr)
		{
			Label->SetText(Companion
				? FText::FromString(FString::Printf(TEXT("%s\nLv.%d ★%d"), *GetRoleDisplayName(Companion->Role).ToString(), Companion->Level, Companion->Star))
				: NSLOCTEXT("GameXXKCompanionRoster", "EmptyRosterSlot", "空位"));
		}
		if (UBorder* SelectionBorder = RosterSlotSelectionBorders.IsValidIndex(SlotIndex) ? RosterSlotSelectionBorders[SlotIndex].Get() : nullptr)
		{
			SelectionBorder->SetVisibility(Companion && Companion->InstanceId == SelectedCompanionId ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
	}
	if (RosterCountText)
	{
		RosterCountText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), CachedRoster.Num(), RosterCapacity)));
	}
}

void UGameXXKCompanionRosterWidget::RefreshProfilePanel()
{
	const bool bHasSelectedCompanion = !SelectedCompanionProfile.InstanceId.IsNone();
	const FString ExperienceText = SelectedCompanionProfile.ExperienceRequiredForNextLevel > 0
		? FString::Printf(TEXT("经验  %d / %d"), SelectedCompanionProfile.Experience, SelectedCompanionProfile.ExperienceRequiredForNextLevel)
		: TEXT("经验  已满级");
	if (ProfileTitleText)
	{
		ProfileTitleText->SetText(bHasSelectedCompanion
			? FText::FromString(FString::Printf(TEXT("%s%s"), *GetRoleDisplayName(SelectedCompanionProfile.Role).ToString(), SelectedCompanionProfile.bIsActive ? TEXT(" · 已出战") : TEXT("")))
			: NSLOCTEXT("GameXXKCompanionRoster", "NoCompanion", "尚未招募伙伴"));
	}
	if (ProfileDetailText)
	{
		ProfileDetailText->SetText(bHasSelectedCompanion
			? FText::FromString(FString::Printf(
				TEXT("等级  Lv.%d\n%s\n星级  ★%d\n\n气血  %d\n攻击  %d\n防御  %d\n内力  %d"),
				SelectedCompanionProfile.Level,
				*ExperienceText,
				SelectedCompanionProfile.Star,
				SelectedCompanionProfile.Attributes.Health,
				SelectedCompanionProfile.Attributes.Attack,
				SelectedCompanionProfile.Attributes.Defense,
				SelectedCompanionProfile.Attributes.Mana))
			: NSLOCTEXT("GameXXKCompanionRoster", "NoCompanionDetail", "招募后可在此查看属性与个人牌组。"));
	}
	if (LoadoutStatusText)
	{
		LoadoutStatusText->SetText(bLoadoutReadOnly
			? NSLOCTEXT("GameXXKCompanionRoster", "LockedStatus", "本次路线已锁定，牌组只读")
			: FText::FromString(FString::Printf(TEXT("已选 %d / 5 张 · 升星印 %d"), PendingPersonalCardIds.Num(), SigilCount)));
	}
	if (ApplyLoadoutButton)
	{
		ApplyLoadoutButton->SetIsEnabled(!bLoadoutReadOnly && bHasSelectedCompanion && PendingPersonalCardIds.Num() == 5);
	}
	if (SetActiveButton)
	{
		SetActiveButton->SetIsEnabled(!bLoadoutReadOnly && bHasSelectedCompanion);
	}
	const bool bHasActivePermanentCompanion = CachedRoster.ContainsByPredicate([](const FGameXXKPermanentCompanion& Companion)
	{
		return Companion.bIsActive;
	});
	if (ClearActiveButton)
	{
		ClearActiveButton->SetIsEnabled(!bRecruitmentActionsReadOnly && bHasActivePermanentCompanion);
	}
	const int32 RequiredSigils = bHasSelectedCompanion ? SelectedCompanionProfile.Star : 0;
	if (PromoteStarButton)
	{
		PromoteStarButton->SetIsEnabled(!bRecruitmentActionsReadOnly
			&& bHasSelectedCompanion
			&& SelectedCompanionProfile.Star < 5
			&& SigilCount >= RequiredSigils);
	}
	if (PromoteStarButtonText)
	{
		PromoteStarButtonText->SetText(FText::FromString(bHasSelectedCompanion
			? FString::Printf(TEXT("升星 · 消耗 %d 枚升星印"), RequiredSigils)
			: TEXT("升星 · 请选择伙伴")));
	}
}

void UGameXXKCompanionRosterWidget::RefreshRecruitmentPanel()
{
	const bool bHasPendingCandidate = !PendingRecruitmentCandidate.InstanceId.IsNone();
	if (RecruitmentStatusText)
	{
		const FText DefaultStatus = bHasPendingCandidate
			? FText::FromString(FString::Printf(
				TEXT("待决定：%s · 个人牌组 %d 张\n选择左侧伙伴替换，或放弃候选。"),
				*GetRoleDisplayName(PendingRecruitmentCandidate.Role).ToString(),
				PendingRecruitmentCandidate.PersonalCardIds.Num()))
			: NSLOCTEXT("GameXXKCompanionRoster", "RecruitmentReady", "招贤会固定保存候选；满员时不会重掷。");
		RecruitmentStatusText->SetText(RecruitmentFeedback.IsEmpty() ? DefaultStatus : FText::FromString(RecruitmentFeedback));
	}
	if (RecruitButton)
	{
		RecruitButton->SetIsEnabled(!bRecruitmentActionsReadOnly && !bHasPendingCandidate);
	}
	if (ReplacePendingButton)
	{
		ReplacePendingButton->SetIsEnabled(!bRecruitmentActionsReadOnly && bHasPendingCandidate && !SelectedCompanionId.IsNone());
	}
	if (DiscardPendingButton)
	{
		DiscardPendingButton->SetIsEnabled(!bRecruitmentActionsReadOnly && bHasPendingCandidate);
	}
}

void UGameXXKCompanionRosterWidget::RefreshPersonalCards()
{
	if (!PersonalCardGrid || !WidgetTree)
	{
		return;
	}

	ClearCardTooltipHoverState();
	const TArray<FName>& VisibleCardIds = bEditingHeroDeck ? VisibleHeroCardIds : VisiblePersonalCardIds;
	const TArray<FName>& UnlockedCardIds = bEditingHeroDeck ? UnlockedHeroCardIds : UnlockedPersonalCardIds;
	const TArray<FName>& PendingCardIds = bEditingHeroDeck ? PendingHeroCardIds : PendingPersonalCardIds;
	PersonalCardGrid->ClearChildren();
	PersonalCardButtons.Reset();
	for (int32 CardIndex = 0; CardIndex < VisibleCardIds.Num(); ++CardIndex)
	{
		const FName CardId = VisibleCardIds[CardIndex];
		const bool bUnlocked = UnlockedCardIds.Contains(CardId);
		const bool bSelected = PendingCardIds.Contains(CardId);
		UGameXXKCompanionRosterCardButton* CardButton = WidgetTree->ConstructWidget<UGameXXKCompanionRosterCardButton>(UGameXXKCompanionRosterCardButton::StaticClass());
		CardButton->Configure(this, CardId, bEditingHeroDeck);
		BuildCardFace(WidgetTree, CardButton, FGameXXKCardCatalog::FindCardDefinition(CardId), bSelected, bUnlocked);
		// Keep locked and read-only cards hoverable so their actual unavailable reason is inspectable.
		// ToggleSelectedCompanionCard and ToggleHeroCard remain the canonical mutation gates.
		CardButton->SetIsEnabled(true);
		CardButton->SetRenderOpacity(!bLoadoutReadOnly && bUnlocked ? 1.0f : 0.62f);

		USizeBox* CardSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		CardSizeBox->SetWidthOverride(PersonalCardSize.X);
		CardSizeBox->SetHeightOverride(PersonalCardSize.Y);
		CardSizeBox->AddChild(CardButton);
		if (UUniformGridSlot* GridSlot = PersonalCardGrid->AddChildToUniformGrid(CardSizeBox, CardIndex / PersonalCardColumnCount, CardIndex % PersonalCardColumnCount))
		{
			GridSlot->SetHorizontalAlignment(HAlign_Center);
			GridSlot->SetVerticalAlignment(VAlign_Center);
		}
		PersonalCardButtons.Add(CardButton);
	}
}

void UGameXXKCompanionRosterWidget::RefreshDeckSummaries()
{
	if (HeroDeckSummaryText)
	{
		HeroDeckSummaryText->SetText(FText::FromString(FString::Printf(
			TEXT("主角牌组  %d / 8\n%s"),
			HeroCardSummary.Num(),
			*BuildCardSummary(HeroCardSummary))));
	}
	if (TaskNpcDeckSummaryText)
	{
		const FString NpcTitle = TaskNpcCardSummary.NpcId.IsNone() ? TEXT("任务 NPC 未加入") : TaskNpcCardSummary.NpcId.ToString();
		TaskNpcDeckSummaryText->SetText(FText::FromString(FString::Printf(
			TEXT("任务 NPC · %s\n固定支援牌组  %d / 3 · 只读\n%s"),
			*NpcTitle,
			TaskNpcCardSummary.SelectedCardIds.Num(),
			*BuildCardSummary(TaskNpcCardSummary.SelectedCardIds))));
	}
}

void UGameXXKCompanionRosterWidget::RefreshDeckEditorControls()
{
	if (DeckCaptionText)
	{
		DeckCaptionText->SetText(bEditingHeroDeck
			? NSLOCTEXT("GameXXKCompanionRoster", "HeroDeckCaption", "主角牌组（12 张，编入 8 张）")
			: NSLOCTEXT("GameXXKCompanionRoster", "PersonalDeckCaption", "个人牌组（12 张，编入 5 张）"));
	}
	if (HeroDeckToggleButtonText)
	{
		HeroDeckToggleButtonText->SetText(bEditingHeroDeck
			? NSLOCTEXT("GameXXKCompanionRoster", "ReturnPersonalDeck", "查看伙伴牌组")
			: NSLOCTEXT("GameXXKCompanionRoster", "OpenHeroDeck", "编辑主角牌组"));
	}
	if (ApplyHeroLoadoutButton)
	{
		ApplyHeroLoadoutButton->SetVisibility(bEditingHeroDeck ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		ApplyHeroLoadoutButton->SetIsEnabled(bEditingHeroDeck && !bLoadoutReadOnly && PendingHeroCardIds.Num() == 8);
	}
	if (HeroDeckStatusText)
	{
		HeroDeckStatusText->SetVisibility(bEditingHeroDeck ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		HeroDeckStatusText->SetText(bEditingHeroDeck
			? (bLoadoutReadOnly
				? NSLOCTEXT("GameXXKCompanionRoster", "HeroDeckLocked", "路线已锁定 · 只读")
				: FText::FromString(FString::Printf(TEXT("已选 %d / 8 张"), PendingHeroCardIds.Num())))
			: FText::GetEmpty());
	}
}

void UGameXXKCompanionRosterWidget::RefreshCardTooltip()
{
	if (!CardTooltipPanel || !CardTooltipText)
	{
		return;
	}
	if (HoveredCardTooltipId.IsNone() || bHoveredCardTooltipIsHeroDeck != bEditingHeroDeck)
	{
		CardTooltipPanel->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	const TArray<FName>& VisibleCardIds = bEditingHeroDeck ? VisibleHeroCardIds : VisiblePersonalCardIds;
	const TArray<FName>& UnlockedCardIds = bEditingHeroDeck ? UnlockedHeroCardIds : UnlockedPersonalCardIds;
	const TArray<FName>& PendingCardIds = bEditingHeroDeck ? PendingHeroCardIds : PendingPersonalCardIds;
	if (!VisibleCardIds.Contains(HoveredCardTooltipId))
	{
		CardTooltipPanel->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(HoveredCardTooltipId);
	if (!Definition)
	{
		CardTooltipPanel->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	FGameXXKCardTooltipContext Context;
	const bool bSelected = PendingCardIds.Contains(HoveredCardTooltipId);
	const int32 RequiredDeckSize = bEditingHeroDeck ? 8 : 5;
	if (bLoadoutReadOnly)
	{
		Context.UnavailableReason = TEXT("本次路线已锁定，牌组只读。");
	}
	else if (!UnlockedCardIds.Contains(HoveredCardTooltipId))
	{
		Context.UnavailableReason = TEXT("此牌尚未解锁。");
	}
	else if (!bSelected && PendingCardIds.Num() >= RequiredDeckSize)
	{
		Context.UnavailableReason = bEditingHeroDeck
			? TEXT("主角牌组已满（8 张），无法编入此牌。")
			: TEXT("该伙伴个人牌组已满（5 张），无法编入此牌。");
	}
	else
	{
		// An already-selected card remains actionable at capacity because clicking it removes the card.
		Context.InteractionResult = bEditingHeroDeck
			? TEXT("点击后编入/移出主角牌组；需保持 8 张。")
			: TEXT("点击后编入/移出该伙伴个人牌组；需保持 5 张。");
	}

	CardTooltipText->SetText(FText::FromString(GameXXKCardText::DescribeTooltip(*Definition, nullptr, Context)));
	CardTooltipPanel->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UGameXXKCompanionRosterWidget::ClearCardTooltipHoverState()
{
	HoveredCardTooltipId = NAME_None;
	bHoveredCardTooltipIsHeroDeck = false;
	if (CardTooltipPanel)
	{
		CardTooltipPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (CardTooltipText)
	{
		CardTooltipText->SetText(FText::GetEmpty());
	}
}

bool UGameXXKCompanionRosterWidget::IsCurrentSelectedCompanion(const FName InstanceId) const
{
	return !InstanceId.IsNone() && CachedRoster.ContainsByPredicate([InstanceId](const FGameXXKPermanentCompanion& Companion)
	{
		return Companion.InstanceId == InstanceId;
	});
}

void UGameXXKCompanionRosterWidget::HandleApplyLoadoutClicked()
{
	ApplySelectedCompanionCardLoadout();
}

void UGameXXKCompanionRosterWidget::HandleSetActiveClicked()
{
	SetSelectedCompanionAsActive();
}

void UGameXXKCompanionRosterWidget::HandleClearActiveClicked()
{
	ClearActivePermanentCompanion();
}

void UGameXXKCompanionRosterWidget::HandleHeroDeckToggleClicked()
{
	if (bEditingHeroDeck)
	{
		bEditingHeroDeck = false;
		RefreshProgrammaticLayout();
		return;
	}

	OpenHeroDeckEditor();
}

void UGameXXKCompanionRosterWidget::HandleApplyHeroLoadoutClicked()
{
	ApplyHeroCardLoadout();
}

void UGameXXKCompanionRosterWidget::HandleRecruitClicked()
{
	BeginRandomRecruitment();
}

void UGameXXKCompanionRosterWidget::HandleReplacePendingClicked()
{
	ResolvePendingRecruitmentWithSelectedCompanion();
}

void UGameXXKCompanionRosterWidget::HandleDiscardPendingClicked()
{
	DiscardPendingRecruitment();
}

void UGameXXKCompanionRosterWidget::HandlePromoteStarClicked()
{
	PromoteSelectedCompanionStar();
}
