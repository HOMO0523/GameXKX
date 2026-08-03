#include "UI/GameXXKBattleAtlasCache.h"

#include "Engine/Texture2D.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	class FFakeAtlasLoadHandle final : public IGameXXKBattleAtlasLoadHandle
	{
	public:
		virtual void Cancel() override
		{
			bCancelled = true;
		}

		bool bCancelled = false;
	};

	class FFakeAtlasLoader final : public IGameXXKBattleAtlasLoader
	{
	public:
		struct FPendingLoad
		{
			FPendingLoad(
				FGameXXKAtlasLoaderCompletion InCompletion,
				TSharedRef<FFakeAtlasLoadHandle> InHandle)
				: Completion(MoveTemp(InCompletion))
				, Handle(MoveTemp(InHandle))
			{
			}

			FGameXXKAtlasLoaderCompletion Completion;
			TSharedRef<FFakeAtlasLoadHandle> Handle;
		};

		virtual TSharedPtr<IGameXXKBattleAtlasLoadHandle> RequestAsyncLoad(
			const FSoftObjectPath& Path,
			FGameXXKAtlasLoaderCompletion Completion) override
		{
			RequestCounts.FindOrAdd(Path)++;
			TSharedRef<FFakeAtlasLoadHandle> Handle = MakeShared<FFakeAtlasLoadHandle>();
			TUniquePtr<FPendingLoad> Pending = MakeUnique<FPendingLoad>(MoveTemp(Completion), Handle);
			PendingLoads.Add(Path, MoveTemp(Pending));
			Handles.Add(Path, Handle);
			return Handle;
		}

		int32 RequestCount(const FSoftObjectPath& Path) const
		{
			const int32* Count = RequestCounts.Find(Path);
			return Count != nullptr ? *Count : 0;
		}

		bool IsPending(const FSoftObjectPath& Path) const
		{
			return PendingLoads.Contains(Path);
		}

		bool WasCancelled(const FSoftObjectPath& Path) const
		{
			const TSharedRef<FFakeAtlasLoadHandle>* Handle = Handles.Find(Path);
			return Handle != nullptr && Handle->Get().bCancelled;
		}

		void Complete(const FSoftObjectPath& Path, UTexture2D* Texture, const int64 ResidentBytes)
		{
			TUniquePtr<FPendingLoad> Pending;
			if (TUniquePtr<FPendingLoad>* Found = PendingLoads.Find(Path))
			{
				Pending = MoveTemp(*Found);
				PendingLoads.Remove(Path);
			}

			if (Pending && Pending->Completion)
			{
				Pending->Completion(Texture, ResidentBytes);
			}
		}

	private:
		TMap<FSoftObjectPath, int32> RequestCounts;
		TMap<FSoftObjectPath, TUniquePtr<FPendingLoad>> PendingLoads;
		TMap<FSoftObjectPath, TSharedRef<FFakeAtlasLoadHandle>> Handles;
	};

	TStrongObjectPtr<UTexture2D> MakeTestTexture()
	{
		return TStrongObjectPtr<UTexture2D>(NewObject<UTexture2D>(GetTransientPackage()));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBattleAtlasCacheCoalescingTest,
	"GameXXK.UI.Battle.AtlasCache.Coalescing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBattleAtlasCacheCoalescingTest::RunTest(const FString& Parameters)
{
	double Now = 100.0;
	const TSharedRef<FFakeAtlasLoader> Loader = MakeShared<FFakeAtlasLoader>();
	FGameXXKBattleAtlasCache Cache(Loader, [&Now]() { return Now; });
	const FSoftObjectPath Path(TEXT("/Game/Test/T_Shared.T_Shared"));
	TStrongObjectPtr<UTexture2D> Texture = MakeTestTexture();

	TArray<EGameXXKAtlasLoadResult> Results;
	TArray<UTexture2D*> Textures;
	Cache.Acquire(Path, 1, [&Results, &Textures](UTexture2D* LoadedTexture, EGameXXKAtlasLoadResult Result)
	{
		Results.Add(Result);
		Textures.Add(LoadedTexture);
	});
	Cache.Acquire(Path, 2, [&Results, &Textures](UTexture2D* LoadedTexture, EGameXXKAtlasLoadResult Result)
	{
		Results.Add(Result);
		Textures.Add(LoadedTexture);
	});

	TestEqual(TEXT("duplicate path requests coalesce across sessions"), Loader->RequestCount(Path), 1);
	TestEqual(TEXT("one request is active before completion"), Cache.GetStats().ActiveRequestCount, 1);
	Loader->Complete(Path, Texture.Get(), 16ll * 1024ll * 1024ll);
	TestEqual(TEXT("every waiter completes exactly once"), Results.Num(), 2);
	TestEqual(TEXT("first waiter receives Loaded"), Results[0], EGameXXKAtlasLoadResult::Loaded);
	TestEqual(TEXT("second waiter receives Loaded"), Results[1], EGameXXKAtlasLoadResult::Loaded);
	TestTrue(TEXT("first waiter receives the loaded texture"), Textures[0] == Texture.Get());
	TestTrue(TEXT("second waiter receives the loaded texture"), Textures[1] == Texture.Get());
	TestEqual(TEXT("completed request leaves no active load"), Cache.GetStats().ActiveRequestCount, 0);
	TestEqual(TEXT("loaded texture contributes weighted resident bytes"), Cache.GetStats().ResidentBytes, 16ll * 1024ll * 1024ll);

	int32 HitCallbacks = 0;
	Cache.Acquire(Path, 3, [&HitCallbacks, Expected = Texture.Get()](UTexture2D* LoadedTexture, EGameXXKAtlasLoadResult Result)
	{
		if (Result == EGameXXKAtlasLoadResult::Loaded && LoadedTexture == Expected)
		{
			++HitCallbacks;
		}
	});
	TestEqual(TEXT("resident hit completes immediately"), HitCallbacks, 1);
	TestEqual(TEXT("resident hit does not issue another load"), Loader->RequestCount(Path), 1);
	TestEqual(TEXT("cache never performs synchronous loads"), Cache.GetStats().SyncLoadCount, 0);
	TestEqual(TEXT("cache never forces garbage collection"), Cache.GetStats().ForcedGcCount, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBattleAtlasCacheSessionAndTimeoutTest,
	"GameXXK.UI.Battle.AtlasCache.SessionAndTimeout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBattleAtlasCacheSessionAndTimeoutTest::RunTest(const FString& Parameters)
{
	double Now = 10.0;
	const TSharedRef<FFakeAtlasLoader> Loader = MakeShared<FFakeAtlasLoader>();
	FGameXXKBattleAtlasCache Cache(Loader, [&Now]() { return Now; }, 256ll * 1024ll * 1024ll, 5.0);
	const FSoftObjectPath SharedPath(TEXT("/Game/Test/T_SessionShared.T_SessionShared"));
	TStrongObjectPtr<UTexture2D> SharedTexture = MakeTestTexture();

	TArray<EGameXXKAtlasLoadResult> CancelledSessionResults;
	Cache.Acquire(SharedPath, 10, [&CancelledSessionResults](UTexture2D*, EGameXXKAtlasLoadResult Result)
	{
		CancelledSessionResults.Add(Result);
	});
	Cache.CancelSession(10);
	TestEqual(TEXT("existing waiter is cancelled exactly once"), CancelledSessionResults.Num(), 1);
	TestEqual(TEXT("existing waiter reports Cancelled"), CancelledSessionResults[0], EGameXXKAtlasLoadResult::Cancelled);

	Cache.Acquire(SharedPath, 10, [&CancelledSessionResults](UTexture2D*, EGameXXKAtlasLoadResult Result)
	{
		CancelledSessionResults.Add(Result);
	});
	TestEqual(TEXT("future request on cancelled token completes once"), CancelledSessionResults.Num(), 2);
	TestEqual(TEXT("future request on cancelled token is stale"), CancelledSessionResults[1], EGameXXKAtlasLoadResult::StaleSession);
	TestEqual(TEXT("stale request does not start another load"), Loader->RequestCount(SharedPath), 1);

	EGameXXKAtlasLoadResult ZeroTokenResult = EGameXXKAtlasLoadResult::Loaded;
	Cache.Acquire(FSoftObjectPath(TEXT("/Game/Test/T_Zero.T_Zero")), 0,
		[&ZeroTokenResult](UTexture2D*, EGameXXKAtlasLoadResult Result) { ZeroTokenResult = Result; });
	TestEqual(TEXT("token zero is always stale"), ZeroTokenResult, EGameXXKAtlasLoadResult::StaleSession);

	int32 OtherSessionLoaded = 0;
	Cache.Acquire(SharedPath, 11, [&OtherSessionLoaded](UTexture2D* Texture, EGameXXKAtlasLoadResult Result)
	{
		if (Texture != nullptr && Result == EGameXXKAtlasLoadResult::Loaded)
		{
			++OtherSessionLoaded;
		}
	});
	TestEqual(TEXT("a different session shares the still-running path load"), Loader->RequestCount(SharedPath), 1);
	Loader->Complete(SharedPath, SharedTexture.Get(), 32);
	TestEqual(TEXT("different session receives shared completion"), OtherSessionLoaded, 1);
	TestEqual(TEXT("cancelled waiter is never called a second time"), CancelledSessionResults.Num(), 2);

	const FSoftObjectPath TimeoutPath(TEXT("/Game/Test/T_Timeout.T_Timeout"));
	TArray<EGameXXKAtlasLoadResult> EarlyResults;
	TArray<EGameXXKAtlasLoadResult> LateResults;
	Cache.Acquire(TimeoutPath, 20, [&EarlyResults](UTexture2D*, EGameXXKAtlasLoadResult Result) { EarlyResults.Add(Result); });
	Now = 13.0;
	Cache.Acquire(TimeoutPath, 21, [&LateResults](UTexture2D*, EGameXXKAtlasLoadResult Result) { LateResults.Add(Result); });
	Now = 16.0;
	Cache.AdvanceTimeouts(Now);
	TestEqual(TEXT("earlier waiter times out independently"), EarlyResults.Num(), 1);
	TestEqual(TEXT("earlier waiter reports TimedOut"), EarlyResults[0], EGameXXKAtlasLoadResult::TimedOut);
	TestEqual(TEXT("later waiter remains pending"), LateResults.Num(), 0);
	TStrongObjectPtr<UTexture2D> TimeoutTexture = MakeTestTexture();
	Loader->Complete(TimeoutPath, TimeoutTexture.Get(), 64);
	TestEqual(TEXT("late waiter still completes"), LateResults.Num(), 1);
	TestEqual(TEXT("late waiter reports Loaded"), LateResults[0], EGameXXKAtlasLoadResult::Loaded);
	TestEqual(TEXT("timed out waiter is never called again"), EarlyResults.Num(), 1);

	const FSoftObjectPath MissingPath(TEXT("/Game/Test/T_Missing.T_Missing"));
	TArray<EGameXXKAtlasLoadResult> MissingResults;
	Cache.Acquire(MissingPath, 30, [&MissingResults](UTexture2D*, EGameXXKAtlasLoadResult Result) { MissingResults.Add(Result); });
	Loader->Complete(MissingPath, nullptr, 0);
	TestEqual(TEXT("failed load invokes fallback once"), MissingResults.Num(), 1);
	TestEqual(TEXT("failed load reports Missing"), MissingResults[0], EGameXXKAtlasLoadResult::Missing);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBattleAtlasCacheBudgetAndLruTest,
	"GameXXK.UI.Battle.AtlasCache.BudgetAndLru",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBattleAtlasCacheBudgetAndLruTest::RunTest(const FString& Parameters)
{
	double Now = 1.0;
	const TSharedRef<FFakeAtlasLoader> Loader = MakeShared<FFakeAtlasLoader>();
	FGameXXKBattleAtlasCache Cache(Loader, [&Now]() { return Now; }, 64, 10.0);
	const FSoftObjectPath PathA(TEXT("/Game/Test/T_A.T_A"));
	const FSoftObjectPath PathB(TEXT("/Game/Test/T_B.T_B"));
	const FSoftObjectPath PathC(TEXT("/Game/Test/T_C.T_C"));
	const FSoftObjectPath PathD(TEXT("/Game/Test/T_D.T_D"));
	const FSoftObjectPath PathE(TEXT("/Game/Test/T_E.T_E"));
	TStrongObjectPtr<UTexture2D> TextureA = MakeTestTexture();
	TStrongObjectPtr<UTexture2D> TextureB = MakeTestTexture();
	TStrongObjectPtr<UTexture2D> TextureC = MakeTestTexture();
	TStrongObjectPtr<UTexture2D> TextureD = MakeTestTexture();
	TStrongObjectPtr<UTexture2D> TextureE = MakeTestTexture();

	Cache.Pin(PathA);
	Cache.Pin(PathA);
	Cache.Acquire(PathA, 1, [](UTexture2D*, EGameXXKAtlasLoadResult) {});
	Loader->Complete(PathA, TextureA.Get(), 40);
	Cache.Unpin(PathA);

	EGameXXKAtlasLoadResult FirstBResult = EGameXXKAtlasLoadResult::Loaded;
	Cache.Acquire(PathB, 2, [&FirstBResult](UTexture2D*, EGameXXKAtlasLoadResult Result) { FirstBResult = Result; });
	Loader->Complete(PathB, TextureB.Get(), 40);
	TestEqual(TEXT("one remaining pin prevents eviction"), FirstBResult, EGameXXKAtlasLoadResult::Missing);
	TestEqual(TEXT("over-budget texture is not made resident"), Cache.GetStats().ResidentBytes, 40ll);
	TestTrue(TEXT("resident bytes never exceed injected cap"), Cache.GetStats().ResidentBytes <= 64ll);

	Cache.Unpin(PathA);
	EGameXXKAtlasLoadResult SecondBResult = EGameXXKAtlasLoadResult::Missing;
	Cache.Acquire(PathB, 3, [&SecondBResult](UTexture2D*, EGameXXKAtlasLoadResult Result) { SecondBResult = Result; });
	TestEqual(TEXT("nonresident budget failure starts a later retry"), Loader->RequestCount(PathB), 2);
	Loader->Complete(PathB, TextureB.Get(), 40);
	TestEqual(TEXT("unpinned oldest resident is evicted for retry"), SecondBResult, EGameXXKAtlasLoadResult::Loaded);
	TestEqual(TEXT("retry remains within budget"), Cache.GetStats().ResidentBytes, 40ll);

	Cache.Acquire(PathC, 4, [](UTexture2D*, EGameXXKAtlasLoadResult) {});
	Loader->Complete(PathC, TextureC.Get(), 20);
	TestEqual(TEXT("two weighted residents fit exactly under cap"), Cache.GetStats().ResidentBytes, 60ll);
	Cache.Acquire(PathB, 5, [](UTexture2D*, EGameXXKAtlasLoadResult) {});
	TestEqual(TEXT("LRU hit does not reload B"), Loader->RequestCount(PathB), 2);
	Cache.Acquire(PathD, 6, [](UTexture2D*, EGameXXKAtlasLoadResult) {});
	Loader->Complete(PathD, TextureD.Get(), 20);
	TestEqual(TEXT("LRU eviction retains bounded bytes"), Cache.GetStats().ResidentBytes, 60ll);
	Cache.Acquire(PathC, 7, [](UTexture2D*, EGameXXKAtlasLoadResult) {});
	TestEqual(TEXT("least-recently-used C was evicted after B hit"), Loader->RequestCount(PathC), 2);
	Cache.Clear();

	Cache.Acquire(PathA, 70, [](UTexture2D*, EGameXXKAtlasLoadResult) {});
	Loader->Complete(PathA, TextureA.Get(), 40);
	EGameXXKAtlasLoadResult OversizedResult = EGameXXKAtlasLoadResult::Loaded;
	Cache.Acquire(PathE, 71, [&OversizedResult](UTexture2D*, EGameXXKAtlasLoadResult Result)
	{
		OversizedResult = Result;
	});
	Loader->Complete(PathE, TextureE.Get(), 80);
	TestEqual(TEXT("texture larger than the entire budget falls back"), OversizedResult, EGameXXKAtlasLoadResult::Missing);
	TestEqual(TEXT("oversized admission first evicts unpinned residents"), Cache.GetStats().ResidentBytes, 0ll);
	Cache.Clear();

	Cache.Pin(PathB);
	Cache.Acquire(PathB, 8, [](UTexture2D*, EGameXXKAtlasLoadResult) {});
	Loader->Complete(PathB, TextureB.Get(), 40);
	EGameXXKAtlasLoadResult HugeResult = EGameXXKAtlasLoadResult::Loaded;
	Cache.Acquire(PathE, 9, [&HugeResult](UTexture2D*, EGameXXKAtlasLoadResult Result) { HugeResult = Result; });
	Loader->Complete(PathE, TextureE.Get(), 64);
	TestEqual(TEXT("texture that cannot fit beside a pin falls back"), HugeResult, EGameXXKAtlasLoadResult::Missing);
	TestEqual(TEXT("strict admission never evicts pinned resident"), Cache.GetStats().ResidentBytes, 40ll);
	TestTrue(TEXT("peak resident bytes never exceed cap"), Cache.GetStats().PeakResidentBytes <= 64ll);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBattleAtlasCacheClearGenerationTest,
	"GameXXK.UI.Battle.AtlasCache.ClearGeneration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBattleAtlasCacheClearGenerationTest::RunTest(const FString& Parameters)
{
	double Now = 50.0;
	const TSharedRef<FFakeAtlasLoader> Loader = MakeShared<FFakeAtlasLoader>();
	FGameXXKBattleAtlasCache Cache(Loader, [&Now]() { return Now; }, 256, 10.0);
	const FSoftObjectPath OldPathA(TEXT("/Game/Test/T_OldA.T_OldA"));
	const FSoftObjectPath OldPathB(TEXT("/Game/Test/T_OldB.T_OldB"));
	const FSoftObjectPath NewPath(TEXT("/Game/Test/T_Reentrant.T_Reentrant"));
	int32 OldCancelled = 0;
	int32 NewLoaded = 0;

	Cache.Acquire(OldPathA, 1, [&Cache, &OldCancelled, &NewLoaded, NewPath](UTexture2D*, EGameXXKAtlasLoadResult Result)
	{
		if (Result == EGameXXKAtlasLoadResult::Cancelled)
		{
			++OldCancelled;
			Cache.Acquire(NewPath, 3, [&NewLoaded](UTexture2D* Texture, EGameXXKAtlasLoadResult NewResult)
			{
				if (Texture != nullptr && NewResult == EGameXXKAtlasLoadResult::Loaded)
				{
					++NewLoaded;
				}
			});
		}
	});
	Cache.Acquire(OldPathB, 2, [&OldCancelled](UTexture2D*, EGameXXKAtlasLoadResult Result)
	{
		if (Result == EGameXXKAtlasLoadResult::Cancelled)
		{
			++OldCancelled;
		}
	});

	Cache.Clear();
	TestEqual(TEXT("Clear cancels every existing waiter once"), OldCancelled, 2);
	TestTrue(TEXT("Clear cancels first old loader handle"), Loader->WasCancelled(OldPathA));
	TestTrue(TEXT("Clear cancels second old loader handle"), Loader->WasCancelled(OldPathB));
	TestEqual(TEXT("reentrant Acquire survives Clear callback dispatch"), Loader->RequestCount(NewPath), 1);
	TestEqual(TEXT("only reentrant request remains active"), Cache.GetStats().ActiveRequestCount, 1);

	TStrongObjectPtr<UTexture2D> OldTextureA = MakeTestTexture();
	TStrongObjectPtr<UTexture2D> OldTextureB = MakeTestTexture();
	Loader->Complete(OldPathA, OldTextureA.Get(), 64);
	Loader->Complete(OldPathB, OldTextureB.Get(), 64);
	TestEqual(TEXT("late callbacks from old generation are discarded"), Cache.GetStats().ResidentBytes, 0ll);
	TestEqual(TEXT("late callbacks do not disturb active generation"), Cache.GetStats().ActiveRequestCount, 1);

	TStrongObjectPtr<UTexture2D> NewTexture = MakeTestTexture();
	Loader->Complete(NewPath, NewTexture.Get(), 64);
	TestEqual(TEXT("reentrant request completes normally"), NewLoaded, 1);
	TestEqual(TEXT("new generation becomes resident"), Cache.GetStats().ResidentBytes, 64ll);

	const FSoftObjectPath DestructionPath(TEXT("/Game/Test/T_Destruction.T_Destruction"));
	const FSoftObjectPath DestructionReentrantPath(TEXT("/Game/Test/T_DestructionReentrant.T_DestructionReentrant"));
	int32 DestructionCancelled = 0;
	int32 DestructionReentrantCancelled = 0;
	EGameXXKAtlasLoadResult DestructionReentrantResult = EGameXXKAtlasLoadResult::Loaded;
	TUniquePtr<FGameXXKBattleAtlasCache> DestructingCache = MakeUnique<FGameXXKBattleAtlasCache>(
		Loader,
		[&Now]() { return Now; },
		256,
		10.0);
	FGameXXKBattleAtlasCache* DestructingCacheDuringCallback = DestructingCache.Get();
	DestructingCache->Acquire(
			DestructionPath,
			99,
			[&DestructionCancelled,
			 &DestructionReentrantCancelled,
			 &DestructionReentrantResult,
			 DestructingCacheDuringCallback,
			 DestructionReentrantPath](UTexture2D*, EGameXXKAtlasLoadResult Result)
			{
				if (Result == EGameXXKAtlasLoadResult::Cancelled)
				{
					++DestructionCancelled;
					DestructingCacheDuringCallback->Acquire(
						DestructionReentrantPath,
						100,
						[&DestructionReentrantCancelled, &DestructionReentrantResult](
							UTexture2D*,
							EGameXXKAtlasLoadResult ReentrantResult)
						{
							DestructionReentrantResult = ReentrantResult;
							++DestructionReentrantCancelled;
						});
				}
			});
	DestructingCache.Reset();
	TestEqual(TEXT("cache destruction completes its waiter exactly once"), DestructionCancelled, 1);
	TestTrue(TEXT("cache destruction cancels its loader handle"), Loader->WasCancelled(DestructionPath));
	TestEqual(TEXT("Acquire reentered during destruction completes exactly once"), DestructionReentrantCancelled, 1);
	TestEqual(TEXT("Acquire reentered during destruction reports Cancelled"),
		DestructionReentrantResult, EGameXXKAtlasLoadResult::Cancelled);
	TestEqual(TEXT("Acquire reentered during destruction never starts a loader"),
		Loader->RequestCount(DestructionReentrantPath), 0);
	TestFalse(TEXT("Acquire reentered during destruction leaves no pending load"),
		Loader->IsPending(DestructionReentrantPath));
	return true;
}

#endif
