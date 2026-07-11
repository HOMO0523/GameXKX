#include "GameXXKTownPCGAutomationLibrary.h"

#include "ActorFactories/ActorFactory.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Builders/CubeBuilder.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SplineComponent.h"
#include "Containers/StringConv.h"
#include "Editor.h"
#include "Elements/PCGCreatePoints.h"
#include "Elements/PCGStaticMeshSpawner.h"
#include "Engine/StaticMesh.h"
#include "Engine/Level.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "HAL/FileManager.h"
#include "Helpers/PCGHelpers.h"
#include "LevelUtils.h"
#include "MeshSelectors/PCGMeshSelectorWeighted.h"
#include "Materials/MaterialInterface.h"
#include "Misc/Crc.h"
#include "Misc/PackageName.h"
#include "PCGComponent.h"
#include "PCGEdge.h"
#include "PCGGraph.h"
#include "PCGNode.h"
#include "PCGPoint.h"
#include "PCGPin.h"
#include "PCGVolume.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "ScopedTransaction.h"
#include "Subsystems/PCGSubsystem.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

#include <openssl/sha.h>

namespace GameXXKTownPCGAutomation
{
	constexpr TCHAR VerticalSliceGraphRoot[] = TEXT("/Game/GameXXK/Environment/TownPCG/VerticalSlice/");
	constexpr TCHAR B0RGraphRoot[] = TEXT("/Game/GameXXK/Environment/TownPCG/B0R/");
	constexpr TCHAR B1GraphRoot[] = TEXT("/Game/GameXXK/Environment/TownPCG/B1/");
	constexpr TCHAR B1MapPackage[] = TEXT("/Game/GameXXK/Maps/Dev/L_Qingshan_PCG_Dress_B1");
	constexpr TCHAR PrototypeMapRoot[] = TEXT("/Game/GameXXK/Maps/Prototype/");
	constexpr TCHAR B0RMapRoot[] = TEXT("/Game/GameXXK/Maps/Dev/");
	const FName PrototypeOnlyTag(TEXT("PrototypeOnly"));

	bool IsManagedGraphPath(const FString& Path)
	{
		return Path.StartsWith(VerticalSliceGraphRoot, ESearchCase::CaseSensitive)
			|| Path.StartsWith(B0RGraphRoot, ESearchCase::CaseSensitive)
			|| Path.StartsWith(B1GraphRoot, ESearchCase::CaseSensitive);
	}

	bool IsB1GraphPath(const FString& Path)
	{
		return Path.StartsWith(B1GraphRoot, ESearchCase::CaseSensitive);
	}

	bool IsManagedPrototypeMapPath(const FString& Path)
	{
		return Path.StartsWith(PrototypeMapRoot, ESearchCase::CaseSensitive)
			|| Path.StartsWith(B0RMapRoot, ESearchCase::CaseSensitive);
	}

	class FRevertibleEditorTransaction
	{
	public:
		explicit FRevertibleEditorTransaction(const FText& Description)
		{
			if (GEditor && !GEditor->IsTransactionActive())
			{
				Transaction = MakeUnique<FScopedTransaction>(Description);
				bCanRollback = Transaction->IsOutstanding();
			}
		}

		~FRevertibleEditorTransaction()
		{
			if (!bFinished)
			{
				Rollback();
			}
		}

		bool IsValid() const
		{
			return bCanRollback;
		}

		void Commit()
		{
			Transaction.Reset();
			bFinished = true;
		}

		void Rollback()
		{
			if (bFinished)
			{
				return;
			}
			Transaction.Reset();
			if (bCanRollback && GEditor)
			{
				GEditor->UndoTransaction(false);
			}
			bFinished = true;
		}

	private:
		TUniquePtr<FScopedTransaction> Transaction;
		bool bCanRollback = false;
		bool bFinished = false;
	};

	FString ObjectPathForPackage(const FString& LongPackageName)
	{
		return FString::Printf(TEXT("%s.%s"), *LongPackageName, *FPackageName::GetLongPackageAssetName(LongPackageName));
	}

	template <typename AssetType>
	AssetType* LoadAssetByPackagePath(const FString& LongPackageName)
	{
		return LoadObject<AssetType>(nullptr, *ObjectPathForPackage(LongPackageName));
	}

	TSharedRef<FJsonObject> MakeResult(const bool bSuccess, const TCHAR* Operation)
	{
		TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetBoolField(TEXT("success"), bSuccess);
		Result->SetStringField(TEXT("operation"), Operation);
		return Result;
	}

	FString ToJson(const TSharedRef<FJsonObject>& Object)
	{
		FString Output;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Output);
		FJsonSerializer::Serialize(Object, Writer);
		return Output;
	}

	FString ErrorJson(const TCHAR* Operation, const FString& Error, const FString& ActorLabel = FString(), const FString& Graph = FString())
	{
		TSharedRef<FJsonObject> Result = MakeResult(false, Operation);
		if (!ActorLabel.IsEmpty())
		{
			Result->SetStringField(TEXT("actor_label"), ActorLabel);
		}
		if (!Graph.IsEmpty())
		{
			Result->SetStringField(TEXT("graph"), Graph);
		}
		Result->SetStringField(TEXT("error"), Error);
		return ToJson(Result);
	}

	UWorld* GetEditorWorld()
	{
		return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	}

	TArray<AActor*> FindActorsByExactLabel(UWorld* World, const FString& ActorLabel)
	{
		TArray<AActor*> Matches;
		if (!World)
		{
			return Matches;
		}

		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (IsValid(*It) && It->GetActorLabel() == ActorLabel)
			{
				Matches.Add(*It);
			}
		}
		return Matches;
	}

	bool ValidateActorLabel(const FString& ActorLabel, FString& OutError)
	{
		if (ActorLabel.IsEmpty() || ActorLabel.TrimStartAndEnd() != ActorLabel)
		{
			OutError = TEXT("actor label must be nonblank and trimmed");
			return false;
		}
		return true;
	}

	bool IsFiniteVector(const FVector& Vector)
	{
		return FMath::IsFinite(Vector.X) && FMath::IsFinite(Vector.Y) && FMath::IsFinite(Vector.Z);
	}

	bool IsExistingPackageFileReadOnly(const FString& LongPackageName, const FString& Extension)
	{
		const FString Filename = FPackageName::LongPackageNameToFilename(LongPackageName, Extension);
		return IFileManager::Get().FileExists(*Filename) && IFileManager::Get().IsReadOnly(*Filename);
	}

	bool VerifyDirectedEdge(
		const UPCGNode* SourceNode,
		const FName SourcePinLabel,
		const UPCGNode* DestinationNode,
		const FName DestinationPinLabel)
	{
		if (!SourceNode || !DestinationNode)
		{
			return false;
		}
		const UPCGPin* SourcePin = SourceNode->GetOutputPin(SourcePinLabel);
		const UPCGPin* DestinationPin = DestinationNode->GetInputPin(DestinationPinLabel);
		if (!SourcePin || !DestinationPin)
		{
			return false;
		}

		return SourcePin->Edges.ContainsByPredicate(
			[SourcePin, DestinationPin](const UPCGEdge* Edge)
			{
				return Edge && Edge->IsValid() && Edge->InputPin == SourcePin && Edge->OutputPin == DestinationPin;
			});
	}

	bool ValidatePrototypeReadContext(UWorld* World, const AActor* ActorToInspect, FString& OutError)
	{
		if (!World)
		{
			OutError = TEXT("no editor world is available");
			return false;
		}
		ULevel* CurrentLevel = World->GetCurrentLevel();
		if (!CurrentLevel || CurrentLevel->GetWorld() != World)
		{
			OutError = TEXT("editor world has no valid current level");
			return false;
		}

		const FString WorldPackageName = World->GetOutermost()->GetName();
		const FString CurrentLevelPackageName = CurrentLevel->GetOutermost()->GetName();
		if (!IsManagedPrototypeMapPath(WorldPackageName))
		{
			OutError = TEXT("town PCG actor operations are restricted to managed prototype/Dev maps");
			return false;
		}
		if (!IsManagedPrototypeMapPath(CurrentLevelPackageName))
		{
			OutError = TEXT("current level is not a managed prototype/Dev map level");
			return false;
		}
		if (ActorToInspect && ActorToInspect->GetLevel() != CurrentLevel)
		{
			OutError = TEXT("the exact-label actor belongs to a different level");
			return false;
		}
		return true;
	}

	bool ValidatePrototypeMutationContext(UWorld* World, const AActor* ActorToMutate, FString& OutError)
	{
		if (!ValidatePrototypeReadContext(World, ActorToMutate, OutError))
		{
			return false;
		}
		ULevel* CurrentLevel = World->GetCurrentLevel();
		if (FLevelUtils::IsLevelLocked(CurrentLevel))
		{
			OutError = TEXT("current editor level is locked");
			return false;
		}
		if (IsExistingPackageFileReadOnly(CurrentLevel->GetOutermost()->GetName(), FPackageName::GetMapPackageExtension()))
		{
			OutError = TEXT("current editor map package is read-only");
			return false;
		}
		return true;
	}

	bool ValidateB1MutationContext(UWorld* World, const AActor* ActorToMutate, FString& OutError)
	{
		if (!ValidatePrototypeMutationContext(World, ActorToMutate, OutError))
		{
			return false;
		}
		if (World->GetOutermost()->GetName() != B1MapPackage
			|| World->GetCurrentLevel()->GetOutermost()->GetName() != B1MapPackage)
		{
			OutError = FString::Printf(
				TEXT("advanced town PCG mutation requires exact B1 world/current level '%s'"),
				B1MapPackage);
			return false;
		}
		return true;
	}

	bool ResolveManagedSplineSemanticTag(const TArray<FName>& Tags, FName& OutTag)
	{
		static const TSet<FName> AllowedTags = {
			FName(TEXT("Quick_Road_CityScope")),
			FName(TEXT("Quick_Road_MainRoad")),
			FName(TEXT("Quick_Road_RoadEdge")),
			FName(TEXT("TownPCG_River")),
		};
		for (const FName Tag : Tags)
		{
			if (AllowedTags.Contains(Tag))
			{
				OutTag = Tag;
				return true;
			}
		}
		return false;
	}

	APCGVolume* FindUniquePCGVolume(UWorld* World, const FString& ActorLabel, FString& OutError)
	{
		const TArray<AActor*> Matches = FindActorsByExactLabel(World, ActorLabel);
		if (Matches.Num() == 0)
		{
			OutError = FString::Printf(TEXT("no actor has the exact label '%s'"), *ActorLabel);
			return nullptr;
		}
		if (Matches.Num() > 1)
		{
			OutError = FString::Printf(TEXT("%d actors have the exact label '%s'; expected exactly one"), Matches.Num(), *ActorLabel);
			return nullptr;
		}

		APCGVolume* Volume = Cast<APCGVolume>(Matches[0]);
		if (!Volume)
		{
			OutError = FString::Printf(TEXT("actor '%s' is not a PCG volume"), *ActorLabel);
		}
		else if (!Volume->PCGComponent)
		{
			OutError = FString::Printf(TEXT("PCG volume '%s' has no PCG component"), *ActorLabel);
			Volume = nullptr;
		}
		return Volume;
	}

	int32 CountGeneratedInstancedComponents(const AActor* Actor)
	{
		if (!Actor)
		{
			return 0;
		}

		TInlineComponentArray<UInstancedStaticMeshComponent*> Components;
		Actor->GetComponents(Components);
		int32 Count = 0;
		for (const UInstancedStaticMeshComponent* Component : Components)
		{
			if (Component && Component->ComponentHasTag(PCGHelpers::DefaultPCGTag))
			{
				++Count;
			}
		}
		return Count;
	}

	bool IsLowercaseSha256(const FString& Value)
	{
		if (Value.Len() != 64)
		{
			return false;
		}
		for (const TCHAR Character : Value)
		{
			if (!((Character >= TEXT('0') && Character <= TEXT('9'))
				|| (Character >= TEXT('a') && Character <= TEXT('f'))))
			{
				return false;
			}
		}
		return true;
	}

	FString CanonicalTransformString(const FTransform& Transform)
	{
		FQuat Rotation = Transform.GetRotation().GetNormalized();
		const bool bNegate = Rotation.W < 0.0
			|| (FMath::IsNearlyZero(Rotation.W) && Rotation.X < 0.0)
			|| (FMath::IsNearlyZero(Rotation.W) && FMath::IsNearlyZero(Rotation.X) && Rotation.Y < 0.0)
			|| (FMath::IsNearlyZero(Rotation.W) && FMath::IsNearlyZero(Rotation.X)
				&& FMath::IsNearlyZero(Rotation.Y) && Rotation.Z < 0.0);
		if (bNegate)
		{
			Rotation.X = -Rotation.X;
			Rotation.Y = -Rotation.Y;
			Rotation.Z = -Rotation.Z;
			Rotation.W = -Rotation.W;
		}

		const FVector Location = Transform.GetLocation();
		const FVector Scale = Transform.GetScale3D();
		return FString::Printf(
			TEXT("%lld,%lld,%lld|%lld,%lld,%lld,%lld|%lld,%lld,%lld"),
			static_cast<long long>(FMath::RoundToInt64(Location.X * 1000.0)),
			static_cast<long long>(FMath::RoundToInt64(Location.Y * 1000.0)),
			static_cast<long long>(FMath::RoundToInt64(Location.Z * 1000.0)),
			static_cast<long long>(FMath::RoundToInt64(Rotation.X * 1000000.0)),
			static_cast<long long>(FMath::RoundToInt64(Rotation.Y * 1000000.0)),
			static_cast<long long>(FMath::RoundToInt64(Rotation.Z * 1000000.0)),
			static_cast<long long>(FMath::RoundToInt64(Rotation.W * 1000000.0)),
			static_cast<long long>(FMath::RoundToInt64(Scale.X * 1000000.0)),
			static_cast<long long>(FMath::RoundToInt64(Scale.Y * 1000000.0)),
			static_cast<long long>(FMath::RoundToInt64(Scale.Z * 1000000.0)));
	}

	int32 StableTransformHash(const FTransform& Transform)
	{
		return static_cast<int32>(FCrc::StrCrc32(*CanonicalTransformString(Transform)) & 0x7fffffffU);
	}

	FString Sha256ForCanonicalSeedRecords(const TArray<FString>& Records)
	{
		const FString Canonical = FString::Join(Records, TEXT("\n"));
		FTCHARToUTF8 Utf8(*Canonical);
		FSHA256Signature Signature{};
		SHA256_CTX Context;
		if (SHA256_Init(&Context) != 1
			|| (Utf8.Length() > 0 && SHA256_Update(&Context, Utf8.Get(), Utf8.Length()) != 1)
			|| SHA256_Final(Signature.Signature, &Context) != 1)
		{
			return FString();
		}
		return Signature.ToString().ToLower();
	}
}

FString UGameXXKTownPCGAutomationLibrary::CreateOrUpdateTaggedSpline(
	const FString& ActorLabel,
	const TArray<FVector>& WorldPoints,
	const bool bClosedLoop,
	const TArray<FName>& Tags)
{
	using namespace GameXXKTownPCGAutomation;
	constexpr TCHAR Operation[] = TEXT("CreateOrUpdateTaggedSpline");
	FString Error;
	if (!ValidateActorLabel(ActorLabel, Error))
	{
		return ErrorJson(Operation, Error, ActorLabel);
	}
	if (WorldPoints.Num() < 2)
	{
		return ErrorJson(Operation, TEXT("a spline requires at least two world points"), ActorLabel);
	}
	for (const FVector& Point : WorldPoints)
	{
		if (!IsFiniteVector(Point))
		{
			return ErrorJson(Operation, TEXT("spline world points must contain only finite values"), ActorLabel);
		}
	}

	TArray<FName> UniqueTags;
	for (const FName Tag : Tags)
	{
		if (Tag.IsNone())
		{
			return ErrorJson(Operation, TEXT("spline tags must not contain None"), ActorLabel);
		}
		UniqueTags.AddUnique(Tag);
	}
	UniqueTags.Sort([](const FName A, const FName B) { return A.LexicalLess(B); });
	FName ManagedSemanticTag;
	if (!UniqueTags.Contains(PrototypeOnlyTag) || !ResolveManagedSplineSemanticTag(UniqueTags, ManagedSemanticTag))
	{
		return ErrorJson(
			Operation,
			TEXT("spline tags must include PrototypeOnly and one supported managed semantic tag"),
			ActorLabel);
	}

	UWorld* World = GetEditorWorld();
	if (!ValidatePrototypeMutationContext(World, nullptr, Error))
	{
		return ErrorJson(Operation, Error, ActorLabel);
	}
	const TArray<AActor*> Matches = FindActorsByExactLabel(World, ActorLabel);
	if (Matches.Num() > 1)
	{
		return ErrorJson(
			Operation,
			FString::Printf(TEXT("%d actors have the exact label; expected at most one"), Matches.Num()),
			ActorLabel);
	}

	AActor* SplineActor = Matches.Num() == 1 ? Matches[0] : nullptr;
	USplineComponent* SplineComponent = nullptr;
	if (SplineActor)
	{
		if (!ValidatePrototypeMutationContext(World, SplineActor, Error))
		{
			return ErrorJson(Operation, Error, ActorLabel);
		}
		if (SplineActor->GetClass() != AActor::StaticClass())
		{
			return ErrorJson(Operation, TEXT("the exact-label actor is not a plain Actor spline owner"), ActorLabel);
		}
		if (!SplineActor->Tags.Contains(PrototypeOnlyTag) || !SplineActor->Tags.Contains(ManagedSemanticTag))
		{
			return ErrorJson(
				Operation,
				TEXT("existing spline actor is not owned by town PCG assembly for the requested semantic tag"),
				ActorLabel);
		}
		TInlineComponentArray<USplineComponent*> SplineComponents;
		SplineActor->GetComponents(SplineComponents);
		if (SplineComponents.Num() != 1)
		{
			return ErrorJson(
				Operation,
				FString::Printf(TEXT("the exact-label actor owns %d spline components; expected exactly one"), SplineComponents.Num()),
				ActorLabel);
		}
		SplineComponent = SplineComponents[0];
		if (!SplineComponent || SplineComponent->GetOwner() != SplineActor)
		{
			return ErrorJson(Operation, TEXT("the spline component is not owned by the exact-label actor"), ActorLabel);
		}
	}

	FRevertibleEditorTransaction Transaction(NSLOCTEXT("GameXXK", "CreateOrUpdateTaggedSpline", "Create Or Update Tagged Spline"));
	if (!Transaction.IsValid())
	{
		return ErrorJson(Operation, TEXT("could not start an isolated editor transaction"), ActorLabel);
	}
	if (!SplineActor)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.ObjectFlags |= RF_Transactional;
		SpawnParameters.OverrideLevel = World->GetCurrentLevel();
		SplineActor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, SpawnParameters);
		if (!SplineActor)
		{
			return ErrorJson(Operation, TEXT("failed to spawn spline owner actor"), ActorLabel);
		}
		SplineActor->SetActorLabel(ActorLabel);
		SplineComponent = NewObject<USplineComponent>(
			SplineActor,
			USplineComponent::StaticClass(),
			TEXT("SplineComponent"),
			RF_Transactional);
		if (!SplineComponent)
		{
			return ErrorJson(Operation, TEXT("failed to create spline component"), ActorLabel);
		}
		SplineComponent->CreationMethod = EComponentCreationMethod::Instance;
		SplineComponent->SetMobility(EComponentMobility::Static);
		SplineComponent->SetRelativeTransform(FTransform::Identity);
		SplineActor->SetRootComponent(SplineComponent);
		SplineActor->AddInstanceComponent(SplineComponent);
		SplineComponent->RegisterComponent();
	}

	SplineActor->Modify();
	SplineComponent->Modify();
	SplineActor->SetActorTransform(FTransform::Identity);
	SplineActor->Tags = UniqueTags;
	SplineComponent->ComponentTags = UniqueTags;
	SplineComponent->ClearSplinePoints(false);
	SplineComponent->SetSplinePoints(WorldPoints, ESplineCoordinateSpace::World, false);
	SplineComponent->SetClosedLoop(bClosedLoop, false);
	SplineComponent->UpdateSpline();
	SplineActor->MarkPackageDirty();
	Transaction.Commit();

	TArray<TSharedPtr<FJsonValue>> JsonTags;
	for (const FName Tag : UniqueTags)
	{
		JsonTags.Add(MakeShared<FJsonValueString>(Tag.ToString()));
	}
	TSharedRef<FJsonObject> Result = MakeResult(true, Operation);
	Result->SetStringField(TEXT("label"), ActorLabel);
	Result->SetNumberField(TEXT("point_count"), WorldPoints.Num());
	Result->SetBoolField(TEXT("closed"), bClosedLoop);
	Result->SetArrayField(TEXT("tags"), JsonTags);
	Result->SetStringField(TEXT("error"), TEXT(""));
	return ToJson(Result);
}

FString UGameXXKTownPCGAutomationLibrary::CreateOrUpdateTownPCGGraph(
	const FString& GraphAssetPath,
	const FString& StaticMeshPath,
	const TArray<FTransform>& BuildingTransforms)
{
	using namespace GameXXKTownPCGAutomation;
	constexpr TCHAR Operation[] = TEXT("CreateOrUpdateTownPCGGraph");

	if (!FPackageName::IsValidLongPackageName(GraphAssetPath, true))
	{
		return ErrorJson(Operation, TEXT("graph must be a valid long package name"), FString(), GraphAssetPath);
	}
	if (!IsManagedGraphPath(GraphAssetPath))
	{
		return ErrorJson(Operation, TEXT("graph path must be under the managed town PCG graph roots"), FString(), GraphAssetPath);
	}
	if (IsExistingPackageFileReadOnly(GraphAssetPath, FPackageName::GetAssetPackageExtension()))
	{
		return ErrorJson(Operation, TEXT("graph package is read-only"), FString(), GraphAssetPath);
	}
	if (!FPackageName::IsValidLongPackageName(StaticMeshPath, true))
	{
		return ErrorJson(Operation, TEXT("static mesh must be a valid long package name"), FString(), GraphAssetPath);
	}
	for (const FTransform& Transform : BuildingTransforms)
	{
		if (Transform.ContainsNaN())
		{
			return ErrorJson(Operation, TEXT("building transforms must contain only finite values"), FString(), GraphAssetPath);
		}
	}

	UStaticMesh* StaticMesh = LoadAssetByPackagePath<UStaticMesh>(StaticMeshPath);
	if (!StaticMesh)
	{
		return ErrorJson(Operation, FString::Printf(TEXT("could not load static mesh '%s'"), *StaticMeshPath), FString(), GraphAssetPath);
	}

	UObject* ExistingGraphObject = LoadAssetByPackagePath<UObject>(GraphAssetPath);
	if (ExistingGraphObject && !ExistingGraphObject->IsA<UPCGGraph>())
	{
		return ErrorJson(
			Operation,
			FString::Printf(
				TEXT("requested graph object exists but is not a PCG graph (class '%s')"),
				*ExistingGraphObject->GetClass()->GetPathName()),
			FString(),
			GraphAssetPath);
	}
	UPCGGraph* Graph = Cast<UPCGGraph>(ExistingGraphObject);
	FRevertibleEditorTransaction Transaction(NSLOCTEXT("GameXXK", "AuthorTownPCGGraph", "Author Town PCG Graph"));
	if (!Transaction.IsValid())
	{
		return ErrorJson(Operation, TEXT("could not start an isolated editor transaction"), FString(), GraphAssetPath);
	}
	bool bCreated = false;
	if (!Graph)
	{
		UPackage* Package = CreatePackage(*GraphAssetPath);
		if (!Package)
		{
			return ErrorJson(Operation, TEXT("could not create graph package"), FString(), GraphAssetPath);
		}
		Package->FullyLoad();
		const FName AssetName(*FPackageName::GetLongPackageAssetName(GraphAssetPath));
		UObject* ExistingNamedObject = FindObject<UObject>(Package, *AssetName.ToString());
		if (ExistingNamedObject && !ExistingNamedObject->IsA<UPCGGraph>())
		{
			return ErrorJson(
				Operation,
				FString::Printf(
					TEXT("requested graph object exists but is not a PCG graph (class '%s')"),
					*ExistingNamedObject->GetClass()->GetPathName()),
				FString(),
				GraphAssetPath);
		}
		Graph = Cast<UPCGGraph>(ExistingNamedObject);
		if (!Graph)
		{
			Graph = NewObject<UPCGGraph>(Package, AssetName, RF_Public | RF_Standalone | RF_Transactional);
			bCreated = Graph != nullptr;
		}
	}
	if (!Graph)
	{
		return ErrorJson(Operation, TEXT("could not create or load graph"), FString(), GraphAssetPath);
	}

	Graph->Modify();
	TArray<UPCGNode*> AuthoredNodes = Graph->GetNodes();
	Graph->RemoveNodes(AuthoredNodes);

	UPCGCreatePointsSettings* CreatePointsSettings = nullptr;
	UPCGNode* CreatePointsNode = Graph->AddNodeOfType<UPCGCreatePointsSettings>(CreatePointsSettings);
	UPCGStaticMeshSpawnerSettings* SpawnerSettings = nullptr;
	UPCGNode* SpawnerNode = Graph->AddNodeOfType<UPCGStaticMeshSpawnerSettings>(SpawnerSettings);
	if (!CreatePointsNode || !CreatePointsSettings || !SpawnerNode || !SpawnerSettings)
	{
		return ErrorJson(Operation, TEXT("could not create PCG graph nodes"), FString(), GraphAssetPath);
	}

	CreatePointsSettings->Modify();
	CreatePointsSettings->CoordinateSpace = EPCGCoordinateSpace::World;
	CreatePointsSettings->bCullPointsOutsideVolume = false;
	CreatePointsSettings->PointsToCreate.Reset(BuildingTransforms.Num());
	for (int32 Index = 0; Index < BuildingTransforms.Num(); ++Index)
	{
		CreatePointsSettings->PointsToCreate.Emplace(BuildingTransforms[Index], 1.0f, Index + 1);
	}

	SpawnerSettings->Modify();
	SpawnerSettings->SetMeshSelectorType(UPCGMeshSelectorWeighted::StaticClass());
	UPCGMeshSelectorWeighted* WeightedSelector = Cast<UPCGMeshSelectorWeighted>(SpawnerSettings->MeshSelectorParameters);
	if (!WeightedSelector)
	{
		return ErrorJson(Operation, TEXT("could not initialize weighted mesh selector"), FString(), GraphAssetPath);
	}
	WeightedSelector->Modify();
	WeightedSelector->MeshEntries = {FPCGMeshSelectorWeightedEntry(TSoftObjectPtr<UStaticMesh>(StaticMesh), 1)};
	SpawnerSettings->bAllowMergeDifferentDataInSameInstancedComponents = true;

	Graph->AddLabeledEdge(CreatePointsNode, PCGPinConstants::DefaultOutputLabel, SpawnerNode, PCGPinConstants::DefaultInputLabel);
	Graph->AddLabeledEdge(SpawnerNode, PCGPinConstants::DefaultOutputLabel, Graph->GetOutputNode(), PCGPinConstants::DefaultOutputLabel);
	const bool bCreateToSpawnerVerified = VerifyDirectedEdge(
		CreatePointsNode,
		PCGPinConstants::DefaultOutputLabel,
		SpawnerNode,
		PCGPinConstants::DefaultInputLabel);
	const bool bSpawnerToOutputVerified = VerifyDirectedEdge(
		SpawnerNode,
		PCGPinConstants::DefaultOutputLabel,
		Graph->GetOutputNode(),
		PCGPinConstants::DefaultOutputLabel);
	if (!bCreateToSpawnerVerified || !bSpawnerToOutputVerified)
	{
		return ErrorJson(Operation, TEXT("could not connect PCG graph nodes"), FString(), GraphAssetPath);
	}
	Graph->MarkPackageDirty();
	if (bCreated)
	{
		FAssetRegistryModule::AssetCreated(Graph);
	}

	const FString PackageFilename = FPackageName::LongPackageNameToFilename(GraphAssetPath, FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.SaveFlags = SAVE_NoError;
	if (!UPackage::SavePackage(Graph->GetOutermost(), Graph, *PackageFilename, SaveArgs))
	{
		if (bCreated)
		{
			FAssetRegistryModule::AssetDeleted(Graph);
		}
		return ErrorJson(Operation, TEXT("failed to save graph package"), FString(), GraphAssetPath);
	}
	Transaction.Commit();

	TSharedRef<FJsonObject> Result = MakeResult(true, Operation);
	Result->SetStringField(TEXT("graph"), GraphAssetPath);
	Result->SetNumberField(TEXT("point_count"), BuildingTransforms.Num());
	Result->SetNumberField(TEXT("node_count"), Graph->GetNodes().Num());
	Result->SetNumberField(TEXT("verified_edge_count"), 2);
	Result->SetStringField(TEXT("error"), TEXT(""));
	return ToJson(Result);
}

FString UGameXXKTownPCGAutomationLibrary::CreateOrUpdateTownPCGGraphAdvanced(
	const FString& GraphAssetPath,
	const FString& StaticMeshPath,
	const TArray<FTransform>& PointTransforms,
	const int32 BaseSeed,
	const TArray<FString>& MaterialOverridePaths,
	const TArray<FString>& ConsumedRoadEdgeActorLabels,
	const FString& ConsumedRoadEdgeGeometryDigest,
	const float MinimumRoadClearanceCm)
{
	using namespace GameXXKTownPCGAutomation;
	constexpr TCHAR Operation[] = TEXT("CreateOrUpdateTownPCGGraphAdvanced");
	if (!FPackageName::IsValidLongPackageName(GraphAssetPath, true) || !IsB1GraphPath(GraphAssetPath))
	{
		return ErrorJson(Operation, TEXT("advanced graph path must be under the B1 town PCG graph root"), FString(), GraphAssetPath);
	}
	if (IsExistingPackageFileReadOnly(GraphAssetPath, FPackageName::GetAssetPackageExtension()))
	{
		return ErrorJson(Operation, TEXT("graph package is read-only"), FString(), GraphAssetPath);
	}
	if (!FPackageName::IsValidLongPackageName(StaticMeshPath, true))
	{
		return ErrorJson(Operation, TEXT("static mesh must be a valid long package name"), FString(), GraphAssetPath);
	}
	if (PointTransforms.Num() == 0)
	{
		return ErrorJson(Operation, TEXT("at least one point transform is required"), FString(), GraphAssetPath);
	}
	TSet<FString> CanonicalPointTransforms;
	for (const FTransform& Transform : PointTransforms)
	{
		if (Transform.ContainsNaN())
		{
			return ErrorJson(Operation, TEXT("point transforms must contain only finite values"), FString(), GraphAssetPath);
		}
		const FString Canonical = CanonicalTransformString(Transform);
		if (CanonicalPointTransforms.Contains(Canonical))
		{
			return ErrorJson(Operation, TEXT("duplicate canonical point transform is not allowed"), FString(), GraphAssetPath);
		}
		CanonicalPointTransforms.Add(Canonical);
	}
	if (!FMath::IsFinite(MinimumRoadClearanceCm) || MinimumRoadClearanceCm < 0.0f)
	{
		return ErrorJson(Operation, TEXT("minimum road clearance must be finite and nonnegative"), FString(), GraphAssetPath);
	}
	if (!IsLowercaseSha256(ConsumedRoadEdgeGeometryDigest))
	{
		return ErrorJson(Operation, TEXT("geometry digest must be 64 lowercase hexadecimal characters"), FString(), GraphAssetPath);
	}

	TArray<FString> SortedEdgeLabels;
	for (const FString& Label : ConsumedRoadEdgeActorLabels)
	{
		if (Label.IsEmpty() || Label.TrimStartAndEnd() != Label)
		{
			return ErrorJson(Operation, TEXT("consumed road edge labels must be nonblank and trimmed"), FString(), GraphAssetPath);
		}
		SortedEdgeLabels.AddUnique(Label);
	}
	SortedEdgeLabels.Sort();
	if (SortedEdgeLabels.Num() == 0)
	{
		return ErrorJson(Operation, TEXT("at least one consumed road edge label is required"), FString(), GraphAssetPath);
	}

	UStaticMesh* StaticMesh = LoadAssetByPackagePath<UStaticMesh>(StaticMeshPath);
	if (!StaticMesh)
	{
		return ErrorJson(Operation, FString::Printf(TEXT("could not load static mesh '%s'"), *StaticMeshPath), FString(), GraphAssetPath);
	}
	if (MaterialOverridePaths.Num() != StaticMesh->GetStaticMaterials().Num())
	{
		return ErrorJson(
			Operation,
			FString::Printf(TEXT("material override count %d does not match mesh slot count %d"),
				MaterialOverridePaths.Num(), StaticMesh->GetStaticMaterials().Num()),
			FString(), GraphAssetPath);
	}

	TArray<TSoftObjectPtr<UMaterialInterface>> LoadedMaterialOverrides;
	LoadedMaterialOverrides.Reserve(MaterialOverridePaths.Num());
	for (const FString& MaterialPath : MaterialOverridePaths)
	{
		if (MaterialPath.IsEmpty())
		{
			LoadedMaterialOverrides.Add(TSoftObjectPtr<UMaterialInterface>());
			continue;
		}
		if (!FPackageName::IsValidLongPackageName(MaterialPath, true))
		{
			return ErrorJson(Operation, TEXT("material override must be blank or a valid long package name"), FString(), GraphAssetPath);
		}
		UMaterialInterface* Material = LoadAssetByPackagePath<UMaterialInterface>(MaterialPath);
		if (!Material)
		{
			return ErrorJson(Operation, FString::Printf(TEXT("could not load material override '%s'"), *MaterialPath), FString(), GraphAssetPath);
		}
		LoadedMaterialOverrides.Add(TSoftObjectPtr<UMaterialInterface>(Material));
	}

	UObject* ExistingGraphObject = LoadAssetByPackagePath<UObject>(GraphAssetPath);
	if (ExistingGraphObject && !ExistingGraphObject->IsA<UPCGGraph>())
	{
		return ErrorJson(Operation, TEXT("requested graph object exists but is not a PCG graph"), FString(), GraphAssetPath);
	}
	UPCGGraph* Graph = Cast<UPCGGraph>(ExistingGraphObject);
	FRevertibleEditorTransaction Transaction(NSLOCTEXT("GameXXK", "AuthorTownPCGGraphAdvanced", "Author Town PCG Graph Advanced"));
	if (!Transaction.IsValid())
	{
		return ErrorJson(Operation, TEXT("could not start an isolated editor transaction"), FString(), GraphAssetPath);
	}
	bool bCreated = false;
	if (!Graph)
	{
		UPackage* Package = CreatePackage(*GraphAssetPath);
		if (!Package)
		{
			return ErrorJson(Operation, TEXT("could not create graph package"), FString(), GraphAssetPath);
		}
		Package->FullyLoad();
		const FName AssetName(*FPackageName::GetLongPackageAssetName(GraphAssetPath));
		UObject* ExistingNamedObject = FindObject<UObject>(Package, *AssetName.ToString());
		if (ExistingNamedObject && !ExistingNamedObject->IsA<UPCGGraph>())
		{
			return ErrorJson(Operation, TEXT("requested graph object exists but is not a PCG graph"), FString(), GraphAssetPath);
		}
		Graph = Cast<UPCGGraph>(ExistingNamedObject);
		if (!Graph)
		{
			Graph = NewObject<UPCGGraph>(Package, AssetName, RF_Public | RF_Standalone | RF_Transactional);
			bCreated = Graph != nullptr;
		}
	}
	if (!Graph)
	{
		return ErrorJson(Operation, TEXT("could not create or load graph"), FString(), GraphAssetPath);
	}

	Graph->Modify();
	TArray<UPCGNode*> AuthoredNodes = Graph->GetNodes();
	Graph->RemoveNodes(AuthoredNodes);
	UPCGCreatePointsSettings* CreatePointsSettings = nullptr;
	UPCGNode* CreatePointsNode = Graph->AddNodeOfType<UPCGCreatePointsSettings>(CreatePointsSettings);
	UPCGStaticMeshSpawnerSettings* SpawnerSettings = nullptr;
	UPCGNode* SpawnerNode = Graph->AddNodeOfType<UPCGStaticMeshSpawnerSettings>(SpawnerSettings);
	if (!CreatePointsNode || !CreatePointsSettings || !SpawnerNode || !SpawnerSettings)
	{
		return ErrorJson(Operation, TEXT("could not create PCG graph nodes"), FString(), GraphAssetPath);
	}

	struct FStablePointRecord
	{
		FString CanonicalTransform;
		FTransform Transform;
		int32 Seed = 0;
	};
	TArray<FStablePointRecord> PointRecords;
	PointRecords.Reserve(PointTransforms.Num());
	for (const FTransform& Transform : PointTransforms)
	{
		FStablePointRecord& Record = PointRecords.AddDefaulted_GetRef();
		Record.CanonicalTransform = CanonicalTransformString(Transform);
		Record.Transform = Transform;
		Record.Seed = PCGHelpers::ComputeSeed(BaseSeed, StableTransformHash(Transform));
	}
	PointRecords.Sort([](const FStablePointRecord& A, const FStablePointRecord& B)
	{
		return A.CanonicalTransform < B.CanonicalTransform;
	});
	TArray<FString> CanonicalSeedRecords;
	CanonicalSeedRecords.Reserve(PointRecords.Num());
	CreatePointsSettings->Modify();
	CreatePointsSettings->CoordinateSpace = EPCGCoordinateSpace::World;
	CreatePointsSettings->bCullPointsOutsideVolume = false;
	CreatePointsSettings->PointsToCreate.Reset(PointRecords.Num());
	for (const FStablePointRecord& Record : PointRecords)
	{
		CanonicalSeedRecords.Add(Record.CanonicalTransform + TEXT("|") + LexToString(Record.Seed));
		CreatePointsSettings->PointsToCreate.Emplace(Record.Transform, 1.0f, Record.Seed);
	}
	const FString PointSeedDigest = Sha256ForCanonicalSeedRecords(CanonicalSeedRecords);
	if (!IsLowercaseSha256(PointSeedDigest))
	{
		return ErrorJson(Operation, TEXT("could not compute point seed digest"), FString(), GraphAssetPath);
	}

	SpawnerSettings->Modify();
	SpawnerSettings->SetMeshSelectorType(UPCGMeshSelectorWeighted::StaticClass());
	UPCGMeshSelectorWeighted* WeightedSelector = Cast<UPCGMeshSelectorWeighted>(SpawnerSettings->MeshSelectorParameters);
	if (!WeightedSelector)
	{
		return ErrorJson(Operation, TEXT("could not initialize weighted mesh selector"), FString(), GraphAssetPath);
	}
	WeightedSelector->Modify();
	FPCGMeshSelectorWeightedEntry Entry(TSoftObjectPtr<UStaticMesh>(StaticMesh), 1);
	Entry.Descriptor.OverrideMaterials = LoadedMaterialOverrides;
	Entry.Descriptor.ComputeHash();
	WeightedSelector->MeshEntries = {Entry};
	SpawnerSettings->bAllowMergeDifferentDataInSameInstancedComponents = true;

	Graph->AddLabeledEdge(CreatePointsNode, PCGPinConstants::DefaultOutputLabel, SpawnerNode, PCGPinConstants::DefaultInputLabel);
	Graph->AddLabeledEdge(SpawnerNode, PCGPinConstants::DefaultOutputLabel, Graph->GetOutputNode(), PCGPinConstants::DefaultOutputLabel);
	if (!VerifyDirectedEdge(CreatePointsNode, PCGPinConstants::DefaultOutputLabel, SpawnerNode, PCGPinConstants::DefaultInputLabel)
		|| !VerifyDirectedEdge(SpawnerNode, PCGPinConstants::DefaultOutputLabel, Graph->GetOutputNode(), PCGPinConstants::DefaultOutputLabel))
	{
		return ErrorJson(Operation, TEXT("could not connect PCG graph nodes"), FString(), GraphAssetPath);
	}

	TSharedRef<FJsonObject> Contract = MakeShared<FJsonObject>();
	Contract->SetNumberField(TEXT("schema_version"), 1);
	Contract->SetStringField(TEXT("static_mesh_path"), StaticMeshPath);
	Contract->SetNumberField(TEXT("point_count"), PointRecords.Num());
	Contract->SetNumberField(TEXT("base_seed"), BaseSeed);
	Contract->SetStringField(TEXT("point_seed_sha256"), PointSeedDigest);
	Contract->SetStringField(TEXT("road_edge_geometry_digest"), ConsumedRoadEdgeGeometryDigest);
	Contract->SetNumberField(TEXT("minimum_road_clearance_cm"), MinimumRoadClearanceCm);
	TArray<TSharedPtr<FJsonValue>> JsonEdgeLabels;
	for (const FString& Label : SortedEdgeLabels)
	{
		JsonEdgeLabels.Add(MakeShared<FJsonValueString>(Label));
	}
	Contract->SetArrayField(TEXT("consumed_road_edge_actor_labels"), JsonEdgeLabels);
	TArray<TSharedPtr<FJsonValue>> JsonMaterialPaths;
	for (const FString& Path : MaterialOverridePaths)
	{
		JsonMaterialPaths.Add(MakeShared<FJsonValueString>(Path));
	}
	Contract->SetArrayField(TEXT("material_override_paths"), JsonMaterialPaths);
	const FString ContractJson = ToJson(Contract);
#if WITH_EDITORONLY_DATA
	Graph->Description = FText::FromString(TEXT("GAMEXXK_TOWN_PCG_CONTRACT:") + ContractJson);
#endif
	Graph->MarkPackageDirty();
	if (bCreated)
	{
		FAssetRegistryModule::AssetCreated(Graph);
	}

	const FString PackageFilename = FPackageName::LongPackageNameToFilename(GraphAssetPath, FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.SaveFlags = SAVE_NoError;
	if (!UPackage::SavePackage(Graph->GetOutermost(), Graph, *PackageFilename, SaveArgs))
	{
		if (bCreated)
		{
			FAssetRegistryModule::AssetDeleted(Graph);
		}
		return ErrorJson(Operation, TEXT("failed to save graph package"), FString(), GraphAssetPath);
	}
	Transaction.Commit();

	TSharedRef<FJsonObject> Result = MakeResult(true, Operation);
	Result->SetStringField(TEXT("graph"), GraphAssetPath);
	Result->SetNumberField(TEXT("point_count"), PointTransforms.Num());
	Result->SetNumberField(TEXT("base_seed"), BaseSeed);
	Result->SetStringField(TEXT("point_seed_sha256"), PointSeedDigest);
	Result->SetStringField(TEXT("contract"), ContractJson);
	Result->SetStringField(TEXT("error"), TEXT(""));
	return ToJson(Result);
}

FString UGameXXKTownPCGAutomationLibrary::AttachTownPCGGraph(
	const FString& ActorLabel,
	const FString& GraphAssetPath,
	const FVector& Center,
	const FVector& Extent)
{
	using namespace GameXXKTownPCGAutomation;
	constexpr TCHAR Operation[] = TEXT("AttachTownPCGGraph");
	FString Error;
	if (!ValidateActorLabel(ActorLabel, Error))
	{
		return ErrorJson(Operation, Error, ActorLabel, GraphAssetPath);
	}
	if (!FPackageName::IsValidLongPackageName(GraphAssetPath, true))
	{
		return ErrorJson(Operation, TEXT("graph must be a valid long package name"), ActorLabel, GraphAssetPath);
	}
	if (!IsManagedGraphPath(GraphAssetPath))
	{
		return ErrorJson(Operation, TEXT("graph path must be under the managed town PCG graph roots"), ActorLabel, GraphAssetPath);
	}
	if (!IsFiniteVector(Center) || !IsFiniteVector(Extent))
	{
		return ErrorJson(Operation, TEXT("center and extent must contain only finite values"), ActorLabel, GraphAssetPath);
	}
	if (Extent.X <= 0.0 || Extent.Y <= 0.0 || Extent.Z <= 0.0)
	{
		return ErrorJson(Operation, TEXT("extent components must all be positive"), ActorLabel, GraphAssetPath);
	}

	UWorld* World = GetEditorWorld();
	if (!ValidatePrototypeMutationContext(World, nullptr, Error))
	{
		return ErrorJson(Operation, Error, ActorLabel, GraphAssetPath);
	}

	UPCGGraph* Graph = LoadAssetByPackagePath<UPCGGraph>(GraphAssetPath);
	if (!Graph)
	{
		return ErrorJson(Operation, TEXT("could not load PCG graph"), ActorLabel, GraphAssetPath);
	}

	const TArray<AActor*> Matches = FindActorsByExactLabel(World, ActorLabel);
	if (Matches.Num() > 1)
	{
		return ErrorJson(Operation, FString::Printf(TEXT("%d actors have the exact label; expected at most one"), Matches.Num()), ActorLabel, GraphAssetPath);
	}

	APCGVolume* Volume = nullptr;
	if (Matches.Num() == 1)
	{
		Volume = Cast<APCGVolume>(Matches[0]);
		if (!Volume)
		{
			return ErrorJson(Operation, TEXT("the exact-label actor is not a PCG volume"), ActorLabel, GraphAssetPath);
		}
		if (!Volume->PCGComponent)
		{
			return ErrorJson(Operation, TEXT("PCG volume has no PCG component"), ActorLabel, GraphAssetPath);
		}
	}
	if (Volume && !ValidatePrototypeMutationContext(World, Volume, Error))
	{
		return ErrorJson(Operation, Error, ActorLabel, GraphAssetPath);
	}

	UCubeBuilder* CubeBuilder = NewObject<UCubeBuilder>(GetTransientPackage());
	if (!CubeBuilder)
	{
		return ErrorJson(Operation, TEXT("could not create volume brush builder"), ActorLabel, GraphAssetPath);
	}
	CubeBuilder->X = Extent.X * 2.0;
	CubeBuilder->Y = Extent.Y * 2.0;
	CubeBuilder->Z = Extent.Z * 2.0;
	CubeBuilder->Hollow = false;

	FRevertibleEditorTransaction Transaction(NSLOCTEXT("GameXXK", "AttachTownPCGGraph", "Attach Town PCG Graph"));
	if (!Transaction.IsValid())
	{
		return ErrorJson(Operation, TEXT("could not start an isolated editor transaction"), ActorLabel, GraphAssetPath);
	}
	if (!Volume)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.ObjectFlags |= RF_Transactional;
		Volume = World->SpawnActor<APCGVolume>(APCGVolume::StaticClass(), FTransform(Center), SpawnParameters);
		if (!Volume)
		{
			return ErrorJson(Operation, TEXT("failed to spawn PCG volume"), ActorLabel, GraphAssetPath);
		}
		Volume->SetActorLabel(ActorLabel);
	}

	Volume->Modify();
	Volume->SetActorLocation(Center);
	Volume->SetActorRotation(FRotator::ZeroRotator);
	Volume->SetActorScale3D(FVector::OneVector);
	UActorFactory::CreateBrushForVolumeActor(Volume, CubeBuilder);
	if (!Volume->GetBrushComponent())
	{
		return ErrorJson(Operation, TEXT("failed to create PCG volume brush"), ActorLabel, GraphAssetPath);
	}
	Volume->PCGComponent->Modify();
	Volume->PCGComponent->SetGraph(Graph);
	Volume->PCGComponent->GenerationTrigger = EPCGComponentGenerationTrigger::GenerateOnDemand;
	Volume->PCGComponent->bGenerateOnDropWhenTriggerOnDemand = false;
	Volume->PCGComponent->bRegenerateInEditor = false;
	Volume->MarkPackageDirty();
	Transaction.Commit();

	TSharedRef<FJsonObject> Result = MakeResult(true, Operation);
	Result->SetStringField(TEXT("actor_label"), ActorLabel);
	Result->SetStringField(TEXT("graph"), GraphAssetPath);
	Result->SetNumberField(TEXT("generated_component_count"), CountGeneratedInstancedComponents(Volume));
	Result->SetStringField(TEXT("error"), TEXT(""));
	return ToJson(Result);
}

FString UGameXXKTownPCGAutomationLibrary::AttachTownPCGGraphAdvanced(
	const FString& ActorLabel,
	const FString& GraphAssetPath,
	const FVector& Center,
	const FVector& Extent,
	const int32 ComponentSeed)
{
	using namespace GameXXKTownPCGAutomation;
	constexpr TCHAR Operation[] = TEXT("AttachTownPCGGraphAdvanced");
	FString Error;
	if (!ValidateActorLabel(ActorLabel, Error))
	{
		return ErrorJson(Operation, Error, ActorLabel, GraphAssetPath);
	}
	if (!FPackageName::IsValidLongPackageName(GraphAssetPath, true) || !IsB1GraphPath(GraphAssetPath))
	{
		return ErrorJson(Operation, TEXT("advanced graph path must be under the B1 town PCG graph root"), ActorLabel, GraphAssetPath);
	}
	if (!IsFiniteVector(Center) || !IsFiniteVector(Extent)
		|| Extent.X <= 0.0 || Extent.Y <= 0.0 || Extent.Z <= 0.0)
	{
		return ErrorJson(Operation, TEXT("center/extent must be finite and extent positive"), ActorLabel, GraphAssetPath);
	}
	UWorld* World = GetEditorWorld();
	if (!ValidateB1MutationContext(World, nullptr, Error))
	{
		return ErrorJson(Operation, Error, ActorLabel, GraphAssetPath);
	}
	UPCGGraph* Graph = LoadAssetByPackagePath<UPCGGraph>(GraphAssetPath);
	if (!Graph)
	{
		return ErrorJson(Operation, TEXT("could not load PCG graph"), ActorLabel, GraphAssetPath);
	}
	const TArray<AActor*> Matches = FindActorsByExactLabel(World, ActorLabel);
	if (Matches.Num() > 1)
	{
		return ErrorJson(Operation, TEXT("multiple actors have the exact PCG volume label"), ActorLabel, GraphAssetPath);
	}
	APCGVolume* Volume = Matches.Num() == 1 ? Cast<APCGVolume>(Matches[0]) : nullptr;
	if (Matches.Num() == 1 && (!Volume || !Volume->PCGComponent))
	{
		return ErrorJson(Operation, TEXT("exact-label actor is not a valid PCG volume"), ActorLabel, GraphAssetPath);
	}
	if (Volume && !ValidateB1MutationContext(World, Volume, Error))
	{
		return ErrorJson(Operation, Error, ActorLabel, GraphAssetPath);
	}

	UCubeBuilder* CubeBuilder = NewObject<UCubeBuilder>(GetTransientPackage());
	if (!CubeBuilder)
	{
		return ErrorJson(Operation, TEXT("could not create volume brush builder"), ActorLabel, GraphAssetPath);
	}
	CubeBuilder->X = Extent.X * 2.0;
	CubeBuilder->Y = Extent.Y * 2.0;
	CubeBuilder->Z = Extent.Z * 2.0;
	CubeBuilder->Hollow = false;

	FRevertibleEditorTransaction Transaction(NSLOCTEXT("GameXXK", "AttachTownPCGGraphAdvanced", "Attach Town PCG Graph Advanced"));
	if (!Transaction.IsValid())
	{
		return ErrorJson(Operation, TEXT("could not start an isolated editor transaction"), ActorLabel, GraphAssetPath);
	}
	if (!Volume)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.ObjectFlags |= RF_Transactional;
		SpawnParameters.OverrideLevel = World->GetCurrentLevel();
		Volume = World->SpawnActor<APCGVolume>(APCGVolume::StaticClass(), FTransform(Center), SpawnParameters);
		if (!Volume || !Volume->PCGComponent)
		{
			return ErrorJson(Operation, TEXT("failed to spawn PCG volume"), ActorLabel, GraphAssetPath);
		}
		Volume->SetActorLabel(ActorLabel);
	}
	Volume->Modify();
	Volume->SetActorLocation(Center);
	Volume->SetActorRotation(FRotator::ZeroRotator);
	Volume->SetActorScale3D(FVector::OneVector);
	UActorFactory::CreateBrushForVolumeActor(Volume, CubeBuilder);
	if (!Volume->GetBrushComponent())
	{
		return ErrorJson(Operation, TEXT("failed to create PCG volume brush"), ActorLabel, GraphAssetPath);
	}
	Volume->PCGComponent->Modify();
	Volume->PCGComponent->SetGraph(Graph);
	Volume->PCGComponent->GenerationTrigger = EPCGComponentGenerationTrigger::GenerateOnDemand;
	Volume->PCGComponent->bGenerateOnDropWhenTriggerOnDemand = false;
	Volume->PCGComponent->bRegenerateInEditor = false;
	Volume->PCGComponent->Seed = ComponentSeed;
	Volume->MarkPackageDirty();
	Transaction.Commit();

	TSharedRef<FJsonObject> Result = MakeResult(true, Operation);
	Result->SetStringField(TEXT("actor_label"), ActorLabel);
	Result->SetStringField(TEXT("graph"), GraphAssetPath);
	Result->SetNumberField(TEXT("component_seed"), ComponentSeed);
	Result->SetNumberField(TEXT("generated_component_count"), CountGeneratedInstancedComponents(Volume));
	Result->SetStringField(TEXT("error"), TEXT(""));
	return ToJson(Result);
}

FString UGameXXKTownPCGAutomationLibrary::GenerateTownPCG(const FString& ActorLabel)
{
	using namespace GameXXKTownPCGAutomation;
	constexpr TCHAR Operation[] = TEXT("GenerateTownPCG");
	FString Error;
	if (!ValidateActorLabel(ActorLabel, Error))
	{
		return ErrorJson(Operation, Error, ActorLabel);
	}
	UWorld* World = GetEditorWorld();
	if (!ValidatePrototypeMutationContext(World, nullptr, Error))
	{
		return ErrorJson(Operation, Error, ActorLabel);
	}
	APCGVolume* Volume = FindUniquePCGVolume(World, ActorLabel, Error);
	if (!Volume)
	{
		return ErrorJson(Operation, Error, ActorLabel);
	}
	if (!ValidatePrototypeMutationContext(World, Volume, Error))
	{
		return ErrorJson(Operation, Error, ActorLabel);
	}

	const auto MakeStateResult = [Volume, Operation](const bool bScheduled, const FPCGTaskId TaskId)
	{
		const bool bGenerating = Volume->PCGComponent->IsGenerating();
		const bool bGenerated = Volume->PCGComponent->bGenerated;
		TSharedRef<FJsonObject> Result = MakeResult(true, Operation);
		Result->SetStringField(TEXT("actor_label"), Volume->GetActorLabel());
		Result->SetBoolField(TEXT("scheduled"), bScheduled);
		Result->SetBoolField(TEXT("generating"), bGenerating);
		Result->SetBoolField(TEXT("generated"), bGenerated);
		Result->SetNumberField(TEXT("generated_component_count"), CountGeneratedInstancedComponents(Volume));
		if (TaskId != InvalidPCGTaskId)
		{
			Result->SetNumberField(TEXT("task_id"), static_cast<double>(TaskId));
		}
		Result->SetStringField(TEXT("error"), TEXT(""));
		return ToJson(Result);
	};
	if (Volume->PCGComponent->IsGenerating())
	{
		return MakeStateResult(true, Volume->PCGComponent->GetGenerationTaskId());
	}
	if (Volume->PCGComponent->bGenerated)
	{
		return MakeStateResult(false, InvalidPCGTaskId);
	}

	FRevertibleEditorTransaction Transaction(NSLOCTEXT("GameXXK", "GenerateTownPCG", "Generate Town PCG"));
	if (!Transaction.IsValid())
	{
		return ErrorJson(Operation, TEXT("could not start an isolated editor transaction"), ActorLabel);
	}
	Volume->PCGComponent->Modify();
	const FPCGTaskId TaskId = Volume->PCGComponent->GenerateLocalGetTaskId(true);
	if (TaskId == InvalidPCGTaskId)
	{
		if (Volume->PCGComponent->bGenerated)
		{
			Transaction.Commit();
			return MakeStateResult(false, InvalidPCGTaskId);
		}
		return ErrorJson(Operation, TEXT("PCG generation could not be scheduled"), ActorLabel);
	}
	Transaction.Commit();
	return MakeStateResult(true, TaskId);
}

FString UGameXXKTownPCGAutomationLibrary::GetTownPCGStatus(const FString& ActorLabel)
{
	using namespace GameXXKTownPCGAutomation;
	constexpr TCHAR Operation[] = TEXT("GetTownPCGStatus");
	FString Error;
	if (!ValidateActorLabel(ActorLabel, Error))
	{
		return ErrorJson(Operation, Error, ActorLabel);
	}
	UWorld* World = GetEditorWorld();
	if (!ValidatePrototypeReadContext(World, nullptr, Error))
	{
		return ErrorJson(Operation, Error, ActorLabel);
	}
	APCGVolume* Volume = FindUniquePCGVolume(World, ActorLabel, Error);
	if (!Volume)
	{
		return ErrorJson(Operation, Error, ActorLabel);
	}
	if (!ValidatePrototypeReadContext(World, Volume, Error))
	{
		return ErrorJson(Operation, Error, ActorLabel);
	}

	const bool bGenerating = Volume->PCGComponent->IsGenerating();
	const bool bGenerated = Volume->PCGComponent->bGenerated;
	const TCHAR* State = bGenerating ? TEXT("generating") : (bGenerated ? TEXT("generated") : TEXT("idle"));
	TSharedRef<FJsonObject> Result = MakeResult(true, Operation);
	Result->SetStringField(TEXT("actor_label"), ActorLabel);
	Result->SetStringField(TEXT("state"), State);
	Result->SetBoolField(TEXT("generating"), bGenerating);
	Result->SetBoolField(TEXT("generated"), bGenerated);
	Result->SetNumberField(TEXT("generated_component_count"), CountGeneratedInstancedComponents(Volume));
	Result->SetNumberField(TEXT("component_seed"), Volume->PCGComponent->Seed);
	if (const UPCGGraph* Graph = Volume->PCGComponent->GetGraph())
	{
#if WITH_EDITORONLY_DATA
		Result->SetStringField(TEXT("graph_contract"), Graph->Description.ToString());
#endif
	}
	if (bGenerating)
	{
		Result->SetNumberField(TEXT("task_id"), static_cast<double>(Volume->PCGComponent->GetGenerationTaskId()));
	}
	Result->SetStringField(TEXT("error"), TEXT(""));
	return ToJson(Result);
}

FString UGameXXKTownPCGAutomationLibrary::ClearTownPCG(const FString& ActorLabel)
{
	using namespace GameXXKTownPCGAutomation;
	constexpr TCHAR Operation[] = TEXT("ClearTownPCG");
	FString Error;
	if (!ValidateActorLabel(ActorLabel, Error))
	{
		return ErrorJson(Operation, Error, ActorLabel);
	}
	UWorld* World = GetEditorWorld();
	if (!ValidatePrototypeMutationContext(World, nullptr, Error))
	{
		return ErrorJson(Operation, Error, ActorLabel);
	}
	APCGVolume* Volume = FindUniquePCGVolume(World, ActorLabel, Error);
	if (!Volume)
	{
		return ErrorJson(Operation, Error, ActorLabel);
	}
	if (!ValidatePrototypeMutationContext(World, Volume, Error))
	{
		return ErrorJson(Operation, Error, ActorLabel);
	}

	FRevertibleEditorTransaction Transaction(NSLOCTEXT("GameXXK", "ClearTownPCG", "Clear Town PCG"));
	if (!Transaction.IsValid())
	{
		return ErrorJson(Operation, TEXT("could not start an isolated editor transaction"), ActorLabel);
	}
	Volume->PCGComponent->Modify();
	Volume->PCGComponent->CleanupLocalImmediate(true, true);
	Transaction.Commit();
	TSharedRef<FJsonObject> Result = MakeResult(true, Operation);
	Result->SetStringField(TEXT("actor_label"), ActorLabel);
	if (const UPCGGraph* Graph = Volume->PCGComponent->GetGraph())
	{
		Result->SetStringField(TEXT("graph"), Graph->GetOutermost()->GetName());
	}
	Result->SetNumberField(TEXT("generated_component_count"), CountGeneratedInstancedComponents(Volume));
	Result->SetStringField(TEXT("error"), TEXT(""));
	return ToJson(Result);
}
