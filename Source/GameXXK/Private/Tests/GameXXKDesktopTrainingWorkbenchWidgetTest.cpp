#include "GameXXKTrainingRules.h"
#include "GameXXKDesktopInventoryRules.h"
#include "GameXXKEquipmentRules.h"
#include "GameXXKMVPRules.h"
#include "GameXXKPartyFormationRules.h"
#include "GameXXKPermanentPartyTestFixtures.h"
#include "IGameXXKDesktopOverlayModule.h"
#include "MVP/GameXXKMVPPlayerController.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKDesktopTrainingLayout.h"
#include "UI/GameXXKDesktopHudSessionSubsystem.h"
#include "UI/GameXXKDesktopWorkbenchSessionState.h"
#include "UI/GameXXKBattleAnimationPresentation.h"
#include "UI/GameXXKBattleAtlasCache.h"
#include "UI/GameXXKDesktopTrainingWorkbenchWidget.h"
#include "UI/GameXXKInventoryWindowWidget.h"

#include "Engine/GameInstance.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "HAL/PlatformTime.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/PackageName.h"
#include "Misc/ScopeExit.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SWindow.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	void UnlockToolsTalent(FGameXXKRuntimeState& State)
	{
		State.Talents.NodeRanks.Add(TEXT("Talent.Root"), 1);
		State.Talents.NodeRanks.Add(TEXT("Talent.Entry.Tools"), 1);
	}

	void UnlockOfflineTalent(FGameXXKRuntimeState& State)
	{
		State.Talents.NodeRanks.Add(TEXT("Talent.Root"), 1);
		State.Talents.NodeRanks.Add(TEXT("Talent.Entry.IdleOffline"), 1);
	}

	void PreserveLegacyWorkbenchCapacity(FGameXXKRuntimeState& State)
	{
		State.Talents.MinimumBackpackCapacity = 200;
	}

	FString GetButtonNormalResourcePath(const UButton* Button)
	{
		const UObject* Resource = Button ? Button->GetStyle().Normal.GetResourceObject() : nullptr;
		return Resource ? Resource->GetPathName() : FString();
	}

	FString GetBorderResourcePath(const UBorder* Border)
	{
		const UObject* Resource = Border ? Border->Background.GetResourceObject() : nullptr;
		return Resource ? Resource->GetPathName() : FString();
	}

	FString GetImageResourcePath(const UImage* Image)
	{
		const UObject* Resource = Image ? Image->GetBrush().GetResourceObject() : nullptr;
		return Resource ? Resource->GetPathName() : FString();
	}

	struct FExpectedOwnerIdlePresentation
	{
		const TCHAR* OwnerToken;
		bool bExactOwnerId;
		const TCHAR* AssetId;
		const TCHAR* TexturePath;
	};

	const FExpectedOwnerIdlePresentation ExpectedNonHeroIdlePresentations[] = {
		{TEXT("Companion_Blade_"), false,
			TEXT("character_01_blade_2k_idle"),
			TEXT("/Game/GameXXK/BattleAnimations/Atlases/T_character_01_blade_2k_idle_atlas.T_character_01_blade_2k_idle_atlas")},
		{TEXT("Companion_Guard_"), false,
			TEXT("character_02_guard_2k_idle"),
			TEXT("/Game/GameXXK/BattleAnimations/Atlases/T_character_02_guard_2k_idle_atlas.T_character_02_guard_2k_idle_atlas")},
		{TEXT("Companion_Healer_"), false,
			TEXT("character_03_healer_2k_idle"),
			TEXT("/Game/GameXXK/BattleAnimations/Atlases/T_character_03_healer_2k_idle_atlas.T_character_03_healer_2k_idle_atlas")},
		{TEXT("Companion_Hunter_"), false,
			TEXT("character_04_hunter_2k_idle"),
			TEXT("/Game/GameXXK/BattleAnimations/Atlases/T_character_04_hunter_2k_idle_atlas.T_character_04_hunter_2k_idle_atlas")},
		{TEXT("Companion_Sorcerer_"), false,
			TEXT("character_05_sorcerer_2k_idle"),
			TEXT("/Game/GameXXK/BattleAnimations/Atlases/T_character_05_sorcerer_2k_idle_atlas.T_character_05_sorcerer_2k_idle_atlas")},
		{TEXT("Companion_FormationMaster_"), false,
			TEXT("character_06_formation_master_2k_idle"),
			TEXT("/Game/GameXXK/BattleAnimations/Atlases/T_character_06_formation_master_2k_idle_atlas.T_character_06_formation_master_2k_idle_atlas")},
		{TEXT("Npc.TusiChief"), true,
			TEXT("character_07_tusi_chief_2k_idle"),
			TEXT("/Game/GameXXK/BattleAnimations/Atlases/T_character_07_tusi_chief_2k_idle_atlas.T_character_07_tusi_chief_2k_idle_atlas")},
		{TEXT("Npc.SongJinBao"), true,
			TEXT("character_08_song_jin_bao_2k_idle"),
			TEXT("/Game/GameXXK/BattleAnimations/Atlases/T_character_08_song_jin_bao_2k_idle_atlas.T_character_08_song_jin_bao_2k_idle_atlas")},
		{TEXT("Npc.YueBai"), true,
			TEXT("character_09_yue_bai_2k_idle"),
			TEXT("/Game/GameXXK/BattleAnimations/Atlases/T_character_09_yue_bai_2k_idle_atlas.T_character_09_yue_bai_2k_idle_atlas")},
		{TEXT("Npc.ZhouGuangZu"), true,
			TEXT("character_10_zhou_guang_zu_2k_idle"),
			TEXT("/Game/GameXXK/BattleAnimations/Atlases/T_character_10_zhou_guang_zu_2k_idle_atlas.T_character_10_zhou_guang_zu_2k_idle_atlas")},
		{TEXT("Npc.JinGui"), true,
			TEXT("character_11_jin_gui_2k_idle"),
			TEXT("/Game/GameXXK/BattleAnimations/Atlases/T_character_11_jin_gui_2k_idle_atlas.T_character_11_jin_gui_2k_idle_atlas")},
		{TEXT("Npc.QiongMeiEr"), true,
			TEXT("character_12_qiong_mei_er_2k_idle"),
			TEXT("/Game/GameXXK/BattleAnimations/Atlases/T_character_12_qiong_mei_er_2k_idle_atlas.T_character_12_qiong_mei_er_2k_idle_atlas")}};

	const FExpectedOwnerIdlePresentation* FindExpectedOwnerIdlePresentation(
		const FName OwnerId)
	{
		const FString RuntimeOwnerId = OwnerId.ToString();
		for (const FExpectedOwnerIdlePresentation& Expected :
			ExpectedNonHeroIdlePresentations)
		{
			const bool bMatches = Expected.bExactOwnerId
				? RuntimeOwnerId == Expected.OwnerToken
				: RuntimeOwnerId.Contains(Expected.OwnerToken);
			if (bMatches)
			{
				return &Expected;
			}
		}
		return nullptr;
	}

	const FString ExpectedHeroCentralTexturePath(
		TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_HeroFullBody.T_MasterV2_HeroFullBody"));
	const FBox2f ExpectedIdleFrameZeroUv(
		FVector2f(0.0f, 0.0f),
		FVector2f(0.125f, 0.125f));

	class FTravelFallbackAtlasLoadHandle final : public IGameXXKBattleAtlasLoadHandle
	{
	public:
		virtual void Cancel() override { bCancelled = true; }

		bool bCancelled = false;
	};

	class FTravelFallbackAtlasLoader final : public IGameXXKBattleAtlasLoader
	{
	public:
		virtual TSharedPtr<IGameXXKBattleAtlasLoadHandle> RequestAsyncLoad(
			const FSoftObjectPath& Path,
			FGameXXKAtlasLoaderCompletion Completion) override
		{
			RequestedPaths.Add(Path);
			TSharedRef<FTravelFallbackAtlasLoadHandle> Handle = MakeShared<FTravelFallbackAtlasLoadHandle>();
			if (SynchronousMissingPaths.Contains(Path))
			{
				++CompletionDispatchCounts.FindOrAdd(Path);
				Completion(nullptr, 0);
				return Handle;
			}
			if (UTexture2D* const* SynchronousTexture = SynchronousLoadedTextures.Find(Path))
			{
				++CompletionDispatchCounts.FindOrAdd(Path);
				Completion(*SynchronousTexture, 4);
				return Handle;
			}
			PendingCompletions.Add(Path, MoveTemp(Completion));
			return Handle;
		}

		void SetSynchronousMissing(const FSoftObjectPath& Path)
		{
			SynchronousMissingPaths.Add(Path);
		}

		UTexture2D* SetSynchronousLoaded(const FSoftObjectPath& Path)
		{
			UTexture2D* Texture = NewObject<UTexture2D>(GetTransientPackage());
			LoadedTextures.Add(TStrongObjectPtr<UTexture2D>(Texture));
			SynchronousLoadedTextures.Add(Path, Texture);
			return Texture;
		}

		bool Requested(const FSoftObjectPath& Path) const
		{
			return RequestedPaths.Contains(Path);
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

		int32 CompletionDispatchCount(const FSoftObjectPath& Path) const
		{
			return CompletionDispatchCounts.FindRef(Path);
		}

		bool HasPendingCompletion(const FSoftObjectPath& Path) const
		{
			return PendingCompletions.Contains(Path);
		}

		bool CompleteMissing(const FSoftObjectPath& Path)
		{
			FGameXXKAtlasLoaderCompletion* Pending = PendingCompletions.Find(Path);
			if (!Pending)
			{
				return false;
			}
			FGameXXKAtlasLoaderCompletion Completion = MoveTemp(*Pending);
			PendingCompletions.Remove(Path);
			++CompletionDispatchCounts.FindOrAdd(Path);
			Completion(nullptr, 0);
			return true;
		}

		UTexture2D* CompleteLoaded(const FSoftObjectPath& Path)
		{
			FGameXXKAtlasLoaderCompletion* Pending = PendingCompletions.Find(Path);
			if (!Pending)
			{
				return nullptr;
			}
			UTexture2D* Texture = NewObject<UTexture2D>(GetTransientPackage());
			LoadedTextures.Add(TStrongObjectPtr<UTexture2D>(Texture));
			FGameXXKAtlasLoaderCompletion Completion = MoveTemp(*Pending);
			PendingCompletions.Remove(Path);
			++CompletionDispatchCounts.FindOrAdd(Path);
			Completion(Texture, 4);
			return Texture;
		}

	private:
		TArray<FSoftObjectPath> RequestedPaths;
		TMap<FSoftObjectPath, FGameXXKAtlasLoaderCompletion> PendingCompletions;
		TMap<FSoftObjectPath, int32> CompletionDispatchCounts;
		TSet<FSoftObjectPath> SynchronousMissingPaths;
		TMap<FSoftObjectPath, UTexture2D*> SynchronousLoadedTextures;
		TArray<TStrongObjectPtr<UTexture2D>> LoadedTextures;
	};

	template <typename Tag, typename Tag::Type Member>
	struct TTravelPrivateMemberAccessor
	{
		friend typename Tag::Type GetTravelPrivateMember(Tag)
		{
			return Member;
		}
	};

	struct FTravelLoadedAtlasTexturesTag
	{
		using Type = TMap<FSoftObjectPath, TWeakObjectPtr<UTexture2D>>
			UGameXXKDesktopTrainingWorkbenchWidget::*;
		friend Type GetTravelPrivateMember(FTravelLoadedAtlasTexturesTag);
	};
	template struct TTravelPrivateMemberAccessor<
		FTravelLoadedAtlasTexturesTag,
		&UGameXXKDesktopTrainingWorkbenchWidget::TravelLoadedAtlasTextures>;

	struct FApplyTravelClipPairTag
	{
		using Type = bool (UGameXXKDesktopTrainingWorkbenchWidget::*)(
			UImage*,
			const FGameXXKBattleAnimationClipPair&,
			bool,
			FSoftObjectPath&,
			int32&);
		friend Type GetTravelPrivateMember(FApplyTravelClipPairTag);
	};
	template struct TTravelPrivateMemberAccessor<
		FApplyTravelClipPairTag,
		static_cast<FApplyTravelClipPairTag::Type>(
			&UGameXXKDesktopTrainingWorkbenchWidget::ApplyTravelAnimationFrame)>;

	struct FTravelAtlasWidgetFixture
	{
		UGameInstance* GameInstance = nullptr;
		UGameXXKMVPSubsystem* Subsystem = nullptr;
		UGameXXKDesktopTrainingWorkbenchWidget* Widget = nullptr;
		UImage* PermanentImage = nullptr;
	};

	bool BuildTravelAtlasWidgetFixture(
		const FName PermanentUnitId,
		const FName QuestNpcId,
		TUniquePtr<FGameXXKBattleAtlasCache> Cache,
		FTravelAtlasWidgetFixture& OutFixture,
		FString& OutError)
	{
		OutFixture = FTravelAtlasWidgetFixture();
		OutError.Reset();
		OutFixture.GameInstance = NewObject<UGameInstance>();
		OutFixture.Subsystem = NewObject<UGameXXKMVPSubsystem>(OutFixture.GameInstance);
		if (!OutFixture.Subsystem || !OutFixture.Subsystem->StartGame())
		{
			OutError = TEXT("The fixture could not start the game.");
			return false;
		}

		FGameXXKRuntimeState& State = OutFixture.Subsystem->GetMutableRuntimeState();
		FGameXXKPermanentCompanion* ActiveCompanion =
			State.CardRun.CompanionRoster.PermanentCompanions.FindByPredicate(
				[](const FGameXXKPermanentCompanion& Candidate)
				{
					return Candidate.bIsActive && !Candidate.InstanceId.IsNone();
				});
		if (!ActiveCompanion)
		{
			OutError = TEXT("The fixture has no active companion to retag.");
			return false;
		}
		const FName PreviousCompanionId = ActiveCompanion->InstanceId;
		ActiveCompanion->InstanceId = PermanentUnitId;
		for (FGameXXKPartyMemberRef& Ref : State.CardRun.OrderedFormation.Members)
		{
			if (Ref.Kind == EGameXXKPartyMemberKind::PermanentCompanion
				&& Ref.MemberId == PreviousCompanionId)
			{
				Ref.MemberId = PermanentUnitId;
			}
		}
		FGameXXKPartyFormationRules::ProjectCompatibility(State);
		if (!GameXXKPermanentPartyTestFixtures::SelectNpc(
			State,
			QuestNpcId,
			&OutError))
		{
			return false;
		}
		const FName StageId = FGameXXKTrainingRules::MakeStageId(
			EGameXXKTrainingDifficulty::Normal,
			1);
		if (!OutFixture.Subsystem->StartTrainingTravel(StageId))
		{
			OutError = TEXT("The fixture could not start Training Travel.");
			return false;
		}

		OutFixture.Widget = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
		if (!OutFixture.Widget)
		{
			OutError = TEXT("The fixture could not create the Workbench widget.");
			return false;
		}
		OutFixture.Widget->SetMVPSubsystem(OutFixture.Subsystem);
		OutFixture.Widget->SetTravelAtlasCacheForTest(MoveTemp(Cache));
		if (!OutFixture.Widget->OpenWorkbench())
		{
			OutError = TEXT("The fixture could not open the Workbench.");
			return false;
		}
		OutFixture.PermanentImage = OutFixture.Widget->WidgetTree
			? Cast<UImage>(OutFixture.Widget->WidgetTree->FindWidget(TEXT("TravelCompanionAnimatedUnit_0")))
			: nullptr;
		if (!OutFixture.PermanentImage)
		{
			OutError = TEXT("The fixture has no permanent-companion image.");
			return false;
		}
		return true;
	}

	FName CreateCarryTestEquipment(
		FAutomationTestBase& Test,
		FGameXXKRuntimeState& State,
		const EGameXXKEquipmentSlot Slot,
		const TCHAR* Context)
	{
		FGameXXKEquipmentCreateRequest Request;
		Request.Set = EGameXXKEquipmentSet::XuanJia;
		Request.Quality = EGameXXKEquipmentQuality::Rare;
		Request.ItemLevel = 1;
		Request.bForceSlot = true;
		Request.ForcedSlot = Slot;

		FName InstanceId;
		FString Error;
		if (!Test.TestTrue(
			Context,
			FGameXXKEquipmentRules::CreateRolledInstance(
				State.EquipmentCollection,
				Request,
				InstanceId,
				&Error)))
		{
			Test.AddError(Error);
		}
		return InstanceId;
	}

	UGameXXKInventoryWindowWidget* FindEmbeddedInventory(
		UGameXXKDesktopTrainingWorkbenchWidget* Workbench)
	{
		return Workbench && Workbench->WidgetTree
			? Cast<UGameXXKInventoryWindowWidget>(
				Workbench->WidgetTree->FindWidget(TEXT("EmbeddedApprovedBackpack")))
			: nullptr;
	}

	FString GetNamedText(UWidgetTree* Tree, const FName WidgetName)
	{
		const UTextBlock* TextBlock = Tree
			? Cast<UTextBlock>(Tree->FindWidget(WidgetName))
			: nullptr;
		return TextBlock ? TextBlock->GetText().ToString() : FString();
	}

	UGameXXKInventorySlotButton* FindEmbeddedBackpackButton(
		UGameXXKDesktopTrainingWorkbenchWidget* Workbench,
		const int32 SlotIndex)
	{
		UGameXXKInventoryWindowWidget* Inventory = FindEmbeddedInventory(Workbench);
		return Inventory && Inventory->WidgetTree
			? Cast<UGameXXKInventorySlotButton>(Inventory->WidgetTree->FindWidget(
				*FString::Printf(TEXT("InventoryBackpackSlot_%02d"), SlotIndex)))
			: nullptr;
	}

	UGameXXKInventorySlotButton* FindEmbeddedEquipmentButton(
		UGameXXKDesktopTrainingWorkbenchWidget* Workbench,
		const TCHAR* SlotName)
	{
		UGameXXKInventoryWindowWidget* Inventory = FindEmbeddedInventory(Workbench);
		return Inventory && Inventory->WidgetTree
			? Cast<UGameXXKInventorySlotButton>(Inventory->WidgetTree->FindWidget(
				*FString::Printf(TEXT("InventoryEquipmentSlot_%s"), SlotName)))
			: nullptr;
	}

	UGameXXKDesktopTrainingActionButton* FindWorkbenchActionButton(
		UGameXXKDesktopTrainingWorkbenchWidget* Workbench,
		const FName WidgetName)
	{
		return Workbench && Workbench->WidgetTree
			? Cast<UGameXXKDesktopTrainingActionButton>(
				Workbench->WidgetTree->FindWidget(WidgetName))
			: nullptr;
	}

	bool ClickAndFlush(
		UGameXXKDesktopTrainingWorkbenchWidget* Workbench,
		UButton* Button)
	{
		if (!Workbench || !Button)
		{
			return false;
		}
		Button->OnClicked.Broadcast();
		Workbench->TickForTest(0.0f);
		return true;
	}

	bool RouteVisibleButtonDelegateAndFlush(
		FAutomationTestBase& Test,
		UGameXXKDesktopTrainingWorkbenchWidget* Workbench,
		UButton* Button,
		const TCHAR* Context)
	{
		if (!Test.TestNotNull(
			*FString::Printf(TEXT("%s button exists before delegate routing"), Context),
			Button)
			|| !Workbench)
		{
			return false;
		}
		Workbench->ForceLayoutPrepass();
		const UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Button->Slot);
		if (!Test.TestEqual(
			*FString::Printf(TEXT("%s button is visible before delegate routing"), Context),
			Button->GetVisibility(),
			ESlateVisibility::Visible)
			|| !Test.TestTrue(
				*FString::Printf(TEXT("%s button is enabled before delegate routing"), Context),
				Button->GetIsEnabled())
			|| !Test.TestNotNull(
				*FString::Printf(TEXT("%s button has canvas geometry after layout prepass"), Context),
				CanvasSlot))
		{
			return false;
		}
		const FVector2D LocalSize = CanvasSlot->GetSize();
		if (!Test.TestTrue(
			*FString::Printf(TEXT("%s button has a valid local size after layout prepass"), Context),
			LocalSize.X > 0.0f && LocalSize.Y > 0.0f))
		{
			return false;
		}
		Button->OnClicked.Broadcast();
		Workbench->TickForTest(0.0f);
		return true;
	}

	void TestLockedCellOverlay(
		FAutomationTestBase& Test,
		const TCHAR* Context,
		UImage* LockedIcon)
	{
		if (!Test.TestNotNull(Context, LockedIcon))
		{
			return;
		}
		const UObject* Resource = LockedIcon->GetBrush().GetResourceObject();
		Test.TestTrue(
			*FString::Printf(TEXT("%s uses the approved locked-card texture"), Context),
			Resource && Resource->GetPathName().Contains(
				TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_CardLockedIcon")));
		Test.TestEqual(
			*FString::Printf(TEXT("%s is hit-test-invisible"), Context),
			LockedIcon->GetVisibility(),
			ESlateVisibility::HitTestInvisible);
		const UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(LockedIcon->Slot);
		if (Test.TestNotNull(
			*FString::Printf(TEXT("%s owns overlay geometry"), Context),
			OverlaySlot))
		{
			Test.TestEqual(
				*FString::Printf(TEXT("%s is right aligned"), Context),
				OverlaySlot->GetHorizontalAlignment(),
				HAlign_Right);
			Test.TestEqual(
				*FString::Printf(TEXT("%s is top aligned"), Context),
				OverlaySlot->GetVerticalAlignment(),
				VAlign_Top);
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingWorkbenchSlateBuildContractTest,
	"GameXXK.DesktopTraining.Workbench.SlateBuildContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingWorkbenchSlateBuildContractTest::RunTest(const FString& Parameters)
{
	UGameXXKDesktopTrainingWorkbenchWidget* Widget = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	TestNotNull(TEXT("workbench widget exists for the Slate build contract"), Widget);
	if (!Widget)
	{
		return false;
	}

	const TSharedRef<SWidget> SlateWidget = Widget->TakeWidget();
	TestNotNull(TEXT("workbench creates a WidgetTree root before Slate paints"), Widget->WidgetTree ? Widget->WidgetTree->RootWidget.Get() : nullptr);
	TestTrue(TEXT("workbench TakeWidget is not the null Slate placeholder"), SlateWidget != SNullWidget::NullWidget);
	UCanvasPanel* OverlayRoot = Widget->WidgetTree
		? Cast<UCanvasPanel>(Widget->WidgetTree->RootWidget)
		: nullptr;
	TestNotNull(TEXT("workbench root is the full-host transparent overlay canvas"), OverlayRoot);
	TestEqual(TEXT("transparent overlay root has the stable approved name"),
		OverlayRoot ? OverlayRoot->GetFName() : NAME_None,
		FName(TEXT("DesktopTrainingOverlayRoot")));
	UScaleBox* ScaleRoot = Widget->WidgetTree
		? Cast<UScaleBox>(Widget->WidgetTree->FindWidget(TEXT("DesktopTrainingScaleRoot")))
		: nullptr;
	TestNotNull(TEXT("full-host canvas owns the movable uniform HUD ScaleBox"), ScaleRoot);
	TestNotNull(TEXT("movable HUD ScaleBox uses a canvas slot"),
		ScaleRoot ? Cast<UCanvasPanelSlot>(ScaleRoot->Slot) : nullptr);
	TestTrue(TEXT("workbench root uses ScaleToFit"), ScaleRoot && ScaleRoot->GetStretch() == EStretch::ScaleToFit);
	USizeBox* ReferenceBox = ScaleRoot ? Cast<USizeBox>(ScaleRoot->GetContent()) : nullptr;
	TestNotNull(TEXT("ScaleBox owns the fixed reference SizeBox"), ReferenceBox);
	TestTrue(TEXT("collapsed reference width is 1038"), ReferenceBox && FMath::IsNearlyEqual(ReferenceBox->GetWidthOverride(), 1038.0f));
	TestTrue(TEXT("collapsed reference reserves one passive notice row"), ReferenceBox && FMath::IsNearlyEqual(ReferenceBox->GetHeightOverride(), 254.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingWorkbenchNativeConstructDoesNotRebuildSlateTreeTest,
	"GameXXK.DesktopTraining.Workbench.NativeConstructDoesNotRebuildSlateTree",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingWorkbenchNativeConstructDoesNotRebuildSlateTreeTest::RunTest(const FString& Parameters)
{
	UGameXXKDesktopTrainingWorkbenchWidget* Widget = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	TestNotNull(TEXT("workbench widget exists for the native construct lifecycle contract"), Widget);
	if (!Widget)
	{
		return false;
	}

	Widget->TakeWidget();
	Widget->ConstructForTest();
	TestEqual(TEXT("NativeConstruct leaves the Slate tree built by RebuildWidget intact"),
		Widget->GetProgrammaticLayoutBuildCountForTest(),
		1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingWorkbenchMasterV2ResourceContractTest,
	"GameXXK.DesktopTraining.Workbench.MasterV2ResourceContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingWorkbenchMasterV2ResourceContractTest::RunTest(const FString& Parameters)
{
	UGameXXKDesktopTrainingWorkbenchWidget* Widget = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	TestNotNull(TEXT("resource contract widget exists"), Widget);
	if (!Widget)
	{
		return false;
	}
	const TArray<FString> ResourcePaths = Widget->GetMasterV2ResourcePathsForTest();

	int32 ApprovedResourceCount = 0;
	bool bHasPanelLarge = false;
	bool bHasItemSlot = false;
	bool bHasEquipmentSlot = false;
	bool bHasHeroFullBody = false;
	bool bHasCloseInk = false;
	bool bHasIngot = false;
	bool bHasRejectedStarButton = false;
	bool bHasRejectedGenericTab = false;
	bool bHasApprovedNormalTab = false;
	bool bHasApprovedSelectedTab = false;
	int32 NavDiscCount = 0;
	for (const FString& Path : ResourcePaths)
	{
		if (Path.Contains(TEXT("/Game/GameXXK/UI/MasterV2/Approved/")))
		{
			++ApprovedResourceCount;
			bHasPanelLarge |= Path.Contains(TEXT("T_MasterV2_PanelLarge"));
			bHasItemSlot |= Path.Contains(TEXT("T_MasterV2_ItemSlot"));
			bHasEquipmentSlot |= Path.Contains(TEXT("T_MasterV2_EquipmentSlot"));
			bHasHeroFullBody |= Path.Contains(TEXT("T_MasterV2_HeroFullBody"));
			bHasCloseInk |= Path.Contains(TEXT("T_MasterV2_CloseInk"));
			bHasIngot |= Path.Contains(TEXT("T_MasterV2_Ingot"));
			bHasRejectedStarButton |= Path.Contains(TEXT("T_MasterV2_ButtonNeutral"))
				|| Path.Contains(TEXT("T_MasterV2_ButtonPrimary"))
				|| Path.Contains(TEXT("T_MasterV2_ButtonDanger"));
			bHasRejectedGenericTab |= Path.Contains(TEXT("T_MasterV2_TabNormal"))
				|| Path.Contains(TEXT("T_MasterV2_TabSelected"));
			bHasApprovedNormalTab |= Path.Contains(TEXT("003_tab_1"));
			bHasApprovedSelectedTab |= Path.Contains(TEXT("004_tab_2"));
			NavDiscCount += Path.Contains(TEXT("T_MasterV2_NavDisc")) ? 1 : 0;
		}
	}
	TestTrue(TEXT("workbench uses approved MasterV2 brush resources"), ApprovedResourceCount >= 3);
	TestTrue(TEXT("workbench uses the approved large panel texture"), bHasPanelLarge);
	TestTrue(TEXT("workbench uses the approved item slot texture"), bHasItemSlot);
	TestTrue(TEXT("workbench uses the approved equipment slot texture"), bHasEquipmentSlot);
	TestTrue(TEXT("workbench reuses the approved PSD backpack hero"), bHasHeroFullBody);
	TestTrue(TEXT("workbench reuses the approved PSD close ink"), bHasCloseInk);
	TestTrue(TEXT("workbench reuses the approved PSD ingot"), bHasIngot);
	TestFalse(TEXT("workbench never advertises the user-rejected star button base"), bHasRejectedStarButton);
	TestFalse(TEXT("workbench never substitutes the rejected generic star tabs"), bHasRejectedGenericTab);
	TestTrue(TEXT("workbench advertises the approved normal tab state"), bHasApprovedNormalTab);
	TestTrue(TEXT("workbench advertises the approved selected tab state"), bHasApprovedSelectedTab);
	TestTrue(TEXT("approved normal tab package exists"),
		FPackageName::DoesPackageExist(TEXT("/Game/GameXXK/UI/MasterV2/Approved/003_tab_1")));
	TestTrue(TEXT("approved selected tab package exists"),
		FPackageName::DoesPackageExist(TEXT("/Game/GameXXK/UI/MasterV2/Approved/004_tab_2")));
	TestTrue(TEXT("approved square selected-state package exists"),
		FPackageName::DoesPackageExist(
			TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_SquareSelected")));
	TestEqual(TEXT("workbench no longer advertises legacy MasterV2 navigation discs"), NavDiscCount, 0);
	const TCHAR* RemovedButtonPackages[] = {
		TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_TabNormal"),
		TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_TabSelected"),
		TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_ButtonNeutral"),
		TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_ButtonPrimary"),
		TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_ButtonDanger"),
		TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_SelectionInk"),
		TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_TooltipPaper"),
		TEXT("/Game/GameXXK/UI/Talents/Flat/T_TalentItemSlotSelected")};
	for (const TCHAR* PackagePath : RemovedButtonPackages)
	{
		TestFalse(
			*FString::Printf(TEXT("rejected button package is deleted: %s"), PackagePath),
			FPackageName::DoesPackageExist(PackagePath));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingWorkbenchImageTruthNavigationBindingTest,
	"GameXXK.DesktopTraining.Workbench.ImageTruthNavigationBinding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingWorkbenchImageTruthNavigationBindingTest::RunTest(const FString& Parameters)
{
	UGameXXKDesktopTrainingWorkbenchWidget* Widget = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	TestNotNull(TEXT("image-truth navigation fixture widget exists"), Widget);
	if (!Widget)
	{
		return false;
	}

	const TArray<FString> Paths = Widget->GetBottomNavigationIconResourcePathsForTest();
	TestEqual(TEXT("five bottom navigation icons are bound from the image truth set"), Paths.Num(), 5);
	for (const FString& Path : Paths)
	{
		TestTrue(
			TEXT("bottom navigation icon path stays inside ImageTruth/Training"),
			Path.StartsWith(TEXT("/Game/GameXXK/UI/ImageTruth/Training/")));
	}
	TestTrue(TEXT("warehouse truth glyph is selected"),
		Widget->GetBottomNavigationIconResourcePathForTest(EGameXXKDesktopTrainingNav::Warehouse).Contains(TEXT("T_TrainingNavWarehouse")));
	TestTrue(TEXT("formation truth glyph is selected"),
		Widget->GetBottomNavigationIconResourcePathForTest(EGameXXKDesktopTrainingNav::Formation).Contains(TEXT("T_TrainingNavFormation")));
	TestTrue(TEXT("talents truth glyph is selected"),
		Widget->GetBottomNavigationIconResourcePathForTest(EGameXXKDesktopTrainingNav::Talents).Contains(TEXT("T_TrainingNavTalents")));
	TestTrue(TEXT("tools truth glyph is selected"),
		Widget->GetBottomNavigationIconResourcePathForTest(EGameXXKDesktopTrainingNav::Tools).Contains(TEXT("T_TrainingNavTools")));
	TestTrue(TEXT("training truth glyph is selected"),
		Widget->GetBottomNavigationIconResourcePathForTest(EGameXXKDesktopTrainingNav::Training).Contains(TEXT("T_TrainingNavTraining")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingWorkbenchApprovedControlBindingTest,
	"GameXXK.DesktopTraining.Workbench.ApprovedControlBinding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingWorkbenchApprovedControlBindingTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	TestNotNull(TEXT("approved-control fixture subsystem exists"), Subsystem);
	if (!Subsystem || !Subsystem->StartGame())
	{
		return false;
	}

	UGameXXKDesktopTrainingWorkbenchWidget* Widget = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	TestNotNull(TEXT("approved-control workbench exists"), Widget);
	if (!Widget)
	{
		return false;
	}
	Widget->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("approved-control fixture expands the backpack"), Widget->OpenBackpack());
	Widget->HandleActionClicked(4);
	Widget->TakeWidget();

	const auto TestApprovedButton = [this, Widget](
		const FName WidgetName,
		const TCHAR* ExpectedResourceToken,
		const bool bMustBeImageBrush)
	{
		UButton* Button = Widget->WidgetTree
			? Cast<UButton>(Widget->WidgetTree->FindWidget(WidgetName))
			: nullptr;
		TestNotNull(*FString::Printf(TEXT("%s exists"), *WidgetName.ToString()), Button);
		if (!Button)
		{
			return;
		}
		TestTrue(
			*FString::Printf(TEXT("%s binds the approved PSD resource %s"), *WidgetName.ToString(), ExpectedResourceToken),
			GetButtonNormalResourcePath(Button).Contains(ExpectedResourceToken));
		TestEqual(
			*FString::Printf(TEXT("%s does not dark-color multiply approved art"), *WidgetName.ToString()),
			Button->GetBackgroundColor(),
			FLinearColor::White);
		if (bMustBeImageBrush)
		{
			TestTrue(
				*FString::Printf(TEXT("%s preserves its non-stretch image brush"), *WidgetName.ToString()),
				Button->GetStyle().Normal.DrawAs == ESlateBrushDrawType::Image);
		}
	};

	UWidget* CollectButton = Widget->WidgetTree
		? Widget->WidgetTree->FindWidget(TEXT("TravelCollectButton"))
		: nullptr;
	TestNull(TEXT("travel strip has no harvest/collect button"), CollectButton);
	TestApprovedButton(TEXT("TravelRetryButton"), TEXT("T_TrainingRetryButtonBase"), false);
	TestApprovedButton(TEXT("TrainingDifficultyDropdownButton"), TEXT("003_tab_1"), false);
	TestApprovedButton(TEXT("TrainingChapterTab_0"), TEXT("004_tab_2"), false);
	UButton* TrainingNode = Widget->WidgetTree
		? Cast<UButton>(Widget->WidgetTree->FindWidget(TEXT("TrainingNode_1")))
		: nullptr;
	if (TestNotNull(TEXT("TrainingNode_1 exists"), TrainingNode))
	{
		TestEqual(TEXT("TrainingNode_1 uses a code-native circular brush"),
			TrainingNode->GetStyle().Normal.DrawAs,
			ESlateBrushDrawType::RoundedBox);
		TestNull(TEXT("TrainingNode_1 no longer binds the rejected legacy navigation disc"),
			TrainingNode->GetStyle().Normal.GetResourceObject());
	}
	UImage* TrainingNodeStatus = Widget->WidgetTree
		? Cast<UImage>(Widget->WidgetTree->FindWidget(TEXT("TrainingNodeStatusIcon_1")))
		: nullptr;
	TestTrue(TEXT("TrainingNode_1 binds one generated Training status icon"),
		TrainingNodeStatus
			&& GetImageResourcePath(TrainingNodeStatus).Contains(TEXT("/UI/Training/MapNodes/T_TrainingNode_")));
	TestApprovedButton(TEXT("TrainingChallengeButton"), TEXT("004_tab_2"), false);
	TestApprovedButton(TEXT("TrainingTravelButton"), TEXT("004_tab_2"), false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingWorkbenchApprovedSecondaryControlBindingTest,
	"GameXXK.DesktopTraining.Workbench.ApprovedSecondaryControlBinding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingWorkbenchApprovedSecondaryControlBindingTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	TestNotNull(TEXT("secondary-control fixture subsystem exists"), Subsystem);
	if (!Subsystem || !Subsystem->StartGame())
	{
		return false;
	}
	UnlockToolsTalent(Subsystem->GetMutableRuntimeState());

	UGameXXKDesktopTrainingWorkbenchWidget* Widget = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	TestNotNull(TEXT("secondary-control workbench exists"), Widget);
	if (!Widget)
	{
		return false;
	}
	Widget->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("secondary-control fixture expands the backpack"), Widget->OpenBackpack());
	Widget->TakeWidget();

	const auto TestApprovedButton = [this, Widget](const FName WidgetName, const TCHAR* ExpectedResourceToken)
	{
		UButton* Button = Widget->WidgetTree
			? Cast<UButton>(Widget->WidgetTree->FindWidget(WidgetName))
			: nullptr;
		TestNotNull(*FString::Printf(TEXT("%s exists"), *WidgetName.ToString()), Button);
		if (!Button)
		{
			return;
		}
		TestTrue(
			*FString::Printf(TEXT("%s binds %s"), *WidgetName.ToString(), ExpectedResourceToken),
			GetButtonNormalResourcePath(Button).Contains(ExpectedResourceToken));
		TestEqual(
			*FString::Printf(TEXT("%s keeps the approved source color"), *WidgetName.ToString()),
			Button->GetBackgroundColor(),
			FLinearColor::White);
	};

	Widget->HandleActionClicked(3);
	TestApprovedButton(TEXT("ToolButton_0"), TEXT("004_tab_2"));
	TestApprovedButton(TEXT("ToolButton_1"), TEXT("003_tab_1"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingChallengeDelegatesToExistingRouteTest,
	"GameXXK.DesktopTraining.Workbench.ChallengeDelegatesToExistingRoute",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingChallengeDelegatesToExistingRouteTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	TestTrue(TEXT("route-delegation fixture starts in town"), Subsystem->StartGame());

	UGameXXKDesktopTrainingWorkbenchWidget* Widget = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	Widget->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("route-delegation fixture opens the workbench"), Widget->OpenWorkbench());
	const FName StageId = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 2);
	TestTrue(TEXT("route-delegation fixture selects the first unlocked challenge stage"), Widget->SelectStageForTest(StageId));

	TestTrue(TEXT("Challenge starts directly without the town quest"), Widget->ClickChallengeForTest());
	TestEqual(TEXT("Challenge opens the Slay-the-Spire route map first"),
		Subsystem->GetRuntimeState().Screen,
		EGameXXKScreen::DungeonMap);
	TestTrue(TEXT("direct Challenge owns a generated route map"), Subsystem->IsTrainingChallengeRouteMapActive());
	TestFalse(TEXT("the workbench closes before the battle surface opens"), Widget->IsWorkbenchVisibleForTest());
	TestNull(TEXT("the workbench never constructs an embedded BattleBoard"),
		Widget->WidgetTree ? Widget->WidgetTree->FindWidget(TEXT("ChallengeBattleBoard")) : nullptr);
	TestNull(TEXT("the rejected auto button is absent"),
		Widget->WidgetTree ? Widget->WidgetTree->FindWidget(TEXT("ChallengeAutoButton")) : nullptr);
	TestNull(TEXT("the rejected debug-advance button is absent"),
		Widget->WidgetTree ? Widget->WidgetTree->FindWidget(TEXT("ChallengeAdvanceButton")) : nullptr);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingChallengePreservesPrerequisitesTest,
	"GameXXK.DesktopTraining.Workbench.ChallengeBypassesTownQuest",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingChallengePreservesPrerequisitesTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	TestTrue(TEXT("missing-prerequisite fixture starts in town"), Subsystem->StartGame());
	UGameXXKDesktopTrainingWorkbenchWidget* Widget = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	Widget->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("missing-prerequisite fixture opens the workbench"), Widget->OpenWorkbench());
	const FName StageId = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 2);
	TestTrue(TEXT("missing-prerequisite fixture selects the first unlocked challenge stage"), Widget->SelectStageForTest(StageId));

	const EGameXXKQuestState QuestBefore = Subsystem->GetRuntimeState().QuestState;
	const FGameXXKCompanionPartySelection PartyBefore = Subsystem->GetRuntimeState().CardRun.PartySelection;
	TestTrue(TEXT("Challenge ignores missing town-route prerequisites"), Widget->ClickChallengeForTest());
	TestEqual(TEXT("direct Challenge opens the route map"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::DungeonMap);
	TestEqual(TEXT("direct Challenge does not silently accept the quest"), Subsystem->GetRuntimeState().QuestState, QuestBefore);
	TestTrue(TEXT("direct Challenge does not alter party selection"),
		FGameXXKCompanionPartySelection::StaticStruct()->CompareScriptStruct(
			&Subsystem->GetRuntimeState().CardRun.PartySelection,
			&PartyBefore,
			PPF_None));
	TestFalse(TEXT("direct Challenge closes the workbench"), Widget->IsWorkbenchVisibleForTest());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingChallengeButtonRequiresRoutePrerequisitesTest,
	"GameXXK.DesktopTraining.Workbench.ChallengeButtonIgnoresRouteQuest",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingChallengeButtonRequiresRoutePrerequisitesTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	TestTrue(TEXT("challenge-gate fixture starts in town"), Subsystem->StartGame());

	UGameXXKDesktopTrainingWorkbenchWidget* Widget = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	Widget->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("challenge-gate fixture opens the workbench"), Widget->OpenWorkbench());
	const FName StageId = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 2);
	TestTrue(TEXT("challenge-gate fixture selects the first unlocked challenge stage"), Widget->SelectStageForTest(StageId));
	TestTrue(TEXT("challenge-gate fixture expands the backpack"), Widget->OpenBackpack());
	Widget->HandleActionClicked(4);

	UButton* Challenge = Widget->WidgetTree ? Cast<UButton>(Widget->WidgetTree->FindWidget(TEXT("TrainingChallengeButton"))) : nullptr;
	if (!TestNotNull(TEXT("challenge button is built in training map"), Challenge))
	{
		return false;
	}
	TestTrue(TEXT("challenge is enabled without accepting the route quest"), Challenge->GetIsEnabled());
	TestFalse(TEXT("challenge tooltip never mentions the removed town prerequisite"),
		Challenge->GetToolTipText().ToString().Contains(TEXT("主线任务"))
		|| Challenge->GetToolTipText().ToString().Contains(TEXT("青山镇")));

	UButton* Travel = Widget->WidgetTree ? Cast<UButton>(Widget->WidgetTree->FindWidget(TEXT("TrainingTravelButton"))) : nullptr;
	if (!TestNotNull(TEXT("travel button is built in training map"), Travel))
	{
		return false;
	}
	TestFalse(TEXT("travel is disabled for an uncleared stage"), Travel->GetIsEnabled());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingClearedStageReplayTest,
	"GameXXK.DesktopTraining.Workbench.ClearedStageCanReplay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingClearedStageReplayTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	TestTrue(TEXT("cleared-stage replay fixture starts the game"), Subsystem->StartGame());
	const FName StageId = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 1);
	TestTrue(TEXT("the default stage is already cleared"),
		FGameXXKTrainingRules::IsStageCleared(Subsystem->GetTrainingProgressCopy(), StageId));

	UGameXXKDesktopTrainingWorkbenchWidget* Widget = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	Widget->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("cleared-stage replay fixture opens the workbench"), Widget->OpenWorkbench());
	TestTrue(TEXT("cleared-stage replay fixture selects 1-1"), Widget->SelectStageForTest(StageId));
	TestTrue(TEXT("cleared-stage replay fixture expands the backpack"), Widget->OpenBackpack());
	Widget->HandleActionClicked(4);

	UButton* Challenge = Widget->WidgetTree
		? Cast<UButton>(Widget->WidgetTree->FindWidget(TEXT("TrainingChallengeButton")))
		: nullptr;
	if (!TestNotNull(TEXT("the replay Challenge button is built"), Challenge))
	{
		return false;
	}
	TestTrue(TEXT("a cleared unlocked stage remains directly challengeable"), Challenge->GetIsEnabled());
	TestTrue(TEXT("clicking the cleared stage starts a replay battle"), Widget->ClickChallengeForTest());
	TestEqual(TEXT("the replay opens the challenge route map"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::DungeonMap);
	TestTrue(TEXT("the replay owns a generated challenge route"), Subsystem->IsTrainingChallengeRouteMapActive());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingReferenceGeometryTest,
	"GameXXK.DesktopTraining.Workbench.ReferenceGeometry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingReferenceGeometryTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKDesktopTrainingLayout;
	TestEqual(TEXT("reference canvas is the approved UI Master size"), GetReferenceCanvasSize(), FVector2D(1672.0f, 941.0f));
	TestEqual(TEXT("warehouse matches the selected layout"), GetWarehouseRect(), FVector4(10.0f, 17.0f, 363.0f, 908.0f));
	TestEqual(TEXT("center shell matches the selected layout"), GetCenterShellRect(), FVector4(386.0f, 17.0f, 970.0f, 908.0f));
	TestEqual(TEXT("right shell matches the selected layout"), GetRightShellRect(), FVector4(1369.0f, 17.0f, 291.0f, 908.0f));
	TestEqual(TEXT("scaled idle strip aligns with the Backpack content width"), GetIdleStripRect(), FVector4(397.0f, 17.0f, 945.0f, 184.0f));
	TestEqual(TEXT("backpack surface matches the selected layout"), GetContentRect(), FVector4(397.0f, 244.0f, 945.0f, 533.0f));
	TestEqual(TEXT("navigation matches the selected layout"), GetNavigationRect(), FVector4(397.0f, 788.0f, 945.0f, 137.0f));

	const FFitTransform FullHD = MakeFitTransform(FVector2D(1920.0f, 1080.0f));
	const FFitTransform QHD = MakeFitTransform(FVector2D(2560.0f, 1440.0f));
	TestTrue(TEXT("Full HD uses one uniform scale"), FMath::IsNearlyEqual(FullHD.Scale, 1080.0f / 941.0f, KINDA_SMALL_NUMBER));
	TestTrue(TEXT("QHD uses one uniform scale"), FMath::IsNearlyEqual(QHD.Scale, 1440.0f / 941.0f, KINDA_SMALL_NUMBER));
	const FVector4 FullHDNode = FullHD.ApplyRect(FVector4(0.0f, 0.0f, 58.0f, 58.0f));
	const FVector4 QHDNode = QHD.ApplyRect(FVector4(0.0f, 0.0f, 58.0f, 58.0f));
	TestTrue(TEXT("Full HD nodes remain circular"), FMath::IsNearlyEqual(FullHDNode.Z, FullHDNode.W, KINDA_SMALL_NUMBER));
	TestTrue(TEXT("QHD nodes remain circular"), FMath::IsNearlyEqual(QHDNode.Z, QHDNode.W, KINDA_SMALL_NUMBER));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingWorkbenchStructuralGeometryTest,
	"GameXXK.DesktopTraining.Workbench.StructuralGeometry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingWorkbenchStructuralGeometryTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	if (!TestNotNull(TEXT("structural geometry subsystem exists"), Subsystem) || !Subsystem->StartGame())
	{
		return false;
	}
	UGameXXKDesktopTrainingWorkbenchWidget* Widget = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	TestNotNull(TEXT("structural geometry widget exists"), Widget);
	if (!Widget)
	{
		return false;
	}
	Widget->SetMVPSubsystem(Subsystem);
	// Reference geometry is the downward-expanding authored canvas. Do not let a
	// player's persisted desktop drag position switch this deterministic fixture
	// to the equally valid upward layout.
	Widget->SetPresentationMode(EGameXXKDesktopHudPresentationMode::TownViewport);
	TestTrue(TEXT("structural geometry expands the backpack"), Widget->OpenBackpack());
	Widget->HandleActionClicked(0);
	Widget->HandleActionClicked(4);
	Widget->TakeWidget();

	const auto TestNamedRect = [this, Widget](const FName WidgetName, const FVector4& Expected)
	{
		const FString Name = WidgetName.ToString();
		UWidget* Child = Widget->WidgetTree ? Widget->WidgetTree->FindWidget(WidgetName) : nullptr;
		TestNotNull(*FString::Printf(TEXT("%s exists"), *Name), Child);
		const UCanvasPanelSlot* Slot = Child ? Cast<UCanvasPanelSlot>(Child->Slot) : nullptr;
		TestNotNull(*FString::Printf(TEXT("%s is placed on the reference canvas"), *Name), Slot);
		if (Slot)
		{
			if (!Slot->GetPosition().Equals(FVector2D(Expected.X, Expected.Y), KINDA_SMALL_NUMBER))
			{
				AddInfo(FString::Printf(
					TEXT("%s actual position %s, expected %s"),
					*Name,
					*Slot->GetPosition().ToString(),
					*FVector2D(Expected.X, Expected.Y).ToString()));
			}
			TestEqual(*FString::Printf(TEXT("%s position"), *Name), Slot->GetPosition(), FVector2D(Expected.X, Expected.Y));
			TestEqual(*FString::Printf(TEXT("%s size"), *Name), Slot->GetSize(), FVector2D(Expected.Z, Expected.W));
		}
	};

	TestNamedRect(TEXT("WarehousePanel"), GameXXKDesktopTrainingLayout::GetWarehouseRect());
	TestNamedRect(TEXT("CenterWorkbenchFrame"), GameXXKDesktopTrainingLayout::GetCenterShellRect());
	TestNamedRect(TEXT("TrainingTravelStrip"), GameXXKDesktopTrainingLayout::GetIdleStripRect());
	TestNamedRect(TEXT("BackpackPanel"), GameXXKDesktopTrainingLayout::GetContentRect());
	TestNamedRect(TEXT("TrainingMapPanel"), GameXXKDesktopTrainingLayout::GetRightShellRect());
	TestNamedRect(TEXT("BottomNavigationPanel"), GameXXKDesktopTrainingLayout::GetNavigationRect());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingWorkbenchTransparentDesktopPlacementTest,
	"GameXXK.DesktopTraining.Workbench.TransparentDesktopPlacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingWorkbenchTransparentDesktopPlacementTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	if (!TestNotNull(TEXT("transparent-placement subsystem exists"), Subsystem) || !Subsystem->StartGame())
	{
		return false;
	}
	UGameXXKDesktopTrainingWorkbenchWidget* Widget = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	TestNotNull(TEXT("transparent-placement workbench exists"), Widget);
	if (!Widget)
	{
		return false;
	}
	Widget->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("transparent-placement expands the backpack"), Widget->OpenBackpack());
	Widget->HandleActionClicked(0);
	Widget->HandleActionClicked(4);
	Widget->TakeWidget();

	const auto TestTransparentSurface = [this, Widget](const FName WidgetName)
	{
		UBorder* Surface = Widget->WidgetTree
			? Cast<UBorder>(Widget->WidgetTree->FindWidget(WidgetName))
			: nullptr;
		TestNotNull(*FString::Printf(TEXT("%s exists"), *WidgetName.ToString()), Surface);
		if (!Surface)
		{
			return;
		}
		TestTrue(
			*FString::Printf(TEXT("%s does not paint a backing surface"), *WidgetName.ToString()),
			Surface->Background.DrawAs == ESlateBrushDrawType::NoDrawType);
		TestNull(
			*FString::Printf(TEXT("%s has no backing texture resource"), *WidgetName.ToString()),
			Surface->Background.GetResourceObject());
		TestTrue(
			*FString::Printf(TEXT("%s remains fully transparent"), *WidgetName.ToString()),
			FMath::IsNearlyZero(Surface->GetBrushColor().A));
	};

	const auto TestFunctionalSurface = [this, Widget](const FName WidgetName)
	{
		UBorder* Surface = Widget->WidgetTree
			? Cast<UBorder>(Widget->WidgetTree->FindWidget(WidgetName))
			: nullptr;
		TestNotNull(*FString::Printf(TEXT("%s functional surface exists"), *WidgetName.ToString()), Surface);
		if (!Surface)
		{
			return;
		}
		TestTrue(
			*FString::Printf(TEXT("%s keeps the approved panel art"), *WidgetName.ToString()),
			GetBorderResourcePath(Surface).Contains(TEXT("T_MasterV2_PanelLarge")));
	};

	TestNull(
		TEXT("the erroneous full-canvas workbench paper is not constructed"),
		Widget->WidgetTree
			? Widget->WidgetTree->FindWidget(TEXT("WorkbenchBackground"))
			: nullptr);
	TestTransparentSurface(TEXT("CenterWorkbenchFrame"));
	TestTransparentSurface(TEXT("BottomNavigationPanel"));
	TestFunctionalSurface(TEXT("WarehousePanel"));
	TestTransparentSurface(TEXT("TrainingTravelStrip"));
	TestFunctionalSurface(TEXT("TrainingMapPanel"));
	TestNotNull(
		TEXT("the functional backpack remains the approved embedded inventory"),
		Widget->WidgetTree ? Widget->WidgetTree->FindWidget(TEXT("EmbeddedApprovedBackpack")) : nullptr);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingWorkbenchInnerGeometryTest,
	"GameXXK.DesktopTraining.Workbench.InnerGeometry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingWorkbenchInnerGeometryTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	TestNotNull(TEXT("inner geometry subsystem exists"), Subsystem);
	if (!Subsystem || !Subsystem->StartGame())
	{
		return false;
	}
	UGameXXKDesktopTrainingWorkbenchWidget* Widget = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	TestNotNull(TEXT("inner geometry widget exists"), Widget);
	if (!Widget)
	{
		return false;
	}
	Widget->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("inner geometry expands the backpack"), Widget->OpenBackpack());
	Widget->HandleActionClicked(0);
	Widget->HandleActionClicked(4);
	Widget->TakeWidget();
	TestEqual(TEXT("warehouse exposes nine visible rows"), Widget->GetWarehouseRowCountForTest(), 9);

	const auto TestNamedRect = [this, Widget](const FName WidgetName, const FVector4& Expected)
	{
		const FString Name = WidgetName.ToString();
		UWidget* Child = Widget->WidgetTree ? Widget->WidgetTree->FindWidget(WidgetName) : nullptr;
		TestNotNull(*FString::Printf(TEXT("%s exists"), *Name), Child);
		const UCanvasPanelSlot* Slot = Child ? Cast<UCanvasPanelSlot>(Child->Slot) : nullptr;
		TestNotNull(*FString::Printf(TEXT("%s is on the reference canvas"), *Name), Slot);
		if (Slot)
		{
			TestEqual(*FString::Printf(TEXT("%s position"), *Name), Slot->GetPosition(), FVector2D(Expected.X, Expected.Y));
			TestEqual(*FString::Printf(TEXT("%s size"), *Name), Slot->GetSize(), FVector2D(Expected.Z, Expected.W));
		}
	};

	TestNamedRect(TEXT("WarehouseSlot_0"), FVector4(30.0f, 142.0f, 68.0f, 68.0f));
	TestNamedRect(TEXT("EmbeddedApprovedBackpack"), FVector4(-311.0f, -173.0f, 1920.0f, 1080.0f));
	TestNamedRect(TEXT("BackpackGoldIcon"), FVector4(1034.0f, 291.0f, 30.0f, 30.0f));
	TestNamedRect(TEXT("TrainingNode_1"), FVector4(1473.0f, 194.0f, 82.0f, 82.0f));
	TestNamedRect(TEXT("BottomNavigationButton_0"), FVector4(421.0f, 800.0f, 151.0f, 112.0f));

	UButton* NavigationButton = Widget->WidgetTree
		? Cast<UButton>(Widget->WidgetTree->FindWidget(TEXT("BottomNavigationButton_0")))
		: nullptr;
	TestNotNull(TEXT("bottom navigation button exists for PSD tint contract"), NavigationButton);
	TestEqual(TEXT("approved navigation art is not dark-color multiplied"),
		NavigationButton ? NavigationButton->GetBackgroundColor() : FLinearColor::Black,
		FLinearColor::White);

	UGameXXKInventoryWindowWidget* EmbeddedBackpack = Widget->WidgetTree
		? Cast<UGameXXKInventoryWindowWidget>(Widget->WidgetTree->FindWidget(TEXT("EmbeddedApprovedBackpack")))
		: nullptr;
	TestNotNull(TEXT("center panel embeds the approved PSD backpack widget"), EmbeddedBackpack);
	if (EmbeddedBackpack)
	{
		TestTrue(TEXT("approved backpack runs in desktop-training embedded mode"), EmbeddedBackpack->IsDesktopTrainingEmbeddedModeForTest());
		TestEqual(TEXT("embedded backpack keeps the approved four-column grid"), EmbeddedBackpack->GetBackpackColumnCountForTest(), 4);
		TestEqual(TEXT("embedded backpack keeps twenty visible PSD slots"), EmbeddedBackpack->GetBackpackSlotCountForTest(), 20);
		TestEqual(TEXT("embedded backpack keeps six approved equipment slots"), EmbeddedBackpack->GetEquipmentSlotCountForTest(), 6);

		const auto TestEmbeddedRect = [this, EmbeddedBackpack](const FName WidgetName, const FVector4& Expected)
		{
			UWidget* Child = EmbeddedBackpack->WidgetTree ? EmbeddedBackpack->WidgetTree->FindWidget(WidgetName) : nullptr;
			TestNotNull(*FString::Printf(TEXT("embedded %s exists"), *WidgetName.ToString()), Child);
			const UCanvasPanelSlot* Slot = Child ? Cast<UCanvasPanelSlot>(Child->Slot) : nullptr;
			TestNotNull(*FString::Printf(TEXT("embedded %s keeps PSD canvas placement"), *WidgetName.ToString()), Slot);
			if (Slot)
			{
				TestEqual(*FString::Printf(TEXT("embedded %s position"), *WidgetName.ToString()), Slot->GetPosition(), FVector2D(Expected.X, Expected.Y));
				TestEqual(*FString::Printf(TEXT("embedded %s size"), *WidgetName.ToString()), Slot->GetSize(), FVector2D(Expected.Z, Expected.W));
			}
		};
		TestEmbeddedRect(TEXT("InventoryCentralHeroIdle"), FVector4(478.0f, 304.0f, 518.0f, 518.0f));
		TestEmbeddedRect(TEXT("InventoryEquipmentSlot_Weapon"), FVector4(420.0f, 340.0f, 118.0f, 124.0f));
		TestEmbeddedRect(TEXT("InventoryCharacterTab_0"), FVector4(514.0f, 220.0f, 105.0f, 62.0f));
		UWidget* RemovedTalentTab = EmbeddedBackpack->WidgetTree ? EmbeddedBackpack->WidgetTree->FindWidget(TEXT("InventoryCharacterTab_3")) : nullptr;
		UWidget* RemovedTitleTab = EmbeddedBackpack->WidgetTree ? EmbeddedBackpack->WidgetTree->FindWidget(TEXT("InventoryCharacterTab_4")) : nullptr;
		TestTrue(TEXT("embedded mode removes the top talent tab"), RemovedTalentTab && RemovedTalentTab->GetVisibility() == ESlateVisibility::Collapsed);
		TestTrue(TEXT("embedded mode removes the top title tab"), RemovedTitleTab && RemovedTitleTab->GetVisibility() == ESlateVisibility::Collapsed);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingWorkbenchLayoutContractTest,
	"GameXXK.DesktopTraining.Workbench.LayoutContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingWorkbenchLayoutContractTest::RunTest(const FString& Parameters)
{
	UGameXXKDesktopTrainingWorkbenchWidget* Widget = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	TestNotNull(TEXT("workbench widget can be constructed without a live viewport"), Widget);
	if (!Widget)
	{
		return false;
	}
	TestEqual(TEXT("warehouse uses four columns"), Widget->GetWarehouseColumnCountForTest(), 4);
	TestEqual(TEXT("warehouse uses nine visible rows"), Widget->GetWarehouseRowCountForTest(), 9);
	const FVector2D BackpackRatio = Widget->GetBackpackAspectRatioForTest();
	TestTrue(TEXT("backpack aspect ratio keeps the real wide proportion"), FMath::IsNearlyEqual(BackpackRatio.X / BackpackRatio.Y, 1.76f, 0.001f));

	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	TestNotNull(TEXT("workbench read model fixture subsystem exists"), Subsystem);
	if (!Subsystem || !Subsystem->StartGame())
	{
		return false;
	}
	FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
	UnlockToolsTalent(State);
	UnlockOfflineTalent(State);
	PreserveLegacyWorkbenchCapacity(State);
	State.PlayerGold = 4242;
	State.Inventory.Empty();
	State.Inventory.Add(UGameXXKMVPRules::ItemHealingPowder(), 3);
	State.Inventory.Add(UGameXXKMVPRules::ItemTrainingNormalChest(), 2);
	for (int32 ExtraIndex = 0; ExtraIndex < 31; ++ExtraIndex)
	{
		FGameXXKEquipmentCreateRequest Request;
		Request.Set = EGameXXKEquipmentSet::Starter;
		Request.Quality = EGameXXKEquipmentQuality::Common;
		Request.ItemLevel = 1 + (ExtraIndex % FGameXXKEquipmentRules::MaxItemLevel);
		Request.bForceSlot = true;
		Request.ForcedSlot = EGameXXKEquipmentSlot::Weapon;
		FName InstanceId;
		FString Error;
		TestTrue(TEXT("warehouse pagination fixture creates an equipment instance"),
			FGameXXKEquipmentRules::CreateRolledInstance(State.EquipmentCollection, Request, InstanceId, &Error));
	}
	Widget->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("desktop workbench opens as the persistent idle strip"), Widget->OpenWorkbench());
	TestFalse(TEXT("desktop workbench starts with the backpack collapsed"), Widget->IsBackpackExpandedForTest());
	TestTrue(TEXT("collapsed workbench still owns the live Travel strip"), Widget->HasTravelVisualStripForTest());
	TestNotNull(TEXT("collapsed workbench exposes the down-arrow Tab control"),
		Widget->WidgetTree ? Widget->WidgetTree->FindWidget(TEXT("BackpackTabToggleButton")) : nullptr);
	TestTrue(TEXT("Tab/backpack entry opens the formation-backed backpack view"), Widget->OpenBackpack());
	TestTrue(TEXT("opening backpack expands the center surface"), Widget->IsBackpackExpandedForTest());
	TestEqual(TEXT("expanded backpack exposes five small top-toolbar controls"), Widget->GetTopToolbarButtonCountForTest(), 5);
	TestEqual(TEXT("topmost toolbar uses the confirmed black pushpin truth asset"),
		Widget->GetTopToolbarAlwaysOnTopResourcePathForTest(),
		FString(TEXT("/Game/GameXXK/UI/ImageTruth/Training/T_TrainingTopToolbarAlwaysOnTop.T_TrainingTopToolbarAlwaysOnTop")));
	TestEqual(TEXT("topmost toolbar exposes the confirmed gray disabled pushpin truth asset"),
		Widget->GetTopToolbarAlwaysOnTopOffResourcePathForTest(),
		FString(TEXT("/Game/GameXXK/UI/ImageTruth/Training/T_TrainingTopToolbarAlwaysOnTopOffGray.T_TrainingTopToolbarAlwaysOnTopOffGray")));
	TestFalse(TEXT("warehouse is not forced open with the backpack"), Widget->IsWarehousePanelOpenForTest());
	TestFalse(TEXT("right-side pages are not forced open with the backpack"), Widget->IsRightPanelOpenForTest());
	TestEqual(TEXT("backpack defaults to the hero character"),
		Widget->GetActiveBackpackCharacterIdForTest(),
		FGameXXKEquipmentRules::HeroCharacterId());
	const TArray<FName> BackpackCharacterIds = Widget->GetBackpackCharacterIdsForTest();
	TestEqual(TEXT("backpack exposes the fixed hero and all six starter-role companions"), BackpackCharacterIds.Num(), 7);
	TestTrue(TEXT("backpack character list keeps the hero first"),
		BackpackCharacterIds.Num() > 0
		&& BackpackCharacterIds[0] == FGameXXKEquipmentRules::HeroCharacterId());
	if (BackpackCharacterIds.Num() > 1)
	{
		TestTrue(TEXT("backpack can switch to a permanent companion"), Widget->SelectBackpackCharacterForTest(BackpackCharacterIds[1]));
		TestEqual(TEXT("selected companion becomes the backpack read-model owner"),
			Widget->GetActiveBackpackCharacterIdForTest(),
			BackpackCharacterIds[1]);
	}
	TestFalse(TEXT("backpack rejects an unknown character"), Widget->SelectBackpackCharacterForTest(FName(TEXT("Character.Unknown"))));
	Widget->HandleActionClicked(3);
	TestEqual(TEXT("tools navigation replaces the right-side map"), Widget->GetActiveNavForTest(), EGameXXKDesktopTrainingNav::Tools);
	TestTrue(TEXT("tools panel is active outside challenge viewport"), Widget->IsToolsPanelActiveForTest());
	TestTrue(TEXT("tools navigation opens the right-side panel"), Widget->IsRightPanelOpenForTest());
	TestEqual(TEXT("tools panel exposes a fixed three-by-three input grid"), Widget->GetToolSlotCountForTest(), 9);
	TestEqual(TEXT("tools panel exposes five unified modes"), Widget->GetToolModeCountForTest(), 5);
	Widget->HandleActionClicked(4);
	TestEqual(TEXT("training navigation returns to the map shell"), Widget->GetActiveNavForTest(), EGameXXKDesktopTrainingNav::Training);
	TestFalse(TEXT("training navigation is not the tools panel"), Widget->IsToolsPanelActiveForTest());
	Widget->HandleActionClicked(0);
	TestTrue(TEXT("warehouse navigation independently opens the left panel"), Widget->IsWarehousePanelOpenForTest());
	TestEqual(TEXT("root talent opens the approved second warehouse page"), Widget->GetWarehousePageCountForTest(), 2);
	TestEqual(TEXT("warehouse starts on its first page"), Widget->GetWarehousePageIndexForTest(), 0);
	TestEqual(TEXT("new warehouse partition has no duplicated backpack equipment"), Widget->GetVisibleWarehouseInstanceIdsForTest().Num(), 0);
	TestEqual(TEXT("workbench reads the authoritative runtime gold"), Widget->GetRuntimeGoldForTest(), 4242);
	const FName TravelStage = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 1);
	TestTrue(TEXT("travel fixture starts the default cleared stage"), Subsystem->StartTrainingTravel(TravelStage));
	FGameXXKTrainingOfflineReward SimulatedTravelReward;
	TestTrue(TEXT("travel fixture creates a pending full-health offline reward"),
		Subsystem->SimulateTrainingTravelOffline(512, SimulatedTravelReward));
	TestTrue(TEXT("workbench exposes pending travel gold for collection"), Widget->GetPendingTravelGoldForTest() > 0);
	TestEqual(TEXT("workbench exposes pending normal travel chests"),
		Widget->GetPendingTravelNormalChestCountForTest(), SimulatedTravelReward.NormalChestCount);
	TestEqual(TEXT("workbench exposes pending advanced travel chests"),
		Widget->GetPendingTravelAdvancedChestCountForTest(), SimulatedTravelReward.AdvancedChestCount);
	const int32 GoldBeforeCollect = Widget->GetRuntimeGoldForTest();
	TestTrue(TEXT("workbench collect action deposits pending travel rewards"), Widget->CollectTravelRewardsForTest());
	TestEqual(TEXT("collect action deposits pending travel gold"),
		Widget->GetRuntimeGoldForTest(), GoldBeforeCollect + SimulatedTravelReward.Gold);
	TestEqual(TEXT("collect action clears pending travel gold"), Widget->GetPendingTravelGoldForTest(), 0);
	TestEqual(TEXT("workbench warehouse occupancy comes from the persisted warehouse partition"),
		Widget->GetWarehouseOccupancyForTest(),
		State.DesktopInventory.WarehouseEquipmentInstanceIds.Num());
	const TArray<FName> VisibleItems = Widget->GetVisibleBackpackItemIdsForTest();
	TestTrue(TEXT("workbench backpack read model includes healing powder"), VisibleItems.Contains(UGameXXKMVPRules::ItemHealingPowder()));
	TestTrue(TEXT("workbench backpack read model includes a travel chest"), VisibleItems.Contains(UGameXXKMVPRules::ItemTrainingNormalChest()));
	TestEqual(TEXT("three difficulty bands each expose nine stage definitions"), FGameXXKTrainingRules::GetStageDefinitions().Num(), 27);
	TestEqual(TEXT("normal 1-1 id remains stable"), FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 1), FName(TEXT("Training.Normal.1-1")));
	TestTrue(TEXT("desktop HUD defaults to topmost z-order"), Widget->IsAlwaysOnTopForTest());
	Widget->HandleActionClicked(14);
	TestFalse(TEXT("topmost toolbar action can explicitly disable its state"), Widget->IsAlwaysOnTopForTest());
	Widget->HandleActionClicked(17);
	TestTrue(TEXT("mute toolbar action toggles its state"), Widget->IsMutedForTest());
	Widget->HandleActionClicked(15);
	TestTrue(TEXT("exit toolbar opens confirmation instead of closing immediately"), Widget->IsExitConfirmationOpenForTest());
	TestTrue(TEXT("exit confirmation keeps the idle workbench visible"), Widget->IsWorkbenchVisibleForTest());
	TestTrue(TEXT("cancelling exit confirmation succeeds"), Widget->CancelExitForTest());
	Widget->HandleActionClicked(60);
	TestFalse(TEXT("backpack collapse keeps the workbench alive"), Widget->IsBackpackExpandedForTest());
	TestTrue(TEXT("backpack collapse keeps the idle strip visible"), Widget->IsWorkbenchVisibleForTest() && Widget->HasTravelVisualStripForTest());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingCollapsedResourceHibernateTest,
	"GameXXK.DesktopTraining.Workbench.CollapsedResourceHibernate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingCollapsedResourceHibernateTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	TestTrue(TEXT("hibernation fixture starts the owned roster"), Subsystem->StartGame());
	const FName TravelStage = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 1);
	TestTrue(TEXT("hibernation fixture starts travel"), Subsystem->StartTrainingTravel(TravelStage));

	UGameXXKDesktopTrainingWorkbenchWidget* Workbench = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	Workbench->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("hibernation fixture opens the workbench"), Workbench->OpenWorkbench());
	TestTrue(TEXT("hibernation fixture expands the backpack"), Workbench->OpenBackpack());
	Workbench->HandleActionClicked(3);
	Workbench->HandleActionClicked(0);
	TestTrue(TEXT("fixture has warehouse open"), Workbench->IsWarehousePanelOpenForTest());
	TestTrue(TEXT("fixture has tools open"), Workbench->IsToolsPanelActiveForTest());

	const TArray<FName> NpcIds = Workbench->GetNpcCharacterIdsForTest();
	TestTrue(TEXT("fixture exposes a configurable NPC"), NpcIds.Num() > 0);
	if (NpcIds.IsEmpty())
	{
		return false;
	}
	TestTrue(TEXT("fixture selects an NPC backpack"), Workbench->SelectBackpackCharacterForTest(NpcIds[0]));
	UGameXXKInventoryWindowWidget* Embedded = Workbench->WidgetTree
		? Cast<UGameXXKInventoryWindowWidget>(Workbench->WidgetTree->FindWidget(TEXT("EmbeddedApprovedBackpack")))
		: nullptr;
	TestNotNull(TEXT("expanded workbench owns one embedded inventory"), Embedded);
	if (!Embedded)
	{
		return false;
	}
	TestTrue(TEXT("embedded deck tab opens before collapse"),
		Embedded->OpenCharacterBackpackTabForTest(EGameXXKCharacterBackpackTab::Deck));

	const int32 TravelTickBeforeCollapse = Workbench->GetTravelVisualNativeTickCountForTest();
	Workbench->HandleActionClicked(60);
	TestFalse(TEXT("collapse hides the backpack"), Workbench->IsBackpackExpandedForTest());
	TestFalse(TEXT("collapse does not schedule an unload"), Workbench->IsCollapsedResourceUnloadPendingForTest());
	TestFalse(TEXT("collapse does not mark resources released"), Workbench->AreCollapsedResourcesReleasedForTest());
	TestTrue(TEXT("collapse keeps the top travel strip"), Workbench->HasTravelVisualStripForTest());
	Workbench->TickForTest(0.1f);
	TestTrue(TEXT("travel keeps ticking while collapsed"),
		Workbench->GetTravelVisualNativeTickCountForTest() > TravelTickBeforeCollapse);
	TestEqual(TEXT("collapse never requests GC"), Workbench->GetCollapsedGcRequestCountForTest(), 0);

	Workbench->HandleActionClicked(60);
	TestTrue(TEXT("cold reopen expands the backpack"), Workbench->IsBackpackExpandedForTest());
	TestEqual(TEXT("cold reopen creates one embedded inventory"), Workbench->GetEmbeddedInventoryWidgetCountForTest(), 1);
	TestFalse(TEXT("global close does not restore the warehouse page"), Workbench->IsWarehousePanelOpenForTest());
	TestFalse(TEXT("global close does not restore the tools page"), Workbench->IsToolsPanelActiveForTest());
	TestEqual(TEXT("global close reopens the clean default backpack owner"),
		Workbench->GetEmbeddedBackpackCharacterIdForTest(), FGameXXKEquipmentRules::HeroCharacterId());
	TestEqual(TEXT("global close does not restore the deck subpage"),
		Workbench->GetEmbeddedBackpackTabForTest(), EGameXXKCharacterBackpackTab::Equipment);
	TestTrue(TEXT("global close discards stale embedded deck edits"),
		Workbench->GetEmbeddedPendingDeckIdsForTest().IsEmpty());

	UGameXXKDesktopTrainingWorkbenchWidget* WarmReopen = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	WarmReopen->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("warm fixture opens the workbench"), WarmReopen->OpenWorkbench());
	TestTrue(TEXT("warm fixture expands the backpack"), WarmReopen->OpenBackpack());
	WarmReopen->HandleActionClicked(60);
	WarmReopen->TickForTest(2.9f);
	WarmReopen->HandleActionClicked(60);
	TestFalse(TEXT("warm reopen cancels the pending release"), WarmReopen->IsCollapsedResourceUnloadPendingForTest());
	TestFalse(TEXT("warm reopen keeps active resources"), WarmReopen->AreCollapsedResourcesReleasedForTest());
	TestEqual(TEXT("warm reopen avoids a GC request"), WarmReopen->GetCollapsedGcRequestCountForTest(), 0);

	UGameXXKDesktopTrainingWorkbenchWidget* TalentsReopen = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	TalentsReopen->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("talents fixture opens the workbench"), TalentsReopen->OpenWorkbench());
	TestTrue(TEXT("talents fixture expands the backpack"), TalentsReopen->OpenBackpack());
	TalentsReopen->HandleActionClicked(2);
	TestEqual(TEXT("talents fixture selects the talents page"),
		TalentsReopen->GetActiveNavForTest(), EGameXXKDesktopTrainingNav::Talents);
	TalentsReopen->HandleActionClicked(60);
	TalentsReopen->TickForTest(1.0f);
	TalentsReopen->HandleActionClicked(60);
	TestEqual(TEXT("global close reopens the center on a clean Backpack page"),
		TalentsReopen->GetActiveCenterPageForTest(), EGameXXKDesktopTrainingCenterPage::Backpack);
	TestEqual(TEXT("global close clears stale center navigation"),
		TalentsReopen->GetActiveNavForTest(), EGameXXKDesktopTrainingNav::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingAdaptiveWindowScaleTest,
	"GameXXK.DesktopTraining.Workbench.AdaptiveWindowScaleAndDirection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingAdaptiveWindowScaleTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKDesktopTrainingLayout;
	TestTrue(TEXT("1920x1020 logical work area keeps the authored 100 percent scale"),
		FMath::IsNearlyEqual(ComputeAutomaticHudScale(FVector2D(1920.0f, 1020.0f)), 1.0f));
	TestTrue(TEXT("1366x728 logical work area scales down proportionally"),
		FMath::IsNearlyEqual(
			ComputeAutomaticHudScale(FVector2D(1366.0f, 728.0f)),
			1366.0f / 1920.0f,
			0.001f));
	TestTrue(TEXT("2560x1400 logical work area respects the 125 percent ceiling"),
		FMath::IsNearlyEqual(ComputeAutomaticHudScale(FVector2D(2560.0f, 1400.0f)), 1.25f));
	TestTrue(TEXT("the player's 50 percent option halves the automatic scale"),
		FMath::IsNearlyEqual(
			ComputeEffectiveHudScale(FVector2D(1920.0f, 1020.0f), 50),
			0.5f));
	TestEqual(TEXT("collapsed HUD authored size is 1038x202"),
		GetCollapsedHudLogicalSize(),
		FVector2D(1038.0f, 202.0f));
	TestEqual(TEXT("50 percent collapsed HUD resolves to 519x101"),
		GetCollapsedHudLogicalSize() * ComputeEffectiveHudScale(FVector2D(1920.0f, 1020.0f), 50),
		FVector2D(519.0f, 101.0f));

	const FVector4 WorkArea(0.0f, 0.0f, 1920.0f, 1020.0f);
	TestEqual(TEXT("a bottom-docked strip expands upward"),
		ChooseVerticalExpansionDirection(
			WorkArea,
			FVector4(450.0f, 800.0f, 1038.0f, 202.0f),
			941.0f),
		EGameXXKDesktopVerticalExpansionDirection::Up);
	TestEqual(TEXT("a top-docked strip expands downward"),
		ChooseVerticalExpansionDirection(
			WorkArea,
			FVector4(450.0f, 0.0f, 1038.0f, 202.0f),
			941.0f),
		EGameXXKDesktopVerticalExpansionDirection::Down);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingStableDragAnchorTest,
	"GameXXK.DesktopTraining.Workbench.StableDragAnchor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingStableDragAnchorTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKDesktopTrainingLayout;
	const FVector2D PhysicalWorkArea(1920.0f, 1020.0f);
	const FVector2D CollapsedStripSize(1038.0f, 202.0f);
	const FVector2D DragStartAnchor(0.4f, 0.6f);
	const FVector2D DragStartPointer(600.0f, 700.0f);

	TestEqual(
		TEXT("a stationary pointer preserves the exact drag-start anchor"),
		ResolveDesktopHudDragAnchor(
			DragStartAnchor,
			DragStartPointer,
			DragStartPointer,
			PhysicalWorkArea,
			CollapsedStripSize),
		DragStartAnchor);

	const FVector2D MovedAnchor = ResolveDesktopHudDragAnchor(
		DragStartAnchor,
		DragStartPointer,
		DragStartPointer + FVector2D(88.2f, -81.8f),
		PhysicalWorkArea,
		CollapsedStripSize);
	TestTrue(TEXT("horizontal physical delta maps one-for-one into anchor travel"),
		FMath::IsNearlyEqual(MovedAnchor.X, 0.5f, 0.001f));
	TestTrue(TEXT("vertical physical delta maps one-for-one into anchor travel"),
		FMath::IsNearlyEqual(MovedAnchor.Y, 0.5f, 0.001f));
	TestEqual(
		TEXT("repeating the same pointer sample is deterministic"),
		ResolveDesktopHudDragAnchor(
			DragStartAnchor,
			DragStartPointer,
			DragStartPointer + FVector2D(88.2f, -81.8f),
			PhysicalWorkArea,
			CollapsedStripSize),
		MovedAnchor);

	TestEqual(
		TEXT("large negative movement clamps to the work-area origin"),
		ResolveDesktopHudDragAnchor(
			DragStartAnchor,
			DragStartPointer,
			DragStartPointer - FVector2D(10000.0f, 10000.0f),
			PhysicalWorkArea,
			CollapsedStripSize),
		FVector2D::ZeroVector);
	TestEqual(
		TEXT("large positive movement clamps to the far work-area edge"),
		ResolveDesktopHudDragAnchor(
			DragStartAnchor,
			DragStartPointer,
			DragStartPointer + FVector2D(10000.0f, 10000.0f),
			PhysicalWorkArea,
			CollapsedStripSize),
		FVector2D(1.0f, 1.0f));

	const FVector2D DegenerateAxisAnchor = ResolveDesktopHudDragAnchor(
		DragStartAnchor,
		DragStartPointer,
		DragStartPointer + FVector2D(100.0f, 81.8f),
		FVector2D(800.0f, 1020.0f),
		CollapsedStripSize);
	TestTrue(TEXT("an axis with no available travel resolves to its canonical origin"),
		FMath::IsNearlyZero(DegenerateAxisAnchor.X));
	TestTrue(TEXT("a valid axis remains responsive beside a degenerate axis"),
		FMath::IsNearlyEqual(DegenerateAxisAnchor.Y, 0.7f, 0.001f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingStablePresentationScaleTest,
	"GameXXK.DesktopTraining.Workbench.StablePresentationScale",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingStablePresentationScaleTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKDesktopTrainingLayout;
	const FVector2D PhysicalWorkArea(1920.0f, 1020.0f);
	const FDesktopHudResolvedMetrics Metrics = ResolveDesktopHudMetrics(PhysicalWorkArea, 100);
	TestEqual(TEXT("resolved metrics preserve physical work-area pixels"),
		Metrics.PhysicalWorkAreaSize,
		PhysicalWorkArea);
	TestTrue(TEXT("1920 work area resolves authored 100-percent scale"),
		FMath::IsNearlyEqual(Metrics.Scale, 1.0f));

	const FDesktopOverlayPlacement Collapsed = ComputeDesktopOverlayPlacementAtScale(
		Metrics,
		FVector2D(0.35f, 0.45f),
		false,
		false,
		52.0f);
	const FDesktopOverlayPlacement Expanded = ComputeDesktopOverlayPlacementAtScale(
		Metrics,
		FVector2D(0.35f, 0.45f),
		true,
		false,
		52.0f);
	TestTrue(TEXT("collapsed and expanded layouts share one session scale"),
		FMath::IsNearlyEqual(Collapsed.Scale, Expanded.Scale));
	TestEqual(TEXT("Tab never changes the collapsed strip's resolved physical size"),
		Collapsed.StripSize,
		FVector2D(1038.0f, 202.0f));
	TestEqual(TEXT("desktop presentation keeps the persisted dragged anchor"),
		ResolvePresentationAnchor(true, FVector2D(0.35f, 0.45f)),
		FVector2D(0.35f, 0.45f));
	TestEqual(TEXT("3D-town presentation always resets the default anchor"),
		ResolvePresentationAnchor(false, FVector2D(0.35f, 0.45f)),
		FVector2D(0.5f, 0.08f));
	TestEqual(TEXT("125-percent Windows DPI converts physical HUD pixels to Slate host units"),
		PhysicalPixelsToSlateHost(FVector2D(1000.0f, 500.0f), 1.25f),
		FVector2D(800.0f, 400.0f));

	UGameXXKMVPSubsystem* Subsystem =
		NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	UGameXXKDesktopTrainingWorkbenchWidget* Widget =
		NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	if (!TestTrue(TEXT("stable-scale fixture starts a new game"),
		Subsystem && Subsystem->StartGame())
		|| !TestNotNull(TEXT("stable-scale fixture creates the workbench"), Widget))
	{
		return false;
	}
	Widget->SetMVPSubsystem(Subsystem);
	Widget->ConstructForTest();
	TestTrue(TEXT("stable-scale fixture opens collapsed"), Widget->OpenWorkbench());
	Widget->InitializeDesktopPresentationHostSize(PhysicalWorkArea);
	const float RuntimeCollapsedScale = Widget->GetDesktopPresentationScaleForTest();
	const FVector2D RuntimeFixedHostSize = Widget->GetDesktopWindowSizeForHost();
	TestEqual(TEXT("desktop mode reserves one maximum transparent native host"),
		RuntimeFixedHostSize,
		FVector2D(1820.0f, 941.0f));
	Widget->HandleActionClicked(60);
	Widget->TickForTest(0.0f);
	TestTrue(TEXT("runtime Tab expansion reuses the resolved session scale"),
		FMath::IsNearlyEqual(
			Widget->GetDesktopPresentationScaleForTest(),
			RuntimeCollapsedScale));
	TestEqual(TEXT("Tab expansion never resizes the transparent native host"),
		Widget->GetDesktopWindowSizeForHost(),
		RuntimeFixedHostSize);
	TestTrue(TEXT("canonical 2D map selects the layered desktop presentation"),
		AGameXXKMVPPlayerController::ShouldUseDesktopWindowForMapNameForTest(
			TEXT("/Game/GameXXK/Maps/L_DesktopTrainingHUD")));
	TestTrue(TEXT("PIE-prefixed canonical map remains a desktop presentation"),
		AGameXXKMVPPlayerController::ShouldUseDesktopWindowForMapNameForTest(
			TEXT("UEDPIE_0_L_DesktopTrainingHUD")));
	TestFalse(TEXT("the Qingshan 3D town uses the viewport presentation"),
		AGameXXKMVPPlayerController::ShouldUseDesktopWindowForMapNameForTest(
			TEXT("/Game/GameXXK/Maps/Prototype/L_Qingshan_AsianVillage_Demo")));
	Widget->SetPresentationMode(EGameXXKDesktopHudPresentationMode::TownViewport);
	TestFalse(TEXT("the shared HUD cannot be dragged while presented in 3D town"),
		Widget->CanDragDesktopHudForTest());
	AGameXXKMVPPlayerController* InputOwner =
		NewObject<AGameXXKMVPPlayerController>();
	if (TestNotNull(TEXT("town input-lock fixture creates its controller"), InputOwner))
	{
		InputOwner->SetIgnoreMoveInput(true);
		InputOwner->SetIgnoreLookInput(true);
		InputOwner->SetDesktopWorkbenchTownPanelInputLock(true);
		InputOwner->SetDesktopWorkbenchTownPanelInputLock(true);
		InputOwner->SetDesktopWorkbenchTownPanelInputLock(false);
		TestTrue(TEXT("releasing the HUD lock preserves another movement owner"),
			InputOwner->IsMoveInputIgnored());
		TestTrue(TEXT("releasing the HUD lock preserves another look owner"),
			InputOwner->IsLookInputIgnored());
		InputOwner->SetIgnoreMoveInput(false);
		InputOwner->SetIgnoreLookInput(false);
		TestFalse(TEXT("all movement locks can then clear normally"),
			InputOwner->IsMoveInputIgnored());
		TestFalse(TEXT("all look locks can then clear normally"),
			InputOwner->IsLookInputIgnored());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingTransparentOverlayHostTest,
	"GameXXK.DesktopTraining.Workbench.TransparentOverlayHost",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingTransparentOverlayHostTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKDesktopTrainingLayout;
	const FVector2D HostSize(1920.0f, 1020.0f);
	const FDesktopOverlayPlacement CollapsedPlacement = ComputeDesktopOverlayPlacement(
		HostSize,
		FVector2D(0.5f, 0.08f),
		100,
		false,
		false,
		52.0f);
	TestEqual(TEXT("overlay host keeps the complete monitor work area"),
		CollapsedPlacement.HostSize,
		HostSize);
	TestEqual(TEXT("collapsed visible strip remains 1038x202 at 100 percent"),
		CollapsedPlacement.StripSize,
		FVector2D(1038.0f, 202.0f));
	TestEqual(TEXT("collapsed notice reservation stays inside the movable HUD group"),
		CollapsedPlacement.HudSize,
		FVector2D(1038.0f, 254.0f));
	TestTrue(TEXT("normalized anchor resolves inside the fixed host"),
		CollapsedPlacement.HudTopLeft.Equals(FVector2D(441.0f, 65.44f), 0.01f));

	const FDesktopOverlayPlacement HalfPlacement = ComputeDesktopOverlayPlacement(
		HostSize,
		FVector2D(0.5f, 0.08f),
		50,
		false,
		false,
		52.0f);
	TestEqual(TEXT("50 percent visible strip is exactly 519x101"),
		HalfPlacement.StripSize,
		FVector2D(519.0f, 101.0f));
	TestEqual(TEXT("HUD scale never changes the native host size"),
		HalfPlacement.HostSize,
		HostSize);

	const FDesktopOverlayPlacement BottomExpandedPlacement = ComputeDesktopOverlayPlacement(
		HostSize,
		FVector2D(1.0f, 1.0f),
		100,
		true,
		true,
		52.0f);
	TestEqual(TEXT("expansion keeps the full-screen transparent host fixed"),
		BottomExpandedPlacement.HostSize,
		HostSize);
	TestTrue(TEXT("expanded HUD is clamped inside the host"),
		BottomExpandedPlacement.HudTopLeft.X >= 0.0f
			&& BottomExpandedPlacement.HudTopLeft.Y >= 0.0f
			&& BottomExpandedPlacement.HudTopLeft.X + BottomExpandedPlacement.HudSize.X <= HostSize.X + KINDA_SMALL_NUMBER
			&& BottomExpandedPlacement.HudTopLeft.Y + BottomExpandedPlacement.HudSize.Y <= HostSize.Y + KINDA_SMALL_NUMBER);

	TestEqual(TEXT("town button is 144 square"),
		GetTownToggleButtonSize(),
		FVector2D(144.0f, 144.0f));
	TestTrue(TEXT("closed-warehouse town button sits immediately left of the center shell"),
		FMath::IsNearlyEqual(
			GetTownToggleRect(false).X + GetTownToggleRect(false).Z + 14.0f,
			GetCenterShellRect().X));
	TestEqual(TEXT("open warehouse adds the exact left extension"),
		GetExpandedLeftExtension(true),
		148.0f);
	const FVector4 OpenTownRect = GetTownToggleRect(true);
	const FVector4 ShiftedWarehouseRect = GetWarehouseRect()
		+ FVector4(GetExpandedLeftExtension(true), 0.0f, 0.0f, 0.0f);
	TestTrue(TEXT("open-warehouse town button sits immediately left of the shifted warehouse"),
		FMath::IsNearlyEqual(
			OpenTownRect.X + OpenTownRect.Z + 14.0f,
			ShiftedWarehouseRect.X));
	const FDesktopOverlayPlacement ClosedWarehousePlacement = ComputeDesktopOverlayPlacement(
		HostSize,
		FVector2D(0.8f, 0.08f),
		100,
		true,
		false,
		52.0f,
		false);
	const FDesktopOverlayPlacement OpenWarehousePlacement = ComputeDesktopOverlayPlacement(
		HostSize,
		FVector2D(0.8f, 0.08f),
		100,
		true,
		false,
		52.0f,
		true);
	TestEqual(TEXT("closed warehouse keeps the authored expanded width"),
		ClosedWarehousePlacement.HudSize.X,
		1672.0);
	TestEqual(TEXT("open warehouse reserves the town-button extension"),
		OpenWarehousePlacement.HudSize.X,
		1820.0);
	TestTrue(TEXT("a fitting anchor keeps the strip stable while the extension opens"),
		ClosedWarehousePlacement.StripTopLeft.Equals(
			OpenWarehousePlacement.StripTopLeft,
			0.01f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingTownTogglePresentationTest,
	"GameXXK.DesktopTraining.Workbench.TownTogglePresentation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingTownTogglePresentationTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	if (!TestNotNull(TEXT("town-toggle subsystem exists"), Subsystem) || !Subsystem->StartGame())
	{
		return false;
	}
	UGameXXKDesktopTrainingWorkbenchWidget* Widget =
		NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	if (!TestNotNull(TEXT("town-toggle workbench exists"), Widget))
	{
		return false;
	}
	Widget->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("town-toggle fixture opens collapsed"), Widget->OpenWorkbench());
	Widget->TakeWidget();
	TestNull(TEXT("town button is absent while the backpack is collapsed"),
		Widget->WidgetTree ? Widget->WidgetTree->FindWidget(TEXT("TownToggleButton")) : nullptr);

	TestTrue(TEXT("town-toggle fixture expands the backpack"), Widget->OpenBackpack());
	Widget->TakeWidget();
	UGameXXKDesktopTrainingActionButton* EnterButton = Widget->WidgetTree
		? Cast<UGameXXKDesktopTrainingActionButton>(
			Widget->WidgetTree->FindWidget(TEXT("TownToggleButton")))
		: nullptr;
	if (!TestNotNull(TEXT("expanded desktop shows the town button"), EnterButton))
	{
		return false;
	}
	TestEqual(TEXT("desktop uses the approved enter-town texture"),
		GetButtonNormalResourcePath(EnterButton),
		FString(TEXT("/Game/GameXXK/UI/DesktopOverlay/T_DesktopTownEnterButton.T_DesktopTownEnterButton")));
	TestEqual(TEXT("town button owns the dedicated action"),
		EnterButton->GetConfiguredActionIdForTest(),
		652);
	if (const UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(EnterButton->Slot))
	{
		const FVector4 Expected = GameXXKDesktopTrainingLayout::GetTownToggleRect(false);
		TestEqual(TEXT("closed-warehouse town button position"),
			Slot->GetPosition(), FVector2D(Expected.X, Expected.Y));
		TestEqual(TEXT("closed-warehouse town button size"),
			Slot->GetSize(), FVector2D(Expected.Z, Expected.W));
	}
	else
	{
		AddError(TEXT("town button has no canvas slot"));
	}

	Widget->HandleActionClicked(0);
	UGameXXKDesktopTrainingActionButton* ShiftedButton = Widget->WidgetTree
		? Cast<UGameXXKDesktopTrainingActionButton>(
			Widget->WidgetTree->FindWidget(TEXT("TownToggleButton")))
		: nullptr;
	if (TestNotNull(TEXT("warehouse-open layout keeps the town button"), ShiftedButton))
	{
		const UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(ShiftedButton->Slot);
		const FVector4 Expected = GameXXKDesktopTrainingLayout::GetTownToggleRect(true);
		TestNotNull(TEXT("shifted town button has a canvas slot"), Slot);
		if (Slot)
		{
			TestEqual(TEXT("warehouse-open town button moves to the outer-left edge"),
				Slot->GetPosition(), FVector2D(Expected.X, Expected.Y));
		}
	}

	Widget->SetPresentationMode(EGameXXKDesktopHudPresentationMode::TownViewport);
	Widget->OpenBackpack();
	UGameXXKDesktopTrainingActionButton* ExitButton = Widget->WidgetTree
		? Cast<UGameXXKDesktopTrainingActionButton>(
			Widget->WidgetTree->FindWidget(TEXT("TownToggleButton")))
		: nullptr;
	if (TestNotNull(TEXT("town viewport keeps the same control"), ExitButton))
	{
		TestEqual(TEXT("town viewport uses the approved exit-town texture"),
			GetButtonNormalResourcePath(ExitButton),
			FString(TEXT("/Game/GameXXK/UI/DesktopOverlay/T_DesktopTownExitButton.T_DesktopTownExitButton")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingStoryQuestCarriageRequestTest,
	"GameXXK.DesktopTraining.Workbench.StoryQuestCarriageRequest",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingStoryQuestCarriageRequestTest::RunTest(
	const FString& Parameters)
{
	using namespace GameXXKDesktopTrainingLayout;
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(GameInstance);
	UGameXXKDesktopTrainingWorkbenchWidget* Widget =
		NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	if (!TestTrue(TEXT("placeholder fixture starts"),
		Subsystem && Subsystem->StartGame() && Widget))
	{
		return false;
	}
	Widget->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("placeholder fixture opens collapsed"), Widget->OpenWorkbench());
	Widget->TakeWidget();
	TestNull(TEXT("collapsed workbench has no story button"),
		Widget->WidgetTree->FindWidget(TEXT("StoryQuestButton")));

	TestTrue(TEXT("placeholder fixture expands"), Widget->OpenBackpack());
	Widget->TakeWidget();
	UGameXXKDesktopTrainingActionButton* Button =
		Cast<UGameXXKDesktopTrainingActionButton>(
			Widget->WidgetTree->FindWidget(TEXT("StoryQuestButton")));
	if (!TestNotNull(TEXT("expanded workbench has the story button"), Button))
	{
		return false;
	}
	TestEqual(TEXT("story button owns inert action 654"),
		Button->GetConfiguredActionIdForTest(), 654);
	TestEqual(TEXT("story button uses approved texture"),
		GetButtonNormalResourcePath(Button),
		FString(TEXT("/Game/GameXXK/UI/DesktopOverlay/"
			"T_DesktopStoryQuestButton.T_DesktopStoryQuestButton")));
	const UCanvasPanelSlot* ClosedSlot = Cast<UCanvasPanelSlot>(Button->Slot);
	const FVector4 ClosedRect = GetStoryQuestRect(false);
	TestTrue(TEXT("closed-left-drawer story button uses authored rect"),
		ClosedSlot
		&& ClosedSlot->GetPosition() == FVector2D(ClosedRect.X, ClosedRect.Y)
		&& ClosedSlot->GetSize() == FVector2D(ClosedRect.Z, ClosedRect.W));

	const bool ExpandedBefore = Widget->IsBackpackExpandedForTest();
	const bool WarehouseBefore = Widget->IsWarehousePanelOpenForTest();
	const EGameXXKDesktopTrainingNav NavBefore = Widget->GetActiveNavForTest();
	const EGameXXKDesktopTrainingCenterPage CenterBefore =
		Widget->GetActiveCenterPageForTest();
	const bool RightPanelBefore = Widget->IsRightPanelOpenForTest();
	const bool CarryBefore = Widget->IsCarryingItemForTest();
	const int32 ToolCountBefore = Widget->GetOccupiedToolSlotCountForTest();
	const int32 WarehousePageBefore = Widget->GetWarehousePageIndexForTest();
	const bool TownPendingBefore = Widget->IsTownMapTravelPendingForTest();
	const FText NoticeBefore = Widget->GetLastDesktopInventoryNoticeForTest();
	const int32 BuildCountBefore = Widget->GetProgrammaticLayoutBuildCountForTest();
	const int32 StoryCountBefore =
		Subsystem->GetRuntimeState().NarrativeProgress.StoryProgressById.Num();
	const int32 TaskCountBefore =
		Subsystem->GetRuntimeState().NarrativeProgress.TaskProgressById.Num();
	const FName TrackedTaskBefore =
		Subsystem->GetRuntimeState().NarrativeProgress.TrackedTaskId;
	const TArray<FGameXXKPartyMemberRef> OrderedFormationBefore =
		Subsystem->GetRuntimeState().CardRun.OrderedFormation.Members;
	int32 RequestCount = 0;
	Widget->SetStoryCarriageRequestedForTest(
		FGameXXKStoryCarriageRequested::CreateLambda([&RequestCount]()
		{
			++RequestCount;
			return true;
		}));

	Button->HandleClicked();
	TestEqual(TEXT("one click emits one carriage request"), RequestCount, 1);
	TestEqual(TEXT("carriage request preserves expansion"),
		Widget->IsBackpackExpandedForTest(), ExpandedBefore);
	TestEqual(TEXT("carriage request preserves Warehouse"),
		Widget->IsWarehousePanelOpenForTest(), WarehouseBefore);
	TestEqual(TEXT("carriage request preserves navigation"),
		Widget->GetActiveNavForTest(), NavBefore);
	TestEqual(TEXT("carriage request preserves center page"),
		Widget->GetActiveCenterPageForTest(), CenterBefore);
	TestEqual(TEXT("carriage request preserves right panel"),
		Widget->IsRightPanelOpenForTest(), RightPanelBefore);
	TestEqual(TEXT("carriage request preserves carry state"),
		Widget->IsCarryingItemForTest(), CarryBefore);
	TestEqual(TEXT("carriage request preserves tool reservations"),
		Widget->GetOccupiedToolSlotCountForTest(), ToolCountBefore);
	TestEqual(TEXT("carriage request preserves Warehouse page"),
		Widget->GetWarehousePageIndexForTest(), WarehousePageBefore);
	TestEqual(TEXT("semantic request does not own town pending state"),
		Widget->IsTownMapTravelPendingForTest(), TownPendingBefore);
	TestEqual(TEXT("carriage request preserves notice"),
		Widget->GetLastDesktopInventoryNoticeForTest(), NoticeBefore);
	TestEqual(TEXT("carriage request does not rebuild layout"),
		Widget->GetProgrammaticLayoutBuildCountForTest(), BuildCountBefore);
	TestEqual(TEXT("carriage request preserves story records"),
		Subsystem->GetRuntimeState().NarrativeProgress.StoryProgressById.Num(),
		StoryCountBefore);
	TestEqual(TEXT("carriage request preserves task records"),
		Subsystem->GetRuntimeState().NarrativeProgress.TaskProgressById.Num(),
		TaskCountBefore);
	TestEqual(TEXT("carriage request preserves tracked task"),
		Subsystem->GetRuntimeState().NarrativeProgress.TrackedTaskId,
		TrackedTaskBefore);
	TestEqual(TEXT("carriage request preserves ordered formation"),
		Subsystem->GetRuntimeState().CardRun.OrderedFormation.Members,
		OrderedFormationBefore);

	Widget->HandleActionClicked(0);
	Button = Cast<UGameXXKDesktopTrainingActionButton>(
		Widget->WidgetTree->FindWidget(TEXT("StoryQuestButton")));
	const UCanvasPanelSlot* OpenSlot = Button ? Cast<UCanvasPanelSlot>(Button->Slot) : nullptr;
	const FVector4 OpenRect = GetStoryQuestRect(true);
	TestTrue(TEXT("Warehouse-open story button shifts with town button"),
		OpenSlot && OpenSlot->GetPosition() == FVector2D(OpenRect.X, OpenRect.Y));

	FDesktopNativeRegionState RegionState;
	RegionState.bExpanded = true;
	RegionState.bStoryQuestVisible = true;
	RegionState.StoryQuestRect = ClosedRect;
	const TArray<FDesktopNativeRegionShape> Shapes =
		BuildDesktopNativeRegionShapes(RegionState);
	TestTrue(TEXT("story button center is a native interactive surface"),
		IsPointInsideDesktopNativeRegionShapes(
			Shapes,
			FVector2D(ClosedRect.X + ClosedRect.Z * 0.5f,
				ClosedRect.Y + ClosedRect.W * 0.5f)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingTownSessionSubsystemTest,
	"GameXXK.DesktopTraining.Workbench.TownSessionOneShot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingTownSessionSubsystemTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKDesktopHudSessionSubsystem* Session =
		NewObject<UGameXXKDesktopHudSessionSubsystem>(TestGameInstance);
	if (!TestNotNull(TEXT("town session subsystem exists"), Session))
	{
		return false;
	}
	FGameXXKDesktopWorkbenchSessionState Expected;
	Expected.bValid = true;
	Expected.bBackpackExpanded = true;
	Expected.bWarehousePanelOpen = true;
	Expected.WarehousePageIndex = 2;
	Expected.ActiveNav = EGameXXKDesktopTrainingNav::Talents;
	Expected.ActiveCenterPage = EGameXXKDesktopTrainingCenterPage::Talents;
	Expected.RightPanel = EGameXXKDesktopTrainingRightPanel::Tools;
	Expected.ActiveToolMode = EGameXXKDesktopToolMode::Socket;
	Expected.ActiveToolCombineKind = EGameXXKToolCombineKind::Gem;
	Expected.SelectedToolSocketIndex = 3;
	Expected.ActiveCharacterRoster = EGameXXKDesktopTrainingCharacterRoster::Npcs;
	Expected.ActiveFormationRoster = EGameXXKDesktopTrainingCharacterRoster::Companions;
	Expected.SelectedStageId = TEXT("Training.Hard.2-3");
	Expected.ActiveTrainingDifficultyIndex = 1;
	Expected.ActiveTrainingChapter = 2;
	Expected.ActiveBackpackCharacterId = TEXT("Npc.YueBai");
	Expected.LastCompanionBackpackCharacterId = TEXT("Companion_Blade_Test");
	Expected.LastNpcBackpackCharacterId = TEXT("Npc.YueBai");
	Expected.FormationCandidateCharacterId = TEXT("Companion_Blade_Test");
	Expected.bCharacterRosterMembersExpanded = true;
	Expected.bSettingsPanelOpen = true;
	Expected.EmbeddedInventory.CharacterId = TEXT("Npc.YueBai");
	Expected.EmbeddedInventory.ActiveCharacterTab = EGameXXKCharacterBackpackTab::Deck;
	Expected.EmbeddedInventory.PendingDeckIds = {TEXT("Card.A"), TEXT("Card.B")};

	Session->StoreForMapTravel(Expected);
	FGameXXKDesktopWorkbenchSessionState Actual;
	TestTrue(TEXT("pending town state is consumed once"),
		Session->ConsumeAfterMapTravel(Actual));
	TestTrue(TEXT("stable workbench state round-trips"), Actual == Expected);
	TestFalse(TEXT("consumed state cannot leak into another map"),
		Session->ConsumeAfterMapTravel(Actual));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingMousePassthroughPolicyTest,
	"GameXXK.DesktopTraining.Workbench.MousePassthroughPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingMousePassthroughPolicyTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKDesktopTrainingLayout;
	FDesktopOverlayMouseState State;
	TestTrue(TEXT("transparent background passes the mouse through"),
		ShouldDesktopOverlayPassMouseThrough(State));

	State.bPointerOverInteractiveSurface = true;
	TestFalse(TEXT("paper and controls keep native mouse input"),
		ShouldDesktopOverlayPassMouseThrough(State));
	State.bPointerOverInteractiveSurface = false;

	State.bCarryingItem = true;
	TestFalse(TEXT("a carried item keeps the complete HUD interactive"),
		ShouldDesktopOverlayPassMouseThrough(State));
	State.bCarryingItem = false;

	State.bMouseCaptured = true;
	TestFalse(TEXT("Slate mouse capture keeps the complete HUD interactive"),
		ShouldDesktopOverlayPassMouseThrough(State));
	State.bMouseCaptured = false;

	State.bDragDropActive = true;
	TestFalse(TEXT("drag-drop keeps the complete HUD interactive"),
		ShouldDesktopOverlayPassMouseThrough(State));
	State.bDragDropActive = false;

	State.bHudDragging = true;
	TestFalse(TEXT("HUD dragging keeps native input until release"),
		ShouldDesktopOverlayPassMouseThrough(State));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingMousePassthroughSynchronizationTest,
	"GameXXK.DesktopTraining.Workbench.MousePassthroughSynchronization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingMousePassthroughSynchronizationTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKDesktopTrainingLayout;
	TestTrue(TEXT("pointer movement refreshes hit testing before dispatch"),
		ShouldSynchronizeDesktopMousePassthrough(EDesktopOverlayMouseSignal::Move));
	TestTrue(TEXT("button presses refresh hit testing before dispatch"),
		ShouldSynchronizeDesktopMousePassthrough(EDesktopOverlayMouseSignal::Button));
	TestTrue(TEXT("wheel input refreshes hit testing before dispatch"),
		ShouldSynchronizeDesktopMousePassthrough(EDesktopOverlayMouseSignal::Wheel));
	TestFalse(TEXT("unrelated native messages do not poll the overlay"),
		ShouldSynchronizeDesktopMousePassthrough(EDesktopOverlayMouseSignal::Other));
	TestEqual(TEXT("desktop carried icons use native client coordinates and HUD scale"),
		DesktopClientPointToReference(
			FVector2D(400.0f, 300.0f),
			0.5f,
			FVector2D(28.0f, 28.0f)),
		FVector2D(772.0f, 572.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingNativeWindowRegionTest,
	"GameXXK.DesktopTraining.Workbench.NativeWindowRegion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingNativeWindowRegionTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKDesktopTrainingLayout;
	FDesktopNativeRegionState Collapsed;
	Collapsed.NoticeHeight = 52.0f;
	const TArray<FDesktopNativeRegionShape> CollapsedShapes =
		BuildDesktopNativeRegionShapes(Collapsed);
	TestTrue(TEXT("collapsed idle strip belongs to the native window"),
		IsPointInsideDesktopNativeRegionShapes(CollapsedShapes, FVector2D(100.0f, 100.0f)));
	TestTrue(TEXT("collapsed notice rail belongs to the native window"),
		IsPointInsideDesktopNativeRegionShapes(CollapsedShapes, FVector2D(100.0f, 225.0f)));
	TestTrue(TEXT("collapsed progress and Tab row remains inside the native window"),
		IsPointInsideDesktopNativeRegionShapes(CollapsedShapes, FVector2D(800.0f, 215.0f)));
	TestFalse(TEXT("space below the collapsed progress and Tab row remains transparent"),
		IsPointInsideDesktopNativeRegionShapes(CollapsedShapes, FVector2D(800.0f, 235.0f)));
	TestFalse(TEXT("a point below the collapsed HUD is outside the native window"),
		IsPointInsideDesktopNativeRegionShapes(CollapsedShapes, FVector2D(100.0f, 260.0f)));

	FDesktopNativeRegionState Expanded;
	Expanded.bExpanded = true;
	const TArray<FDesktopNativeRegionShape> CenterOnlyShapes =
		BuildDesktopNativeRegionShapes(Expanded);
	TestTrue(TEXT("expanded center shell belongs to the native window"),
		IsPointInsideDesktopNativeRegionShapes(CenterOnlyShapes, FVector2D(700.0f, 400.0f)));
	TestTrue(TEXT("expanded progress and Tab row remains inside the native window"),
		IsPointInsideDesktopNativeRegionShapes(CenterOnlyShapes, FVector2D(1200.0f, 215.0f)));
	TestFalse(TEXT("transparent gap below the expanded control row remains outside"),
		IsPointInsideDesktopNativeRegionShapes(CenterOnlyShapes, FVector2D(900.0f, 235.0f)));
	TestFalse(TEXT("closed warehouse leaves its area outside the native window"),
		IsPointInsideDesktopNativeRegionShapes(CenterOnlyShapes, FVector2D(100.0f, 400.0f)));
	TestFalse(TEXT("closed right panel leaves its area outside the native window"),
		IsPointInsideDesktopNativeRegionShapes(CenterOnlyShapes, FVector2D(1500.0f, 400.0f)));

	Expanded.bWarehouseOpen = true;
	Expanded.bRightPanelOpen = true;
	const TArray<FDesktopNativeRegionShape> AllPanelsShapes =
		BuildDesktopNativeRegionShapes(Expanded);
	TestTrue(TEXT("open warehouse joins the native window region"),
		IsPointInsideDesktopNativeRegionShapes(AllPanelsShapes, FVector2D(100.0f, 400.0f)));
	TestTrue(TEXT("open right panel joins the native window region"),
		IsPointInsideDesktopNativeRegionShapes(AllPanelsShapes, FVector2D(1500.0f, 400.0f)));

	Expanded.bWarehouseOpen = false;
	Expanded.bRightPanelOpen = false;
	Expanded.bExitConfirmationOpen = true;
	const TArray<FDesktopNativeRegionShape> ModalShapes =
		BuildDesktopNativeRegionShapes(Expanded);
	TestTrue(TEXT("exit confirmation intentionally owns the complete HUD bounds"),
		IsPointInsideDesktopNativeRegionShapes(ModalShapes, FVector2D(100.0f, 400.0f)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingOverlayProviderRoutingTest,
	"GameXXK.DesktopTraining.Workbench.DesktopOverlayProviderRouting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingOverlayProviderRoutingTest::RunTest(const FString& Parameters)
{
	IGameXXKDesktopOverlayModule& Overlay = IGameXXKDesktopOverlayModule::Get();
	const FIntRect GlassMargins = Overlay.GetCompositionGlassMarginsForTest();
	TestEqual(TEXT("desktop composition extends glass across the complete client area"),
		GlassMargins,
		FIntRect(-1, -1, -1, -1));
	TestTrue(TEXT("only the armed D3D12 overlay title is intercepted"),
		Overlay.ShouldInterceptForTest(
			IGameXXKDesktopOverlayModule::OverlayWindowTitle,
			true,
			ERHIInterfaceType::D3D12));
	TestFalse(TEXT("an unarmed window is delegated"),
		Overlay.ShouldInterceptForTest(
			IGameXXKDesktopOverlayModule::OverlayWindowTitle,
			false,
			ERHIInterfaceType::D3D12));
	TestFalse(TEXT("an unrelated Slate window is delegated"),
		Overlay.ShouldInterceptForTest(
			TEXT("GameXXK"),
			true,
			ERHIInterfaceType::D3D12));
	TestFalse(TEXT("a non-D3D12 RHI is delegated"),
		Overlay.ShouldInterceptForTest(
			IGameXXKDesktopOverlayModule::OverlayWindowTitle,
			true,
			ERHIInterfaceType::Vulkan));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingTransparentSlateWindowContractTest,
	"GameXXK.DesktopTraining.Workbench.TransparentSlateWindowContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingTransparentSlateWindowContractTest::RunTest(const FString& Parameters)
{
	const FVector2D WindowPosition(240.0f, 80.0f);
	const FVector2D WindowSize(1038.0f, 254.0f);
	const TSharedRef<SWindow> Window =
		AGameXXKMVPPlayerController::BuildDesktopTrainingOverlayWindowForTest(
			WindowPosition,
			WindowSize);
	TestEqual(TEXT("desktop HUD requests per-pixel Slate output"),
		Window->GetTransparencySupport(),
		EWindowTransparency::PerPixel);
	TestEqual(TEXT("desktop overlay uses its unique provider tag"),
		Window->GetTitle().ToString(),
		FString(TEXT("GameXXKDesktopOverlay")));
	TestFalse(TEXT("desktop host has no operating-system border"), Window->HasOSWindowBorder());
	TestFalse(TEXT("desktop host has no close box"), Window->HasCloseBox());
	TestFalse(TEXT("desktop host has no minimize box"), Window->HasMinimizeBox());
	TestFalse(TEXT("desktop host has no maximize box"), Window->HasMaximizeBox());
	TestTrue(TEXT("desktop host has no Slate title bar"),
		Window->GetTitleBarSize().IsSet()
			&& FMath::IsNearlyZero(Window->GetTitleBarSize().Get()));
	TestTrue(TEXT("desktop host still accepts Tab and button input"), Window->AcceptsInput());
	TestTrue(TEXT("desktop HUD starts topmost"),
		Window->IsTopmostWindow());
	TestTrue(TEXT("desktop window occupies only the requested HUD rectangle"),
		FVector2D(Window->GetInitialDesiredSizeInScreen()).Equals(WindowSize, 0.01f));
	TestFalse(TEXT("ordinary editor PIE keeps the editor viewport visible"),
		AGameXXKMVPPlayerController::ShouldHideDesktopTrainingGameViewportForTest(true, false));
	TestTrue(TEXT("UnrealEditor -game hides the redundant opaque game viewport"),
		AGameXXKMVPPlayerController::ShouldHideDesktopTrainingGameViewportForTest(true, true));
	TestTrue(TEXT("packaged game hides the redundant opaque game viewport"),
		AGameXXKMVPPlayerController::ShouldHideDesktopTrainingGameViewportForTest(false, false));
	TestFalse(TEXT("a requested but unattached overlay never hides the viewport"),
		AGameXXKMVPPlayerController::ShouldHideViewportAfterOverlayAttachForTest(true, false));
	TestTrue(TEXT("an attached requested overlay may hide the viewport"),
		AGameXXKMVPPlayerController::ShouldHideViewportAfterOverlayAttachForTest(true, true));
	TestFalse(TEXT("an unrequested overlay never hides the viewport"),
		AGameXXKMVPPlayerController::ShouldHideViewportAfterOverlayAttachForTest(false, true));
	TestTrue(TEXT("overlay is attempted before a session failure"),
		AGameXXKMVPPlayerController::ShouldAttemptDesktopOverlayAfterFailureForTest(false));
	TestFalse(TEXT("overlay failure is latched instead of retried on every button rebuild"),
		AGameXXKMVPPlayerController::ShouldAttemptDesktopOverlayAfterFailureForTest(true));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingOverlayReuseAttachmentTest,
	"GameXXK.DesktopTraining.Workbench.DesktopOverlayReuseRequiresNativeReattach",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingOverlayReuseAttachmentTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("a composed existing overlay must restore the workbench native attachment after challenge return"),
		AGameXXKMVPPlayerController::ShouldRefreshExistingDesktopOverlayAttachmentForTest(
			true,
			true,
			true));
	TestFalse(TEXT("a missing overlay window cannot restore an attachment"),
		AGameXXKMVPPlayerController::ShouldRefreshExistingDesktopOverlayAttachmentForTest(
			false,
			true,
			true));
	TestFalse(TEXT("a failed composition cannot reuse the overlay"),
		AGameXXKMVPPlayerController::ShouldRefreshExistingDesktopOverlayAttachmentForTest(
			true,
			false,
			true));
	TestFalse(TEXT("a missing workbench cannot restore an attachment"),
		AGameXXKMVPPlayerController::ShouldRefreshExistingDesktopOverlayAttachmentForTest(
			true,
			true,
			false));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingRendererStencilAlphaContractTest,
	"GameXXK.DesktopTraining.Workbench.RendererStencilAlphaContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingRendererStencilAlphaContractTest::RunTest(const FString& Parameters)
{
	const IConsoleVariable* CustomDepth =
		IConsoleManager::Get().FindConsoleVariable(TEXT("r.CustomDepth"));
	const IConsoleVariable* PropagateAlpha =
		IConsoleManager::Get().FindConsoleVariable(TEXT("r.PostProcessing.PropagateAlpha"));
	TestNotNull(TEXT("renderer exposes the custom depth-stencil setting"), CustomDepth);
	TestNotNull(TEXT("renderer exposes the alpha-output setting"), PropagateAlpha);
	TestEqual(TEXT("custom depth-stencil pass is enabled with stencil"),
		CustomDepth ? CustomDepth->GetInt() : INDEX_NONE,
		3);
	TestFalse(TEXT("safe single-window HUD does not request scene-alpha propagation"),
		PropagateAlpha && PropagateAlpha->GetBool());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingHudScaleSettingsTest,
	"GameXXK.DesktopTraining.Workbench.HudScaleSettings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingHudScaleSettingsTest::RunTest(const FString& Parameters)
{
	static const TCHAR* SettingsSection = TEXT("/Script/GameXXK.DesktopHudSettings");
	static const TCHAR* ScaleKey = TEXT("HudScalePercent");
	int32 PreviousScale = 100;
	const bool bHadPreviousScale = GConfig
		&& GConfig->GetInt(SettingsSection, ScaleKey, PreviousScale, GGameUserSettingsIni);
	ON_SCOPE_EXIT
	{
		if (GConfig)
		{
			if (bHadPreviousScale)
			{
				GConfig->SetInt(SettingsSection, ScaleKey, PreviousScale, GGameUserSettingsIni);
			}
			else
			{
				GConfig->RemoveKey(SettingsSection, ScaleKey, GGameUserSettingsIni);
			}
			GConfig->Flush(false, GGameUserSettingsIni);
		}
	};
	if (!GConfig)
	{
		AddError(TEXT("HUD scale settings require GConfig"));
		return false;
	}
	GConfig->SetInt(SettingsSection, ScaleKey, 100, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);

	UGameXXKMVPSubsystem* Subsystem =
		NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	UGameXXKDesktopTrainingWorkbenchWidget* Widget =
		NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	if (!TestTrue(TEXT("HUD scale fixture starts"),
		Subsystem && Subsystem->StartGame() && Widget))
	{
		return false;
	}
	Widget->SetMVPSubsystem(Subsystem);
	Widget->ConstructForTest();
	if (!TestTrue(TEXT("HUD scale fixture opens Backpack"),
		Widget->OpenWorkbench() && Widget->OpenBackpack()))
	{
		return false;
	}
	TestEqual(TEXT("HUD scale defaults to 100 percent"), Widget->GetHudScalePercentForTest(), 100);
	TestNull(TEXT("the unimplemented Shop toolbar button is removed"),
		Widget->WidgetTree ? Widget->WidgetTree->FindWidget(TEXT("TopToolbarShop")) : nullptr);
	UGameXXKDesktopTrainingActionButton* SettingsButton = Widget->WidgetTree
		? Cast<UGameXXKDesktopTrainingActionButton>(
			Widget->WidgetTree->FindWidget(TEXT("TopToolbarSettings")))
		: nullptr;
	if (!TestNotNull(TEXT("Settings replaces Shop immediately left of Exit"), SettingsButton))
	{
		return false;
	}
	UImage* SettingsIcon = Widget->WidgetTree
		? Cast<UImage>(Widget->WidgetTree->FindWidget(TEXT("TopToolbarSettingsIcon")))
		: nullptr;
	TestTrue(TEXT("Settings uses the existing project gear icon"),
		SettingsIcon
		&& GetImageResourcePath(SettingsIcon).Contains(TEXT("T_TownPsd_HudSettings")));
	SettingsButton->OnClicked.Broadcast();
	Widget->TickForTest(0.0f);
	TestNotNull(TEXT("Settings opens a HUD settings panel"),
		Widget->WidgetTree ? Widget->WidgetTree->FindWidget(TEXT("DesktopHudSettingsPanel")) : nullptr);
	UGameXXKDesktopTrainingActionButton* HalfScaleButton = Widget->WidgetTree
		? Cast<UGameXXKDesktopTrainingActionButton>(
			Widget->WidgetTree->FindWidget(TEXT("HudScale50Button")))
		: nullptr;
	if (!TestNotNull(TEXT("HUD settings exposes the approved 50 percent option"), HalfScaleButton))
	{
		return false;
	}
	HalfScaleButton->OnClicked.Broadcast();
	Widget->TickForTest(0.0f);
	TestEqual(TEXT("50 percent selection updates the live preference"),
		Widget->GetHudScalePercentForTest(),
		50);
	int32 PersistedScale = 0;
	TestTrue(TEXT("HUD scale preference is written to config"),
		GConfig->GetInt(SettingsSection, ScaleKey, PersistedScale, GGameUserSettingsIni));
	TestEqual(TEXT("HUD scale config persists 50 percent"), PersistedScale, 50);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingIdleStripControlRailTest,
	"GameXXK.DesktopTraining.Workbench.IdleStripControlRailAndRetryStates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingIdleStripControlRailTest::RunTest(
	const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem =
		NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	UGameXXKDesktopTrainingWorkbenchWidget* Widget =
		NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	if (!TestTrue(TEXT("idle-strip control fixture starts"),
		Subsystem && Subsystem->StartGame() && Widget))
	{
		return false;
	}
	Widget->SetMVPSubsystem(Subsystem);
	// This test verifies local reference coordinates, not the persisted choice
	// to expand upward when the real desktop HUD was dragged near the taskbar.
	Widget->SetPresentationMode(EGameXXKDesktopHudPresentationMode::TownViewport);
	Widget->ConstructForTest();
	if (!TestTrue(TEXT("idle-strip control fixture opens the collapsed workbench"),
		Widget->OpenWorkbench()))
	{
		return false;
	}

	USizeBox* GroupReference = Widget->WidgetTree
		? Cast<USizeBox>(Widget->WidgetTree->FindWidget(TEXT("TrainingIdleGroupReference")))
		: nullptr;
	TestNotNull(TEXT("idle strip owns a logical unscaled group reference"), GroupReference);
	TestTrue(TEXT("idle group reference is 1038x202 before uniform scaling"),
		GroupReference
		&& FMath::IsNearlyEqual(GroupReference->GetWidthOverride(), 1038.0f)
		&& FMath::IsNearlyEqual(GroupReference->GetHeightOverride(), 202.0f));
	UWidget* NoticePanel = Widget->WidgetTree
		? Widget->WidgetTree->FindWidget(TEXT("DesktopInventoryNoticePanel"))
		: nullptr;
	UTextBlock* EmptyNoticeText = Widget->WidgetTree
		? Cast<UTextBlock>(Widget->WidgetTree->FindWidget(TEXT("DesktopInventoryNoticeText")))
		: nullptr;
	TestTrue(TEXT("the one-line battle report remains visible before the first event"),
		NoticePanel
		&& NoticePanel->GetVisibility() != ESlateVisibility::Collapsed
		&& NoticePanel->GetVisibility() != ESlateVisibility::Hidden);
	TestTrue(TEXT("an empty battle report uses a stable placeholder instead of disappearing"),
		EmptyNoticeText && EmptyNoticeText->GetText().ToString().Contains(TEXT("暂无记录")));

	const auto TestLocalRect = [this, Widget](
		const FName WidgetName,
		const FVector2D Position,
		const FVector2D Size)
	{
		UWidget* Child = Widget->WidgetTree
			? Widget->WidgetTree->FindWidget(WidgetName)
			: nullptr;
		const UCanvasPanelSlot* Slot = Child ? Cast<UCanvasPanelSlot>(Child->Slot) : nullptr;
		if (TestNotNull(*FString::Printf(TEXT("%s owns a local canvas slot"), *WidgetName.ToString()), Slot))
		{
			if (!Slot->GetPosition().Equals(Position, KINDA_SMALL_NUMBER))
			{
				AddInfo(FString::Printf(
					TEXT("%s actual local position %s, expected %s"),
					*WidgetName.ToString(),
					*Slot->GetPosition().ToString(),
					*Position.ToString()));
			}
			TestEqual(*FString::Printf(TEXT("%s local position"), *WidgetName.ToString()), Slot->GetPosition(), Position);
			TestEqual(*FString::Printf(TEXT("%s local size"), *WidgetName.ToString()), Slot->GetSize(), Size);
		}
	};
	TestLocalRect(TEXT("TrainingNormalChestButton"), FVector2D(953.0f, 8.0f), FVector2D(72.0f, 72.0f));
	TestLocalRect(TEXT("TrainingAdvancedChestButton"), FVector2D(953.0f, 84.0f), FVector2D(72.0f, 72.0f));
	TestLocalRect(TEXT("TravelRetryButton"), FVector2D(893.0f, 18.0f), FVector2D(52.0f, 52.0f));
	TestLocalRect(TEXT("IdleStripFoldButton"), FVector2D(440.5f, 202.0f), FVector2D(72.0f, 24.0f));
	TestLocalRect(TEXT("TrainingWaveProgressPanel"), FVector2D(533.0f, 202.0f), FVector2D(420.0f, 24.0f));
	TestLocalRect(TEXT("BackpackTabToggleButton"), FVector2D(953.0f, 202.0f), FVector2D(72.0f, 24.0f));

	UImage* NormalChestIcon = Widget->WidgetTree
		? Cast<UImage>(Widget->WidgetTree->FindWidget(TEXT("TrainingNormalChestIcon")))
		: nullptr;
	UImage* AdvancedChestIcon = Widget->WidgetTree
		? Cast<UImage>(Widget->WidgetTree->FindWidget(TEXT("TrainingAdvancedChestIcon")))
		: nullptr;
	TestTrue(TEXT("normal chest icon fills the approved 66px footprint inside its 72px button"),
		NormalChestIcon && NormalChestIcon->GetBrush().ImageSize == FVector2D(66.0f, 66.0f));
	TestTrue(TEXT("advanced chest icon fills the approved 66px footprint inside its 72px button"),
		AdvancedChestIcon && AdvancedChestIcon->GetBrush().ImageSize == FVector2D(66.0f, 66.0f));

	const TCHAR* ExpectedWaveMarkerAssets[] = {
		TEXT("T_TrainingWaveMarkerBoss"),
		TEXT("T_TrainingWaveMarkerNormal"),
		TEXT("T_TrainingWaveMarkerElite"),
		TEXT("T_TrainingWaveMarkerNormal"),
		TEXT("T_TrainingWaveMarkerElite"),
		TEXT("T_TrainingWaveMarkerNormal"),
		TEXT("T_TrainingWaveMarkerNormal")};
	for (int32 MarkerIndex = 0; MarkerIndex < UE_ARRAY_COUNT(ExpectedWaveMarkerAssets); ++MarkerIndex)
	{
		UImage* Marker = Widget->WidgetTree
			? Cast<UImage>(Widget->WidgetTree->FindWidget(
				*FString::Printf(TEXT("TrainingWaveMarker_%d"), MarkerIndex)))
			: nullptr;
		TestTrue(
			*FString::Printf(TEXT("wave marker %d follows the approved right-to-left sequence"), MarkerIndex),
			Marker && GetImageResourcePath(Marker).Contains(ExpectedWaveMarkerAssets[MarkerIndex]));
	}
	UTextBlock* StageLabel = Widget->WidgetTree
		? Cast<UTextBlock>(Widget->WidgetTree->FindWidget(TEXT("TrainingWaveStageText")))
		: nullptr;
	UTextBlock* WaveLabel = Widget->WidgetTree
		? Cast<UTextBlock>(Widget->WidgetTree->FindWidget(TEXT("TrainingWaveIndexText")))
		: nullptr;
	TestTrue(TEXT("wave progress names the current normal stage"),
		StageLabel && StageLabel->GetText().ToString().Contains(TEXT("普通")));
	TestEqual(TEXT("wave progress starts at the rightmost first wave"),
		WaveLabel ? WaveLabel->GetText().ToString() : FString(),
		FString(TEXT("1/7")));

	TestTrue(TEXT("retry circular base package exists"), FPackageName::DoesPackageExist(
		TEXT("/Game/GameXXK/UI/Training/IdleStrip/T_TrainingRetryButtonBase")));
	TestTrue(TEXT("retry enabled icon package exists"), FPackageName::DoesPackageExist(
		TEXT("/Game/GameXXK/UI/Training/IdleStrip/T_TrainingRetryIconEnabled")));
	TestTrue(TEXT("retry disabled icon package exists"), FPackageName::DoesPackageExist(
		TEXT("/Game/GameXXK/UI/Training/IdleStrip/T_TrainingRetryIconDisabled")));

	UButton* RetryButton = Widget->WidgetTree
		? Cast<UButton>(Widget->WidgetTree->FindWidget(TEXT("TravelRetryButton")))
		: nullptr;
	UImage* RetryIcon = RetryButton ? Cast<UImage>(RetryButton->GetContent()) : nullptr;
	TestTrue(TEXT("enabled retry button uses the circular paper base"),
		GetButtonNormalResourcePath(RetryButton).Contains(TEXT("T_TrainingRetryButtonBase")));
	TestTrue(TEXT("enabled retry state uses the jade double-arrow icon"),
		GetImageResourcePath(RetryIcon).Contains(TEXT("T_TrainingRetryIconEnabled")));
	TestTrue(TEXT("enabled retry tooltip names the active auto-retry state"),
		RetryButton && RetryButton->GetToolTipText().ToString().Contains(TEXT("已开启")));

	Widget->HandleActionClicked(10);
	Widget->TickForTest(0.0f);
	RetryButton = Widget->WidgetTree
		? Cast<UButton>(Widget->WidgetTree->FindWidget(TEXT("TravelRetryButton")))
		: nullptr;
	RetryIcon = RetryButton ? Cast<UImage>(RetryButton->GetContent()) : nullptr;
	TestTrue(TEXT("disabled retry state keeps the same circular paper base"),
		GetButtonNormalResourcePath(RetryButton).Contains(TEXT("T_TrainingRetryButtonBase")));
	TestTrue(TEXT("disabled retry state uses the grey double-arrow icon"),
		GetImageResourcePath(RetryIcon).Contains(TEXT("T_TrainingRetryIconDisabled")));
	TestTrue(TEXT("disabled retry tooltip names the inactive auto-retry state"),
		RetryButton && RetryButton->GetToolTipText().ToString().Contains(TEXT("已关闭")));

	UGameXXKDesktopTrainingActionButton* FoldButton = Widget->WidgetTree
		? Cast<UGameXXKDesktopTrainingActionButton>(
			Widget->WidgetTree->FindWidget(TEXT("IdleStripFoldButton")))
		: nullptr;
	if (!TestNotNull(TEXT("single-line summary owns the 72x24 strip fold button"), FoldButton))
	{
		return false;
	}
	FoldButton->OnClicked.Broadcast();
	Widget->TickForTest(0.0f);
	UWidget* TravelStrip = Widget->WidgetTree
		? Widget->WidgetTree->FindWidget(TEXT("TrainingTravelStrip"))
		: nullptr;
	TestTrue(TEXT("folding hides the complete animated travel strip"),
		!TravelStrip || TravelStrip->GetVisibility() == ESlateVisibility::Collapsed);
	TestLocalRect(TEXT("IdleStripFoldButton"), FVector2D(440.5f, 0.0f), FVector2D(72.0f, 24.0f));
	TestLocalRect(TEXT("TrainingWaveProgressPanel"), FVector2D(533.0f, 0.0f), FVector2D(420.0f, 24.0f));
	TestLocalRect(TEXT("BackpackTabToggleButton"), FVector2D(953.0f, 0.0f), FVector2D(72.0f, 24.0f));
	UTextBlock* FoldedNormalChestText = Widget->WidgetTree
		? Cast<UTextBlock>(Widget->WidgetTree->FindWidget(TEXT("TrainingFoldedNormalChestText")))
		: nullptr;
	UTextBlock* FoldedAdvancedChestText = Widget->WidgetTree
		? Cast<UTextBlock>(Widget->WidgetTree->FindWidget(TEXT("TrainingFoldedAdvancedChestText")))
		: nullptr;
	TestTrue(TEXT("folded summary appends the normal chest count after Tab"),
		FoldedNormalChestText
		&& FoldedNormalChestText->GetText().ToString().Contains(TEXT("普通")));
	TestTrue(TEXT("folded summary appends the advanced chest count after Tab"),
		FoldedAdvancedChestText
		&& FoldedAdvancedChestText->GetText().ToString().Contains(TEXT("高级")));
	FoldButton = Widget->WidgetTree
		? Cast<UGameXXKDesktopTrainingActionButton>(
			Widget->WidgetTree->FindWidget(TEXT("IdleStripFoldButton")))
		: nullptr;
	if (!TestNotNull(TEXT("folded summary keeps the down-expand control available"), FoldButton))
	{
		return false;
	}
	FoldButton->OnClicked.Broadcast();
	Widget->TickForTest(0.0f);
	UGameXXKDesktopTrainingActionButton* FoldedTabButton = Widget->WidgetTree
		? Cast<UGameXXKDesktopTrainingActionButton>(
			Widget->WidgetTree->FindWidget(TEXT("BackpackTabToggleButton")))
		: nullptr;
	if (!TestNotNull(TEXT("folded summary keeps the Tab entry available"), FoldedTabButton))
	{
		return false;
	}
	FoldedTabButton->OnClicked.Broadcast();
	Widget->TickForTest(0.0f);
	TestNotNull(TEXT("expanded Backpack keeps the compact fold control"),
		Widget->WidgetTree ? Widget->WidgetTree->FindWidget(TEXT("IdleStripFoldButton")) : nullptr);
	TestNotNull(TEXT("expanded Backpack keeps a shortened wave progress rail"),
		Widget->WidgetTree ? Widget->WidgetTree->FindWidget(TEXT("TrainingWaveProgressPanel")) : nullptr);
	TestLocalRect(TEXT("IdleStripFoldButton"), FVector2D(837.5f, 210.0f), FVector2D(72.0f, 24.0f));
	TestLocalRect(TEXT("TrainingWaveProgressPanel"), FVector2D(930.0f, 210.0f), FVector2D(340.0f, 24.0f));
	TestLocalRect(TEXT("BackpackTabToggleButton"), FVector2D(1270.0f, 210.0f), FVector2D(72.0f, 24.0f));
	TestLocalRect(TEXT("TrainingNormalChestButton"), FVector2D(1270.0f, 25.0f), FVector2D(72.0f, 72.0f));
	TestLocalRect(TEXT("TrainingAdvancedChestButton"), FVector2D(1270.0f, 101.0f), FVector2D(72.0f, 72.0f));
	TestLocalRect(TEXT("TravelRetryButton"), FVector2D(1210.0f, 35.0f), FVector2D(52.0f, 52.0f));
	TestLocalRect(TEXT("TopToolbarAlwaysOnTop"), FVector2D(1028.0f, 252.0f), FVector2D(42.0f, 36.0f));
	TestLocalRect(TEXT("TopToolbarExit"), FVector2D(1216.0f, 252.0f), FVector2D(42.0f, 36.0f));
	TestLocalRect(TEXT("BackpackGoldIcon"), FVector2D(1034.0f, 291.0f), FVector2D(30.0f, 30.0f));
	TestLocalRect(TEXT("BackpackGoldText"), FVector2D(1068.0f, 292.0f), FVector2D(100.0f, 30.0f));
	TestLocalRect(TEXT("BackpackPanelCloseButton"), FVector2D(1272.0f, 252.0f), FVector2D(44.0f, 44.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingNoticeRailStateMachineTest,
	"GameXXK.DesktopTraining.Workbench.NoticeRailStateMachine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingNoticeRailStateMachineTest::RunTest(
	const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem =
		NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	UGameXXKDesktopTrainingWorkbenchWidget* Widget =
		NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	if (!TestTrue(TEXT("notice-rail fixture starts"),
		Subsystem && Subsystem->StartGame() && Widget))
	{
		return false;
	}
	Widget->SetMVPSubsystem(Subsystem);
	Widget->ConstructForTest();
	if (!TestTrue(TEXT("notice-rail fixture opens collapsed"), Widget->OpenWorkbench()))
	{
		return false;
	}

	// Existing workbench notices feed the new session history without requiring
	// a private test-only append path.
	for (int32 Index = 0; Index < 24; ++Index)
	{
		Widget->HandleActionClicked(18);
	}

	auto FindActionButton = [Widget](const FName Name)
	{
		return Widget->WidgetTree
			? Cast<UGameXXKDesktopTrainingActionButton>(Widget->WidgetTree->FindWidget(Name))
			: nullptr;
	};
	auto IsVisible = [](const UWidget* Candidate)
	{
		return Candidate
			&& Candidate->GetVisibility() != ESlateVisibility::Collapsed
			&& Candidate->GetVisibility() != ESlateVisibility::Hidden;
	};
	auto CountNoticeLines = [Widget]()
	{
		int32 Count = 0;
		while (Widget->WidgetTree
			&& Widget->WidgetTree->FindWidget(
				*FString::Printf(TEXT("DesktopNoticeLine_%d"), Count)))
		{
			++Count;
		}
		return Count;
	};

	UGameXXKDesktopTrainingActionButton* Surface = FindActionButton(TEXT("DesktopNoticeSurfaceButton"));
	UWidget* RecordsBar = Widget->WidgetTree
		? Widget->WidgetTree->FindWidget(TEXT("DesktopNoticeRecordsBar"))
		: nullptr;
	TestNotNull(TEXT("single-line notice owns an interactive hover surface"), Surface);
	TestEqual(TEXT("notice defaults to exactly one rendered line"), CountNoticeLines(), 1);
	TestFalse(TEXT("RECORDS bar is hidden before hover"), IsVisible(RecordsBar));
	if (!Surface)
	{
		return false;
	}
	Surface->OnHovered.Broadcast();
	TestTrue(TEXT("hover reveals the complete RECORDS operation bar"), IsVisible(RecordsBar));

	UGameXXKDesktopTrainingActionButton* Expand = FindActionButton(TEXT("DesktopNoticeExpandButton"));
	if (!TestNotNull(TEXT("single-line RECORDS bar exposes Expand"), Expand))
	{
		return false;
	}
	Expand->OnClicked.Broadcast();
	Widget->TickForTest(0.0f);
	TestEqual(TEXT("first expand enters the five-line medium state"), CountNoticeLines(), 5);
	TestNotNull(TEXT("medium state exposes Collapse"), FindActionButton(TEXT("DesktopNoticeCollapseButton")));
	Expand = FindActionButton(TEXT("DesktopNoticeExpandButton"));
	if (!TestNotNull(TEXT("medium state still exposes Expand"), Expand))
	{
		return false;
	}
	Expand->OnClicked.Broadcast();
	Widget->TickForTest(0.0f);
	TestEqual(TEXT("second expand enters the eighteen-line long state"), CountNoticeLines(), 18);
	TestNull(TEXT("long state has no further Expand button"), FindActionButton(TEXT("DesktopNoticeExpandButton")));
	TestNotNull(TEXT("long state keeps Collapse"), FindActionButton(TEXT("DesktopNoticeCollapseButton")));

	// Tab-open is a passive one-line projection. It must ignore hover and keep
	// the prior long state available for the same session after Tab closes.
	Widget->HandleActionClicked(60);
	Widget->TickForTest(0.0f);
	Surface = FindActionButton(TEXT("DesktopNoticeSurfaceButton"));
	RecordsBar = Widget->WidgetTree
		? Widget->WidgetTree->FindWidget(TEXT("DesktopNoticeRecordsBar"))
		: nullptr;
	TestEqual(TEXT("Tab-open forces a passive one-line projection"), CountNoticeLines(), 1);
	TestTrue(TEXT("Tab-open notice surface is input transparent"),
		Surface && Surface->GetVisibility() == ESlateVisibility::HitTestInvisible);
	if (Surface)
	{
		Surface->OnHovered.Broadcast();
	}
	TestFalse(TEXT("Tab-open hover cannot reveal RECORDS"), IsVisible(RecordsBar));

	Widget->HandleActionClicked(60);
	Widget->TickForTest(0.0f);
	TestEqual(TEXT("closing Tab restores the prior session long state"), CountNoticeLines(), 18);
	Surface = FindActionButton(TEXT("DesktopNoticeSurfaceButton"));
	if (Surface)
	{
		Surface->OnHovered.Broadcast();
	}
	UGameXXKDesktopTrainingActionButton* Settings =
		FindActionButton(TEXT("DesktopNoticeSettingsButton"));
	if (!TestNotNull(TEXT("RECORDS bar exposes Settings"), Settings))
	{
		return false;
	}
	Settings->OnClicked.Broadcast();
	Widget->TickForTest(0.0f);
	UWidget* SettingsPanel = Widget->WidgetTree
		? Widget->WidgetTree->FindWidget(TEXT("DesktopNoticeSettingsPanel"))
		: nullptr;
	TestTrue(TEXT("Settings opens the category panel"), IsVisible(SettingsPanel));
	TArray<UWidget*> AllWidgets;
	Widget->WidgetTree->GetAllWidgets(AllWidgets);
	FString SettingsText;
	for (const UWidget* Candidate : AllWidgets)
	{
		if (const UTextBlock* Text = Cast<UTextBlock>(Candidate))
		{
			SettingsText += Text->GetText().ToString();
			SettingsText += TEXT("\n");
		}
	}
	TestFalse(TEXT("deferred battle information is absent from Settings"),
		SettingsText.Contains(TEXT("战斗信息")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingNoticeCategoryPersistenceTest,
	"GameXXK.DesktopTraining.Workbench.NoticeCategoryPersistence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingNoticeCategoryPersistenceTest::RunTest(
	const FString& Parameters)
{
	static const TCHAR* SettingsSection =
		TEXT("/Script/GameXXK.DesktopTrainingNoticeSettings");
	static const TCHAR* SystemKey = TEXT("System");
	bool PreviousSystemValue = true;
	const bool bHadPreviousSystemValue = GConfig
		&& GConfig->GetBool(SettingsSection, SystemKey, PreviousSystemValue, GGameUserSettingsIni);
	ON_SCOPE_EXIT
	{
		if (GConfig)
		{
			if (bHadPreviousSystemValue)
			{
				GConfig->SetBool(SettingsSection, SystemKey, PreviousSystemValue, GGameUserSettingsIni);
			}
			else
			{
				GConfig->RemoveKey(SettingsSection, SystemKey, GGameUserSettingsIni);
			}
			GConfig->Flush(false, GGameUserSettingsIni);
		}
	};
	if (!GConfig)
	{
		AddError(TEXT("notice-category persistence requires GConfig"));
		return false;
	}
	GConfig->SetBool(SettingsSection, SystemKey, true, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);

	auto BuildWidget = [this]() -> UGameXXKDesktopTrainingWorkbenchWidget*
	{
		UGameXXKMVPSubsystem* Subsystem =
			NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
		UGameXXKDesktopTrainingWorkbenchWidget* Widget =
			NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
		if (!Subsystem || !Subsystem->StartGame() || !Widget)
		{
			return nullptr;
		}
		Widget->SetMVPSubsystem(Subsystem);
		Widget->ConstructForTest();
		return Widget->OpenWorkbench() ? Widget : nullptr;
	};
	auto HoverAndOpenSettings = [](UGameXXKDesktopTrainingWorkbenchWidget* Widget)
	{
		UGameXXKDesktopTrainingActionButton* Surface = Widget && Widget->WidgetTree
			? Cast<UGameXXKDesktopTrainingActionButton>(
				Widget->WidgetTree->FindWidget(TEXT("DesktopNoticeSurfaceButton")))
			: nullptr;
		if (Surface)
		{
			Surface->OnHovered.Broadcast();
		}
		UGameXXKDesktopTrainingActionButton* Settings = Widget && Widget->WidgetTree
			? Cast<UGameXXKDesktopTrainingActionButton>(
				Widget->WidgetTree->FindWidget(TEXT("DesktopNoticeSettingsButton")))
			: nullptr;
		if (Settings)
		{
			Settings->OnClicked.Broadcast();
			Widget->TickForTest(0.0f);
		}
		return Settings != nullptr;
	};

	UGameXXKDesktopTrainingWorkbenchWidget* First = BuildWidget();
	if (!TestNotNull(TEXT("first persistence widget opens"), First)
		|| !TestTrue(TEXT("first persistence widget opens Settings"), HoverAndOpenSettings(First)))
	{
		return false;
	}
	UGameXXKDesktopTrainingActionButton* FirstSystemToggle = First->WidgetTree
		? Cast<UGameXXKDesktopTrainingActionButton>(
			First->WidgetTree->FindWidget(TEXT("DesktopNoticeCategoryButton_10")))
		: nullptr;
	if (!TestNotNull(TEXT("Settings exposes System category"), FirstSystemToggle))
	{
		return false;
	}
	FirstSystemToggle->OnClicked.Broadcast();
	First->TickForTest(0.0f);
	bool StoredSystemValue = true;
	TestTrue(TEXT("System category writes a persistent config value"),
		GConfig->GetBool(SettingsSection, SystemKey, StoredSystemValue, GGameUserSettingsIni));
	TestFalse(TEXT("disabled System category persists as false"), StoredSystemValue);

	UGameXXKDesktopTrainingWorkbenchWidget* Second = BuildWidget();
	if (!TestNotNull(TEXT("second persistence widget opens"), Second))
	{
		return false;
	}
	Second->HandleActionClicked(18);
	TestEqual(TEXT("a filtered System notice remains in history but not in the visible line"),
		GetNamedText(Second->WidgetTree, TEXT("DesktopInventoryNoticeText")),
		FString(TEXT("暂无记录")));
	TestEqual(TEXT("a new widget still starts in the one-line state"),
		Second->WidgetTree && Second->WidgetTree->FindWidget(TEXT("DesktopNoticeLine_0")) ? 1 : 0,
		1);
	TestNull(TEXT("display size is session-only and does not reopen medium/long"),
		Second->WidgetTree ? Second->WidgetTree->FindWidget(TEXT("DesktopNoticeLine_1")) : nullptr);

	// Restore System through the same production Settings control before the
	// scope guard restores the user's original pre-test value.
	if (HoverAndOpenSettings(Second))
	{
		if (UGameXXKDesktopTrainingActionButton* SecondSystemToggle = Second->WidgetTree
			? Cast<UGameXXKDesktopTrainingActionButton>(
				Second->WidgetTree->FindWidget(TEXT("DesktopNoticeCategoryButton_10")))
			: nullptr)
		{
			SecondSystemToggle->OnClicked.Broadcast();
			Second->TickForTest(0.0f);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingNoticeWheelTest,
	"GameXXK.DesktopTraining.Workbench.NoticeWheelHistory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingNoticeWheelTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem =
		NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	UGameXXKDesktopTrainingWorkbenchWidget* Widget =
		NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	if (!TestTrue(TEXT("notice-wheel fixture starts"),
		Subsystem && Subsystem->StartGame() && Widget))
	{
		return false;
	}
	Widget->SetMVPSubsystem(Subsystem);
	Widget->ConstructForTest();
	if (!TestTrue(TEXT("notice-wheel fixture opens"), Widget->OpenWorkbench()))
	{
		return false;
	}
	for (int32 Index = 0; Index < 12; ++Index)
	{
		Widget->HandleActionClicked(Index % 2 == 0 ? 18 : 19);
	}
	UGameXXKDesktopTrainingActionButton* Surface = Widget->WidgetTree
		? Cast<UGameXXKDesktopTrainingActionButton>(
			Widget->WidgetTree->FindWidget(TEXT("DesktopNoticeSurfaceButton")))
		: nullptr;
	if (!TestNotNull(TEXT("notice-wheel surface exists"), Surface))
	{
		return false;
	}
	TestFalse(TEXT("single-line state ignores the mouse wheel"), Surface->HandleMouseWheel(1.0f));
	Surface->OnHovered.Broadcast();
	UGameXXKDesktopTrainingActionButton* Expand = Cast<UGameXXKDesktopTrainingActionButton>(
		Widget->WidgetTree->FindWidget(TEXT("DesktopNoticeExpandButton")));
	if (!TestNotNull(TEXT("notice-wheel fixture exposes Expand"), Expand))
	{
		return false;
	}
	Expand->OnClicked.Broadcast();
	Widget->TickForTest(0.0f);
	Surface = Cast<UGameXXKDesktopTrainingActionButton>(
		Widget->WidgetTree->FindWidget(TEXT("DesktopNoticeSurfaceButton")));
	TestTrue(TEXT("medium state consumes upward wheel input"),
		Surface && Surface->HandleMouseWheel(1.0f));
	TestEqual(TEXT("upward wheel moves one message page step into older history"),
		Widget->GetNoticeScrollOffsetForTest(),
		1);
	TestTrue(TEXT("medium state consumes downward wheel input"),
		Surface && Surface->HandleMouseWheel(-1.0f));
	TestEqual(TEXT("downward wheel returns to the newest history"),
		Widget->GetNoticeScrollOffsetForTest(),
		0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingNoticeCategoryRoutingTest,
	"GameXXK.DesktopTraining.Workbench.NoticeCategoryRouting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingNoticeCategoryRoutingTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem =
		NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	UGameXXKDesktopTrainingWorkbenchWidget* Widget =
		NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	if (!TestTrue(TEXT("notice-category routing fixture starts"),
		Subsystem && Subsystem->StartGame() && Widget))
	{
		return false;
	}
	Widget->SetMVPSubsystem(Subsystem);
	Widget->ConstructForTest();
	if (!TestTrue(TEXT("notice-category routing fixture opens"), Widget->OpenWorkbench()))
	{
		return false;
	}
	Widget->TickForTest(1.0f);
	Subsystem->GetMutableRuntimeState().PlayerLevel += 1;
	Widget->TickForTest(1.0f);
	TestEqual(TEXT("live level changes route to CharacterLevelUp"),
		Widget->GetLastNoticeCategoryForTest(),
		EGameXXKDesktopNoticeCategory::CharacterLevelUp);

	FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
	FGameXXKTrainingChestToken Token;
	Token.Tier = EGameXXKTrainingRewardTier::NormalChest;
	Token.AcquisitionOrdinal = ++State.Training.NextChestAcquisitionOrdinal;
	State.Training.OwnedChestTokens.Add(Token);
	Widget->HandleActionClicked(600);
	TestEqual(TEXT("opening a chest routes to ChestOpenResult"),
		Widget->GetLastNoticeCategoryForTest(),
		EGameXXKDesktopNoticeCategory::ChestOpenResult);

	Widget->HandleActionClicked(315);
	TestEqual(TEXT("reforge resolution routes to EnhanceReforge even when rejected"),
		Widget->GetLastNoticeCategoryForTest(),
		EGameXXKDesktopNoticeCategory::EnhanceReforge);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingItemCarryStateMachineTest,
	"GameXXK.DesktopTraining.Workbench.ItemCarryStateMachine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingItemCarryStateMachineTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	TestNotNull(TEXT("item-carry fixture subsystem exists"), Subsystem);
	if (!Subsystem || !Subsystem->StartGame())
	{
		return false;
	}
	UGameXXKDesktopTrainingWorkbenchWidget* Widget = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	Widget->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("item-carry fixture opens backpack"), Widget->OpenBackpack());

	const FName StoneId = UGameXXKMVPRules::ItemEnhancementStone();
	const int32 StoneSlot = Widget->FindBackpackItemSlotForTest(StoneId);
	TestTrue(TEXT("fixture finds a draggable backpack stack"), StoneSlot != INDEX_NONE);
	TestTrue(TEXT("left click picks the stack up without mutating inventory"), Widget->PickUpBackpackSlotForTest(StoneSlot));
	TestTrue(TEXT("picked item is attached to the cursor state"), Widget->IsCarryingItemForTest());
	TestEqual(TEXT("pickup is non-committing"), Subsystem->GetItemCount(StoneId), 10);
	TestTrue(TEXT("right click cancels the carried item"), Widget->CancelCarriedItemForTest());
	TestFalse(TEXT("cancel clears cursor attachment"), Widget->IsCarryingItemForTest());
	TestEqual(TEXT("cancel restores the exact authoritative source"), Subsystem->GetItemCount(StoneId), 10);

	TestTrue(TEXT("item can be picked up again"), Widget->PickUpBackpackSlotForTest(StoneSlot));
	Widget->HandleActionClicked(60);
	TestFalse(TEXT("Tab collapse automatically returns the carried item"), Widget->IsCarryingItemForTest());
	TestEqual(TEXT("Tab collapse never mutates the item stack"), Subsystem->GetItemCount(StoneId), 10);
	TestTrue(TEXT("reopening backpack after cancellation succeeds"), Widget->OpenBackpack());

	Widget->HandleActionClicked(3);
	TestTrue(TEXT("only tools are open for right-click routing"), Widget->IsToolsPanelActiveForTest() && !Widget->IsWarehousePanelOpenForTest());
	const int32 ToolEquipmentSlot = Widget->FindFirstBackpackEquipmentSlotForTest();
	const FGameXXKDesktopInventoryEntryKey ToolEquipmentEntry = FGameXXKDesktopInventoryRules::GetEntryAt(
		Subsystem->GetRuntimeState(), EGameXXKDesktopItemContainer::Backpack, ToolEquipmentSlot);
	TestTrue(TEXT("right click routes compatible equipment into first empty tool slot"), Widget->RightClickBackpackSlotForTest(ToolEquipmentSlot));
	TestEqual(TEXT("tool reservation does not consume the equipment instance"),
		FGameXXKEquipmentRules::FindInstance(Subsystem->GetRuntimeState().EquipmentCollection, ToolEquipmentEntry.EntryId) != nullptr, true);
	TestEqual(TEXT("first tool slot records the reserved equipment"), Widget->GetToolSlotItemIdForTest(0), ToolEquipmentEntry.EntryId);

	Widget->HandleActionClicked(0);
	TestTrue(TEXT("warehouse and tools can be visible together"), Widget->IsWarehousePanelOpenForTest() && Widget->IsToolsPanelActiveForTest());
	const int32 EquipmentSlot = Widget->FindFirstBackpackEquipmentSlotForTest();
	TestTrue(TEXT("fixture finds another backpack equipment entry"), EquipmentSlot != INDEX_NONE);
	TestTrue(TEXT("warehouse wins right-click priority while both side panels are open"), Widget->RightClickBackpackSlotForTest(EquipmentSlot));
	TestEqual(TEXT("warehouse priority does not add another tool reservation"), Widget->GetOccupiedToolSlotCountForTest(), 1);
	TestEqual(TEXT("warehouse receives the equipment entry"), Widget->GetWarehouseOccupancyForTest(), 1);

	TestTrue(TEXT("combine mode can be selected"), Widget->SetToolModeForTest(EGameXXKDesktopToolMode::Combine));
	const int32 StonesBeforeUnavailableConfirm = Subsystem->GetItemCount(StoneId);
	TestFalse(TEXT("incomplete nine-piece combine recipe rejects confirm"), Widget->ConfirmToolForTest());
	TestEqual(TEXT("incomplete combine never consumes material"), Subsystem->GetItemCount(StoneId), StonesBeforeUnavailableConfirm);
	TestEqual(TEXT("failed combine keeps tool input in place"), Widget->GetToolSlotItemIdForTest(0), ToolEquipmentEntry.EntryId);

	TestTrue(TEXT("picking the reserved tool item attaches it to cursor"), Widget->PickUpToolSlotForTest(0));
	TestTrue(TEXT("tool source is now carried"), Widget->IsCarryingItemForTest());
	TestTrue(TEXT("right-click cancellation returns it to its tool cell"), Widget->CancelCarriedItemForTest());
	TestEqual(TEXT("cancel restores original tool slot"), Widget->GetToolSlotItemIdForTest(0), ToolEquipmentEntry.EntryId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingItemCarryPlacementTest,
	"GameXXK.DesktopTraining.Workbench.ItemCarryPlacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingItemCarryPlacementTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	if (!TestNotNull(TEXT("placement fixture subsystem exists"), Subsystem)
		|| !Subsystem->StartGame())
	{
		return false;
	}
	FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
	State.Screen = EGameXXKScreen::Town;
	State.Inventory.Add(UGameXXKMVPRules::ItemHealingPowder(), 3);
	if (!TestTrue(TEXT("placement fixture normalizes"), Subsystem->NormalizeDesktopInventoryState()))
	{
		return false;
	}

	const FName StoneId = UGameXXKMVPRules::ItemEnhancementStone();
	const FName SandId = UGameXXKMVPRules::ItemHealingPowder();
	const FGameXXKDesktopInventoryEntryKey StoneEntry =
		FGameXXKDesktopInventoryRules::MakeItemEntry(StoneId);
	const FGameXXKDesktopInventoryEntryKey SandEntry =
		FGameXXKDesktopInventoryRules::MakeItemEntry(SandId);
	const int32 InitialStoneSlot = FGameXXKDesktopInventoryRules::FindEntrySlot(
		State, EGameXXKDesktopItemContainer::Backpack, StoneEntry);
	const int32 InitialSandSlot = FGameXXKDesktopInventoryRules::FindEntrySlot(
		State, EGameXXKDesktopItemContainer::Backpack, SandEntry);
	if (!TestTrue(TEXT("fixture finds two whole item stacks"),
		InitialStoneSlot != INDEX_NONE && InitialSandSlot != INDEX_NONE))
	{
		return false;
	}
	FString Error;
	TestTrue(TEXT("fixture places one item stack in Warehouse slot zero"),
		FGameXXKDesktopInventoryRules::MoveEntry(
			State,
			EGameXXKDesktopItemContainer::Backpack,
			InitialStoneSlot,
			EGameXXKDesktopItemContainer::Warehouse,
			0,
			&Error));

	const FGameXXKOrderedPartyFormation FormationBefore = State.CardRun.OrderedFormation;
	const FGameXXKTrainingProgress TrainingBefore = State.Training;
	UGameXXKDesktopTrainingWorkbenchWidget* Workbench =
		NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	Workbench->SetMVPSubsystem(Subsystem);
	Workbench->ConstructForTest();
	if (!TestTrue(TEXT("placement fixture opens Backpack"), Workbench->OpenBackpack()))
	{
		return false;
	}

	int32 SandSource = FGameXXKDesktopInventoryRules::FindEntrySlot(
		State, EGameXXKDesktopItemContainer::Backpack, SandEntry);
	int32 EmptyBackpackSlot = FGameXXKDesktopInventoryRules::FindFirstEmptySlot(
		State, EGameXXKDesktopItemContainer::Backpack);
	UGameXXKInventorySlotButton* SandSourceButton = FindEmbeddedBackpackButton(Workbench, SandSource);
	if (!TestNotNull(TEXT("real Backpack source button exists"), SandSourceButton)
		|| !ClickAndFlush(Workbench, SandSourceButton))
	{
		return false;
	}
	TestTrue(TEXT("real OnClicked starts the non-committing carry"),
		Workbench->HasDesktopCarriedEntry());
	TestEqual(TEXT("pickup leaves the whole stack authoritative"),
		Subsystem->GetItemCount(SandId), 3);
	UImage* CarriedVisual = Workbench->WidgetTree
		? Cast<UImage>(Workbench->WidgetTree->FindWidget(TEXT("DesktopCarriedItemImage")))
		: nullptr;
	if (TestNotNull(TEXT("carry creates a visible cursor preview"), CarriedVisual))
	{
		TestEqual(TEXT("cursor preview never owns the second click"),
			CarriedVisual->GetVisibility(), ESlateVisibility::HitTestInvisible);
		TestFalse(TEXT("cursor preview is disabled"), CarriedVisual->GetIsEnabled());
	}
	UGameXXKInventorySlotButton* EmptyBackpackButton =
		FindEmbeddedBackpackButton(Workbench, EmptyBackpackSlot);
	TestTrue(TEXT("real second Backpack click places into an empty cell"),
		ClickAndFlush(Workbench, EmptyBackpackButton));
	TestFalse(TEXT("successful empty placement clears carry"),
		Workbench->HasDesktopCarriedEntry());
	TestEqual(TEXT("empty Backpack target receives the exact stack key"),
		FGameXXKDesktopInventoryRules::GetEntryAt(
			State, EGameXXKDesktopItemContainer::Backpack, EmptyBackpackSlot),
		SandEntry);

	TestTrue(TEXT("real click picks the moved stack up again"),
		ClickAndFlush(
			Workbench,
			FindEmbeddedBackpackButton(Workbench, EmptyBackpackSlot)));
	TestTrue(TEXT("clicking the original source cell cancels"),
		ClickAndFlush(
			Workbench,
			FindEmbeddedBackpackButton(Workbench, EmptyBackpackSlot)));
	TestFalse(TEXT("source cancellation clears carry"),
		Workbench->HasDesktopCarriedEntry());
	TestEqual(TEXT("source cancellation never mutates the physical cell"),
		FGameXXKDesktopInventoryRules::GetEntryAt(
			State, EGameXXKDesktopItemContainer::Backpack, EmptyBackpackSlot),
		SandEntry);

	TestTrue(TEXT("real Warehouse navigation button opens the panel"),
		ClickAndFlush(
			Workbench,
			FindWorkbenchActionButton(Workbench, TEXT("BottomNavigationButton_0"))));
	const int32 EquipmentSource = Workbench->FindFirstBackpackEquipmentSlotForTest();
	const FGameXXKDesktopInventoryEntryKey EquipmentEntry =
		FGameXXKDesktopInventoryRules::GetEntryAt(
			State, EGameXXKDesktopItemContainer::Backpack, EquipmentSource);
	if (!TestTrue(TEXT("fixture finds an equipment-instance source"),
		EquipmentSource != INDEX_NONE && EquipmentEntry.bEquipmentInstance))
	{
		return false;
	}
	TestTrue(TEXT("real equipment-cell click starts a carry"),
		ClickAndFlush(
			Workbench,
			FindEmbeddedBackpackButton(Workbench, EquipmentSource)));
	TestTrue(TEXT("occupied Warehouse target atomically accepts a mixed swap"),
		ClickAndFlush(
			Workbench,
			FindWorkbenchActionButton(Workbench, TEXT("WarehouseSlot_0"))));
	TestFalse(TEXT("successful occupied Warehouse swap clears carry"),
		Workbench->HasDesktopCarriedEntry());
	TestEqual(TEXT("Warehouse receives the equipment instance"),
		FGameXXKDesktopInventoryRules::GetEntryAt(
			State, EGameXXKDesktopItemContainer::Warehouse, 0),
		EquipmentEntry);
	TestEqual(TEXT("displaced item stack returns to the exact Backpack source"),
		FGameXXKDesktopInventoryRules::GetEntryAt(
			State, EGameXXKDesktopItemContainer::Backpack, EquipmentSource),
		StoneEntry);

	TestTrue(TEXT("real Warehouse click picks the swapped equipment up"),
		ClickAndFlush(
			Workbench,
			FindWorkbenchActionButton(Workbench, TEXT("WarehouseSlot_0"))));
	TestTrue(TEXT("occupied Backpack target atomically accepts the reverse mixed swap"),
		ClickAndFlush(
			Workbench,
			FindEmbeddedBackpackButton(Workbench, EquipmentSource)));
	TestEqual(TEXT("reverse swap restores equipment to Backpack"),
		FGameXXKDesktopInventoryRules::GetEntryAt(
			State, EGameXXKDesktopItemContainer::Backpack, EquipmentSource),
		EquipmentEntry);
	TestEqual(TEXT("reverse swap restores item stack to Warehouse"),
		FGameXXKDesktopInventoryRules::GetEntryAt(
			State, EGameXXKDesktopItemContainer::Warehouse, 0),
		StoneEntry);

	TestTrue(TEXT("Backpack equipment can be picked for an empty Warehouse target"),
		ClickAndFlush(
			Workbench,
			FindEmbeddedBackpackButton(Workbench, EquipmentSource)));
	TestTrue(TEXT("empty Warehouse placement succeeds"),
		ClickAndFlush(
			Workbench,
			FindWorkbenchActionButton(Workbench, TEXT("WarehouseSlot_1"))));
	TestEqual(TEXT("empty Warehouse target owns the equipment instance"),
		FGameXXKDesktopInventoryRules::GetEntryAt(
			State, EGameXXKDesktopItemContainer::Warehouse, 1),
		EquipmentEntry);
	TestTrue(TEXT("Warehouse item stack can be picked for an empty Backpack target"),
		ClickAndFlush(
			Workbench,
			FindWorkbenchActionButton(Workbench, TEXT("WarehouseSlot_0"))));
	TestTrue(TEXT("empty Backpack placement from Warehouse succeeds"),
		ClickAndFlush(
			Workbench,
			FindEmbeddedBackpackButton(Workbench, EquipmentSource)));
	TestEqual(TEXT("empty Backpack target receives the Warehouse stack"),
		FGameXXKDesktopInventoryRules::GetEntryAt(
			State, EGameXXKDesktopItemContainer::Backpack, EquipmentSource),
		StoneEntry);

	TArray<int32> OccupiedBackpackSlots;
	for (int32 SlotIndex = 0;
		SlotIndex < FGameXXKDesktopInventoryRules::BackpackCapacity
			&& OccupiedBackpackSlots.Num() < 2;
		++SlotIndex)
	{
		if (FGameXXKDesktopInventoryRules::GetEntryAt(
			State, EGameXXKDesktopItemContainer::Backpack, SlotIndex).IsValid())
		{
			OccupiedBackpackSlots.Add(SlotIndex);
		}
	}
	if (!TestEqual(TEXT("ABA fixture finds two occupied cells"),
		OccupiedBackpackSlots.Num(), 2))
	{
		return false;
	}
	const int32 AbaSource = OccupiedBackpackSlots[0];
	const int32 AbaReplacementSource = OccupiedBackpackSlots[1];
	const FGameXXKDesktopInventoryEntryKey ExpectedCarriedEntry =
		FGameXXKDesktopInventoryRules::GetEntryAt(
			State, EGameXXKDesktopItemContainer::Backpack, AbaSource);
	TestTrue(TEXT("ABA fixture picks the expected source through real OnClicked"),
		ClickAndFlush(
			Workbench,
			FindEmbeddedBackpackButton(Workbench, AbaSource)));
	Swap(
		State.DesktopInventory.BackpackSlots[AbaSource],
		State.DesktopInventory.BackpackSlots[AbaReplacementSource]);
	const int32 AbaTarget = FGameXXKDesktopInventoryRules::FindFirstEmptySlot(
		State, EGameXXKDesktopItemContainer::Backpack);
	const FGameXXKDesktopInventoryEntryKey AbaTargetBefore =
		FGameXXKDesktopInventoryRules::GetEntryAt(
			State, EGameXXKDesktopItemContainer::Backpack, AbaTarget);
	TestTrue(TEXT("ABA target owns a real empty button"),
		ClickAndFlush(
			Workbench,
			FindEmbeddedBackpackButton(Workbench, AbaTarget)));
	TestTrue(TEXT("stale ExpectedEntry rejection retains carry"),
		Workbench->HasDesktopCarriedEntry());
	TestFalse(TEXT("stale ExpectedEntry rejection surfaces a reason"),
		Workbench->GetLastDesktopInventoryNoticeForTest().IsEmpty());
	UBorder* FirstFailureNoticePanel = Workbench->WidgetTree
		? Cast<UBorder>(Workbench->WidgetTree->FindWidget(TEXT("DesktopInventoryNoticePanel")))
		: nullptr;
	UTextBlock* FirstFailureNoticeText = Workbench->WidgetTree
		? Cast<UTextBlock>(Workbench->WidgetTree->FindWidget(TEXT("DesktopInventoryNoticeText")))
		: nullptr;
	if (TestNotNull(TEXT("the first failed drop creates a rendered notice panel"), FirstFailureNoticePanel)
		&& TestNotNull(TEXT("the first failed drop creates rendered notice text"), FirstFailureNoticeText))
	{
		TestTrue(TEXT("the first failed drop notice is visibly rendered"),
			FirstFailureNoticePanel->GetVisibility() != ESlateVisibility::Collapsed
				&& FirstFailureNoticePanel->GetVisibility() != ESlateVisibility::Hidden);
		TestTrue(TEXT("the first failed drop notice text is visibly rendered"),
			FirstFailureNoticeText->GetVisibility() != ESlateVisibility::Collapsed
				&& FirstFailureNoticeText->GetVisibility() != ESlateVisibility::Hidden);
		TestEqual(TEXT("the rendered notice exposes the actual rules failure"),
			FirstFailureNoticeText->GetText(),
			Workbench->GetLastDesktopInventoryNoticeForTest());
	}
	TestEqual(TEXT("stale ExpectedEntry rejection leaves target unchanged"),
		FGameXXKDesktopInventoryRules::GetEntryAt(
			State, EGameXXKDesktopItemContainer::Backpack, AbaTarget),
		AbaTargetBefore);
	TestNotEqual(TEXT("ABA source was externally replaced"),
		FGameXXKDesktopInventoryRules::GetEntryAt(
			State, EGameXXKDesktopItemContainer::Backpack, AbaSource),
		ExpectedCarriedEntry);
	TestTrue(TEXT("right click cancels the retained stale carry"),
		Workbench->CancelCarriedFromWorkbenchRightMouseForTest());

	TestTrue(TEXT("inventory carry never changes ordered Formation"),
		FGameXXKOrderedPartyFormation::StaticStruct()->CompareScriptStruct(
			&State.CardRun.OrderedFormation,
			&FormationBefore,
			PPF_None));
	TestTrue(TEXT("inventory carry never changes Training Travel"),
		FGameXXKTrainingProgress::StaticStruct()->CompareScriptStruct(
			&State.Training,
			&TrainingBefore,
			PPF_None));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingItemCarryCharacterSubpageBoundaryTest,
	"GameXXK.DesktopTraining.Workbench.ItemCarryCharacterSubpageBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingItemCarryCharacterSubpageBoundaryTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	if (!TestNotNull(TEXT("character-subpage boundary fixture subsystem exists"), Subsystem)
		|| !Subsystem->StartGame())
	{
		return false;
	}
	UGameXXKDesktopTrainingWorkbenchWidget* Workbench =
		NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	Workbench->SetMVPSubsystem(Subsystem);
	Workbench->ConstructForTest();
	if (!TestTrue(TEXT("character-subpage boundary fixture opens Backpack"),
		Workbench->OpenBackpack()))
	{
		return false;
	}

	const EGameXXKCharacterBackpackTab TargetTabs[] = {
		EGameXXKCharacterBackpackTab::Deck,
		EGameXXKCharacterBackpackTab::Attributes,
		EGameXXKCharacterBackpackTab::Talents,
		EGameXXKCharacterBackpackTab::Titles};
	const int32 TargetTabButtonIndices[] = {2, 0, 3, 4};
	for (int32 CaseIndex = 0; CaseIndex < UE_ARRAY_COUNT(TargetTabs); ++CaseIndex)
	{
		const int32 SourceSlot = Workbench->FindFirstBackpackEquipmentSlotForTest();
		const FGameXXKDesktopInventoryEntryKey SourceEntry =
			FGameXXKDesktopInventoryRules::GetEntryAt(
				Subsystem->GetRuntimeState(),
				EGameXXKDesktopItemContainer::Backpack,
				SourceSlot);
		if (!TestTrue(
			*FString::Printf(TEXT("subpage case %d finds an authoritative carry source"), CaseIndex),
			SourceSlot != INDEX_NONE && SourceEntry.IsValid())
			|| !TestTrue(
				*FString::Printf(TEXT("subpage case %d starts carry through a real slot callback"), CaseIndex),
				ClickAndFlush(
					Workbench,
					FindEmbeddedBackpackButton(Workbench, SourceSlot))))
		{
			return false;
		}
		TestTrue(
			*FString::Printf(TEXT("subpage case %d establishes carry before the tab click"), CaseIndex),
			Workbench->HasDesktopCarriedEntry());

		UGameXXKInventoryWindowWidget* Embedded = FindEmbeddedInventory(Workbench);
		UGameXXKCharacterBackpackTabButton* TargetTabButton =
			Embedded && Embedded->WidgetTree
				? Cast<UGameXXKCharacterBackpackTabButton>(Embedded->WidgetTree->FindWidget(
					*FString::Printf(
						TEXT("InventoryCharacterTab_%d"),
						TargetTabButtonIndices[CaseIndex])))
				: nullptr;
		if (!TestNotNull(
			*FString::Printf(TEXT("subpage case %d owns the real target tab button"), CaseIndex),
			TargetTabButton))
		{
			return false;
		}
		const int32 BuildCountBeforeTabCallback =
			Workbench->GetProgrammaticLayoutBuildCountForTest();
		TargetTabButton->OnClicked.Broadcast();
		TestFalse(
			*FString::Printf(TEXT("subpage case %d callback cancels carry immediately"), CaseIndex),
			Workbench->HasDesktopCarriedEntry());
		TestEqual(
			*FString::Printf(TEXT("subpage case %d callback never rebuilds the parent reentrantly"), CaseIndex),
			Workbench->GetProgrammaticLayoutBuildCountForTest(),
			BuildCountBeforeTabCallback);
		TestTrue(
			*FString::Printf(TEXT("subpage case %d schedules one safe parent refresh"), CaseIndex),
			Workbench->HasPendingLayoutRefreshForTest());
		TestEqual(
			*FString::Printf(TEXT("subpage case %d preserves the authoritative source"), CaseIndex),
			FGameXXKDesktopInventoryRules::GetEntryAt(
				Subsystem->GetRuntimeState(),
				EGameXXKDesktopItemContainer::Backpack,
				SourceSlot),
			SourceEntry);

		Workbench->TickForTest(0.0f);
		Embedded = FindEmbeddedInventory(Workbench);
		TestEqual(
			*FString::Printf(TEXT("subpage case %d keeps the clicked subpage after deferred rebuild"), CaseIndex),
			Embedded ? Embedded->GetActiveCharacterBackpackTabForTest()
				: EGameXXKCharacterBackpackTab::Equipment,
			TargetTabs[CaseIndex]);
		TestNull(
			*FString::Printf(TEXT("subpage case %d deferred rebuild removes the carried visual"), CaseIndex),
			Workbench->WidgetTree
				? Workbench->WidgetTree->FindWidget(TEXT("DesktopCarriedItemImage"))
				: nullptr);

		UGameXXKCharacterBackpackTabButton* EquipmentTabButton =
			Embedded && Embedded->WidgetTree
				? Cast<UGameXXKCharacterBackpackTabButton>(
					Embedded->WidgetTree->FindWidget(TEXT("InventoryCharacterTab_1")))
				: nullptr;
		if (!TestNotNull(
			*FString::Printf(TEXT("subpage case %d can return through the real Equipment tab"), CaseIndex),
			EquipmentTabButton)
			|| !ClickAndFlush(Workbench, EquipmentTabButton))
		{
			return false;
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingCharacterSubpageViewportReattachTest,
	"GameXXK.DesktopTraining.Workbench.CharacterSubpageViewportReattach",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingCharacterSubpageViewportReattachTest::RunTest(
	const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem =
		NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestTrue(TEXT("viewport-reattach fixture starts a new game"),
		Subsystem && Subsystem->StartGame()))
	{
		return false;
	}
	UGameXXKDesktopTrainingWorkbenchWidget* Workbench =
		NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	Workbench->SetMVPSubsystem(Subsystem);
	Workbench->ConstructForTest();
	if (!TestTrue(TEXT("viewport-reattach fixture opens Backpack"),
		Workbench->OpenBackpack()))
	{
		return false;
	}

	UGameXXKInventoryWindowWidget* Embedded = FindEmbeddedInventory(Workbench);
	UGameXXKCharacterBackpackTabButton* AttributesButton = Embedded && Embedded->WidgetTree
		? Cast<UGameXXKCharacterBackpackTabButton>(
			Embedded->WidgetTree->FindWidget(TEXT("InventoryCharacterTab_0")))
		: nullptr;
	if (!TestNotNull(TEXT("viewport-reattach fixture owns the real Attributes button"),
		AttributesButton))
	{
		return false;
	}
	AttributesButton->OnClicked.Broadcast();
	TestTrue(TEXT("Attributes callback schedules the parent layout refresh"),
		Workbench->HasPendingLayoutRefreshForTest());
	TestEqual(TEXT("the clicked embedded widget reaches Attributes before reattach"),
		Embedded->GetActiveCharacterBackpackTabForTest(),
		EGameXXKCharacterBackpackTab::Attributes);

	const int32 BuildCountBeforeReattach =
		Workbench->GetProgrammaticLayoutBuildCountForTest();
	Workbench->SimulateViewportReattachForTest();
	Embedded = FindEmbeddedInventory(Workbench);
	if (!TestNotNull(TEXT("viewport reattach rebuilds one embedded inventory"), Embedded))
	{
		return false;
	}
	TestEqual(TEXT("viewport reattach builds the layout exactly once"),
		Workbench->GetProgrammaticLayoutBuildCountForTest(),
		BuildCountBeforeReattach + 1);
	TestEqual(TEXT("viewport reattach preserves the clicked Attributes page"),
		Embedded->GetActiveCharacterBackpackTabForTest(),
		EGameXXKCharacterBackpackTab::Attributes);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingItemCarryEquipmentTest,
	"GameXXK.DesktopTraining.Workbench.ItemCarryEquipment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingItemCarryEquipmentTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	if (!TestNotNull(TEXT("equipment-carry fixture subsystem exists"), Subsystem)
		|| !Subsystem->StartGame())
	{
		return false;
	}
	FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
	State.Screen = EGameXXKScreen::Town;
	if (!TestTrue(TEXT("fixture owns a permanent companion"),
		State.CardRun.CompanionRoster.PermanentCompanions.Num() > 0))
	{
		return false;
	}
	const FName CharacterId =
		State.CardRun.CompanionRoster.PermanentCompanions[0].InstanceId;
	const FName ExistingWeapon = CreateCarryTestEquipment(
		*this, State, EGameXXKEquipmentSlot::Weapon,
		TEXT("fixture creates the initially equipped weapon"));
	const FName IncomingWeapon = CreateCarryTestEquipment(
		*this, State, EGameXXKEquipmentSlot::Weapon,
		TEXT("fixture creates the replacement weapon"));
	const FName WrongSlotWeapon = CreateCarryTestEquipment(
		*this, State, EGameXXKEquipmentSlot::Weapon,
		TEXT("fixture creates the wrong-slot weapon"));
	const FName IncomingArmor = CreateCarryTestEquipment(
		*this, State, EGameXXKEquipmentSlot::Armor,
		TEXT("fixture creates the empty-slot armor"));
	FGameXXKEquipmentTransactionResult EquipResult;
	if (!TestTrue(TEXT("fixture equips the companion's existing weapon"),
		Subsystem->EquipEquipmentInstance(
			CharacterId,
			EGameXXKEquipmentSlot::Weapon,
			ExistingWeapon,
			EquipResult))
		|| !TestTrue(TEXT("equipment-carry fixture normalizes"),
			Subsystem->NormalizeDesktopInventoryState()))
	{
		return false;
	}

	UGameXXKDesktopTrainingWorkbenchWidget* Workbench =
		NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	Workbench->SetMVPSubsystem(Subsystem);
	Workbench->ConstructForTest();
	TestTrue(TEXT("equipment-carry fixture opens Backpack"), Workbench->OpenBackpack());
	TestTrue(TEXT("fixture views the exact companion owner"),
		Workbench->SelectBackpackCharacterForTest(CharacterId));
	TestEqual(TEXT("embedded inventory follows the viewed owner"),
		Workbench->GetEmbeddedBackpackCharacterIdForTest(), CharacterId);

	const FGameXXKDesktopInventoryEntryKey IncomingEntry =
		FGameXXKDesktopInventoryRules::MakeEquipmentEntry(IncomingWeapon);
	const int32 IncomingSource = FGameXXKDesktopInventoryRules::FindEntrySlot(
		State, EGameXXKDesktopItemContainer::Backpack, IncomingEntry);
	if (!TestTrue(TEXT("replacement weapon has an authoritative Backpack cell"),
		IncomingSource != INDEX_NONE))
	{
		return false;
	}
	TestTrue(TEXT("real Backpack OnClicked carries replacement equipment"),
		ClickAndFlush(
			Workbench,
			FindEmbeddedBackpackButton(Workbench, IncomingSource)));
	TestTrue(TEXT("explicit Alt seam toggles the carried source lock"),
		Workbench->HandleDesktopSlotAltClicked(
			EGameXXKDesktopItemContainer::Backpack,
			IncomingSource));
	Workbench->TickForTest(0.0f);
	TestTrue(TEXT("Alt priority keeps carry unchanged"),
		Workbench->HasDesktopCarriedEntry());
	TestTrue(TEXT("Alt locks the stable incoming instance"),
		FGameXXKDesktopInventoryRules::IsEntryLocked(State, IncomingEntry));

	TestTrue(TEXT("real embedded equipment OnClicked delegates to Workbench"),
		ClickAndFlush(
			Workbench,
			FindEmbeddedEquipmentButton(Workbench, TEXT("Weapon"))));
	TestFalse(TEXT("successful equipment replacement clears carry"),
		Workbench->HasDesktopCarriedEntry());
	const FGameXXKEquipmentLoadout* CharacterLoadout =
		State.EquipmentCollection.CharacterLoadouts.Find(CharacterId);
	if (TestNotNull(TEXT("viewed companion owns a six-slot loadout"), CharacterLoadout))
	{
		TestEqual(TEXT("replacement equips the carried instance on the viewed owner"),
			FGameXXKEquipmentRules::GetLoadoutSlotInstanceId(
				*CharacterLoadout,
				EGameXXKEquipmentSlot::Weapon),
			IncomingWeapon);
	}
	const FGameXXKEquipmentLoadout* HeroLoadout =
		State.EquipmentCollection.CharacterLoadouts.Find(
			FGameXXKEquipmentRules::HeroCharacterId());
	TestTrue(TEXT("equipment delegation never redirects to Hero"),
		!HeroLoadout
			|| FGameXXKEquipmentRules::GetLoadoutSlotInstanceId(
				*HeroLoadout,
				EGameXXKEquipmentSlot::Weapon).IsNone());
	TestEqual(TEXT("displaced equipment returns to the exact carried source cell"),
		FGameXXKDesktopInventoryRules::GetEntryAt(
			State, EGameXXKDesktopItemContainer::Backpack, IncomingSource),
		FGameXXKDesktopInventoryRules::MakeEquipmentEntry(ExistingWeapon));
	TestTrue(TEXT("lock persists across equipment replacement"),
		FGameXXKDesktopInventoryRules::IsEntryLocked(State, IncomingEntry));
	UGameXXKInventoryWindowWidget* Embedded = FindEmbeddedInventory(Workbench);
	UImage* EquipmentLockedIcon = Embedded && Embedded->WidgetTree
		? Cast<UImage>(Embedded->WidgetTree->FindWidget(
			TEXT("InventoryEquipmentLockedIcon_Weapon")))
		: nullptr;
	TestLockedCellOverlay(
		*this,
		TEXT("embedded Equipment lock overlay"),
		EquipmentLockedIcon);

	const FGameXXKDesktopInventoryEntryKey WrongSlotEntry =
		FGameXXKDesktopInventoryRules::MakeEquipmentEntry(WrongSlotWeapon);
	const int32 WrongSlotSource = FGameXXKDesktopInventoryRules::FindEntrySlot(
		State, EGameXXKDesktopItemContainer::Backpack, WrongSlotEntry);
	TestTrue(TEXT("wrong-slot weapon starts a real carry"),
		ClickAndFlush(
			Workbench,
			FindEmbeddedBackpackButton(Workbench, WrongSlotSource)));
	TestTrue(TEXT("wrong equipment slot owns a real OnClicked target"),
		ClickAndFlush(
			Workbench,
			FindEmbeddedEquipmentButton(Workbench, TEXT("Head"))));
	TestTrue(TEXT("wrong-slot rejection retains carry"),
		Workbench->HasDesktopCarriedEntry());
	TestFalse(TEXT("wrong-slot rejection surfaces a reason"),
		Workbench->GetLastDesktopInventoryNoticeForTest().IsEmpty());
	Embedded = FindEmbeddedInventory(Workbench);
	TestTrue(TEXT("equipment-slot right click cancels carry before unequip behavior"),
		Embedded && Embedded->HandleConfiguredSlotRightClicked(
			EGameXXKInventorySlotSource::Equipment,
			1,
			TEXT("Head")));
	Workbench->TickForTest(0.0f);
	TestFalse(TEXT("equipment-slot right click clears rejected carry"),
		Workbench->HasDesktopCarriedEntry());

	const FGameXXKDesktopInventoryEntryKey ArmorEntry =
		FGameXXKDesktopInventoryRules::MakeEquipmentEntry(IncomingArmor);
	const int32 ArmorSource = FGameXXKDesktopInventoryRules::FindEntrySlot(
		State, EGameXXKDesktopItemContainer::Backpack, ArmorEntry);
	TestTrue(TEXT("armor starts a real carry"),
		ClickAndFlush(
			Workbench,
			FindEmbeddedBackpackButton(Workbench, ArmorSource)));
	TestTrue(TEXT("compatible empty equipment slot accepts the carried instance"),
		ClickAndFlush(
			Workbench,
			FindEmbeddedEquipmentButton(Workbench, TEXT("Armor"))));
	CharacterLoadout = State.EquipmentCollection.CharacterLoadouts.Find(CharacterId);
	if (TestNotNull(TEXT("empty-slot equip keeps the companion loadout"), CharacterLoadout))
	{
		TestEqual(TEXT("empty Armor slot now owns the incoming armor"),
			FGameXXKEquipmentRules::GetLoadoutSlotInstanceId(
				*CharacterLoadout,
				EGameXXKEquipmentSlot::Armor),
			IncomingArmor);
	}
	TestFalse(TEXT("empty-slot equip clears the exact source cell"),
		FGameXXKDesktopInventoryRules::GetEntryAt(
			State, EGameXXKDesktopItemContainer::Backpack, ArmorSource).IsValid());

	Embedded = FindEmbeddedInventory(Workbench);
	UGameXXKInventorySlotButton* WeaponButton =
		FindEmbeddedEquipmentButton(Workbench, TEXT("Weapon"));
	TestTrue(TEXT("equipment click without carry keeps select/detail behavior"),
		ClickAndFlush(Workbench, WeaponButton));
	Embedded = FindEmbeddedInventory(Workbench);
	TestFalse(TEXT("selected equipped instance exposes detail text"),
		Embedded ? Embedded->GetSelectedDetailTextForTest().IsEmpty() : true);
	TestTrue(TEXT("explicit Equipment Alt seam toggles the equipped lock"),
		Embedded && Embedded->HandleConfiguredSlotAltClicked(
			EGameXXKInventorySlotSource::Equipment,
			0,
			TEXT("Weapon")));
	Workbench->TickForTest(0.0f);
	TestFalse(TEXT("Equipment Alt toggles the exact equipped instance"),
		FGameXXKDesktopInventoryRules::IsEntryLocked(State, IncomingEntry));
	TestFalse(TEXT("Equipment Alt never starts a carry"),
		Workbench->HasDesktopCarriedEntry());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingItemCarryToolAndLockTest,
	"GameXXK.DesktopTraining.Workbench.ItemCarryToolAndLocks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingItemCarryToolAndLockTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	if (!TestNotNull(TEXT("tool-carry fixture subsystem exists"), Subsystem)
		|| !Subsystem->StartGame())
	{
		return false;
	}
	FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
	UnlockToolsTalent(State);
	State.Screen = EGameXXKScreen::Town;
	if (!TestTrue(TEXT("tool-carry fixture normalizes"),
		Subsystem->NormalizeDesktopInventoryState()))
	{
		return false;
	}
	const FName StoneId = UGameXXKMVPRules::ItemEnhancementStone();
	const FGameXXKDesktopInventoryEntryKey StoneEntry =
		FGameXXKDesktopInventoryRules::MakeItemEntry(StoneId);
	const int32 StoneSource = FGameXXKDesktopInventoryRules::FindEntrySlot(
		State, EGameXXKDesktopItemContainer::Backpack, StoneEntry);
	FString Error;
	if (!TestTrue(TEXT("fixture moves the item stack to Warehouse zero"),
		FGameXXKDesktopInventoryRules::MoveEntry(
			State,
			EGameXXKDesktopItemContainer::Backpack,
			StoneSource,
			EGameXXKDesktopItemContainer::Warehouse,
			0,
			&Error)))
	{
		return false;
	}

	TArray<int32> SourceSlots;
	TArray<FGameXXKDesktopInventoryEntryKey> SourceEntries;
	for (int32 SlotIndex = 0;
		SlotIndex < FGameXXKDesktopInventoryRules::BackpackCapacity
			&& SourceSlots.Num() < 4;
		++SlotIndex)
	{
		const FGameXXKDesktopInventoryEntryKey Entry =
			FGameXXKDesktopInventoryRules::GetEntryAt(
				State, EGameXXKDesktopItemContainer::Backpack, SlotIndex);
		if (Entry.IsValid())
		{
			SourceSlots.Add(SlotIndex);
			SourceEntries.Add(Entry);
		}
	}
	if (!TestEqual(TEXT("tool fixture finds four independent storage entries"),
		SourceSlots.Num(), 4))
	{
		return false;
	}

	UGameXXKDesktopTrainingWorkbenchWidget* Workbench =
		NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	Workbench->SetMVPSubsystem(Subsystem);
	Workbench->ConstructForTest();
	TestTrue(TEXT("tool fixture opens Backpack"), Workbench->OpenBackpack());
	TestTrue(TEXT("real Tools navigation OnClicked opens the panel"),
		ClickAndFlush(
			Workbench,
			FindWorkbenchActionButton(Workbench, TEXT("BottomNavigationButton_3"))));

	TestTrue(TEXT("first storage entry starts a real carry"),
		ClickAndFlush(
			Workbench,
			FindEmbeddedBackpackButton(Workbench, SourceSlots[0])));
	TestTrue(TEXT("empty Tool target reserves without committing"),
		ClickAndFlush(
			Workbench,
			FindWorkbenchActionButton(Workbench, TEXT("ToolInputSlot_0"))));
	TestEqual(TEXT("Tool zero records the first reservation"),
		Workbench->GetToolSlotItemIdForTest(0), SourceEntries[0].EntryId);
	TestEqual(TEXT("first reservation leaves its authoritative cell unchanged"),
		FGameXXKDesktopInventoryRules::GetEntryAt(
			State, EGameXXKDesktopItemContainer::Backpack, SourceSlots[0]),
		SourceEntries[0]);

	TestTrue(TEXT("second storage entry starts a real carry"),
		ClickAndFlush(
			Workbench,
			FindEmbeddedBackpackButton(Workbench, SourceSlots[1])));
	TestTrue(TEXT("second empty Tool target reserves without committing"),
		ClickAndFlush(
			Workbench,
			FindWorkbenchActionButton(Workbench, TEXT("ToolInputSlot_1"))));
	TestEqual(TEXT("second reservation leaves its authoritative cell unchanged"),
		FGameXXKDesktopInventoryRules::GetEntryAt(
			State, EGameXXKDesktopItemContainer::Backpack, SourceSlots[1]),
		SourceEntries[1]);

	TestTrue(TEXT("real occupied Tool source starts a carry"),
		ClickAndFlush(
			Workbench,
			FindWorkbenchActionButton(Workbench, TEXT("ToolInputSlot_0"))));
	TestTrue(TEXT("occupied Tool target swaps reservations"),
		ClickAndFlush(
			Workbench,
			FindWorkbenchActionButton(Workbench, TEXT("ToolInputSlot_1"))));
	TestFalse(TEXT("Tool-to-Tool swap ends carry"),
		Workbench->HasDesktopCarriedEntry());
	TestEqual(TEXT("displaced reservation returns to original Tool slot"),
		Workbench->GetToolSlotItemIdForTest(0), SourceEntries[1].EntryId);
	TestEqual(TEXT("carried reservation occupies Tool target"),
		Workbench->GetToolSlotItemIdForTest(1), SourceEntries[0].EntryId);

	TestTrue(TEXT("Tool original-source cancel starts with a pickup"),
		ClickAndFlush(
			Workbench,
			FindWorkbenchActionButton(Workbench, TEXT("ToolInputSlot_0"))));
	TestTrue(TEXT("clicking the original Tool source cancels"),
		ClickAndFlush(
			Workbench,
			FindWorkbenchActionButton(Workbench, TEXT("ToolInputSlot_0"))));
	TestFalse(TEXT("Tool original-source cancel clears carry"),
		Workbench->HasDesktopCarriedEntry());
	TestEqual(TEXT("Tool original-source cancel restores its reservation"),
		Workbench->GetToolSlotItemIdForTest(0), SourceEntries[1].EntryId);

	TestTrue(TEXT("third storage entry starts a real carry"),
		ClickAndFlush(
			Workbench,
			FindEmbeddedBackpackButton(Workbench, SourceSlots[2])));
	TestTrue(TEXT("storage-to-occupied-Tool replaces the reservation"),
		ClickAndFlush(
			Workbench,
			FindWorkbenchActionButton(Workbench, TEXT("ToolInputSlot_1"))));
	TestFalse(TEXT("storage-to-occupied-Tool replacement ends carry"),
		Workbench->HasDesktopCarriedEntry());
	TestEqual(TEXT("Tool target now reserves the incoming storage entry"),
		Workbench->GetToolSlotItemIdForTest(1), SourceEntries[2].EntryId);
	TestEqual(TEXT("replaced Tool reservation is released to unchanged authority"),
		FGameXXKDesktopInventoryRules::GetEntryAt(
			State, EGameXXKDesktopItemContainer::Backpack, SourceSlots[0]),
		SourceEntries[0]);
	TestFalse(TEXT("released reservation is no longer hidden by Tool state"),
		Workbench->ShouldHideDesktopInventoryEntry(
			EGameXXKDesktopItemContainer::Backpack,
			SourceEntries[0]));

	TestTrue(TEXT("Tool-to-storage fixture picks Tool zero"),
		ClickAndFlush(
			Workbench,
			FindWorkbenchActionButton(Workbench, TEXT("ToolInputSlot_0"))));
	TestTrue(TEXT("Tool Alt seam toggles reserved entry while another carry exists"),
		Workbench->HandleDesktopToolSlotAltClicked(1));
	Workbench->TickForTest(0.0f);
	TestTrue(TEXT("Tool Alt priority leaves the other carry unchanged"),
		Workbench->HasDesktopCarriedEntry());
	TestTrue(TEXT("Tool Alt locks the reserved stable entry"),
		FGameXXKDesktopInventoryRules::IsEntryLocked(State, SourceEntries[2]));
	UImage* ToolLockedIcon = Workbench->WidgetTree
		? Cast<UImage>(Workbench->WidgetTree->FindWidget(TEXT("ToolLockedIcon_1")))
		: nullptr;
	TestLockedCellOverlay(*this, TEXT("Tool lock overlay"), ToolLockedIcon);
	TestTrue(TEXT("Tool-to-occupied-storage uses authoritative MoveOrSwap"),
		ClickAndFlush(
			Workbench,
			FindEmbeddedBackpackButton(Workbench, SourceSlots[3])));
	TestFalse(TEXT("successful Tool-to-storage swap clears carry"),
		Workbench->HasDesktopCarriedEntry());
	TestFalse(TEXT("successful Tool-to-storage swap empties original Tool slot"),
		!Workbench->GetToolSlotItemIdForTest(0).IsNone());
	TestEqual(TEXT("Tool entry arrives at the occupied storage target"),
		FGameXXKDesktopInventoryRules::GetEntryAt(
			State, EGameXXKDesktopItemContainer::Backpack, SourceSlots[3]),
		SourceEntries[1]);
	TestEqual(TEXT("displaced storage entry returns to Tool entry authority"),
		FGameXXKDesktopInventoryRules::GetEntryAt(
			State, EGameXXKDesktopItemContainer::Backpack, SourceSlots[1]),
		SourceEntries[3]);

	TestTrue(TEXT("real Warehouse navigation opens beside Tools"),
		ClickAndFlush(
			Workbench,
			FindWorkbenchActionButton(Workbench, TEXT("BottomNavigationButton_0"))));
	const int32 StoneQuantityBeforeLock = State.DesktopInventory.WarehouseItems.FindRef(StoneId);
	TestTrue(TEXT("Warehouse Alt seam locks the whole item stack"),
		Workbench->HandleDesktopSlotAltClicked(
			EGameXXKDesktopItemContainer::Warehouse,
			0));
	Workbench->TickForTest(0.0f);
	TestTrue(TEXT("whole-stack item lock uses the item ID"),
		State.DesktopInventory.LockedItemIds.Contains(StoneId));
	TestEqual(TEXT("Alt lock never changes stack quantity"),
		State.DesktopInventory.WarehouseItems.FindRef(StoneId),
		StoneQuantityBeforeLock);
	UImage* WarehouseLockedIcon = Workbench->WidgetTree
		? Cast<UImage>(Workbench->WidgetTree->FindWidget(TEXT("WarehouseLockedIcon_0")))
		: nullptr;
	TestLockedCellOverlay(*this, TEXT("Warehouse lock overlay"), WarehouseLockedIcon);
	TestFalse(TEXT("empty Warehouse lock request is rejected"),
		Workbench->HandleDesktopSlotAltClicked(
			EGameXXKDesktopItemContainer::Warehouse,
			1));
	TestFalse(TEXT("empty lock rejection never starts carry"),
		Workbench->HasDesktopCarriedEntry());

	const int32 EmptyBackpackSlot = FGameXXKDesktopInventoryRules::FindFirstEmptySlot(
		State, EGameXXKDesktopItemContainer::Backpack);
	TestTrue(TEXT("locked Warehouse item starts a real carry"),
		ClickAndFlush(
			Workbench,
			FindWorkbenchActionButton(Workbench, TEXT("WarehouseSlot_0"))));
	TestTrue(TEXT("locked item remains manually movable"),
		ClickAndFlush(
			Workbench,
			FindEmbeddedBackpackButton(Workbench, EmptyBackpackSlot)));
	TestFalse(TEXT("locked item move empties its former Warehouse cell"),
		FGameXXKDesktopInventoryRules::GetEntryAt(
			State, EGameXXKDesktopItemContainer::Warehouse, 0).IsValid());
	TestEqual(TEXT("locked item move reaches the exact requested Backpack cell"),
		FGameXXKDesktopInventoryRules::GetEntryAt(
			State, EGameXXKDesktopItemContainer::Backpack, EmptyBackpackSlot),
		StoneEntry);
	TestEqual(TEXT("locked item physical location actually changes containers"),
		FGameXXKDesktopInventoryRules::FindEntrySlot(
			State,
			EGameXXKDesktopItemContainer::Backpack,
			StoneEntry),
		EmptyBackpackSlot);
	TestTrue(TEXT("whole-stack lock persists after manual movement"),
		FGameXXKDesktopInventoryRules::IsEntryLocked(State, StoneEntry));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingToolReservationAuthorityTest,
	"GameXXK.DesktopTraining.Workbench.ItemCarryToolReservationAuthority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingToolReservationAuthorityTest::RunTest(const FString& Parameters)
{
	{
		UGameInstance* TestGameInstance = NewObject<UGameInstance>();
		UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
		if (!TestNotNull(TEXT("hidden-backing fixture subsystem exists"), Subsystem)
			|| !Subsystem->StartGame())
		{
			return false;
		}
		FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
		UnlockToolsTalent(State);
		State.Screen = EGameXXKScreen::Town;
		if (!TestTrue(TEXT("hidden-backing fixture normalizes"),
			Subsystem->NormalizeDesktopInventoryState()))
		{
			return false;
		}

		TArray<int32> SourceSlots;
		TArray<FGameXXKDesktopInventoryEntryKey> SourceEntries;
		for (int32 SlotIndex = 0;
			SlotIndex < FGameXXKDesktopInventoryRules::BackpackCapacity
				&& SourceSlots.Num() < 2;
			++SlotIndex)
		{
			const FGameXXKDesktopInventoryEntryKey Entry =
				FGameXXKDesktopInventoryRules::GetEntryAt(
					State,
					EGameXXKDesktopItemContainer::Backpack,
					SlotIndex);
			if (Entry.IsValid())
			{
				SourceSlots.Add(SlotIndex);
				SourceEntries.Add(Entry);
			}
		}
		if (!TestEqual(TEXT("hidden-backing fixture finds two entries"), SourceSlots.Num(), 2))
		{
			return false;
		}

		UGameXXKDesktopTrainingWorkbenchWidget* Workbench =
			NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
		Workbench->SetMVPSubsystem(Subsystem);
		Workbench->ConstructForTest();
		TestTrue(TEXT("hidden-backing fixture opens Backpack"), Workbench->OpenBackpack());
		TestTrue(TEXT("hidden-backing fixture opens Tools through the real nav"),
			ClickAndFlush(
				Workbench,
				FindWorkbenchActionButton(Workbench, TEXT("BottomNavigationButton_3"))));
		for (int32 ToolSlotIndex = 0; ToolSlotIndex < 2; ++ToolSlotIndex)
		{
			TestTrue(
				*FString::Printf(TEXT("reservation %d starts from its real storage button"), ToolSlotIndex),
				ClickAndFlush(
					Workbench,
					FindEmbeddedBackpackButton(Workbench, SourceSlots[ToolSlotIndex])));
			TestTrue(
				*FString::Printf(TEXT("reservation %d reaches its real Tool button"), ToolSlotIndex),
				ClickAndFlush(
					Workbench,
					FindWorkbenchActionButton(
						Workbench,
						*FString::Printf(TEXT("ToolInputSlot_%d"), ToolSlotIndex))));
		}
		TestTrue(TEXT("Tool zero pickup establishes the carried reservation"),
			ClickAndFlush(
				Workbench,
				FindWorkbenchActionButton(Workbench, TEXT("ToolInputSlot_0"))));
		if (!TestTrue(TEXT("hidden-backing drop begins with carry active"),
			Workbench->HasDesktopCarriedEntry()))
		{
			return false;
		}
		const FGameXXKRuntimeState BeforeHiddenBackingDrop = State;
		TestTrue(TEXT("the hidden backing cell still owns a real click target"),
			ClickAndFlush(
				Workbench,
				FindEmbeddedBackpackButton(Workbench, SourceSlots[1])));
		TestTrue(TEXT("another Tool reservation backing cell rejects the drop and retains carry"),
			Workbench->HasDesktopCarriedEntry());
		TestTrue(TEXT("hidden backing rejection preserves every authoritative runtime field"),
			FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(
				&State,
				&BeforeHiddenBackingDrop,
				PPF_None));
		TestTrue(TEXT("hidden backing rejection leaves the carried origin Tool cell empty"),
			Workbench->GetToolSlotItemIdForTest(0).IsNone());
		TestEqual(TEXT("hidden backing rejection preserves the other Tool reservation"),
			Workbench->GetToolSlotItemIdForTest(1),
			SourceEntries[1].EntryId);
		TestEqual(TEXT("hidden backing rejection preserves the carried entry's source"),
			FGameXXKDesktopInventoryRules::GetEntryAt(
				State,
				EGameXXKDesktopItemContainer::Backpack,
				SourceSlots[0]),
			SourceEntries[0]);
		TestEqual(TEXT("hidden backing rejection preserves the target reservation source"),
			FGameXXKDesktopInventoryRules::GetEntryAt(
				State,
				EGameXXKDesktopItemContainer::Backpack,
				SourceSlots[1]),
			SourceEntries[1]);
		TestFalse(TEXT("hidden backing rejection displays a reason"),
			Workbench->GetLastDesktopInventoryNoticeForTest().IsEmpty());
	}

	{
		UGameInstance* TestGameInstance = NewObject<UGameInstance>();
		UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
		if (!TestNotNull(TEXT("stale-confirm fixture subsystem exists"), Subsystem)
			|| !Subsystem->StartGame())
		{
			return false;
		}
		FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
		UnlockToolsTalent(State);
		State.Screen = EGameXXKScreen::Town;
		if (!TestTrue(TEXT("stale-confirm fixture normalizes"),
			Subsystem->NormalizeDesktopInventoryState()))
		{
			return false;
		}

		TArray<int32> EquipmentSlots;
		TArray<FGameXXKDesktopInventoryEntryKey> EquipmentEntries;
		for (int32 SlotIndex = 0;
			SlotIndex < FGameXXKDesktopInventoryRules::BackpackCapacity
				&& EquipmentSlots.Num() < 2;
			++SlotIndex)
		{
			const FGameXXKDesktopInventoryEntryKey Entry =
				FGameXXKDesktopInventoryRules::GetEntryAt(
					State,
					EGameXXKDesktopItemContainer::Backpack,
					SlotIndex);
			if (Entry.bEquipmentInstance)
			{
				EquipmentSlots.Add(SlotIndex);
				EquipmentEntries.Add(Entry);
			}
		}
		if (!TestEqual(TEXT("stale-confirm fixture finds two equipment cells"),
			EquipmentSlots.Num(), 2))
		{
			return false;
		}

		UGameXXKDesktopTrainingWorkbenchWidget* Workbench =
			NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
		Workbench->SetMVPSubsystem(Subsystem);
		Workbench->ConstructForTest();
		TestTrue(TEXT("stale-confirm fixture opens Backpack"), Workbench->OpenBackpack());
		TestTrue(TEXT("stale-confirm fixture opens Tools through the real nav"),
			ClickAndFlush(
				Workbench,
				FindWorkbenchActionButton(Workbench, TEXT("BottomNavigationButton_3"))));
		TestTrue(TEXT("stale-confirm fixture picks equipment through its real cell"),
			ClickAndFlush(
				Workbench,
				FindEmbeddedBackpackButton(Workbench, EquipmentSlots[0])));
		TestTrue(TEXT("stale-confirm fixture reserves equipment through Tool zero"),
			ClickAndFlush(
				Workbench,
				FindWorkbenchActionButton(Workbench, TEXT("ToolInputSlot_0"))));
		Swap(
			State.DesktopInventory.BackpackSlots[EquipmentSlots[0]],
			State.DesktopInventory.BackpackSlots[EquipmentSlots[1]]);
		const FGameXXKRuntimeState BeforeStaleConfirm = State;
		UGameXXKDesktopTrainingActionButton* ConfirmButton =
			FindWorkbenchActionButton(Workbench, TEXT("ToolConfirmButton"));
		if (!TestNotNull(TEXT("stale-confirm fixture owns the real confirm button"), ConfirmButton))
		{
			return false;
		}
		TestTrue(TEXT("stale-confirm fixture invokes the real confirm callback"),
			ClickAndFlush(Workbench, ConfirmButton));
		TestTrue(TEXT("stale reservation confirmation preserves every runtime field"),
			FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(
				&State,
				&BeforeStaleConfirm,
				PPF_None));
		TestEqual(TEXT("stale reservation remains in Tool zero after rejection"),
			Workbench->GetToolSlotItemIdForTest(0),
			EquipmentEntries[0].EntryId);
		TestNotNull(TEXT("stale confirmation never dismantles the reserved instance"),
			FGameXXKEquipmentRules::FindInstance(
				State.EquipmentCollection,
				EquipmentEntries[0].EntryId));
		TestFalse(TEXT("stale reservation confirmation displays a reason"),
			Workbench->GetLastDesktopInventoryNoticeForTest().IsEmpty());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingEmbeddedBackpackDeferredRefreshTest,
	"GameXXK.DesktopTraining.Workbench.EmbeddedBackpackDeferredRefresh",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingEmbeddedBackpackDeferredRefreshTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	if (!TestNotNull(TEXT("deferred-refresh fixture subsystem exists"), Subsystem)
		|| !Subsystem->StartGame())
	{
		return false;
	}

	UGameXXKDesktopTrainingWorkbenchWidget* Widget = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	if (!TestNotNull(TEXT("deferred-refresh fixture widget exists"), Widget))
	{
		return false;
	}
	Widget->SetMVPSubsystem(Subsystem);
	Widget->ConstructForTest();
	TestTrue(TEXT("deferred-refresh fixture opens backpack"), Widget->OpenBackpack());

	UGameXXKInventoryWindowWidget* EmbeddedBackpack = Widget->WidgetTree
		? Cast<UGameXXKInventoryWindowWidget>(Widget->WidgetTree->FindWidget(TEXT("EmbeddedApprovedBackpack")))
		: nullptr;
	if (!TestNotNull(TEXT("expanded workbench owns the embedded approved backpack"), EmbeddedBackpack))
	{
		return false;
	}

	const int32 OccupiedEquipmentSlot = Widget->FindFirstBackpackEquipmentSlotForTest();
	if (!TestTrue(TEXT("new game exposes an occupied embedded equipment slot"), OccupiedEquipmentSlot != INDEX_NONE))
	{
		return false;
	}
	const int32 BuildCountBeforeClick = Widget->GetProgrammaticLayoutBuildCountForTest();

	EmbeddedBackpack->HandleConfiguredSlotClicked(
		EGameXXKInventorySlotSource::PlayerBackpack,
		OccupiedEquipmentSlot,
		NAME_None);

	TestTrue(TEXT("embedded click keeps the parent backpack expanded"), Widget->IsBackpackExpandedForTest());
	TestTrue(TEXT("embedded occupied-slot click carries the item"), Widget->IsCarryingItemForTest());
	TestEqual(TEXT("embedded callback does not synchronously rebuild the parent tree"),
		Widget->GetProgrammaticLayoutBuildCountForTest(),
		BuildCountBeforeClick);
	TestTrue(TEXT("embedded callback leaves one parent refresh pending"), Widget->HasPendingLayoutRefreshForTest());

	Widget->TickForTest(0.0f);
	TestEqual(TEXT("the next tick performs exactly one parent rebuild"),
		Widget->GetProgrammaticLayoutBuildCountForTest(),
		BuildCountBeforeClick + 1);
	TestTrue(TEXT("deferred rebuild keeps the parent backpack expanded"), Widget->IsBackpackExpandedForTest());
	TestNotNull(TEXT("deferred rebuild restores the embedded approved backpack"),
		Widget->WidgetTree ? Widget->WidgetTree->FindWidget(TEXT("EmbeddedApprovedBackpack")) : nullptr);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingCarriedRightClickCancelTest,
	"GameXXK.DesktopTraining.Workbench.CarriedRightClickCancelKeepsBackpack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingCarriedRightClickCancelTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	if (!TestNotNull(TEXT("right-cancel fixture subsystem exists"), Subsystem)
		|| !Subsystem->StartGame())
	{
		return false;
	}

	UGameXXKDesktopTrainingWorkbenchWidget* Widget = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	if (!TestNotNull(TEXT("right-cancel fixture widget exists"), Widget))
	{
		return false;
	}
	Widget->SetMVPSubsystem(Subsystem);
	Widget->ConstructForTest();
	TestTrue(TEXT("right-cancel fixture opens Backpack"), Widget->OpenBackpack());

	UGameXXKInventoryWindowWidget* Embedded = Widget->WidgetTree
		? Cast<UGameXXKInventoryWindowWidget>(Widget->WidgetTree->FindWidget(TEXT("EmbeddedApprovedBackpack")))
		: nullptr;
	if (!TestNotNull(TEXT("right-cancel fixture owns the embedded Backpack"), Embedded))
	{
		return false;
	}
	const int32 OriginalSlot = Widget->FindFirstBackpackEquipmentSlotForTest();
	if (!TestTrue(TEXT("right-cancel fixture finds the first occupied equipment slot"), OriginalSlot != INDEX_NONE))
	{
		return false;
	}

	const int32 BuildCountBeforePickup = Widget->GetProgrammaticLayoutBuildCountForTest();
	Embedded->HandleConfiguredSlotClicked(
		EGameXXKInventorySlotSource::PlayerBackpack,
		OriginalSlot,
		NAME_None);
	TestTrue(TEXT("configured left callback enters carry state"), Widget->IsCarryingItemForTest());
	TestEqual(TEXT("configured left callback performs no synchronous rebuild"),
		Widget->GetProgrammaticLayoutBuildCountForTest(), BuildCountBeforePickup);
	TestTrue(TEXT("configured left callback leaves one refresh pending"), Widget->HasPendingLayoutRefreshForTest());
	Widget->TickForTest(0.0f);
	TestEqual(TEXT("pickup safe boundary performs exactly one rebuild"),
		Widget->GetProgrammaticLayoutBuildCountForTest(), BuildCountBeforePickup + 1);
	TestNotNull(TEXT("pickup rebuild owns a carried visual"),
		Widget->WidgetTree ? Widget->WidgetTree->FindWidget(TEXT("DesktopCarriedItemImage")) : nullptr);

	Embedded = Widget->WidgetTree
		? Cast<UGameXXKInventoryWindowWidget>(Widget->WidgetTree->FindWidget(TEXT("EmbeddedApprovedBackpack")))
		: nullptr;
	if (!TestNotNull(TEXT("pickup rebuild exposes a fresh embedded Backpack"), Embedded))
	{
		return false;
	}
	const int32 BuildCountBeforeCancel = Widget->GetProgrammaticLayoutBuildCountForTest();
	TestTrue(TEXT("workbench right-mouse fallback cancels the carried item"),
		Widget->CancelCarriedFromWorkbenchRightMouseForTest());
	TestFalse(TEXT("right-mouse cancel clears CarriedEntry immediately"), Widget->IsCarryingItemForTest());
	TestEqual(TEXT("right-mouse callback performs no synchronous parent rebuild"),
		Widget->GetProgrammaticLayoutBuildCountForTest(), BuildCountBeforeCancel);
	TestTrue(TEXT("right-mouse callback leaves one refresh pending"), Widget->HasPendingLayoutRefreshForTest());
	TestEqual(TEXT("right-mouse cancel preserves the authoritative origin slot"),
		Widget->FindFirstBackpackEquipmentSlotForTest(), OriginalSlot);

	Widget->TickForTest(0.0f);
	TestEqual(TEXT("right-mouse safe boundary performs exactly one rebuild"),
		Widget->GetProgrammaticLayoutBuildCountForTest(), BuildCountBeforeCancel + 1);
	TestTrue(TEXT("right-mouse cancel keeps the parent expanded"), Widget->IsBackpackExpandedForTest());
	TestEqual(TEXT("right-mouse cancel keeps Backpack in the center"),
		Widget->GetActiveCenterPageForTest(), EGameXXKDesktopTrainingCenterPage::Backpack);
	UWidget* RebuiltPaper = Widget->WidgetTree
		? Widget->WidgetTree->FindWidget(TEXT("EmbeddedBackpackPaperReference"))
		: nullptr;
	TestNotNull(TEXT("right-mouse rebuild restores the Backpack paper root"), RebuiltPaper);
	if (RebuiltPaper)
	{
		TestTrue(TEXT("rebuilt Backpack paper remains visible"),
			RebuiltPaper->GetVisibility() != ESlateVisibility::Collapsed
			&& RebuiltPaper->GetVisibility() != ESlateVisibility::Hidden);
	}
	TestNotNull(TEXT("right-mouse rebuild restores a fresh embedded Backpack"),
		Widget->WidgetTree ? Widget->WidgetTree->FindWidget(TEXT("EmbeddedApprovedBackpack")) : nullptr);
	TestNull(TEXT("right-mouse rebuild removes the floating carried visual"),
		Widget->WidgetTree ? Widget->WidgetTree->FindWidget(TEXT("DesktopCarriedItemImage")) : nullptr);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingWorkbenchCloseStackTest,
	"GameXXK.DesktopTraining.Workbench.ParentCloseStack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingWorkbenchCloseStackTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	if (!TestNotNull(TEXT("close-stack fixture subsystem exists"), Subsystem)
		|| !Subsystem->StartGame())
	{
		return false;
	}
	FGameXXKRuntimeState& CloseStackState = Subsystem->GetMutableRuntimeState();
	const FName ToolInputEquipment = CreateCarryTestEquipment(
		*this,
		CloseStackState,
		EGameXXKEquipmentSlot::Weapon,
		TEXT("close-stack fixture creates a deterministic tool input"));
	const FName CarryEquipment = CreateCarryTestEquipment(
		*this,
		CloseStackState,
		EGameXXKEquipmentSlot::Armor,
		TEXT("close-stack fixture creates a deterministic carried item"));
	if (!TestTrue(TEXT("close-stack fixture normalizes its physical Backpack cells"),
		Subsystem->NormalizeDesktopInventoryState()))
	{
		return false;
	}

	UGameXXKDesktopTrainingWorkbenchWidget* Widget = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	if (!TestNotNull(TEXT("close-stack fixture widget exists"), Widget))
	{
		return false;
	}
	Widget->SetMVPSubsystem(Subsystem);
	Widget->ConstructForTest();
	TestTrue(TEXT("close-stack fixture opens the collapsed workbench"), Widget->OpenWorkbench());
	UButton* CollapsedBackpackTab = Widget->WidgetTree
		? Cast<UButton>(Widget->WidgetTree->FindWidget(TEXT("BackpackTabToggleButton")))
		: nullptr;
	if (TestNotNull(TEXT("collapsed workbench exposes the Backpack Tab"), CollapsedBackpackTab))
	{
		TestTrue(TEXT("collapsed Backpack Tab uses the approved normal tab texture"),
			GetButtonNormalResourcePath(CollapsedBackpackTab).Contains(TEXT("003_tab_1")));
		const UTextBlock* CollapsedTabLabel = Cast<UTextBlock>(CollapsedBackpackTab->GetContent());
		TestEqual(TEXT("collapsed Backpack Tab displays the down arrow"),
			CollapsedTabLabel ? CollapsedTabLabel->GetText().ToString() : FString(), FString(TEXT("▼")));
	}
	TestNull(TEXT("collapsed workbench has no Backpack paper close button"),
		Widget->WidgetTree ? Widget->WidgetTree->FindWidget(TEXT("BackpackPanelCloseButton")) : nullptr);
	TestTrue(TEXT("backpack opens"), Widget->OpenBackpack());

	UButton* ExpandedBackpackTab = Widget->WidgetTree
		? Cast<UButton>(Widget->WidgetTree->FindWidget(TEXT("BackpackTabToggleButton")))
		: nullptr;
	if (TestNotNull(TEXT("expanded Backpack retains its separate Tab control"), ExpandedBackpackTab))
	{
		const FButtonStyle& ExpandedTabStyle = ExpandedBackpackTab->GetStyle();
		TestTrue(TEXT("expanded Backpack Tab uses the approved selected tab texture"),
			GetButtonNormalResourcePath(ExpandedBackpackTab).Contains(TEXT("004_tab_2")));
		const auto BrushUsesCloseInk = [](const FSlateBrush& Brush)
		{
			const UObject* Resource = Brush.GetResourceObject();
			return Resource && Resource->GetPathName().Contains(TEXT("CloseInk"));
		};
		TestFalse(TEXT("expanded Backpack Tab normal brush does not use CloseInk"), BrushUsesCloseInk(ExpandedTabStyle.Normal));
		TestFalse(TEXT("expanded Backpack Tab hovered brush does not use CloseInk"), BrushUsesCloseInk(ExpandedTabStyle.Hovered));
		TestFalse(TEXT("expanded Backpack Tab pressed brush does not use CloseInk"), BrushUsesCloseInk(ExpandedTabStyle.Pressed));
		TestFalse(TEXT("expanded Backpack Tab disabled brush does not use CloseInk"), BrushUsesCloseInk(ExpandedTabStyle.Disabled));
		const UTextBlock* ExpandedTabLabel = Cast<UTextBlock>(ExpandedBackpackTab->GetContent());
		TestEqual(TEXT("expanded Backpack Tab displays the up arrow"),
			ExpandedTabLabel ? ExpandedTabLabel->GetText().ToString() : FString(), FString(TEXT("▲")));
	}

	UButton* BackpackPanelClose = Widget->WidgetTree
		? Cast<UButton>(Widget->WidgetTree->FindWidget(TEXT("BackpackPanelCloseButton")))
		: nullptr;
	if (TestNotNull(TEXT("Backpack page exposes a separate paper close button"), BackpackPanelClose))
	{
		TestTrue(TEXT("Backpack paper close uses the approved CloseInk"),
			GetButtonNormalResourcePath(BackpackPanelClose).Contains(TEXT("CloseInk")));
		const FString ExpectedCloseDescription(TEXT("关闭背包与全部子界面"));
		TestEqual(TEXT("Backpack paper close exposes the global-close tooltip"),
			BackpackPanelClose->GetToolTipText().ToString(), ExpectedCloseDescription);
		const UTextBlock* AccessibleCloseLabel = Cast<UTextBlock>(BackpackPanelClose->GetContent());
		TestEqual(TEXT("Backpack paper close owns accessible label text"),
			AccessibleCloseLabel ? AccessibleCloseLabel->GetText().ToString() : FString(), ExpectedCloseDescription);
		TestEqual(TEXT("Backpack paper accessible label remains visually transparent"),
			AccessibleCloseLabel ? AccessibleCloseLabel->GetRenderOpacity() : 1.0f, 0.0f);
#if WITH_ACCESSIBILITY
		BackpackPanelClose->TakeWidget();
		BackpackPanelClose->SynchronizeProperties();
		TestTrue(TEXT("Backpack paper close exposes its label to Slate accessibility"),
			BackpackPanelClose->GetAccessibleText().ToString().Contains(ExpectedCloseDescription));
#endif
		const UCanvasPanelSlot* CloseSlot = Cast<UCanvasPanelSlot>(BackpackPanelClose->Slot);
		if (TestNotNull(TEXT("Backpack paper close has a canvas slot"), CloseSlot))
		{
			const FVector4 ContentRect = GameXXKDesktopTrainingLayout::GetContentRect();
			const FVector2D ClosePosition = CloseSlot->GetPosition();
			const FVector2D CloseSize = CloseSlot->GetSize();
			TestTrue(TEXT("Backpack paper close is fully inside the paper ContentRect"),
				ClosePosition.X >= ContentRect.X
				&& ClosePosition.Y >= ContentRect.Y
				&& ClosePosition.X + CloseSize.X <= ContentRect.X + ContentRect.Z
				&& ClosePosition.Y + CloseSize.Y <= ContentRect.Y + ContentRect.W);
			TestTrue(TEXT("Backpack paper close sits in the paper top-right corner"),
				ClosePosition.X >= ContentRect.X + ContentRect.Z - 100.0f
				&& ClosePosition.Y <= ContentRect.Y + 100.0f);
			const FName TopToolbarNames[] = {
				TEXT("TopToolbarAlwaysOnTop"),
				TEXT("TopToolbarMute"),
				TEXT("TopToolbarMail"),
				TEXT("TopToolbarSettings"),
				TEXT("TopToolbarExit")};
			for (const FName ToolbarName : TopToolbarNames)
			{
				const UWidget* ToolbarButton = Widget->WidgetTree->FindWidget(ToolbarName);
				const UCanvasPanelSlot* ToolbarSlot = ToolbarButton
					? Cast<UCanvasPanelSlot>(ToolbarButton->Slot)
					: nullptr;
				if (!TestNotNull(*FString::Printf(TEXT("%s has a canvas slot"), *ToolbarName.ToString()), ToolbarSlot))
				{
					continue;
				}
				const FVector2D ToolbarPosition = ToolbarSlot->GetPosition();
				const FVector2D ToolbarSize = ToolbarSlot->GetSize();
				const bool bIntersectsToolbar = ClosePosition.X < ToolbarPosition.X + ToolbarSize.X
					&& ClosePosition.X + CloseSize.X > ToolbarPosition.X
					&& ClosePosition.Y < ToolbarPosition.Y + ToolbarSize.Y
					&& ClosePosition.Y + CloseSize.Y > ToolbarPosition.Y;
				TestFalse(*FString::Printf(TEXT("Backpack paper close does not intersect %s"), *ToolbarName.ToString()),
					bIntersectsToolbar);
			}
		}
	}
	TArray<UWidget*> ExpandedWidgets;
	Widget->WidgetTree->GetAllWidgets(ExpandedWidgets);
	int32 BackpackPanelCloseCount = 0;
	for (const UWidget* Child : ExpandedWidgets)
	{
		BackpackPanelCloseCount += Child && Child->GetFName() == TEXT("BackpackPanelCloseButton") ? 1 : 0;
	}
	TestEqual(TEXT("Backpack page creates exactly one paper close button"), BackpackPanelCloseCount, 1);

	Widget->HandleActionClicked(3); // Exercise the expanded Tab against real transient state.
	const int32 TabCloseToolSlot = FGameXXKDesktopInventoryRules::FindEntrySlot(
		Subsystem->GetRuntimeState(),
		EGameXXKDesktopItemContainer::Backpack,
		FGameXXKDesktopInventoryRules::MakeEquipmentEntry(ToolInputEquipment));
	TestTrue(TEXT("expanded Tab fixture reserves a real tool input"),
		TabCloseToolSlot != INDEX_NONE && Widget->RightClickBackpackSlotForTest(TabCloseToolSlot));
	const int32 TabCloseEquipmentSlot = FGameXXKDesktopInventoryRules::FindEntrySlot(
		Subsystem->GetRuntimeState(),
		EGameXXKDesktopItemContainer::Backpack,
		FGameXXKDesktopInventoryRules::MakeEquipmentEntry(CarryEquipment));
	TestTrue(TEXT("expanded Tab fixture carries a real Backpack item"),
		TabCloseEquipmentSlot != INDEX_NONE && Widget->PickUpBackpackSlotForTest(TabCloseEquipmentSlot));
	ExpandedBackpackTab = Widget->WidgetTree
		? Cast<UButton>(Widget->WidgetTree->FindWidget(TEXT("BackpackTabToggleButton")))
		: nullptr;
	if (ExpandedBackpackTab)
	{
		ExpandedBackpackTab->OnClicked.Broadcast();
	}
	TestFalse(TEXT("clicking the expanded Backpack Tab dispatches global collapse"), Widget->IsBackpackExpandedForTest());
	TestFalse(TEXT("expanded Backpack Tab click closes the right rail"), Widget->IsRightPanelOpenForTest());
	TestFalse(TEXT("expanded Backpack Tab click cancels the carried item"), Widget->IsCarryingItemForTest());
	TestEqual(TEXT("expanded Backpack Tab click returns tool reservations"), Widget->GetOccupiedToolSlotCountForTest(), 0);
	Widget->TickForTest(0.0f);
	CollapsedBackpackTab = Widget->WidgetTree
		? Cast<UButton>(Widget->WidgetTree->FindWidget(TEXT("BackpackTabToggleButton")))
		: nullptr;
	if (TestNotNull(TEXT("Tab collapse rebuilds the collapsed Tab"), CollapsedBackpackTab))
	{
		TestTrue(TEXT("rebuilt collapsed Tab uses the approved normal texture"),
			GetButtonNormalResourcePath(CollapsedBackpackTab).Contains(TEXT("003_tab_1")));
		const UTextBlock* CollapsedTabLabel = Cast<UTextBlock>(CollapsedBackpackTab->GetContent());
		TestEqual(TEXT("rebuilt collapsed Tab displays the down arrow"),
			CollapsedTabLabel ? CollapsedTabLabel->GetText().ToString() : FString(), FString(TEXT("▼")));
		TestNull(TEXT("collapsed state removes the Backpack paper close button"),
			Widget->WidgetTree ? Widget->WidgetTree->FindWidget(TEXT("BackpackPanelCloseButton")) : nullptr);
		CollapsedBackpackTab->OnClicked.Broadcast();
	}
	Widget->TickForTest(0.0f);
	TestTrue(TEXT("clicking the collapsed Backpack Tab reopens the Backpack"), Widget->IsBackpackExpandedForTest());

	Widget->HandleActionClicked(0); // Warehouse.
	Widget->HandleActionClicked(3); // Tools.
	Widget->HandleActionClicked(2); // Talents in the center.
	TestTrue(TEXT("warehouse is open"), Widget->IsWarehousePanelOpenForTest());
	TestTrue(TEXT("tools are open"), Widget->IsToolsPanelActiveForTest());
	TestEqual(TEXT("talents own the center"), Widget->GetActiveCenterPageForTest(), EGameXXKDesktopTrainingCenterPage::Talents);
	TestNotNull(TEXT("warehouse owns a local close control"),
		Widget->WidgetTree ? Widget->WidgetTree->FindWidget(TEXT("WarehouseCloseButton")) : nullptr);
	TestNotNull(TEXT("talents own a local close control"),
		Widget->WidgetTree ? Widget->WidgetTree->FindWidget(TEXT("TalentsCloseButton")) : nullptr);
	TestNull(TEXT("Talents page does not reuse the Backpack paper close button"),
		Widget->WidgetTree ? Widget->WidgetTree->FindWidget(TEXT("BackpackPanelCloseButton")) : nullptr);
	TestNotNull(TEXT("tools own a local close control"),
		Widget->WidgetTree ? Widget->WidgetTree->FindWidget(TEXT("ToolsCloseButton")) : nullptr);

	Widget->HandleActionClicked(63); // Central close.
	TestEqual(TEXT("central close returns to backpack"), Widget->GetActiveCenterPageForTest(), EGameXXKDesktopTrainingCenterPage::Backpack);
	TestTrue(TEXT("central close preserves warehouse"), Widget->IsWarehousePanelOpenForTest());
	TestTrue(TEXT("central close preserves tools"), Widget->IsToolsPanelActiveForTest());

	Widget->HandleActionClicked(1); // Formation in the center.
	TestNotNull(TEXT("formation owns a local close control"),
		Widget->WidgetTree ? Widget->WidgetTree->FindWidget(TEXT("FormationCloseButton")) : nullptr);
	TestNull(TEXT("Formation page does not reuse the Backpack paper close button"),
		Widget->WidgetTree ? Widget->WidgetTree->FindWidget(TEXT("BackpackPanelCloseButton")) : nullptr);
	Widget->HandleActionClicked(63);
	TestEqual(TEXT("formation close also returns to backpack"),
		Widget->GetActiveCenterPageForTest(), EGameXXKDesktopTrainingCenterPage::Backpack);

	Widget->HandleActionClicked(62); // Warehouse close.
	TestFalse(TEXT("warehouse close affects only warehouse"), Widget->IsWarehousePanelOpenForTest());
	TestTrue(TEXT("warehouse close preserves tools"), Widget->IsToolsPanelActiveForTest());

	Widget->HandleActionClicked(64); // Right-panel close.
	TestFalse(TEXT("right close closes tools"), Widget->IsRightPanelOpenForTest());

	Widget->HandleActionClicked(4); // Training right panel.
	TestTrue(TEXT("training opens on the right"), Widget->IsRightPanelOpenForTest());
	TestNotNull(TEXT("training owns a local close control"),
		Widget->WidgetTree ? Widget->WidgetTree->FindWidget(TEXT("TrainingCloseButton")) : nullptr);
	Widget->HandleActionClicked(64);
	TestFalse(TEXT("right close also closes training"), Widget->IsRightPanelOpenForTest());

	Widget->HandleActionClicked(3); // Tools, then reserve one real backpack entry.
	const int32 ToolInputSlot = FGameXXKDesktopInventoryRules::FindEntrySlot(
		Subsystem->GetRuntimeState(),
		EGameXXKDesktopItemContainer::Backpack,
		FGameXXKDesktopInventoryRules::MakeEquipmentEntry(ToolInputEquipment));
	TestTrue(TEXT("tool reservation is created before global close"),
		ToolInputSlot != INDEX_NONE && Widget->RightClickBackpackSlotForTest(ToolInputSlot));
	TestEqual(TEXT("one tool reservation exists before global close"), Widget->GetOccupiedToolSlotCountForTest(), 1);
	Widget->HandleActionClicked(0);
	const TArray<FName> CompanionIds = Widget->GetCompanionCharacterIdsForTest();
	if (!TestTrue(TEXT("fixture exposes a permanent companion Backpack owner"), CompanionIds.Num() > 0))
	{
		return false;
	}
	TestTrue(TEXT("permanent companion Backpack is selected before global close"),
		Widget->SelectBackpackCharacterForTest(CompanionIds[0]));
	TestEqual(TEXT("the selected permanent companion owns Backpack before global close"),
		Widget->GetActiveBackpackCharacterIdForTest(), CompanionIds[0]);
	Widget->HandleActionClicked(1);
	const int32 EquipmentSlot = FGameXXKDesktopInventoryRules::FindEntrySlot(
		Subsystem->GetRuntimeState(),
		EGameXXKDesktopItemContainer::Backpack,
		FGameXXKDesktopInventoryRules::MakeEquipmentEntry(CarryEquipment));
	TestTrue(TEXT("an item is carried before global close"),
		EquipmentSlot != INDEX_NONE && Widget->PickUpBackpackSlotForTest(EquipmentSlot));
	Widget->HandleActionClicked(63); // Return to Backpack so its paper X is visible.
	BackpackPanelClose = Widget->WidgetTree
		? Cast<UButton>(Widget->WidgetTree->FindWidget(TEXT("BackpackPanelCloseButton")))
		: nullptr;
	if (TestNotNull(TEXT("Backpack paper X is restored after local Formation close"), BackpackPanelClose))
	{
		BackpackPanelClose->OnClicked.Broadcast();
	}
	TestFalse(TEXT("global close collapses backpack"), Widget->IsBackpackExpandedForTest());
	TestFalse(TEXT("global close closes warehouse"), Widget->IsWarehousePanelOpenForTest());
	TestFalse(TEXT("global close closes right rail"), Widget->IsRightPanelOpenForTest());
	TestFalse(TEXT("global close cancels carried item"), Widget->IsCarryingItemForTest());
	TestEqual(TEXT("global close returns all tool reservations"), Widget->GetOccupiedToolSlotCountForTest(), 0);
	Widget->TickForTest(0.0f);
	TestNull(TEXT("paper X is absent after its global close callback collapses the Backpack"),
		Widget->WidgetTree ? Widget->WidgetTree->FindWidget(TEXT("BackpackPanelCloseButton")) : nullptr);

	Widget->HandleActionClicked(60); // Keyboard Tab and the X share this action.
	TestTrue(TEXT("Tab reopens"), Widget->IsBackpackExpandedForTest());
	TestEqual(TEXT("reopen starts on clean backpack"), Widget->GetActiveCenterPageForTest(), EGameXXKDesktopTrainingCenterPage::Backpack);
	TestFalse(TEXT("reopen does not restore warehouse"), Widget->IsWarehousePanelOpenForTest());
	TestFalse(TEXT("reopen does not restore right rail"), Widget->IsRightPanelOpenForTest());
	TestEqual(TEXT("reopen resets the Backpack owner to the default hero"),
		Widget->GetActiveBackpackCharacterIdForTest(), FGameXXKEquipmentRules::HeroCharacterId());
	TestEqual(TEXT("the rebuilt embedded Backpack is configured for the default hero"),
		Widget->GetEmbeddedBackpackCharacterIdForTest(), FGameXXKEquipmentRules::HeroCharacterId());
	UButton* HeroRosterButton = Widget->WidgetTree
		? Cast<UButton>(Widget->WidgetTree->FindWidget(TEXT("CharacterRosterHeroButton")))
		: nullptr;
	UButton* CompanionRosterButton = Widget->WidgetTree
		? Cast<UButton>(Widget->WidgetTree->FindWidget(TEXT("CharacterRosterCompanionButton")))
		: nullptr;
	if (TestNotNull(TEXT("reopen exposes the hero roster tab"), HeroRosterButton)
		&& TestNotNull(TEXT("reopen exposes the companion roster tab"), CompanionRosterButton))
	{
		const UObject* HeroTabTexture = HeroRosterButton->GetStyle().Normal.GetResourceObject();
		const UObject* CompanionTabTexture = CompanionRosterButton->GetStyle().Normal.GetResourceObject();
		TestTrue(TEXT("reopen keeps the folded hero roster tab unselected until its member row is expanded"),
			HeroTabTexture && HeroTabTexture->GetPathName().Contains(TEXT("003_tab_1")));
		TestTrue(TEXT("reopen does not retain the companion roster selection"),
			CompanionTabTexture && CompanionTabTexture->GetPathName().Contains(TEXT("003_tab_1")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingWorkbenchLocalCloseSessionTest,
	"GameXXK.DesktopTraining.Workbench.LocalClosePreservesEmbeddedSession",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingWorkbenchLocalCloseSessionTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	if (!TestNotNull(TEXT("local-close fixture subsystem exists"), Subsystem)
		|| !Subsystem->StartGame())
	{
		return false;
	}

	UGameXXKDesktopTrainingWorkbenchWidget* Widget = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	if (!TestNotNull(TEXT("local-close fixture widget exists"), Widget))
	{
		return false;
	}
	Widget->SetMVPSubsystem(Subsystem);
	Widget->ConstructForTest();
	TestTrue(TEXT("local-close fixture opens Backpack"), Widget->OpenBackpack());

	const auto StageNonDefaultDeckSession = [this, Widget](const TCHAR* Context) -> TArray<FName>
	{
		UGameXXKInventoryWindowWidget* Embedded = Widget->WidgetTree
			? Cast<UGameXXKInventoryWindowWidget>(Widget->WidgetTree->FindWidget(TEXT("EmbeddedApprovedBackpack")))
			: nullptr;
		if (!TestNotNull(*FString::Printf(TEXT("%s owns an embedded Backpack"), Context), Embedded))
		{
			return TArray<FName>();
		}
		if (!TestTrue(*FString::Printf(TEXT("%s opens the Deck tab"), Context),
			Embedded->OpenCharacterBackpackTabForTest(EGameXXKCharacterBackpackTab::Deck)))
		{
			return TArray<FName>();
		}
		const TArray<FName> OriginalDeck = Embedded->GetPendingHeroDeckIdsForTest();
		if (!TestTrue(*FString::Printf(TEXT("%s starts with a staged card"), Context), OriginalDeck.Num() > 0)
			|| !Embedded->ToggleHeroDeckCardForTest(OriginalDeck[0]))
		{
			return TArray<FName>();
		}
		const TArray<FName> DraftDeck = Embedded->GetPendingHeroDeckIdsForTest();
		TestEqual(*FString::Printf(TEXT("%s owns one unapplied deck edit"), Context),
			DraftDeck.Num(), OriginalDeck.Num() - 1);
		return DraftDeck;
	};

	Widget->HandleActionClicked(0); // Open Warehouse before staging the local session.
	const TArray<FName> WarehouseDraft = StageNonDefaultDeckSession(TEXT("warehouse close"));
	if (WarehouseDraft.IsEmpty())
	{
		return false;
	}
	UButton* WarehouseClose = Widget->WidgetTree
		? Cast<UButton>(Widget->WidgetTree->FindWidget(TEXT("WarehouseCloseButton")))
		: nullptr;
	if (!TestNotNull(TEXT("warehouse exposes its real close button"), WarehouseClose))
	{
		return false;
	}
	const int32 WarehouseBuildCount = Widget->GetProgrammaticLayoutBuildCountForTest();
	WarehouseClose->OnClicked.Broadcast();
	TestEqual(TEXT("warehouse close callback performs no synchronous rebuild"),
		Widget->GetProgrammaticLayoutBuildCountForTest(), WarehouseBuildCount);
	TestTrue(TEXT("warehouse close callback leaves one parent refresh pending"), Widget->HasPendingLayoutRefreshForTest());
	Widget->TickForTest(0.0f);
	TestEqual(TEXT("warehouse close next tick performs exactly one rebuild"),
		Widget->GetProgrammaticLayoutBuildCountForTest(), WarehouseBuildCount + 1);
	TestFalse(TEXT("warehouse close closes only Warehouse"), Widget->IsWarehousePanelOpenForTest());
	TestEqual(TEXT("warehouse close leaves Backpack in the center"),
		Widget->GetActiveCenterPageForTest(), EGameXXKDesktopTrainingCenterPage::Backpack);
	TestEqual(TEXT("warehouse close restores the embedded Deck tab"),
		Widget->GetEmbeddedBackpackTabForTest(), EGameXXKCharacterBackpackTab::Deck);
	TestEqual(TEXT("warehouse close restores the unapplied deck edit"),
		Widget->GetEmbeddedPendingDeckIdsForTest(), WarehouseDraft);

	Widget->HandleActionClicked(3); // Open Tools before staging a fresh local session.
	const TArray<FName> ToolsDraft = StageNonDefaultDeckSession(TEXT("tools close"));
	if (ToolsDraft.IsEmpty())
	{
		return false;
	}
	UButton* ToolsClose = Widget->WidgetTree
		? Cast<UButton>(Widget->WidgetTree->FindWidget(TEXT("ToolsCloseButton")))
		: nullptr;
	if (!TestNotNull(TEXT("tools exposes its real close button"), ToolsClose))
	{
		return false;
	}
	const int32 ToolsBuildCount = Widget->GetProgrammaticLayoutBuildCountForTest();
	ToolsClose->OnClicked.Broadcast();
	TestEqual(TEXT("tools close callback performs no synchronous rebuild"),
		Widget->GetProgrammaticLayoutBuildCountForTest(), ToolsBuildCount);
	TestTrue(TEXT("tools close callback leaves one parent refresh pending"), Widget->HasPendingLayoutRefreshForTest());
	Widget->TickForTest(0.0f);
	TestEqual(TEXT("tools close next tick performs exactly one rebuild"),
		Widget->GetProgrammaticLayoutBuildCountForTest(), ToolsBuildCount + 1);
	TestFalse(TEXT("tools close closes only the right rail"), Widget->IsRightPanelOpenForTest());
	TestEqual(TEXT("tools close leaves Backpack in the center"),
		Widget->GetActiveCenterPageForTest(), EGameXXKDesktopTrainingCenterPage::Backpack);
	TestEqual(TEXT("tools close restores the embedded Deck tab"),
		Widget->GetEmbeddedBackpackTabForTest(), EGameXXKCharacterBackpackTab::Deck);
	TestEqual(TEXT("tools close restores the unapplied deck edit"),
		Widget->GetEmbeddedPendingDeckIdsForTest(), ToolsDraft);

	Widget->HandleActionClicked(1);
	UButton* FormationClose = Widget->WidgetTree
		? Cast<UButton>(Widget->WidgetTree->FindWidget(TEXT("FormationCloseButton")))
		: nullptr;
	if (!TestNotNull(TEXT("formation exposes its real close button"), FormationClose))
	{
		return false;
	}
	const int32 FormationBuildCount = Widget->GetProgrammaticLayoutBuildCountForTest();
	FormationClose->OnClicked.Broadcast();
	TestEqual(TEXT("formation close callback performs no synchronous rebuild"),
		Widget->GetProgrammaticLayoutBuildCountForTest(), FormationBuildCount);
	TestTrue(TEXT("formation close callback leaves one parent refresh pending"), Widget->HasPendingLayoutRefreshForTest());
	Widget->TickForTest(0.0f);
	TestEqual(TEXT("formation close next tick performs exactly one rebuild"),
		Widget->GetProgrammaticLayoutBuildCountForTest(), FormationBuildCount + 1);
	TestEqual(TEXT("formation close button delegate returns the center to Backpack"),
		Widget->GetActiveCenterPageForTest(), EGameXXKDesktopTrainingCenterPage::Backpack);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingItemCarryBoundaryRollbackTest,
	"GameXXK.DesktopTraining.Workbench.ItemCarryBoundaryRollback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingItemCarryBoundaryRollbackTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	if (!TestNotNull(TEXT("boundary fixture subsystem exists"), Subsystem)
		|| !Subsystem->StartGame())
	{
		return false;
	}
	UGameXXKDesktopTrainingWorkbenchWidget* Widget = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	if (!TestNotNull(TEXT("boundary fixture widget exists"), Widget))
	{
		return false;
	}
	Widget->SetMVPSubsystem(Subsystem);
	Widget->ConstructForTest();
	TestTrue(TEXT("boundary fixture opens backpack"), Widget->OpenBackpack());

	const FName StoneId = UGameXXKMVPRules::ItemEnhancementStone();
	const int32 OriginalStoneSlot = Widget->FindBackpackItemSlotForTest(StoneId);
	const int32 OccupiedEquipmentSlot = Widget->FindFirstBackpackEquipmentSlotForTest();
	TestTrue(TEXT("fixture exposes an occupied non-origin destination"),
		OriginalStoneSlot != INDEX_NONE && OccupiedEquipmentSlot != INDEX_NONE && OccupiedEquipmentSlot != OriginalStoneSlot);
	TestTrue(TEXT("pickup before invalid drop succeeds"), Widget->PickUpBackpackSlotForTest(OriginalStoneSlot));
	TestTrue(TEXT("occupied destination atomically swaps placement"), Widget->DropCarriedOnBackpackSlotForTest(OccupiedEquipmentSlot));
	TestFalse(TEXT("successful occupied placement clears carry"), Widget->IsCarryingItemForTest());
	TestEqual(TEXT("occupied placement moves the whole stack to the target"), Widget->FindBackpackItemSlotForTest(StoneId), OccupiedEquipmentSlot);
	TestEqual(TEXT("occupied placement preserves stack quantity"), Subsystem->GetItemCount(StoneId), 10);
	TestTrue(TEXT("reverse pickup after occupied placement succeeds"), Widget->PickUpBackpackSlotForTest(OccupiedEquipmentSlot));
	TestTrue(TEXT("reverse occupied placement restores the original cells"), Widget->DropCarriedOnBackpackSlotForTest(OriginalStoneSlot));
	TestEqual(TEXT("reverse occupied placement restores the exact origin slot"), Widget->FindBackpackItemSlotForTest(StoneId), OriginalStoneSlot);
	TestTrue(TEXT("explicit cancellation remains available after an occupied swap"),
		Widget->PickUpBackpackSlotForTest(OriginalStoneSlot) && Widget->CancelCarriedItemForTest());

	const TArray<FName> CharacterIds = Widget->GetBackpackCharacterIdsForTest();
	if (TestTrue(TEXT("new game exposes a second backpack owner"), CharacterIds.Num() > 1))
	{
		TestTrue(TEXT("pickup before owner switch succeeds"), Widget->PickUpBackpackSlotForTest(OriginalStoneSlot));
		TestTrue(TEXT("switching role/partner succeeds"), Widget->SelectBackpackCharacterForTest(CharacterIds[1]));
		TestFalse(TEXT("owner switch cancels cursor payload"), Widget->IsCarryingItemForTest());
	}

	const int32 StoneSlotAfterOwnerSwitch = Widget->FindBackpackItemSlotForTest(StoneId);
	TestTrue(TEXT("pickup before backpack sort succeeds"), Widget->PickUpBackpackSlotForTest(StoneSlotAfterOwnerSwitch));
	Widget->HandleActionClicked(61);
	TestFalse(TEXT("backpack sort cancels cursor payload"), Widget->IsCarryingItemForTest());

	Widget->HandleActionClicked(0);
	TestTrue(TEXT("warehouse opens independently"), Widget->IsWarehousePanelOpenForTest());
	TestTrue(TEXT("pickup before warehouse close succeeds"),
		Widget->PickUpBackpackSlotForTest(Widget->FindBackpackItemSlotForTest(StoneId)));
	Widget->HandleActionClicked(0);
	TestFalse(TEXT("closing the warehouse cancels cursor payload"), Widget->IsCarryingItemForTest());

	Widget->HandleActionClicked(3);
	TestTrue(TEXT("tools panel opens independently"), Widget->IsToolsPanelActiveForTest());
	const int32 BoundaryToolEquipmentSlot = Widget->FindFirstBackpackEquipmentSlotForTest();
	TestTrue(TEXT("tool reservation is created"),
		Widget->RightClickBackpackSlotForTest(BoundaryToolEquipmentSlot));
	TestTrue(TEXT("reserved tool input can be picked up"), Widget->PickUpToolSlotForTest(0));
	Widget->HandleActionClicked(4);
	TestFalse(TEXT("switching right-side page cancels cursor payload"), Widget->IsCarryingItemForTest());
	TestEqual(TEXT("switching away from tools clears transient reservations"), Widget->GetOccupiedToolSlotCountForTest(), 0);
	TestEqual(TEXT("switching away from tools preserves authoritative quantity"), Subsystem->GetItemCount(StoneId), 10);

	TestTrue(TEXT("pickup before exit confirmation succeeds"),
		Widget->PickUpBackpackSlotForTest(Widget->FindBackpackItemSlotForTest(StoneId)));
	Widget->HandleActionClicked(15);
	TestFalse(TEXT("opening exit confirmation cancels cursor payload"), Widget->IsCarryingItemForTest());
	Widget->HandleActionClicked(53);

	TestTrue(TEXT("pickup before application deactivation succeeds"),
		Widget->PickUpBackpackSlotForTest(Widget->FindBackpackItemSlotForTest(StoneId)));
	Widget->NotifyApplicationDeactivatedForTest();
	TestFalse(TEXT("application focus loss cancels cursor payload"), Widget->IsCarryingItemForTest());

	TestTrue(TEXT("pickup before external Slate rebuild succeeds"),
		Widget->PickUpBackpackSlotForTest(Widget->FindBackpackItemSlotForTest(StoneId)));
	Widget->ForceExternalSlateRebuildForTest();
	TestFalse(TEXT("external widget rebuild cancels cursor payload"), Widget->IsCarryingItemForTest());

	Subsystem->SetSaveSlotWriteDelegateForTest(FGameXXKSaveSlotWriteDelegate::CreateLambda(
		[](USaveGame*, const FString&, const int32)
		{
			return true;
		}));
	TestTrue(TEXT("pickup before save boundary succeeds"),
		Widget->PickUpBackpackSlotForTest(Widget->FindBackpackItemSlotForTest(StoneId)));
	TestTrue(TEXT("save boundary fixture succeeds without filesystem IO"),
		Subsystem->SaveCurrentGame(TEXT("DesktopCarryBoundary"), 991));
	TestFalse(TEXT("save/load boundary cancels cursor payload before serialization"), Widget->IsCarryingItemForTest());
	Subsystem->ResetSaveSlotWriteDelegateForTest();

	TestTrue(TEXT("pickup before widget destruction succeeds"),
		Widget->PickUpBackpackSlotForTest(Widget->FindBackpackItemSlotForTest(StoneId)));
	Widget->DestructForTest();
	TestFalse(TEXT("widget destruction cancels cursor payload"), Widget->IsCarryingItemForTest());
	TestEqual(TEXT("all rollback boundaries preserve stack quantity"), Subsystem->GetItemCount(StoneId), 10);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingWorkbenchTravelTickAvoidsSlateRebuildTest,
	"GameXXK.DesktopTraining.Workbench.TravelTickAvoidsSlateRebuild",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingWorkbenchTravelTickAvoidsSlateRebuildTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	TestNotNull(TEXT("travel tick fixture subsystem exists"), Subsystem);
	if (!Subsystem || !Subsystem->StartGame())
	{
		return false;
	}

	const FName StageId = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 1);
	TestTrue(TEXT("travel tick fixture starts a cleared stage"), Subsystem->StartTrainingTravel(StageId));

	UGameXXKDesktopTrainingWorkbenchWidget* Widget = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	TestNotNull(TEXT("travel tick fixture widget exists"), Widget);
	if (!Widget)
	{
		return false;
	}
	Widget->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("travel tick fixture opens the workbench"), Widget->OpenWorkbench());
	const int32 InitialLayoutBuildCount = Widget->GetProgrammaticLayoutBuildCountForTest();

	// Logical combat now uses the same 1-1 health as active challenge. The first
	// few seconds may still be walking or fighting, but must update the existing
	// strip rather than scheduling a whole-tree rebuild.
	for (int32 TickIndex = 0; TickIndex < 4; ++TickIndex)
	{
		Widget->TickForTest(1.0f);
	}
	TestFalse(TEXT("travel NativeTick schedules no layout rebuild"), Widget->HasPendingLayoutRefreshForTest());
	TestEqual(TEXT("travel settlement preserves the existing widget tree"), Widget->GetProgrammaticLayoutBuildCountForTest(), InitialLayoutBuildCount);
	TestTrue(TEXT("in-place travel refresh keeps the workbench visible"), Widget->IsWorkbenchVisibleForTest());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingWorkbenchTravelPartyAtlasFallbackInventoryTest,
	"GameXXK.DesktopTraining.Workbench.TravelPartyAtlasFallbackInventory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingWorkbenchTravelPartyAtlasFallbackInventoryTest::RunTest(const FString& Parameters)
{
	struct FPartyAtlasExpectation
	{
		const TCHAR* RuntimeUnitId;
		bool bHasOneK;
	};
	const FPartyAtlasExpectation Expectations[] = {
		{TEXT("Companion_Blade_Test"), true},
		{TEXT("Companion_Guard_Test"), false},
		{TEXT("Companion_Healer_Test"), false},
		{TEXT("Companion_Hunter_Test"), false},
		{TEXT("Companion_Sorcerer_Test"), false},
		{TEXT("Companion_FormationMaster_Test"), false},
		{TEXT("Npc.TusiChief"), true},
		{TEXT("Npc.SongJinBao"), false},
		{TEXT("Npc.YueBai"), false},
		{TEXT("Npc.ZhouGuangZu"), false},
		{TEXT("Npc.JinGui"), false},
		{TEXT("Npc.QiongMeiEr"), false}};
	const EGameXXKBattleAnimationAction Actions[] = {
		EGameXXKBattleAnimationAction::Idle,
		EGameXXKBattleAnimationAction::Attack,
		EGameXXKBattleAnimationAction::Hit,
		EGameXXKBattleAnimationAction::Death};
	const auto PackageExists = [](const FGameXXKBattleAnimationClipDescriptor& Clip)
	{
		return Clip.IsValid() && FPackageName::DoesPackageExist(Clip.TexturePath.GetLongPackageName());
	};

	for (const FPartyAtlasExpectation& Expectation : Expectations)
	{
		for (const EGameXXKBattleAnimationAction Action : Actions)
		{
			const FGameXXKBattleAnimationClipPair Pair =
				FGameXXKBattleAnimationPresentation::ResolveCompactTravelClipPair(
					FName(Expectation.RuntimeUnitId),
					false,
					Action);
			const FString Label = FString::Printf(
				TEXT("%s action %d"),
				Expectation.RuntimeUnitId,
				static_cast<int32>(Action));
			TestTrue(*FString::Printf(TEXT("%s resolves a preferred 1K descriptor"), *Label), Pair.Preferred.IsValid());
			TestTrue(*FString::Printf(TEXT("%s resolves a fallback 2K descriptor"), *Label), Pair.Fallback.IsValid());
			TestTrue(*FString::Printf(TEXT("%s prefers the compact 1K package path"), *Label),
				Pair.Preferred.TexturePath.ToString().Contains(TEXT("_1k_")));
			TestTrue(*FString::Printf(TEXT("%s falls back to the matching 2K package path"), *Label),
				Pair.Fallback.TexturePath.ToString().Contains(TEXT("_2k_")));
			const bool bPreferredExists = PackageExists(Pair.Preferred);
			const bool bFallbackExists = PackageExists(Pair.Fallback);
			TestTrue(*FString::Printf(TEXT("%s has at least one loadable compact Travel package"), *Label),
				bPreferredExists || bFallbackExists);
			TestEqual(*FString::Printf(TEXT("%s has the expected 1K inventory state"), *Label),
				bPreferredExists,
				Expectation.bHasOneK);
			TestTrue(*FString::Printf(TEXT("%s keeps its existing 2K fallback package"), *Label), bFallbackExists);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingWorkbenchTravelPartyAtlasAsyncFallbackTest,
	"GameXXK.DesktopTraining.Workbench.TravelPartyAtlasAsyncFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingWorkbenchTravelPartyAtlasAsyncFallbackTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	if (!TestNotNull(TEXT("Travel atlas fallback fixture subsystem exists"), Subsystem)
		|| !TestTrue(TEXT("Travel atlas fallback fixture starts the game"), Subsystem->StartGame()))
	{
		return false;
	}

	FGameXXKRuntimeState& InitialState = Subsystem->GetMutableRuntimeState();
	const FGameXXKPermanentCompanion* Blade = InitialState.CardRun.CompanionRoster.PermanentCompanions.FindByPredicate(
		[](const FGameXXKPermanentCompanion& Candidate)
		{
			return Candidate.bIsActive && Candidate.InstanceId.ToString().Contains(TEXT("Companion_Blade_"));
		});
	if (!TestNotNull(TEXT("fixture owns an active Blade"), Blade))
	{
		return false;
	}
	const FName BladeId = Blade->InstanceId;
	const FName GuardId(TEXT("CompanionInstance.Companion_Guard_01.TravelFallbackTest"));
	FString FormationError;
	TestTrue(TEXT("fixture selects Tusi Chief through ordered formation"),
		GameXXKPermanentPartyTestFixtures::SelectNpc(
			InitialState,
			TEXT("Npc.TusiChief"),
			&FormationError));
	const FName StageId = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 1);
	if (!TestTrue(TEXT("fixture starts Travel with Blade and Tusi Chief"), Subsystem->StartTrainingTravel(StageId)))
	{
		return false;
	}
	const FName InitialEnemyId = Subsystem->GetTrainingTravelRuntimeCopy().EnemyDefinitionId;

	const TSharedRef<FTravelFallbackAtlasLoader> Loader = MakeShared<FTravelFallbackAtlasLoader>();
	UGameXXKDesktopTrainingWorkbenchWidget* Widget = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	if (!TestNotNull(TEXT("Travel atlas fallback fixture widget exists"), Widget))
	{
		return false;
	}
	Widget->SetMVPSubsystem(Subsystem);
	Widget->SetTravelAtlasCacheForTest(MakeUnique<FGameXXKBattleAtlasCache>(
		Loader,
		[]() { return FPlatformTime::Seconds(); }));
	if (!TestTrue(TEXT("Travel atlas fallback fixture opens the workbench"), Widget->OpenWorkbench()))
	{
		return false;
	}

	UImage* PermanentImage = Widget->WidgetTree
		? Cast<UImage>(Widget->WidgetTree->FindWidget(TEXT("TravelCompanionAnimatedUnit_0")))
		: nullptr;
	UImage* QuestImage = Widget->WidgetTree
		? Cast<UImage>(Widget->WidgetTree->FindWidget(TEXT("TravelCompanionAnimatedUnit_1")))
		: nullptr;
	if (!TestNotNull(TEXT("fixture owns the permanent companion image"), PermanentImage)
		|| !TestNotNull(TEXT("fixture owns the quest companion image"), QuestImage))
	{
		return false;
	}

	const FGameXXKBattleAnimationClipPair BladePair =
		FGameXXKBattleAnimationPresentation::ResolveCompactTravelClipPair(
			BladeId, false, EGameXXKBattleAnimationAction::Idle);
	const FGameXXKBattleAnimationClipPair TusiPair =
		FGameXXKBattleAnimationPresentation::ResolveCompactTravelClipPair(
			TEXT("Npc.TusiChief"), false, EGameXXKBattleAnimationAction::Idle);
	TestTrue(TEXT("Blade 1K is requested first"), Loader->Requested(BladePair.Preferred.TexturePath));
	TestTrue(TEXT("Tusi Chief 1K is requested first"), Loader->Requested(TusiPair.Preferred.TexturePath));
	TestEqual(TEXT("Blade preferred Idle is requested exactly once"),
		Loader->RequestCount(BladePair.Preferred.TexturePath), 1);
	TestEqual(TEXT("Tusi preferred Idle is requested exactly once"),
		Loader->RequestCount(TusiPair.Preferred.TexturePath), 1);
	const auto MakeOneKUnitId = [](const FName UnitId)
	{
		return FName(*(UnitId.ToString() + TEXT(".1K")));
	};
	const EGameXXKBattleAnimationAction WrapperActions[] = {
		EGameXXKBattleAnimationAction::Idle,
		EGameXXKBattleAnimationAction::Attack,
		EGameXXKBattleAnimationAction::Hit,
		EGameXXKBattleAnimationAction::Death};
	for (const EGameXXKBattleAnimationAction Action : WrapperActions)
	{
		const FGameXXKBattleAnimationClipPair HeroPair =
			FGameXXKBattleAnimationPresentation::ResolveCompactTravelClipPair(
				FGameXXKEquipmentRules::HeroCharacterId(), false, Action);
		const FGameXXKBattleAnimationClipDescriptor HeroWrapper =
			FGameXXKBattleAnimationPresentation::ResolveClipForDefinition(
				MakeOneKUnitId(FGameXXKEquipmentRules::HeroCharacterId()),
				NAME_None,
				false,
				Action);
		TestEqual(TEXT("Hero single-clip wrapper keeps the authored 1K descriptor"),
			HeroWrapper.TexturePath, HeroPair.Preferred.TexturePath);
		TestEqual(TEXT("Hero single-clip wrapper requests its 1K atlas exactly once"),
			Loader->RequestCount(HeroPair.Preferred.TexturePath), 1);
		TestEqual(TEXT("Hero single-clip wrapper never pre-requests a 2K fallback"),
			Loader->RequestCount(HeroPair.Fallback.TexturePath), 0);

		const FGameXXKBattleAnimationClipPair EnemyPair =
			FGameXXKBattleAnimationPresentation::ResolveCompactTravelClipPair(
				InitialEnemyId, true, Action);
		const FName OneKEnemyId = MakeOneKUnitId(InitialEnemyId);
		const FGameXXKBattleAnimationClipDescriptor EnemyWrapper =
			FGameXXKBattleAnimationPresentation::ResolveClipForDefinition(
				OneKEnemyId,
				OneKEnemyId,
				true,
				Action);
		TestEqual(TEXT("Enemy single-clip wrapper keeps the authored 1K descriptor"),
			EnemyWrapper.TexturePath, EnemyPair.Preferred.TexturePath);
		TestEqual(TEXT("Enemy single-clip wrapper requests its 1K atlas exactly once"),
			Loader->RequestCount(EnemyPair.Preferred.TexturePath), 1);
		TestEqual(TEXT("Enemy single-clip wrapper never pre-requests a 2K fallback"),
			Loader->RequestCount(EnemyPair.Fallback.TexturePath), 0);
	}
	const FGameXXKBattleAnimationClipPair HeroIdlePair =
		FGameXXKBattleAnimationPresentation::ResolveCompactTravelClipPair(
			FGameXXKEquipmentRules::HeroCharacterId(), false, EGameXXKBattleAnimationAction::Idle);
	const FGameXXKBattleAnimationClipPair EnemyIdlePair =
		FGameXXKBattleAnimationPresentation::ResolveCompactTravelClipPair(
			InitialEnemyId, true, EGameXXKBattleAnimationAction::Idle);
	TestTrue(TEXT("fixture reports the missing Hero wrapper atlas"),
		Loader->CompleteMissing(HeroIdlePair.Preferred.TexturePath));
	TestTrue(TEXT("fixture reports the missing Enemy wrapper atlas"),
		Loader->CompleteMissing(EnemyIdlePair.Preferred.TexturePath));
	TestEqual(TEXT("failed Hero single-clip wrapper still has no fallback request"),
		Loader->RequestCount(HeroIdlePair.Fallback.TexturePath), 0);
	TestEqual(TEXT("failed Enemy single-clip wrapper still has no fallback request"),
		Loader->RequestCount(EnemyIdlePair.Fallback.TexturePath), 0);
	TestEqual(TEXT("failed Hero wrapper never retries its preferred request"),
		Loader->RequestCount(HeroIdlePair.Preferred.TexturePath), 1);
	TestEqual(TEXT("failed Enemy wrapper never retries its preferred request"),
		Loader->RequestCount(EnemyIdlePair.Preferred.TexturePath), 1);
	UTexture2D* BladeTexture = Loader->CompleteLoaded(BladePair.Preferred.TexturePath);
	UTexture2D* TusiTexture = Loader->CompleteLoaded(TusiPair.Preferred.TexturePath);
	TestNotNull(TEXT("fixture supplies the Blade 1K atlas"), BladeTexture);
	TestNotNull(TEXT("fixture supplies the Tusi Chief 1K atlas"), TusiTexture);
	TestTrue(TEXT("Blade image uses its preferred 1K texture"),
		PermanentImage->GetBrush().GetResourceObject() == BladeTexture);
	TestTrue(TEXT("Tusi image uses its preferred 1K texture"),
		QuestImage->GetBrush().GetResourceObject() == TusiTexture);
	const auto AdvanceToPermanentCompanionAttack = [Widget]()
	{
		for (int32 Guard = 0; Guard < 64; ++Guard)
		{
			if (Widget->GetTravelVisualPartyActionNameForTest(1) == TEXT("Attack"))
			{
				return true;
			}
			Widget->TickForTest(0.5f);
		}
		return false;
	};
	if (!TestTrue(TEXT("fixture reaches Blade's non-Idle attack presentation"),
		AdvanceToPermanentCompanionAttack()))
	{
		return false;
	}
	const FGameXXKBattleAnimationClipPair BladeAttackPair =
		FGameXXKBattleAnimationPresentation::ResolveCompactTravelClipPair(
			BladeId, false, EGameXXKBattleAnimationAction::Attack);
	TestTrue(TEXT("Blade Attack requests its preferred 1K atlas"),
		Loader->Requested(BladeAttackPair.Preferred.TexturePath));
	TestEqual(TEXT("Blade Attack preferred atlas is requested exactly once"),
		Loader->RequestCount(BladeAttackPair.Preferred.TexturePath), 1);
	TestNull(TEXT("the Idle Blade brush clears while Attack is pending"),
		PermanentImage->GetBrush().GetResourceObject());
	TestTrue(TEXT("the pending Blade Attack is transparent"),
		FMath::IsNearlyZero(PermanentImage->GetRenderOpacity()));
	TestTrue(TEXT("fixture reports the missing Blade Attack 1K atlas"),
		Loader->CompleteMissing(BladeAttackPair.Preferred.TexturePath));
	TestTrue(TEXT("failed Blade Attack 1K load requests its 2K fallback"),
		Loader->Requested(BladeAttackPair.Fallback.TexturePath));
	TestEqual(TEXT("Blade Attack fallback is requested exactly once"),
		Loader->RequestCount(BladeAttackPair.Fallback.TexturePath), 1);
	Widget->TickForTest(
		FGameXXKTrainingTravelVisualRuntime::HeroAttackSeconds
		+ FGameXXKTrainingTravelVisualRuntime::EnemyHitSeconds
		+ 0.01f);
	TestEqual(TEXT("Blade's completed presentation returns to Idle before the identity switch"),
		Widget->GetTravelVisualPartyActionNameForTest(1), FString(TEXT("Idle")));

	FGameXXKRuntimeState& SwitchedState = Subsystem->GetMutableRuntimeState();
	FGameXXKPermanentCompanion* SwitchedCompanion =
		SwitchedState.CardRun.CompanionRoster.PermanentCompanions.FindByPredicate(
			[BladeId](const FGameXXKPermanentCompanion& Candidate)
			{
				return Candidate.bIsActive && Candidate.InstanceId == BladeId;
			});
	if (!TestNotNull(TEXT("fixture can retag the active companion for the Guard identity switch"), SwitchedCompanion))
	{
		return false;
	}
	SwitchedCompanion->InstanceId = GuardId;
	for (FGameXXKPartyMemberRef& Ref : SwitchedState.CardRun.OrderedFormation.Members)
	{
		if (Ref.Kind == EGameXXKPartyMemberKind::PermanentCompanion
			&& Ref.MemberId == BladeId)
		{
			Ref.MemberId = GuardId;
		}
	}
	FGameXXKPartyFormationRules::ProjectCompatibility(SwitchedState);
	TestTrue(TEXT("fixture selects Song Jin Bao through ordered formation"),
		GameXXKPermanentPartyTestFixtures::SelectNpc(
			SwitchedState,
			TEXT("Npc.SongJinBao"),
			&FormationError));
	if (!TestTrue(TEXT("fixture restarts Travel with Guard and Song Jin Bao"), Subsystem->StartTrainingTravel(StageId)))
	{
		return false;
	}
	Widget->TickForTest(0.01f);

	const FGameXXKBattleAnimationClipPair GuardPair =
		FGameXXKBattleAnimationPresentation::ResolveCompactTravelClipPair(
			GuardId, false, EGameXXKBattleAnimationAction::Idle);
	const FGameXXKBattleAnimationClipPair SongPair =
		FGameXXKBattleAnimationPresentation::ResolveCompactTravelClipPair(
			TEXT("Npc.SongJinBao"), false, EGameXXKBattleAnimationAction::Idle);
	TestTrue(TEXT("Guard 1K is requested before fallback"), Loader->Requested(GuardPair.Preferred.TexturePath));
	TestTrue(TEXT("Song Jin Bao 1K is requested before fallback"), Loader->Requested(SongPair.Preferred.TexturePath));
	TestEqual(TEXT("Guard preferred Idle is requested exactly once"),
		Loader->RequestCount(GuardPair.Preferred.TexturePath), 1);
	TestEqual(TEXT("Song preferred Idle is requested exactly once"),
		Loader->RequestCount(SongPair.Preferred.TexturePath), 1);
	TestNull(TEXT("identity switch immediately clears the stale Blade brush"), PermanentImage->GetBrush().GetResourceObject());
	TestNull(TEXT("identity switch immediately clears the stale Tusi brush"), QuestImage->GetBrush().GetResourceObject());
	TestTrue(TEXT("Guard stays transparent while neither atlas is ready"), FMath::IsNearlyZero(PermanentImage->GetRenderOpacity()));
	TestTrue(TEXT("Song Jin Bao stays transparent while neither atlas is ready"), FMath::IsNearlyZero(QuestImage->GetRenderOpacity()));
	TestTrue(TEXT("Guard identity switch resets the applied path"),
		Widget->GetTravelAppliedCompanionAtlasPathForTest(0).IsNull());
	TestTrue(TEXT("Song identity switch resets the applied path"),
		Widget->GetTravelAppliedCompanionAtlasPathForTest(1).IsNull());
	TestEqual(TEXT("Guard identity switch resets the applied frame"),
		Widget->GetTravelAppliedCompanionFrameForTest(0), INDEX_NONE);
	TestEqual(TEXT("Song identity switch resets the applied frame"),
		Widget->GetTravelAppliedCompanionFrameForTest(1), INDEX_NONE);
	TStrongObjectPtr<UTexture2D> StaleBrushSentinel(NewObject<UTexture2D>(GetTransientPackage()));
	FSlateBrush StaleBrush;
	StaleBrush.SetResourceObject(StaleBrushSentinel.Get());
	PermanentImage->SetBrush(StaleBrush);
	PermanentImage->SetRenderOpacity(1.0f);
	const int32 BladeFallbackDispatchCountBefore =
		Loader->CompletionDispatchCount(BladeAttackPair.Fallback.TexturePath);
	UTexture2D* LateBladeAttackTexture = Loader->CompleteLoaded(BladeAttackPair.Fallback.TexturePath);
	TestNotNull(TEXT("fixture delivers Blade's old 2K Attack after the identity switch"), LateBladeAttackTexture);
	TestEqual(TEXT("late Blade completion dispatches through the registered loader callback exactly once"),
		Loader->CompletionDispatchCount(BladeAttackPair.Fallback.TexturePath),
		BladeFallbackDispatchCountBefore + 1);
	TestNull(TEXT("late same-session Blade callback never revives the Guard slot"),
		PermanentImage->GetBrush().GetResourceObject());
	TestTrue(TEXT("late same-session Blade callback leaves the Guard slot transparent"),
		FMath::IsNearlyZero(PermanentImage->GetRenderOpacity()));
	TestTrue(TEXT("late same-session Blade callback leaves the Guard path reset"),
		Widget->GetTravelAppliedCompanionAtlasPathForTest(0).IsNull());
	TestEqual(TEXT("late same-session Blade callback leaves the Guard frame reset"),
		Widget->GetTravelAppliedCompanionFrameForTest(0), INDEX_NONE);

	TestTrue(TEXT("fixture reports the missing Guard 1K atlas"), Loader->CompleteMissing(GuardPair.Preferred.TexturePath));
	TestTrue(TEXT("failed Guard 1K load triggers the 2K fallback request"), Loader->Requested(GuardPair.Fallback.TexturePath));
	TestTrue(TEXT("fixture reports the missing Song Jin Bao 1K atlas"), Loader->CompleteMissing(SongPair.Preferred.TexturePath));
	TestTrue(TEXT("failed Song Jin Bao 1K load triggers the 2K fallback request"), Loader->Requested(SongPair.Fallback.TexturePath));
	TestEqual(TEXT("Guard fallback is requested exactly once"),
		Loader->RequestCount(GuardPair.Fallback.TexturePath), 1);
	TestEqual(TEXT("Song fallback is requested exactly once"),
		Loader->RequestCount(SongPair.Fallback.TexturePath), 1);
	TestNull(TEXT("failed preferred loads never restore Blade"), PermanentImage->GetBrush().GetResourceObject());
	TestNull(TEXT("failed preferred loads never restore Tusi"), QuestImage->GetBrush().GetResourceObject());

	UTexture2D* GuardTexture = Loader->CompleteLoaded(GuardPair.Fallback.TexturePath);
	UTexture2D* SongTexture = Loader->CompleteLoaded(SongPair.Fallback.TexturePath);
	TestNotNull(TEXT("fixture supplies the Guard 2K fallback"), GuardTexture);
	TestNotNull(TEXT("fixture supplies the Song Jin Bao 2K fallback"), SongTexture);
	TestTrue(TEXT("Guard applies the matching 2K texture"),
		PermanentImage->GetBrush().GetResourceObject() == GuardTexture);
	TestTrue(TEXT("Song Jin Bao applies the matching 2K texture"),
		QuestImage->GetBrush().GetResourceObject() == SongTexture);
	TestTrue(TEXT("Guard returns to full opacity after fallback apply"), FMath::IsNearlyEqual(PermanentImage->GetRenderOpacity(), 1.0f));
	TestTrue(TEXT("Song Jin Bao returns to full opacity after fallback apply"), FMath::IsNearlyEqual(QuestImage->GetRenderOpacity(), 1.0f));
	TestEqual(TEXT("Guard records the applied 2K path"),
		Widget->GetTravelAppliedCompanionAtlasPathForTest(0), GuardPair.Fallback.TexturePath);
	TestEqual(TEXT("Song records the applied 2K path"),
		Widget->GetTravelAppliedCompanionAtlasPathForTest(1), SongPair.Fallback.TexturePath);
	const auto TestAppliedUv = [this](
		const TCHAR* Label,
		const UImage* Image,
		const FGameXXKBattleAnimationClipDescriptor& Clip,
		const int32 AppliedFrame)
	{
		const FBox2f Uv = Image->GetBrush().GetUVRegion();
		const FBox2f ExpectedUv = FGameXXKBattleAnimationPresentation::CalculateUvRegion(Clip, AppliedFrame);
		TestTrue(*FString::Printf(TEXT("%s uses the matching fallback UV frame"), Label),
			Uv.Min.Equals(ExpectedUv.Min, 0.0001f)
			&& Uv.Max.Equals(ExpectedUv.Max, 0.0001f));
	};
	TestAppliedUv(TEXT("Guard"), PermanentImage, GuardPair.Fallback,
		Widget->GetTravelAppliedCompanionFrameForTest(0));
	TestAppliedUv(TEXT("Song Jin Bao"), QuestImage, SongPair.Fallback,
		Widget->GetTravelAppliedCompanionFrameForTest(1));

	if (!TestTrue(TEXT("fixture reaches Guard's non-Idle attack presentation"),
		AdvanceToPermanentCompanionAttack()))
	{
		return false;
	}
	const FGameXXKBattleAnimationClipPair GuardAttackPair =
		FGameXXKBattleAnimationPresentation::ResolveCompactTravelClipPair(
			GuardId, false, EGameXXKBattleAnimationAction::Attack);
	TestTrue(TEXT("Guard Attack requests its preferred 1K atlas"),
		Loader->Requested(GuardAttackPair.Preferred.TexturePath));
	TestEqual(TEXT("Guard Attack preferred atlas is requested exactly once"),
		Loader->RequestCount(GuardAttackPair.Preferred.TexturePath), 1);
	TestNull(TEXT("Guard Idle fallback clears immediately while Attack is pending"),
		PermanentImage->GetBrush().GetResourceObject());
	TestTrue(TEXT("pending Guard Attack is transparent before session close"),
		FMath::IsNearlyZero(PermanentImage->GetRenderOpacity()));
	TestTrue(TEXT("pending Guard Attack path is reset before session close"),
		Widget->GetTravelAppliedCompanionAtlasPathForTest(0).IsNull());
	TestEqual(TEXT("pending Guard Attack frame is reset before session close"),
		Widget->GetTravelAppliedCompanionFrameForTest(0), INDEX_NONE);
	Widget->DestructForTest();
	TStrongObjectPtr<UTexture2D> ClosedSessionSentinel(NewObject<UTexture2D>(GetTransientPackage()));
	FSlateBrush ClosedSessionBrush;
	ClosedSessionBrush.SetResourceObject(ClosedSessionSentinel.Get());
	PermanentImage->SetBrush(ClosedSessionBrush);
	PermanentImage->SetRenderOpacity(1.0f);
	const int32 ClosedSessionDispatchCountBefore =
		Loader->CompletionDispatchCount(GuardAttackPair.Preferred.TexturePath);
	UTexture2D* LateClosedSessionTexture = Loader->CompleteLoaded(GuardAttackPair.Preferred.TexturePath);
	TestNotNull(TEXT("fixture delivers Guard Attack after its atlas session closes"), LateClosedSessionTexture);
	TestEqual(TEXT("closed-session loader completion dispatches exactly once"),
		Loader->CompletionDispatchCount(GuardAttackPair.Preferred.TexturePath),
		ClosedSessionDispatchCountBefore + 1);
	TestTrue(TEXT("closed-session callback never mutates the Guard brush"),
		PermanentImage->GetBrush().GetResourceObject() == ClosedSessionSentinel.Get());
	TestTrue(TEXT("closed-session callback never mutates Guard opacity"),
		FMath::IsNearlyEqual(PermanentImage->GetRenderOpacity(), 1.0f));
	TestTrue(TEXT("closed-session callback leaves the Guard path reset"),
		Widget->GetTravelAppliedCompanionAtlasPathForTest(0).IsNull());
	TestEqual(TEXT("closed-session callback leaves the Guard frame reset"),
		Widget->GetTravelAppliedCompanionFrameForTest(0), INDEX_NONE);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingWorkbenchTravelPartyAtlasSynchronousFallbackTest,
	"GameXXK.DesktopTraining.Workbench.TravelPartyAtlasSynchronousFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingWorkbenchTravelPartyAtlasSynchronousFallbackTest::RunTest(
	const FString& Parameters)
{
	const FName GuardId(TEXT("CompanionInstance.Companion_Guard_01.SynchronousFallbackTest"));
	const FGameXXKBattleAnimationClipPair GuardPair =
		FGameXXKBattleAnimationPresentation::ResolveCompactTravelClipPair(
			GuardId, false, EGameXXKBattleAnimationAction::Idle);
	const TSharedRef<FTravelFallbackAtlasLoader> Loader = MakeShared<FTravelFallbackAtlasLoader>();
	Loader->SetSynchronousMissing(GuardPair.Preferred.TexturePath);
	UTexture2D* ExpectedFallbackTexture = Loader->SetSynchronousLoaded(GuardPair.Fallback.TexturePath);
	TUniquePtr<FGameXXKBattleAtlasCache> Cache = MakeUnique<FGameXXKBattleAtlasCache>(
		Loader,
		[]() { return FPlatformTime::Seconds(); });
	FTravelAtlasWidgetFixture Fixture;
	FString FixtureError;
	if (!TestTrue(TEXT("synchronous fallback fixture opens"), BuildTravelAtlasWidgetFixture(
		GuardId,
		TEXT("Npc.TusiChief"),
		MoveTemp(Cache),
		Fixture,
		FixtureError)))
	{
		AddError(FixtureError);
		return false;
	}

	TestEqual(TEXT("reentrant preferred request occurs exactly once"),
		Loader->RequestCount(GuardPair.Preferred.TexturePath), 1);
	TestEqual(TEXT("reentrant fallback request occurs exactly once"),
		Loader->RequestCount(GuardPair.Fallback.TexturePath), 1);
	TestEqual(TEXT("synchronous preferred failure dispatches exactly once"),
		Loader->CompletionDispatchCount(GuardPair.Preferred.TexturePath), 1);
	TestEqual(TEXT("synchronous fallback load dispatches exactly once"),
		Loader->CompletionDispatchCount(GuardPair.Fallback.TexturePath), 1);
	TestFalse(TEXT("synchronous preferred completion leaves no pending loader callback"),
		Loader->HasPendingCompletion(GuardPair.Preferred.TexturePath));
	TestFalse(TEXT("synchronous fallback completion leaves no pending loader callback"),
		Loader->HasPendingCompletion(GuardPair.Fallback.TexturePath));
	TestTrue(TEXT("reentrant completion applies the matching fallback texture"),
		Fixture.PermanentImage->GetBrush().GetResourceObject() == ExpectedFallbackTexture);
	TestTrue(TEXT("reentrant fallback application restores opacity"),
		FMath::IsNearlyEqual(Fixture.PermanentImage->GetRenderOpacity(), 1.0f));
	Fixture.Widget->TickForTest(0.01f);
	TestEqual(TEXT("a later update never retries synchronous preferred failure"),
		Loader->RequestCount(GuardPair.Preferred.TexturePath), 1);
	TestEqual(TEXT("a later update never loops the synchronous fallback"),
		Loader->RequestCount(GuardPair.Fallback.TexturePath), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingWorkbenchTravelPartyAtlasTimeoutFallbackTest,
	"GameXXK.DesktopTraining.Workbench.TravelPartyAtlasTimeoutFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingWorkbenchTravelPartyAtlasTimeoutFallbackTest::RunTest(
	const FString& Parameters)
{
	const FName GuardId(TEXT("CompanionInstance.Companion_Guard_01.TimeoutFallbackTest"));
	const FGameXXKBattleAnimationClipPair GuardPair =
		FGameXXKBattleAnimationPresentation::ResolveCompactTravelClipPair(
			GuardId, false, EGameXXKBattleAnimationAction::Idle);
	const TSharedRef<FTravelFallbackAtlasLoader> Loader = MakeShared<FTravelFallbackAtlasLoader>();
	TUniquePtr<FGameXXKBattleAtlasCache> Cache = MakeUnique<FGameXXKBattleAtlasCache>(
		Loader,
		[]() { return FPlatformTime::Seconds(); },
		FGameXXKBattleAtlasCache::DefaultResidentBudgetBytes,
		0.0);
	FTravelAtlasWidgetFixture Fixture;
	FString FixtureError;
	if (!TestTrue(TEXT("timeout fallback fixture opens"), BuildTravelAtlasWidgetFixture(
		GuardId,
		TEXT("Npc.TusiChief"),
		MoveTemp(Cache),
		Fixture,
		FixtureError)))
	{
		AddError(FixtureError);
		return false;
	}

	TestEqual(TEXT("timeout fixture starts with one preferred request"),
		Loader->RequestCount(GuardPair.Preferred.TexturePath), 1);
	TestEqual(TEXT("timeout fixture does not request fallback before advancing deadlines"),
		Loader->RequestCount(GuardPair.Fallback.TexturePath), 0);
	Fixture.Widget->TickForTest(0.001f);
	TestEqual(TEXT("cache timeout requests the fallback exactly once"),
		Loader->RequestCount(GuardPair.Fallback.TexturePath), 1);
	TestEqual(TEXT("timeout is distinct from a loader-completion failure"),
		Loader->CompletionDispatchCount(GuardPair.Preferred.TexturePath), 0);
	TestTrue(TEXT("timeout leaves the fallback loader callback pending"),
		Loader->HasPendingCompletion(GuardPair.Fallback.TexturePath));
	UTexture2D* FallbackTexture = Loader->CompleteLoaded(GuardPair.Fallback.TexturePath);
	TestNotNull(TEXT("timeout fixture completes the fallback texture"), FallbackTexture);
	TestTrue(TEXT("timeout fallback applies the matching 2K texture"),
		Fixture.PermanentImage->GetBrush().GetResourceObject() == FallbackTexture);
	TestTrue(TEXT("timeout fallback restores opacity"),
		FMath::IsNearlyEqual(Fixture.PermanentImage->GetRenderOpacity(), 1.0f));
	TestEqual(TEXT("timeout path never retries the preferred request"),
		Loader->RequestCount(GuardPair.Preferred.TexturePath), 1);
	TestEqual(TEXT("timeout path never loops the fallback request"),
		Loader->RequestCount(GuardPair.Fallback.TexturePath), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingWorkbenchTravelPartyAtlasSelectionRulesTest,
	"GameXXK.DesktopTraining.Workbench.TravelPartyAtlasSelectionRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingWorkbenchTravelPartyAtlasSelectionRulesTest::RunTest(
	const FString& Parameters)
{
	UGameXXKDesktopTrainingWorkbenchWidget* Widget =
		NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	UImage* Image = NewObject<UImage>(Widget);
	if (!TestNotNull(TEXT("selection-rules fixture widget exists"), Widget)
		|| !TestNotNull(TEXT("selection-rules fixture image exists"), Image))
	{
		return false;
	}

	const FGameXXKBattleAnimationClipPair Pair =
		FGameXXKBattleAnimationPresentation::ResolveCompactTravelClipPair(
			TEXT("CompanionInstance.Companion_Guard_01.SelectionRulesTest"),
			false,
			EGameXXKBattleAnimationAction::Attack);
	TStrongObjectPtr<UTexture2D> PreferredTexture(NewObject<UTexture2D>(GetTransientPackage()));
	TStrongObjectPtr<UTexture2D> FallbackTexture(NewObject<UTexture2D>(GetTransientPackage()));
	const auto LoadedTexturesMember = GetTravelPrivateMember(FTravelLoadedAtlasTexturesTag());
	TMap<FSoftObjectPath, TWeakObjectPtr<UTexture2D>>& LoadedTextures =
		Widget->*LoadedTexturesMember;
	LoadedTextures.Add(Pair.Fallback.TexturePath, FallbackTexture.Get());
	LoadedTextures.Add(Pair.Preferred.TexturePath, PreferredTexture.Get());
	TestEqual(TEXT("selection fixture makes both preferred and fallback atlases resident"),
		LoadedTextures.Num(), 2);

	FSlateBrush FallbackBrush;
	FallbackBrush.SetResourceObject(FallbackTexture.Get());
	Image->SetBrush(FallbackBrush);
	Image->SetRenderOpacity(1.0f);
	FSoftObjectPath AppliedPath = Pair.Fallback.TexturePath;
	int32 AppliedFrame = 7;
	const auto ApplyClipPair = GetTravelPrivateMember(FApplyTravelClipPairTag());
	TestTrue(TEXT("valid pair applies when both atlases are resident"),
		(Widget->*ApplyClipPair)(Image, Pair, false, AppliedPath, AppliedFrame));
	TestTrue(TEXT("preferred 1K wins when both atlases are resident"),
		Image->GetBrush().GetResourceObject() == PreferredTexture.Get());
	TestEqual(TEXT("preferred 1K path becomes the applied path"),
		AppliedPath, Pair.Preferred.TexturePath);
	TestTrue(TEXT("preferred 1K application keeps full opacity"),
		FMath::IsNearlyEqual(Image->GetRenderOpacity(), 1.0f));

	FGameXXKBattleAnimationClipPair InvalidPair;
	TestFalse(TEXT("an invalid pair cannot apply a frame"),
		(Widget->*ApplyClipPair)(Image, InvalidPair, false, AppliedPath, AppliedFrame));
	TestNull(TEXT("invalid pair clears the existing brush resource"),
		Image->GetBrush().GetResourceObject());
	TestTrue(TEXT("invalid pair hides the image"), FMath::IsNearlyZero(Image->GetRenderOpacity()));
	TestTrue(TEXT("invalid pair resets the applied path"), AppliedPath.IsNull());
	TestEqual(TEXT("invalid pair resets the applied frame"), AppliedFrame, INDEX_NONE);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingWorkbenchTravelVisualStripTest,
	"GameXXK.DesktopTraining.Workbench.TravelVisualStrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingWorkbenchTravelVisualStripTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	TestNotNull(TEXT("travel visual fixture subsystem exists"), Subsystem);
	if (!Subsystem || !Subsystem->StartGame())
	{
		return false;
	}

	const FName StageId = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 1);
	TestTrue(TEXT("travel visual fixture starts the cleared stage"), Subsystem->StartTrainingTravel(StageId));

	UGameXXKDesktopTrainingWorkbenchWidget* Widget = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	TestNotNull(TEXT("travel visual fixture widget exists"), Widget);
	if (!Widget)
	{
		return false;
	}
	Widget->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("travel visual fixture opens the workbench"), Widget->OpenWorkbench());
	TestTrue(TEXT("top strip creates a live travel visual surface"), Widget->HasTravelVisualStripForTest());
	TestTrue(TEXT("travel visual surface declares the generated walkloop atlas"),
		Widget->GetTravelVisualAtlasResourcePathForTest().Contains(TEXT("walkloop_pilot_v1")));
	TestTrue(TEXT("compact travel combat requests only 1K battle atlas siblings"),
		Widget->AreTravelCombatAtlasesOneKForTest());
	TestTrue(TEXT("travel visual surface declares the seamless background"),
		Widget->GetTravelVisualBackgroundResourcePathForTest().Contains(TEXT("TrainingIdleStrip_Background")));
	TestTrue(TEXT("travel visual background resolves from confirmed ImageTruth"),
		Widget->GetTravelVisualBackgroundResourcePathForTest().Contains(TEXT("/Game/GameXXK/UI/ImageTruth/Training/")));

	UBorder* TravelStrip = Widget->WidgetTree
		? Cast<UBorder>(Widget->WidgetTree->FindWidget(TEXT("TrainingTravelStrip")))
		: nullptr;
	TestNotNull(TEXT("travel visual strip owns a clipping container"), TravelStrip);
	if (TravelStrip)
	{
		TestTrue(
			TEXT("travel strip has no paper-panel backing behind the transparent scene"),
			TravelStrip->Background.DrawAs == ESlateBrushDrawType::NoDrawType);
		TestNull(
			TEXT("travel strip transparent container has no backing texture"),
			TravelStrip->Background.GetResourceObject());
	}

	TArray<UImage*> BackgroundTiles;
	for (int32 TileIndex = 0; TileIndex < 3; ++TileIndex)
	{
		UImage* Tile = Widget->WidgetTree
			? Cast<UImage>(Widget->WidgetTree->FindWidget(
				*FString::Printf(TEXT("TravelBackgroundTile_%d"), TileIndex)))
			: nullptr;
		TestNotNull(*FString::Printf(TEXT("seamless background tile %d exists"), TileIndex), Tile);
		BackgroundTiles.Add(Tile);
	}
	if (BackgroundTiles.Num() == 3
		&& BackgroundTiles[0]
		&& BackgroundTiles[1]
		&& BackgroundTiles[2])
	{
		const UCanvasPanelSlot* LeftSlot = Cast<UCanvasPanelSlot>(BackgroundTiles[0]->Slot);
		const UCanvasPanelSlot* CenterSlot = Cast<UCanvasPanelSlot>(BackgroundTiles[1]->Slot);
		const UCanvasPanelSlot* RightSlot = Cast<UCanvasPanelSlot>(BackgroundTiles[2]->Slot);
		TestNotNull(TEXT("left seamless tile uses canvas geometry"), LeftSlot);
		TestNotNull(TEXT("center seamless tile uses canvas geometry"), CenterSlot);
		TestNotNull(TEXT("right seamless tile uses canvas geometry"), RightSlot);
		if (LeftSlot && CenterSlot && RightSlot)
		{
			const FVector2D TileSize = CenterSlot->GetSize();
			TestTrue(
				TEXT("background is enlarged enough for its opaque road to continue below the character feet"),
				TileSize.Y >= 290.0f);
			TestTrue(
				TEXT("background enlargement preserves the authored two-point-five-to-one aspect"),
				FMath::IsNearlyEqual(TileSize.X / TileSize.Y, 2.5f, 0.001f));
			TestTrue(
				TEXT("background Y keeps the authored yellow road line on the character foot plane"),
				FMath::Abs(CenterSlot->GetPosition().Y) <= 8.0f);
			TestEqual(TEXT("rightward loop starts with one tile left of the viewport"), LeftSlot->GetPosition().X, -TileSize.X);
			TestEqual(TEXT("rightward loop keeps the middle tile at the origin"), CenterSlot->GetPosition().X, 0.0);
			TestEqual(TEXT("rightward loop keeps one tile to the right"), RightSlot->GetPosition().X, TileSize.X);
		}
	}

	Widget->TickForTest(0.5f);
	TestTrue(TEXT("travel strip moves while the runner is walking"), Widget->GetTravelVisualScrollOffsetForTest() > 0.0f);
	TestEqual(TEXT("travel strip displays the generated 12 fps walkloop frame"), Widget->GetTravelVisualWalkFrameForTest(), 6);
	for (const UImage* Tile : BackgroundTiles)
	{
		if (Tile)
		{
			TestTrue(
				TEXT("left-walking hero drives the seamless scene to the right"),
				Tile->GetRenderTransform().Translation.X > 0.0f);
		}
	}

	Widget->TickForTest(0.5f);
	TestTrue(TEXT("travel strip keeps the same visual runtime across deferred layout refresh"),
		Widget->GetTravelVisualScrollOffsetForTest() >= 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingWorkbenchTravelVisualLoopTest,
	"GameXXK.DesktopTraining.Workbench.TravelVisualLoop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingWorkbenchTravelVisualLoopTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	TestNotNull(TEXT("travel visual loop fixture subsystem exists"), Subsystem);
	if (!Subsystem || !Subsystem->StartGame())
	{
		return false;
	}

	const FName StageId = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 1);
	TestTrue(TEXT("travel visual loop fixture starts the cleared stage"), Subsystem->StartTrainingTravel(StageId));
	UGameXXKDesktopTrainingWorkbenchWidget* Widget = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	TestNotNull(TEXT("travel visual loop fixture widget exists"), Widget);
	if (!Widget)
	{
		return false;
	}
	Widget->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("travel visual loop fixture opens the workbench"), Widget->OpenWorkbench());

	// Advance until the authored route reports a completed loop. The guard is a
	// hang detector; it is deliberately not an assertion about combat duration.
	for (int32 Guard = 0;
		Guard < 512 && Widget->GetTravelVisualCompletedLoopCountForTest() < 1;
		++Guard)
	{
		Widget->TickForTest(1.0f);
	}
	TestEqual(TEXT("one completed travel route increments the visual loop count"),
		Widget->GetTravelVisualCompletedLoopCountForTest(), 1);
	TestEqual(TEXT("the travel runner returns to the same 1-1 stage after its loop"),
		Subsystem->GetTrainingProgressCopy().CurrentTravelStageId, StageId);
	TestTrue(TEXT("the next encounter is walking after the visual loop reset"),
		Subsystem->GetTrainingTravelRuntimeCopy().Phase == EGameXXKTrainingTravelPhase::Walking);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingWorkbenchTravelCombatPresentationTest,
	"GameXXK.DesktopTraining.Workbench.TravelCombatPresentation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingWorkbenchTravelCombatPresentationTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	TestNotNull(TEXT("travel combat fixture subsystem exists"), Subsystem);
	if (!Subsystem || !Subsystem->StartGame())
	{
		return false;
	}

	FGameXXKRuntimeState& PartyState = Subsystem->GetMutableRuntimeState();
	const FGameXXKPermanentCompanion* ActiveCompanion = PartyState.CardRun.CompanionRoster.PermanentCompanions.FindByPredicate(
		[](const FGameXXKPermanentCompanion& Candidate)
		{
			return Candidate.bIsActive && !Candidate.InstanceId.IsNone();
		});
	TestNotNull(TEXT("travel combat fixture has an active permanent companion"), ActiveCompanion);
	if (!ActiveCompanion)
	{
		return false;
	}
	FString FormationError;
	TestTrue(TEXT("travel combat fixture selects Yue Bai through ordered formation"),
		GameXXKPermanentPartyTestFixtures::SelectNpc(
			PartyState,
			TEXT("Npc.YueBai"),
			&FormationError));

	const FName StageId = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 1);
	TestTrue(TEXT("travel combat fixture starts cleared 1-1"), Subsystem->StartTrainingTravel(StageId));
	const FName FirstEnemyId = Subsystem->GetTrainingTravelRuntimeCopy().EnemyDefinitionId;
	TestFalse(TEXT("travel combat fixture has an authored first enemy"), FirstEnemyId.IsNone());

	UGameXXKDesktopTrainingWorkbenchWidget* Widget = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	TestNotNull(TEXT("travel combat fixture widget exists"), Widget);
	if (!Widget)
	{
		return false;
	}
	Widget->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("travel combat fixture opens the workbench"), Widget->OpenWorkbench());

	UImage* EnemyImage = Widget->WidgetTree
		? Cast<UImage>(Widget->WidgetTree->FindWidget(TEXT("TravelEnemyAnimatedUnit_0")))
		: nullptr;
	UImage* SecondEnemyImage = Widget->WidgetTree
		? Cast<UImage>(Widget->WidgetTree->FindWidget(TEXT("TravelEnemyAnimatedUnit_1")))
		: nullptr;
	UImage* ThirdEnemyImage = Widget->WidgetTree
		? Cast<UImage>(Widget->WidgetTree->FindWidget(TEXT("TravelEnemyAnimatedUnit_2")))
		: nullptr;
	UImage* HeroImage = Widget->WidgetTree
		? Cast<UImage>(Widget->WidgetTree->FindWidget(TEXT("TravelHeroAnimatedUnit")))
		: nullptr;
	UImage* PermanentCompanionImage = Widget->WidgetTree
		? Cast<UImage>(Widget->WidgetTree->FindWidget(TEXT("TravelCompanionAnimatedUnit_0")))
		: nullptr;
	UImage* QuestCompanionImage = Widget->WidgetTree
		? Cast<UImage>(Widget->WidgetTree->FindWidget(TEXT("TravelCompanionAnimatedUnit_1")))
		: nullptr;
	UBorder* EnemyHealth = Widget->WidgetTree
		? Cast<UBorder>(Widget->WidgetTree->FindWidget(TEXT("TravelEnemyHealth_0")))
		: nullptr;
	UBorder* SecondEnemyHealth = Widget->WidgetTree
		? Cast<UBorder>(Widget->WidgetTree->FindWidget(TEXT("TravelEnemyHealth_1")))
		: nullptr;
	UBorder* ThirdEnemyHealth = Widget->WidgetTree
		? Cast<UBorder>(Widget->WidgetTree->FindWidget(TEXT("TravelEnemyHealth_2")))
		: nullptr;
	UBorder* EnemyHealthTrack = Widget->WidgetTree
		? Cast<UBorder>(Widget->WidgetTree->FindWidget(TEXT("TravelEnemyHealthTrack_0")))
		: nullptr;
	UBorder* SecondEnemyHealthTrack = Widget->WidgetTree
		? Cast<UBorder>(Widget->WidgetTree->FindWidget(TEXT("TravelEnemyHealthTrack_1")))
		: nullptr;
	UBorder* ThirdEnemyHealthTrack = Widget->WidgetTree
		? Cast<UBorder>(Widget->WidgetTree->FindWidget(TEXT("TravelEnemyHealthTrack_2")))
		: nullptr;
	UProgressBar* HeroHealth = Widget->WidgetTree
		? Cast<UProgressBar>(Widget->WidgetTree->FindWidget(TEXT("TravelHeroHealth")))
		: nullptr;
	UProgressBar* PermanentCompanionHealth = Widget->WidgetTree
		? Cast<UProgressBar>(Widget->WidgetTree->FindWidget(TEXT("TravelCompanionHealth_0")))
		: nullptr;
	UProgressBar* QuestCompanionHealth = Widget->WidgetTree
		? Cast<UProgressBar>(Widget->WidgetTree->FindWidget(TEXT("TravelCompanionHealth_1")))
		: nullptr;
	TestNotNull(TEXT("top strip owns a real animated enemy image"), EnemyImage);
	TestNotNull(TEXT("top strip owns the second enemy formation slot"), SecondEnemyImage);
	TestNotNull(TEXT("top strip owns the third enemy formation slot"), ThirdEnemyImage);
	TestNotNull(TEXT("top strip owns a real animated hero image"), HeroImage);
	TestNotNull(TEXT("top strip owns the selected permanent companion image"), PermanentCompanionImage);
	TestNotNull(TEXT("top strip owns the selected quest companion image"), QuestCompanionImage);
	TestNotNull(TEXT("top strip owns the enemy HP bar"), EnemyHealth);
	TestNotNull(TEXT("top strip owns the second enemy HP bar"), SecondEnemyHealth);
	TestNotNull(TEXT("top strip owns the third enemy HP bar"), ThirdEnemyHealth);
	TestNotNull(TEXT("top strip owns the first enemy HP track"), EnemyHealthTrack);
	TestNotNull(TEXT("top strip owns the second enemy HP track"), SecondEnemyHealthTrack);
	TestNotNull(TEXT("top strip owns the third enemy HP track"), ThirdEnemyHealthTrack);
	TestNotNull(TEXT("top strip owns the hero HP bar"), HeroHealth);
	TestNotNull(TEXT("top strip owns the permanent companion HP bar"), PermanentCompanionHealth);
	TestNotNull(TEXT("top strip owns the quest companion HP bar"), QuestCompanionHealth);
	const auto TestDynamicBarVolatile = [this](const TCHAR* Label, UWidget* Bar)
	{
		if (!TestNotNull(Label, Bar))
		{
			return;
		}
		const FBoolProperty* VolatileProperty = CastField<FBoolProperty>(
			UWidget::StaticClass()->FindPropertyByName(TEXT("bIsVolatile")));
		if (TestNotNull(TEXT("UWidget exposes its volatility flag"), VolatileProperty))
		{
			TestTrue(*FString::Printf(TEXT("%s is volatile for per-frame HP paint"), Label),
				VolatileProperty->GetPropertyValue_InContainer(Bar));
		}
	};
	TestDynamicBarVolatile(TEXT("first enemy HP bar"), EnemyHealth);
	TestDynamicBarVolatile(TEXT("second enemy HP bar"), SecondEnemyHealth);
	TestDynamicBarVolatile(TEXT("third enemy HP bar"), ThirdEnemyHealth);
	TestDynamicBarVolatile(TEXT("hero HP bar"), HeroHealth);
	TestDynamicBarVolatile(TEXT("permanent companion HP bar"), PermanentCompanionHealth);
	TestDynamicBarVolatile(TEXT("quest companion HP bar"), QuestCompanionHealth);
	TestNull(TEXT("travel strip has no harvest/collect button"), Widget->WidgetTree
		? Widget->WidgetTree->FindWidget(TEXT("TravelCollectButton"))
		: nullptr);
	TestEqual(TEXT("953 px lane uses three overlapping seamless tiles"), Widget->GetTravelBackgroundTileCountForTest(), 3);
	TestEqual(TEXT("player-facing strip removes verbose diagnostic text"), Widget->GetTravelVerboseTextBlockCountForTest(), 0);
	if (HeroImage)
	{
		TestEqual(
			TEXT("walking hero keeps a bottom-center ground anchor"),
			HeroImage->GetRenderTransformPivot(),
			FVector2D(0.5f, 1.0f));
		TestTrue(
			TEXT("walking hero uses the authored left-facing atlas without mirroring"),
			HeroImage->GetRenderTransform().Scale.Equals(FVector2D(1.0f, 1.0f), 0.001f));
	}
	if (EnemyImage)
	{
		TestEqual(
			TEXT("enemy keeps the same bottom-center ground anchor as the battle board"),
			EnemyImage->GetRenderTransformPivot(),
			FVector2D(0.5f, 1.0f));
	}
	if (PermanentCompanionImage && QuestCompanionImage && PermanentCompanionHealth && QuestCompanionHealth)
	{
		TestEqual(
			TEXT("permanent companion shares the party ground anchor"),
			PermanentCompanionImage->GetRenderTransformPivot(),
			FVector2D(0.5f, 1.0f));
		TestEqual(
			TEXT("quest companion shares the party ground anchor"),
			QuestCompanionImage->GetRenderTransformPivot(),
			FVector2D(0.5f, 1.0f));
		TestEqual(
			TEXT("companions stay hidden during the walk approach"),
			PermanentCompanionImage->GetVisibility(),
			ESlateVisibility::Collapsed);
		TestEqual(
			TEXT("quest companion stays hidden during the walk approach"),
			QuestCompanionImage->GetVisibility(),
			ESlateVisibility::Collapsed);
	}

	for (int32 WalkingSecond = 1;
		WalkingSecond < FGameXXKTrainingRules::TravelEncounterSpawnDelaySeconds;
		++WalkingSecond)
	{
		Widget->TickForTest(1.0f);
		TestEqual(
			TEXT("the first four seconds keep the travel presentation walking"),
			Widget->GetTravelVisualPhaseForTest(),
			EGameXXKTrainingTravelVisualPhase::Walking);
		if (EnemyImage && SecondEnemyImage && ThirdEnemyImage
			&& EnemyHealth && SecondEnemyHealth && ThirdEnemyHealth)
		{
			TestEqual(TEXT("walking hides the first enemy slot"), EnemyImage->GetVisibility(), ESlateVisibility::Collapsed);
			TestEqual(TEXT("walking hides the second enemy slot"), SecondEnemyImage->GetVisibility(), ESlateVisibility::Collapsed);
			TestEqual(TEXT("walking hides the third enemy slot"), ThirdEnemyImage->GetVisibility(), ESlateVisibility::Collapsed);
			TestEqual(TEXT("walking keeps the first enemy HP bar in the Slate tree"), EnemyHealth->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
			TestEqual(TEXT("walking keeps the second enemy HP bar in the Slate tree"), SecondEnemyHealth->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
			TestEqual(TEXT("walking keeps the third enemy HP bar in the Slate tree"), ThirdEnemyHealth->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
			TestTrue(TEXT("walking visually hides the first enemy HP bar"), FMath::IsNearlyZero(EnemyHealth->GetRenderOpacity()));
			TestTrue(TEXT("walking visually hides the second enemy HP bar"), FMath::IsNearlyZero(SecondEnemyHealth->GetRenderOpacity()));
			TestTrue(TEXT("walking visually hides the third enemy HP bar"), FMath::IsNearlyZero(ThirdEnemyHealth->GetRenderOpacity()));
			TestTrue(TEXT("walking clears the first enemy HP bar before reuse"),
				FMath::IsNearlyZero(Widget->GetTravelEnemyHealthBarPercentForTest(0)));
			TestTrue(TEXT("walking clears the second enemy HP bar before reuse"),
				FMath::IsNearlyZero(Widget->GetTravelEnemyHealthBarPercentForTest(1)));
			TestTrue(TEXT("walking clears the third enemy HP bar before reuse"),
				FMath::IsNearlyZero(Widget->GetTravelEnemyHealthBarPercentForTest(2)));
		}
	}
	Widget->TickForTest(1.0f);
	TestEqual(
		TEXT("standing at an encounter switches to idle presentation"),
		Widget->GetTravelVisualPhaseForTest(),
		EGameXXKTrainingTravelVisualPhase::EncounterIdle);
	TestTrue(TEXT("the authored enemy is visible while standing"), Widget->IsTravelVisualEnemyVisibleForTest());
	if (EnemyImage && SecondEnemyImage && ThirdEnemyImage)
	{
		TestEqual(TEXT("ordinary encounter shows its first enemy slot"), EnemyImage->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
		TestEqual(TEXT("ordinary encounter shows its second enemy slot"), SecondEnemyImage->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
		TestEqual(TEXT("ordinary encounter leaves its third enemy slot empty"), ThirdEnemyImage->GetVisibility(), ESlateVisibility::Collapsed);
	}
	if (EnemyHealth && SecondEnemyHealth && ThirdEnemyHealth)
	{
		TestTrue(TEXT("the first enemy HP bar becomes visible at spawn"), FMath::IsNearlyEqual(EnemyHealth->GetRenderOpacity(), 1.0f));
		TestTrue(TEXT("the second enemy HP bar becomes visible at spawn"), FMath::IsNearlyEqual(SecondEnemyHealth->GetRenderOpacity(), 1.0f));
		TestTrue(TEXT("the unused third enemy HP bar remains visually hidden"), FMath::IsNearlyZero(ThirdEnemyHealth->GetRenderOpacity()));
	}
	TestEqual(TEXT("encounter idle retains the first authored enemy"), Widget->GetTravelVisualEnemyDefinitionIdForTest(), FirstEnemyId);
	TestEqual(TEXT("standing hero uses the battle idle action"), Widget->GetTravelVisualHeroActionForTest(), EGameXXKBattleAnimationAction::Idle);
	TestEqual(TEXT("PIE probe exposes the visual idle phase by name"), Widget->GetTravelVisualPhaseNameForTest(), FString(TEXT("EncounterIdle")));
	TestEqual(TEXT("PIE probe exposes the hero idle action by name"), Widget->GetTravelVisualHeroActionNameForTest(), FString(TEXT("Idle")));
	TestEqual(TEXT("Blade is independently idle at encounter entry"), Widget->GetTravelVisualPartyActionNameForTest(1), FString(TEXT("Idle")));
	TestEqual(TEXT("Tusi Chief is independently idle at encounter entry"), Widget->GetTravelVisualPartyActionNameForTest(2), FString(TEXT("Idle")));
	TestTrue(TEXT("Blade exposes a real normalized HP fraction"), Widget->GetTravelVisualPartyHealthFractionForTest(1) > 0.0f);
	TestTrue(TEXT("Tusi Chief exposes a real normalized HP fraction"), Widget->GetTravelVisualPartyHealthFractionForTest(2) > 0.0f);
	TestEqual(TEXT("PIE probe exposes the enemy idle action by name"), Widget->GetTravelVisualEnemyActionNameForTest(), FString(TEXT("Idle")));
	TestTrue(TEXT("PIE probe exposes a normalized hero HP fraction"), Widget->GetTravelVisualHeroHealthFractionForTest() > 0.0f);
	TestTrue(TEXT("PIE probe exposes a normalized enemy HP fraction"), Widget->GetTravelVisualEnemyHealthFractionForTest() > 0.0f);
	TestTrue(TEXT("the first enemy bar starts the encounter full"),
		FMath::IsNearlyEqual(Widget->GetTravelEnemyHealthBarPercentForTest(0), 1.0f, 0.001f));
	TestTrue(TEXT("the hero bar starts the encounter full"),
		FMath::IsNearlyEqual(Widget->GetTravelHeroHealthBarPercentForTest(), 1.0f, 0.001f));
	TestTrue(TEXT("the permanent companion bar starts the encounter full"),
		FMath::IsNearlyEqual(Widget->GetTravelCompanionHealthBarPercentForTest(0), 1.0f, 0.001f));
	TestTrue(TEXT("the quest companion bar starts the encounter full"),
		FMath::IsNearlyEqual(Widget->GetTravelCompanionHealthBarPercentForTest(1), 1.0f, 0.001f));
	TestTrue(
		TEXT("the visible hero HP bar receives the visual HP fraction"),
		FMath::IsNearlyEqual(
			Widget->GetTravelHeroHealthBarPercentForTest(),
			Widget->GetTravelVisualHeroHealthFractionForTest(),
			0.001f));
	TestTrue(
		TEXT("the visible permanent-companion HP bar receives its visual HP fraction"),
		FMath::IsNearlyEqual(
			Widget->GetTravelCompanionHealthBarPercentForTest(0),
			Widget->GetTravelVisualPartyHealthFractionForTest(1),
			0.001f));
	TestTrue(
		TEXT("the visible quest-companion HP bar receives its visual HP fraction"),
		FMath::IsNearlyEqual(
			Widget->GetTravelCompanionHealthBarPercentForTest(1),
			Widget->GetTravelVisualPartyHealthFractionForTest(2),
			0.001f));
	TestTrue(
		TEXT("the visible enemy HP bar receives its visual HP fraction"),
		FMath::IsNearlyEqual(
			Widget->GetTravelEnemyHealthBarPercentForTest(0),
			Widget->GetTravelVisualEnemyHealthFractionForTest(),
			0.001f));
	if (HeroImage)
	{
		const FVector2D IdleScale = HeroImage->GetRenderTransform().Scale;
		TestTrue(
			TEXT("battle idle preserves the production atlas facing instead of reversing the hero"),
			IdleScale.X > 0.0f);
		TestTrue(
			TEXT("battle idle uses a uniform content normalization scale"),
			FMath::IsNearlyEqual(IdleScale.X, IdleScale.Y, 0.001f));
		TestTrue(
			TEXT("battle idle normalizes its 81.2 percent alpha height to the 90.6 percent walk height"),
			FMath::IsNearlyEqual(IdleScale.Y, 1.116f, 0.01f));
	}
	if (EnemyImage)
	{
		const FVector2D EnemyIdleScale = EnemyImage->GetRenderTransform().Scale;
		TestTrue(
			TEXT("enemy idle preserves the battle-board authored facing toward the hero"),
			EnemyIdleScale.X > 0.0f);
		TestTrue(
			TEXT("enemy idle uses a uniform content-normalization scale"),
			FMath::IsNearlyEqual(EnemyIdleScale.X, EnemyIdleScale.Y, 0.001f));
		TestTrue(
			TEXT("enemy idle is enlarged to approximately the normalized hero height"),
			EnemyIdleScale.Y >= 1.11f);
	}
	if (PermanentCompanionImage && QuestCompanionImage && PermanentCompanionHealth && QuestCompanionHealth)
	{
		TestEqual(
			TEXT("selected permanent companion joins the encounter"),
			PermanentCompanionImage->GetVisibility(),
			ESlateVisibility::SelfHitTestInvisible);
		TestEqual(
			TEXT("selected quest companion joins the encounter"),
			QuestCompanionImage->GetVisibility(),
			ESlateVisibility::SelfHitTestInvisible);
		TestTrue(
			TEXT("permanent companion uses a positive uniform authored-facing scale"),
			PermanentCompanionImage->GetRenderTransform().Scale.X > 0.0f
			&& FMath::IsNearlyEqual(
				PermanentCompanionImage->GetRenderTransform().Scale.X,
				PermanentCompanionImage->GetRenderTransform().Scale.Y,
				0.001f));
		TestEqual(TEXT("selected permanent companion health bar joins the encounter"), PermanentCompanionHealth->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
		TestEqual(TEXT("selected quest companion health bar joins the encounter"), QuestCompanionHealth->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
	}

	Widget->TickForTest(1.0f);
	TestEqual(
		TEXT("the logical kill begins a real-time hero attack instead of jumping to walk"),
		Widget->GetTravelVisualPhaseForTest(),
		EGameXXKTrainingTravelVisualPhase::HeroAttack);
	TestEqual(
		TEXT("the presentation retains the defeated enemy after the gameplay runner advances"),
		Widget->GetTravelVisualEnemyDefinitionIdForTest(),
		FirstEnemyId);
	TestEqual(TEXT("PIE probe distinguishes the visual attack from the advanced logical runner"), Widget->GetTravelVisualPhaseNameForTest(), FString(TEXT("HeroAttack")));
	TestEqual(TEXT("PIE probe exposes the hero attack action by name"), Widget->GetTravelVisualHeroActionNameForTest(), FString(TEXT("Attack")));
	TestEqual(TEXT("Blade stays idle during the hero action"), Widget->GetTravelVisualPartyActionNameForTest(1), FString(TEXT("Idle")));
	if (HeroImage)
	{
		const FVector2D AttackScale = HeroImage->GetRenderTransform().Scale;
		TestTrue(
			TEXT("hero attack keeps its authored left-facing direction"),
			AttackScale.X > 0.0f);
		TestTrue(
			TEXT("hero attack normalizes its 59.8 percent alpha height to the 90.6 percent walk height"),
			FMath::IsNearlyEqual(AttackScale.Y, 1.516f, 0.01f));
	}
	Widget->TickForTest(0.5f);
	Widget->TickForTest(0.5f);
	TestEqual(TEXT("the next logical action belongs to Blade"), Widget->GetTravelVisualPartyActionNameForTest(1), FString(TEXT("Attack")));
	TestEqual(TEXT("hero returns idle during Blade's action"), Widget->GetTravelVisualPartyActionNameForTest(0), FString(TEXT("Idle")));

	if (EnemyImage)
	{
		const float EnemyIdleScale = EnemyImage->GetRenderTransform().Scale.Y;
		Widget->TickForTest(FGameXXKTrainingTravelVisualRuntime::HeroAttackSeconds);
		TestEqual(
			TEXT("hero attack advances into the enemy hit presentation"),
			Widget->GetTravelVisualPhaseForTest(),
			EGameXXKTrainingTravelVisualPhase::EnemyHit);
		const FVector2D EnemyHitScale = EnemyImage->GetRenderTransform().Scale;
		TestTrue(
			TEXT("enemy hit keeps the authored direction and uniform scale"),
			EnemyHitScale.X > 0.0f
			&& FMath::IsNearlyEqual(EnemyHitScale.X, EnemyHitScale.Y, 0.001f));
		TestTrue(
			TEXT("enemy hit compensates its tighter transparent bounds instead of shrinking"),
			EnemyHitScale.Y > EnemyIdleScale);

		for (int32 SettlementGuard = 0;
			SettlementGuard < 128
				&& Subsystem->GetTrainingTravelRuntimeCopy().Phase != EGameXXKTrainingTravelPhase::Walking;
			++SettlementGuard)
		{
			TestTrue(TEXT("the fixture advances to the next encounter boundary"),
				Widget->AdvanceTravelForTest(1));
		}
		if (!TestEqual(
			TEXT("the completed formation owns the next logical Walking interval"),
			Subsystem->GetTrainingTravelRuntimeCopy().Phase,
			EGameXXKTrainingTravelPhase::Walking))
		{
			return false;
		}
		for (int32 PresentationGuard = 0;
			PresentationGuard < 256
				&& Widget->GetTravelVisualPhaseForTest() != EGameXXKTrainingTravelVisualPhase::Walking;
			++PresentationGuard)
		{
			Widget->TickForTest(0.05f);
			if (!TestEqual(
				TEXT("queued combat presentation never consumes the next spawn delay"),
				Subsystem->GetTrainingTravelRuntimeCopy().WalkStep,
				0))
			{
				return false;
			}
		}
		TestEqual(
			TEXT("the defeated formation finishes before the next walkloop starts"),
			Widget->GetTravelVisualPhaseForTest(),
			EGameXXKTrainingTravelVisualPhase::Walking);

		Widget->TickForTest(0.25f);
		TestEqual(
			TEXT("death presentation time never consumes the next spawn delay"),
			Subsystem->GetTrainingTravelRuntimeCopy().WalkStep,
			0);
		for (int32 VisibleWalkingSecond = 1;
			VisibleWalkingSecond < FGameXXKTrainingRules::TravelEncounterSpawnDelaySeconds;
			++VisibleWalkingSecond)
		{
			Widget->TickForTest(1.0f);
			TestEqual(
				TEXT("four complete visible walkloop seconds keep enemies hidden"),
				Widget->GetTravelVisualPhaseForTest(),
				EGameXXKTrainingTravelVisualPhase::Walking);
		}
		Widget->TickForTest(0.74f);
		TestEqual(
			TEXT("the next formation remains hidden immediately before five visible seconds"),
			Widget->GetTravelVisualPhaseForTest(),
			EGameXXKTrainingTravelVisualPhase::Walking);
		Widget->TickForTest(0.02f);
		TestEqual(
			TEXT("the next formation appears only after five visible walkloop seconds"),
			Widget->GetTravelVisualPhaseForTest(),
			EGameXXKTrainingTravelVisualPhase::EncounterIdle);
		TestTrue(TEXT("the next wave hero bar respawns full"),
			FMath::IsNearlyEqual(Widget->GetTravelHeroHealthBarPercentForTest(), 1.0f, 0.001f));
		TestTrue(TEXT("the next wave enemy bar respawns full"),
			FMath::IsNearlyEqual(Widget->GetTravelEnemyHealthBarPercentForTest(0), 1.0f, 0.001f));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingWorkbenchFormationNpcPortraitTest,
	"GameXXK.DesktopTraining.Workbench.FormationNpcPortraitPermanent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingWorkbenchFormationNpcPortraitTest::RunTest(
	const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem =
		NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestTrue(TEXT("formation NPC portrait fixture starts"),
		Subsystem && Subsystem->StartGame()))
	{
		return false;
	}
	FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
	State.Screen = EGameXXKScreen::Town;
	State.CardRun.ActiveTemporaryQuestNpcId = NAME_None;
	FName ExpectedNpcId;
	TestTrue(TEXT("fixture resolves an authoritative NPC"),
		FGameXXKPartyFormationRules::ResolveQuestNpcId(State, ExpectedNpcId));

	UGameXXKDesktopTrainingWorkbenchWidget* Widget =
		NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	Widget->SetMVPSubsystem(Subsystem);
	Widget->ConstructForTest();
	if (!TestTrue(TEXT("permanent NPC fixture opens workbench"), Widget->OpenWorkbench())
		|| !TestTrue(TEXT("permanent NPC fixture opens Backpack"), Widget->OpenBackpack()))
	{
		return false;
	}
	Widget->HandleActionClicked(1);
	UImage* NpcPortrait = Widget->WidgetTree
		? Cast<UImage>(Widget->WidgetTree->FindWidget(TEXT("FormationCurrentPortrait_2")))
		: nullptr;
	TestNotNull(TEXT("permanent NPC portrait exists"), NpcPortrait);
	TestEqual(TEXT("permanent NPC portrait remains visible"),
		NpcPortrait ? NpcPortrait->GetVisibility() : ESlateVisibility::Collapsed,
		ESlateVisibility::HitTestInvisible);
	TestFalse(TEXT("permanent NPC portrait has a real texture"),
		GetImageResourcePath(NpcPortrait).IsEmpty());
	TestTrue(TEXT("permanent NPC portrait matches the resolved identity"),
		ExpectedNpcId != TEXT("Npc.TusiChief")
			|| GetImageResourcePath(NpcPortrait).Contains(TEXT("T_CardPortrait_Npc_TusiChief")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingTransparentClearDrawContractTest,
	"GameXXK.DesktopTraining.Workbench.TransparentClearDrawContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingTransparentClearDrawContractTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("the independent desktop window owns a persistent clear layer outside the rebuilt workbench"),
		AGameXXKMVPPlayerController::ShouldBuildPersistentDesktopClearHostForTest(true));
	IGameXXKDesktopOverlayModule& Overlay = IGameXXKDesktopOverlayModule::Get();
	TestTrue(TEXT("the transparent clear uses an explicit render-target clear before Slate content"),
		Overlay.UsesRenderTargetTransparentClearForTest());
	TestEqual(TEXT("the retained surface is reset to premultiplied transparent black"),
		Overlay.GetRenderTargetTransparentClearColorForTest(),
		FLinearColor::Transparent);
	TestFalse(TEXT("a non-composition window does not add the persistent clear layer"),
		AGameXXKMVPPlayerController::ShouldBuildPersistentDesktopClearHostForTest(false));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingWorkbenchCharacterRosterTest,
	"GameXXK.DesktopTraining.Workbench.CharacterRosterPlacementAndViewIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingWorkbenchCharacterRosterTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	if (!TestTrue(TEXT("character-roster fixture starts a new game"), Subsystem && Subsystem->StartGame()))
	{
		return false;
	}
	UGameXXKDesktopTrainingWorkbenchWidget* Widget = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	Widget->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("character-roster fixture opens the workbench"), Widget->OpenWorkbench());
	TestTrue(TEXT("character-roster fixture expands the backpack"), Widget->OpenBackpack());
	TestNotNull(TEXT("character page exposes the hero roster tab"),
		Widget->WidgetTree ? Widget->WidgetTree->FindWidget(TEXT("CharacterRosterHeroButton")) : nullptr);
	TestNotNull(TEXT("character page exposes the partner roster tab"),
		Widget->WidgetTree ? Widget->WidgetTree->FindWidget(TEXT("CharacterRosterCompanionButton")) : nullptr);
	TestNotNull(TEXT("character page exposes the NPC roster tab"),
		Widget->WidgetTree ? Widget->WidgetTree->FindWidget(TEXT("CharacterRosterNpcButton")) : nullptr);
	const FName RosterButtonNames[] = {
		TEXT("CharacterRosterHeroButton"),
		TEXT("CharacterRosterCompanionButton"),
		TEXT("CharacterRosterNpcButton")};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(RosterButtonNames); ++Index)
	{
		UWidget* Button = Widget->WidgetTree ? Widget->WidgetTree->FindWidget(RosterButtonNames[Index]) : nullptr;
		const UButton* RosterButton = Cast<UButton>(Button);
		TestTrue(
			*FString::Printf(TEXT("collapsed roster button %d uses the approved normal state"), Index),
			GetButtonNormalResourcePath(RosterButton).Contains(TEXT("003_tab_1")));
		const UCanvasPanelSlot* Slot = Button ? Cast<UCanvasPanelSlot>(Button->Slot) : nullptr;
		if (TestNotNull(*FString::Printf(TEXT("roster button %d is placed on the reference canvas"), Index), Slot))
		{
			TestEqual(
				*FString::Printf(TEXT("roster button %d is fixed to the lower-left row"), Index),
				Slot->GetPosition(),
				FVector2D(414.0f + Index * 113.0f, 706.0f));
			TestEqual(
				*FString::Printf(TEXT("roster button %d keeps the compact portrait size"), Index),
				Slot->GetSize(),
				FVector2D(105.0f, 62.0f));
			TestTrue(
				*FString::Printf(TEXT("roster button %d remains above bottom navigation"), Index),
				Slot->GetPosition().Y + Slot->GetSize().Y < GameXXKDesktopTrainingLayout::GetNavigationRect().Y);
		}
	}
	const UImage* HeroRepresentative = Cast<UImage>(
		Widget->WidgetTree ? Widget->WidgetTree->FindWidget(TEXT("CharacterRosterRepresentativePortrait_0")) : nullptr);
	const UImage* CompanionRepresentative = Cast<UImage>(
		Widget->WidgetTree ? Widget->WidgetTree->FindWidget(TEXT("CharacterRosterRepresentativePortrait_1")) : nullptr);
	const UImage* NpcRepresentative = Cast<UImage>(
		Widget->WidgetTree ? Widget->WidgetTree->FindWidget(TEXT("CharacterRosterRepresentativePortrait_2")) : nullptr);
	TestEqual(
		TEXT("hero roster entry uses the real hero portrait"),
		GetImageResourcePath(HeroRepresentative),
		FString(TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Hero.T_CardPortrait_Hero")));
	TestTrue(
		TEXT("partner roster entry uses a real profession portrait"),
		GetImageResourcePath(CompanionRepresentative).Contains(TEXT("/T_CardPortrait_Role_")));
	TestTrue(
		TEXT("NPC roster entry uses a real named-NPC portrait"),
		GetImageResourcePath(NpcRepresentative).Contains(TEXT("/T_CardPortrait_Npc_")));

	const TArray<FName> CompanionIds = Widget->GetCompanionCharacterIdsForTest();
	const TArray<FName> NpcIds = Widget->GetNpcCharacterIdsForTest();
	TestEqual(TEXT("partner roster exposes one owned member per profession"), CompanionIds.Num(), 6);
	TestEqual(TEXT("NPC roster exposes all six owned named NPCs"), NpcIds.Num(), 6);
	if (CompanionIds.Num() != 6 || NpcIds.Num() != 6)
	{
		return false;
	}

	const FName InitialCompanionId =
		Subsystem->GetRuntimeState().CardRun.PartySelection.ActivePermanentCompanionInstanceId;
	FName InitialNpcId;
	TestTrue(TEXT("initial permanent NPC resolves from ordered formation"),
		FGameXXKPartyFormationRules::ResolveQuestNpcId(
			Subsystem->GetRuntimeState(),
			InitialNpcId));
	const FGameXXKTrainingTravelRuntime InitialTravel = Subsystem->GetTrainingTravelRuntimeCopy();
	const FName InitialTravelCompanionId = InitialTravel.PartyUnits.IsValidIndex(1)
		? InitialTravel.PartyUnits[1].UnitId
		: NAME_None;
	const FName InitialTravelNpcId = InitialTravel.PartyUnits.IsValidIndex(2)
		? InitialTravel.PartyUnits[2].UnitId
		: NAME_None;
	const FName SelectedCompanionId = CompanionIds.Contains(InitialCompanionId) && CompanionIds[0] != InitialCompanionId
		? CompanionIds[0]
		: CompanionIds[1];
	TestTrue(TEXT("clicking a partner portrait changes only the viewed backpack owner"),
		Widget->SelectBackpackCharacterForTest(SelectedCompanionId));
	TestEqual(TEXT("partner view does not silently replace the active permanent party slot"),
		Subsystem->GetRuntimeState().CardRun.PartySelection.ActivePermanentCompanionInstanceId,
		InitialCompanionId);
	TestEqual(TEXT("partner view does not rebuild the running Travel companion slot"),
		Subsystem->GetTrainingTravelRuntimeCopy().PartyUnits.IsValidIndex(1)
			? Subsystem->GetTrainingTravelRuntimeCopy().PartyUnits[1].UnitId
			: NAME_None,
		InitialTravelCompanionId);
	TestEqual(TEXT("embedded backpack switches to the selected partner owner"),
		Widget->GetEmbeddedBackpackCharacterIdForTest(), SelectedCompanionId);
	TestEqual(TEXT("partner view stays on the explicit backpack center page"),
		Widget->GetActiveCenterPageForTest(), EGameXXKDesktopTrainingCenterPage::Backpack);
	TestEqual(TEXT("partner view does not counterfeit a bottom-navigation selection"),
		Widget->GetActiveNavForTest(), EGameXXKDesktopTrainingNav::None);

	const FName SelectedNpcId = NpcIds.Contains(InitialNpcId) && NpcIds[0] != InitialNpcId
		? NpcIds[0]
		: NpcIds[1];
	TestTrue(TEXT("clicking an NPC portrait changes only the viewed backpack owner"),
		Widget->SelectBackpackCharacterForTest(SelectedNpcId));
	FName NpcAfterView;
	TestTrue(TEXT("NPC still resolves after viewing another owner"),
		FGameXXKPartyFormationRules::ResolveQuestNpcId(
			Subsystem->GetRuntimeState(),
			NpcAfterView));
	TestEqual(TEXT("NPC view does not silently replace the active NPC party slot"),
		NpcAfterView,
		InitialNpcId);
	TestTrue(TEXT("NPC view never revives temporary provenance"),
		Subsystem->GetRuntimeState().CardRun.ActiveTemporaryQuestNpcId.IsNone());
	TestEqual(TEXT("NPC view does not rebuild the running Travel NPC slot"),
		Subsystem->GetTrainingTravelRuntimeCopy().PartyUnits.IsValidIndex(2)
			? Subsystem->GetTrainingTravelRuntimeCopy().PartyUnits[2].UnitId
			: NAME_None,
		InitialTravelNpcId);
	TestEqual(TEXT("embedded backpack switches to the selected NPC owner"),
		Widget->GetEmbeddedBackpackCharacterIdForTest(), SelectedNpcId);

	Widget->HandleActionClicked(1);
	TestEqual(TEXT("bottom formation navigation selects the formation center page"),
		Widget->GetActiveCenterPageForTest(), EGameXXKDesktopTrainingCenterPage::Formation);
	TestEqual(TEXT("bottom formation navigation owns the sole navigation focus"),
		Widget->GetActiveNavForTest(), EGameXXKDesktopTrainingNav::Formation);
	TestNotNull(TEXT("formation navigation builds a real formation page"),
		Widget->WidgetTree ? Widget->WidgetTree->FindWidget(TEXT("FormationPanel")) : nullptr);
	TestNull(TEXT("formation navigation replaces rather than impersonates the backpack page"),
		Widget->WidgetTree ? Widget->WidgetTree->FindWidget(TEXT("BackpackPanel")) : nullptr);
	UButton* FormationApply = Widget->WidgetTree
		? Cast<UButton>(Widget->WidgetTree->FindWidget(TEXT("FormationApplyButton")))
		: nullptr;
	UTextBlock* FormationApplyLabel = FormationApply
		? Cast<UTextBlock>(FormationApply->GetContent())
		: nullptr;
	TestNotNull(TEXT("formation apply owns a text label"), FormationApplyLabel);
	TestFalse(TEXT("formation apply label never wraps across resolution/DPI changes"),
		FormationApplyLabel && FormationApplyLabel->GetAutoWrapText());
	UButton* FormationRosterTab = Widget->WidgetTree
		? Cast<UButton>(Widget->WidgetTree->FindWidget(TEXT("FormationCompanionRosterButton")))
		: nullptr;
	UTextBlock* FormationRosterLabel = FormationRosterTab
		? Cast<UTextBlock>(FormationRosterTab->GetContent())
		: nullptr;
	TestNotNull(TEXT("formation roster tab owns a text label"), FormationRosterLabel);
	TestFalse(TEXT("formation roster label never wraps"),
		FormationRosterLabel && FormationRosterLabel->GetAutoWrapText());

	TestTrue(TEXT("formation page accepts a permanent-partner candidate without applying it"),
		Widget->SelectFormationCandidateForTest(SelectedCompanionId));
	TestEqual(TEXT("candidate selection alone still leaves the permanent party slot untouched"),
		Subsystem->GetRuntimeState().CardRun.PartySelection.ActivePermanentCompanionInstanceId,
		InitialCompanionId);
	TestTrue(TEXT("explicit formation apply writes the permanent-partner slot"),
		Widget->ApplyFormationCandidateForTest());
	TestEqual(TEXT("explicit formation apply replaces the permanent partner"),
		Subsystem->GetRuntimeState().CardRun.PartySelection.ActivePermanentCompanionInstanceId,
		SelectedCompanionId);
	TestEqual(TEXT("explicit formation apply rebuilds the running Travel companion slot"),
		Subsystem->GetTrainingTravelRuntimeCopy().PartyUnits[1].UnitId,
		SelectedCompanionId);

	TestTrue(TEXT("formation page accepts an NPC candidate without applying it"),
		Widget->SelectFormationCandidateForTest(SelectedNpcId));
	FName NpcAfterCandidate;
	TestTrue(TEXT("NPC resolves after candidate-only selection"),
		FGameXXKPartyFormationRules::ResolveQuestNpcId(
			Subsystem->GetRuntimeState(),
			NpcAfterCandidate));
	TestEqual(TEXT("candidate selection alone still leaves the NPC party slot untouched"),
		NpcAfterCandidate,
		InitialNpcId);
	for (int32 NpcIndex = 0; NpcIndex < 6; ++NpcIndex)
	{
		UButton* CandidateButton = Widget->WidgetTree
			? Cast<UButton>(Widget->WidgetTree->FindWidget(
				*FString::Printf(TEXT("FormationCandidateButton_%d"), NpcIndex)))
			: nullptr;
		TestNotNull(
			*FString::Printf(TEXT("owned NPC candidate %d exists"), NpcIndex),
			CandidateButton);
		TestTrue(
			*FString::Printf(TEXT("owned NPC candidate %d is enabled"), NpcIndex),
			CandidateButton && CandidateButton->GetIsEnabled());
	}
	TestTrue(TEXT("explicit formation apply writes the NPC slot"),
		Widget->ApplyFormationCandidateForTest());
	FName AppliedNpcId;
	TestTrue(TEXT("applied NPC resolves from ordered formation"),
		FGameXXKPartyFormationRules::ResolveQuestNpcId(
			Subsystem->GetRuntimeState(),
			AppliedNpcId));
	TestEqual(TEXT("explicit formation apply replaces the fixed NPC"),
		AppliedNpcId,
		SelectedNpcId);
	TestTrue(TEXT("explicit NPC apply keeps temporary provenance retired"),
		Subsystem->GetRuntimeState().CardRun.ActiveTemporaryQuestNpcId.IsNone());
	TestEqual(TEXT("explicit formation apply rebuilds the running Travel NPC slot"),
		Subsystem->GetTrainingTravelRuntimeCopy().PartyUnits[2].UnitId,
		SelectedNpcId);
	TestEqual(TEXT("formation changes do not overwrite the backpack viewing owner"),
		Widget->GetActiveBackpackCharacterIdForTest(), SelectedNpcId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingWorkbenchMapTravelSessionPreservesNpcTest,
	"GameXXK.DesktopTraining.Workbench.MapTravelSessionPreservesNpc",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingWorkbenchMapTravelSessionPreservesNpcTest::RunTest(
	const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem =
		NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestTrue(TEXT("map-session fixture starts"), Subsystem && Subsystem->StartGame())
		|| !TestTrue(TEXT("map-session fixture selects Yue Bai"),
			Subsystem->SelectTownQuestNpcForParty(TEXT("Npc.YueBai"))))
	{
		return false;
	}
	FName NpcBefore;
	TestTrue(TEXT("Yue Bai resolves before map session transfer"),
		FGameXXKPartyFormationRules::ResolveQuestNpcId(
			Subsystem->GetRuntimeState(),
			NpcBefore));

	UGameXXKDesktopTrainingWorkbenchWidget* FirstWidget =
		NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	FirstWidget->SetMVPSubsystem(Subsystem);
	FirstWidget->ConstructForTest();
	TestTrue(TEXT("first map-session workbench opens"), FirstWidget->OpenWorkbench());
	TestTrue(TEXT("first map-session backpack opens"), FirstWidget->OpenBackpack());
	const FGameXXKDesktopWorkbenchSessionState Session =
		FirstWidget->CaptureSessionStateForMapTravel();

	UGameXXKDesktopTrainingWorkbenchWidget* SecondWidget =
		NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	SecondWidget->SetMVPSubsystem(Subsystem);
	SecondWidget->ConstructForTest();
	SecondWidget->RestoreSessionStateAfterMapTravel(Session);

	FName NpcAfter;
	TestTrue(TEXT("NPC resolves after map session transfer"),
		FGameXXKPartyFormationRules::ResolveQuestNpcId(
			Subsystem->GetRuntimeState(),
			NpcAfter));
	TestEqual(TEXT("map session is presentation-only"), NpcAfter, NpcBefore);
	TestTrue(TEXT("map session never revives temporary provenance"),
		Subsystem->GetRuntimeState().CardRun.ActiveTemporaryQuestNpcId.IsNone());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingWorkbenchCharacterRosterOwnerPresentationTest,
	"GameXXK.DesktopTraining.Workbench.CharacterRoster.OwnerPresentationAllThirteen",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingWorkbenchCharacterRosterOwnerPresentationTest::RunTest(
	const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem =
		NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	if (!TestTrue(TEXT("owner-presentation fixture starts a new game"),
		Subsystem && Subsystem->StartGame()))
	{
		return false;
	}
	FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
	State.Screen = EGameXXKScreen::Town;
	UGameXXKDesktopTrainingWorkbenchWidget* Widget =
		NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	Widget->SetMVPSubsystem(Subsystem);

	const FName HeroId = FGameXXKEquipmentRules::HeroCharacterId();
	const TArray<FName> CompanionIds = Widget->GetCompanionCharacterIdsForTest();
	const TArray<FName> NpcIds = Widget->GetNpcCharacterIdsForTest();
	if (!TestEqual(TEXT("owner-presentation fixture owns six permanent companions"),
		CompanionIds.Num(), 6)
		|| !TestEqual(TEXT("owner-presentation fixture owns six quest NPCs"),
			NpcIds.Num(), 6))
	{
		return false;
	}
	TArray<FName> OwnerIds;
	OwnerIds.Add(HeroId);
	OwnerIds.Append(CompanionIds);
	OwnerIds.Append(NpcIds);
	if (!TestEqual(TEXT("owner-presentation fixture covers all thirteen owners"),
		OwnerIds.Num(), 13))
	{
		return false;
	}
	TestEqual(TEXT("independent non-Hero presentation table has twelve exact rows"),
		static_cast<int32>(UE_ARRAY_COUNT(ExpectedNonHeroIdlePresentations)), 12);

	const EGameXXKEquipmentSlot Slots[] = {
		EGameXXKEquipmentSlot::Weapon,
		EGameXXKEquipmentSlot::Head,
		EGameXXKEquipmentSlot::Armor,
		EGameXXKEquipmentSlot::Belt,
		EGameXXKEquipmentSlot::Shoes,
		EGameXXKEquipmentSlot::Accessory};
	TMap<FName, TArray<FName>> ExpectedEquipmentByOwner;
	for (const FName OwnerId : OwnerIds)
	{
		TArray<FName>& ExpectedEquipment =
			ExpectedEquipmentByOwner.FindOrAdd(OwnerId);
		for (const EGameXXKEquipmentSlot Slot : Slots)
		{
			const FName InstanceId = CreateCarryTestEquipment(
				*this,
				State,
				Slot,
				TEXT("owner-presentation fixture creates one unique slot instance"));
			FGameXXKEquipmentTransactionResult EquipResult;
			if (InstanceId.IsNone()
				|| !TestTrue(
					TEXT("owner-presentation fixture equips the unique instance to its owner"),
					Subsystem->EquipEquipmentInstance(
						OwnerId, Slot, InstanceId, EquipResult)))
			{
				return false;
			}
			ExpectedEquipment.Add(InstanceId);
		}
	}
	if (!TestTrue(TEXT("owner-presentation fixture normalizes shared physical cells"),
		Subsystem->NormalizeDesktopInventoryState()))
	{
		return false;
	}
	const FName TravelStage = FGameXXKTrainingRules::MakeStageId(
		EGameXXKTrainingDifficulty::Normal, 1);
	if (!TestTrue(TEXT("owner-presentation fixture starts a running Travel party"),
		Subsystem->StartTrainingTravel(TravelStage)))
	{
		return false;
	}
	if (!TestTrue(TEXT("owner-presentation fixture opens the workbench"),
		Widget->OpenWorkbench())
		|| !TestTrue(TEXT("owner-presentation fixture opens Backpack"),
			Widget->OpenBackpack()))
	{
		return false;
	}

	const TArray<FGameXXKDesktopInventoryEntryKey> BackpackBefore =
		State.DesktopInventory.BackpackSlots;
	const TArray<FGameXXKDesktopInventoryEntryKey> WarehouseBefore =
		State.DesktopInventory.WarehouseSlots;
	const FGameXXKCompanionPartySelection PartySelectionBefore =
		State.CardRun.PartySelection;
	const FGameXXKOrderedPartyFormation OrderedFormationBefore =
		State.CardRun.OrderedFormation;
	const FGameXXKTrainingTravelRuntime TravelBefore =
		Subsystem->GetTrainingTravelRuntimeCopy();
	FString HeroResourcePath;
	FString GuardResourcePath;
	FString YueBaiResourcePath;
	FBox2f HeroUv;
	FBox2f GuardUv;
	FBox2f YueBaiUv;
	TSet<FName> PresentedEquipmentIds;
	TSet<FString> NonHeroCentralResourcePaths;

	const auto VerifyViewedOwner = [
		this,
		Widget,
		Subsystem,
		&State,
		&ExpectedEquipmentByOwner,
		&BackpackBefore,
		&WarehouseBefore,
		&PartySelectionBefore,
		&OrderedFormationBefore,
		&TravelBefore,
		&Slots,
		&PresentedEquipmentIds,
		&HeroResourcePath,
		&GuardResourcePath,
		&YueBaiResourcePath,
		&HeroUv,
		&GuardUv,
		&YueBaiUv,
		&NonHeroCentralResourcePaths,
		HeroId](const FName OwnerId) -> bool
	{
		UGameXXKInventoryWindowWidget* Embedded = FindEmbeddedInventory(Widget);
		if (!TestNotNull(
			*FString::Printf(TEXT("%s owns a live embedded inventory"), *OwnerId.ToString()),
			Embedded))
		{
			return false;
		}
		TestEqual(
			*FString::Printf(TEXT("%s embedded owner follows the visible roster delegate"), *OwnerId.ToString()),
			Embedded->GetConfiguredCharacterIdForTest(),
			OwnerId);
		FGameXXKEquipmentLoadoutSnapshot Snapshot;
		if (!TestTrue(
			*FString::Printf(TEXT("%s exposes a loadout snapshot"), *OwnerId.ToString()),
			Subsystem->GetEquipmentLoadoutSnapshot(OwnerId, Snapshot)))
		{
			return false;
		}
		TestEqual(
			*FString::Printf(TEXT("%s snapshot belongs to the viewed owner"), *OwnerId.ToString()),
			Snapshot.CharacterId,
			OwnerId);
		const TArray<FName>* ExpectedEquipment =
			ExpectedEquipmentByOwner.Find(OwnerId);
		if (!TestNotNull(
			*FString::Printf(TEXT("%s owns an expected six-slot fixture"), *OwnerId.ToString()),
			ExpectedEquipment)
			|| !TestEqual(
				*FString::Printf(TEXT("%s expected fixture has six slots"), *OwnerId.ToString()),
				ExpectedEquipment ? ExpectedEquipment->Num() : 0,
				6))
		{
			return false;
		}
		for (int32 SlotIndex = 0; SlotIndex < UE_ARRAY_COUNT(Slots); ++SlotIndex)
		{
			const FName PresentedInstanceId =
				Embedded->GetEquippedInstanceForSlotForTest(Slots[SlotIndex]);
			TestEqual(
				*FString::Printf(
					TEXT("%s slot %d reads only that owner's instance"),
					*OwnerId.ToString(),
					SlotIndex),
				PresentedInstanceId,
				(*ExpectedEquipment)[SlotIndex]);
			TestFalse(
				*FString::Printf(
					TEXT("%s slot %d never reuses another owner's instance"),
					*OwnerId.ToString(),
					SlotIndex),
				PresentedEquipmentIds.Contains(PresentedInstanceId));
			PresentedEquipmentIds.Add(PresentedInstanceId);
			const FGameXXKEquipmentInstance* Instance =
				FGameXXKEquipmentRules::FindInstance(
					State.EquipmentCollection,
					PresentedInstanceId);
			if (TestNotNull(
				*FString::Printf(
					TEXT("%s slot %d keeps an authoritative instance"),
					*OwnerId.ToString(),
					SlotIndex),
				Instance))
			{
				TestEqual(
					*FString::Printf(
						TEXT("%s slot %d instance belongs to the viewed owner"),
						*OwnerId.ToString(),
						SlotIndex),
					Instance->OwnerCharacterId,
					OwnerId);
			}
		}

		UImage* CentralImage = Embedded->WidgetTree
			? Cast<UImage>(Embedded->WidgetTree->FindWidget(TEXT("InventoryCentralHeroIdle")))
			: nullptr;
		if (!TestNotNull(
			*FString::Printf(TEXT("%s owns central character art"), *OwnerId.ToString()),
			CentralImage))
		{
			return false;
		}
		const UObject* Resource = CentralImage->GetBrush().GetResourceObject();
		if (!TestNotNull(
			*FString::Printf(TEXT("%s central character art resolves a resource"), *OwnerId.ToString()),
			Resource))
		{
			return false;
		}
		const FString ResourcePath = Resource->GetPathName();
		const FBox2f Uv = CentralImage->GetBrush().GetUVRegion();
		TestEqual(
			*FString::Printf(TEXT("%s central character uses a bottom-center pivot"), *OwnerId.ToString()),
			CentralImage->GetRenderTransformPivot(),
			FVector2D(0.5f, 1.0f));
		TestTrue(
			*FString::Printf(TEXT("%s central character art is opaque"), *OwnerId.ToString()),
			FMath::IsNearlyEqual(CentralImage->GetRenderOpacity(), 1.0f));
		if (OwnerId == HeroId)
		{
			TestEqual(TEXT("Hero keeps the exact approved central full-body resource"),
				ResourcePath, ExpectedHeroCentralTexturePath);
			HeroResourcePath = ResourcePath;
			HeroUv = Uv;
		}
		else
		{
			const FExpectedOwnerIdlePresentation* Expected =
				FindExpectedOwnerIdlePresentation(OwnerId);
			if (!TestNotNull(
				*FString::Printf(TEXT("%s owns one independent literal presentation row"), *OwnerId.ToString()),
				Expected))
			{
				return false;
			}
			const FGameXXKBattleAnimationClipDescriptor ResolvedClip =
				FGameXXKBattleAnimationPresentation::ResolveClip(
					OwnerId, false, EGameXXKBattleAnimationAction::Idle);
			TestEqual(
				*FString::Printf(TEXT("%s descriptor AssetId matches the literal table"), *OwnerId.ToString()),
				ResolvedClip.AssetId,
				FString(Expected->AssetId));
			TestEqual(
				*FString::Printf(TEXT("%s descriptor TexturePath matches the literal table"), *OwnerId.ToString()),
				ResolvedClip.TexturePath.ToString(),
				FString(Expected->TexturePath));
			TestEqual(
				*FString::Printf(TEXT("%s loads the literal authored 2K Idle atlas"), *OwnerId.ToString()),
				ResourcePath,
				FString(Expected->TexturePath));
			TestTrue(
				*FString::Printf(TEXT("%s uses the literal 8x8 frame-zero UV"), *OwnerId.ToString()),
				Uv.Min.Equals(ExpectedIdleFrameZeroUv.Min, 0.0001f)
				&& Uv.Max.Equals(ExpectedIdleFrameZeroUv.Max, 0.0001f));
			NonHeroCentralResourcePaths.Add(ResourcePath);
			if (OwnerId.ToString().Contains(TEXT("Companion_Guard_")))
			{
				GuardResourcePath = ResourcePath;
				GuardUv = Uv;
			}
			else if (OwnerId == FName(TEXT("Npc.YueBai")))
			{
				YueBaiResourcePath = ResourcePath;
				YueBaiUv = Uv;
			}
		}

		TestTrue(
			*FString::Printf(TEXT("%s view keeps shared Backpack physical identity"), *OwnerId.ToString()),
			State.DesktopInventory.BackpackSlots == BackpackBefore);
		TestTrue(
			*FString::Printf(TEXT("%s view keeps shared Warehouse physical identity"), *OwnerId.ToString()),
			State.DesktopInventory.WarehouseSlots == WarehouseBefore);
		TestTrue(
			*FString::Printf(TEXT("%s view keeps PartySelection byte-identical"), *OwnerId.ToString()),
			FGameXXKCompanionPartySelection::StaticStruct()->CompareScriptStruct(
				&State.CardRun.PartySelection,
				&PartySelectionBefore,
				PPF_None));
		TestTrue(
			*FString::Printf(TEXT("%s view keeps OrderedFormation byte-identical"), *OwnerId.ToString()),
			FGameXXKOrderedPartyFormation::StaticStruct()->CompareScriptStruct(
				&State.CardRun.OrderedFormation,
				&OrderedFormationBefore,
				PPF_None));
		const FGameXXKTrainingTravelRuntime TravelAfter =
			Subsystem->GetTrainingTravelRuntimeCopy();
		TestTrue(
			*FString::Printf(TEXT("%s view keeps running Travel byte-identical"), *OwnerId.ToString()),
			FGameXXKTrainingTravelRuntime::StaticStruct()->CompareScriptStruct(
				&TravelAfter,
				&TravelBefore,
				PPF_None));
		return true;
	};

	if (!VerifyViewedOwner(HeroId)
		|| !RouteVisibleButtonDelegateAndFlush(
			*this,
			Widget,
			FindWorkbenchActionButton(Widget, TEXT("CharacterRosterCompanionButton")),
			TEXT("partner category")))
	{
		return false;
	}
	for (int32 Index = 0; Index < CompanionIds.Num(); ++Index)
	{
		if (!RouteVisibleButtonDelegateAndFlush(
			*this,
			Widget,
			FindWorkbenchActionButton(
				Widget,
				*FString::Printf(TEXT("CharacterRosterPortraitButton_1_%d"), Index)),
			*FString::Printf(TEXT("partner member %d"), Index))
			|| !VerifyViewedOwner(CompanionIds[Index]))
		{
			return false;
		}
	}
	if (!RouteVisibleButtonDelegateAndFlush(
		*this,
		Widget,
		FindWorkbenchActionButton(Widget, TEXT("CharacterRosterNpcButton")),
		TEXT("NPC category")))
	{
		return false;
	}
	for (int32 Index = 0; Index < NpcIds.Num(); ++Index)
	{
		if (!RouteVisibleButtonDelegateAndFlush(
			*this,
			Widget,
			FindWorkbenchActionButton(
				Widget,
				*FString::Printf(TEXT("CharacterRosterPortraitButton_2_%d"), Index)),
			*FString::Printf(TEXT("NPC member %d"), Index))
			|| !VerifyViewedOwner(NpcIds[Index]))
		{
			return false;
		}
	}
	TestEqual(TEXT("all twelve non-Hero owners resolve unique literal atlas resources"),
		NonHeroCentralResourcePaths.Num(),
		static_cast<int32>(UE_ARRAY_COUNT(ExpectedNonHeroIdlePresentations)));
	if (!RouteVisibleButtonDelegateAndFlush(
		*this,
		Widget,
		FindWorkbenchActionButton(Widget, TEXT("CharacterRosterHeroButton")),
		TEXT("Hero category")))
	{
		return false;
	}
	UGameXXKInventoryWindowWidget* RestoredHero = FindEmbeddedInventory(Widget);
	UImage* RestoredHeroImage = RestoredHero && RestoredHero->WidgetTree
		? Cast<UImage>(RestoredHero->WidgetTree->FindWidget(TEXT("InventoryCentralHeroIdle")))
		: nullptr;
	if (!TestNotNull(TEXT("switching back owns the Hero central image"),
		RestoredHeroImage))
	{
		return false;
	}
	const FString RestoredHeroResourcePath =
		RestoredHeroImage->GetBrush().GetResourceObject()
			? RestoredHeroImage->GetBrush().GetResourceObject()->GetPathName()
			: FString();
	const FBox2f RestoredHeroUv = RestoredHeroImage->GetBrush().GetUVRegion();
	TestEqual(TEXT("Hero resource returns after Guard and Yue Bai views"),
		RestoredHeroResourcePath, HeroResourcePath);
	TestTrue(TEXT("Hero UV returns after Guard and Yue Bai views"),
		RestoredHeroUv.Min.Equals(HeroUv.Min, 0.0001f)
		&& RestoredHeroUv.Max.Equals(HeroUv.Max, 0.0001f));
	TestTrue(TEXT("Hero, Guard, and Yue Bai resolve distinct central resources"),
		!HeroResourcePath.IsEmpty()
		&& !GuardResourcePath.IsEmpty()
		&& !YueBaiResourcePath.IsEmpty()
		&& HeroResourcePath != GuardResourcePath
		&& GuardResourcePath != YueBaiResourcePath
		&& HeroResourcePath != YueBaiResourcePath);
	TestTrue(TEXT("Hero full-body UV changes to Guard atlas frame zero"),
		!HeroUv.Min.Equals(GuardUv.Min, 0.0001f)
		|| !HeroUv.Max.Equals(GuardUv.Max, 0.0001f));
	TestTrue(TEXT("Yue Bai retains its own frame-zero UV before the Hero return"),
		YueBaiUv.Min.Equals(ExpectedIdleFrameZeroUv.Min, 0.0001f)
		&& YueBaiUv.Max.Equals(ExpectedIdleFrameZeroUv.Max, 0.0001f));
	TestEqual(TEXT("Hero category delegate restores the embedded Hero owner"),
		RestoredHero->GetConfiguredCharacterIdForTest(), HeroId);
	TestTrue(TEXT("all thirteen six-slot views expose seventy-eight unique instances"),
		PresentedEquipmentIds.Num() == 13 * UE_ARRAY_COUNT(Slots));
	TestTrue(TEXT("all owner views leave shared Backpack physical identity unchanged"),
		State.DesktopInventory.BackpackSlots == BackpackBefore);
	TestTrue(TEXT("all owner views leave OrderedFormation byte-identical"),
		FGameXXKOrderedPartyFormation::StaticStruct()->CompareScriptStruct(
			&State.CardRun.OrderedFormation,
			&OrderedFormationBefore,
			PPF_None));
	const FGameXXKTrainingTravelRuntime TravelAfterAllViews =
		Subsystem->GetTrainingTravelRuntimeCopy();
	TestTrue(TEXT("all owner views leave running Travel byte-identical"),
		FGameXXKTrainingTravelRuntime::StaticStruct()->CompareScriptStruct(
			&TravelAfterAllViews,
			&TravelBefore,
			PPF_None));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingWorkbenchRosterCategoryRepresentativeTest,
	"GameXXK.DesktopTraining.Workbench.CharacterRoster.CategorySelectsStableRepresentative",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingWorkbenchRosterCategoryRepresentativeTest::RunTest(
	const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem =
		NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestTrue(TEXT("roster-category fixture starts a new game"),
		Subsystem && Subsystem->StartGame()))
	{
		return false;
	}
	FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
	State.Screen = EGameXXKScreen::Town;
	UGameXXKDesktopTrainingWorkbenchWidget* Widget =
		NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	Widget->SetMVPSubsystem(Subsystem);
	const TArray<FName> CompanionIds = Widget->GetCompanionCharacterIdsForTest();
	const TArray<FName> NpcIds = Widget->GetNpcCharacterIdsForTest();
	if (!TestEqual(TEXT("roster-category fixture owns six companions"),
		CompanionIds.Num(), 6)
		|| !TestEqual(TEXT("roster-category fixture owns six NPCs"),
			NpcIds.Num(), 6))
	{
		return false;
	}
	const FName ActiveCompanionId =
		State.CardRun.PartySelection.ActivePermanentCompanionInstanceId;
	const FName ExpectedCompanionId = CompanionIds.Contains(ActiveCompanionId)
		? ActiveCompanionId
		: CompanionIds[0];
	const FName ActiveNpcId = GameXXKPermanentPartyTestFixtures::ResolveNpc(State);
	const FName ExpectedNpcId = NpcIds.Contains(ActiveNpcId)
		? ActiveNpcId
		: NpcIds[0];
	const EGameXXKEquipmentSlot Slots[] = {
		EGameXXKEquipmentSlot::Weapon,
		EGameXXKEquipmentSlot::Head,
		EGameXXKEquipmentSlot::Armor,
		EGameXXKEquipmentSlot::Belt,
		EGameXXKEquipmentSlot::Shoes,
		EGameXXKEquipmentSlot::Accessory};
	TMap<FName, TArray<FName>> ExpectedEquipmentByOwner;
	for (const FName OwnerId : {ExpectedCompanionId, ExpectedNpcId})
	{
		TArray<FName>& ExpectedEquipment =
			ExpectedEquipmentByOwner.FindOrAdd(OwnerId);
		for (const EGameXXKEquipmentSlot Slot : Slots)
		{
			const FName InstanceId = CreateCarryTestEquipment(
				*this,
				State,
				Slot,
				TEXT("roster-category fixture creates representative equipment"));
			FGameXXKEquipmentTransactionResult EquipResult;
			if (InstanceId.IsNone()
				|| !TestTrue(TEXT("roster-category fixture equips its representative"),
					Subsystem->EquipEquipmentInstance(
						OwnerId, Slot, InstanceId, EquipResult)))
			{
				return false;
			}
			ExpectedEquipment.Add(InstanceId);
		}
	}
	if (!TestTrue(TEXT("roster-category fixture normalizes shared inventory"),
		Subsystem->NormalizeDesktopInventoryState())
		|| !TestTrue(TEXT("roster-category fixture starts running Travel"),
			Subsystem->StartTrainingTravel(FGameXXKTrainingRules::MakeStageId(
				EGameXXKTrainingDifficulty::Normal, 1)))
		|| !TestTrue(TEXT("roster-category fixture opens the workbench"),
			Widget->OpenWorkbench())
		|| !TestTrue(TEXT("roster-category fixture opens Backpack"),
			Widget->OpenBackpack()))
	{
		return false;
	}

	const TArray<FGameXXKDesktopInventoryEntryKey> BackpackBefore =
		State.DesktopInventory.BackpackSlots;
	const TArray<FGameXXKDesktopInventoryEntryKey> WarehouseBefore =
		State.DesktopInventory.WarehouseSlots;
	const FGameXXKCompanionPartySelection PartyBefore =
		State.CardRun.PartySelection;
	const FGameXXKOrderedPartyFormation FormationBefore =
		State.CardRun.OrderedFormation;
	const FGameXXKTrainingTravelRuntime TravelBefore =
		Subsystem->GetTrainingTravelRuntimeCopy();
	const int32 CarrySourceSlot = Widget->FindFirstBackpackEquipmentSlotForTest();
	if (!TestTrue(TEXT("roster-category fixture has one shared physical source"),
		CarrySourceSlot != INDEX_NONE))
	{
		return false;
	}

	const auto VerifyRepresentative = [
		this,
		Widget,
		Subsystem,
		&State,
		&ExpectedEquipmentByOwner,
		&BackpackBefore,
		&WarehouseBefore,
		&PartyBefore,
		&FormationBefore,
		&TravelBefore,
		&Slots](const FName ExpectedOwnerId) -> bool
	{
		TestEqual(TEXT("category delegate immediately updates the active Backpack owner"),
			Widget->GetActiveBackpackCharacterIdForTest(), ExpectedOwnerId);
		UGameXXKInventoryWindowWidget* Embedded = FindEmbeddedInventory(Widget);
		if (!TestNotNull(TEXT("category delegate owns a rebuilt embedded Backpack"),
			Embedded))
		{
			return false;
		}
		const FName EmbeddedOwnerId = Embedded->GetConfiguredCharacterIdForTest();
		TestEqual(TEXT("category delegate immediately configures the embedded owner"),
			EmbeddedOwnerId, ExpectedOwnerId);
		FGameXXKEquipmentLoadoutSnapshot Snapshot;
		if (!TestTrue(TEXT("embedded category owner resolves a loadout snapshot"),
			Subsystem->GetEquipmentLoadoutSnapshot(EmbeddedOwnerId, Snapshot)))
		{
			return false;
		}
		TestEqual(TEXT("category snapshot belongs to the stable representative"),
			Snapshot.CharacterId, ExpectedOwnerId);
		const TArray<FName>* ExpectedEquipment =
			ExpectedEquipmentByOwner.Find(ExpectedOwnerId);
		if (!TestNotNull(TEXT("stable representative owns a six-slot fixture"),
			ExpectedEquipment))
		{
			return false;
		}
		for (int32 SlotIndex = 0; SlotIndex < UE_ARRAY_COUNT(Slots); ++SlotIndex)
		{
			TestEqual(
				*FString::Printf(TEXT("category representative slot %d switches immediately"), SlotIndex),
				Embedded->GetEquippedInstanceForSlotForTest(Slots[SlotIndex]),
				(*ExpectedEquipment)[SlotIndex]);
		}
		UImage* CentralImage = Embedded->WidgetTree
			? Cast<UImage>(Embedded->WidgetTree->FindWidget(
				TEXT("InventoryCentralHeroIdle")))
			: nullptr;
		if (!TestNotNull(TEXT("category representative owns central art"),
			CentralImage))
		{
			return false;
		}
		const UObject* Resource = CentralImage->GetBrush().GetResourceObject();
		const FExpectedOwnerIdlePresentation* ExpectedPresentation =
			FindExpectedOwnerIdlePresentation(ExpectedOwnerId);
		if (!TestNotNull(TEXT("category representative owns a literal atlas row"),
			ExpectedPresentation))
		{
			return false;
		}
		TestEqual(TEXT("category representative immediately owns the center resource"),
			Resource ? Resource->GetPathName() : FString(),
			FString(ExpectedPresentation->TexturePath));
		TestTrue(TEXT("category delegate preserves shared Backpack physical identity"),
			State.DesktopInventory.BackpackSlots == BackpackBefore);
		TestTrue(TEXT("category delegate preserves shared Warehouse physical identity"),
			State.DesktopInventory.WarehouseSlots == WarehouseBefore);
		TestTrue(TEXT("category delegate preserves PartySelection byte-identically"),
			FGameXXKCompanionPartySelection::StaticStruct()->CompareScriptStruct(
				&State.CardRun.PartySelection, &PartyBefore, PPF_None));
		TestTrue(TEXT("category delegate preserves OrderedFormation byte-identically"),
			FGameXXKOrderedPartyFormation::StaticStruct()->CompareScriptStruct(
				&State.CardRun.OrderedFormation, &FormationBefore, PPF_None));
		const FGameXXKTrainingTravelRuntime TravelAfter =
			Subsystem->GetTrainingTravelRuntimeCopy();
		TestTrue(TEXT("category delegate preserves running Travel byte-identically"),
			FGameXXKTrainingTravelRuntime::StaticStruct()->CompareScriptStruct(
				&TravelAfter, &TravelBefore, PPF_None));
		return true;
	};

	TestEqual(TEXT("category chain begins on Hero"),
		Widget->GetActiveBackpackCharacterIdForTest(),
		FGameXXKEquipmentRules::HeroCharacterId());
	TestTrue(TEXT("Hero carry begins before the partner category switch"),
		Widget->PickUpBackpackSlotForTest(CarrySourceSlot));
	const int32 PartnerBuildCount =
		Widget->GetProgrammaticLayoutBuildCountForTest();
	if (!TestTrue(TEXT("visible partner category delegate routes"),
		RouteVisibleButtonDelegateAndFlush(
			*this,
			Widget,
			FindWorkbenchActionButton(
				Widget,
				TEXT("CharacterRosterCompanionButton")),
			TEXT("partner category"))))
	{
		return false;
	}
	TestFalse(TEXT("partner category switch cancels the structural carry"),
		Widget->HasDesktopCarriedEntry());
	TestEqual(TEXT("partner category switch rebuilds exactly once"),
		Widget->GetProgrammaticLayoutBuildCountForTest(), PartnerBuildCount + 1);
	if (!VerifyRepresentative(ExpectedCompanionId))
	{
		return false;
	}

	TestTrue(TEXT("partner carry begins before the NPC category switch"),
		Widget->PickUpBackpackSlotForTest(CarrySourceSlot));
	const int32 NpcBuildCount =
		Widget->GetProgrammaticLayoutBuildCountForTest();
	if (!TestTrue(TEXT("visible NPC category delegate routes"),
		RouteVisibleButtonDelegateAndFlush(
			*this,
			Widget,
			FindWorkbenchActionButton(
				Widget,
				TEXT("CharacterRosterNpcButton")),
			TEXT("NPC category"))))
	{
		return false;
	}
	TestFalse(TEXT("NPC category switch cancels the structural carry"),
		Widget->HasDesktopCarriedEntry());
	TestEqual(TEXT("NPC category switch rebuilds exactly once"),
		Widget->GetProgrammaticLayoutBuildCountForTest(), NpcBuildCount + 1);
	return VerifyRepresentative(ExpectedNpcId);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingWorkbenchRosterTwoLayerInteractionTest,
	"GameXXK.DesktopTraining.Workbench.CharacterRoster.TwoLayerSelectionAndOwnerPages",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingWorkbenchRosterTwoLayerInteractionTest::RunTest(
	const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem =
		NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestTrue(TEXT("two-layer roster fixture starts a new game"),
		Subsystem && Subsystem->StartGame()))
	{
		return false;
	}
	UGameXXKDesktopTrainingWorkbenchWidget* Widget =
		NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	Widget->SetMVPSubsystem(Subsystem);
	Widget->ConstructForTest();
	if (!TestTrue(TEXT("two-layer roster fixture opens Backpack"),
		Widget->OpenBackpack()))
	{
		return false;
	}

	const TArray<FName> CompanionIds = Widget->GetCompanionCharacterIdsForTest();
	const TArray<FName> NpcIds = Widget->GetNpcCharacterIdsForTest();
	if (!TestEqual(TEXT("two-layer roster fixture owns six companions"), CompanionIds.Num(), 6)
		|| !TestEqual(TEXT("two-layer roster fixture owns six NPCs"), NpcIds.Num(), 6))
	{
		return false;
	}
	if (!RouteVisibleButtonDelegateAndFlush(
		*this,
		Widget,
		FindWorkbenchActionButton(Widget, TEXT("CharacterRosterHeroButton")),
		TEXT("Hero category first-layer open")))
	{
		return false;
	}
	TestTrue(TEXT("opening the Hero category uses its selected texture"),
		GetButtonNormalResourcePath(FindWorkbenchActionButton(
			Widget, TEXT("CharacterRosterHeroButton"))).Contains(TEXT("004_tab_2")));
	TestNotNull(TEXT("opening the Hero category reveals its current member layer"),
		Widget->WidgetTree
			? Widget->WidgetTree->FindWidget(TEXT("CharacterRosterPortraitButton_0_0"))
			: nullptr);
	if (!RouteVisibleButtonDelegateAndFlush(
		*this,
		Widget,
		FindWorkbenchActionButton(Widget, TEXT("CharacterRosterHeroButton")),
		TEXT("Hero category collapse")))
	{
		return false;
	}
	TestTrue(TEXT("collapsing the Hero category restores its normal texture"),
		GetButtonNormalResourcePath(FindWorkbenchActionButton(
			Widget, TEXT("CharacterRosterHeroButton"))).Contains(TEXT("003_tab_1")));
	TestNull(TEXT("collapsing the Hero category hides its member layer"),
		Widget->WidgetTree
			? Widget->WidgetTree->FindWidget(TEXT("CharacterRosterPortraitButton_0_0"))
			: nullptr);

	const FName ActiveCompanionId =
		Subsystem->GetRuntimeState().CardRun.PartySelection.ActivePermanentCompanionInstanceId;
	const FName SelectedCompanionId = CompanionIds[0] == ActiveCompanionId
		? CompanionIds[1]
		: CompanionIds[0];
	if (!RouteVisibleButtonDelegateAndFlush(
		*this,
		Widget,
		FindWorkbenchActionButton(Widget, TEXT("CharacterRosterCompanionButton")),
		TEXT("partner category first-layer open")))
	{
		return false;
	}
	TestNotNull(TEXT("opening the partner category reveals its member layer"),
		Widget->WidgetTree
			? Widget->WidgetTree->FindWidget(TEXT("CharacterRosterPortraitButton_1_0"))
			: nullptr);

	const int32 CompanionIndex = CompanionIds.IndexOfByKey(SelectedCompanionId);
	if (!RouteVisibleButtonDelegateAndFlush(
		*this,
		Widget,
		FindWorkbenchActionButton(
			Widget,
			*FString::Printf(TEXT("CharacterRosterPortraitButton_1_%d"), CompanionIndex)),
		TEXT("explicit partner member")))
	{
		return false;
	}
	TestEqual(TEXT("member click selects the exact companion owner"),
		Widget->GetActiveBackpackCharacterIdForTest(), SelectedCompanionId);
	TestNotNull(TEXT("selecting a partner keeps the member layer open"),
		Widget->WidgetTree
			? Widget->WidgetTree->FindWidget(TEXT("CharacterRosterPortraitButton_1_0"))
			: nullptr);

	if (!RouteVisibleButtonDelegateAndFlush(
		*this,
		Widget,
		FindWorkbenchActionButton(Widget, TEXT("CharacterRosterCompanionButton")),
		TEXT("selected partner category collapse")))
	{
		return false;
	}
	TestNull(TEXT("clicking the selected partner category hides only its member layer"),
		Widget->WidgetTree
			? Widget->WidgetTree->FindWidget(TEXT("CharacterRosterPortraitButton_1_0"))
			: nullptr);
	TestEqual(TEXT("collapsing the member layer preserves the selected companion"),
		Widget->GetActiveBackpackCharacterIdForTest(), SelectedCompanionId);
	TestTrue(TEXT("collapsed partner category returns to its normal texture"),
		GetButtonNormalResourcePath(FindWorkbenchActionButton(
			Widget, TEXT("CharacterRosterCompanionButton"))).Contains(TEXT("003_tab_1")));

	if (!RouteVisibleButtonDelegateAndFlush(
		*this,
		Widget,
		FindWorkbenchActionButton(Widget, TEXT("CharacterRosterCompanionButton")),
		TEXT("selected partner category reopen")))
	{
		return false;
	}
	TestNotNull(TEXT("clicking the selected partner category again restores its member layer"),
		Widget->WidgetTree
			? Widget->WidgetTree->FindWidget(TEXT("CharacterRosterPortraitButton_1_0"))
			: nullptr);
	TestTrue(TEXT("reopened partner category uses its selected texture"),
		GetButtonNormalResourcePath(FindWorkbenchActionButton(
			Widget, TEXT("CharacterRosterCompanionButton"))).Contains(TEXT("004_tab_2")));
	TestEqual(TEXT("reopening the member layer still preserves the selected companion"),
		Widget->GetActiveBackpackCharacterIdForTest(), SelectedCompanionId);

	const auto VerifyOwnerPageButton = [this, Widget](
		const FName ExpectedOwnerId,
		const int32 TabIndex,
		const EGameXXKCharacterBackpackTab ExpectedTab,
		const TCHAR* Context) -> bool
	{
		UGameXXKInventoryWindowWidget* Embedded = FindEmbeddedInventory(Widget);
		UGameXXKCharacterBackpackTabButton* TabButton = Embedded && Embedded->WidgetTree
			? Cast<UGameXXKCharacterBackpackTabButton>(Embedded->WidgetTree->FindWidget(
				*FString::Printf(TEXT("InventoryCharacterTab_%d"), TabIndex)))
			: nullptr;
		if (!RouteVisibleButtonDelegateAndFlush(*this, Widget, TabButton, Context))
		{
			return false;
		}
		Embedded = FindEmbeddedInventory(Widget);
		if (!TestNotNull(*FString::Printf(TEXT("%s rebuilds the embedded owner page"), Context), Embedded))
		{
			return false;
		}
		TestEqual(*FString::Printf(TEXT("%s preserves the exact owner"), Context),
			Embedded->GetConfiguredCharacterIdForTest(), ExpectedOwnerId);
		TestEqual(*FString::Printf(TEXT("%s opens the requested page"), Context),
			Embedded->GetActiveCharacterBackpackTabForTest(), ExpectedTab);
		return true;
	};

	if (!VerifyOwnerPageButton(
		SelectedCompanionId,
		0,
		EGameXXKCharacterBackpackTab::Attributes,
		TEXT("partner Attributes tab")))
	{
		return false;
	}
	UGameXXKInventoryWindowWidget* Embedded = FindEmbeddedInventory(Widget);
	TestTrue(TEXT("partner Attributes page exposes real owner stats"),
		Embedded && Embedded->GetCharacterTabBodyTextForTest().ToString().Contains(TEXT("属性")));
	if (!VerifyOwnerPageButton(
		SelectedCompanionId,
		2,
		EGameXXKCharacterBackpackTab::Deck,
		TEXT("partner Deck tab")))
	{
		return false;
	}
	Embedded = FindEmbeddedInventory(Widget);
	TestTrue(TEXT("partner Deck page exposes the selected owner's cards"),
		Embedded && !Embedded->GetHeroCardBackpackIdsForTest().IsEmpty());

	if (!RouteVisibleButtonDelegateAndFlush(
		*this,
		Widget,
		FindWorkbenchActionButton(Widget, TEXT("CharacterRosterNpcButton")),
		TEXT("NPC category first-layer open")))
	{
		return false;
	}
	const FName SelectedNpcId = NpcIds.Last();
	if (!RouteVisibleButtonDelegateAndFlush(
		*this,
		Widget,
		FindWorkbenchActionButton(
			Widget,
			*FString::Printf(TEXT("CharacterRosterPortraitButton_2_%d"), NpcIds.Num() - 1)),
		TEXT("explicit NPC member")))
	{
		return false;
	}
	TestEqual(TEXT("member click selects the exact NPC owner"),
		Widget->GetActiveBackpackCharacterIdForTest(), SelectedNpcId);
	if (!VerifyOwnerPageButton(
		SelectedNpcId,
		0,
		EGameXXKCharacterBackpackTab::Attributes,
		TEXT("NPC Attributes tab"))
		|| !VerifyOwnerPageButton(
			SelectedNpcId,
			2,
			EGameXXKCharacterBackpackTab::Deck,
			TEXT("NPC Deck tab")))
	{
		return false;
	}
	Embedded = FindEmbeddedInventory(Widget);
	TestTrue(TEXT("NPC Deck page exposes the selected owner's cards"),
		Embedded && !Embedded->GetHeroCardBackpackIdsForTest().IsEmpty());

	if (!RouteVisibleButtonDelegateAndFlush(
		*this,
		Widget,
		FindWorkbenchActionButton(Widget, TEXT("CharacterRosterNpcButton")),
		TEXT("selected NPC category collapse")))
	{
		return false;
	}
	TestNull(TEXT("clicking the selected NPC category hides its six member tabs"),
		Widget->WidgetTree
			? Widget->WidgetTree->FindWidget(TEXT("CharacterRosterPortraitButton_2_0"))
			: nullptr);
	TestEqual(TEXT("collapsing NPC members never resets to the Tusi representative"),
		Widget->GetActiveBackpackCharacterIdForTest(), SelectedNpcId);

	if (!RouteVisibleButtonDelegateAndFlush(
		*this,
		Widget,
		FindWorkbenchActionButton(Widget, TEXT("CharacterRosterCompanionButton")),
		TEXT("restore remembered partner category")))
	{
		return false;
	}
	TestEqual(TEXT("returning to partners restores the last explicit companion"),
		Widget->GetActiveBackpackCharacterIdForTest(), SelectedCompanionId);
	TestTrue(TEXT("partner category is selected after returning"),
		GetButtonNormalResourcePath(FindWorkbenchActionButton(
			Widget, TEXT("CharacterRosterCompanionButton"))).Contains(TEXT("004_tab_2")));
	TestTrue(TEXT("NPC category becomes unselected after returning to partners"),
		GetButtonNormalResourcePath(FindWorkbenchActionButton(
			Widget, TEXT("CharacterRosterNpcButton"))).Contains(TEXT("003_tab_1")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingWorkbenchLiveNumericRefreshTest,
	"GameXXK.DesktopTraining.Workbench.LiveNumericRefreshWithoutRebuild",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingWorkbenchLiveNumericRefreshTest::RunTest(
	const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem =
		NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestTrue(TEXT("live-refresh fixture starts a new game"),
		Subsystem && Subsystem->StartGame()))
	{
		return false;
	}
	FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
	UnlockToolsTalent(State);
	State.Screen = EGameXXKScreen::Town;
	State.Training.bTravelActive = false;
	State.Training.OwnedChestTokens.Reset();

	UGameXXKDesktopTrainingWorkbenchWidget* Widget =
		NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	Widget->SetMVPSubsystem(Subsystem);
	Widget->ConstructForTest();
	if (!TestTrue(TEXT("live-refresh fixture opens the workbench"), Widget->OpenWorkbench())
		|| !TestTrue(TEXT("live-refresh fixture expands Backpack"), Widget->OpenBackpack()))
	{
		return false;
	}
	Widget->HandleActionClicked(3);
	Widget->TickForTest(0.0f);
	if (!TestTrue(TEXT("live-refresh fixture opens Tools"), Widget->IsToolsPanelActiveForTest()))
	{
		return false;
	}
	UGameXXKInventoryWindowWidget* Embedded = FindEmbeddedInventory(Widget);
	if (!TestNotNull(TEXT("live-refresh fixture owns the embedded Backpack"), Embedded)
		|| !TestTrue(TEXT("live-refresh fixture opens Attributes"),
			Embedded->OpenCharacterBackpackTabForTest(EGameXXKCharacterBackpackTab::Attributes)))
	{
		return false;
	}

	const int32 BuildCountBeforeRefresh = Widget->GetProgrammaticLayoutBuildCountForTest();
	UGameXXKInventoryWindowWidget* const EmbeddedBeforeRefresh = Embedded;
	UWidget* const TravelStripBeforeRefresh = Widget->WidgetTree
		? Widget->WidgetTree->FindWidget(TEXT("TrainingTravelStrip"))
		: nullptr;
	if (!TestNotNull(TEXT("live-refresh fixture owns the idle-strip hover surface"),
		TravelStripBeforeRefresh))
	{
		return false;
	}

	State.PlayerGold = 4321;
	State.PlayerLevel = 2;
	State.PlayerXP = 50;
	State.PlayerHP = FMath::Max(1, State.PlayerMaxHP - 7);
	State.Training.PendingTravelGold = 111;
	State.Training.PendingTravelExperience = 222;
	State.Training.PendingTravelNormalChestCount = 3;
	State.Training.PendingTravelAdvancedChestCount = 4;
	State.Training.TravelNormalChestCooldownRemainingSeconds = 119;
	State.Training.TravelAdvancedChestCooldownRemainingSeconds = 179;
	State.ToolProgress.Level = 2;
	State.ToolProgress.Experience = 9;
	FGameXXKTrainingChestToken NormalToken;
	NormalToken.Tier = EGameXXKTrainingRewardTier::NormalChest;
	NormalToken.AcquisitionOrdinal = ++State.Training.NextChestAcquisitionOrdinal;
	State.Training.OwnedChestTokens.Add(NormalToken);

	Widget->TickForTest(1.0f);
	TestEqual(TEXT("live refresh never rebuilds the workbench"),
		Widget->GetProgrammaticLayoutBuildCountForTest(), BuildCountBeforeRefresh);
	TestTrue(TEXT("live refresh preserves the embedded inventory instance"),
		FindEmbeddedInventory(Widget) == EmbeddedBeforeRefresh);
	TestEqual(TEXT("Backpack gold updates in place"),
		GetNamedText(Widget->WidgetTree, TEXT("BackpackGoldText")), FString(TEXT("4321")));
	TestEqual(TEXT("normal chest count updates in place"),
		GetNamedText(Widget->WidgetTree, TEXT("TrainingNormalChestCountText")), FString(TEXT("1")));
	UTextBlock* NormalChestCountText = Widget->WidgetTree
		? Cast<UTextBlock>(Widget->WidgetTree->FindWidget(TEXT("TrainingNormalChestCountText")))
		: nullptr;
	if (TestNotNull(TEXT("normal chest owns a rendered count label"), NormalChestCountText))
	{
		const FSlateFontInfo CountFont = NormalChestCountText->GetFont();
		TestEqual(TEXT("normal chest count uses a two-pixel black outline"),
			CountFont.OutlineSettings.OutlineSize,
			2);
		TestEqual(TEXT("normal chest count outline is black"),
			CountFont.OutlineSettings.OutlineColor,
			FLinearColor::Black);
		TestEqual(TEXT("normal chest count is white"),
			NormalChestCountText->GetColorAndOpacity().GetSpecifiedColor(),
			FLinearColor::White);
		const UOverlaySlot* CountSlot = Cast<UOverlaySlot>(NormalChestCountText->Slot);
		TestTrue(TEXT("normal chest count is pinned to the lower-right corner"),
			CountSlot
			&& CountSlot->GetHorizontalAlignment() == HAlign_Right
			&& CountSlot->GetVerticalAlignment() == VAlign_Bottom
			&& CountSlot->GetPadding().Right <= 3.0f
			&& CountSlot->GetPadding().Bottom <= 2.0f);
	}
	UButton* NormalChestButton = Widget->WidgetTree
		? Cast<UButton>(Widget->WidgetTree->FindWidget(TEXT("TrainingNormalChestButton")))
		: nullptr;
	TestTrue(TEXT("normal chest button becomes enabled"),
		NormalChestButton && NormalChestButton->GetIsEnabled());
	const FString FirstTooltip = TravelStripBeforeRefresh->GetToolTipText().ToString();
	TestTrue(TEXT("hover text updates pending rewards"),
		FirstTooltip.Contains(TEXT("111金币 / 222经验 / 普通箱3 / 高级箱4")));
	TestTrue(TEXT("hover text updates both cooldowns"),
		FirstTooltip.Contains(TEXT("01:59")) && FirstTooltip.Contains(TEXT("02:59")));
	TestTrue(TEXT("visible Attributes page updates XP"),
		GetNamedText(EmbeddedBeforeRefresh->WidgetTree, TEXT("InventoryCharacterExperienceText"))
			.Contains(TEXT("50 / 200")));
	TestTrue(TEXT("visible tool progress updates in place"),
		GetNamedText(Widget->WidgetTree, TEXT("ToolProgressText")).Contains(TEXT("9/")));

	State.Training.TravelNormalChestCooldownRemainingSeconds = 118;
	State.Training.TravelAdvancedChestCooldownRemainingSeconds = 178;
	Widget->TickForTest(1.0f);
	TestTrue(TEXT("live cooldown refresh preserves the hover surface instance"),
		Widget->WidgetTree->FindWidget(TEXT("TrainingTravelStrip")) == TravelStripBeforeRefresh);
	const FString SecondTooltip = TravelStripBeforeRefresh->GetToolTipText().ToString();
	TestTrue(TEXT("hover cooldown continues updating without leaving the strip"),
		SecondTooltip.Contains(TEXT("01:58")) && SecondTooltip.Contains(TEXT("02:58")));
	TestEqual(TEXT("cooldown refresh still does not rebuild the workbench"),
		Widget->GetProgrammaticLayoutBuildCountForTest(), BuildCountBeforeRefresh);

	const TArray<FName> NpcIds = Widget->GetNpcCharacterIdsForTest();
	if (!TestTrue(TEXT("live-refresh fixture exposes a task NPC"), !NpcIds.IsEmpty())
		|| !TestTrue(TEXT("live-refresh fixture selects the task NPC"),
			Widget->SelectBackpackCharacterForTest(NpcIds[0])))
	{
		return false;
	}
	Embedded = FindEmbeddedInventory(Widget);
	if (!TestNotNull(TEXT("task NPC keeps an embedded inventory"), Embedded)
		|| !TestTrue(TEXT("task NPC opens Attributes"),
			Embedded->OpenCharacterBackpackTabForTest(EGameXXKCharacterBackpackTab::Attributes)))
	{
		return false;
	}
	FGameXXKQuestNpcProgression* NpcProgression =
		State.CardRun.PartySelection.QuestNpcProgressions.Find(NpcIds[0]);
	if (!TestNotNull(TEXT("task NPC owns independent progression"), NpcProgression))
	{
		return false;
	}
	State.PlayerXP = 77;
	NpcProgression->Experience = 35;
	Widget->TickForTest(1.0f);
	UTextBlock* NpcExperienceText = Embedded->WidgetTree
		? Cast<UTextBlock>(Embedded->WidgetTree->FindWidget(TEXT("InventoryCharacterExperienceText")))
		: nullptr;
	UProgressBar* NpcExperienceBar = Embedded->WidgetTree
		? Cast<UProgressBar>(Embedded->WidgetTree->FindWidget(TEXT("InventoryCharacterExperienceBar")))
		: nullptr;
	TestTrue(TEXT("task NPC experience text stays visible"),
		NpcExperienceText && NpcExperienceText->GetVisibility() == ESlateVisibility::HitTestInvisible);
	TestTrue(TEXT("task NPC experience bar stays visible"),
		NpcExperienceBar && NpcExperienceBar->GetVisibility() == ESlateVisibility::HitTestInvisible);
	TestTrue(TEXT("task NPC displays its own experience instead of hero experience"),
		NpcExperienceText && NpcExperienceText->GetText().ToString().Contains(TEXT("35 / 100")));
	TestTrue(TEXT("task NPC experience bar displays its own progress"),
		NpcExperienceBar && FMath::IsNearlyEqual(NpcExperienceBar->GetPercent(), 0.35f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingWorkbenchRepeatCombineTest,
	"GameXXK.DesktopTraining.Workbench.Tools.RepeatAutoFillAndCombine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingWorkbenchRepeatCombineTest::RunTest(
	const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem =
		NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestTrue(TEXT("repeat-combine fixture starts a new game"),
		Subsystem && Subsystem->StartGame()))
	{
		return false;
	}
	FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
	UnlockToolsTalent(State);
	PreserveLegacyWorkbenchCapacity(State);
	State.Screen = EGameXXKScreen::Town;
	for (int32 Index = 0; Index < 18; ++Index)
	{
		FGameXXKEquipmentCreateRequest Request;
		Request.Set = EGameXXKEquipmentSet::PoJun;
		Request.Quality = EGameXXKEquipmentQuality::Common;
		Request.ItemLevel = 1 + Index % 10;
		Request.bForceSlot = true;
		Request.ForcedSlot = EGameXXKEquipmentSlot::Accessory;
		FName InstanceId;
		FString Error;
		if (!TestTrue(TEXT("repeat-combine fixture creates common equipment"),
			FGameXXKEquipmentRules::CreateRolledInstance(
				State.EquipmentCollection,
				Request,
				InstanceId,
				&Error)))
		{
			AddError(Error);
			return false;
		}
	}
	if (!TestTrue(TEXT("repeat-combine fixture normalizes desktop storage"),
		Subsystem->NormalizeDesktopInventoryState()))
	{
		return false;
	}

	UGameXXKDesktopTrainingWorkbenchWidget* Widget =
		NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	Widget->SetMVPSubsystem(Subsystem);
	Widget->ConstructForTest();
	if (!TestTrue(TEXT("repeat-combine fixture opens Backpack"), Widget->OpenBackpack()))
	{
		return false;
	}
	Widget->HandleActionClicked(3);
	TestTrue(TEXT("repeat-combine fixture opens Tools"), Widget->IsToolsPanelActiveForTest());
	TestTrue(TEXT("repeat-combine fixture selects Combine"),
		Widget->SetToolModeForTest(EGameXXKDesktopToolMode::Combine));

	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		Widget->HandleActionClicked(311);
		TestEqual(*FString::Printf(TEXT("pass %d auto-fills nine inputs"), Pass + 1),
			Widget->GetOccupiedToolSlotCountForTest(), 9);
		TestTrue(*FString::Printf(TEXT("pass %d combines successfully"), Pass + 1),
			Widget->ConfirmToolForTest());
		TestEqual(*FString::Printf(TEXT("pass %d clears all tool reservations"), Pass + 1),
			Widget->GetOccupiedToolSlotCountForTest(), 0);
	}
	TestEqual(TEXT("two common-equipment combines award eighteen tool experience"),
		Subsystem->GetRuntimeState().ToolProgress.Experience,
		int64(18));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingWarehouseBatchControlsTest,
	"GameXXK.DesktopTraining.Workbench.WarehouseBatchTransferControls",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingWarehouseBatchControlsTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem =
		NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestTrue(TEXT("warehouse-control fixture starts a new game"),
		Subsystem && Subsystem->StartGame()))
	{
		return false;
	}
	Subsystem->GetMutableRuntimeState().Talents.NodeRanks.Add(TEXT("Talent.Root"), 1);
	if (!TestTrue(TEXT("warehouse-control fixture unlocks page two"),
		Subsystem->NormalizeDesktopInventoryState()))
	{
		return false;
	}

	UGameXXKDesktopTrainingWorkbenchWidget* Widget =
		NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	Widget->SetMVPSubsystem(Subsystem);
	Widget->ConstructForTest();
	if (!TestTrue(TEXT("warehouse-control fixture opens Backpack"), Widget->OpenBackpack()))
	{
		return false;
	}
	Widget->HandleActionClicked(0);
	TestTrue(TEXT("warehouse-control fixture opens Warehouse"), Widget->IsWarehousePanelOpenForTest());
	TestNull(TEXT("Warehouse has no previous-page button"),
		Widget->WidgetTree->FindWidget(TEXT("WarehousePreviousButton")));
	TestNull(TEXT("Warehouse has no next-page button"),
		Widget->WidgetTree->FindWidget(TEXT("WarehouseNextButton")));
	TestNull(TEXT("Warehouse has no sort button"),
		Widget->WidgetTree->FindWidget(TEXT("WarehouseSortButton")));
	TestNotNull(TEXT("Warehouse-to-Backpack batch button exists"),
		Widget->WidgetTree->FindWidget(TEXT("WarehouseBatchToBackpackButton")));
	TestNotNull(TEXT("Backpack-to-Warehouse batch button exists"),
		Widget->WidgetTree->FindWidget(TEXT("BackpackBatchToWarehouseButton")));
	for (int32 Page = 0; Page < 6; ++Page)
	{
		TestNotNull(
			*FString::Printf(TEXT("Warehouse page tab %d exists"), Page + 1),
			Widget->WidgetTree->FindWidget(
				*FString::Printf(TEXT("WarehousePageTab_%d"), Page)));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingWarehouseBatchTransferTest,
	"GameXXK.DesktopTraining.Workbench.WarehouseBatchTransfer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingWarehouseBatchTransferTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem =
		NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestTrue(TEXT("warehouse-batch fixture starts a new game"),
		Subsystem && Subsystem->StartGame()))
	{
		return false;
	}
	FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
	State.Talents.NodeRanks.Add(TEXT("Talent.Root"), 1);
	State.Talents.NodeRanks.Add(TEXT("Talent.Entry.Tools"), 1);
	const FName LockedItemId(TEXT("Item.Test.WorkbenchBatch.Locked"));
	const FName CarryItemId(TEXT("Item.Test.WorkbenchBatch.Carried"));
	const FName OtherPageItemId(TEXT("Item.Test.WorkbenchBatch.OtherPage"));
	State.Inventory.Add(LockedItemId, 9);
	State.Inventory.Add(CarryItemId, 4);
	State.Inventory.Add(OtherPageItemId, 2);
	if (!TestTrue(TEXT("warehouse-batch fixture normalizes"),
		Subsystem->NormalizeDesktopInventoryState()))
	{
		return false;
	}
	const FGameXXKDesktopInventoryEntryKey LockedItem =
		FGameXXKDesktopInventoryRules::MakeItemEntry(LockedItemId);
	const FGameXXKDesktopInventoryEntryKey CarryItem =
		FGameXXKDesktopInventoryRules::MakeItemEntry(CarryItemId);
	const FGameXXKDesktopInventoryEntryKey OtherPageItem =
		FGameXXKDesktopInventoryRules::MakeItemEntry(OtherPageItemId);
	FString Error;
	TestTrue(TEXT("warehouse-batch fixture locks one movable item"),
		FGameXXKDesktopInventoryRules::SetEntryLocked(State, LockedItem, true, &Error));
	const int32 OtherPageSource = FGameXXKDesktopInventoryRules::FindEntrySlot(
		State, EGameXXKDesktopItemContainer::Backpack, OtherPageItem);
	TestTrue(TEXT("warehouse-batch fixture places an entry on page two"),
		FGameXXKDesktopInventoryRules::MoveEntry(
			State,
			EGameXXKDesktopItemContainer::Backpack,
			OtherPageSource,
			EGameXXKDesktopItemContainer::Warehouse,
			40,
			&Error));

	UGameXXKDesktopTrainingWorkbenchWidget* Widget =
		NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	Widget->SetMVPSubsystem(Subsystem);
	Widget->ConstructForTest();
	if (!TestTrue(TEXT("warehouse-batch fixture opens Backpack"), Widget->OpenBackpack()))
	{
		return false;
	}
	Widget->HandleActionClicked(3);
	const int32 ReservedEquipmentSlot = Widget->FindFirstBackpackEquipmentSlotForTest();
	if (!TestTrue(TEXT("warehouse-batch fixture picks one equipment reservation"),
		ReservedEquipmentSlot != INDEX_NONE
		&& Widget->PickUpBackpackSlotForTest(ReservedEquipmentSlot)))
	{
		return false;
	}
	const FGameXXKDesktopInventoryEntryKey ReservedEquipment =
		FGameXXKDesktopInventoryRules::GetEntryAt(
			State, EGameXXKDesktopItemContainer::Backpack, ReservedEquipmentSlot);
	Widget->HandleActionClicked(300);
	TestEqual(TEXT("one Tool reservation is held outside authoritative slots"),
		Widget->GetOccupiedToolSlotCountForTest(), 1);
	Widget->HandleActionClicked(0);
	const int32 CarrySlot = Widget->FindBackpackItemSlotForTest(CarryItemId);
	TestTrue(TEXT("warehouse-batch fixture carries one excluded item"),
		CarrySlot != INDEX_NONE && Widget->PickUpBackpackSlotForTest(CarrySlot));
	TestEqual(TEXT("batch starts on Warehouse page one"), Widget->GetWarehousePageIndexForTest(), 0);

	Widget->HandleActionClicked(41);
	TestTrue(TEXT("locked item moves Backpack to current Warehouse page"),
		FGameXXKDesktopInventoryRules::FindEntrySlot(
			State, EGameXXKDesktopItemContainer::Warehouse, LockedItem) >= 0
		&& FGameXXKDesktopInventoryRules::FindEntrySlot(
			State, EGameXXKDesktopItemContainer::Warehouse, LockedItem) < 36);
	TestTrue(TEXT("moved locked item remains locked"),
		FGameXXKDesktopInventoryRules::IsEntryLocked(State, LockedItem));
	TestTrue(TEXT("carried item remains in Backpack and on cursor"),
		Widget->IsCarryingItemForTest()
		&& FGameXXKDesktopInventoryRules::FindEntrySlot(
			State, EGameXXKDesktopItemContainer::Backpack, CarryItem) != INDEX_NONE);
	TestTrue(TEXT("Tool reservation remains in Backpack"),
		FGameXXKDesktopInventoryRules::FindEntrySlot(
			State, EGameXXKDesktopItemContainer::Backpack, ReservedEquipment) != INDEX_NONE);
	TestEqual(TEXT("batch transfer keeps current Warehouse page unchanged"),
		Widget->GetWarehousePageIndexForTest(), 0);
	TestTrue(TEXT("batch transfer reports one movement notice"),
		Widget->GetLastDesktopInventoryNoticeForTest().ToString().StartsWith(TEXT("已移动 ")));

	UButton* ToWarehouseButton = Cast<UButton>(
		Widget->WidgetTree->FindWidget(TEXT("BackpackBatchToWarehouseButton")));
	UButton* ToBackpackButton = Cast<UButton>(
		Widget->WidgetTree->FindWidget(TEXT("WarehouseBatchToBackpackButton")));
	TestTrue(TEXT("exhausted Backpack direction disables live without closing Warehouse"),
		ToWarehouseButton && !ToWarehouseButton->GetIsEnabled());
	TestTrue(TEXT("reverse Warehouse direction enables live"),
		ToBackpackButton && ToBackpackButton->GetIsEnabled());

	TestTrue(TEXT("carried exclusion can be cancelled before reverse batch"),
		Widget->CancelCarriedItemForTest());
	Widget->HandleActionClicked(40);
	TestTrue(TEXT("locked item moves back to Backpack"),
		FGameXXKDesktopInventoryRules::FindEntrySlot(
			State, EGameXXKDesktopItemContainer::Backpack, LockedItem) != INDEX_NONE);
	TestTrue(TEXT("lock remains after reverse batch"),
		FGameXXKDesktopInventoryRules::IsEntryLocked(State, LockedItem));
	TestEqual(TEXT("other Warehouse page entry remains untouched"),
		FGameXXKDesktopInventoryRules::FindEntrySlot(
			State, EGameXXKDesktopItemContainer::Warehouse, OtherPageItem), 40);
	TestEqual(TEXT("reverse batch also keeps current Warehouse page unchanged"),
		Widget->GetWarehousePageIndexForTest(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingWarehouseBatchPartialTest,
	"GameXXK.DesktopTraining.Workbench.WarehouseBatchTransferPartialNotice",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingWarehouseBatchPartialTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem =
		NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestTrue(TEXT("partial-batch fixture starts a new game"),
		Subsystem && Subsystem->StartGame()))
	{
		return false;
	}
	FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
	State.Talents.NodeRanks.Add(TEXT("Talent.Root"), 1);
	State.DesktopInventory.WarehouseSlots.SetNum(
		FGameXXKDesktopInventoryRules::WarehouseCapacity);
	for (int32 Index = 0; Index < 35; ++Index)
	{
		const FName ItemId(*FString::Printf(TEXT("Item.Test.WorkbenchBatch.Full.%02d"), Index));
		State.DesktopInventory.WarehouseItems.Add(ItemId, 1);
		State.DesktopInventory.WarehouseSlots[Index] =
			FGameXXKDesktopInventoryRules::MakeItemEntry(ItemId);
	}
	if (!TestTrue(TEXT("partial-batch fixture normalizes one free page slot"),
		Subsystem->NormalizeDesktopInventoryState()))
	{
		return false;
	}

	UGameXXKDesktopTrainingWorkbenchWidget* Widget =
		NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	Widget->SetMVPSubsystem(Subsystem);
	Widget->ConstructForTest();
	TestTrue(TEXT("partial-batch fixture opens Backpack"), Widget->OpenBackpack());
	Widget->HandleActionClicked(0);
	Widget->HandleActionClicked(41);
	const FString Notice = Widget->GetLastDesktopInventoryNoticeForTest().ToString();
	TestTrue(TEXT("partial batch notice reports one moved entry"),
		Notice.Contains(TEXT("已移动 1 件")));
	TestTrue(TEXT("partial batch notice reports insufficient destination space"),
		Notice.Contains(TEXT("目标空间不足")));
	TestEqual(TEXT("partial batch fills only the selected Warehouse page"),
		FGameXXKDesktopInventoryRules::GetOccupiedSlotCount(
			State, EGameXXKDesktopItemContainer::Warehouse), 36);
	return true;
}

#endif
