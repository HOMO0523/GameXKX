#include "MVP/GameXXKBattleSceneUnitActor.h"

#include "Components/BoxComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "MVP/GameXXKLevelFlow.h"
#include "MVP/GameXXKMVPPlayerController.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "PaperFlipbook.h"
#include "PaperFlipbookComponent.h"

namespace
{
	const FSoftObjectPath HeroBattleFlipbookPath(TEXT("/Game/GameXXK/Characters/Hero/Flipbooks/FB_Hero_Idle_West.FB_Hero_Idle_West"));
	const FSoftObjectPath FollowerBattleFlipbookPath(TEXT("/Game/GameXXK/Characters/Follower/Flipbooks/FB_Npc_Idle_West.FB_Npc_Idle_West"));
	const FSoftObjectPath EnemyBattleFlipbookPath(TEXT("/Game/GameXXK/Characters/Enemies/Flipbooks/FB_Enemy_Default.FB_Enemy_Default"));
	const FSoftObjectPath MoneyMouseBattleFlipbookPath(TEXT("/Game/GameXXK/Characters/Enemies/Flipbooks/FB_Enemy_MoneyMouse.FB_Enemy_MoneyMouse"));
	const FSoftObjectPath NiuHuanBattleFlipbookPath(TEXT("/Game/GameXXK/Characters/Enemies/Flipbooks/FB_Enemy_NiuHuan.FB_Enemy_NiuHuan"));
	const FSoftObjectPath BlackBearBattleFlipbookPath(TEXT("/Game/GameXXK/Characters/Enemies/Flipbooks/FB_Enemy_BlackBear.FB_Enemy_BlackBear"));
	const FSoftObjectPath TigerBossBattleFlipbookPath(TEXT("/Game/GameXXK/Characters/Enemies/Flipbooks/FB_Boss_Tiger.FB_Boss_Tiger"));
	const FLinearColor EnemyBattleTint(1.0f, 1.0f, 1.0f, 1.0f);
	const FLinearColor PartyBattleTint(1.0f, 1.0f, 1.0f, 1.0f);
}

AGameXXKBattleSceneUnitActor::AGameXXKBattleSceneUnitActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	HitArea = CreateDefaultSubobject<UBoxComponent>(TEXT("HitArea"));
	HitArea->SetupAttachment(SceneRoot);
	HitArea->SetBoxExtent(FVector(70.0f, 70.0f, 100.0f));
	HitArea->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	HitArea->SetCollisionObjectType(ECC_WorldDynamic);
	HitArea->SetCollisionResponseToAllChannels(ECR_Ignore);
	HitArea->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	HitArea->SetGenerateOverlapEvents(false);

	BattleVisual = CreateDefaultSubobject<UPaperFlipbookComponent>(TEXT("BattleVisual"));
	BattleVisual->SetupAttachment(SceneRoot);
	BattleVisual->SetRelativeLocation(FVector::ZeroVector);
	BattleVisual->SetRelativeRotation(FRotator(0.0f, 90.0f, -30.0f));
	BattleVisual->SetRelativeScale3D(FVector(0.55f, 0.55f, 0.55f));
	BattleVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BattleVisual->SetCastShadow(false);
	BattleVisual->SetTranslucentSortPriority(20);
	BattleVisual->SetLooping(true);

	LabelText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("LabelText"));
	LabelText->SetupAttachment(SceneRoot);
	LabelText->SetRelativeLocation(FVector(0.0f, 0.0f, 145.0f));
	LabelText->SetHorizontalAlignment(EHTA_Center);
	LabelText->SetVerticalAlignment(EVRTA_TextCenter);
	LabelText->SetWorldSize(24.0f);
	LabelText->SetVisibility(false);
	LabelText->SetHiddenInGame(true);

	HeroBattleFlipbookAsset = TSoftObjectPtr<UPaperFlipbook>(HeroBattleFlipbookPath);
	FollowerBattleFlipbookAsset = TSoftObjectPtr<UPaperFlipbook>(FollowerBattleFlipbookPath);
	EnemyBattleFlipbookAsset = TSoftObjectPtr<UPaperFlipbook>(EnemyBattleFlipbookPath);
	MoneyMouseBattleFlipbookAsset = TSoftObjectPtr<UPaperFlipbook>(MoneyMouseBattleFlipbookPath);
	NiuHuanBattleFlipbookAsset = TSoftObjectPtr<UPaperFlipbook>(NiuHuanBattleFlipbookPath);
	BlackBearBattleFlipbookAsset = TSoftObjectPtr<UPaperFlipbook>(BlackBearBattleFlipbookPath);
	TigerBossBattleFlipbookAsset = TSoftObjectPtr<UPaperFlipbook>(TigerBossBattleFlipbookPath);
}

void AGameXXKBattleSceneUnitActor::ConfigureFromRuntimeUnit(bool bInEnemy, int32 InUnitIndex, const FGameXXKBattleRuntimeUnit& Unit)
{
	bEnemy = bInEnemy;
	UnitIndex = InUnitIndex;
	UnitId = Unit.Id;
	CurrentHP = Unit.HP;
	MaxHP = Unit.MaxHP;
	bDefeated = Unit.bDefeated;
	RefreshLabel();
	RefreshVisual();
}

bool AGameXXKBattleSceneUnitActor::ApplyPrimaryPartyAttack(APawn* InstigatorPawn)
{
	return false;
}

bool AGameXXKBattleSceneUnitActor::IsEnemyUnit() const
{
	return bEnemy;
}

bool AGameXXKBattleSceneUnitActor::CanReceivePrimaryPartyAttack() const
{
	return CanReceiveTargetedBattleAction();
}

bool AGameXXKBattleSceneUnitActor::CanOpenPartyCommandMenu() const
{
	return !bEnemy && UnitIndex != INDEX_NONE && !bDefeated && CurrentHP > 0;
}

bool AGameXXKBattleSceneUnitActor::CanReceiveTargetedBattleAction() const
{
	return bEnemy && UnitIndex != INDEX_NONE && !bDefeated && CurrentHP > 0;
}

int32 AGameXXKBattleSceneUnitActor::GetUnitIndex() const
{
	return UnitIndex;
}

FName AGameXXKBattleSceneUnitActor::GetUnitId() const
{
	return UnitId;
}

UBoxComponent* AGameXXKBattleSceneUnitActor::GetHitArea() const
{
	return HitArea;
}

UTextRenderComponent* AGameXXKBattleSceneUnitActor::GetLabelTextComponent() const
{
	return LabelText;
}

UPaperFlipbookComponent* AGameXXKBattleSceneUnitActor::GetBattleVisualComponent() const
{
	return BattleVisual;
}

UPaperFlipbook* AGameXXKBattleSceneUnitActor::GetCurrentBattleFlipbook() const
{
	return BattleVisual ? BattleVisual->GetFlipbook() : nullptr;
}

void AGameXXKBattleSceneUnitActor::SetMVPSubsystemForTest(UGameXXKMVPSubsystem* InSubsystem)
{
	OverrideSubsystem = InSubsystem;
}

UGameXXKMVPSubsystem* AGameXXKBattleSceneUnitActor::ResolveMVPSubsystem(APawn* InstigatorPawn) const
{
	if (OverrideSubsystem)
	{
		return OverrideSubsystem;
	}

	UGameInstance* GameInstance = nullptr;
	if (InstigatorPawn && InstigatorPawn->GetWorld())
	{
		GameInstance = InstigatorPawn->GetWorld()->GetGameInstance();
	}
	if (!GameInstance && GetWorld())
	{
		GameInstance = GetWorld()->GetGameInstance();
	}
	return GameInstance ? GameInstance->GetSubsystem<UGameXXKMVPSubsystem>() : nullptr;
}

void AGameXXKBattleSceneUnitActor::RefreshFromRuntimeState(UGameXXKMVPSubsystem* Subsystem)
{
	if (!Subsystem)
	{
		return;
	}

	const TArray<FGameXXKBattleRuntimeUnit>& Units = bEnemy
		? Subsystem->GetRuntimeState().ActiveBattleEnemies
		: Subsystem->GetRuntimeState().ActiveBattleParty;
	if (!Units.IsValidIndex(UnitIndex))
	{
		return;
	}
	ConfigureFromRuntimeUnit(bEnemy, UnitIndex, Units[UnitIndex]);
}

void AGameXXKBattleSceneUnitActor::RefreshLabel()
{
	if (!LabelText)
	{
		return;
	}

	const FString Side = bEnemy ? TEXT("Enemy") : TEXT("Party");
	const FString DefeatedSuffix = bDefeated ? TEXT(" defeated") : TEXT("");
	LabelText->SetText(FText::FromString(FString::Printf(
		TEXT("%s %s\nHP %d/%d%s"),
		*Side,
		*UnitId.ToString(),
		CurrentHP,
		MaxHP,
		*DefeatedSuffix)));
}

void AGameXXKBattleSceneUnitActor::RefreshVisual()
{
	if (!BattleVisual)
	{
		return;
	}

	BattleVisual->SetFlipbook(ResolveBattleFlipbook());
	BattleVisual->SetSpriteColor(bEnemy ? EnemyBattleTint : PartyBattleTint);
	BattleVisual->SetTranslucentSortPriority(bEnemy ? 18 : 20);
	if (bDefeated)
	{
		BattleVisual->SetSpriteColor(FLinearColor(0.22f, 0.22f, 0.22f, 0.65f));
	}
	if (BattleVisual->GetFlipbook())
	{
		BattleVisual->PlayFromStart();
	}
}

UPaperFlipbook* AGameXXKBattleSceneUnitActor::ResolveBattleFlipbook() const
{
	if (bEnemy)
	{
		if (UnitId == TEXT("Boss"))
		{
			return TigerBossBattleFlipbookAsset.LoadSynchronous();
		}
		if (UnitId == TEXT("EliteBandit"))
		{
			return BlackBearBattleFlipbookAsset.LoadSynchronous();
		}
		if (UnitId == TEXT("Wolf"))
		{
			return NiuHuanBattleFlipbookAsset.LoadSynchronous();
		}
		if (UnitId == TEXT("Bandit"))
		{
			return MoneyMouseBattleFlipbookAsset.LoadSynchronous();
		}
		return EnemyBattleFlipbookAsset.LoadSynchronous();
	}
	if (UnitId == TEXT("Follower") || UnitIndex > 0)
	{
		return FollowerBattleFlipbookAsset.LoadSynchronous();
	}
	return HeroBattleFlipbookAsset.LoadSynchronous();
}

void AGameXXKBattleSceneUnitActor::RefreshPlayerFlowWidgets(APawn* InstigatorPawn) const
{
	AGameXXKMVPPlayerController* PlayerController = InstigatorPawn ? Cast<AGameXXKMVPPlayerController>(InstigatorPawn->GetController()) : nullptr;
	if (!PlayerController && GetWorld())
	{
		PlayerController = Cast<AGameXXKMVPPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
	}
	if (PlayerController)
	{
		PlayerController->RefreshPlayerFlowWidgetsFromState();
	}
}
