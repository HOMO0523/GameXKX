#include "GameXXKTownPCGAutomationLibrary.h"

#include "ActorFactories/ActorFactory.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Builders/CubeBuilder.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Editor.h"
#include "Elements/PCGCreatePoints.h"
#include "Elements/PCGStaticMeshSpawner.h"
#include "Engine/StaticMesh.h"
#include "Engine/Level.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Helpers/PCGHelpers.h"
#include "LevelUtils.h"
#include "MeshSelectors/PCGMeshSelectorWeighted.h"
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

namespace GameXXKTownPCGAutomation
{
	constexpr TCHAR ManagedGraphRoot[] = TEXT("/Game/GameXXK/Environment/TownPCG/VerticalSlice/");
	constexpr TCHAR PrototypeMapRoot[] = TEXT("/Game/GameXXK/Maps/Prototype/");
	constexpr double GenerationTimeoutSeconds = 15.0;

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

	bool ValidatePrototypeMutationContext(UWorld* World, const AActor* ActorToMutate, FString& OutError)
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
		if (!WorldPackageName.StartsWith(PrototypeMapRoot, ESearchCase::CaseSensitive))
		{
			OutError = TEXT("town PCG actor operations are restricted to prototype maps");
			return false;
		}
		if (!CurrentLevelPackageName.StartsWith(PrototypeMapRoot, ESearchCase::CaseSensitive))
		{
			OutError = TEXT("current level is not a prototype map level");
			return false;
		}
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
		if (ActorToMutate && ActorToMutate->GetLevel() != CurrentLevel)
		{
			OutError = TEXT("the exact-label PCG volume belongs to a different level");
			return false;
		}
		return true;
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
	if (!GraphAssetPath.StartsWith(ManagedGraphRoot, ESearchCase::CaseSensitive))
	{
		return ErrorJson(Operation, TEXT("graph path must be under the managed town PCG VerticalSlice root"), FString(), GraphAssetPath);
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
	if (!GraphAssetPath.StartsWith(ManagedGraphRoot, ESearchCase::CaseSensitive))
	{
		return ErrorJson(Operation, TEXT("graph path must be under the managed town PCG VerticalSlice root"), ActorLabel, GraphAssetPath);
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

	UPCGSubsystem* Subsystem = UPCGSubsystem::GetInstance(World);
	if (!Subsystem)
	{
		return ErrorJson(Operation, TEXT("no PCG subsystem is available"), ActorLabel);
	}
	FRevertibleEditorTransaction Transaction(NSLOCTEXT("GameXXK", "GenerateTownPCG", "Generate Town PCG"));
	if (!Transaction.IsValid())
	{
		return ErrorJson(Operation, TEXT("could not start an isolated editor transaction"), ActorLabel);
	}
	bool bGenerationCompleted = false;
	bool bGenerationCancelled = false;
	const FDelegateHandle GeneratedHandle = Volume->PCGComponent->OnPCGGraphGeneratedDelegate.AddLambda(
		[TargetComponent = Volume->PCGComponent, &bGenerationCompleted](UPCGComponent* GeneratedComponent)
		{
			bGenerationCompleted = GeneratedComponent == TargetComponent;
		});
	const FDelegateHandle CancelledHandle = Volume->PCGComponent->OnPCGGraphCancelledDelegate.AddLambda(
		[TargetComponent = Volume->PCGComponent, &bGenerationCancelled](UPCGComponent* CancelledComponent)
		{
			bGenerationCancelled = CancelledComponent == TargetComponent;
		});
	const auto RemoveGenerationDelegates = [&]()
	{
		Volume->PCGComponent->OnPCGGraphGeneratedDelegate.Remove(GeneratedHandle);
		Volume->PCGComponent->OnPCGGraphCancelledDelegate.Remove(CancelledHandle);
	};
	Volume->PCGComponent->Modify();
	const FPCGTaskId TaskId = Volume->PCGComponent->GenerateLocalGetTaskId(true);
	if (TaskId == InvalidPCGTaskId)
	{
		RemoveGenerationDelegates();
		return ErrorJson(Operation, TEXT("PCG generation could not be scheduled"), ActorLabel);
	}

	const double Deadline = FPlatformTime::Seconds() + GenerationTimeoutSeconds;
	while (!bGenerationCompleted && !bGenerationCancelled && Volume->PCGComponent->IsGenerating() && FPlatformTime::Seconds() < Deadline)
	{
		Subsystem->Tick(0.0f);
		FPlatformProcess::SleepNoStats(0.001f);
	}
	const bool bTimedOut = Volume->PCGComponent->IsGenerating() && !bGenerationCompleted && !bGenerationCancelled;
	if (bTimedOut)
	{
		Volume->PCGComponent->CancelGeneration();
		RemoveGenerationDelegates();
		return ErrorJson(Operation, TEXT("PCG generation timed out"), ActorLabel);
	}
	RemoveGenerationDelegates();
	if (!bGenerationCompleted)
	{
		return ErrorJson(Operation, TEXT("PCG generation was cancelled or aborted"), ActorLabel);
	}
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
