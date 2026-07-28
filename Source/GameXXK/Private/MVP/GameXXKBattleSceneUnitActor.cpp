#include "MVP/GameXXKBattleSceneUnitActor.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/GameInstance.h"
#include "GameXXKBattlePresentation.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "PaperFlipbook.h"
#include "PaperFlipbookComponent.h"
#include "UI/GameXXKBattleAnimationPresentation.h"

namespace
{
	const FSoftObjectPath HeroBattleFlipbookPath(TEXT("/Game/GameXXK/Characters/Hero/Flipbooks/FB_Hero_Idle_West.FB_Hero_Idle_West"));
	const FSoftObjectPath FollowerBattleFlipbookPath(TEXT("/Game/GameXXK/Characters/Follower/Flipbooks/FB_Npc_Idle_West.FB_Npc_Idle_West"));
	const FSoftObjectPath EnemyBattleFlipbookPath(TEXT("/Game/GameXXK/Characters/Enemies/Flipbooks/FB_Enemy_Default.FB_Enemy_Default"));
	const FSoftObjectPath MoneyMouseBattleFlipbookPath(TEXT("/Game/GameXXK/Characters/Enemies/Flipbooks/FB_Enemy_MoneyMouse.FB_Enemy_MoneyMouse"));
	const FSoftObjectPath NiuHuanBattleFlipbookPath(TEXT("/Game/GameXXK/Characters/Enemies/Flipbooks/FB_Enemy_NiuHuan.FB_Enemy_NiuHuan"));
	const FSoftObjectPath BlackBearBattleFlipbookPath(TEXT("/Game/GameXXK/Characters/Enemies/Flipbooks/FB_Enemy_BlackBear.FB_Enemy_BlackBear"));
	const FSoftObjectPath TigerBossBattleFlipbookPath(TEXT("/Game/GameXXK/Characters/Enemies/Flipbooks/FB_Boss_Tiger.FB_Boss_Tiger"));
	const FSoftObjectPath TusiChiefBattleFlipbookPath(TEXT("/Game/GameXXK/Characters/PartyDeckNPC/TusiChief/Flipbooks/FB_PartyDeckNPC_TusiChief_Idle_South.FB_PartyDeckNPC_TusiChief_Idle_South"));
	const FSoftObjectPath SongJinBaoBattleFlipbookPath(TEXT("/Game/GameXXK/Characters/PartyDeckNPC/SongJinBao/Flipbooks/FB_PartyDeckNPC_SongJinBao_Idle_South.FB_PartyDeckNPC_SongJinBao_Idle_South"));
	const FSoftObjectPath YueBaiBattleFlipbookPath(TEXT("/Game/GameXXK/Characters/PartyDeckNPC/YueBai/Flipbooks/FB_PartyDeckNPC_YueBai_Idle_South.FB_PartyDeckNPC_YueBai_Idle_South"));
	const FSoftObjectPath ZhouGuangZuBattleFlipbookPath(TEXT("/Game/GameXXK/Characters/PartyDeckNPC/ZhouGuangZu/Flipbooks/FB_PartyDeckNPC_ZhouGuangZu_Idle_South.FB_PartyDeckNPC_ZhouGuangZu_Idle_South"));
	const FSoftObjectPath JinGuiBattleFlipbookPath(TEXT("/Game/GameXXK/Characters/PartyDeckNPC/JinGui/Flipbooks/FB_PartyDeckNPC_JinGui_Idle_South.FB_PartyDeckNPC_JinGui_Idle_South"));
	const FSoftObjectPath QiongMeiErBattleFlipbookPath(TEXT("/Game/GameXXK/Characters/PartyDeckNPC/QiongMeiEr/Flipbooks/FB_PartyDeckNPC_QiongMeiEr_Idle_South.FB_PartyDeckNPC_QiongMeiEr_Idle_South"));
	const FSoftObjectPath BladeCompanionBattleFlipbookPath(TEXT("/Game/GameXXK/Characters/PartyDeckPartners/Blade/Flipbooks/FB_PartyDeckPartner_Blade_Idle_South.FB_PartyDeckPartner_Blade_Idle_South"));
	const FSoftObjectPath GuardCompanionBattleFlipbookPath(TEXT("/Game/GameXXK/Characters/PartyDeckPartners/Guard/Flipbooks/FB_PartyDeckPartner_Guard_Idle_South.FB_PartyDeckPartner_Guard_Idle_South"));
	const FSoftObjectPath HealerCompanionBattleFlipbookPath(TEXT("/Game/GameXXK/Characters/PartyDeckPartners/Healer/Flipbooks/FB_PartyDeckPartner_Healer_Idle_South.FB_PartyDeckPartner_Healer_Idle_South"));
	const FSoftObjectPath HunterCompanionBattleFlipbookPath(TEXT("/Game/GameXXK/Characters/PartyDeckPartners/Hunter/Flipbooks/FB_PartyDeckPartner_Hunter_Idle_South.FB_PartyDeckPartner_Hunter_Idle_South"));
	const FSoftObjectPath SorcererCompanionBattleFlipbookPath(TEXT("/Game/GameXXK/Characters/PartyDeckPartners/Sorcerer/Flipbooks/FB_PartyDeckPartner_Sorcerer_Idle_South.FB_PartyDeckPartner_Sorcerer_Idle_South"));
	const FSoftObjectPath FormationMasterCompanionBattleFlipbookPath(TEXT("/Game/GameXXK/Characters/PartyDeckPartners/FormationMaster/Flipbooks/FB_PartyDeckPartner_FormationMaster_Idle_South.FB_PartyDeckPartner_FormationMaster_Idle_South"));
	const FLinearColor EnemyBattleTint(1.0f, 1.0f, 1.0f, 1.0f);
	const FLinearColor PartyBattleTint(1.0f, 1.0f, 1.0f, 1.0f);
	const FLinearColor CardTargetHighlightTint(1.0f, 0.86f, 0.34f, 1.0f);
	// The fixed battle camera projects +Y toward screen-right.  These compensate
	// only for transparent/asymmetric art padding; SceneRoot and HitArea stay on
	// their canonical P-slot so targeting and fixed HUD layout remain stable.
	const FVector PartyP1VisualCenterOffset(0.0f, 24.0f, 0.0f);
	const FVector PartyP2VisualCenterOffset = FVector::ZeroVector;
	const FVector PartyP3VisualCenterOffset(0.0f, -8.0f, 0.0f);

	UPaperFlipbook* LoadPartyDeckFlipbook(const FSoftObjectPath& AssetPath)
	{
		return TSoftObjectPtr<UPaperFlipbook>(AssetPath).LoadSynchronous();
	}

	UPaperFlipbook* ResolveNamedTaskNpcFlipbook(FName RuntimeUnitId)
	{
		if (RuntimeUnitId == TEXT("Npc.TusiChief"))
		{
			return LoadPartyDeckFlipbook(TusiChiefBattleFlipbookPath);
		}
		if (RuntimeUnitId == TEXT("Npc.SongJinBao"))
		{
			return LoadPartyDeckFlipbook(SongJinBaoBattleFlipbookPath);
		}
		if (RuntimeUnitId == TEXT("Npc.YueBai"))
		{
			return LoadPartyDeckFlipbook(YueBaiBattleFlipbookPath);
		}
		if (RuntimeUnitId == TEXT("Npc.ZhouGuangZu"))
		{
			return LoadPartyDeckFlipbook(ZhouGuangZuBattleFlipbookPath);
		}
		if (RuntimeUnitId == TEXT("Npc.JinGui"))
		{
			return LoadPartyDeckFlipbook(JinGuiBattleFlipbookPath);
		}
		if (RuntimeUnitId == TEXT("Npc.QiongMeiEr"))
		{
			return LoadPartyDeckFlipbook(QiongMeiErBattleFlipbookPath);
		}
		return nullptr;
	}

	UPaperFlipbook* ResolvePersistentCompanionFlipbook(FName RuntimeUnitId)
	{
		const FString RuntimeId = RuntimeUnitId.ToString();
		if (RuntimeId.StartsWith(TEXT("CompanionInstance.Companion_Blade_")))
		{
			return LoadPartyDeckFlipbook(BladeCompanionBattleFlipbookPath);
		}
		if (RuntimeId.StartsWith(TEXT("CompanionInstance.Companion_Guard_")))
		{
			return LoadPartyDeckFlipbook(GuardCompanionBattleFlipbookPath);
		}
		if (RuntimeId.StartsWith(TEXT("CompanionInstance.Companion_Healer_")))
		{
			return LoadPartyDeckFlipbook(HealerCompanionBattleFlipbookPath);
		}
		if (RuntimeId.StartsWith(TEXT("CompanionInstance.Companion_Hunter_")))
		{
			return LoadPartyDeckFlipbook(HunterCompanionBattleFlipbookPath);
		}
		if (RuntimeId.StartsWith(TEXT("CompanionInstance.Companion_Sorcerer_")))
		{
			return LoadPartyDeckFlipbook(SorcererCompanionBattleFlipbookPath);
		}
		if (RuntimeId.StartsWith(TEXT("CompanionInstance.Companion_FormationMaster_")))
		{
			return LoadPartyDeckFlipbook(FormationMasterCompanionBattleFlipbookPath);
		}
		return nullptr;
	}

}

AGameXXKBattleSceneUnitActor::AGameXXKBattleSceneUnitActor()
{
	PrimaryActorTick.bCanEverTick = true;
	SetActorTickEnabled(false);

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

	HeroBattleFlipbookAsset = TSoftObjectPtr<UPaperFlipbook>(HeroBattleFlipbookPath);
	FollowerBattleFlipbookAsset = TSoftObjectPtr<UPaperFlipbook>(FollowerBattleFlipbookPath);
	EnemyBattleFlipbookAsset = TSoftObjectPtr<UPaperFlipbook>(EnemyBattleFlipbookPath);
	MoneyMouseBattleFlipbookAsset = TSoftObjectPtr<UPaperFlipbook>(MoneyMouseBattleFlipbookPath);
	NiuHuanBattleFlipbookAsset = TSoftObjectPtr<UPaperFlipbook>(NiuHuanBattleFlipbookPath);
	BlackBearBattleFlipbookAsset = TSoftObjectPtr<UPaperFlipbook>(BlackBearBattleFlipbookPath);
	TigerBossBattleFlipbookAsset = TSoftObjectPtr<UPaperFlipbook>(TigerBossBattleFlipbookPath);
}

void AGameXXKBattleSceneUnitActor::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!BattleVisual)
	{
		SetActorTickEnabled(false);
		return;
	}
	if (!bFeedbackActive)
	{
		SetActorTickEnabled(false);
		return;
	}

	FeedbackElapsed += FMath::Max(0.0f, DeltaSeconds);
	const float Alpha = FMath::Clamp(FeedbackElapsed / 0.18f, 0.0f, 1.0f);
	const float LocalXOffset = bFeedbackIsAttack
		? (Alpha < 0.34f ? 6.0f : Alpha < 0.67f ? -6.0f : Alpha < 0.84f ? 3.0f : 0.0f)
		: (Alpha < 0.34f ? -6.0f : Alpha < 0.67f ? 6.0f : Alpha < 0.84f ? -3.0f : 0.0f);
	BattleVisual->SetRelativeLocation(FeedbackBaseLocation + FVector(LocalXOffset, 0.0f, 0.0f));
	BattleVisual->SetRelativeScale3D(FeedbackBaseScale);
	if (Alpha >= 1.0f)
	{
		RestoreFeedbackVisual();
	}
}

void AGameXXKBattleSceneUnitActor::ConfigureFromRuntimeUnit(const bool bInEnemy, const int32 InUnitIndex, const FGameXXKBattleRuntimeUnit& Unit, const int32 InSlotNumber)
{
	if (UnitId != Unit.Id)
	{
		bCardTargetHighlighted = false;
	}
	bEnemy = bInEnemy;
	UnitIndex = InUnitIndex;
	UnitId = Unit.Id;
	SlotNumber = InSlotNumber;
	bFeedbackActive = false;
	FeedbackElapsed = 0.0f;
	SetActorTickEnabled(false);
	ResolveCardRuntimePresentation(Unit);
	ApplyBattleVisualSlotOffset();
	RefreshVisual();
}

void AGameXXKBattleSceneUnitActor::PlayIntentAttackFeedback()
{
	BeginFeedback(true);
}

void AGameXXKBattleSceneUnitActor::PlayHitFeedback()
{
	BeginFeedback(false);
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

UPaperFlipbookComponent* AGameXXKBattleSceneUnitActor::GetBattleVisualComponent() const
{
	return BattleVisual;
}

FVector AGameXXKBattleSceneUnitActor::GetBattleHudProjectionWorldLocation() const
{
	if (!BattleVisual)
	{
		return GetActorLocation();
	}

	return BattleVisual->Bounds.Origin - FVector(0.0f, 0.0f, BattleVisual->Bounds.BoxExtent.Z);
}

UPaperFlipbook* AGameXXKBattleSceneUnitActor::GetCurrentBattleFlipbook() const
{
	return BattleVisual ? BattleVisual->GetFlipbook() : nullptr;
}

int32 AGameXXKBattleSceneUnitActor::GetSlotNumberForTest() const
{
	return SlotNumber;
}

int32 AGameXXKBattleSceneUnitActor::GetCurrentHealthForTest() const
{
	return CurrentHP;
}

int32 AGameXXKBattleSceneUnitActor::GetMaxHealthForTest() const
{
	return MaxHP;
}

void AGameXXKBattleSceneUnitActor::SetCardTargetHighlight(bool bHighlighted)
{
	const bool bDesiredHighlight = bHighlighted && !bDefeated && CurrentHP > 0;
	if (bCardTargetHighlighted == bDesiredHighlight)
	{
		return;
	}

	bCardTargetHighlighted = bDesiredHighlight;
	RefreshVisual();
}

bool AGameXXKBattleSceneUnitActor::IsCardTargetHighlighted() const
{
	return bCardTargetHighlighted;
}

bool AGameXXKBattleSceneUnitActor::IsCardTargetOutlineEnabled() const
{
	return bCardTargetHighlighted && BattleVisual && BattleVisual->bRenderCustomDepth;
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

FVector AGameXXKBattleSceneUnitActor::ResolveBattleVisualSlotOffset() const
{
	if (bEnemy)
	{
		return FVector::ZeroVector;
	}

	switch (SlotNumber)
	{
	case 1:
		return PartyP1VisualCenterOffset;
	case 2:
		return PartyP2VisualCenterOffset;
	case 3:
		return PartyP3VisualCenterOffset;
	default:
		return FVector::ZeroVector;
	}
}

void AGameXXKBattleSceneUnitActor::ApplyBattleVisualSlotOffset()
{
	if (!BattleVisual)
	{
		return;
	}

	if (!bHasBattleVisualAuthoredBaseLocation)
	{
		BattleVisualAuthoredBaseLocation = BattleVisual->GetRelativeLocation();
		bHasBattleVisualAuthoredBaseLocation = true;
	}

	BattleVisual->SetRelativeLocation(BattleVisualAuthoredBaseLocation + ResolveBattleVisualSlotOffset());
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
	BattleVisual->SetRenderCustomDepth(bCardTargetHighlighted && !bDefeated);
	BattleVisual->SetCustomDepthStencilValue(252);
	if (bCardTargetHighlighted && !bDefeated)
	{
		BattleVisual->SetSpriteColor(CardTargetHighlightTint);
	}
	if (bDefeated)
	{
		BattleVisual->SetSpriteColor(FLinearColor(0.22f, 0.22f, 0.22f, 0.65f));
	}
	if (BattleVisual->GetFlipbook())
	{
		BattleVisual->PlayFromStart();
	}
}

void AGameXXKBattleSceneUnitActor::ResolveCardRuntimePresentation(const FGameXXKBattleRuntimeUnit& LegacyUnit)
{
	MaxHP = FMath::Max(1, LegacyUnit.MaxHP);
	CurrentHP = FMath::Clamp(LegacyUnit.HP, 0, MaxHP);
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
			bDefeated = !CardUnit->bLiving;
			if (SlotNumber == INDEX_NONE)
			{
				SlotNumber = FGameXXKBattlePresentation::GetSlotNumber(Runtime, UnitId);
			}
		}
	}

}

void AGameXXKBattleSceneUnitActor::BeginFeedback(const bool bInAttackFeedback)
{
	if (!BattleVisual)
	{
		return;
	}
	FeedbackBaseLocation = BattleVisual->GetRelativeLocation();
	FeedbackBaseScale = BattleVisual->GetRelativeScale3D();
	bFeedbackIsAttack = bInAttackFeedback;
	bFeedbackActive = true;
	FeedbackElapsed = 0.0f;
	SetActorTickEnabled(true);
}

void AGameXXKBattleSceneUnitActor::RestoreFeedbackVisual()
{
	if (BattleVisual)
	{
		BattleVisual->SetRelativeLocation(FeedbackBaseLocation);
		BattleVisual->SetRelativeScale3D(FeedbackBaseScale);
	}
	bFeedbackActive = false;
	FeedbackElapsed = 0.0f;
	SetActorTickEnabled(false);
}

UPaperFlipbook* AGameXXKBattleSceneUnitActor::ResolveBattleFlipbook() const
{
	const FSoftObjectPath ProductionIdlePath = FGameXXKBattleAnimationPresentation::ResolveIdleFlipbookPath(UnitId, bEnemy);
	if (UPaperFlipbook* ProductionIdle = TSoftObjectPtr<UPaperFlipbook>(ProductionIdlePath).LoadSynchronous())
	{
		return ProductionIdle;
	}

	if (bEnemy)
	{
		if (UnitId == TEXT("Tiger") || UnitId == TEXT("Boss"))
		{
			return TigerBossBattleFlipbookAsset.LoadSynchronous();
		}
		if (UnitId == TEXT("BlackBear") || UnitId == TEXT("EliteBandit"))
		{
			return BlackBearBattleFlipbookAsset.LoadSynchronous();
		}
		if (UnitId == TEXT("MoneyRat") || UnitId == TEXT("Bandit"))
		{
			return MoneyMouseBattleFlipbookAsset.LoadSynchronous();
		}
		return EnemyBattleFlipbookAsset.LoadSynchronous();
	}
	if (UPaperFlipbook* NamedTaskNpcFlipbook = ResolveNamedTaskNpcFlipbook(UnitId))
	{
		return NamedTaskNpcFlipbook;
	}
	if (UPaperFlipbook* PersistentCompanionFlipbook = ResolvePersistentCompanionFlipbook(UnitId))
	{
		return PersistentCompanionFlipbook;
	}
	if (UnitId == TEXT("Follower") || UnitIndex > 0)
	{
		return FollowerBattleFlipbookAsset.LoadSynchronous();
	}
	return HeroBattleFlipbookAsset.LoadSynchronous();
}
