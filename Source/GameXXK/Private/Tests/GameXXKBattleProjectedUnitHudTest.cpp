#include "Misc/AutomationTest.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/SizeBox.h"
#include "Engine/GameInstance.h"
#include "GameXXKMVPRules.h"
#include "Layout/Geometry.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKBattleBoardWidget.h"
#include "UI/GameXXKBattleUnitHudWidget.h"
#include "UI/GameXXKBattleUnitResourceWidget.h"
#include "UI/GameXXKBattleUnitStatusEffectsWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	const FVector2D FixedUnitHudSize(272.0f, 142.0f);

	FGameXXKCardStatusStack MakeStatus(const EGameXXKCardStatus Status, const int32 Stacks)
	{
		FGameXXKCardStatusStack Result;
		Result.Status = Status;
		Result.Stacks = Stacks;
		return Result;
	}

	FGameXXKCardCombatUnit MakeUnit(
		const TCHAR* UnitId,
		const EGameXXKCardTargetSide Side,
		const EGameXXKCharacterRole Role,
		const int32 StableSortOrder,
		const int32 HP,
		const int32 MaxHP,
		const int32 Mana,
		const int32 MaxMana)
	{
		FGameXXKCardCombatUnit Unit;
		Unit.UnitId = FName(UnitId);
		Unit.Side = Side;
		Unit.Role = Role;
		Unit.StableSortOrder = StableSortOrder;
		Unit.bLiving = HP > 0;
		Unit.HP = HP;
		Unit.MaxHP = MaxHP;
		Unit.Mana = Mana;
		Unit.MaxMana = MaxMana;
		return Unit;
	}

	void BuildFixedSlotHudFixture(UGameXXKMVPSubsystem* const Subsystem)
	{
		FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
		State = UGameXXKMVPRules::CreateNewGame();
		State.Screen = EGameXXKScreen::Battle;
		State.bHasActiveBattle = true;
		State.CardRun.bHasActiveCardBattle = true;
		State.CardRun.ActiveBattle.Units = {
			MakeUnit(TEXT("Partner.Blade"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Blade, 0, 90, 100, 10, 20),
			MakeUnit(TEXT("Player"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 1, 72, 100, 18, 30),
			MakeUnit(TEXT("Npc.TusiChief"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::QuestNpc, 2, 66, 80, 8, 12),
			MakeUnit(TEXT("Enemy.MoneyRat"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 0, 54, 90, 0, 0),
			MakeUnit(TEXT("Enemy.BlackBear"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 1, 88, 120, 0, 0),
			MakeUnit(TEXT("Enemy.Tiger"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 2, 152, 180, 0, 0),
			// This living unit has no display P-slot and must not create a HUD plate.
			MakeUnit(TEXT("Party.InvalidSlot"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Invalid, 3, 10, 10, 0, 0)};
		State.CardRun.ActiveBattle.Units[1].Armor = 7;
		State.CardRun.ActiveBattle.Units[1].Statuses = {MakeStatus(EGameXXKCardStatus::Poison, 2)};
		State.CardRun.ActiveBattle.Units[3].Statuses = {
			MakeStatus(EGameXXKCardStatus::Poison, 3),
			MakeStatus(EGameXXKCardStatus::Bleed, 2)};
	}

	void AssertFixedHudSlot(
		FAutomationTestBase& Test,
		UGameXXKBattleBoardWidget* const Board,
		const FName UnitId,
		const EGameXXKCardTargetSide ExpectedSide,
		const int32 ExpectedSlotNumber,
		const FVector2D ExpectedAnchor)
	{
		UGameXXKBattleUnitHudWidget* const Hud = Board ? Board->GetProjectedUnitHudForTest(UnitId) : nullptr;
		Test.TestNotNull(FString::Printf(TEXT("%s has a fixed-slot HUD"), *UnitId.ToString()), Hud);
		Test.TestEqual(FString::Printf(TEXT("%s keeps its authoritative side"), *UnitId.ToString()),
			Hud ? Hud->GetSideForTest() : EGameXXKCardTargetSide::Invalid,
			ExpectedSide);
		Test.TestEqual(FString::Printf(TEXT("%s keeps its authoritative P-slot"), *UnitId.ToString()),
			Hud ? Hud->GetSlotNumberForTest() : INDEX_NONE,
			ExpectedSlotNumber);
		Test.TestEqual(FString::Printf(TEXT("%s HUD is visible without an actor-foot projection"), *UnitId.ToString()),
			Hud ? Hud->GetVisibility() : ESlateVisibility::Collapsed,
			ESlateVisibility::SelfHitTestInvisible);

		const UCanvasPanelSlot* const Slot = Hud ? Cast<UCanvasPanelSlot>(Hud->Slot) : nullptr;
		Test.TestNotNull(FString::Printf(TEXT("%s HUD has a Canvas slot"), *UnitId.ToString()), Slot);
		const FAnchors Anchors = Slot ? Slot->GetAnchors() : FAnchors();
		Test.TestEqual(FString::Printf(TEXT("%s HUD uses its stable normalized anchor minimum"), *UnitId.ToString()), Anchors.Minimum, ExpectedAnchor);
		Test.TestEqual(FString::Printf(TEXT("%s HUD uses its stable normalized anchor maximum"), *UnitId.ToString()), Anchors.Maximum, ExpectedAnchor);
		Test.TestEqual(FString::Printf(TEXT("%s exposes its real normalized fixed anchor through the HUD seam"), *UnitId.ToString()),
			Board ? Board->GetProjectedUnitHudAnchorPositionForTest(UnitId) : FVector2D::ZeroVector,
			ExpectedAnchor);
		Test.TestEqual(FString::Printf(TEXT("%s HUD center-aligns on its fixed battle lane"), *UnitId.ToString()),
			Slot ? Slot->GetAlignment() : FVector2D::ZeroVector,
			FVector2D(0.5f, 0.0f));
		const FMargin Offsets = Slot ? Slot->GetOffsets() : FMargin();
		Test.TestEqual(FString::Printf(TEXT("%s HUD keeps the readable fixed plate size"), *UnitId.ToString()),
			FVector2D(Offsets.Right, Offsets.Bottom),
			FixedUnitHudSize);
	}

	void AssertApprovedInnerLaneClearance(
		FAutomationTestBase& Test,
		UGameXXKBattleBoardWidget* const Board)
	{
		constexpr float SafeStageWidth = 1920.0f;
		constexpr float CurrentPieWidth = 1114.0f;
		Test.TestNotNull(TEXT("clearance test has a Board"), Board);
		if (!Board)
		{
			return;
		}

		const float HalfPlateWidth = FixedUnitHudSize.X * 0.5f;
		const FVector2D EnemyInnerAnchor = Board->GetProjectedUnitHudAnchorPositionForTest(TEXT("Enemy.Tiger"));
		const FVector2D PartyInnerAnchor = Board->GetProjectedUnitHudAnchorPositionForTest(TEXT("Npc.TusiChief"));
		const FVector2D EnemyOuterAnchor = Board->GetProjectedUnitHudAnchorPositionForTest(TEXT("Enemy.MoneyRat"));
		const FVector2D PartyOuterAnchor = Board->GetProjectedUnitHudAnchorPositionForTest(TEXT("Partner.Blade"));
		const float EnemyInnerRight = EnemyInnerAnchor.X * SafeStageWidth + HalfPlateWidth;
		const float PartyInnerLeft = PartyInnerAnchor.X * SafeStageWidth - HalfPlateWidth;
		const float GapAtCurrentPie = (PartyInnerLeft - EnemyInnerRight) * CurrentPieWidth / SafeStageWidth;
		const float EnemyOuterLeft = EnemyOuterAnchor.X * SafeStageWidth - HalfPlateWidth;
		const float PartyOuterRight = PartyOuterAnchor.X * SafeStageWidth + HalfPlateWidth;

		Test.TestTrue(TEXT("inner enemy and party HUD plates have at least 40 physical pixels of clearance at 1114-wide PIE"), GapAtCurrentPie >= 40.0f);
		Test.TestTrue(TEXT("outer enemy HUD remains inside the 1920-wide safe stage"), EnemyOuterLeft >= 0.0f);
		Test.TestTrue(TEXT("outer party HUD remains inside the 1920-wide safe stage"), PartyOuterRight <= SafeStageWidth);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBattleProjectedUnitHudTest,
	"GameXXK.UI.Battle.FixedSlotUnitHud",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBattleProjectedUnitHudTest::RunTest(const FString& Parameters)
{
	UGameInstance* const TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* const Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	BuildFixedSlotHudFixture(Subsystem);

	UGameXXKBattleBoardWidget* const Board = NewObject<UGameXXKBattleBoardWidget>();
	Board->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("fixed-slot HUD board initializes"), Board->Initialize());
	Board->NativeConstruct();
	Board->RefreshFromState();

	UCanvasPanel* const Layer = Board->GetBattleProjectedUnitHudLayerForTest();
	TestNotNull(TEXT("board owns the unit HUD canvas layer"), Layer);
	UScaleBox* const SafeStage = Board->WidgetTree
		? Cast<UScaleBox>(Board->WidgetTree->FindWidget(TEXT("BattleHudSafeStage")))
		: nullptr;
	USizeBox* const SafeStageSize = Board->WidgetTree
		? Cast<USizeBox>(Board->WidgetTree->FindWidget(TEXT("BattleHudSafeStageSize")))
		: nullptr;
	TestNotNull(TEXT("board embeds the unit HUD in a centered 16:9 safe stage"), SafeStage);
	TestNotNull(TEXT("the safe stage has a fixed 1920 by 1080 design canvas"), SafeStageSize);
	TestEqual(TEXT("the safe stage scales to fit without stretching the battle composition"),
		SafeStage ? SafeStage->GetStretch() : EStretch::None,
		EStretch::ScaleToFit);
	TestEqual(TEXT("the safe stage can scale up and down with the viewport"),
		SafeStage ? SafeStage->GetStretchDirection() : EStretchDirection::DownOnly,
		EStretchDirection::Both);
	TestEqual(TEXT("the safe stage keeps a 1920 design width"), SafeStageSize ? SafeStageSize->GetWidthOverride() : 0.0f, 1920.0f);
	TestEqual(TEXT("the safe stage keeps a 1080 design height"), SafeStageSize ? SafeStageSize->GetHeightOverride() : 0.0f, 1080.0f);
	TestTrue(TEXT("the fixed HUD layer belongs to the 16:9 stage rather than the full root canvas"),
		Layer && SafeStageSize && Layer->GetParent() == SafeStageSize);
	const UScaleBoxSlot* const SafeStageContentSlot = SafeStageSize ? Cast<UScaleBoxSlot>(SafeStageSize->Slot) : nullptr;
	TestTrue(TEXT("the safe-stage content centers in both axes when a viewport letterboxes or pillarboxes"),
		SafeStageContentSlot
		&& SafeStageContentSlot->GetHorizontalAlignment() == HAlign_Center
		&& SafeStageContentSlot->GetVerticalAlignment() == VAlign_Center);
	TestEqual(TEXT("unit HUD layer remains input transparent"),
		Layer ? Layer->GetVisibility() : ESlateVisibility::Visible,
		ESlateVisibility::SelfHitTestInvisible);
	const UCanvasPanelSlot* const SafeStageSlot = SafeStage ? Cast<UCanvasPanelSlot>(SafeStage->Slot) : nullptr;
	TestTrue(TEXT("unit HUD safe stage remains behind card and intent tooltips"),
		SafeStageSlot && SafeStageSlot->GetZOrder() < 0);
	const FGameXXKBattleHudSafeStageLayout NativeStage = Board->ResolveBattleHudSafeStageLayoutForTest(FVector2D(1920.0f, 1080.0f));
	TestTrue(TEXT("a native 16:9 viewport fills the fixed HUD safe stage"),
		NativeStage.Offset.Equals(FVector2D::ZeroVector, 0.01f)
		&& NativeStage.Size.Equals(FVector2D(1920.0f, 1080.0f), 0.01f)
		&& FMath::IsNearlyEqual(NativeStage.Scale, 1.0f, 0.001f));
	const FGameXXKBattleHudSafeStageLayout WideStage = Board->ResolveBattleHudSafeStageLayoutForTest(FVector2D(2560.0f, 1080.0f));
	TestTrue(TEXT("a wide viewport pillarboxes the fixed HUD stage without horizontal drift"),
		WideStage.Offset.Equals(FVector2D(320.0f, 0.0f), 0.01f)
		&& WideStage.Size.Equals(FVector2D(1920.0f, 1080.0f), 0.01f)
		&& FMath::IsNearlyEqual(WideStage.Scale, 1.0f, 0.001f));
	const FGameXXKBattleHudSafeStageLayout NarrowStage = Board->ResolveBattleHudSafeStageLayoutForTest(FVector2D(1280.0f, 1024.0f));
	TestTrue(TEXT("a tall viewport letterboxes the fixed HUD stage without stretching it"),
		NarrowStage.Offset.Equals(FVector2D(0.0f, 152.0f), 0.01f)
		&& NarrowStage.Size.Equals(FVector2D(1280.0f, 720.0f), 0.01f)
		&& FMath::IsNearlyEqual(NarrowStage.Scale, 2.0f / 3.0f, 0.001f));
	TestEqual(TEXT("only six valid living P-slots create HUD plates"), Board->GetProjectedUnitHudCountForTest(), 6);
	TestEqual(TEXT("the unit HUD layer owns one child for every valid living P-slot"), Layer ? Layer->GetChildrenCount() : INDEX_NONE, 6);
	TestNull(TEXT("a living unit without a valid P-slot has no HUD plate"), Board->GetProjectedUnitHudForTest(TEXT("Party.InvalidSlot")));

	AssertFixedHudSlot(*this, Board, TEXT("Partner.Blade"), EGameXXKCardTargetSide::Party, 1, FVector2D(0.89f, 0.60f));
	AssertFixedHudSlot(*this, Board, TEXT("Player"), EGameXXKCardTargetSide::Party, 2, FVector2D(0.74f, 0.52f));
	AssertFixedHudSlot(*this, Board, TEXT("Npc.TusiChief"), EGameXXKCardTargetSide::Party, 3, FVector2D(0.59f, 0.44f));
	AssertFixedHudSlot(*this, Board, TEXT("Enemy.MoneyRat"), EGameXXKCardTargetSide::Enemy, 1, FVector2D(0.11f, 0.60f));
	AssertFixedHudSlot(*this, Board, TEXT("Enemy.BlackBear"), EGameXXKCardTargetSide::Enemy, 2, FVector2D(0.26f, 0.52f));
	AssertFixedHudSlot(*this, Board, TEXT("Enemy.Tiger"), EGameXXKCardTargetSide::Enemy, 3, FVector2D(0.41f, 0.44f));
	AssertApprovedInnerLaneClearance(*this, Board);

	UGameXXKBattleUnitHudWidget* const HeroHud = Board->GetProjectedUnitHudForTest(TEXT("Player"));
	if (HeroHud && HeroHud->GetResourceWidgetForTest())
	{
		TestEqual(TEXT("hero HP uses authoritative card runtime values"), HeroHud->GetResourceWidgetForTest()->GetHealthDisplayTextForTest(), FString(TEXT("气血 72 / 100")));
		TestEqual(TEXT("hero mana uses authoritative card runtime values"), HeroHud->GetResourceWidgetForTest()->GetManaDisplayTextForTest(), FString(TEXT("内力 18 / 30")));
	}
	const UCanvasPanelSlot* const HeroInitialSlot = HeroHud ? Cast<UCanvasPanelSlot>(HeroHud->Slot) : nullptr;
	const FVector2D HeroInitialAnchor = HeroInitialSlot ? HeroInitialSlot->GetAnchors().Minimum : FVector2D::ZeroVector;
	const FVector2D HeroInitialAlignment = HeroInitialSlot ? HeroInitialSlot->GetAlignment() : FVector2D::ZeroVector;
	const FMargin HeroInitialOffsets = HeroInitialSlot ? HeroInitialSlot->GetOffsets() : FMargin();
	const int32 HeroInitialStatusGeneration = HeroHud && HeroHud->GetStatusEffectsWidgetForTest()
		? HeroHud->GetStatusEffectsWidgetForTest()->GetIconRebuildGenerationForTest()
		: INDEX_NONE;
	FGameXXKCardCombatUnit& HeroRuntimeUnit = Subsystem->GetMutableRuntimeState().CardRun.ActiveBattle.Units[1];
	HeroRuntimeUnit.HP = 49;
	HeroRuntimeUnit.Mana = 6;
	HeroRuntimeUnit.Armor = 11;
	HeroRuntimeUnit.Statuses = {MakeStatus(EGameXXKCardStatus::Bleed, 4)};
	Board->RefreshFromState();
	UGameXXKBattleUnitHudWidget* const UpdatedHeroHud = Board->GetProjectedUnitHudForTest(TEXT("Player"));
	TestEqual(TEXT("a living unit reuses its fixed HUD object when vitals change"), UpdatedHeroHud, HeroHud);
	const UCanvasPanelSlot* const HeroUpdatedSlot = UpdatedHeroHud ? Cast<UCanvasPanelSlot>(UpdatedHeroHud->Slot) : nullptr;
	TestTrue(TEXT("a vitals refresh leaves the fixed HUD canvas geometry untouched"),
		HeroUpdatedSlot
		&& HeroUpdatedSlot->GetAnchors().Minimum.Equals(HeroInitialAnchor, 0.001f)
		&& HeroUpdatedSlot->GetAlignment().Equals(HeroInitialAlignment, 0.001f)
		&& FVector2D(HeroUpdatedSlot->GetOffsets().Right, HeroUpdatedSlot->GetOffsets().Bottom)
			.Equals(FVector2D(HeroInitialOffsets.Right, HeroInitialOffsets.Bottom), 0.001f));
	if (UpdatedHeroHud && UpdatedHeroHud->GetResourceWidgetForTest() && UpdatedHeroHud->GetStatusEffectsWidgetForTest())
	{
		TestEqual(TEXT("a vitals refresh redraws authoritative HP"), UpdatedHeroHud->GetResourceWidgetForTest()->GetHealthDisplayTextForTest(), FString(TEXT("气血 49 / 100")));
		TestEqual(TEXT("a vitals refresh redraws authoritative mana"), UpdatedHeroHud->GetResourceWidgetForTest()->GetManaDisplayTextForTest(), FString(TEXT("内力 6 / 30")));
		TestEqual(TEXT("a vitals refresh redraws armor plus its status badge"), UpdatedHeroHud->GetStatusEffectsWidgetForTest()->GetIconCountForTest(), 2);
		TestTrue(TEXT("a vitals refresh rebuilds its status strip"),
			UpdatedHeroHud->GetStatusEffectsWidgetForTest()->GetIconRebuildGenerationForTest() > HeroInitialStatusGeneration);
	}

	const FGeometry WideGeometry = FGeometry::MakeRoot(FVector2D(1920.0f, 1080.0f), FSlateLayoutTransform());
	// These are deliberately nonsensical actor-foot positions. They remain legal for
	// the targeting arrow bridge, but they must never move the fixed resource HUD.
	Board->RegisterBattleUnitHudScreenPosition(TEXT("Player"), FVector2D(12.0f, 1060.0f));
	Board->RegisterBattleUnitHudScreenPosition(TEXT("Enemy.Tiger"), FVector2D(1910.0f, 8.0f));
	Board->NativeTick(WideGeometry, 0.0f);
	AssertFixedHudSlot(*this, Board, TEXT("Player"), EGameXXKCardTargetSide::Party, 2, FVector2D(0.74f, 0.52f));
	AssertFixedHudSlot(*this, Board, TEXT("Enemy.Tiger"), EGameXXKCardTargetSide::Enemy, 3, FVector2D(0.41f, 0.44f));

	const FGeometry NarrowGeometry = FGeometry::MakeRoot(FVector2D(1280.0f, 1024.0f), FSlateLayoutTransform());
	Board->NativeTick(NarrowGeometry, 0.0f);
	AssertFixedHudSlot(*this, Board, TEXT("Partner.Blade"), EGameXXKCardTargetSide::Party, 1, FVector2D(0.89f, 0.60f));
	AssertFixedHudSlot(*this, Board, TEXT("Enemy.MoneyRat"), EGameXXKCardTargetSide::Enemy, 1, FVector2D(0.11f, 0.60f));

	Subsystem->GetMutableRuntimeState().CardRun.ActiveBattle.Units[5].bLiving = false;
	Board->RefreshFromState();
	TestNull(TEXT("dead fixed-slot units are removed from the board HUD map"), Board->GetProjectedUnitHudForTest(TEXT("Enemy.Tiger")));
	TestEqual(TEXT("dead fixed-slot units reduce the board HUD count"), Board->GetProjectedUnitHudCountForTest(), 5);
	FGameXXKCardCombatUnit& TigerRuntimeUnit = Subsystem->GetMutableRuntimeState().CardRun.ActiveBattle.Units[5];
	TigerRuntimeUnit.bLiving = true;
	TigerRuntimeUnit.HP = 141;
	TigerRuntimeUnit.MaxHP = 180;
	Board->RefreshFromState();
	UGameXXKBattleUnitHudWidget* const RevivedTigerHud = Board->GetProjectedUnitHudForTest(TEXT("Enemy.Tiger"));
	TestNotNull(TEXT("a revived valid P-slot reconstructs its fixed HUD"), RevivedTigerHud);
	AssertFixedHudSlot(*this, Board, TEXT("Enemy.Tiger"), EGameXXKCardTargetSide::Enemy, 3, FVector2D(0.41f, 0.44f));
	TestEqual(TEXT("a revived fixed-slot HUD redraws current HP"),
		RevivedTigerHud && RevivedTigerHud->GetResourceWidgetForTest()
			? RevivedTigerHud->GetResourceWidgetForTest()->GetHealthDisplayTextForTest()
			: FString(),
		FString(TEXT("气血 141 / 180")));
	TestEqual(TEXT("a revived fixed-slot unit restores the board HUD count"), Board->GetProjectedUnitHudCountForTest(), 6);

	return true;
}

#endif
