#include "MVP/GameXXKBattleSceneUnitActor.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "MVP/GameXXKLevelFlow.h"
#include "MVP/GameXXKMVPPlayerController.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKBattleUnitResourceWidget.h"
#include "UI/GameXXKBattleUnitStatusEffectsWidget.h"
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

	int32 ResolveDisplaySlot(const FGameXXKCardCombatUnit& Unit, const int32 FallbackIndex)
	{
		if (Unit.Side == EGameXXKCardTargetSide::Party)
		{
			if (Unit.Role == EGameXXKCharacterRole::Hero)
			{
				return 1;
			}
			if (Unit.Role == EGameXXKCharacterRole::QuestNpc)
			{
				return 3;
			}
			if (Unit.Role != EGameXXKCharacterRole::Invalid)
			{
				return 2;
			}
		}

		if (Unit.Side == EGameXXKCardTargetSide::Enemy && Unit.StableSortOrder >= 0 && Unit.StableSortOrder <= 2)
		{
			return Unit.StableSortOrder + 1;
		}

		return FMath::Clamp(FallbackIndex + 1, 1, 3);
	}

	bool AreStatusStacksEquivalent(const TArray<FGameXXKCardStatusStack>& Left, const TArray<FGameXXKCardStatusStack>& Right)
	{
		if (Left.Num() != Right.Num())
		{
			return false;
		}

		for (int32 Index = 0; Index < Left.Num(); ++Index)
		{
			if (Left[Index].Status != Right[Index].Status || Left[Index].Stacks != Right[Index].Stacks)
			{
				return false;
			}
		}

		return true;
	}
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

	HudAnchorComponent = CreateDefaultSubobject<USceneComponent>(TEXT("HudAnchor"));
	HudAnchorComponent->SetupAttachment(BattleVisual);

	ResourceHudAnchorComponent = CreateDefaultSubobject<USceneComponent>(TEXT("ResourceHudAnchor"));
	ResourceHudAnchorComponent->SetupAttachment(HudAnchorComponent);

	StatusEffectsAnchorComponent = CreateDefaultSubobject<USceneComponent>(TEXT("StatusEffectsAnchor"));
	StatusEffectsAnchorComponent->SetupAttachment(HudAnchorComponent);

	ResourceHudWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("ResourceHudWidget"));
	ResourceHudWidgetComponent->SetupAttachment(ResourceHudAnchorComponent);
	ResourceHudWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
	ResourceHudWidgetComponent->SetDrawSize(FIntPoint(300, 96));
	ResourceHudWidgetComponent->SetPivot(FVector2D(0.5f, 1.0f));
	ResourceHudWidgetComponent->SetTwoSided(false);
	ResourceHudWidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ResourceHudWidgetComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	ResourceHudWidgetComponent->SetGenerateOverlapEvents(false);
	ResourceHudWidgetComponent->SetWidgetClass(UGameXXKBattleUnitResourceWidget::StaticClass());
	ResourceHudWidgetComponent->SetVisibility(false);

	StatusEffectsWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("StatusEffectsWidget"));
	StatusEffectsWidgetComponent->SetupAttachment(StatusEffectsAnchorComponent);
	StatusEffectsWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
	StatusEffectsWidgetComponent->SetDrawSize(FIntPoint(300, 46));
	StatusEffectsWidgetComponent->SetPivot(FVector2D(0.5f, 0.0f));
	StatusEffectsWidgetComponent->SetTwoSided(false);
	StatusEffectsWidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StatusEffectsWidgetComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	StatusEffectsWidgetComponent->SetGenerateOverlapEvents(false);
	StatusEffectsWidgetComponent->SetWidgetClass(UGameXXKBattleUnitStatusEffectsWidget::StaticClass());
	StatusEffectsWidgetComponent->SetVisibility(false);

	HeroBattleFlipbookAsset = TSoftObjectPtr<UPaperFlipbook>(HeroBattleFlipbookPath);
	FollowerBattleFlipbookAsset = TSoftObjectPtr<UPaperFlipbook>(FollowerBattleFlipbookPath);
	EnemyBattleFlipbookAsset = TSoftObjectPtr<UPaperFlipbook>(EnemyBattleFlipbookPath);
	MoneyMouseBattleFlipbookAsset = TSoftObjectPtr<UPaperFlipbook>(MoneyMouseBattleFlipbookPath);
	NiuHuanBattleFlipbookAsset = TSoftObjectPtr<UPaperFlipbook>(NiuHuanBattleFlipbookPath);
	BlackBearBattleFlipbookAsset = TSoftObjectPtr<UPaperFlipbook>(BlackBearBattleFlipbookPath);
	TigerBossBattleFlipbookAsset = TSoftObjectPtr<UPaperFlipbook>(TigerBossBattleFlipbookPath);
	RefreshHudAnchor();
}

void AGameXXKBattleSceneUnitActor::ConfigureFromRuntimeUnit(bool bInEnemy, int32 InUnitIndex, const FGameXXKBattleRuntimeUnit& Unit)
{
	bEnemy = bInEnemy;
	UnitIndex = InUnitIndex;
	UnitId = Unit.Id;
	SlotNumber = FMath::Clamp(InUnitIndex + 1, 1, 3);
	DisplayName = Unit.DisplayName.IsEmpty() ? FText::FromName(Unit.Id) : Unit.DisplayName;
	ResolveCardRuntimePresentation(Unit);
	RefreshLabel();
	RefreshVisual();
	RefreshResourceHudWidget();
	RefreshStatusEffectsWidget();
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

USceneComponent* AGameXXKBattleSceneUnitActor::GetHudAnchorComponentForTest() const
{
	return HudAnchorComponent;
}

USceneComponent* AGameXXKBattleSceneUnitActor::GetResourceHudAnchorComponentForTest() const
{
	return ResourceHudAnchorComponent;
}

USceneComponent* AGameXXKBattleSceneUnitActor::GetStatusEffectsAnchorComponentForTest() const
{
	return StatusEffectsAnchorComponent;
}

UWidgetComponent* AGameXXKBattleSceneUnitActor::GetResourceHudWidgetComponentForTest() const
{
	return ResourceHudWidgetComponent;
}

UWidgetComponent* AGameXXKBattleSceneUnitActor::GetStatusEffectsWidgetComponentForTest() const
{
	return StatusEffectsWidgetComponent;
}

int32 AGameXXKBattleSceneUnitActor::GetSlotNumberForTest() const
{
	return SlotNumber;
}

int32 AGameXXKBattleSceneUnitActor::GetArmorForTest() const
{
	return CurrentArmor;
}

int32 AGameXXKBattleSceneUnitActor::GetCurrentHealthForTest() const
{
	return CurrentHP;
}

int32 AGameXXKBattleSceneUnitActor::GetMaxHealthForTest() const
{
	return MaxHP;
}

int32 AGameXXKBattleSceneUnitActor::GetCurrentManaForTest() const
{
	return CurrentMana;
}

int32 AGameXXKBattleSceneUnitActor::GetMaxManaForTest() const
{
	return MaxMana;
}

bool AGameXXKBattleSceneUnitActor::ShouldShowQiForTest() const
{
	return bShowQi;
}

int32 AGameXXKBattleSceneUnitActor::GetResourcePresentationGenerationForTest() const
{
	return ResourcePresentationGeneration;
}

int32 AGameXXKBattleSceneUnitActor::GetStatusEffectsPresentationGenerationForTest() const
{
	return StatusEffectsPresentationGeneration;
}

FString AGameXXKBattleSceneUnitActor::GetStatusTextForTest() const
{
	return UGameXXKBattleUnitStatusEffectsWidget::BuildStatusText(CurrentStatuses);
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
	RefreshHudAnchor();
}

void AGameXXKBattleSceneUnitActor::RefreshHudAnchor()
{
	if (!HudAnchorComponent || !BattleVisual)
	{
		return;
	}

	const FBoxSphereBounds VisualBounds = BattleVisual->Bounds;
	FVector Foot = VisualBounds.Origin - FVector(0.0f, 0.0f, VisualBounds.BoxExtent.Z);
	if (VisualBounds.SphereRadius <= KINDA_SMALL_NUMBER)
	{
		Foot = BattleVisual->GetComponentLocation();
	}
	if (Foot.ContainsNaN())
	{
		return;
	}

	HudAnchorComponent->SetWorldLocation(Foot + FVector(0.0f, 0.0f, 8.0f));
}

void AGameXXKBattleSceneUnitActor::RefreshResourceHudWidget()
{
	if (!ResourceHudWidgetComponent)
	{
		return;
	}

	const bool bHudVisible = !bDefeated && CurrentHP > 0;
	const bool bPresentationChanged = !bHasResourcePresentation
		|| LastResourceCurrentHP != CurrentHP
		|| LastResourceMaxHP != MaxHP
		|| LastResourceCurrentMana != CurrentMana
		|| LastResourceMaxMana != MaxMana
		|| LastResourceSlotNumber != SlotNumber
		|| !LastResourceDisplayName.EqualTo(DisplayName)
		|| bLastResourceShowQi != bShowQi;
	if (bPresentationChanged)
	{
		bHasResourcePresentation = true;
		LastResourceCurrentHP = CurrentHP;
		LastResourceMaxHP = MaxHP;
		LastResourceCurrentMana = CurrentMana;
		LastResourceMaxMana = MaxMana;
		LastResourceSlotNumber = SlotNumber;
		LastResourceDisplayName = DisplayName;
		bLastResourceShowQi = bShowQi;
		++ResourcePresentationGeneration;
	}

	ResourceHudWidgetComponent->SetVisibility(bHudVisible, true);
	if (!GetWorld())
	{
		return;
	}

	ResourceHudWidgetComponent->InitWidget();
	if (UGameXXKBattleUnitResourceWidget* ResourceWidget = Cast<UGameXXKBattleUnitResourceWidget>(ResourceHudWidgetComponent->GetUserWidgetObject()))
	{
		if (!bHudVisible)
		{
			ResourceWidget->SetVisibility(ESlateVisibility::Collapsed);
			return;
		}

		const FString SlotLabel = SlotNumber == INDEX_NONE
			? FString()
			: FString::Printf(bEnemy ? TEXT("敌 %dP") : TEXT("我 %dP"), SlotNumber);
		ResourceWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		ResourceWidget->SetUnitResources(SlotLabel, DisplayName, CurrentHP, MaxHP, CurrentMana, MaxMana, bShowQi);
	}
}

void AGameXXKBattleSceneUnitActor::RefreshStatusEffectsWidget()
{
	if (!StatusEffectsWidgetComponent)
	{
		return;
	}

	const bool bHudVisible = !bDefeated && CurrentHP > 0;
	const bool bPresentationChanged = !bHasStatusEffectsPresentation
		|| LastStatusEffectsArmor != CurrentArmor
		|| !AreStatusStacksEquivalent(LastStatusEffectsStatuses, CurrentStatuses);
	if (bPresentationChanged)
	{
		bHasStatusEffectsPresentation = true;
		LastStatusEffectsArmor = CurrentArmor;
		LastStatusEffectsStatuses = CurrentStatuses;
		++StatusEffectsPresentationGeneration;
	}

	StatusEffectsWidgetComponent->SetVisibility(bHudVisible, true);
	if (!GetWorld())
	{
		return;
	}

	StatusEffectsWidgetComponent->InitWidget();
	if (UGameXXKBattleUnitStatusEffectsWidget* StatusEffectsWidget = Cast<UGameXXKBattleUnitStatusEffectsWidget>(StatusEffectsWidgetComponent->GetUserWidgetObject()))
	{
		if (!bHudVisible)
		{
			StatusEffectsWidget->SetVisibility(ESlateVisibility::Collapsed);
			return;
		}

		StatusEffectsWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		StatusEffectsWidget->SetStatusEffects(CurrentArmor, CurrentStatuses);
	}
}

void AGameXXKBattleSceneUnitActor::ResolveCardRuntimePresentation(const FGameXXKBattleRuntimeUnit& LegacyUnit)
{
	MaxHP = FMath::Max(1, LegacyUnit.MaxHP);
	CurrentHP = FMath::Clamp(LegacyUnit.HP, 0, MaxHP);
	MaxMana = FMath::Max(0, LegacyUnit.MaxMP);
	CurrentMana = FMath::Clamp(LegacyUnit.MP, 0, MaxMana);
	CurrentArmor = FMath::Max(0, LegacyUnit.Shield);
	CurrentStatuses.Reset();
	bDefeated = LegacyUnit.bDefeated;

	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem(nullptr);
	if (Subsystem && Subsystem->GetRuntimeState().CardRun.bHasActiveCardBattle)
	{
		const FGameXXKCardBattleRuntime& Runtime = Subsystem->GetRuntimeState().CardRun.ActiveBattle;
		const FGameXXKCardCombatUnit* CardUnit = Runtime.Units.FindByPredicate([this](const FGameXXKCardCombatUnit& Candidate)
		{
			return Candidate.UnitId == UnitId;
		});
		if (CardUnit)
		{
			MaxHP = FMath::Max(1, CardUnit->MaxHP);
			CurrentHP = FMath::Clamp(CardUnit->HP, 0, MaxHP);
			MaxMana = FMath::Max(0, CardUnit->MaxMana);
			CurrentMana = FMath::Clamp(CardUnit->Mana, 0, MaxMana);
			CurrentArmor = FMath::Clamp(CardUnit->Armor, 0, 99);
			CurrentStatuses = CardUnit->Statuses;
			bDefeated = !CardUnit->bLiving;
			SlotNumber = ResolveDisplaySlot(*CardUnit, UnitIndex);
		}
	}

	bShowQi = !bEnemy && MaxMana > 0;
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
