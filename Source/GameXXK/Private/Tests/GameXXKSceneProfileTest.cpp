#include "Misc/AutomationTest.h"

#include "Narrative/GameXXKCharacterCatalog.h"
#include "Narrative/GameXXKSceneProfile.h"
#include "Narrative/GameXXKSceneRegistry.h"
#include "Narrative/GameXXKStageContract.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKSceneProfileTestPrivate
{
	const TArray<FName>& RequiredSlots()
	{
		static const TArray<FName> Slots = {
			TEXT("Tutorial.River.CarriageEntry"),
			TEXT("Tutorial.River.CarriageStop"),
			TEXT("Tutorial.River.HeroSpawn"),
			TEXT("Tutorial.River.CarriageExit"),
			TEXT("Tutorial.River.ScrollSpawn"),
			TEXT("Tutorial.River.YueBaiSpawn"),
			TEXT("Tutorial.River.YueBaiAdvance"),
			TEXT("Tutorial.River.CameraOverview"),
			TEXT("Tutorial.River.EncounterTrigger"),
			TEXT("Tutorial.River.TownRelease")};
		return Slots;
	}

	UGameXXKStageContract* MakeContract()
	{
		UGameXXKStageContract* Contract = NewObject<UGameXXKStageContract>();
		Contract->StageContractId = TEXT("Stage.Tutorial.River");
		Contract->RequiredSlotIds.Append(RequiredSlots());
		return Contract;
	}

	UGameXXKCharacterCatalog* MakeCharacters()
	{
		UGameXXKCharacterCatalog* Catalog = NewObject<UGameXXKCharacterCatalog>();
		FGameXXKCharacterDefinition Hero;
		Hero.CharacterId = TEXT("Character.Hero");
		Catalog->Characters.Add(Hero);
		FGameXXKCharacterDefinition YueBai;
		YueBai.CharacterId = TEXT("Npc.YueBai");
		Catalog->Characters.Add(YueBai);
		return Catalog;
	}

	UGameXXKSceneProfile* MakeProfile(
		const FName ProfileId,
		const TCHAR* MapPath,
		const double Offset)
	{
		UGameXXKSceneProfile* Profile = NewObject<UGameXXKSceneProfile>();
		Profile->SceneProfileId = ProfileId;
		Profile->StageContractId = TEXT("Stage.Tutorial.River");
		Profile->MapPath = FSoftObjectPath(MapPath);
		Profile->SceneRootTag = TEXT("GameXXK_StageRoot_TutorialRiver");
		Profile->SafeSlotId = TEXT("Tutorial.River.TownRelease");
		int32 Index = 0;
		for (const FName SlotId : RequiredSlots())
		{
			FGameXXKSceneSlotBinding Binding;
			Binding.SlotId = SlotId;
			Binding.RelativeTransform.SetLocation(FVector(Offset + Index * 10.0, 0.0, 0.0));
			Profile->SlotBindings.Add(Binding);
			++Index;
		}
		FGameXXKNpcScenePlacement Placement;
		Placement.CharacterId = TEXT("Npc.YueBai");
		Placement.HomeSlotId = TEXT("Tutorial.River.YueBaiSpawn");
		Placement.InteractionAnchorSlotId = TEXT("Tutorial.River.YueBaiAdvance");
		Placement.PatrolRegionId = TEXT("Region.Tutorial.River");
		Profile->NpcPlacements.Add(Placement);
		return Profile;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKSceneProfileValidationAndSwapTest,
	"GameXXK.Narrative.SceneProfile.ValidationAndSwap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKSceneProfileValidationAndSwapTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKSceneProfileTestPrivate;
	UGameXXKStageContract* Contract = MakeContract();
	UGameXXKCharacterCatalog* Characters = MakeCharacters();
	UGameXXKSceneProfile* Incomplete = MakeProfile(
		TEXT("SceneProfile.Test.Incomplete"),
		TEXT("/Game/Test/MapA.MapA"),
		100.0);
	Incomplete->SlotBindings.RemoveAt(0);
	FString Error;
	TestFalse(TEXT("incomplete profile rejects"),
		Incomplete->ValidateAgainstContract(*Contract, Characters, &Error));
	TestTrue(TEXT("missing slot is diagnosed"), Error.Contains(TEXT("CarriageEntry")));

	UGameXXKSceneProfile* ProfileA = MakeProfile(
		TEXT("SceneProfile.Test.A"),
		TEXT("/Game/Test/MapA.MapA"),
		100.0);
	TestTrue(TEXT("complete profile A validates"),
		ProfileA->ValidateAgainstContract(*Contract, Characters, &Error));
	UGameXXKSceneRegistry* Registry = NewObject<UGameXXKSceneRegistry>();
	TestFalse(TEXT("map mismatch rejects registration"),
		Registry->RegisterActiveProfile(
			*Contract,
			*ProfileA,
			Characters,
			FSoftObjectPath(TEXT("/Game/Test/MapB.MapB")),
			&Error));
	TestTrue(TEXT("matching map registers profile A"),
		Registry->RegisterActiveProfile(
			*Contract,
			*ProfileA,
			Characters,
			FSoftObjectPath(TEXT("/Game/Test/MapA.MapA")),
			&Error));
	TestEqual(TEXT("profile A resolves"),
		Registry->ResolveProfile(TEXT("Stage.Tutorial.River")), ProfileA);

	UGameXXKSceneProfile* ProfileB = MakeProfile(
		TEXT("SceneProfile.Test.B"),
		TEXT("/Game/Test/MapB.MapB"),
		500.0);
	TestTrue(TEXT("profile B swaps into same contract"),
		Registry->RegisterActiveProfile(
			*Contract,
			*ProfileB,
			Characters,
			FSoftObjectPath(TEXT("/Game/Test/MapB.MapB")),
			&Error));
	TestEqual(TEXT("profile B now resolves"),
		Registry->ResolveProfile(TEXT("Stage.Tutorial.River")), ProfileB);

	const UScriptStruct* ProfileStruct = FGameXXKSceneSlotBinding::StaticStruct();
	TestNotNull(TEXT("scene binding owns relative transform"),
		ProfileStruct->FindPropertyByName(TEXT("RelativeTransform")));
	for (const FName Forbidden : {FName(TEXT("StoryId")), FName(TEXT("TaskId")), FName(TEXT("DialogueId"))})
	{
		TestNull(
			FString::Printf(TEXT("scene binding does not own %s"), *Forbidden.ToString()),
			ProfileStruct->FindPropertyByName(Forbidden));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKSceneProfileSafeResolutionTest,
	"GameXXK.Narrative.SceneProfile.SafeResolution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKSceneProfileSafeResolutionTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKSceneProfileTestPrivate;
	UGameXXKSceneProfile* Profile = MakeProfile(
		TEXT("SceneProfile.Test.Resolve"),
		TEXT("/Game/Test/MapA.MapA"),
		100.0);
	const FTransform Root(FRotator::ZeroRotator, FVector(1000.0, 50.0, 25.0));
	FTransform Resolved;
	FString Error;
	TestTrue(TEXT("known slot resolves"),
		Profile->ResolveWorldTransform(
			TEXT("Tutorial.River.CarriageEntry"), Root, Resolved, &Error));
	TestEqual(TEXT("known relative location composes with root"),
		Resolved.GetLocation(), FVector(1100.0, 50.0, 25.0));

	FTransform SafeFallback;
	TestFalse(TEXT("missing slot reports failure"),
		Profile->ResolveWorldTransform(TEXT("Tutorial.River.Missing"), Root, SafeFallback, &Error));
	TestTrue(TEXT("missing slot error is explicit"), Error.Contains(TEXT("Tutorial.River.Missing")));
	TestNotEqual(TEXT("missing slot never returns zero transform"),
		SafeFallback.GetLocation(), FVector::ZeroVector);
	FTransform ExpectedSafe;
	TestTrue(TEXT("safe slot resolves"),
		Profile->ResolveWorldTransform(Profile->SafeSlotId, Root, ExpectedSafe, nullptr));
	TestEqual(TEXT("missing slot returns configured safe transform"),
		SafeFallback.GetLocation(), ExpectedSafe.GetLocation());
	return true;
}

#endif
