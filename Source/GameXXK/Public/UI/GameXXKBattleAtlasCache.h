#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPath.h"

class UTexture2D;

enum class EGameXXKAtlasLoadResult : uint8
{
	Loaded,
	Missing,
	TimedOut,
	Cancelled,
	StaleSession
};

using FGameXXKAtlasLoadCompletion = TFunction<void(UTexture2D*, EGameXXKAtlasLoadResult)>;
using FGameXXKAtlasLoaderCompletion = TFunction<void(UTexture2D*, int64)>;

class GAMEXXK_API IGameXXKBattleAtlasLoadHandle
{
public:
	virtual ~IGameXXKBattleAtlasLoadHandle() = default;
	virtual void Cancel() = 0;
};

class GAMEXXK_API IGameXXKBattleAtlasLoader
{
public:
	virtual ~IGameXXKBattleAtlasLoader() = default;

	virtual TSharedPtr<IGameXXKBattleAtlasLoadHandle> RequestAsyncLoad(
		const FSoftObjectPath& Path,
		FGameXXKAtlasLoaderCompletion Completion) = 0;
};

struct FGameXXKBattleAtlasCacheStats
{
	int64 ResidentBytes = 0;
	int64 PeakResidentBytes = 0;
	int32 SyncLoadCount = 0;
	int32 ForcedGcCount = 0;
	int32 ActiveRequestCount = 0;
};

class GAMEXXK_API FGameXXKBattleAtlasCache
{
public:
	static constexpr int64 DefaultResidentBudgetBytes = 256ll * 1024ll * 1024ll;
	static constexpr double DefaultTimeoutSeconds = 10.0;

	FGameXXKBattleAtlasCache();
	FGameXXKBattleAtlasCache(
		TSharedRef<IGameXXKBattleAtlasLoader> InLoader,
		TFunction<double()> InClock,
		int64 InResidentBudgetBytes = DefaultResidentBudgetBytes,
		double InTimeoutSeconds = DefaultTimeoutSeconds);
	~FGameXXKBattleAtlasCache();

	FGameXXKBattleAtlasCache(const FGameXXKBattleAtlasCache&) = delete;
	FGameXXKBattleAtlasCache& operator=(const FGameXXKBattleAtlasCache&) = delete;
	FGameXXKBattleAtlasCache(FGameXXKBattleAtlasCache&&) = delete;
	FGameXXKBattleAtlasCache& operator=(FGameXXKBattleAtlasCache&&) = delete;

	void Acquire(const FSoftObjectPath& Path, uint64 SessionToken, FGameXXKAtlasLoadCompletion Completion);
	void Pin(const FSoftObjectPath& Path);
	void Unpin(const FSoftObjectPath& Path);
	void AdvanceTimeouts(double AbsoluteSeconds);
	void CancelSession(uint64 SessionToken);
	void Clear();

	const FGameXXKBattleAtlasCacheStats& GetStats() const;

private:
	struct FState;
	TSharedRef<FState> State;
};
