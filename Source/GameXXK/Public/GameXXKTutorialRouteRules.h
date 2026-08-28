#pragma once

#include "CoreMinimal.h"

#include "GameXXKTutorialRouteRules.generated.h"

UENUM(BlueprintType)
enum class EGameXXKTutorialRouteNodeKind : uint8
{
	Start,
	Battle,
	Merchant,
	Event,
	Camp,
	Chest,
	Boss,
	Settlement
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKTutorialRouteNodeDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TutorialRoute")
	FName NodeId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TutorialRoute")
	EGameXXKTutorialRouteNodeKind Kind = EGameXXKTutorialRouteNodeKind::Start;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TutorialRoute")
	FName GuideId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TutorialRoute")
	FVector2D NormalizedPosition = FVector2D::ZeroVector;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKTutorialRouteEdge
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TutorialRoute")
	FName FromNodeId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TutorialRoute")
	FName ToNodeId;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKTutorialRouteDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TutorialRoute")
	FName RouteId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TutorialRoute")
	TArray<FGameXXKTutorialRouteNodeDefinition> Nodes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TutorialRoute")
	TArray<FGameXXKTutorialRouteEdge> Edges;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKTutorialRouteProgress
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "TutorialRoute")
	FName CurrentNodeId;

	UPROPERTY(BlueprintReadOnly, Category = "TutorialRoute")
	TSet<FName> CompletedNodeIds;

	UPROPERTY(BlueprintReadOnly, Category = "TutorialRoute")
	TSet<FName> ReachableNodeIds;

	UPROPERTY(BlueprintReadOnly, Category = "TutorialRoute")
	bool bCompleted = false;
};

UENUM(BlueprintType)
enum class EGameXXKTutorialCampChoice : uint8
{
	HealPartyThirtyPercent,
	GainRouteGold
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKTutorialCampActionDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TutorialRoute|Camp")
	EGameXXKTutorialCampChoice Choice = EGameXXKTutorialCampChoice::HealPartyThirtyPercent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TutorialRoute|Camp")
	int32 HealingPercent = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TutorialRoute|Camp")
	int32 RouteGold = 0;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKTutorialPartyHealth
{
	GENERATED_BODY()

	FGameXXKTutorialPartyHealth() = default;
	FGameXXKTutorialPartyHealth(FName InUnitId, int32 InCurrentHealth, int32 InMaxHealth)
		: UnitId(InUnitId)
		, CurrentHealth(InCurrentHealth)
		, MaxHealth(InMaxHealth)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TutorialRoute|Camp")
	FName UnitId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TutorialRoute|Camp")
	int32 CurrentHealth = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TutorialRoute|Camp")
	int32 MaxHealth = 1;
};

class GAMEXXK_API FGameXXKTutorialRouteRules final
{
public:
	/** Seed is intentionally ignored: the combat tutorial graph is authored and deterministic. */
	static FGameXXKTutorialRouteDefinition BuildDefinition(int32 IgnoredRandomSeed = 0);
	static FGameXXKTutorialRouteProgress CreateInitialProgress(
		const FGameXXKTutorialRouteDefinition& Definition);
	static bool CompleteReachableNode(
		const FGameXXKTutorialRouteDefinition& Definition,
		FName NodeId,
		FGameXXKTutorialRouteProgress& InOutProgress,
		FString* OutError = nullptr);

	static TArray<FGameXXKTutorialCampActionDefinition> GetCampActions();
	static bool ResolveCampChoice(
		EGameXXKTutorialCampChoice Choice,
		TArray<FGameXXKTutorialPartyHealth>& InOutParty,
		int32& InOutRouteGold,
		FString* OutError = nullptr);
};
