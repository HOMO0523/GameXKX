#include "Misc/AutomationTest.h"

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
			Completion(Texture, 4);
			return MakeShared<FPresentationAtlasLoadHandle>();
		}

		bool Requested(const FSoftObjectPath& Path) const
		{
			return RequestedPaths.Contains(Path);
		}

		TArray<FSoftObjectPath> RequestedPaths;
		TArray<TStrongObjectPtr<UTexture2D>> LoadedTextures;
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

		TArray<TStrongObjectPtr<UTexture2D>> LoadedTextures;
		TMap<FSoftObjectPath, UTexture2D*> TexturesByPath;
		TMap<FSoftObjectPath, FGameXXKAtlasLoaderCompletion> PendingIdleCompletions;
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
		const bool bTargetDefeated = false)
	{
		FGameXXKBattlePresentationEvent Event;
		Event.EventId = EventId;
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
		static int32 ImpactCount(const TBoard*) { return INDEX_NONE; }
		static int32 CompletionCount(const TBoard*) { return INDEX_NONE; }
		static int32 ShakeCount(const TBoard*) { return INDEX_NONE; }
		static int32 DisplayedHealth(const TBoard*, FName) { return INDEX_NONE; }
		static float AttackerRate(const TBoard*) { return 0.0f; }
		static float TargetRate(const TBoard*) { return 0.0f; }
		static float ImpactRate(const TBoard*) { return 0.0f; }
		static FString Readout(const TBoard*) { return FString(); }
	};

	template <typename TBoard>
	struct TPresentationBoardApi<TBoard, std::void_t<
		decltype(std::declval<TBoard&>().QueuePresentation(std::declval<const FGameXXKBattlePresentationEvent&>())),
		decltype(std::declval<const TBoard&>().IsBattlePresentationActiveForTest()),
		decltype(std::declval<const TBoard&>().IsBattleDeathPresentationActiveForTest()),
		decltype(std::declval<const TBoard&>().GetBattlePresentationQueueCountForTest()),
		decltype(std::declval<const TBoard&>().GetActiveBattlePresentationEventIdForTest()),
		decltype(std::declval<const TBoard&>().GetActiveBattlePresentationElapsedForTest()),
		decltype(std::declval<const TBoard&>().GetBattlePresentationImpactCountForTest()),
		decltype(std::declval<const TBoard&>().GetBattlePresentationCompletionCountForTest()),
		decltype(std::declval<const TBoard&>().GetBattlePresentationHudShakeCountForTest()),
		decltype(std::declval<const TBoard&>().GetDisplayedHealthForTest(std::declval<FName>())),
		decltype(std::declval<const TBoard&>().GetActiveAttackerPlaybackRateForTest()),
		decltype(std::declval<const TBoard&>().GetActiveTargetPlaybackRateForTest()),
		decltype(std::declval<const TBoard&>().GetActiveImpactPlaybackRateForTest()),
		decltype(std::declval<const TBoard&>().GetBattlePresentationReadoutForTest())>>
	{
		static constexpr bool bAvailable = true;
		static void Queue(TBoard* Board, const FGameXXKBattlePresentationEvent& Event) { Board->QueuePresentation(Event); }
		static bool IsActive(const TBoard* Board) { return Board->IsBattlePresentationActiveForTest(); }
		static bool IsDeathActive(const TBoard* Board) { return Board->IsBattleDeathPresentationActiveForTest(); }
		static int32 QueueCount(const TBoard* Board) { return Board->GetBattlePresentationQueueCountForTest(); }
		static uint64 ActiveEventId(const TBoard* Board) { return Board->GetActiveBattlePresentationEventIdForTest(); }
		static double ActiveElapsed(const TBoard* Board) { return Board->GetActiveBattlePresentationElapsedForTest(); }
		static int32 ImpactCount(const TBoard* Board) { return Board->GetBattlePresentationImpactCountForTest(); }
		static int32 CompletionCount(const TBoard* Board) { return Board->GetBattlePresentationCompletionCountForTest(); }
		static int32 ShakeCount(const TBoard* Board) { return Board->GetBattlePresentationHudShakeCountForTest(); }
		static int32 DisplayedHealth(const TBoard* Board, const FName UnitId) { return Board->GetDisplayedHealthForTest(UnitId); }
		static float AttackerRate(const TBoard* Board) { return Board->GetActiveAttackerPlaybackRateForTest(); }
		static float TargetRate(const TBoard* Board) { return Board->GetActiveTargetPlaybackRateForTest(); }
		static float ImpactRate(const TBoard* Board) { return Board->GetActiveImpactPlaybackRateForTest(); }
		static FString Readout(const TBoard* Board) { return Board->GetBattlePresentationReadoutForTest(); }
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
	FApi::Queue(Board, First);
	TestTrue(TEXT("queueing prefetches the attacker Attack atlas before presentation starts"),
		AtlasLoader->Requested(FGameXXKBattleAnimationPresentation::ResolveClip(
			First.AttackerUnitId, First.bAttackerEnemy, EGameXXKBattleAnimationAction::Attack).TexturePath));
	TestTrue(TEXT("queueing prefetches the target Hit atlas before presentation starts"),
		AtlasLoader->Requested(FGameXXKBattleAnimationPresentation::ResolveClip(
			First.TargetUnitId, First.bTargetEnemy, EGameXXKBattleAnimationAction::Hit).TexturePath));
	TestTrue(TEXT("queueing prefetches the generic Impact atlas before presentation starts"),
		AtlasLoader->Requested(FGameXXKBattleAnimationPresentation::ResolveGenericClip(
			EGameXXKBattleAnimationAction::Impact).TexturePath));

	Board->AdvanceVisualsAtRealTime(0.0);
	TestTrue(TEXT("the first absolute-clock sample starts the queued presentation"), FApi::IsActive(Board));
	TestEqual(TEXT("the immutable event id survives queue activation"), FApi::ActiveEventId(Board), First.EventId);
	TestEqual(TEXT("Attack playback uses the authored two-times rate"), FApi::AttackerRate(Board), 2.0f);
	TestEqual(TEXT("Hit playback uses the authored two-times rate"), FApi::TargetRate(Board), 2.0f);
	TestEqual(TEXT("generic Impact playback uses the authored four-times rate"), FApi::ImpactRate(Board), 4.0f);
	TestEqual(TEXT("presentation overlays the pre-impact health immediately"),
		FApi::DisplayedHealth(Board, First.TargetUnitId), First.TargetHealthBefore);
	TestEqual(TEXT("the real fixed HUD renders the pre-impact health"),
		GetRenderedHealth(Board, First.TargetUnitId), FString(TEXT("气血 100 / 120")));

	Board->AdvanceVisualsAtRealTime(0.55);
	TestEqual(TEXT("the first partial step retains pre-impact health"),
		FApi::DisplayedHealth(Board, First.TargetUnitId), First.TargetHealthBefore);
	Board->AdvanceVisualsAtRealTime(1.099);
	TestEqual(TEXT("the second partial step immediately before 1.1 retains pre-impact health"),
		FApi::DisplayedHealth(Board, First.TargetUnitId), First.TargetHealthBefore);
	TestEqual(TEXT("impact has not fired before the 1.1 marker"), FApi::ImpactCount(Board), 0);
	TestEqual(TEXT("HUD shake has not fired before the 1.1 marker"), FApi::ShakeCount(Board), 0);

	Board->AdvanceVisualsAtRealTime(1.101);
	TestEqual(TEXT("crossing 1.1 applies the immutable post-impact health"),
		FApi::DisplayedHealth(Board, First.TargetUnitId), First.TargetHealthAfter);
	TestEqual(TEXT("crossing 1.1 redraws the actual fixed HUD"),
		GetRenderedHealth(Board, First.TargetUnitId), FString(TEXT("气血 70 / 120")));
	TestEqual(TEXT("crossing 1.1 fires impact exactly once"), FApi::ImpactCount(Board), 1);
	TestEqual(TEXT("crossing 1.1 fires the HUD-root shake exactly once"), FApi::ShakeCount(Board), 1);
	TestEqual(TEXT("damage presentation emits its readout at the marker"), FApi::Readout(Board), FString(TEXT("-30")));
	Board->AdvanceVisualsAtRealTime(1.8);
	TestEqual(TEXT("later samples cannot refire the same impact"), FApi::ImpactCount(Board), 1);
	TestEqual(TEXT("later samples cannot restart the same shake"), FApi::ShakeCount(Board), 1);

	Board->AdvanceVisualsAtRealTime(2.5);
	TestFalse(TEXT("a nonlethal presentation completes at exactly 2.5 real seconds"), FApi::IsActive(Board));
	TestEqual(TEXT("the paired Attack/Hit completion fires exactly once"), FApi::CompletionCount(Board), 1);
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

	const int32 ImpactBeforeLargeDelta = FApi::ImpactCount(Board);
	const int32 CompletionBeforeLargeDelta = FApi::CompletionCount(Board);
	const FGameXXKBattlePresentationEvent LargeDelta = MakePresentationEvent(
		2, TEXT("Player"), false, TEXT("Enemy.Tiger"), true, 70, 60);
	const FGameXXKBattlePresentationEvent Overflow = MakePresentationEvent(
		3, TEXT("Enemy.BlackBear"), true, TEXT("Player"), false, 90, 90, true);
	FApi::Queue(Board, LargeDelta);
	FApi::Queue(Board, Overflow);
	Board->AdvanceVisualsAtRealTime(10.0);
	Board->AdvanceVisualsAtRealTime(13.0);
	TestEqual(TEXT("one large delta still fires the crossed impact only once"),
		FApi::ImpactCount(Board), ImpactBeforeLargeDelta + 1);
	TestEqual(TEXT("one large delta still fires the crossed completion only once"),
		FApi::CompletionCount(Board), CompletionBeforeLargeDelta + 1);
	TestEqual(TEXT("large-delta overflow starts the next immutable event"),
		FApi::ActiveEventId(Board), Overflow.EventId);
	TestTrue(TEXT("large-delta overflow carries exactly one half second into the next event"),
		FMath::IsNearlyEqual(FApi::ActiveElapsed(Board), 0.5, 0.0001));
	Board->AdvanceVisualsAtRealTime(13.0);
	TestEqual(TEXT("repeating the same absolute sample cannot refire impact"),
		FApi::ImpactCount(Board), ImpactBeforeLargeDelta + 1);
	TestEqual(TEXT("repeating the same absolute sample cannot refire completion"),
		FApi::CompletionCount(Board), CompletionBeforeLargeDelta + 1);
	Board->AdvanceVisualsAtRealTime(13.601);
	TestEqual(TEXT("an avoided packet emits the avoid readout when its own marker crosses"),
		FApi::Readout(Board), FString(TEXT("闪避")));
	TestEqual(TEXT("the overflow event owns one distinct impact"),
		FApi::ImpactCount(Board), ImpactBeforeLargeDelta + 2);
	Board->AdvanceVisualsAtRealTime(15.0);
	TestFalse(TEXT("the overflow event completes on the inherited 12.5 epoch"), FApi::IsActive(Board));

	const FGameXXKBattlePresentationEvent Lethal = MakePresentationEvent(
		4, TEXT("Player"), false, TEXT("Enemy.Tiger"), true, 60, 0, false, true);
	const FGameXXKBattlePresentationEvent AfterDeath = MakePresentationEvent(
		5, TEXT("Enemy.BlackBear"), true, TEXT("Player"), false, 90, 82);
	FApi::Queue(Board, Lethal);
	FApi::Queue(Board, AfterDeath);
	Board->AdvanceVisualsAtRealTime(20.0);
	Board->AdvanceVisualsAtRealTime(22.5);
	TestTrue(TEXT("lethal Attack/Hit completion starts Death before the next event"), FApi::IsDeathActive(Board));
	TestEqual(TEXT("Death keeps the lethal immutable event active"), FApi::ActiveEventId(Board), Lethal.EventId);
	TestEqual(TEXT("the event behind Death remains queued"), FApi::QueueCount(Board), 1);
	TestEqual(TEXT("the lethal visual is not removed before Death presentation"),
		Board->GetUnitVisualForTest(Lethal.TargetUnitId), TargetVisual);
	TestFalse(TEXT("the lethal visual is not marked removed before Death completes"),
		TargetVisual ? TargetVisual->IsRemovedForTest() : true);
	Board->AdvanceVisualsAtRealTime(27.499);
	TestEqual(TEXT("the lethal visual remains until the five-second Death boundary"),
		Board->GetUnitVisualForTest(Lethal.TargetUnitId), TargetVisual);
	Board->AdvanceVisualsAtRealTime(27.5);
	TestNull(TEXT("the lethal visual may be removed only after Death completes"),
		Board->GetUnitVisualForTest(Lethal.TargetUnitId));
	TestEqual(TEXT("the event behind Death starts only after Death removal"),
		FApi::ActiveEventId(Board), AfterDeath.EventId);

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
	const FSoftObjectPath AttackPath = FGameXXKBattleAnimationPresentation::ResolveClip(
		LateIdleEvent.AttackerUnitId,
		LateIdleEvent.bAttackerEnemy,
		EGameXXKBattleAnimationAction::Attack).TexturePath;
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
		2.0f);

	return true;
}

#endif
