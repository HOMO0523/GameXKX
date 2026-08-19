#include "UI/GameXXKBattleAtlasCache.h"

#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/Texture2D.h"
#include "HAL/PlatformTime.h"
#include "UObject/StrongObjectPtr.h"

DEFINE_LOG_CATEGORY_STATIC(LogGameXXKBattleAtlasCache, Log, All);

namespace
{
	class FGameXXKStreamableAtlasLoadHandle final : public IGameXXKBattleAtlasLoadHandle
	{
	public:
		explicit FGameXXKStreamableAtlasLoadHandle(TSharedPtr<FStreamableHandle> InHandle)
			: Handle(MoveTemp(InHandle))
		{
		}

		virtual void Cancel() override
		{
			if (Handle.IsValid())
			{
				Handle->CancelHandle();
			}
		}

	private:
		TSharedPtr<FStreamableHandle> Handle;
	};

	class FGameXXKStreamableAtlasLoader final : public IGameXXKBattleAtlasLoader
	{
	public:
		virtual TSharedPtr<IGameXXKBattleAtlasLoadHandle> RequestAsyncLoad(
			const FSoftObjectPath& Path,
			FGameXXKAtlasLoaderCompletion Completion) override
		{
			FStreamableAsyncLoadParams Params;
			Params.TargetsToStream.Add(Path);
			Params.OnComplete = FStreamableDelegateWithHandle::CreateLambda(
				[Completion = MoveTemp(Completion)](TSharedPtr<FStreamableHandle> CompletedHandle) mutable
				{
					UTexture2D* Texture = CompletedHandle.IsValid()
						? CompletedHandle->GetLoadedAsset<UTexture2D>()
						: nullptr;
					const int64 ResidentBytes = Texture != nullptr
						? static_cast<int64>(Texture->CalcTextureMemorySizeEnum(TMC_AllMips))
						: 0;
					if (Completion)
					{
						Completion(Texture, ResidentBytes);
					}
				});

			TSharedPtr<FStreamableHandle> Handle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
				MoveTemp(Params), TEXT("GameXXK battle atlas"));
			if (!Handle.IsValid())
			{
				return nullptr;
			}

			return TSharedPtr<IGameXXKBattleAtlasLoadHandle>(
				MakeShared<FGameXXKStreamableAtlasLoadHandle>(MoveTemp(Handle)));
		}
	};
}

struct FGameXXKBattleAtlasCache::FState : public TSharedFromThis<FState>
{
	struct FWaiter
	{
		uint64 SessionToken = 0;
		double DeadlineSeconds = 0.0;
		FGameXXKAtlasLoadCompletion Completion;
	};

	struct FRequest
	{
		uint64 Generation = 0;
		TArray<FWaiter> Waiters;
		TSharedPtr<IGameXXKBattleAtlasLoadHandle> Handle;
	};

	struct FResident
	{
		TStrongObjectPtr<UTexture2D> Texture;
		int64 Bytes = 0;
		uint64 LastTouch = 0;
	};

	FState(
		TSharedRef<IGameXXKBattleAtlasLoader> InLoader,
		TFunction<double()> InClock,
		const int64 InResidentBudgetBytes,
		const double InTimeoutSeconds)
		: Loader(MoveTemp(InLoader))
		, Clock(MoveTemp(InClock))
		, ResidentBudgetBytes(FMath::Max<int64>(0, InResidentBudgetBytes))
		, TimeoutSeconds(FMath::IsFinite(InTimeoutSeconds) && InTimeoutSeconds >= 0.0
			? InTimeoutSeconds
			: FGameXXKBattleAtlasCache::DefaultTimeoutSeconds)
	{
	}

	double GetNow() const
	{
		const double Now = Clock ? Clock() : FPlatformTime::Seconds();
		return FMath::IsFinite(Now) ? Now : 0.0;
	}

	void Touch(FResident& Resident)
	{
		Resident.LastTouch = ++TouchSequence;
	}

	bool IsPinned(const FSoftObjectPath& Path) const
	{
		const int32* Count = PinCounts.Find(Path);
		return Count != nullptr && *Count > 0;
	}

	bool MakeRoomFor(const int64 IncomingBytes)
	{
		if (IncomingBytes < 0)
		{
			return false;
		}
		// NullRHI and other headless render paths can report zero texture
		// residency for a valid loaded UObject. Keep that asset usable without
		// charging the byte budget; a null texture is still rejected by Admit.
		if (IncomingBytes == 0)
		{
			return true;
		}

		const auto Fits = [this, IncomingBytes]()
		{
			return IncomingBytes <= ResidentBudgetBytes
				&& Stats.ResidentBytes <= ResidentBudgetBytes - IncomingBytes;
		};
		while (!Fits())
		{
			const FSoftObjectPath* OldestPath = nullptr;
			uint64 OldestTouch = MAX_uint64;
			for (const TPair<FSoftObjectPath, FResident>& Pair : Residents)
			{
				if (!IsPinned(Pair.Key) && Pair.Value.LastTouch < OldestTouch)
				{
					OldestPath = &Pair.Key;
					OldestTouch = Pair.Value.LastTouch;
				}
			}

			if (OldestPath == nullptr)
			{
				return false;
			}

			const int64 RemovedBytes = Residents.FindChecked(*OldestPath).Bytes;
			Stats.ResidentBytes = FMath::Max<int64>(0, Stats.ResidentBytes - RemovedBytes);
			Residents.Remove(*OldestPath);
		}

		return true;
	}

	bool Admit(
		const FSoftObjectPath& Path,
		UTexture2D* Texture,
		const int64 ResidentBytes)
	{
		if (Texture == nullptr || !MakeRoomFor(ResidentBytes))
		{
			if (Texture != nullptr)
			{
				UE_LOG(
					LogGameXXKBattleAtlasCache,
					Warning,
					TEXT("Atlas %s (%lld bytes) cannot enter the %lld-byte resident budget; using fallback."),
					*Path.ToString(),
					ResidentBytes,
					ResidentBudgetBytes);
			}
			return false;
		}

		FResident Resident;
		Resident.Texture.Reset(Texture);
		Resident.Bytes = ResidentBytes;
		Touch(Resident);
		Residents.Add(Path, MoveTemp(Resident));
		Stats.ResidentBytes += ResidentBytes;
		Stats.PeakResidentBytes = FMath::Max(Stats.PeakResidentBytes, Stats.ResidentBytes);
		return true;
	}

	void Dispatch(TArray<FWaiter> Waiters, UTexture2D* Texture, const EGameXXKAtlasLoadResult Result)
	{
		TStrongObjectPtr<UTexture2D> KeepAlive(Texture);
		for (FWaiter& Waiter : Waiters)
		{
			if (Waiter.Completion)
			{
				Waiter.Completion(Result == EGameXXKAtlasLoadResult::Loaded ? Texture : nullptr, Result);
			}
		}
	}

	void Complete(
		const FSoftObjectPath Path,
		const uint64 RequestGeneration,
		UTexture2D* Texture,
		const int64 ResidentBytes)
	{
		FRequest* Existing = Requests.Find(Path);
		if (Existing == nullptr || Existing->Generation != RequestGeneration || RequestGeneration != Generation)
		{
			return;
		}

		FRequest CompletedRequest = MoveTemp(*Existing);
		Requests.Remove(Path);
		Stats.ActiveRequestCount = Requests.Num();
		const bool bAdmitted = Admit(Path, Texture, ResidentBytes);
		Dispatch(
			MoveTemp(CompletedRequest.Waiters),
			bAdmitted ? Texture : nullptr,
			bAdmitted ? EGameXXKAtlasLoadResult::Loaded : EGameXXKAtlasLoadResult::Missing);
	}

	void Acquire(
		const FSoftObjectPath& Path,
		const uint64 SessionToken,
		FGameXXKAtlasLoadCompletion Completion)
	{
		if (bShuttingDown)
		{
			if (Completion)
			{
				Completion(nullptr, EGameXXKAtlasLoadResult::Cancelled);
			}
			return;
		}

		if (SessionToken == 0 || CancelledSessions.Contains(SessionToken))
		{
			if (Completion)
			{
				Completion(nullptr, EGameXXKAtlasLoadResult::StaleSession);
			}
			return;
		}

		if (!Path.IsValid())
		{
			if (Completion)
			{
				Completion(nullptr, EGameXXKAtlasLoadResult::Missing);
			}
			return;
		}

		if (FResident* Resident = Residents.Find(Path))
		{
			Touch(*Resident);
			TStrongObjectPtr<UTexture2D> KeepAlive(Resident->Texture);
			if (Completion)
			{
				Completion(KeepAlive.Get(), EGameXXKAtlasLoadResult::Loaded);
			}
			return;
		}

		FWaiter Waiter;
		Waiter.SessionToken = SessionToken;
		Waiter.DeadlineSeconds = GetNow() + TimeoutSeconds;
		Waiter.Completion = MoveTemp(Completion);
		if (FRequest* Existing = Requests.Find(Path))
		{
			Existing->Waiters.Add(MoveTemp(Waiter));
			return;
		}

		FRequest Request;
		Request.Generation = Generation;
		Request.Waiters.Add(MoveTemp(Waiter));
		Requests.Add(Path, MoveTemp(Request));
		Stats.ActiveRequestCount = Requests.Num();

		const uint64 RequestGeneration = Generation;
		const TWeakPtr<FState> WeakState = AsShared();
		TSharedPtr<IGameXXKBattleAtlasLoadHandle> Handle = Loader->RequestAsyncLoad(
			Path,
			[WeakState, Path, RequestGeneration](UTexture2D* LoadedTexture, const int64 LoadedBytes)
			{
				if (const TSharedPtr<FState> PinnedState = WeakState.Pin())
				{
					PinnedState->Complete(Path, RequestGeneration, LoadedTexture, LoadedBytes);
				}
			});

		if (FRequest* Pending = Requests.Find(Path);
			Pending != nullptr && Pending->Generation == RequestGeneration)
		{
			Pending->Handle = Handle;
			if (!Handle.IsValid())
			{
				Complete(Path, RequestGeneration, nullptr, 0);
			}
		}
	}

	void Pin(const FSoftObjectPath& Path)
	{
		if (!Path.IsValid())
		{
			return;
		}

		int32& Count = PinCounts.FindOrAdd(Path);
		if (Count < MAX_int32)
		{
			++Count;
		}
	}

	void Unpin(const FSoftObjectPath& Path)
	{
		int32* Count = PinCounts.Find(Path);
		if (Count == nullptr)
		{
			return;
		}

		if (*Count <= 1)
		{
			PinCounts.Remove(Path);
		}
		else
		{
			--(*Count);
		}
	}

	void AdvanceTimeouts(const double AbsoluteSeconds)
	{
		if (!FMath::IsFinite(AbsoluteSeconds))
		{
			return;
		}

		TArray<FWaiter> TimedOutWaiters;
		for (TPair<FSoftObjectPath, FRequest>& Pair : Requests)
		{
			for (int32 Index = Pair.Value.Waiters.Num() - 1; Index >= 0; --Index)
			{
				if (Pair.Value.Waiters[Index].DeadlineSeconds <= AbsoluteSeconds)
				{
					TimedOutWaiters.Add(MoveTemp(Pair.Value.Waiters[Index]));
					Pair.Value.Waiters.RemoveAtSwap(Index, 1, EAllowShrinking::No);
				}
			}
		}

		Dispatch(MoveTemp(TimedOutWaiters), nullptr, EGameXXKAtlasLoadResult::TimedOut);
	}

	void CancelSession(const uint64 SessionToken)
	{
		if (SessionToken == 0)
		{
			return;
		}

		CancelledSessions.Add(SessionToken);
		TArray<FWaiter> CancelledWaiters;
		for (TPair<FSoftObjectPath, FRequest>& Pair : Requests)
		{
			for (int32 Index = Pair.Value.Waiters.Num() - 1; Index >= 0; --Index)
			{
				if (Pair.Value.Waiters[Index].SessionToken == SessionToken)
				{
					CancelledWaiters.Add(MoveTemp(Pair.Value.Waiters[Index]));
					Pair.Value.Waiters.RemoveAtSwap(Index, 1, EAllowShrinking::No);
				}
			}
		}

		Dispatch(MoveTemp(CancelledWaiters), nullptr, EGameXXKAtlasLoadResult::Cancelled);
	}

	void Clear(const bool bDispatchCancellation)
	{
		++Generation;
		TArray<FWaiter> CancelledWaiters;
		TArray<TSharedPtr<IGameXXKBattleAtlasLoadHandle>> Handles;
		for (TPair<FSoftObjectPath, FRequest>& Pair : Requests)
		{
			CancelledWaiters.Append(MoveTemp(Pair.Value.Waiters));
			if (Pair.Value.Handle.IsValid())
			{
				Handles.Add(MoveTemp(Pair.Value.Handle));
			}
		}

		Requests.Reset();
		Residents.Reset();
		PinCounts.Reset();
		CancelledSessions.Reset();
		Stats.ActiveRequestCount = 0;
		Stats.ResidentBytes = 0;

		for (const TSharedPtr<IGameXXKBattleAtlasLoadHandle>& Handle : Handles)
		{
			Handle->Cancel();
		}
		if (bDispatchCancellation)
		{
			Dispatch(MoveTemp(CancelledWaiters), nullptr, EGameXXKAtlasLoadResult::Cancelled);
		}
	}

	void Shutdown()
	{
		if (bShuttingDown)
		{
			return;
		}

		bShuttingDown = true;
		Clear(true);
	}

	TSharedRef<IGameXXKBattleAtlasLoader> Loader;
	TFunction<double()> Clock;
	int64 ResidentBudgetBytes = FGameXXKBattleAtlasCache::DefaultResidentBudgetBytes;
	double TimeoutSeconds = FGameXXKBattleAtlasCache::DefaultTimeoutSeconds;
	uint64 Generation = 1;
	uint64 TouchSequence = 0;
	bool bShuttingDown = false;
	TMap<FSoftObjectPath, FRequest> Requests;
	TMap<FSoftObjectPath, FResident> Residents;
	TMap<FSoftObjectPath, int32> PinCounts;
	TSet<uint64> CancelledSessions;
	FGameXXKBattleAtlasCacheStats Stats;
};

FGameXXKBattleAtlasCache::FGameXXKBattleAtlasCache()
	: FGameXXKBattleAtlasCache(
		MakeShared<FGameXXKStreamableAtlasLoader>(),
		[]() { return FPlatformTime::Seconds(); })
{
}

FGameXXKBattleAtlasCache::FGameXXKBattleAtlasCache(
	TSharedRef<IGameXXKBattleAtlasLoader> InLoader,
	TFunction<double()> InClock,
	const int64 InResidentBudgetBytes,
	const double InTimeoutSeconds)
	: State(MakeShared<FState>(
		MoveTemp(InLoader),
		MoveTemp(InClock),
		InResidentBudgetBytes,
		InTimeoutSeconds))
{
}

FGameXXKBattleAtlasCache::~FGameXXKBattleAtlasCache()
{
	State->Shutdown();
}

void FGameXXKBattleAtlasCache::Acquire(
	const FSoftObjectPath& Path,
	const uint64 SessionToken,
	FGameXXKAtlasLoadCompletion Completion)
{
	State->Acquire(Path, SessionToken, MoveTemp(Completion));
}

void FGameXXKBattleAtlasCache::Pin(const FSoftObjectPath& Path)
{
	State->Pin(Path);
}

void FGameXXKBattleAtlasCache::Unpin(const FSoftObjectPath& Path)
{
	State->Unpin(Path);
}

void FGameXXKBattleAtlasCache::AdvanceTimeouts(const double AbsoluteSeconds)
{
	State->AdvanceTimeouts(AbsoluteSeconds);
}

void FGameXXKBattleAtlasCache::CancelSession(const uint64 SessionToken)
{
	State->CancelSession(SessionToken);
}

void FGameXXKBattleAtlasCache::Clear()
{
	State->Clear(true);
}

const FGameXXKBattleAtlasCacheStats& FGameXXKBattleAtlasCache::GetStats() const
{
	return State->Stats;
}
