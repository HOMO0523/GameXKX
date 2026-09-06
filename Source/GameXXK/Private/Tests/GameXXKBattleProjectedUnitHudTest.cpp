#include "Misc/AutomationTest.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "GameXXKMVPRules.h"
#include "Layout/Geometry.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKBattleAtlasCache.h"
#include "UI/GameXXKBattleBoardWidget.h"
#include "UI/GameXXKBattleUnitHudWidget.h"
#include "UI/GameXXKBattleUnitResourceWidget.h"
#include "UI/GameXXKBattleUnitStatusEffectsWidget.h"
#include "UI/GameXXKBattleUnitVisualWidget.h"
#include "UObject/StrongObjectPtr.h"

#include <type_traits>
#include <utility>

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	class FFixedSlotAtlasLoadHandle final : public IGameXXKBattleAtlasLoadHandle
	{
	public:
		virtual void Cancel() override {}
	};

	class FFixedSlotAtlasLoader final : public IGameXXKBattleAtlasLoader
	{
	public:
		virtual TSharedPtr<IGameXXKBattleAtlasLoadHandle> RequestAsyncLoad(
			const FSoftObjectPath& Path,
			FGameXXKAtlasLoaderCompletion Completion) override
		{
			RequestedPaths.Add(Path);
			const FString PathString = Path.ToString();
			if (PathString.Contains(TEXT("_idle_atlas")) || PathString.Contains(TEXT("impact_ink_generic")))
			{
				UTexture2D* const Texture = NewObject<UTexture2D>(GetTransientPackage());
				LoadedTextures.Add(TStrongObjectPtr<UTexture2D>(Texture));
				Completion(Texture, 4);
			}
			return MakeShared<FFixedSlotAtlasLoadHandle>();
		}

		bool Requested(const FSoftObjectPath& Path) const
		{
			return RequestedPaths.Contains(Path);
		}

		TArray<FSoftObjectPath> RequestedPaths;
		TArray<TStrongObjectPtr<UTexture2D>> LoadedTextures;
	};

	template <typename TBoard, typename = void>
	struct TFixedSlotPresentationApi
	{
		static constexpr bool bAvailable = false;
		static void Queue(TBoard*, const FGameXXKBattlePresentationEvent&) {}
		static int32 DisplayedHealth(const TBoard*, FName) { return INDEX_NONE; }
		static float AttackerRate(const TBoard*) { return 0.0f; }
		static float TargetRate(const TBoard*) { return 0.0f; }
		static float ImpactRate(const TBoard*) { return 0.0f; }
		static FString Readout(const TBoard*) { return FString(); }
	};

	template <typename TBoard>
	struct TFixedSlotPresentationApi<TBoard, std::void_t<
		decltype(std::declval<TBoard&>().QueuePresentation(std::declval<const FGameXXKBattlePresentationEvent&>())),
		decltype(std::declval<const TBoard&>().GetDisplayedHealthForTest(std::declval<FName>())),
		decltype(std::declval<const TBoard&>().GetActiveAttackerPlaybackRateForTest()),
		decltype(std::declval<const TBoard&>().GetActiveTargetPlaybackRateForTest()),
		decltype(std::declval<const TBoard&>().GetActiveImpactPlaybackRateForTest()),
		decltype(std::declval<const TBoard&>().GetBattlePresentationReadoutForTest())>>
	{
		static constexpr bool bAvailable = true;
		static void Queue(TBoard* Board, const FGameXXKBattlePresentationEvent& Event) { Board->QueuePresentation(Event); }
		static int32 DisplayedHealth(const TBoard* Board, const FName UnitId) { return Board->GetDisplayedHealthForTest(UnitId); }
		static float AttackerRate(const TBoard* Board) { return Board->GetActiveAttackerPlaybackRateForTest(); }
		static float TargetRate(const TBoard* Board) { return Board->GetActiveTargetPlaybackRateForTest(); }
		static float ImpactRate(const TBoard* Board) { return Board->GetActiveImpactPlaybackRateForTest(); }
		static FString Readout(const TBoard* Board) { return Board->GetBattlePresentationReadoutForTest(); }
	};

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
		FVector2D ExpectedAnchor)
	{
		ExpectedAnchor.Y += 0.025f;
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
	using FPresentationApi = TFixedSlotPresentationApi<UGameXXKBattleBoardWidget>;
	TestTrue(TEXT("fixed-slot Board exposes marker-driven presentation state"), FPresentationApi::bAvailable);
	if (!FPresentationApi::bAvailable)
	{
		return false;
	}

	UGameInstance* const TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* const Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	BuildFixedSlotHudFixture(Subsystem);

	UGameXXKBattleBoardWidget* const Board = NewObject<UGameXXKBattleBoardWidget>();
	Board->SetMVPSubsystem(Subsystem);
	const TSharedRef<FFixedSlotAtlasLoader> AtlasLoader = MakeShared<FFixedSlotAtlasLoader>();
	Board->SetAtlasCacheForTest(MakeUnique<FGameXXKBattleAtlasCache>(
		AtlasLoader,
		[]() { return 10.0; }));
	TestTrue(TEXT("fixed-slot HUD board initializes"), Board->Initialize());
	Board->NativeConstruct();
	Board->RefreshFromState();
	TestTrue(TEXT("fixed-slot fixture begins a common-stage visual session"), Board->BeginBattleVisualSession(901));

	UCanvasPanel* const Layer = Board->GetBattleProjectedUnitHudLayerForTest();
	TestNotNull(TEXT("board owns the unit HUD canvas layer"), Layer);
	UScaleBox* const SafeStage = Board->WidgetTree
		? Cast<UScaleBox>(Board->WidgetTree->FindWidget(TEXT("BattleHudSafeStage")))
		: nullptr;
	USizeBox* const SafeStageSize = Board->WidgetTree
		? Cast<USizeBox>(Board->WidgetTree->FindWidget(TEXT("BattleHudSafeStageSize")))
		: nullptr;
	UCanvasPanel* const DesignStage = Board->GetBattleDesignStageForTest();
	UCanvasPanel* const ControlsLayer = Board->GetBattleControlsLayerForTest();
	TestNotNull(TEXT("board embeds the unit HUD in a centered 16:9 safe stage"), SafeStage);
	TestNotNull(TEXT("the safe stage has a fixed 1920 by 1080 design canvas"), SafeStageSize);
	TestNotNull(TEXT("the fixed stage owns one design canvas for every battle element"), DesignStage);
	TestEqual(TEXT("the safe stage scales to fit without stretching the battle composition"),
		SafeStage ? SafeStage->GetStretch() : EStretch::None,
		EStretch::ScaleToFit);
	TestEqual(TEXT("the safe stage can scale up and down with the viewport"),
		SafeStage ? SafeStage->GetStretchDirection() : EStretchDirection::DownOnly,
		EStretchDirection::Both);
	TestEqual(TEXT("the safe stage keeps a 1920 design width"), SafeStageSize ? SafeStageSize->GetWidthOverride() : 0.0f, 1920.0f);
	TestEqual(TEXT("the safe stage keeps a 1080 design height"), SafeStageSize ? SafeStageSize->GetHeightOverride() : 0.0f, 1080.0f);
	TestTrue(TEXT("the design canvas is the sole content of the fixed 1920 by 1080 size box"),
		DesignStage && SafeStageSize && DesignStage->GetParent() == SafeStageSize);
	TestTrue(TEXT("the fixed HUD layer belongs to the controls container inside the common stage"),
		Layer && ControlsLayer && Layer->GetParent() == ControlsLayer);
	const UScaleBoxSlot* const SafeStageContentSlot = SafeStageSize ? Cast<UScaleBoxSlot>(SafeStageSize->Slot) : nullptr;
	TestTrue(TEXT("the safe-stage content centers in both axes when a viewport letterboxes or pillarboxes"),
		SafeStageContentSlot
		&& SafeStageContentSlot->GetHorizontalAlignment() == HAlign_Center
		&& SafeStageContentSlot->GetVerticalAlignment() == VAlign_Center);
	TestEqual(TEXT("unit HUD layer remains input transparent"),
		Layer ? Layer->GetVisibility() : ESlateVisibility::Visible,
		ESlateVisibility::SelfHitTestInvisible);
	const UCanvasPanelSlot* const SafeStageSlot = SafeStage ? Cast<UCanvasPanelSlot>(SafeStage->Slot) : nullptr;
	TestEqual(TEXT("the common stage occupies the viewport root above the full-screen backdrop"),
		SafeStageSlot ? SafeStageSlot->GetZOrder() : INDEX_NONE, 1);
	const UCanvasPanelSlot* const ControlsSlot = ControlsLayer ? Cast<UCanvasPanelSlot>(ControlsLayer->Slot) : nullptr;
	TestEqual(TEXT("unit HUD and card controls share canonical controls z twenty"),
		ControlsSlot ? ControlsSlot->GetZOrder() : INDEX_NONE, 20);
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
	TestEqual(TEXT("the common stage owns one persistent image visual for every valid living P-slot"),
		Board->GetUnitVisualCountForTest(), 6);
	TestNull(TEXT("a living unit without a valid P-slot has no HUD plate"), Board->GetProjectedUnitHudForTest(TEXT("Party.InvalidSlot")));
	TestNull(TEXT("a living unit without a valid P-slot has no formation visual"), Board->GetUnitVisualForTest(TEXT("Party.InvalidSlot")));

	AssertFixedHudSlot(*this, Board, TEXT("Partner.Blade"), EGameXXKCardTargetSide::Party, 1, FVector2D(0.905f, 0.60f));
	AssertFixedHudSlot(*this, Board, TEXT("Player"), EGameXXKCardTargetSide::Party, 2, FVector2D(0.755f, 0.52f));
	AssertFixedHudSlot(*this, Board, TEXT("Npc.TusiChief"), EGameXXKCardTargetSide::Party, 3, FVector2D(0.605f, 0.44f));
	AssertFixedHudSlot(*this, Board, TEXT("Enemy.MoneyRat"), EGameXXKCardTargetSide::Enemy, 1, FVector2D(0.095f, 0.60f));
	AssertFixedHudSlot(*this, Board, TEXT("Enemy.BlackBear"), EGameXXKCardTargetSide::Enemy, 2, FVector2D(0.245f, 0.52f));
	AssertFixedHudSlot(*this, Board, TEXT("Enemy.Tiger"), EGameXXKCardTargetSide::Enemy, 3, FVector2D(0.395f, 0.44f));
	const FName FormationUnitIds[] = {
		TEXT("Partner.Blade"), TEXT("Player"), TEXT("Npc.TusiChief"),
		TEXT("Enemy.MoneyRat"), TEXT("Enemy.BlackBear"), TEXT("Enemy.Tiger")};
	for (const FName UnitId : FormationUnitIds)
	{
		UGameXXKBattleUnitVisualWidget* const Visual = Board->GetUnitVisualForTest(UnitId);
		TestNotNull(FString::Printf(TEXT("%s has a persistent formation visual"), *UnitId.ToString()), Visual);
		TestTrue(FString::Printf(TEXT("%s visual is a direct design-stage child"), *UnitId.ToString()),
			Visual && Visual->GetParent() == DesignStage);
		const UCanvasPanelSlot* const VisualSlot = Visual ? Cast<UCanvasPanelSlot>(Visual->Slot) : nullptr;
		TestEqual(FString::Printf(TEXT("%s visual remains at formation z ten"), *UnitId.ToString()),
			VisualSlot ? VisualSlot->GetZOrder() : INDEX_NONE, 10);
		TestNotNull(FString::Printf(TEXT("%s retains a stable target proxy"), *UnitId.ToString()),
			Board->GetUnitTargetProxyForTest(UnitId));
	}

	UGameXXKBattleUnitVisualWidget* const CinematicAttacker = Board->GetUnitVisualForTest(TEXT("Player"));
	UGameXXKBattleUnitVisualWidget* const CinematicTarget = Board->GetUnitVisualForTest(TEXT("Enemy.Tiger"));
	UTexture2D* const AttackerIdleAtlas = CinematicAttacker ? CinematicAttacker->GetAtlasForTest() : nullptr;
	UTexture2D* const TargetIdleAtlas = CinematicTarget ? CinematicTarget->GetAtlasForTest() : nullptr;
	UWidget* const AttackerOriginalParent = CinematicAttacker ? CinematicAttacker->GetParent() : nullptr;
	UWidget* const TargetOriginalParent = CinematicTarget ? CinematicTarget->GetParent() : nullptr;
	FGameXXKBattlePresentationEvent CinematicEvent;
	CinematicEvent.EventId = 701;
	CinematicEvent.AttackerUnitId = TEXT("Player");
	CinematicEvent.TargetUnitId = TEXT("Enemy.Tiger");
	CinematicEvent.bTargetEnemy = true;
	CinematicEvent.HealthDamage = 18;
	CinematicEvent.TargetHealthBefore = 170;
	CinematicEvent.TargetHealthAfter = 152;
	FPresentationApi::Queue(Board, CinematicEvent);
	TestTrue(TEXT("the Board requests Attack asynchronously before the central action"),
		AtlasLoader->Requested(FGameXXKBattleAnimationPresentation::ResolveClip(
			TEXT("Player"), false, EGameXXKBattleAnimationAction::Attack).TexturePath));
	TestTrue(TEXT("the Board requests Hit asynchronously before the central action"),
		AtlasLoader->Requested(FGameXXKBattleAnimationPresentation::ResolveClip(
			TEXT("Enemy.Tiger"), true, EGameXXKBattleAnimationAction::Hit).TexturePath));
	TestFalse(TEXT("the Board never requests the retired generic Impact atlas"),
		AtlasLoader->Requested(FGameXXKBattleAnimationPresentation::ResolveGenericClip(
			EGameXXKBattleAnimationAction::Impact).TexturePath));

	Board->AdvanceVisualsAtRealTime(0.0);
	TestEqual(TEXT("central action reuses the exact persistent attacker object"),
		Board->GetUnitVisualForTest(TEXT("Player")), CinematicAttacker);
	TestEqual(TEXT("central action reuses the exact persistent target object"),
		Board->GetUnitVisualForTest(TEXT("Enemy.Tiger")), CinematicTarget);
	TestTrue(TEXT("central attacker remains a direct child of the same design stage"),
		CinematicAttacker && CinematicAttacker->GetParent() == AttackerOriginalParent && AttackerOriginalParent == DesignStage);
	TestTrue(TEXT("central target remains a direct child of the same design stage"),
		CinematicTarget && CinematicTarget->GetParent() == TargetOriginalParent && TargetOriginalParent == DesignStage);
	TestEqual(TEXT("an unavailable Attack atlas retains the already-loaded Idle atlas"),
		CinematicAttacker ? CinematicAttacker->GetAtlasForTest() : nullptr, AttackerIdleAtlas);
	TestEqual(TEXT("an unavailable Hit atlas retains the already-loaded Idle atlas"),
		CinematicTarget ? CinematicTarget->GetAtlasForTest() : nullptr, TargetIdleAtlas);
	TestEqual(TEXT("attacker Idle fallback plays at its authored rate"), FPresentationApi::AttackerRate(Board), 1.0f);
	TestEqual(TEXT("target Idle fallback plays at its authored rate"), FPresentationApi::TargetRate(Board), 1.0f);
	TestEqual(TEXT("the retired generic Impact has no active playback despite participant fallback"),
		FPresentationApi::ImpactRate(Board), 0.0f);
	TestTrue(TEXT("cinematic attacker size is exactly two times formation"),
		CinematicAttacker && CinematicAttacker->GetPresentedSize().Equals(FVector2D(820.0f, 820.0f), 0.01f));
	TestTrue(TEXT("cinematic target size is exactly two times formation"),
		CinematicTarget && CinematicTarget->GetPresentedSize().Equals(FVector2D(820.0f, 820.0f), 0.01f));
	TestTrue(TEXT("party cinematic participant uses the stable right-side X anchor"),
		CinematicAttacker && FMath::IsNearlyEqual(CinematicAttacker->GetStageCenter().X, 1330.0f, 0.01f));
	TestTrue(TEXT("enemy cinematic participant uses the stable left-side X anchor"),
		CinematicTarget && FMath::IsNearlyEqual(CinematicTarget->GetStageCenter().X, 590.0f, 0.01f));
	TestTrue(TEXT("party cinematic participant remains vertically centered for Task 10"),
		CinematicAttacker && FMath::IsNearlyEqual(CinematicAttacker->GetStageCenter().Y, 540.0f, 0.01f));
	TestTrue(TEXT("enemy cinematic participant remains vertically centered for Task 10"),
		CinematicTarget && FMath::IsNearlyEqual(CinematicTarget->GetStageCenter().Y, 540.0f, 0.01f));
	TestTrue(TEXT("party cinematic participant keeps positive X scale without mirroring"),
		CinematicAttacker && CinematicAttacker->GetRenderTransform().Scale.X > 0.0f);
	TestTrue(TEXT("enemy cinematic participant keeps positive X scale without mirroring"),
		CinematicTarget && CinematicTarget->GetRenderTransform().Scale.X > 0.0f);
	for (const FName UnitId : FormationUnitIds)
	{
		UGameXXKBattleUnitVisualWidget* const Visual = Board->GetUnitVisualForTest(UnitId);
		const bool bParticipant = UnitId == CinematicEvent.AttackerUnitId || UnitId == CinematicEvent.TargetUnitId;
		TestTrue(FString::Printf(TEXT("%s leaves no visible 410 by 410 formation visual during the central action"), *UnitId.ToString()),
			Visual && (bParticipant
				? Visual->GetPresentedSize().Equals(FVector2D(820.0f, 820.0f), 0.01f)
				: Visual->GetVisibility() == ESlateVisibility::Hidden));
	}
	const UCanvasPanelSlot* const AttackerCinematicSlot = CinematicAttacker ? Cast<UCanvasPanelSlot>(CinematicAttacker->Slot) : nullptr;
	const UCanvasPanelSlot* const TargetCinematicSlot = CinematicTarget ? Cast<UCanvasPanelSlot>(CinematicTarget->Slot) : nullptr;
	TestEqual(TEXT("central attacker renders at z forty"), AttackerCinematicSlot ? AttackerCinematicSlot->GetZOrder() : INDEX_NONE, 40);
	TestEqual(TEXT("central target renders at z forty"), TargetCinematicSlot ? TargetCinematicSlot->GetZOrder() : INDEX_NONE, 40);

	UBorder* const CinematicDimmer = Board->WidgetTree
		? Cast<UBorder>(Board->WidgetTree->FindWidget(TEXT("BattleCinematicDimmer")))
		: nullptr;
	UGameXXKBattleUnitVisualWidget* const CinematicImpact = Board->WidgetTree
		? Cast<UGameXXKBattleUnitVisualWidget>(Board->WidgetTree->FindWidget(TEXT("BattleCinematicImpact")))
		: nullptr;
	UTextBlock* const CinematicReadout = Board->WidgetTree
		? Cast<UTextBlock>(Board->WidgetTree->FindWidget(TEXT("BattleCinematicReadout")))
		: nullptr;
	TestNotNull(TEXT("the common stage owns a cinematic dimmer"), CinematicDimmer);
	TestNotNull(TEXT("the common stage retains only a hidden compatibility impact widget"), CinematicImpact);
	TestNotNull(TEXT("the common stage owns a damage or avoid readout"), CinematicReadout);
	const UCanvasPanelSlot* const DimmerSlot = CinematicDimmer ? Cast<UCanvasPanelSlot>(CinematicDimmer->Slot) : nullptr;
	const UCanvasPanelSlot* const ImpactSlot = CinematicImpact ? Cast<UCanvasPanelSlot>(CinematicImpact->Slot) : nullptr;
	const UCanvasPanelSlot* const ReadoutSlot = CinematicReadout ? Cast<UCanvasPanelSlot>(CinematicReadout->Slot) : nullptr;
	TestEqual(TEXT("fifty-percent black dimmer renders at z thirty"), DimmerSlot ? DimmerSlot->GetZOrder() : INDEX_NONE, 30);
	TestTrue(TEXT("the dimmer is black with exactly fifty percent opacity"),
		CinematicDimmer
		&& CinematicDimmer->GetBrushColor().Equals(FLinearColor(0.0f, 0.0f, 0.0f, 0.5f), 0.001f));
	TestEqual(TEXT("the hidden compatibility impact slot retains its harmless legacy z-order"), ImpactSlot ? ImpactSlot->GetZOrder() : INDEX_NONE, 50);
	TestEqual(TEXT("damage or avoid readout renders at z sixty"), ReadoutSlot ? ReadoutSlot->GetZOrder() : INDEX_NONE, 60);
	TestEqual(TEXT("pre-marker displayed-health overlay retains health before"),
		FPresentationApi::DisplayedHealth(Board, CinematicEvent.TargetUnitId), CinematicEvent.TargetHealthBefore);
	const UGameXXKBattleUnitHudWidget* const PreImpactTargetHud = Board->GetProjectedUnitHudForTest(CinematicEvent.TargetUnitId);
	TestEqual(TEXT("pre-marker target HUD visibly retains health before"),
		PreImpactTargetHud && PreImpactTargetHud->GetResourceWidgetForTest()
			? PreImpactTargetHud->GetResourceWidgetForTest()->GetHealthDisplayTextForTest()
			: FString(),
		FString(TEXT("气血 170 / 180")));
	TestEqual(TEXT("impact remains hidden before its marker"),
		CinematicImpact ? CinematicImpact->GetVisibility() : ESlateVisibility::Visible,
		ESlateVisibility::Hidden);

	Board->AdvanceVisualsAtRealTime(0.301);
	TestEqual(TEXT("crossing zero-point-three updates the displayed-health overlay"),
		FPresentationApi::DisplayedHealth(Board, CinematicEvent.TargetUnitId), CinematicEvent.TargetHealthAfter);
	const UGameXXKBattleUnitHudWidget* const PostImpactTargetHud = Board->GetProjectedUnitHudForTest(CinematicEvent.TargetUnitId);
	TestEqual(TEXT("crossing zero-point-three redraws the real target HUD"),
		PostImpactTargetHud && PostImpactTargetHud->GetResourceWidgetForTest()
			? PostImpactTargetHud->GetResourceWidgetForTest()->GetHealthDisplayTextForTest()
			: FString(),
		FString(TEXT("气血 152 / 180")));
	TestEqual(TEXT("crossing zero-point-three emits the damage readout"),
		FPresentationApi::Readout(Board), FString(TEXT("-18")));
	TestEqual(TEXT("the retired generic impact stays hidden at the damage marker"),
		CinematicImpact ? CinematicImpact->GetVisibility() : ESlateVisibility::Hidden,
		ESlateVisibility::Hidden);
	TestNull(TEXT("the retired generic impact never binds an atlas"),
		CinematicImpact ? CinematicImpact->GetAtlasForTest() : nullptr);

	Board->AdvanceVisualsAtRealTime(0.821);
	TestTrue(TEXT("surviving attacker restores its formation size"),
		CinematicAttacker && CinematicAttacker->GetPresentedSize().Equals(FVector2D(410.0f, 410.0f), 0.01f));
	TestTrue(TEXT("surviving target restores its formation size"),
		CinematicTarget && CinematicTarget->GetPresentedSize().Equals(FVector2D(410.0f, 410.0f), 0.01f));
	TestEqual(TEXT("surviving attacker restores formation z ten"),
		AttackerCinematicSlot ? AttackerCinematicSlot->GetZOrder() : INDEX_NONE, 10);
	TestEqual(TEXT("surviving target restores formation z ten"),
		TargetCinematicSlot ? TargetCinematicSlot->GetZOrder() : INDEX_NONE, 10);
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
	AssertFixedHudSlot(*this, Board, TEXT("Player"), EGameXXKCardTargetSide::Party, 2, FVector2D(0.755f, 0.52f));
	AssertFixedHudSlot(*this, Board, TEXT("Enemy.Tiger"), EGameXXKCardTargetSide::Enemy, 3, FVector2D(0.395f, 0.44f));

	const FGeometry NarrowGeometry = FGeometry::MakeRoot(FVector2D(1280.0f, 1024.0f), FSlateLayoutTransform());
	Board->NativeTick(NarrowGeometry, 0.0f);
	AssertFixedHudSlot(*this, Board, TEXT("Partner.Blade"), EGameXXKCardTargetSide::Party, 1, FVector2D(0.905f, 0.60f));
	AssertFixedHudSlot(*this, Board, TEXT("Enemy.MoneyRat"), EGameXXKCardTargetSide::Enemy, 1, FVector2D(0.095f, 0.60f));

	Subsystem->GetMutableRuntimeState().CardRun.ActiveBattle.Units[5].bLiving = false;
	Board->RefreshFromState();
	TestNull(TEXT("dead fixed-slot units are removed from the board HUD map"), Board->GetProjectedUnitHudForTest(TEXT("Enemy.Tiger")));
	TestNull(TEXT("dead fixed-slot units are removed from the persistent visual registry"), Board->GetUnitVisualForTest(TEXT("Enemy.Tiger")));
	TestEqual(TEXT("dead fixed-slot units reduce the board HUD count"), Board->GetProjectedUnitHudCountForTest(), 5);
	FGameXXKCardCombatUnit& TigerRuntimeUnit = Subsystem->GetMutableRuntimeState().CardRun.ActiveBattle.Units[5];
	TigerRuntimeUnit.bLiving = true;
	TigerRuntimeUnit.HP = 141;
	TigerRuntimeUnit.MaxHP = 180;
	Board->RefreshFromState();
	UGameXXKBattleUnitHudWidget* const RevivedTigerHud = Board->GetProjectedUnitHudForTest(TEXT("Enemy.Tiger"));
	TestNotNull(TEXT("a revived valid P-slot reconstructs its fixed HUD"), RevivedTigerHud);
	AssertFixedHudSlot(*this, Board, TEXT("Enemy.Tiger"), EGameXXKCardTargetSide::Enemy, 3, FVector2D(0.395f, 0.44f));
	TestEqual(TEXT("a revived fixed-slot HUD redraws current HP"),
		RevivedTigerHud && RevivedTigerHud->GetResourceWidgetForTest()
			? RevivedTigerHud->GetResourceWidgetForTest()->GetHealthDisplayTextForTest()
			: FString(),
		FString(TEXT("气血 141 / 180")));
	TestEqual(TEXT("a revived fixed-slot unit restores the board HUD count"), Board->GetProjectedUnitHudCountForTest(), 6);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBattleProjectedUnitHudIdleSyncTest,
	"GameXXK.UI.Battle.FixedSlotUnitHudIdleSync",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBattleProjectedUnitHudIdleSyncTest::RunTest(const FString& Parameters)
{
	const auto RenderedHealth = [](UGameXXKBattleBoardWidget* const Board, const FName UnitId) -> FString
	{
		const UGameXXKBattleUnitHudWidget* const Hud = Board ? Board->GetProjectedUnitHudForTest(UnitId) : nullptr;
		return Hud && Hud->GetResourceWidgetForTest()
			? Hud->GetResourceWidgetForTest()->GetHealthDisplayTextForTest()
			: FString();
	};

	UGameInstance* const TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* const Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	BuildFixedSlotHudFixture(Subsystem);

	UGameXXKBattleBoardWidget* const Board = NewObject<UGameXXKBattleBoardWidget>();
	Board->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("idle-sync HUD board initializes"), Board->Initialize());
	Board->NativeConstruct();
	Board->RefreshFromState();
	TestTrue(TEXT("idle-sync fixture begins a common-stage visual session"), Board->BeginBattleVisualSession(902));
	TestEqual(TEXT("idle-sync fixture renders the initial authoritative enemy HP"),
		RenderedHealth(Board, TEXT("Enemy.MoneyRat")), FString(TEXT("气血 54 / 90")));

	// An external runtime mutation (for example a recovery path that commits
	// authoritative HP without a Board refresh) must be picked up by the very
	// next idle visual sample.  HP text is allowed to freeze only while a damage
	// presentation is actively animating that unit.
	Subsystem->GetMutableRuntimeState().CardRun.ActiveBattle.Units[3].HP = 9;
	Board->AdvanceVisualsAtRealTime(0.10);
	TestEqual(TEXT("an idle visual sample re-syncs an externally mutated enemy HP number"),
		RenderedHealth(Board, TEXT("Enemy.MoneyRat")), FString(TEXT("气血 9 / 90")));

	Subsystem->GetMutableRuntimeState().CardRun.ActiveBattle.Units[1].HP = 31;
	const FGeometry TickGeometry = FGeometry::MakeRoot(FVector2D(1280.0f, 720.0f), FSlateLayoutTransform());
	Board->NativeTick(TickGeometry, 0.016f);
	TestEqual(TEXT("an idle board tick re-syncs an externally mutated hero HP number"),
		RenderedHealth(Board, TEXT("Player")), FString(TEXT("气血 31 / 100")));
	return true;
}

#endif
