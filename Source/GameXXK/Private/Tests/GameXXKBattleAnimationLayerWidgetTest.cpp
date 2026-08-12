#include "Misc/AutomationTest.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "GameXXKMVPRules.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKBattleAnimationPresentation.h"
#include "UI/GameXXKBattleAtlasCache.h"
#include "UI/GameXXKBattleBoardWidget.h"
#include "UI/GameXXKBattleUnitHudWidget.h"
#include "UI/GameXXKBattleUnitResourceWidget.h"
#include "UI/GameXXKBattleUnitVisualWidget.h"
#include "UObject/StrongObjectPtr.h"

#include <type_traits>
#include <utility>

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	class FPresentationAtlasLoadHandle final : public IGameXXKBattleAtlasLoadHandle
	{
	public:
		virtual void Cancel() override {}
	};

	/** Drives the real atlas cache while completing every request synchronously through its async-loader seam. */
	class FPresentationAtlasLoader final : public IGameXXKBattleAtlasLoader
	{
	public:
		virtual TSharedPtr<IGameXXKBattleAtlasLoadHandle> RequestAsyncLoad(
			const FSoftObjectPath& Path,
			FGameXXKAtlasLoaderCompletion Completion) override
		{
			RequestedPaths.Add(Path);
			UTexture2D* const Texture = NewObject<UTexture2D>(GetTransientPackage());
			LoadedTextures.Add(TStrongObjectPtr<UTexture2D>(Texture));
			TexturesByPath.Add(Path, Texture);
			Completion(Texture, 4);
			return MakeShared<FPresentationAtlasLoadHandle>();
		}

		bool Requested(const FSoftObjectPath& Path) const
		{
			return RequestedPaths.Contains(Path);
		}

		UTexture2D* GetTexture(const FSoftObjectPath& Path) const
		{
			return TexturesByPath.FindRef(Path);
		}

		int32 RequestCount(const FSoftObjectPath& Path) const
		{
			int32 Count = 0;
			for (const FSoftObjectPath& RequestedPath : RequestedPaths)
			{
				Count += RequestedPath == Path ? 1 : 0;
			}
			return Count;
		}

		TArray<FSoftObjectPath> RequestedPaths;
		TArray<TStrongObjectPtr<UTexture2D>> LoadedTextures;
		TMap<FSoftObjectPath, UTexture2D*> TexturesByPath;
	};

	/** Holds Idle completions while delivering action atlases immediately to reproduce a late-load race. */
	class FLateIdlePresentationAtlasLoader final : public IGameXXKBattleAtlasLoader
	{
	public:
		virtual TSharedPtr<IGameXXKBattleAtlasLoadHandle> RequestAsyncLoad(
			const FSoftObjectPath& Path,
			FGameXXKAtlasLoaderCompletion Completion) override
		{
			UTexture2D* const Texture = NewObject<UTexture2D>(GetTransientPackage());
			LoadedTextures.Add(TStrongObjectPtr<UTexture2D>(Texture));
			TexturesByPath.Add(Path, Texture);
			if (Path.ToString().Contains(TEXT("_idle_atlas")))
			{
				PendingIdleCompletions.Add(Path, MoveTemp(Completion));
			}
			else
			{
				Completion(Texture, 4);
			}
			return MakeShared<FPresentationAtlasLoadHandle>();
		}

		UTexture2D* GetTexture(const FSoftObjectPath& Path) const
		{
			return TexturesByPath.FindRef(Path);
		}

		bool CompleteIdle(const FSoftObjectPath& Path)
		{
			FGameXXKAtlasLoaderCompletion* const Pending = PendingIdleCompletions.Find(Path);
			UTexture2D* const Texture = TexturesByPath.FindRef(Path);
			if (!Pending || !Texture)
			{
				return false;
			}
			FGameXXKAtlasLoaderCompletion Completion = MoveTemp(*Pending);
			PendingIdleCompletions.Remove(Path);
			Completion(Texture, 4);
			return true;
		}

		bool CompleteIdleMissing(const FSoftObjectPath& Path)
		{
			FGameXXKAtlasLoaderCompletion* const Pending = PendingIdleCompletions.Find(Path);
			if (!Pending)
			{
				return false;
			}
			FGameXXKAtlasLoaderCompletion Completion = MoveTemp(*Pending);
			PendingIdleCompletions.Remove(Path);
			Completion(nullptr, 0);
			return true;
		}

		TArray<TStrongObjectPtr<UTexture2D>> LoadedTextures;
		TMap<FSoftObjectPath, UTexture2D*> TexturesByPath;
		TMap<FSoftObjectPath, FGameXXKAtlasLoaderCompletion> PendingIdleCompletions;
	};

	/** Holds the generic Impact request while every unit atlas is available immediately. */
	class FLateImpactPresentationAtlasLoader final : public IGameXXKBattleAtlasLoader
	{
	public:
		virtual TSharedPtr<IGameXXKBattleAtlasLoadHandle> RequestAsyncLoad(
			const FSoftObjectPath& Path,
			FGameXXKAtlasLoaderCompletion Completion) override
		{
			UTexture2D* const Texture = NewObject<UTexture2D>(GetTransientPackage());
			LoadedTextures.Add(TStrongObjectPtr<UTexture2D>(Texture));
			TexturesByPath.Add(Path, Texture);
			if (Path.ToString().Contains(TEXT("impact_ink_generic")))
			{
				PendingImpactCompletions.Add(Path, MoveTemp(Completion));
			}
			else
			{
				Completion(Texture, 4);
			}
			return MakeShared<FPresentationAtlasLoadHandle>();
		}

		UTexture2D* GetTexture(const FSoftObjectPath& Path) const
		{
			return TexturesByPath.FindRef(Path);
		}

		bool CompleteImpact(const FSoftObjectPath& Path)
		{
			FGameXXKAtlasLoaderCompletion* const Pending = PendingImpactCompletions.Find(Path);
			UTexture2D* const Texture = TexturesByPath.FindRef(Path);
			if (!Pending || !Texture)
			{
				return false;
			}
			FGameXXKAtlasLoaderCompletion Completion = MoveTemp(*Pending);
			PendingImpactCompletions.Remove(Path);
			Completion(Texture, 4);
			return true;
		}

		TArray<TStrongObjectPtr<UTexture2D>> LoadedTextures;
		TMap<FSoftObjectPath, UTexture2D*> TexturesByPath;
		TMap<FSoftObjectPath, FGameXXKAtlasLoaderCompletion> PendingImpactCompletions;
	};

	/** Holds Death requests while every Idle, Attack, Hit, and Impact atlas is available immediately. */
	class FLateDeathPresentationAtlasLoader final : public IGameXXKBattleAtlasLoader
	{
	public:
		virtual TSharedPtr<IGameXXKBattleAtlasLoadHandle> RequestAsyncLoad(
			const FSoftObjectPath& Path,
			FGameXXKAtlasLoaderCompletion Completion) override
		{
			RequestedPaths.Add(Path);
			UTexture2D* const Texture = NewObject<UTexture2D>(GetTransientPackage());
			LoadedTextures.Add(TStrongObjectPtr<UTexture2D>(Texture));
			TexturesByPath.Add(Path, Texture);
			if (Path.ToString().Contains(TEXT("_death_atlas")))
			{
				PendingDeathCompletions.Add(Path, MoveTemp(Completion));
			}
			else
			{
				Completion(Texture, 4);
			}
			return MakeShared<FPresentationAtlasLoadHandle>();
		}

		bool Requested(const FSoftObjectPath& Path) const
		{
			return RequestedPaths.Contains(Path);
		}

		UTexture2D* GetTexture(const FSoftObjectPath& Path) const
		{
			return TexturesByPath.FindRef(Path);
		}

		bool CompleteDeath(const FSoftObjectPath& Path)
		{
			FGameXXKAtlasLoaderCompletion* const Pending = PendingDeathCompletions.Find(Path);
			UTexture2D* const Texture = TexturesByPath.FindRef(Path);
			if (!Pending || !Texture)
			{
				return false;
			}
			FGameXXKAtlasLoaderCompletion Completion = MoveTemp(*Pending);
			PendingDeathCompletions.Remove(Path);
			Completion(Texture, 4);
			return true;
		}

		TArray<FSoftObjectPath> RequestedPaths;
		TArray<TStrongObjectPtr<UTexture2D>> LoadedTextures;
		TMap<FSoftObjectPath, UTexture2D*> TexturesByPath;
		TMap<FSoftObjectPath, FGameXXKAtlasLoaderCompletion> PendingDeathCompletions;
	};

	FGameXXKCardCombatUnit MakePresentationUnit(
		const TCHAR* UnitId,
		const EGameXXKCardTargetSide Side,
		const EGameXXKCharacterRole Role,
		const int32 StableSortOrder,
		const int32 HP,
		const int32 MaxHP)
	{
		FGameXXKCardCombatUnit Unit;
		Unit.UnitId = FName(UnitId);
		Unit.Side = Side;
		Unit.Role = Role;
		Unit.StableSortOrder = StableSortOrder;
		Unit.bLiving = true;
		Unit.HP = HP;
		Unit.MaxHP = MaxHP;
		return Unit;
	}

	void BuildPresentationFixture(UGameXXKMVPSubsystem* const Subsystem)
	{
		FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
		State = UGameXXKMVPRules::CreateNewGame();
		State.Screen = EGameXXKScreen::Battle;
		State.bHasActiveBattle = true;
		State.CardRun.bHasActiveCardBattle = true;
		State.CardRun.ActiveBattle.Units = {
			MakePresentationUnit(TEXT("Partner.Blade"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Blade, 0, 84, 100),
			MakePresentationUnit(TEXT("Player"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 1, 90, 100),
			MakePresentationUnit(TEXT("Enemy.Tiger"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 0, 70, 120),
			MakePresentationUnit(TEXT("Enemy.BlackBear"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 1, 110, 140)};
	}

	FGameXXKBattlePresentationEvent MakePresentationEvent(
		const uint64 EventId,
		const FName AttackerUnitId,
		const bool bAttackerEnemy,
		const FName TargetUnitId,
		const bool bTargetEnemy,
		const int32 HealthBefore,
		const int32 HealthAfter,
		const bool bAvoided = false,
		const bool bTargetDefeated = false,
		const int32 HitOrdinal = 0)
	{
		FGameXXKBattlePresentationEvent Event;
		Event.EventId = EventId;
		Event.HitOrdinal = HitOrdinal;
		Event.AttackerUnitId = AttackerUnitId;
		Event.bAttackerEnemy = bAttackerEnemy;
		Event.TargetUnitId = TargetUnitId;
		Event.bTargetEnemy = bTargetEnemy;
		Event.TargetHealthBefore = HealthBefore;
		Event.TargetHealthAfter = HealthAfter;
		Event.HealthDamage = FMath::Max(0, HealthBefore - HealthAfter);
		Event.bAvoided = bAvoided;
		Event.bTargetDefeated = bTargetDefeated;
		return Event;
	}

	FString GetRenderedHealth(const UGameXXKBattleBoardWidget* const Board, const FName UnitId)
	{
		const UGameXXKBattleUnitHudWidget* const Hud = Board ? Board->GetProjectedUnitHudForTest(UnitId) : nullptr;
		return Hud && Hud->GetResourceWidgetForTest()
			? Hud->GetResourceWidgetForTest()->GetHealthDisplayTextForTest()
			: FString();
	}

	/**
	 * The primary template lets this new behavior test compile against the RED baseline.
	 * Once the wished-for Board API exists, the specialization below exercises only real Board state.
	 */
	template <typename TBoard, typename = void>
	struct TPresentationBoardApi
	{
		static constexpr bool bAvailable = false;
		static void Queue(TBoard*, const FGameXXKBattlePresentationEvent&) {}
		static bool IsActive(const TBoard*) { return false; }
		static bool IsDeathActive(const TBoard*) { return false; }
		static int32 QueueCount(const TBoard*) { return INDEX_NONE; }
		static uint64 ActiveEventId(const TBoard*) { return 0; }
		static double ActiveElapsed(const TBoard*) { return -1.0; }
		static double ActiveDuration(const TBoard*) { return -1.0; }
		static int32 ImpactCount(const TBoard*) { return INDEX_NONE; }
		static int32 CompletionCount(const TBoard*) { return INDEX_NONE; }
		static int32 ShakeCount(const TBoard*) { return INDEX_NONE; }
		static FVector2D ShakeAmplitude(const TBoard*) { return FVector2D(-1.0f, -1.0f); }
		static double ShakeDuration(const TBoard*) { return -1.0; }
		static int32 DisplayedHealth(const TBoard*, FName) { return INDEX_NONE; }
		static float AttackerRate(const TBoard*) { return 0.0f; }
		static float TargetRate(const TBoard*) { return 0.0f; }
		static float ImpactRate(const TBoard*) { return 0.0f; }
		static FString Readout(const TBoard*) { return FString(); }
		static FVector2D ReadoutScale(const TBoard*) { return FVector2D::ZeroVector; }
		static float ReadoutOpacity(const TBoard*) { return -1.0f; }
	};

	template <typename TBoard>
	struct TPresentationBoardApi<TBoard, std::void_t<
		decltype(std::declval<TBoard&>().QueuePresentation(std::declval<const FGameXXKBattlePresentationEvent&>())),
		decltype(std::declval<const TBoard&>().IsBattlePresentationActiveForTest()),
		decltype(std::declval<const TBoard&>().IsBattleDeathPresentationActiveForTest()),
		decltype(std::declval<const TBoard&>().GetBattlePresentationQueueCountForTest()),
		decltype(std::declval<const TBoard&>().GetActiveBattlePresentationEventIdForTest()),
		decltype(std::declval<const TBoard&>().GetActiveBattlePresentationElapsedForTest()),
		decltype(std::declval<const TBoard&>().GetActiveBattlePresentationDurationForTest()),
		decltype(std::declval<const TBoard&>().GetBattlePresentationImpactCountForTest()),
		decltype(std::declval<const TBoard&>().GetBattlePresentationCompletionCountForTest()),
		decltype(std::declval<const TBoard&>().GetBattlePresentationHudShakeCountForTest()),
		decltype(std::declval<const TBoard&>().GetBattlePresentationShakeAmplitudeForTest()),
		decltype(std::declval<const TBoard&>().GetBattlePresentationShakeDurationForTest()),
		decltype(std::declval<const TBoard&>().GetDisplayedHealthForTest(std::declval<FName>())),
		decltype(std::declval<const TBoard&>().GetActiveAttackerPlaybackRateForTest()),
		decltype(std::declval<const TBoard&>().GetActiveTargetPlaybackRateForTest()),
		decltype(std::declval<const TBoard&>().GetActiveImpactPlaybackRateForTest()),
		decltype(std::declval<const TBoard&>().GetBattlePresentationReadoutForTest()),
		decltype(std::declval<const TBoard&>().GetBattlePresentationReadoutScaleForTest()),
		decltype(std::declval<const TBoard&>().GetBattlePresentationReadoutOpacityForTest())>>
	{
		static constexpr bool bAvailable = true;
		static void Queue(TBoard* Board, const FGameXXKBattlePresentationEvent& Event) { Board->QueuePresentation(Event); }
		static bool IsActive(const TBoard* Board) { return Board->IsBattlePresentationActiveForTest(); }
		static bool IsDeathActive(const TBoard* Board) { return Board->IsBattleDeathPresentationActiveForTest(); }
		static int32 QueueCount(const TBoard* Board) { return Board->GetBattlePresentationQueueCountForTest(); }
		static uint64 ActiveEventId(const TBoard* Board) { return Board->GetActiveBattlePresentationEventIdForTest(); }
		static double ActiveElapsed(const TBoard* Board) { return Board->GetActiveBattlePresentationElapsedForTest(); }
		static double ActiveDuration(const TBoard* Board) { return Board->GetActiveBattlePresentationDurationForTest(); }
		static int32 ImpactCount(const TBoard* Board) { return Board->GetBattlePresentationImpactCountForTest(); }
		static int32 CompletionCount(const TBoard* Board) { return Board->GetBattlePresentationCompletionCountForTest(); }
		static int32 ShakeCount(const TBoard* Board) { return Board->GetBattlePresentationHudShakeCountForTest(); }
		static FVector2D ShakeAmplitude(const TBoard* Board) { return Board->GetBattlePresentationShakeAmplitudeForTest(); }
		static double ShakeDuration(const TBoard* Board) { return Board->GetBattlePresentationShakeDurationForTest(); }
		static int32 DisplayedHealth(const TBoard* Board, const FName UnitId) { return Board->GetDisplayedHealthForTest(UnitId); }
		static float AttackerRate(const TBoard* Board) { return Board->GetActiveAttackerPlaybackRateForTest(); }
		static float TargetRate(const TBoard* Board) { return Board->GetActiveTargetPlaybackRateForTest(); }
		static float ImpactRate(const TBoard* Board) { return Board->GetActiveImpactPlaybackRateForTest(); }
		static FString Readout(const TBoard* Board) { return Board->GetBattlePresentationReadoutForTest(); }
		static FVector2D ReadoutScale(const TBoard* Board) { return Board->GetBattlePresentationReadoutScaleForTest(); }
		static float ReadoutOpacity(const TBoard* Board) { return Board->GetBattlePresentationReadoutOpacityForTest(); }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBattleAnimationLayerWidgetTest,
	"GameXXK.MVP.Battle.AnimationLayerWidget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBattleAnimationLayerWidgetTest::RunTest(const FString& Parameters)
{
	using FApi = TPresentationBoardApi<UGameXXKBattleBoardWidget>;
	TestTrue(TEXT("Board exposes the immutable marker-driven presentation queue"), FApi::bAvailable);
	if (!FApi::bAvailable)
	{
		return false;
	}

	UGameInstance* const TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* const Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	BuildPresentationFixture(Subsystem);

	UGameXXKBattleBoardWidget* const Board = NewObject<UGameXXKBattleBoardWidget>();
	Board->SetMVPSubsystem(Subsystem);
	const TSharedRef<FPresentationAtlasLoader> AtlasLoader = MakeShared<FPresentationAtlasLoader>();
	Board->SetAtlasCacheForTest(MakeUnique<FGameXXKBattleAtlasCache>(AtlasLoader, []() { return 0.0; }));
	TestTrue(TEXT("timeline fixture initializes its real Board"), Board->Initialize());
	Board->NativeConstruct();
	TestTrue(TEXT("timeline fixture begins one visual session"), Board->BeginBattleVisualSession(1001));

	UGameXXKBattleUnitVisualWidget* const AttackerVisual = Board->GetUnitVisualForTest(TEXT("Player"));
	UGameXXKBattleUnitVisualWidget* const TargetVisual = Board->GetUnitVisualForTest(TEXT("Enemy.Tiger"));
	TestNotNull(TEXT("timeline fixture owns the persistent attacker visual"), AttackerVisual);
	TestNotNull(TEXT("timeline fixture owns the persistent target visual"), TargetVisual);
	UWidget* const AttackerParent = AttackerVisual ? AttackerVisual->GetParent() : nullptr;
	UWidget* const TargetParent = TargetVisual ? TargetVisual->GetParent() : nullptr;

	const FGameXXKBattlePresentationEvent First = MakePresentationEvent(
		1, TEXT("Player"), false, TEXT("Enemy.Tiger"), true, 100, 70);
	const FGameXXKBattleAnimationClipDescriptor FirstAttackClip =
		FGameXXKBattleAnimationPresentation::ResolveClip(
			First.AttackerUnitId, First.bAttackerEnemy, EGameXXKBattleAnimationAction::Attack);
	const FGameXXKBattleAnimationClipDescriptor FirstHitClip =
		FGameXXKBattleAnimationPresentation::ResolveClip(
			First.TargetUnitId, First.bTargetEnemy, EGameXXKBattleAnimationAction::Hit);
	FApi::Queue(Board, First);
	TestTrue(TEXT("queueing prefetches the attacker Attack atlas before presentation starts"),
		AtlasLoader->Requested(FirstAttackClip.TexturePath));
	TestTrue(TEXT("queueing prefetches the target Hit atlas before presentation starts"),
		AtlasLoader->Requested(FirstHitClip.TexturePath));
	TestFalse(TEXT("queueing never prefetches the retired generic Impact atlas"),
		AtlasLoader->Requested(FGameXXKBattleAnimationPresentation::ResolveGenericClip(
			EGameXXKBattleAnimationAction::Impact).TexturePath));

	Board->AdvanceVisualsAtRealTime(0.0);
	TestTrue(TEXT("the first absolute-clock sample starts the queued presentation"), FApi::IsActive(Board));
	TestEqual(TEXT("the immutable event id survives queue activation"), FApi::ActiveEventId(Board), First.EventId);
	const FGameXXKBattleAnimationClipDescriptor FittedFirstAttackClip =
		FGameXXKBattleAnimationPresentation::FitClipToDuration(FirstAttackClip, 0.82f);
	const FGameXXKBattleAnimationClipDescriptor FittedFirstHitClip =
		FGameXXKBattleAnimationPresentation::FitClipToDuration(FirstHitClip, 0.82f);
	TestTrue(TEXT("the active first-hit duration is zero-point-eight-two seconds"),
		FMath::IsNearlyEqual(FApi::ActiveDuration(Board), 0.82, 0.0001));
	TestEqual(TEXT("Attack playback fits the complete atlas to the first-hit rhythm"),
		FApi::AttackerRate(Board), FittedFirstAttackClip.PlaybackRate);
	TestEqual(TEXT("Hit playback fits the complete atlas to the first-hit rhythm"),
		FApi::TargetRate(Board), FittedFirstHitClip.PlaybackRate);
	TestEqual(TEXT("the retired generic Impact has no active playback"), FApi::ImpactRate(Board), 0.0f);
	TestEqual(TEXT("the existing attacker visual binds the asynchronously loaded Attack atlas"),
		AttackerVisual ? AttackerVisual->GetAtlasForTest() : nullptr,
		AtlasLoader->GetTexture(FirstAttackClip.TexturePath));
	TestEqual(TEXT("the existing target visual binds the asynchronously loaded Hit atlas"),
		TargetVisual ? TargetVisual->GetAtlasForTest() : nullptr,
		AtlasLoader->GetTexture(FirstHitClip.TexturePath));
	TestTrue(TEXT("the real Attack visual is visible, enlarged, and positively scaled"),
		AttackerVisual
		&& AttackerVisual->GetVisibility() == ESlateVisibility::SelfHitTestInvisible
		&& AttackerVisual->GetUnitImageForTest()
		&& AttackerVisual->GetUnitImageForTest()->GetVisibility() == ESlateVisibility::SelfHitTestInvisible
		&& AttackerVisual->GetPresentedSize().Equals(FVector2D(820.0f, 820.0f), 0.01f)
		&& AttackerVisual->GetRenderTransform().Scale.X > 0.0f);
	TestTrue(TEXT("the real Hit visual is visible, enlarged, and positively scaled"),
		TargetVisual
		&& TargetVisual->GetVisibility() == ESlateVisibility::SelfHitTestInvisible
		&& TargetVisual->GetUnitImageForTest()
		&& TargetVisual->GetUnitImageForTest()->GetVisibility() == ESlateVisibility::SelfHitTestInvisible
		&& TargetVisual->GetPresentedSize().Equals(FVector2D(820.0f, 820.0f), 0.01f)
		&& TargetVisual->GetRenderTransform().Scale.X > 0.0f);
	UBorder* const TimelineDimmer = Board->WidgetTree
		? Cast<UBorder>(Board->WidgetTree->FindWidget(TEXT("BattleCinematicDimmer")))
		: nullptr;
	const UCanvasPanelSlot* const TimelineDimmerSlot = TimelineDimmer
		? Cast<UCanvasPanelSlot>(TimelineDimmer->Slot)
		: nullptr;
	TestTrue(TEXT("the real fifty-percent dimmer is visible throughout the action"),
		TimelineDimmer
		&& TimelineDimmer->GetVisibility() == ESlateVisibility::SelfHitTestInvisible
		&& TimelineDimmer->GetBrushColor().Equals(FLinearColor(0.0f, 0.0f, 0.0f, 0.5f), 0.001f));
	TestEqual(TEXT("the real action dimmer renders at z thirty"),
		TimelineDimmerSlot ? TimelineDimmerSlot->GetZOrder() : INDEX_NONE,
		30);
	UCanvasPanel* const ViewportCinematicCover = Board->WidgetTree
		? Cast<UCanvasPanel>(Board->WidgetTree->FindWidget(TEXT("BattleCinematicViewportCover")))
		: nullptr;
	TestTrue(TEXT("the close-up owns a full-viewport cover for aspect-ratio margins"),
		ViewportCinematicCover
		&& ViewportCinematicCover->GetParent() == Board->GetRootWidget()
		&& ViewportCinematicCover->GetVisibility() == ESlateVisibility::SelfHitTestInvisible);
	TestEqual(TEXT("presentation overlays the pre-impact health immediately"),
		FApi::DisplayedHealth(Board, First.TargetUnitId), First.TargetHealthBefore);
	TestEqual(TEXT("the real fixed HUD renders the pre-impact health"),
		GetRenderedHealth(Board, First.TargetUnitId), FString(TEXT("气血 100 / 120")));

	Board->AdvanceVisualsAtRealTime(0.15);
	const int32 ExpectedAttackFrameAtPartial =
		FGameXXKBattleAnimationPresentation::CalculateFrameIndex(FittedFirstAttackClip, 0.15f, false);
	const int32 ExpectedHitFrameAtPartial =
		FGameXXKBattleAnimationPresentation::CalculateFrameIndex(FittedFirstHitClip, 0.15f, false);
	TestEqual(TEXT("the real Attack visual advances at the authored absolute-time frame"),
		AttackerVisual ? AttackerVisual->GetCurrentFrameForTest() : INDEX_NONE,
		ExpectedAttackFrameAtPartial);
	TestEqual(TEXT("the real Hit visual advances at the authored absolute-time frame"),
		TargetVisual ? TargetVisual->GetCurrentFrameForTest() : INDEX_NONE,
		ExpectedHitFrameAtPartial);
	TestEqual(TEXT("paired Attack and Hit visuals remain frame-synchronized"),
		AttackerVisual ? AttackerVisual->GetCurrentFrameForTest() : INDEX_NONE,
		TargetVisual ? TargetVisual->GetCurrentFrameForTest() : INDEX_NONE);
	TestEqual(TEXT("the first partial step retains pre-impact health"),
		FApi::DisplayedHealth(Board, First.TargetUnitId), First.TargetHealthBefore);
	Board->AdvanceVisualsAtRealTime(0.299);
	TestEqual(TEXT("the second partial step immediately before zero-point-three retains pre-impact health"),
		FApi::DisplayedHealth(Board, First.TargetUnitId), First.TargetHealthBefore);
	TestEqual(TEXT("impact has not fired before the zero-point-three marker"), FApi::ImpactCount(Board), 0);
	TestEqual(TEXT("HUD shake has not fired before the zero-point-three marker"), FApi::ShakeCount(Board), 0);

	Board->AdvanceVisualsAtRealTime(0.301);
	TestEqual(TEXT("crossing zero-point-three applies the immutable post-impact health"),
		FApi::DisplayedHealth(Board, First.TargetUnitId), First.TargetHealthAfter);
	TestEqual(TEXT("crossing zero-point-three redraws the actual fixed HUD"),
		GetRenderedHealth(Board, First.TargetUnitId), FString(TEXT("气血 70 / 120")));
	TestEqual(TEXT("crossing zero-point-three fires impact exactly once"), FApi::ImpactCount(Board), 1);
	TestEqual(TEXT("crossing zero-point-three fires the HUD-root shake exactly once"), FApi::ShakeCount(Board), 1);
	TestEqual(TEXT("damage presentation emits its readout at the marker"), FApi::Readout(Board), FString(TEXT("-30")));
	TestTrue(TEXT("thirty-percent heavy impact drives the authored nine-by-four-point-five shake"),
		FApi::ShakeAmplitude(Board).Equals(FVector2D(9.0f, 4.5f), 0.001f));
	TestTrue(TEXT("thirty-percent heavy impact drives the authored zero-point-two shake duration"),
		FMath::IsNearlyEqual(FApi::ShakeDuration(Board), 0.20, 0.0001));
	TestTrue(TEXT("heavy damage readout reaches its one-point-three impact peak"),
		FApi::ReadoutScale(Board).Equals(FVector2D(1.30f, 1.30f), 0.001f));
	TestTrue(TEXT("damage readout is fully opaque at impact"),
		FMath::IsNearlyEqual(FApi::ReadoutOpacity(Board), 1.0f, 0.001f));
	TestTrue(TEXT("crossing the marker moves the full viewport root for HUD shake"),
		Board->GetBattleViewportRootForTest()
		&& !Board->GetBattleViewportRootForTest()->GetRenderTransform().Translation.IsNearlyZero(0.001f));
	Board->AdvanceVisualsAtRealTime(0.50);
	TestEqual(TEXT("later samples cannot refire the same impact"), FApi::ImpactCount(Board), 1);
	TestEqual(TEXT("later samples cannot restart the same shake"), FApi::ShakeCount(Board), 1);
	TestTrue(TEXT("heavy readout settles toward unit scale after the impact peak"),
		FApi::ReadoutScale(Board).X < 1.30f && FApi::ReadoutScale(Board).X > 1.0f);
	TestTrue(TEXT("heavy readout fades after the impact peak"),
		FApi::ReadoutOpacity(Board) < 1.0f && FApi::ReadoutOpacity(Board) > 0.0f);
	TestTrue(TEXT("heavy shake is exactly settled at its authored duration"),
		Board->GetBattleViewportRootForTest()
		&& Board->GetBattleViewportRootForTest()->GetRenderTransform().Translation.IsNearlyZero(0.001f));

	Board->AdvanceVisualsAtRealTime(0.82);
	TestFalse(TEXT("a nonlethal presentation completes at exactly zero-point-eight-two real seconds"), FApi::IsActive(Board));
	TestEqual(TEXT("the paired Attack/Hit completion fires exactly once"), FApi::CompletionCount(Board), 1);
	TestEqual(TEXT("completion hides the real cinematic dimmer"),
		TimelineDimmer ? TimelineDimmer->GetVisibility() : ESlateVisibility::Visible,
		ESlateVisibility::Hidden);
	TestEqual(TEXT("completion also hides the full-viewport margin cover"),
		ViewportCinematicCover ? ViewportCinematicCover->GetVisibility() : ESlateVisibility::Visible,
		ESlateVisibility::Hidden);
	TestTrue(TEXT("completion restores damage readout unit scale"),
		FApi::ReadoutScale(Board).Equals(FVector2D(1.0f, 1.0f), 0.001f));
	TestTrue(TEXT("completion restores damage readout opacity"),
		FMath::IsNearlyEqual(FApi::ReadoutOpacity(Board), 1.0f, 0.001f));
	TestTrue(TEXT("completion restores the full viewport root after shake"),
		Board->GetBattleViewportRootForTest()
		&& Board->GetBattleViewportRootForTest()->GetRenderTransform().Translation.IsNearlyZero(0.001f));
	TestEqual(TEXT("the same attacker visual remains owned by the same design stage"),
		Board->GetUnitVisualForTest(TEXT("Player")), AttackerVisual);
	TestEqual(TEXT("the same target visual remains owned by the same design stage"),
		Board->GetUnitVisualForTest(TEXT("Enemy.Tiger")), TargetVisual);
	TestTrue(TEXT("the surviving attacker restores its 410 by 410 Idle formation"),
		AttackerVisual
		&& AttackerVisual->GetParent() == AttackerParent
		&& AttackerVisual->GetPresentedSize().Equals(FVector2D(410.0f, 410.0f), 0.01f));
	TestTrue(TEXT("the surviving target restores its 410 by 410 Idle formation"),
		TargetVisual
		&& TargetVisual->GetParent() == TargetParent
		&& TargetVisual->GetPresentedSize().Equals(FVector2D(410.0f, 410.0f), 0.01f));

	UGameInstance* const FiveHitGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* const FiveHitSubsystem = NewObject<UGameXXKMVPSubsystem>(FiveHitGameInstance);
	BuildPresentationFixture(FiveHitSubsystem);
	UGameXXKBattleBoardWidget* const FiveHitBoard = NewObject<UGameXXKBattleBoardWidget>();
	FiveHitBoard->SetMVPSubsystem(FiveHitSubsystem);
	const TSharedRef<FPresentationAtlasLoader> FiveHitLoader = MakeShared<FPresentationAtlasLoader>();
	FiveHitBoard->SetAtlasCacheForTest(MakeUnique<FGameXXKBattleAtlasCache>(
		FiveHitLoader,
		[]() { return 0.0; }));
	TestTrue(TEXT("five-hit fixture initializes its real Board"), FiveHitBoard->Initialize());
	FiveHitBoard->NativeConstruct();
	TestTrue(TEXT("five-hit fixture begins one visual session"), FiveHitBoard->BeginBattleVisualSession(1010));
	for (int32 HitIndex = 0; HitIndex < 5; ++HitIndex)
	{
		FApi::Queue(FiveHitBoard, MakePresentationEvent(
			100 + HitIndex,
			TEXT("Player"),
			false,
			TEXT("Enemy.Tiger"),
			true,
			70 - HitIndex * 10,
			60 - HitIndex * 10,
			false,
			false,
			HitIndex));
	}
	FiveHitBoard->AdvanceVisualsAtRealTime(0.0);
	TestTrue(TEXT("five-hit first packet uses the readable zero-point-eight-two duration"),
		FMath::IsNearlyEqual(FApi::ActiveDuration(FiveHitBoard), 0.82, 0.0001));
	FiveHitBoard->AdvanceVisualsAtRealTime(0.299);
	TestEqual(TEXT("five-hit packet one retains old HP before its own impact"),
		FApi::DisplayedHealth(FiveHitBoard, TEXT("Enemy.Tiger")), 70);
	FiveHitBoard->AdvanceVisualsAtRealTime(0.301);
	TestEqual(TEXT("five-hit packet one applies only its own HP at impact"),
		FApi::DisplayedHealth(FiveHitBoard, TEXT("Enemy.Tiger")), 60);
	TestEqual(TEXT("five-hit packet one fires one impact"), FApi::ImpactCount(FiveHitBoard), 1);
	FiveHitBoard->AdvanceVisualsAtRealTime(0.821);
	TestEqual(TEXT("five-hit packet two starts after one completion"), FApi::ActiveEventId(FiveHitBoard), 101ull);
	TestEqual(TEXT("five-hit packet one completes exactly once"), FApi::CompletionCount(FiveHitBoard), 1);
	TestTrue(TEXT("five-hit follow-up packets use the compact zero-point-three duration"),
		FMath::IsNearlyEqual(FApi::ActiveDuration(FiveHitBoard), 0.30, 0.0001));
	const FGameXXKBattleAnimationClipDescriptor FittedFollowAttackClip =
		FGameXXKBattleAnimationPresentation::FitClipToDuration(FirstAttackClip, 0.30f);
	TestEqual(TEXT("five-hit follow-up refits the complete attack atlas to zero-point-three seconds"),
		FApi::AttackerRate(FiveHitBoard), FittedFollowAttackClip.PlaybackRate);
	FiveHitBoard->AdvanceVisualsAtRealTime(0.919);
	TestEqual(TEXT("five-hit packet two still shows packet-one HP before its marker"),
		FApi::DisplayedHealth(FiveHitBoard, TEXT("Enemy.Tiger")), 60);
	FiveHitBoard->AdvanceVisualsAtRealTime(0.921);
	TestEqual(TEXT("five-hit packet two applies only its own HP at impact"),
		FApi::DisplayedHealth(FiveHitBoard, TEXT("Enemy.Tiger")), 50);
	FiveHitBoard->AdvanceVisualsAtRealTime(1.121);
	FiveHitBoard->AdvanceVisualsAtRealTime(1.221);
	TestEqual(TEXT("five-hit packet three owns the third impact"), FApi::ImpactCount(FiveHitBoard), 3);
	TestEqual(TEXT("five-hit packet three reaches its immutable HP"),
		FApi::DisplayedHealth(FiveHitBoard, TEXT("Enemy.Tiger")), 40);
	FiveHitBoard->AdvanceVisualsAtRealTime(1.421);
	FiveHitBoard->AdvanceVisualsAtRealTime(1.521);
	TestEqual(TEXT("five-hit packet four owns the fourth impact"), FApi::ImpactCount(FiveHitBoard), 4);
	TestEqual(TEXT("five-hit packet four reaches its immutable HP"),
		FApi::DisplayedHealth(FiveHitBoard, TEXT("Enemy.Tiger")), 30);
	FiveHitBoard->AdvanceVisualsAtRealTime(1.721);
	FiveHitBoard->AdvanceVisualsAtRealTime(1.821);
	TestEqual(TEXT("five-hit packet five owns the fifth impact"), FApi::ImpactCount(FiveHitBoard), 5);
	TestEqual(TEXT("five-hit packet five reaches its immutable HP"),
		FApi::DisplayedHealth(FiveHitBoard, TEXT("Enemy.Tiger")), 20);
	FiveHitBoard->AdvanceVisualsAtRealTime(2.019);
	TestTrue(TEXT("five-hit queue remains active immediately before the two-point-zero-two boundary"),
		FApi::IsActive(FiveHitBoard));
	FiveHitBoard->AdvanceVisualsAtRealTime(2.0201);
	TestFalse(TEXT("five-hit queue drains within the two-point-zero-two-second timing tolerance"),
		FApi::IsActive(FiveHitBoard));
	TestEqual(TEXT("five-hit queue fires every impact exactly once"), FApi::ImpactCount(FiveHitBoard), 5);
	TestEqual(TEXT("five-hit queue fires every completion exactly once"), FApi::CompletionCount(FiveHitBoard), 5);

	const auto BuildImpactFeedbackBoard = [this](
		const uint64 SessionToken,
		const double StartSeconds,
		const int32 Damage,
		const bool bDefeated,
		const bool bAvoided = false)
	{
		UGameInstance* const GameInstance = NewObject<UGameInstance>();
		UGameXXKMVPSubsystem* const FeedbackSubsystem = NewObject<UGameXXKMVPSubsystem>(GameInstance);
		BuildPresentationFixture(FeedbackSubsystem);
		UGameXXKBattleBoardWidget* const FeedbackBoard = NewObject<UGameXXKBattleBoardWidget>();
		FeedbackBoard->SetMVPSubsystem(FeedbackSubsystem);
		const TSharedRef<FPresentationAtlasLoader> FeedbackLoader = MakeShared<FPresentationAtlasLoader>();
		FeedbackBoard->SetAtlasCacheForTest(MakeUnique<FGameXXKBattleAtlasCache>(
			FeedbackLoader,
			[StartSeconds]() { return StartSeconds; }));
		TestTrue(TEXT("graded feedback fixture initializes its real Board"), FeedbackBoard->Initialize());
		FeedbackBoard->NativeConstruct();
		TestTrue(TEXT("graded feedback fixture begins one visual session"),
			FeedbackBoard->BeginBattleVisualSession(SessionToken));
		const int32 HealthAfter = bDefeated ? 0 : FMath::Max(0, 100 - Damage);
		FApi::Queue(FeedbackBoard, MakePresentationEvent(
			SessionToken,
			TEXT("Player"),
			false,
			TEXT("Enemy.Tiger"),
			true,
			100,
			HealthAfter,
			bAvoided,
			bDefeated));
		FeedbackBoard->AdvanceVisualsAtRealTime(StartSeconds);
		FeedbackBoard->AdvanceVisualsAtRealTime(StartSeconds + (bAvoided ? 0.161 : 0.301));
		return FeedbackBoard;
	};
	UGameXXKBattleBoardWidget* const LightFeedbackBoard =
		BuildImpactFeedbackBoard(1101, 30.0, 5, false);
	TestTrue(TEXT("five-percent light hit uses the authored three-by-one-point-five shake"),
		FApi::ShakeAmplitude(LightFeedbackBoard).Equals(FVector2D(3.0f, 1.5f), 0.001f));
	TestTrue(TEXT("five-percent light hit uses the authored zero-point-twelve shake"),
		FMath::IsNearlyEqual(FApi::ShakeDuration(LightFeedbackBoard), 0.12, 0.0001));
	TestTrue(TEXT("five-percent light hit uses the one-point-twelve readout peak"),
		FApi::ReadoutScale(LightFeedbackBoard).Equals(FVector2D(1.12f, 1.12f), 0.001f));
	UGameXXKBattleBoardWidget* const MediumFeedbackBoard =
		BuildImpactFeedbackBoard(1102, 32.0, 20, false);
	TestTrue(TEXT("twenty-percent medium hit uses the authored six-by-three shake"),
		FApi::ShakeAmplitude(MediumFeedbackBoard).Equals(FVector2D(6.0f, 3.0f), 0.001f));
	TestTrue(TEXT("twenty-percent medium hit uses the authored zero-point-sixteen shake"),
		FMath::IsNearlyEqual(FApi::ShakeDuration(MediumFeedbackBoard), 0.16, 0.0001));
	TestTrue(TEXT("twenty-percent medium hit uses the one-point-two readout peak"),
		FApi::ReadoutScale(MediumFeedbackBoard).Equals(FVector2D(1.20f, 1.20f), 0.001f));
	UGameXXKBattleBoardWidget* const HeavyFeedbackBoard =
		BuildImpactFeedbackBoard(1103, 34.0, 45, false);
	TestTrue(TEXT("forty-five-percent heavy hit uses the authored nine-by-four-point-five shake"),
		FApi::ShakeAmplitude(HeavyFeedbackBoard).Equals(FVector2D(9.0f, 4.5f), 0.001f));
	TestTrue(TEXT("forty-five-percent heavy hit uses the one-point-three readout peak"),
		FApi::ReadoutScale(HeavyFeedbackBoard).Equals(FVector2D(1.30f, 1.30f), 0.001f));
	UGameXXKBattleBoardWidget* const LethalFeedbackBoard =
		BuildImpactFeedbackBoard(1104, 36.0, 1, true);
	TestTrue(TEXT("lethal transition uses the authored fourteen-by-seven shake"),
		FApi::ShakeAmplitude(LethalFeedbackBoard).Equals(FVector2D(14.0f, 7.0f), 0.001f));
	TestTrue(TEXT("lethal transition uses the authored zero-point-two-six shake"),
		FMath::IsNearlyEqual(FApi::ShakeDuration(LethalFeedbackBoard), 0.26, 0.0001));
	TestTrue(TEXT("lethal transition uses the one-point-four-two readout peak"),
		FApi::ReadoutScale(LethalFeedbackBoard).Equals(FVector2D(1.42f, 1.42f), 0.001f));

	LethalFeedbackBoard->CancelBattleVisualSession(1104);
	TestTrue(TEXT("session cancellation restores zero shake amplitude"),
		FApi::ShakeAmplitude(LethalFeedbackBoard).IsNearlyZero(0.001f));
	TestTrue(TEXT("session cancellation restores zero shake duration"),
		FMath::IsNearlyZero(FApi::ShakeDuration(LethalFeedbackBoard), 0.0001));
	TestTrue(TEXT("session cancellation restores readout unit scale"),
		FApi::ReadoutScale(LethalFeedbackBoard).Equals(FVector2D(1.0f, 1.0f), 0.001f));
	TestTrue(TEXT("session cancellation restores readout opacity"),
		FMath::IsNearlyEqual(FApi::ReadoutOpacity(LethalFeedbackBoard), 1.0f, 0.001f));
	TestTrue(TEXT("session cancellation restores the viewport root translation"),
		LethalFeedbackBoard->GetBattleViewportRootForTest()
		&& LethalFeedbackBoard->GetBattleViewportRootForTest()->GetRenderTransform().Translation.IsNearlyZero(0.001f));
	LethalFeedbackBoard->NativeDestruct();
	TestTrue(TEXT("widget teardown keeps readout unit scale"),
		FApi::ReadoutScale(LethalFeedbackBoard).Equals(FVector2D(1.0f, 1.0f), 0.001f));
	TestTrue(TEXT("widget teardown keeps readout opacity"),
		FMath::IsNearlyEqual(FApi::ReadoutOpacity(LethalFeedbackBoard), 1.0f, 0.001f));

	UGameInstance* const CrossDeathGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* const CrossDeathSubsystem = NewObject<UGameXXKMVPSubsystem>(CrossDeathGameInstance);
	BuildPresentationFixture(CrossDeathSubsystem);
	UGameXXKBattleBoardWidget* const CrossDeathBoard = NewObject<UGameXXKBattleBoardWidget>();
	CrossDeathBoard->SetMVPSubsystem(CrossDeathSubsystem);
	const TSharedRef<FPresentationAtlasLoader> CrossDeathLoader = MakeShared<FPresentationAtlasLoader>();
	CrossDeathBoard->SetAtlasCacheForTest(MakeUnique<FGameXXKBattleAtlasCache>(
		CrossDeathLoader,
		[]() { return 40.0; }));
	TestTrue(TEXT("cross-Death fixture initializes its real Board"), CrossDeathBoard->Initialize());
	CrossDeathBoard->NativeConstruct();
	TestTrue(TEXT("cross-Death fixture begins one visual session"),
		CrossDeathBoard->BeginBattleVisualSession(1011));
	const FGameXXKBattlePresentationEvent CrossDeathLethal = MakePresentationEvent(
		200,
		TEXT("Player"),
		false,
		TEXT("Enemy.Tiger"),
		true,
		70,
		0,
		false,
		true,
		0);
	const FGameXXKBattlePresentationEvent CrossDeathFollowUp = MakePresentationEvent(
		201,
		TEXT("Enemy.BlackBear"),
		true,
		TEXT("Player"),
		false,
		90,
		80,
		false,
		false,
		1);
	FApi::Queue(CrossDeathBoard, CrossDeathLethal);
	FApi::Queue(CrossDeathBoard, CrossDeathFollowUp);
	CrossDeathBoard->AdvanceVisualsAtRealTime(40.0);
	TestEqual(TEXT("cross-Death fixture starts on the lethal immutable event"),
		FApi::ActiveEventId(CrossDeathBoard), CrossDeathLethal.EventId);
	CrossDeathBoard->AdvanceVisualsAtRealTime(42.021);
	TestFalse(TEXT("one large delta drains lethal Hit, inserted Death, and the later Hit"),
		FApi::IsActive(CrossDeathBoard));
	TestEqual(TEXT("one large delta fires both crossed Hit impacts exactly once"),
		FApi::ImpactCount(CrossDeathBoard), 2);
	TestEqual(TEXT("one large delta completes lethal Hit, Death, and later Hit exactly once"),
		FApi::CompletionCount(CrossDeathBoard), 3);
	TestNull(TEXT("the crossed Death still removes only its defeated target visual"),
		CrossDeathBoard->GetUnitVisualForTest(CrossDeathLethal.TargetUnitId));
	TestNotNull(TEXT("the event after crossed Death preserves its surviving target visual"),
		CrossDeathBoard->GetUnitVisualForTest(CrossDeathFollowUp.TargetUnitId));

	const int32 ImpactBeforeLargeDelta = FApi::ImpactCount(Board);
	const int32 CompletionBeforeLargeDelta = FApi::CompletionCount(Board);
	const FGameXXKBattlePresentationEvent LargeDelta = MakePresentationEvent(
		2, TEXT("Player"), false, TEXT("Enemy.Tiger"), true, 70, 60);
	const FGameXXKBattlePresentationEvent Overflow = MakePresentationEvent(
		3, TEXT("Enemy.BlackBear"), true, TEXT("Player"), false, 90, 90, true);
	FApi::Queue(Board, LargeDelta);
	FApi::Queue(Board, Overflow);
	Board->AdvanceVisualsAtRealTime(10.0);
	Board->AdvanceVisualsAtRealTime(10.90);
	TestEqual(TEXT("one large delta still fires the crossed impact only once"),
		FApi::ImpactCount(Board), ImpactBeforeLargeDelta + 1);
	TestEqual(TEXT("one large delta still fires the crossed completion only once"),
		FApi::CompletionCount(Board), CompletionBeforeLargeDelta + 1);
	TestEqual(TEXT("large-delta overflow starts the next immutable event"),
		FApi::ActiveEventId(Board), Overflow.EventId);
	TestTrue(TEXT("large-delta overflow carries exactly zero-point-zero-eight seconds into the next event"),
		FMath::IsNearlyEqual(FApi::ActiveElapsed(Board), 0.08, 0.0001));
	TestTrue(TEXT("the avoided overflow packet owns the zero-point-four-five duration"),
		FMath::IsNearlyEqual(FApi::ActiveDuration(Board), 0.45, 0.0001));
	Board->AdvanceVisualsAtRealTime(10.90);
	TestEqual(TEXT("repeating the same absolute sample cannot refire impact"),
		FApi::ImpactCount(Board), ImpactBeforeLargeDelta + 1);
	TestEqual(TEXT("repeating the same absolute sample cannot refire completion"),
		FApi::CompletionCount(Board), CompletionBeforeLargeDelta + 1);
	Board->AdvanceVisualsAtRealTime(10.981);
	TestEqual(TEXT("an avoided packet emits the avoid readout when its own marker crosses"),
		FApi::Readout(Board), FString(TEXT("闪避")));
	TestEqual(TEXT("the overflow event owns one distinct impact"),
		FApi::ImpactCount(Board), ImpactBeforeLargeDelta + 2);
	TestEqual(TEXT("an avoided packet never increments HUD shake bookkeeping"),
		FApi::ShakeCount(Board), 2);
	TestTrue(TEXT("an avoided packet publishes zero shake amplitude"),
		FApi::ShakeAmplitude(Board).IsNearlyZero(0.001f));
	TestTrue(TEXT("an avoided packet publishes zero shake duration"),
		FMath::IsNearlyZero(FApi::ShakeDuration(Board), 0.0001));
	TestTrue(TEXT("avoid readout reaches its one-point-one impact peak"),
		FApi::ReadoutScale(Board).Equals(FVector2D(1.10f, 1.10f), 0.001f));
	TestTrue(TEXT("avoid keeps the viewport root motionless"),
		Board->GetBattleViewportRootForTest()
		&& Board->GetBattleViewportRootForTest()->GetRenderTransform().Translation.IsNearlyZero(0.001f));
	Board->AdvanceVisualsAtRealTime(11.271);
	TestFalse(TEXT("the overflow event completes on the inherited ten-point-eight-two epoch"), FApi::IsActive(Board));

	const FGameXXKBattlePresentationEvent Lethal = MakePresentationEvent(
		4, TEXT("Player"), false, TEXT("Enemy.Tiger"), true, 60, 0, false, true);
	const FGameXXKBattleAnimationClipDescriptor DeathClip =
		FGameXXKBattleAnimationPresentation::ResolveClip(
			Lethal.TargetUnitId, Lethal.bTargetEnemy, EGameXXKBattleAnimationAction::Death);
	const FGameXXKBattleAnimationClipDescriptor FittedDeathClip =
		FGameXXKBattleAnimationPresentation::FitClipToDuration(DeathClip, 0.90f);
	const FGameXXKBattlePresentationEvent AfterDeath = MakePresentationEvent(
		5, TEXT("Enemy.BlackBear"), true, TEXT("Player"), false, 90, 82);
	FApi::Queue(Board, Lethal);
	FApi::Queue(Board, AfterDeath);
	Board->AdvanceVisualsAtRealTime(20.0);
	Board->AdvanceVisualsAtRealTime(20.821);
	TestTrue(TEXT("lethal Attack/Hit completion starts Death before the next event"), FApi::IsDeathActive(Board));
	TestTrue(TEXT("active Death owns the zero-point-nine-second rhythm"),
		FMath::IsNearlyEqual(FApi::ActiveDuration(Board), 0.90, 0.0001));
	TestEqual(TEXT("Death keeps the lethal immutable event active"), FApi::ActiveEventId(Board), Lethal.EventId);
	TestEqual(TEXT("the event behind Death remains queued"), FApi::QueueCount(Board), 1);
	TestTrue(TEXT("lethal completion asynchronously requests the actual Death atlas"),
		AtlasLoader->Requested(DeathClip.TexturePath));
	TestEqual(TEXT("the persistent lethal visual binds the actual Death atlas"),
		TargetVisual ? TargetVisual->GetAtlasForTest() : nullptr,
		AtlasLoader->GetTexture(DeathClip.TexturePath));
	TestTrue(TEXT("the real Death visual remains visible, enlarged, and positively scaled"),
		TargetVisual
		&& TargetVisual->GetVisibility() == ESlateVisibility::SelfHitTestInvisible
		&& TargetVisual->GetUnitImageForTest()
		&& TargetVisual->GetUnitImageForTest()->GetVisibility() == ESlateVisibility::SelfHitTestInvisible
		&& TargetVisual->GetPresentedSize().Equals(FVector2D(820.0f, 820.0f), 0.01f)
		&& TargetVisual->GetRenderTransform().Scale.X > 0.0f);
	TestEqual(TEXT("the actual Death clip begins at frame zero"),
		TargetVisual ? TargetVisual->GetCurrentFrameForTest() : INDEX_NONE,
		0);
	TestEqual(TEXT("the lethal visual is not removed before Death presentation"),
		Board->GetUnitVisualForTest(Lethal.TargetUnitId), TargetVisual);
	TestFalse(TEXT("the lethal visual is not marked removed before Death completes"),
		TargetVisual ? TargetVisual->IsRemovedForTest() : true);
	Board->AdvanceVisualsAtRealTime(21.20);
	TestEqual(TEXT("the actual Death visual advances from its absolute start epoch"),
		TargetVisual ? TargetVisual->GetCurrentFrameForTest() : INDEX_NONE,
		FGameXXKBattleAnimationPresentation::CalculateFrameIndex(FittedDeathClip, 0.38f, false));
	Board->AdvanceVisualsAtRealTime(21.719);
	TestEqual(TEXT("the actual Death visual reaches its authored terminal frame before removal"),
		TargetVisual ? TargetVisual->GetCurrentFrameForTest() : INDEX_NONE,
		DeathClip.FrameCount - 1);
	TestEqual(TEXT("the lethal visual remains until the zero-point-nine-second Death boundary"),
		Board->GetUnitVisualForTest(Lethal.TargetUnitId), TargetVisual);
	Board->AdvanceVisualsAtRealTime(21.721);
	TestNull(TEXT("the lethal visual may be removed only after Death completes"),
		Board->GetUnitVisualForTest(Lethal.TargetUnitId));
	TestEqual(TEXT("the event behind Death starts only after Death removal"),
		FApi::ActiveEventId(Board), AfterDeath.EventId);

	UGameInstance* const ColdDeathGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* const ColdDeathSubsystem = NewObject<UGameXXKMVPSubsystem>(ColdDeathGameInstance);
	BuildPresentationFixture(ColdDeathSubsystem);
	UGameXXKBattleBoardWidget* const ColdDeathBoard = NewObject<UGameXXKBattleBoardWidget>();
	ColdDeathBoard->SetMVPSubsystem(ColdDeathSubsystem);
	const TSharedRef<FLateDeathPresentationAtlasLoader> ColdDeathLoader =
		MakeShared<FLateDeathPresentationAtlasLoader>();
	ColdDeathBoard->SetAtlasCacheForTest(MakeUnique<FGameXXKBattleAtlasCache>(
		ColdDeathLoader,
		[]() { return 80.0; }));
	TestTrue(TEXT("cold-Death fixture initializes its real Board"), ColdDeathBoard->Initialize());
	ColdDeathBoard->NativeConstruct();
	TestTrue(TEXT("cold-Death fixture begins a visual session"),
		ColdDeathBoard->BeginBattleVisualSession(1002));

	const FGameXXKBattlePresentationEvent ColdDeathEvent = MakePresentationEvent(
		6, TEXT("Player"), false, TEXT("Enemy.Tiger"), true, 70, 0, false, true);
	const FGameXXKBattlePresentationEvent AfterColdDeathEvent = MakePresentationEvent(
		7, TEXT("Enemy.BlackBear"), true, TEXT("Player"), false, 90, 81);
	const FGameXXKBattleAnimationClipDescriptor ColdDeathClip =
		FGameXXKBattleAnimationPresentation::ResolveClip(
			ColdDeathEvent.TargetUnitId,
			ColdDeathEvent.bTargetEnemy,
			EGameXXKBattleAnimationAction::Death);
	const FGameXXKBattleAnimationClipDescriptor FittedColdDeathClip =
		FGameXXKBattleAnimationPresentation::FitClipToDuration(ColdDeathClip, 0.90f);
	const FGameXXKBattleAnimationClipDescriptor ColdDeathIdleClip =
		FGameXXKBattleAnimationPresentation::ResolveClip(
			ColdDeathEvent.TargetUnitId,
			ColdDeathEvent.bTargetEnemy,
			EGameXXKBattleAnimationAction::Idle);
	UGameXXKBattleUnitVisualWidget* const ColdDeathTarget =
		ColdDeathBoard->GetUnitVisualForTest(ColdDeathEvent.TargetUnitId);
	FApi::Queue(ColdDeathBoard, ColdDeathEvent);
	FApi::Queue(ColdDeathBoard, AfterColdDeathEvent);
	ColdDeathBoard->AdvanceVisualsAtRealTime(80.0);
	TestFalse(TEXT("Death atlas is not prefetched before the lethal Attack/Hit completes"),
		ColdDeathLoader->Requested(ColdDeathClip.TexturePath));
	ColdDeathBoard->AdvanceVisualsAtRealTime(80.821);
	TestTrue(TEXT("cold Death starts at the lethal Attack/Hit completion boundary"),
		FApi::IsDeathActive(ColdDeathBoard));
	TestTrue(TEXT("cold Death asynchronously requests its atlas only when enqueued"),
		ColdDeathLoader->Requested(ColdDeathClip.TexturePath));
	TestEqual(TEXT("pending cold Death keeps the persistent target on its Idle atlas"),
		ColdDeathTarget ? ColdDeathTarget->GetAtlasForTest() : nullptr,
		ColdDeathLoader->GetTexture(ColdDeathIdleClip.TexturePath));
	ColdDeathBoard->AdvanceVisualsAtRealTime(81.20);
	TestTrue(TEXT("cold Death request completes while that exact Death entry is active"),
		ColdDeathLoader->CompleteDeath(ColdDeathClip.TexturePath));
	TestEqual(TEXT("late Death completion reuses the exact persistent target visual"),
		ColdDeathBoard->GetUnitVisualForTest(ColdDeathEvent.TargetUnitId),
		ColdDeathTarget);
	TestEqual(TEXT("late Death completion binds the actual Death atlas"),
		ColdDeathTarget ? ColdDeathTarget->GetAtlasForTest() : nullptr,
		ColdDeathLoader->GetTexture(ColdDeathClip.TexturePath));
	TestTrue(TEXT("late Death remains visible at the 820 square cinematic placement with positive scale"),
		ColdDeathTarget
		&& ColdDeathTarget->GetVisibility() == ESlateVisibility::SelfHitTestInvisible
		&& ColdDeathTarget->GetUnitImageForTest()
		&& ColdDeathTarget->GetUnitImageForTest()->GetVisibility() == ESlateVisibility::SelfHitTestInvisible
		&& ColdDeathTarget->GetPresentedSize().Equals(FVector2D(820.0f, 820.0f), 0.01f)
		&& ColdDeathTarget->GetRenderTransform().Scale.X > 0.0f);
	TestEqual(TEXT("late Death catches up from its original start epoch rather than load time"),
		ColdDeathTarget ? ColdDeathTarget->GetCurrentFrameForTest() : INDEX_NONE,
		FGameXXKBattleAnimationPresentation::CalculateFrameIndex(FittedColdDeathClip, 0.38f, false));
	ColdDeathBoard->AdvanceVisualsAtRealTime(81.719);
	TestEqual(TEXT("late Death remains non-looping and reaches its authored terminal frame"),
		ColdDeathTarget ? ColdDeathTarget->GetCurrentFrameForTest() : INDEX_NONE,
		ColdDeathClip.FrameCount - 1);
	TestEqual(TEXT("late-loaded Death cannot remove its persistent target before completion"),
		ColdDeathBoard->GetUnitVisualForTest(ColdDeathEvent.TargetUnitId),
		ColdDeathTarget);
	ColdDeathBoard->AdvanceVisualsAtRealTime(81.721);
	TestNull(TEXT("late-loaded Death removes its persistent target only at completion"),
		ColdDeathBoard->GetUnitVisualForTest(ColdDeathEvent.TargetUnitId));
	TestEqual(TEXT("the event behind late-loaded Death starts only after removal"),
		FApi::ActiveEventId(ColdDeathBoard),
		AfterColdDeathEvent.EventId);

	UGameInstance* const StaleDeathGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* const StaleDeathSubsystem = NewObject<UGameXXKMVPSubsystem>(StaleDeathGameInstance);
	BuildPresentationFixture(StaleDeathSubsystem);
	UGameXXKBattleBoardWidget* const StaleDeathBoard = NewObject<UGameXXKBattleBoardWidget>();
	StaleDeathBoard->SetMVPSubsystem(StaleDeathSubsystem);
	const TSharedRef<FLateDeathPresentationAtlasLoader> StaleDeathLoader =
		MakeShared<FLateDeathPresentationAtlasLoader>();
	StaleDeathBoard->SetAtlasCacheForTest(MakeUnique<FGameXXKBattleAtlasCache>(
		StaleDeathLoader,
		[]() { return 90.0; }));
	TestTrue(TEXT("stale-Death fixture initializes its real Board"), StaleDeathBoard->Initialize());
	StaleDeathBoard->NativeConstruct();
	TestTrue(TEXT("stale-Death fixture begins a visual session"),
		StaleDeathBoard->BeginBattleVisualSession(1003));
	const FGameXXKBattlePresentationEvent StaleDeathEvent = MakePresentationEvent(
		8, TEXT("Enemy.BlackBear"), true, TEXT("Player"), false, 90, 0, false, true);
	const FGameXXKBattlePresentationEvent AfterStaleDeathEvent = MakePresentationEvent(
		9, TEXT("Enemy.BlackBear"), true, TEXT("Enemy.Tiger"), true, 70, 61);
	const FGameXXKBattleAnimationClipDescriptor StaleDeathClip =
		FGameXXKBattleAnimationPresentation::ResolveClip(
			StaleDeathEvent.TargetUnitId,
			StaleDeathEvent.bTargetEnemy,
			EGameXXKBattleAnimationAction::Death);
	FApi::Queue(StaleDeathBoard, StaleDeathEvent);
	FApi::Queue(StaleDeathBoard, AfterStaleDeathEvent);
	StaleDeathBoard->AdvanceVisualsAtRealTime(90.0);
	StaleDeathBoard->AdvanceVisualsAtRealTime(90.821);
	TestTrue(TEXT("stale-Death fixture leaves its cold Death request pending"),
		FApi::IsDeathActive(StaleDeathBoard)
		&& StaleDeathLoader->Requested(StaleDeathClip.TexturePath));
	StaleDeathBoard->AdvanceVisualsAtRealTime(91.721);
	UGameXXKBattleUnitVisualWidget* const AfterStaleDeathTarget =
		StaleDeathBoard->GetUnitVisualForTest(AfterStaleDeathEvent.TargetUnitId);
	UTexture2D* const AfterStaleDeathAtlas =
		AfterStaleDeathTarget ? AfterStaleDeathTarget->GetAtlasForTest() : nullptr;
	TestEqual(TEXT("stale-Death fixture advances to the later queue entry"),
		FApi::ActiveEventId(StaleDeathBoard),
		AfterStaleDeathEvent.EventId);
	TestTrue(TEXT("the completed Death request can still report late through the cache"),
		StaleDeathLoader->CompleteDeath(StaleDeathClip.TexturePath));
	TestEqual(TEXT("a stale Death callback cannot pollute the later event atlas"),
		AfterStaleDeathTarget ? AfterStaleDeathTarget->GetAtlasForTest() : nullptr,
		AfterStaleDeathAtlas);
	TestEqual(TEXT("a stale Death callback cannot switch the later event identity"),
		FApi::ActiveEventId(StaleDeathBoard),
		AfterStaleDeathEvent.EventId);

	UGameInstance* const LateIdleGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* const LateIdleSubsystem = NewObject<UGameXXKMVPSubsystem>(LateIdleGameInstance);
	BuildPresentationFixture(LateIdleSubsystem);
	UGameXXKBattleBoardWidget* const LateIdleBoard = NewObject<UGameXXKBattleBoardWidget>();
	LateIdleBoard->SetMVPSubsystem(LateIdleSubsystem);
	const TSharedRef<FLateIdlePresentationAtlasLoader> LateIdleLoader =
		MakeShared<FLateIdlePresentationAtlasLoader>();
	LateIdleBoard->SetAtlasCacheForTest(MakeUnique<FGameXXKBattleAtlasCache>(
		LateIdleLoader,
		[]() { return 30.0; }));
	TestTrue(TEXT("late-idle race fixture initializes its real Board"), LateIdleBoard->Initialize());
	LateIdleBoard->NativeConstruct();
	TestTrue(TEXT("late-idle race fixture begins a visual session"),
		LateIdleBoard->BeginBattleVisualSession(1002));

	const FGameXXKBattlePresentationEvent LateIdleEvent = MakePresentationEvent(
		6, TEXT("Player"), false, TEXT("Enemy.Tiger"), true, 70, 55);
	FApi::Queue(LateIdleBoard, LateIdleEvent);
	LateIdleBoard->AdvanceVisualsAtRealTime(30.0);
	UGameXXKBattleUnitVisualWidget* const LateIdleAttacker =
		LateIdleBoard->GetUnitVisualForTest(LateIdleEvent.AttackerUnitId);
	const FGameXXKBattleAnimationClipDescriptor LateIdleAttackClip =
		FGameXXKBattleAnimationPresentation::ResolveClip(
		LateIdleEvent.AttackerUnitId,
		LateIdleEvent.bAttackerEnemy,
		EGameXXKBattleAnimationAction::Attack);
	const FGameXXKBattleAnimationClipDescriptor FittedLateIdleAttackClip =
		FGameXXKBattleAnimationPresentation::FitClipToDuration(LateIdleAttackClip, 0.82f);
	const FSoftObjectPath AttackPath = LateIdleAttackClip.TexturePath;
	const FSoftObjectPath IdlePath = FGameXXKBattleAnimationPresentation::ResolveClip(
		LateIdleEvent.AttackerUnitId,
		LateIdleEvent.bAttackerEnemy,
		EGameXXKBattleAnimationAction::Idle).TexturePath;
	UTexture2D* const ActiveAttackAtlas = LateIdleLoader->GetTexture(AttackPath);
	TestNotNull(TEXT("race fixture asynchronously delivers the Attack atlas before start"), ActiveAttackAtlas);
	TestEqual(TEXT("race fixture starts with the delivered Attack atlas"),
		LateIdleAttacker ? LateIdleAttacker->GetAtlasForTest() : nullptr,
		ActiveAttackAtlas);
	TestTrue(TEXT("race fixture releases the delayed Idle callback after action start"),
		LateIdleLoader->CompleteIdle(IdlePath));
	TestNotEqual(TEXT("the delayed Idle callback owns a distinct texture"),
		LateIdleLoader->GetTexture(IdlePath),
		ActiveAttackAtlas);
	TestEqual(TEXT("a late Idle callback cannot overwrite the active participant texture"),
		LateIdleAttacker ? LateIdleAttacker->GetAtlasForTest() : nullptr,
		ActiveAttackAtlas);
	TestEqual(TEXT("a late Idle callback cannot replace the active participant clip"),
		FApi::AttackerRate(LateIdleBoard),
		FittedLateIdleAttackClip.PlaybackRate);

#if 0 // The generic Impact visual and all of its delayed-load/lifetime behavior were intentionally retired.
	UGameInstance* const LateImpactGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* const LateImpactSubsystem = NewObject<UGameXXKMVPSubsystem>(LateImpactGameInstance);
	BuildPresentationFixture(LateImpactSubsystem);
	UGameXXKBattleBoardWidget* const LateImpactBoard = NewObject<UGameXXKBattleBoardWidget>();
	LateImpactBoard->SetMVPSubsystem(LateImpactSubsystem);
	const TSharedRef<FLateImpactPresentationAtlasLoader> LateImpactLoader =
		MakeShared<FLateImpactPresentationAtlasLoader>();
	LateImpactBoard->SetAtlasCacheForTest(MakeUnique<FGameXXKBattleAtlasCache>(
		LateImpactLoader,
		[]() { return 40.0; }));
	TestTrue(TEXT("late-impact fixture initializes its real Board"), LateImpactBoard->Initialize());
	LateImpactBoard->NativeConstruct();
	TestTrue(TEXT("late-impact fixture begins a visual session"),
		LateImpactBoard->BeginBattleVisualSession(1003));
	UGameXXKBattleUnitVisualWidget* const LateImpactVisual = LateImpactBoard->WidgetTree
		? Cast<UGameXXKBattleUnitVisualWidget>(LateImpactBoard->WidgetTree->FindWidget(TEXT("BattleCinematicImpact")))
		: nullptr;
	TestNotNull(TEXT("late-impact fixture owns the real generic Impact widget"), LateImpactVisual);

	const FGameXXKBattlePresentationEvent LateImpactEvent = MakePresentationEvent(
		7, TEXT("Player"), false, TEXT("Enemy.Tiger"), true, 70, 51);
	const FGameXXKBattleAnimationClipDescriptor GenericImpactClip =
		FGameXXKBattleAnimationPresentation::ResolveGenericClip(EGameXXKBattleAnimationAction::Impact);
	FApi::Queue(LateImpactBoard, LateImpactEvent);
	LateImpactBoard->AdvanceVisualsAtRealTime(40.0);
	LateImpactBoard->AdvanceVisualsAtRealTime(41.101);
	TestEqual(TEXT("a pending Impact does not block the exact-once health marker"),
		FApi::DisplayedHealth(LateImpactBoard, LateImpactEvent.TargetUnitId),
		LateImpactEvent.TargetHealthAfter);
	TestEqual(TEXT("a pending Impact still fires the marker exactly once"), FApi::ImpactCount(LateImpactBoard), 1);
	TestEqual(TEXT("a pending Impact still fires shake exactly once"), FApi::ShakeCount(LateImpactBoard), 1);
	TestEqual(TEXT("a pending Impact still publishes the damage readout"),
		FApi::Readout(LateImpactBoard), FString(TEXT("-19")));
	TestNull(TEXT("the generic Impact has no atlas before its delayed completion"),
		LateImpactVisual ? LateImpactVisual->GetAtlasForTest() : nullptr);
	TestTrue(TEXT("the generic Impact image is not visibly rendered while its atlas is pending"),
		LateImpactVisual
		&& (!LateImpactVisual->GetUnitImageForTest()
			|| LateImpactVisual->GetUnitImageForTest()->GetVisibility() == ESlateVisibility::Hidden));
	const double LateImpactMarkerAbsoluteSeconds = 41.1;
	const double LateImpactLoadAbsoluteSeconds = 41.601;
	LateImpactBoard->AdvanceVisualsAtRealTime(LateImpactLoadAbsoluteSeconds);
	TestTrue(TEXT("the late generic Impact request completes while its event is active"),
		LateImpactLoader->CompleteImpact(GenericImpactClip.TexturePath));
	TestEqual(TEXT("late Impact completion binds its atlas to the still-active widget"),
		LateImpactVisual ? LateImpactVisual->GetAtlasForTest() : nullptr,
		LateImpactLoader->GetTexture(GenericImpactClip.TexturePath));
	TestEqual(TEXT("late Impact completion makes the already-fired Impact image visible"),
		LateImpactVisual && LateImpactVisual->GetUnitImageForTest()
			? LateImpactVisual->GetUnitImageForTest()->GetVisibility()
			: ESlateVisibility::Hidden,
		ESlateVisibility::SelfHitTestInvisible);
	TestEqual(TEXT("late Impact immediately catches up from the original absolute marker epoch"),
		LateImpactVisual ? LateImpactVisual->GetCurrentFrameForTest() : INDEX_NONE,
		FGameXXKBattleAnimationPresentation::CalculateFrameIndex(
			GenericImpactClip,
			static_cast<float>(LateImpactLoadAbsoluteSeconds - LateImpactMarkerAbsoluteSeconds),
			false));
	TestEqual(TEXT("late Impact completion cannot refire health or marker bookkeeping"),
		FApi::ImpactCount(LateImpactBoard), 1);
	TestEqual(TEXT("late Impact completion cannot restart shake"), FApi::ShakeCount(LateImpactBoard), 1);
	TestEqual(TEXT("late Impact completion preserves the original readout"),
		FApi::Readout(LateImpactBoard), FString(TEXT("-19")));
	const double LateImpactFollowUpAbsoluteSeconds = 41.801;
	LateImpactBoard->AdvanceVisualsAtRealTime(LateImpactFollowUpAbsoluteSeconds);
	TestEqual(TEXT("late Impact continues four-times playback from the unchanged marker epoch"),
		LateImpactVisual ? LateImpactVisual->GetCurrentFrameForTest() : INDEX_NONE,
		FGameXXKBattleAnimationPresentation::CalculateFrameIndex(
			GenericImpactClip,
			static_cast<float>(LateImpactFollowUpAbsoluteSeconds - LateImpactMarkerAbsoluteSeconds),
			false));

	UGameInstance* const SwitchedImpactGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* const SwitchedImpactSubsystem = NewObject<UGameXXKMVPSubsystem>(SwitchedImpactGameInstance);
	BuildPresentationFixture(SwitchedImpactSubsystem);
	UGameXXKBattleBoardWidget* const SwitchedImpactBoard = NewObject<UGameXXKBattleBoardWidget>();
	SwitchedImpactBoard->SetMVPSubsystem(SwitchedImpactSubsystem);
	const TSharedRef<FLateImpactPresentationAtlasLoader> SwitchedImpactLoader =
		MakeShared<FLateImpactPresentationAtlasLoader>();
	SwitchedImpactBoard->SetAtlasCacheForTest(MakeUnique<FGameXXKBattleAtlasCache>(
		SwitchedImpactLoader,
		[]() { return 50.0; }));
	TestTrue(TEXT("switched-impact fixture initializes its real Board"), SwitchedImpactBoard->Initialize());
	SwitchedImpactBoard->NativeConstruct();
	TestTrue(TEXT("switched-impact fixture begins a visual session"),
		SwitchedImpactBoard->BeginBattleVisualSession(1004));
	UGameXXKBattleUnitVisualWidget* const SwitchedImpactVisual = SwitchedImpactBoard->WidgetTree
		? Cast<UGameXXKBattleUnitVisualWidget>(SwitchedImpactBoard->WidgetTree->FindWidget(TEXT("BattleCinematicImpact")))
		: nullptr;
	const FGameXXKBattlePresentationEvent SupersededImpactEvent = MakePresentationEvent(
		8, TEXT("Player"), false, TEXT("Enemy.Tiger"), true, 70, 60);
	const FGameXXKBattlePresentationEvent CurrentImpactEvent = MakePresentationEvent(
		9, TEXT("Enemy.BlackBear"), true, TEXT("Player"), false, 90, 81);
	FApi::Queue(SwitchedImpactBoard, SupersededImpactEvent);
	FApi::Queue(SwitchedImpactBoard, CurrentImpactEvent);
	SwitchedImpactBoard->AdvanceVisualsAtRealTime(50.0);
	SwitchedImpactBoard->AdvanceVisualsAtRealTime(52.5);
	TestEqual(TEXT("large completion switches to the next event before the late Impact arrives"),
		FApi::ActiveEventId(SwitchedImpactBoard), CurrentImpactEvent.EventId);
	TestTrue(TEXT("the coalesced late Impact load completes after the event switch"),
		SwitchedImpactLoader->CompleteImpact(GenericImpactClip.TexturePath));
	TestNull(TEXT("a stale late Impact callback cannot bind into the pre-marker current event"),
		SwitchedImpactVisual ? SwitchedImpactVisual->GetAtlasForTest() : nullptr);
	TestEqual(TEXT("a stale late Impact callback cannot reveal the current event before its marker"),
		SwitchedImpactVisual ? SwitchedImpactVisual->GetVisibility() : ESlateVisibility::Visible,
		ESlateVisibility::Hidden);
	TestEqual(TEXT("late completion after an event switch cannot refire the prior marker"),
		FApi::ImpactCount(SwitchedImpactBoard), 1);
	SwitchedImpactBoard->AdvanceVisualsAtRealTime(53.601);
	TestEqual(TEXT("the current event may bind its legitimately prefetched Impact at its own marker"),
		SwitchedImpactVisual ? SwitchedImpactVisual->GetAtlasForTest() : nullptr,
		SwitchedImpactLoader->GetTexture(GenericImpactClip.TexturePath));
	TestEqual(TEXT("the current event owns exactly one later Impact marker"),
		FApi::ImpactCount(SwitchedImpactBoard), 2);

	UGameInstance* const ImpactLifetimeGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* const ImpactLifetimeSubsystem = NewObject<UGameXXKMVPSubsystem>(ImpactLifetimeGameInstance);
	BuildPresentationFixture(ImpactLifetimeSubsystem);
	UGameXXKBattleBoardWidget* const ImpactLifetimeBoard = NewObject<UGameXXKBattleBoardWidget>();
	ImpactLifetimeBoard->SetMVPSubsystem(ImpactLifetimeSubsystem);
	const TSharedRef<FPresentationAtlasLoader> ImpactLifetimeLoader = MakeShared<FPresentationAtlasLoader>();
	TUniquePtr<FGameXXKBattleAtlasCache> ImpactLifetimeCache = MakeUnique<FGameXXKBattleAtlasCache>(
		ImpactLifetimeLoader,
		[]() { return 100.0; },
		24);
	FGameXXKBattleAtlasCache* const ImpactLifetimeCachePtr = ImpactLifetimeCache.Get();
	ImpactLifetimeBoard->SetAtlasCacheForTest(MoveTemp(ImpactLifetimeCache));
	TestTrue(TEXT("Impact lifetime fixture initializes its real Board"), ImpactLifetimeBoard->Initialize());
	ImpactLifetimeBoard->NativeConstruct();
	TestTrue(TEXT("Impact lifetime fixture begins a visual session"),
		ImpactLifetimeBoard->BeginBattleVisualSession(1009));
	UGameXXKBattleUnitVisualWidget* const ImpactLifetimeVisual = ImpactLifetimeBoard->WidgetTree
		? Cast<UGameXXKBattleUnitVisualWidget>(
			ImpactLifetimeBoard->WidgetTree->FindWidget(TEXT("BattleCinematicImpact")))
		: nullptr;
	TestNotNull(TEXT("Impact lifetime fixture owns the persistent generic Impact widget"),
		ImpactLifetimeVisual);
	TestEqual(TEXT("three pinned Idle atlases establish the cache accounting baseline"),
		ImpactLifetimeCachePtr->GetStats().ResidentBytes,
		12ll);

	const FGameXXKBattlePresentationEvent FirstImpactLifetimeEvent = MakePresentationEvent(
		10, TEXT("Player"), false, TEXT("Enemy.Tiger"), true, 70, 52);
	const FGameXXKBattleAnimationClipDescriptor ImpactLifetimeClip =
		FGameXXKBattleAnimationPresentation::ResolveGenericClip(EGameXXKBattleAnimationAction::Impact);
	FApi::Queue(ImpactLifetimeBoard, FirstImpactLifetimeEvent);
	ImpactLifetimeBoard->AdvanceVisualsAtRealTime(100.0);
	ImpactLifetimeBoard->AdvanceVisualsAtRealTime(101.101);
	UTexture2D* const FirstImpactLifetimeAtlas =
		ImpactLifetimeVisual ? ImpactLifetimeVisual->GetAtlasForTest() : nullptr;
	TestNotNull(TEXT("the first Impact marker binds its cache-resident atlas"), FirstImpactLifetimeAtlas);
	TestEqual(TEXT("Idle and presentation residents exactly fill the bounded cache"),
		ImpactLifetimeCachePtr->GetStats().ResidentBytes,
		24ll);
	ImpactLifetimeBoard->AdvanceVisualsAtRealTime(102.5);
	TestEqual(TEXT("presentation completion hides the persistent Impact widget"),
		ImpactLifetimeVisual ? ImpactLifetimeVisual->GetVisibility() : ESlateVisibility::Visible,
		ESlateVisibility::Hidden);
	TestNull(TEXT("presentation completion releases the hidden Impact widget atlas"),
		ImpactLifetimeVisual ? ImpactLifetimeVisual->GetAtlasForTest() : nullptr);

	const TArray<FSoftObjectPath> PostImpactPaths = {
		FSoftObjectPath(TEXT("/Game/Test/T_PostImpactA.T_PostImpactA")),
		FSoftObjectPath(TEXT("/Game/Test/T_PostImpactB.T_PostImpactB")),
		FSoftObjectPath(TEXT("/Game/Test/T_PostImpactC.T_PostImpactC"))};
	for (const FSoftObjectPath& PostImpactPath : PostImpactPaths)
	{
		EGameXXKAtlasLoadResult LoadResult = EGameXXKAtlasLoadResult::Missing;
		ImpactLifetimeCachePtr->Acquire(
			PostImpactPath,
			1009,
			[&LoadResult](UTexture2D*, const EGameXXKAtlasLoadResult Result)
			{
				LoadResult = Result;
			});
		TestEqual(TEXT("completion unpins each presentation resident for bounded-cache eviction"),
			LoadResult,
			EGameXXKAtlasLoadResult::Loaded);
	}
	TestEqual(TEXT("post-completion evictions keep cache accounting at the exact budget"),
		ImpactLifetimeCachePtr->GetStats().ResidentBytes,
		24ll);

	const FGameXXKBattlePresentationEvent SecondImpactLifetimeEvent = MakePresentationEvent(
		11, TEXT("Player"), false, TEXT("Enemy.Tiger"), true, 52, 41);
	FApi::Queue(ImpactLifetimeBoard, SecondImpactLifetimeEvent);
	ImpactLifetimeBoard->AdvanceVisualsAtRealTime(110.0);
	ImpactLifetimeBoard->AdvanceVisualsAtRealTime(111.101);
	TestEqual(TEXT("the later event reloads the evicted generic Impact path"),
		ImpactLifetimeLoader->RequestCount(ImpactLifetimeClip.TexturePath),
		2);
	TestNotEqual(TEXT("the later event binds a fresh Impact texture"),
		ImpactLifetimeVisual ? ImpactLifetimeVisual->GetAtlasForTest() : nullptr,
		FirstImpactLifetimeAtlas);
	TestEqual(TEXT("the later event binds the freshly cache-admitted Impact texture"),
		ImpactLifetimeVisual ? ImpactLifetimeVisual->GetAtlasForTest() : nullptr,
		ImpactLifetimeLoader->GetTexture(ImpactLifetimeClip.TexturePath));
	ImpactLifetimeBoard->AdvanceVisualsAtRealTime(112.5);
	TestNull(TEXT("the later event also releases its Impact atlas on completion"),
		ImpactLifetimeVisual ? ImpactLifetimeVisual->GetAtlasForTest() : nullptr);

#endif

	UGameInstance* const IdleFailureGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* const IdleFailureSubsystem = NewObject<UGameXXKMVPSubsystem>(IdleFailureGameInstance);
	BuildPresentationFixture(IdleFailureSubsystem);
	UGameXXKBattleBoardWidget* const IdleFailureBoard = NewObject<UGameXXKBattleBoardWidget>();
	IdleFailureBoard->SetMVPSubsystem(IdleFailureSubsystem);
	const TSharedRef<FLateIdlePresentationAtlasLoader> IdleFailureLoader =
		MakeShared<FLateIdlePresentationAtlasLoader>();
	IdleFailureBoard->SetAtlasCacheForTest(MakeUnique<FGameXXKBattleAtlasCache>(
		IdleFailureLoader,
		[]() { return 60.0; },
		FGameXXKBattleAtlasCache::DefaultResidentBudgetBytes,
		0.1));
	TestTrue(TEXT("late-idle failure fixture initializes its real Board"), IdleFailureBoard->Initialize());
	IdleFailureBoard->NativeConstruct();
	TestTrue(TEXT("late-idle failure fixture begins a visual session"),
		IdleFailureBoard->BeginBattleVisualSession(1005));
	const FGameXXKBattlePresentationEvent IdleFailureEvent = MakePresentationEvent(
		10, TEXT("Player"), false, TEXT("Enemy.Tiger"), true, 70, 54);
	FApi::Queue(IdleFailureBoard, IdleFailureEvent);
	IdleFailureBoard->AdvanceVisualsAtRealTime(60.0);
	UGameXXKBattleUnitVisualWidget* const MissingIdleAttacker =
		IdleFailureBoard->GetUnitVisualForTest(IdleFailureEvent.AttackerUnitId);
	UGameXXKBattleUnitVisualWidget* const TimedOutIdleTarget =
		IdleFailureBoard->GetUnitVisualForTest(IdleFailureEvent.TargetUnitId);
	const FSoftObjectPath MissingIdlePath = FGameXXKBattleAnimationPresentation::ResolveClip(
		IdleFailureEvent.AttackerUnitId,
		IdleFailureEvent.bAttackerEnemy,
		EGameXXKBattleAnimationAction::Idle).TexturePath;
	const FSoftObjectPath TimedOutIdlePath = FGameXXKBattleAnimationPresentation::ResolveClip(
		IdleFailureEvent.TargetUnitId,
		IdleFailureEvent.bTargetEnemy,
		EGameXXKBattleAnimationAction::Idle).TexturePath;
	const FGameXXKBattleAnimationClipDescriptor FailureAttackClip =
		FGameXXKBattleAnimationPresentation::ResolveClip(
		IdleFailureEvent.AttackerUnitId,
		IdleFailureEvent.bAttackerEnemy,
		EGameXXKBattleAnimationAction::Attack);
	const FGameXXKBattleAnimationClipDescriptor FailureHitClip =
		FGameXXKBattleAnimationPresentation::ResolveClip(
		IdleFailureEvent.TargetUnitId,
		IdleFailureEvent.bTargetEnemy,
		EGameXXKBattleAnimationAction::Hit);
	const FGameXXKBattleAnimationClipDescriptor FittedFailureAttackClip =
		FGameXXKBattleAnimationPresentation::FitClipToDuration(FailureAttackClip, 0.82f);
	const FGameXXKBattleAnimationClipDescriptor FittedFailureHitClip =
		FGameXXKBattleAnimationPresentation::FitClipToDuration(FailureHitClip, 0.82f);
	const FSoftObjectPath FailureAttackPath = FailureAttackClip.TexturePath;
	const FSoftObjectPath FailureHitPath = FailureHitClip.TexturePath;
	UTexture2D* const FailureAttackAtlas = IdleFailureLoader->GetTexture(FailureAttackPath);
	UTexture2D* const FailureHitAtlas = IdleFailureLoader->GetTexture(FailureHitPath);
	TestEqual(TEXT("Missing fixture begins with the real Attack atlas"),
		MissingIdleAttacker ? MissingIdleAttacker->GetAtlasForTest() : nullptr,
		FailureAttackAtlas);
	TestEqual(TEXT("TimedOut fixture begins with the real Hit atlas"),
		TimedOutIdleTarget ? TimedOutIdleTarget->GetAtlasForTest() : nullptr,
		FailureHitAtlas);
	TestTrue(TEXT("the delayed attacker Idle completes as Missing during Attack"),
		IdleFailureLoader->CompleteIdleMissing(MissingIdlePath));
	TestEqual(TEXT("a late Missing Idle cannot clear the active Attack atlas"),
		MissingIdleAttacker ? MissingIdleAttacker->GetAtlasForTest() : nullptr,
		FailureAttackAtlas);
	TestEqual(TEXT("a late Missing Idle cannot hide the active attacker widget"),
		MissingIdleAttacker ? MissingIdleAttacker->GetVisibility() : ESlateVisibility::Hidden,
		ESlateVisibility::SelfHitTestInvisible);
	TestEqual(TEXT("a late Missing Idle cannot replace the active Attack clip"),
		FApi::AttackerRate(IdleFailureBoard), FittedFailureAttackClip.PlaybackRate);
	TestEqual(TEXT("a late Missing Idle preserves the active Attack frame"),
		MissingIdleAttacker ? MissingIdleAttacker->GetCurrentFrameForTest() : INDEX_NONE,
		0);
	IdleFailureBoard->AdvanceVisualsAtRealTime(60.101);
	TestEqual(TEXT("a late TimedOut Idle cannot clear the active Hit atlas"),
		TimedOutIdleTarget ? TimedOutIdleTarget->GetAtlasForTest() : nullptr,
		FailureHitAtlas);
	TestEqual(TEXT("a late TimedOut Idle cannot hide the active target widget"),
		TimedOutIdleTarget ? TimedOutIdleTarget->GetVisibility() : ESlateVisibility::Hidden,
		ESlateVisibility::SelfHitTestInvisible);
	TestEqual(TEXT("a late TimedOut Idle cannot replace the active Hit clip"),
		FApi::TargetRate(IdleFailureBoard), FittedFailureHitClip.PlaybackRate);
	TestEqual(TEXT("a late TimedOut Idle keeps advancing the active Hit frames"),
		TimedOutIdleTarget ? TimedOutIdleTarget->GetCurrentFrameForTest() : INDEX_NONE,
		FGameXXKBattleAnimationPresentation::CalculateFrameIndex(
			FittedFailureHitClip,
			0.101f,
			false));

	UGameInstance* const FormationFailureGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* const FormationFailureSubsystem = NewObject<UGameXXKMVPSubsystem>(FormationFailureGameInstance);
	BuildPresentationFixture(FormationFailureSubsystem);
	UGameXXKBattleBoardWidget* const FormationFailureBoard = NewObject<UGameXXKBattleBoardWidget>();
	FormationFailureBoard->SetMVPSubsystem(FormationFailureSubsystem);
	const TSharedRef<FLateIdlePresentationAtlasLoader> FormationFailureLoader =
		MakeShared<FLateIdlePresentationAtlasLoader>();
	FormationFailureBoard->SetAtlasCacheForTest(MakeUnique<FGameXXKBattleAtlasCache>(
		FormationFailureLoader,
		[]() { return 70.0; }));
	TestTrue(TEXT("formation Idle failure fixture initializes its real Board"), FormationFailureBoard->Initialize());
	FormationFailureBoard->NativeConstruct();
	TestTrue(TEXT("formation Idle failure fixture begins a visual session"),
		FormationFailureBoard->BeginBattleVisualSession(1006));
	const FName FormationFailureUnitId(TEXT("Enemy.Tiger"));
	const FSoftObjectPath FormationFailureIdlePath = FGameXXKBattleAnimationPresentation::ResolveClip(
		FormationFailureUnitId,
		true,
		EGameXXKBattleAnimationAction::Idle).TexturePath;
	UGameXXKBattleUnitVisualWidget* const FormationFailureVisual =
		FormationFailureBoard->GetUnitVisualForTest(FormationFailureUnitId);
	if (FormationFailureVisual)
	{
		FormationFailureVisual->SetAtlas(FormationFailureLoader->GetTexture(FormationFailureIdlePath));
	}
	TestNotNull(TEXT("formation fixture begins with an Idle atlas awaiting terminal result"),
		FormationFailureVisual ? FormationFailureVisual->GetAtlasForTest() : nullptr);
	TestTrue(TEXT("the formation Idle completes as Missing outside presentation"),
		FormationFailureLoader->CompleteIdleMissing(FormationFailureIdlePath));
	TestNull(TEXT("a Missing Idle still clears the ordinary formation atlas"),
		FormationFailureVisual ? FormationFailureVisual->GetAtlasForTest() : nullptr);
	TestTrue(TEXT("a Missing Idle still reveals the ordinary formation placeholder"),
		FormationFailureBoard->IsUnitTargetPlaceholderVisibleForTest(FormationFailureUnitId));

	return true;
}

#endif
