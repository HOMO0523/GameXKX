#include "Quick_RoadAutomationLibrary.h"

#include "Tools/Quick_RoadLandscapeCreator.h"
#include "Tools/Quick_RoadLayoutRoadGenerator.h"
#include "Quick_RoadLayoutDrawStageParams.h"
#include "Quick_RoadLayoutRoadMeshComponent.h"
#include "Quick_RoadLayoutRoadTags.h"
#include "Quick_RoadLayoutSplineTags.h"
#include "Quick_RoadRoadInfluenceComponent.h"
#include "Quick_RoadRoadSplineWidthComponent.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Algo/Reverse.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Landscape.h"
#include "LandscapeComponent.h"
#include "LandscapeDataAccess.h"
#include "LandscapeEdit.h"
#include "LandscapeEditLayer.h"
#include "LandscapeEditTypes.h"
#include "LandscapeInfo.h"
#include "LevelUtils.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "ProceduralMeshComponent.h"
#include "ScopedTransaction.h"
#include "Selection.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace QuickRoadAutomation
{
	constexpr TCHAR B1MapPackage[] = TEXT("/Game/GameXXK/Maps/Dev/L_Qingshan_PCG_Dress_B1");
	const FName OwnerTag(TEXT("QingshanB1QuickRoadOwned"));
	const FName NetworkCategoryTag(TEXT("QingshanB1QuickRoadNetwork"));
	const FName EdgeCategoryTag(TEXT("QingshanB1QuickRoadEdge"));
	const FName BakeCategoryTag(TEXT("QingshanB1QuickRoadBake"));
	constexpr TCHAR WidthGroupTagPrefix[] = TEXT("QingshanB1QuickRoadWidthGroup_");

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

	FString ErrorJson(const TCHAR* Operation, const FString& Error)
	{
		TSharedRef<FJsonObject> Result = MakeResult(false, Operation);
		Result->SetStringField(TEXT("error"), Error);
		return ToJson(Result);
	}

	UWorld* GetEditorWorld()
	{
		return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	}

	bool ValidateB1MutationContext(UWorld*& OutWorld, FString& OutError)
	{
		OutWorld = GetEditorWorld();
		if (!OutWorld || !OutWorld->GetCurrentLevel())
		{
			OutError = TEXT("no editor world/current level is available");
			return false;
		}
		const FString WorldPackageName = OutWorld->GetOutermost()->GetName();
		const FString CurrentLevelPackageName = OutWorld->GetCurrentLevel()->GetOutermost()->GetName();
		if (WorldPackageName != B1MapPackage || CurrentLevelPackageName != B1MapPackage)
		{
			OutError = FString::Printf(
				TEXT("QuickRoad B1 automation requires exact world/current level '%s' (got '%s'/'%s')"),
				B1MapPackage, *WorldPackageName, *CurrentLevelPackageName);
			return false;
		}
		if (FLevelUtils::IsLevelLocked(OutWorld->GetCurrentLevel()))
		{
			OutError = TEXT("current B1 level is locked");
			return false;
		}
		const FString MapFilename = FPackageName::LongPackageNameToFilename(
			CurrentLevelPackageName, FPackageName::GetMapPackageExtension());
		if (IFileManager::Get().FileExists(*MapFilename) && IFileManager::Get().IsReadOnly(*MapFilename))
		{
			OutError = TEXT("current B1 map package is read-only");
			return false;
		}
		return true;
	}

	class FScopedActorSelectionRestore
	{
	public:
		FScopedActorSelectionRestore()
		{
			if (!GEditor)
			{
				return;
			}
			for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
			{
				if (AActor* Actor = Cast<AActor>(*It))
				{
					SelectedActors.Add(Actor);
				}
			}
		}

		~FScopedActorSelectionRestore()
		{
			if (!GEditor)
			{
				return;
			}
			GEditor->SelectNone(false, true, false);
			for (const TWeakObjectPtr<AActor>& Actor : SelectedActors)
			{
				if (Actor.IsValid())
				{
					GEditor->SelectActor(Actor.Get(), true, false, true);
				}
			}
		}

	private:
		TArray<TWeakObjectPtr<AActor>> SelectedActors;
	};

	FString ObjectPathForPackage(const FString& LongPackageName)
	{
		return FString::Printf(TEXT("%s.%s"), *LongPackageName, *FPackageName::GetLongPackageAssetName(LongPackageName));
	}

	UMaterialInterface* LoadMaterial(const FString& LongPackageName, FString& OutError)
	{
		if (!FPackageName::IsValidLongPackageName(LongPackageName, true))
		{
			OutError = TEXT("material path must be a valid long package name");
			return nullptr;
		}
		UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *ObjectPathForPackage(LongPackageName));
		if (!Material)
		{
			OutError = FString::Printf(TEXT("could not load material '%s'"), *LongPackageName);
		}
		return Material;
	}

	TArray<AActor*> FindActorsByExactLabel(UWorld* World, const FString& Label)
	{
		TArray<AActor*> Matches;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (IsValid(*It) && It->GetLevel() == World->GetCurrentLevel() && It->GetActorLabel() == Label)
			{
				Matches.Add(*It);
			}
		}
		return Matches;
	}

	TSet<ALandscape*> SnapshotLandscapes(UWorld* World)
	{
		TSet<ALandscape*> Result;
		for (TActorIterator<ALandscape> It(World); It; ++It)
		{
			if (IsValid(*It))
			{
				Result.Add(*It);
			}
		}
		return Result;
	}

	TArray<ALandscape*> DiffLandscapes(UWorld* World, const TSet<ALandscape*>& Before)
	{
		TArray<ALandscape*> Result;
		for (TActorIterator<ALandscape> It(World); It; ++It)
		{
			if (IsValid(*It) && !Before.Contains(*It))
			{
				Result.Add(*It);
			}
		}
		return Result;
	}

	void DestroyNewLandscapes(UWorld* World, const TArray<ALandscape*>& NewLandscapes)
	{
		for (ALandscape* Landscape : NewLandscapes)
		{
			if (IsValid(Landscape))
			{
				World->DestroyActor(Landscape);
			}
		}
	}

	TSet<AActor*> SnapshotActors(UWorld* World)
	{
		TSet<AActor*> Result;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (IsValid(*It))
			{
				Result.Add(*It);
			}
		}
		return Result;
	}

	TArray<AActor*> DiffActors(UWorld* World, const TSet<AActor*>& Before)
	{
		TArray<AActor*> Result;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (IsValid(*It) && !Before.Contains(*It))
			{
				Result.Add(*It);
			}
		}
		return Result;
	}

	bool IsOwnedNetwork(const AActor* Actor)
	{
		return Actor && Actor->ActorHasTag(Quick_RoadLayoutRoadTags::MainRoadNetwork)
			&& Actor->ActorHasTag(OwnerTag) && Actor->ActorHasTag(NetworkCategoryTag);
	}

	TArray<AActor*> FindOwnedNetworks(UWorld* World)
	{
		TArray<AActor*> Result;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (IsOwnedNetwork(*It))
			{
				Result.Add(*It);
			}
		}
		return Result;
	}

	AActor* FindUniqueOwnedNetwork(UWorld* World, FString& OutError)
	{
		const TArray<AActor*> Networks = FindOwnedNetworks(World);
		if (Networks.Num() != 1)
		{
			OutError = FString::Printf(TEXT("expected exactly one owned QuickRoad network, found %d"), Networks.Num());
			return nullptr;
		}
		return Networks[0];
	}

	bool RejectUnownedMatchingNetworks(UWorld* World, const FName RoadTagExpression, FString& OutError)
	{
		TArray<AActor*> MatchingNetworks;
		FQuick_RoadLayoutRoadGenerator::CollectRoadNetworkActorsByTag(World, RoadTagExpression, MatchingNetworks);
		for (AActor* Actor : MatchingNetworks)
		{
			if (!IsOwnedNetwork(Actor))
			{
				OutError = FString::Printf(
					TEXT("refusing to mutate matching unowned QuickRoad network '%s'"),
					Actor ? *Actor->GetActorLabel() : TEXT("<null>"));
				return false;
			}
		}
		return true;
	}

	bool ApplyRoadMaterial(AActor* Network, UMaterialInterface* Material, int32& OutMeshCount)
	{
		OutMeshCount = 0;
		if (!Network || !Material)
		{
			return false;
		}
		TInlineComponentArray<UQuick_RoadLayoutRoadMeshComponent*> Meshes;
		Network->GetComponents(Meshes);
		for (UQuick_RoadLayoutRoadMeshComponent* Mesh : Meshes)
		{
			if (Mesh && Mesh->GetNumSections() > 0)
			{
				Mesh->Modify();
				Mesh->SetMaterial(0, Material);
				++OutMeshCount;
			}
		}
		return OutMeshCount > 0;
	}

	bool ForceHeightMerge(ALandscape* Landscape)
	{
		for (int32 Attempt = 0; Attempt < 4; ++Attempt)
		{
			Landscape->RequestLayersContentUpdateForceAll(ELandscapeLayerUpdateMode::Update_Heightmap_All);
			Landscape->ForceUpdateLayersContent();
			if (Landscape->IsUpToDate())
			{
				return true;
			}
		}
		return false;
	}

	bool ValidateRawLayerData(ALandscape* Landscape, const FGuid& LayerGuid)
	{
		for (ULandscapeComponent* Component : Landscape->LandscapeComponents)
		{
			const FLandscapeLayerComponentData* Data = Component ? Component->GetLayerData(LayerGuid) : nullptr;
			if (!Data || !Data->HeightmapData.Texture)
			{
				return false;
			}
		}
		return Landscape->LandscapeComponents.Num() > 0;
	}

	bool ReadPackedHeights(
		ALandscape* Landscape,
		const FGuid& LayerGuid,
		int32& OutMinX,
		int32& OutMinY,
		int32& OutMaxX,
		int32& OutMaxY,
		TArray<uint16>& OutPacked)
	{
		ULandscapeInfo* Info = Landscape ? Landscape->GetLandscapeInfo() : nullptr;
		if (!Info || !Info->GetLandscapeExtent(OutMinX, OutMinY, OutMaxX, OutMaxY))
		{
			return false;
		}
		const int32 Width = OutMaxX - OutMinX + 1;
		const int32 Height = OutMaxY - OutMinY + 1;
		OutPacked.Init(LandscapeDataAccess::GetTexHeight(0.0f), Width * Height);
		FLandscapeEditDataInterface Reader(Info, LayerGuid, false);
		int32 MinX = OutMinX;
		int32 MinY = OutMinY;
		int32 MaxX = OutMaxX;
		int32 MaxY = OutMaxY;
		Reader.GetHeightData(MinX, MinY, MaxX, MaxY, OutPacked.GetData(), Width);
		return MinX == OutMinX && MinY == OutMinY && MaxX == OutMaxX && MaxY == OutMaxY;
	}

	bool ReadRawEditLayerHeights(
		ALandscape* Landscape,
		const ULandscapeEditLayerBase* RoadLayer,
		int32& OutMinX,
		int32& OutMinY,
		int32& OutMaxX,
		int32& OutMaxY,
		TArray<uint16>& OutPacked)
	{
		return Landscape && RoadLayer && ValidateRawLayerData(Landscape, RoadLayer->GetGuid())
			&& ReadPackedHeights(
				Landscape, RoadLayer->GetGuid(), OutMinX, OutMinY, OutMaxX, OutMaxY, OutPacked);
	}

	bool WritePackedLayerHeights(
		ALandscape* Landscape,
		const FGuid& LayerGuid,
		const int32 MinX,
		const int32 MinY,
		const int32 MaxX,
		const int32 MaxY,
		const TArray<uint16>& Packed)
	{
		ULandscapeInfo* Info = Landscape ? Landscape->GetLandscapeInfo() : nullptr;
		const int32 Width = MaxX - MinX + 1;
		const int32 Height = MaxY - MinY + 1;
		if (!Info || Width <= 0 || Height <= 0 || Packed.Num() != Width * Height)
		{
			return false;
		}
		FLandscapeEditDataInterface Writer(Info, LayerGuid, true);
		Writer.SetHeightData(MinX, MinY, MaxX, MaxY, Packed.GetData(), Width, false);
		Writer.Flush();
		return true;
	}

	class FScopedRoadLayerDeltaRollback
	{
	public:
		FScopedRoadLayerDeltaRollback(
			ALandscape* InLandscape,
			const FGuid& InLayerGuid,
			const int32 InMinX,
			const int32 InMinY,
			const int32 InMaxX,
			const int32 InMaxY,
			const TArray<uint16>& InPacked)
			: Landscape(InLandscape)
			, LayerGuid(InLayerGuid)
			, PreviousEditingLayer(InLandscape ? InLandscape->GetEditingLayer() : FGuid())
			, MinX(InMinX)
			, MinY(InMinY)
			, MaxX(InMaxX)
			, MaxY(InMaxY)
			, OriginalPacked(InPacked)
		{
		}

		~FScopedRoadLayerDeltaRollback()
		{
			if (bCommitted || !IsValid(Landscape))
			{
				return;
			}
			Landscape->Modify();
			const bool bRestored = WritePackedLayerHeights(
				Landscape, LayerGuid, MinX, MinY, MaxX, MaxY, OriginalPacked);
			Landscape->SetEditingLayer(PreviousEditingLayer);
			const bool bMerged = ForceHeightMerge(Landscape);
			if (!bRestored || !bMerged)
			{
				UE_LOG(LogTemp, Error, TEXT("QuickRoad failed to fully restore the previous raw Landscape road delta"));
			}
			Landscape->MarkPackageDirty();
			Landscape->PostEditChange();
		}

		void Commit()
		{
			bCommitted = true;
		}

	private:
		TObjectPtr<ALandscape> Landscape = nullptr;
		FGuid LayerGuid;
		FGuid PreviousEditingLayer;
		int32 MinX = 0;
		int32 MinY = 0;
		int32 MaxX = 0;
		int32 MaxY = 0;
		TArray<uint16> OriginalPacked;
		bool bCommitted = false;
	};

	FString Sha256(const void* Data, const uint32 Size)
	{
		FSHA256Signature Signature{};
		return FPlatformMisc::GetSHA256Signature(Data, Size, Signature)
			? Signature.ToString().ToLower() : FString();
	}

	FString Sha256String(const FString& Value)
	{
		FTCHARToUTF8 Utf8(*Value);
		return Sha256(Utf8.Get(), static_cast<uint32>(Utf8.Length()));
	}

	FString HeightDigest(
		const int32 MinX,
		const int32 MinY,
		const int32 MaxX,
		const int32 MaxY,
		const TArray<uint16>& Packed)
	{
		TArray<uint8> Bytes;
		Bytes.Reserve(sizeof(int32) * 4 + Packed.Num() * sizeof(uint16));
		for (const int32 Value : {MinX, MinY, MaxX, MaxY})
		{
			Bytes.Append(reinterpret_cast<const uint8*>(&Value), sizeof(Value));
		}
		Bytes.Append(reinterpret_cast<const uint8*>(Packed.GetData()), Packed.Num() * sizeof(uint16));
		return Sha256(Bytes.GetData(), static_cast<uint32>(Bytes.Num()));
	}

	ALandscape* FindExactLandscape(UWorld* World, const FString& Label, FString& OutError)
	{
		const TArray<AActor*> Matches = FindActorsByExactLabel(World, Label);
		if (Matches.Num() != 1)
		{
			OutError = FString::Printf(TEXT("expected exactly one actor labeled '%s', found %d"), *Label, Matches.Num());
			return nullptr;
		}
		ALandscape* Landscape = Cast<ALandscape>(Matches[0]);
		if (!Landscape)
		{
			OutError = TEXT("exact-label actor is not an ALandscape");
		}
		return Landscape;
	}

	bool ValidateRoadLayer(
		ALandscape* Landscape,
		const FName EditLayerName,
		const ULandscapeEditLayerBase*& OutRoadLayer,
		FString& OutError)
	{
		OutRoadLayer = Landscape ? Landscape->GetEditLayerConst(EditLayerName) : nullptr;
		if (!OutRoadLayer)
		{
			OutError = FString::Printf(TEXT("Landscape edit layer '%s' does not exist"), *EditLayerName.ToString());
			return false;
		}
		if (!Cast<ULandscapeEditLayer>(OutRoadLayer) || !OutRoadLayer->NeedsPersistentTextures())
		{
			OutError = TEXT("road layer must be a persistent standard additive Landscape edit layer");
			return false;
		}
		if (OutRoadLayer->IsLocked())
		{
			OutError = TEXT("road Landscape edit layer is locked");
			return false;
		}
		if (!OutRoadLayer->IsVisible())
		{
			OutError = TEXT("road Landscape edit layer must be visible");
			return false;
		}
		if (!FMath::IsNearlyEqual(
			OutRoadLayer->GetAlphaForTargetType(ELandscapeToolTargetType::Heightmap), 1.0f, KINDA_SMALL_NUMBER))
		{
			OutError = TEXT("road Landscape edit layer height alpha must equal one");
			return false;
		}
		return true;
	}

	FString SanitizedTag(const FName Tag)
	{
		FString Value = Tag.ToString();
		for (TCHAR& Character : Value)
		{
			if (!FChar::IsAlnum(Character) && Character != TEXT('_'))
			{
				Character = TEXT('_');
			}
		}
		return Value;
	}

	FName SourceCategoryTag(const FName RoadTag)
	{
		return FName(*(TEXT("QingshanB1QuickRoadSource_") + SanitizedTag(RoadTag)));
	}

	FName WidthGroupTag(const FName RoadTag)
	{
		return FName(*(FString(WidthGroupTagPrefix) + SanitizedTag(RoadTag)));
	}

	bool IsWidthGroupTag(const FName Tag)
	{
		return Tag.ToString().StartsWith(WidthGroupTagPrefix, ESearchCase::CaseSensitive);
	}

	TArray<AActor*> FindOwnedEdges(UWorld* World)
	{
		TArray<AActor*> Result;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->ActorHasTag(OwnerTag) && It->ActorHasTag(EdgeCategoryTag))
			{
				Result.Add(*It);
			}
		}
		Result.Sort([](const AActor& A, const AActor& B)
		{
			return A.GetActorLabel() < B.GetActorLabel();
		});
		return Result;
	}

	TSet<AActor*> SnapshotRoadEdgeActors(UWorld* World)
	{
		TSet<AActor*> Result;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->ActorHasTag(Quick_RoadLayoutSplineTags::RoadEdge))
			{
				Result.Add(*It);
			}
		}
		return Result;
	}

	TArray<AActor*> DiffRoadEdgeActors(UWorld* World, const TSet<AActor*>& Before)
	{
		TArray<AActor*> Result;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->ActorHasTag(Quick_RoadLayoutSplineTags::RoadEdge) && !Before.Contains(*It))
			{
				Result.Add(*It);
			}
		}
		return Result;
	}

	FString QuantizedPoint(const FVector& Point)
	{
		return FString::Printf(
			TEXT("%lld,%lld,%lld"),
			static_cast<long long>(FMath::RoundToInt64(Point.X * 10.0)),
			static_cast<long long>(FMath::RoundToInt64(Point.Y * 10.0)),
			static_cast<long long>(FMath::RoundToInt64(Point.Z * 10.0)));
	}

	FString CanonicalPointSequence(const TArray<FString>& Points, const bool bClosed)
	{
		if (Points.Num() == 0)
		{
			return FString();
		}
		TArray<FString> Reverse = Points;
		Algo::Reverse(Reverse);
		if (!bClosed)
		{
			const FString ForwardText = FString::Join(Points, TEXT(";"));
			const FString ReverseText = FString::Join(Reverse, TEXT(";"));
			return ForwardText < ReverseText ? ForwardText : ReverseText;
		}

		FString Best;
		const TArray<FString>* Orientations[] = {&Points, &Reverse};
		for (const TArray<FString>* Orientation : Orientations)
		{
			for (int32 Offset = 0; Offset < Orientation->Num(); ++Offset)
			{
				TArray<FString> Rotated;
				Rotated.Reserve(Orientation->Num());
				for (int32 Index = 0; Index < Orientation->Num(); ++Index)
				{
					Rotated.Add((*Orientation)[(Offset + Index) % Orientation->Num()]);
				}
				const FString Candidate = FString::Join(Rotated, TEXT(";"));
				if (Best.IsEmpty() || Candidate < Best)
				{
					Best = Candidate;
				}
			}
		}
		return Best;
	}

	struct FBoundaryEdgeKey
	{
		int32 A = INDEX_NONE;
		int32 B = INDEX_NONE;

		FBoundaryEdgeKey() = default;
		FBoundaryEdgeKey(const int32 InA, const int32 InB)
			: A(FMath::Min(InA, InB))
			, B(FMath::Max(InA, InB))
		{
		}

		bool operator==(const FBoundaryEdgeKey& Other) const
		{
			return A == Other.A && B == Other.B;
		}
	};

	uint32 GetTypeHash(const FBoundaryEdgeKey& Edge)
	{
		return HashCombineFast(::GetTypeHash(Edge.A), ::GetTypeHash(Edge.B));
	}

	struct FRoadBoundaryPolyline
	{
		TArray<FVector> Points;
		bool bClosed = false;
		double LengthCm = 0.0;
		FString Canonical;
	};

	double PolylineLength(const TArray<FVector>& Points, const bool bClosed)
	{
		double Length = 0.0;
		for (int32 Index = 1; Index < Points.Num(); ++Index)
		{
			Length += FVector::Distance(Points[Index - 1], Points[Index]);
		}
		if (bClosed && Points.Num() > 2)
		{
			Length += FVector::Distance(Points.Last(), Points[0]);
		}
		return Length;
	}

	FVector SamplePolylineAtDistance(
		const TArray<FVector>& Points,
		const bool bClosed,
		const double TargetDistance)
	{
		if (Points.Num() == 0)
		{
			return FVector::ZeroVector;
		}
		double Remaining = FMath::Max(TargetDistance, 0.0);
		const int32 SegmentCount = bClosed ? Points.Num() : Points.Num() - 1;
		for (int32 SegmentIndex = 0; SegmentIndex < SegmentCount; ++SegmentIndex)
		{
			const FVector& Start = Points[SegmentIndex];
			const FVector& End = Points[(SegmentIndex + 1) % Points.Num()];
			const double SegmentLength = FVector::Distance(Start, End);
			if (Remaining <= SegmentLength || SegmentIndex == SegmentCount - 1)
			{
				return FMath::Lerp(Start, End, SegmentLength > UE_SMALL_NUMBER
					? FMath::Clamp(Remaining / SegmentLength, 0.0, 1.0) : 0.0);
			}
			Remaining -= SegmentLength;
		}
		return Points.Last();
	}

	void ResamplePolyline(
		TArray<FVector>& Points,
		const bool bClosed,
		const float SampleDistanceCm)
	{
		const double Length = PolylineLength(Points, bClosed);
		if (Points.Num() < 2 || Length <= UE_SMALL_NUMBER || SampleDistanceCm <= UE_SMALL_NUMBER)
		{
			return;
		}
		const int32 SegmentCount = FMath::Max(
			bClosed ? 3 : 1,
			FMath::CeilToInt(Length / static_cast<double>(SampleDistanceCm)));
		const int32 SampleCount = bClosed ? SegmentCount : SegmentCount + 1;
		TArray<FVector> Resampled;
		Resampled.Reserve(SampleCount);
		for (int32 Index = 0; Index < SampleCount; ++Index)
		{
			const double Distance = Length * static_cast<double>(Index) / static_cast<double>(SegmentCount);
			Resampled.Add(SamplePolylineAtDistance(Points, bClosed, Distance));
		}
		Points = MoveTemp(Resampled);
	}

	bool PointInsideClosedPolyline2D(const FVector& Point, const TArray<FVector>& Polygon)
	{
		bool bInside = false;
		for (int32 Index = 0, Previous = Polygon.Num() - 1; Index < Polygon.Num(); Previous = Index++)
		{
			const FVector& A = Polygon[Index];
			const FVector& B = Polygon[Previous];
			const bool bCrosses = (A.Y > Point.Y) != (B.Y > Point.Y);
			if (bCrosses)
			{
				const double IntersectionX = (B.X - A.X) * (Point.Y - A.Y)
					/ (B.Y - A.Y) + A.X;
				if (Point.X < IntersectionX)
				{
					bInside = !bInside;
				}
			}
		}
		return bInside;
	}

	double AbsoluteArea2D(const TArray<FVector>& Points)
	{
		double TwiceArea = 0.0;
		for (int32 Index = 0; Index < Points.Num(); ++Index)
		{
			const FVector& A = Points[Index];
			const FVector& B = Points[(Index + 1) % Points.Num()];
			TwiceArea += A.X * B.Y - B.X * A.Y;
		}
		return FMath::Abs(TwiceArea) * 0.5;
	}

	bool ExtractAllRoadBoundaryPolylines(
		AActor* Network,
		const float MinPolylineLengthCm,
		const float VertexWeldToleranceCm,
		const bool bIncludeIntersectionPatches,
		const bool bEnableSplineResample,
		const float SplineResampleDistanceCm,
		TArray<FRoadBoundaryPolyline>& OutPolylines,
		FString& OutError)
	{
		OutPolylines.Reset();
		TInlineComponentArray<UQuick_RoadLayoutRoadMeshComponent*> Meshes;
		if (Network)
		{
			Network->GetComponents(Meshes);
		}
		Meshes.RemoveAll([bIncludeIntersectionPatches](const UQuick_RoadLayoutRoadMeshComponent* Mesh)
		{
			return !Mesh || Mesh->GetNumSections() == 0
				|| (!bIncludeIntersectionPatches
					&& Mesh->ComponentHasTag(Quick_RoadLayoutRoadTags::IntersectionPatch));
		});
		Meshes.Sort([](const UQuick_RoadLayoutRoadMeshComponent& A, const UQuick_RoadLayoutRoadMeshComponent& B)
		{
			return A.GetFName().LexicalLess(B.GetFName());
		});
		if (Meshes.Num() == 0)
		{
			OutError = TEXT("owned road network has no boundary-source mesh components");
			return false;
		}

		TMap<FIntVector, TArray<int32>> WeldedVertexBuckets;
		TArray<FVector> WeldedVertices;
		TMap<FBoundaryEdgeKey, int32> EdgeUseCounts;
		const double InverseTolerance = 1.0 / static_cast<double>(VertexWeldToleranceCm);
		auto ResolveVertex = [&](const FVector& WorldPosition)
		{
			const FIntVector Key(
				FMath::RoundToInt(WorldPosition.X * InverseTolerance),
				FMath::RoundToInt(WorldPosition.Y * InverseTolerance),
				FMath::RoundToInt(WorldPosition.Z * InverseTolerance));
			int32 BestExisting = INDEX_NONE;
			for (int32 OffsetZ = -1; OffsetZ <= 1; ++OffsetZ)
			{
				for (int32 OffsetY = -1; OffsetY <= 1; ++OffsetY)
				{
					for (int32 OffsetX = -1; OffsetX <= 1; ++OffsetX)
					{
						const TArray<int32>* Bucket = WeldedVertexBuckets.Find(
							Key + FIntVector(OffsetX, OffsetY, OffsetZ));
						if (!Bucket)
						{
							continue;
						}
						for (const int32 ExistingIndex : *Bucket)
						{
							if (FVector::DistSquared(WeldedVertices[ExistingIndex], WorldPosition)
								<= FMath::Square(static_cast<double>(VertexWeldToleranceCm))
								&& (BestExisting == INDEX_NONE || ExistingIndex < BestExisting))
							{
								BestExisting = ExistingIndex;
							}
						}
					}
				}
			}
			if (BestExisting != INDEX_NONE)
			{
				return BestExisting;
			}
			const int32 NewIndex = WeldedVertices.Add(WorldPosition);
			WeldedVertexBuckets.FindOrAdd(Key).Add(NewIndex);
			return NewIndex;
		};

		for (UQuick_RoadLayoutRoadMeshComponent* Mesh : Meshes)
		{
			const FTransform ComponentToWorld = Mesh->GetComponentTransform();
			for (int32 SectionIndex = 0; SectionIndex < Mesh->GetNumSections(); ++SectionIndex)
			{
				const FProcMeshSection* Section = Mesh->GetProcMeshSection(SectionIndex);
				if (!Section)
				{
					continue;
				}
				TArray<int32> SectionVertexIndices;
				SectionVertexIndices.Reserve(Section->ProcVertexBuffer.Num());
				for (const FProcMeshVertex& Vertex : Section->ProcVertexBuffer)
				{
					SectionVertexIndices.Add(ResolveVertex(ComponentToWorld.TransformPosition(Vertex.Position)));
				}
				for (int32 TriangleIndex = 0; TriangleIndex + 2 < Section->ProcIndexBuffer.Num(); TriangleIndex += 3)
				{
					const int32 LocalA = static_cast<int32>(Section->ProcIndexBuffer[TriangleIndex]);
					const int32 LocalB = static_cast<int32>(Section->ProcIndexBuffer[TriangleIndex + 1]);
					const int32 LocalC = static_cast<int32>(Section->ProcIndexBuffer[TriangleIndex + 2]);
					if (!SectionVertexIndices.IsValidIndex(LocalA)
						|| !SectionVertexIndices.IsValidIndex(LocalB)
						|| !SectionVertexIndices.IsValidIndex(LocalC))
					{
						continue;
					}
					const int32 TriangleVertices[] = {
						SectionVertexIndices[LocalA], SectionVertexIndices[LocalB], SectionVertexIndices[LocalC]};
					for (int32 EdgeIndex = 0; EdgeIndex < 3; ++EdgeIndex)
					{
						const int32 A = TriangleVertices[EdgeIndex];
						const int32 B = TriangleVertices[(EdgeIndex + 1) % 3];
						if (A != B)
						{
							++EdgeUseCounts.FindOrAdd(FBoundaryEdgeKey(A, B));
						}
					}
				}
			}
		}

		TSet<FBoundaryEdgeKey> UnusedBoundaryEdges;
		TMap<int32, TArray<int32>> Adjacency;
		for (const TPair<FBoundaryEdgeKey, int32>& Pair : EdgeUseCounts)
		{
			if (Pair.Value != 1)
			{
				continue;
			}
			UnusedBoundaryEdges.Add(Pair.Key);
			Adjacency.FindOrAdd(Pair.Key.A).AddUnique(Pair.Key.B);
			Adjacency.FindOrAdd(Pair.Key.B).AddUnique(Pair.Key.A);
		}
		for (TPair<int32, TArray<int32>>& Pair : Adjacency)
		{
			Pair.Value.Sort([&WeldedVertices](const int32 A, const int32 B)
			{
				return QuantizedPoint(WeldedVertices[A]) < QuantizedPoint(WeldedVertices[B]);
			});
		}

		while (UnusedBoundaryEdges.Num() > 0)
		{
			TArray<FBoundaryEdgeKey> CandidateEdges = UnusedBoundaryEdges.Array();
			CandidateEdges.Sort([&WeldedVertices](const FBoundaryEdgeKey& Left, const FBoundaryEdgeKey& Right)
			{
				const FString LeftKey = QuantizedPoint(WeldedVertices[Left.A]) + TEXT("|")
					+ QuantizedPoint(WeldedVertices[Left.B]);
				const FString RightKey = QuantizedPoint(WeldedVertices[Right.A]) + TEXT("|")
					+ QuantizedPoint(WeldedVertices[Right.B]);
				return LeftKey < RightKey;
			});
			const FBoundaryEdgeKey SeedEdge = CandidateEdges[0];
			int32 StartVertex = SeedEdge.A;
			if (Adjacency.FindRef(SeedEdge.B).Num() == 1
				&& Adjacency.FindRef(SeedEdge.A).Num() != 1)
			{
				StartVertex = SeedEdge.B;
			}

			FRoadBoundaryPolyline Polyline;
			Polyline.Points.Add(WeldedVertices[StartVertex]);
			int32 CurrentVertex = StartVertex;
			while (true)
			{
				const TArray<int32>* Neighbors = Adjacency.Find(CurrentVertex);
				if (!Neighbors)
				{
					break;
				}
				int32 NextVertex = INDEX_NONE;
				for (const int32 Neighbor : *Neighbors)
				{
					if (UnusedBoundaryEdges.Contains(FBoundaryEdgeKey(CurrentVertex, Neighbor)))
					{
						NextVertex = Neighbor;
						break;
					}
				}
				if (NextVertex == INDEX_NONE)
				{
					break;
				}
				UnusedBoundaryEdges.Remove(FBoundaryEdgeKey(CurrentVertex, NextVertex));
				if (NextVertex == StartVertex)
				{
					Polyline.bClosed = Polyline.Points.Num() >= 3;
					break;
				}
				Polyline.Points.Add(WeldedVertices[NextVertex]);
				CurrentVertex = NextVertex;
			}
			Polyline.LengthCm = PolylineLength(Polyline.Points, Polyline.bClosed);
			if (Polyline.Points.Num() >= 2 && Polyline.LengthCm >= MinPolylineLengthCm)
			{
				OutPolylines.Add(MoveTemp(Polyline));
			}
		}

		TArray<bool> bInnerLoop;
		bInnerLoop.Init(false, OutPolylines.Num());
		for (int32 Index = 0; Index < OutPolylines.Num(); ++Index)
		{
			if (!OutPolylines[Index].bClosed || OutPolylines[Index].Points.Num() < 3)
			{
				continue;
			}
			const double Area = AbsoluteArea2D(OutPolylines[Index].Points);
			for (int32 OtherIndex = 0; OtherIndex < OutPolylines.Num(); ++OtherIndex)
			{
				if (Index != OtherIndex && OutPolylines[OtherIndex].bClosed
					&& AbsoluteArea2D(OutPolylines[OtherIndex].Points) > Area
					&& PointInsideClosedPolyline2D(
						OutPolylines[Index].Points[0], OutPolylines[OtherIndex].Points))
				{
					bInnerLoop[Index] = true;
					break;
				}
			}
		}
		for (int32 Index = OutPolylines.Num() - 1; Index >= 0; --Index)
		{
			if (bInnerLoop[Index])
			{
				OutPolylines.RemoveAt(Index);
			}
		}

		for (FRoadBoundaryPolyline& Polyline : OutPolylines)
		{
			if (bEnableSplineResample)
			{
				ResamplePolyline(Polyline.Points, Polyline.bClosed, SplineResampleDistanceCm);
			}
			TArray<FString> CanonicalPoints;
			for (const FVector& Point : Polyline.Points)
			{
				CanonicalPoints.Add(QuantizedPoint(Point));
			}
			Polyline.Canonical = CanonicalPointSequence(CanonicalPoints, Polyline.bClosed);
		}
		OutPolylines.Sort([](const FRoadBoundaryPolyline& A, const FRoadBoundaryPolyline& B)
		{
			return A.Canonical < B.Canonical;
		});
		if (OutPolylines.Num() == 0)
		{
			OutError = TEXT("no complete outer road boundary polylines passed the extraction thresholds");
			return false;
		}
		return true;
	}

	bool AddBoundarySplineComponents(
		AActor* EdgeActor,
		const TArray<FRoadBoundaryPolyline>& Polylines,
		int32& OutSplineCount)
	{
		OutSplineCount = 0;
		if (!EdgeActor || Polylines.Num() == 0)
		{
			return false;
		}
		TInlineComponentArray<USplineComponent*> ExistingSplines;
		EdgeActor->GetComponents(ExistingSplines);
		ExistingSplines.Sort([](const USplineComponent& A, const USplineComponent& B)
		{
			return A.GetFName().LexicalLess(B.GetFName());
		});
		for (int32 Index = 1; Index < ExistingSplines.Num(); ++Index)
		{
			ExistingSplines[Index]->DestroyComponent();
		}
		for (int32 PolylineIndex = 0; PolylineIndex < Polylines.Num(); ++PolylineIndex)
		{
			USplineComponent* Spline = PolylineIndex == 0 && ExistingSplines.Num() > 0
				? ExistingSplines[0]
				: NewObject<USplineComponent>(
					EdgeActor,
					FName(*FString::Printf(TEXT("RoadBoundarySpline_%03d"), PolylineIndex)),
					RF_Transactional);
			if (!Spline)
			{
				return false;
			}
			if (PolylineIndex > 0 || ExistingSplines.Num() == 0)
			{
				EdgeActor->AddInstanceComponent(Spline);
				if (USceneComponent* Root = EdgeActor->GetRootComponent())
				{
					Spline->SetupAttachment(Root);
				}
				else
				{
					EdgeActor->SetRootComponent(Spline);
				}
				Spline->RegisterComponent();
			}
			Spline->Modify();
			Spline->ComponentTags.AddUnique(Quick_RoadLayoutSplineTags::LayoutSpline);
			Spline->ComponentTags.AddUnique(Quick_RoadLayoutSplineTags::RoadEdge);
			Spline->ClearSplinePoints(false);
			for (const FVector& Point : Polylines[PolylineIndex].Points)
			{
				Spline->AddSplinePoint(Point, ESplineCoordinateSpace::World, false);
			}
			for (int32 PointIndex = 0; PointIndex < Spline->GetNumberOfSplinePoints(); ++PointIndex)
			{
				Spline->SetSplinePointType(PointIndex, ESplinePointType::Linear, false);
			}
			Spline->SetClosedLoop(Polylines[PolylineIndex].bClosed, false);
			Spline->UpdateSpline();
			++OutSplineCount;
		}
		return OutSplineCount == Polylines.Num();
	}

	FString CanonicalSplineDigestForActor(const AActor* Actor, int32& OutSplineCount)
	{
		OutSplineCount = 0;
		TArray<FString> CanonicalSplines;
		TInlineComponentArray<USplineComponent*> Splines;
		if (Actor)
		{
			Actor->GetComponents(Splines);
		}
		for (const USplineComponent* Spline : Splines)
		{
			if (!Spline || Spline->GetNumberOfSplinePoints() == 0)
			{
				continue;
			}
			TArray<FString> Points;
			for (int32 Index = 0; Index < Spline->GetNumberOfSplinePoints(); ++Index)
			{
				Points.Add(QuantizedPoint(Spline->GetLocationAtSplinePoint(Index, ESplineCoordinateSpace::World)));
			}
			CanonicalSplines.Add(CanonicalPointSequence(Points, Spline->IsClosedLoop()));
			++OutSplineCount;
		}
		CanonicalSplines.Sort();
		return Sha256String(FString::Join(CanonicalSplines, TEXT("\n")));
	}

	FString CanonicalSplineDigest(UWorld* World, int32& OutSplineCount)
	{
		OutSplineCount = 0;
		TArray<FString> CanonicalSplines;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (!It->ActorHasTag(OwnerTag) || !It->ActorHasTag(EdgeCategoryTag))
			{
				continue;
			}
			TInlineComponentArray<USplineComponent*> Splines;
			It->GetComponents(Splines);
			for (USplineComponent* Spline : Splines)
			{
				if (!Spline)
				{
					continue;
				}
				TArray<FString> Points;
				for (int32 Index = 0; Index < Spline->GetNumberOfSplinePoints(); ++Index)
				{
					Points.Add(QuantizedPoint(Spline->GetLocationAtSplinePoint(
						Index, ESplineCoordinateSpace::World)));
				}
				if (Points.Num() > 0)
				{
					CanonicalSplines.Add(CanonicalPointSequence(Points, Spline->IsClosedLoop()));
					++OutSplineCount;
				}
			}
		}
		CanonicalSplines.Sort();
		return Sha256String(FString::Join(CanonicalSplines, TEXT("\n")));
	}

	void CountRoadGeometry(AActor* Network, int32& OutTriangles, int32& OutIntersectionPatches)
	{
		OutTriangles = 0;
		OutIntersectionPatches = 0;
		TInlineComponentArray<UQuick_RoadLayoutRoadMeshComponent*> Meshes;
		if (Network)
		{
			Network->GetComponents(Meshes);
		}
		for (UQuick_RoadLayoutRoadMeshComponent* Mesh : Meshes)
		{
			if (!Mesh)
			{
				continue;
			}
			if (Mesh->ComponentHasTag(Quick_RoadLayoutRoadTags::IntersectionPatch))
			{
				++OutIntersectionPatches;
			}
			for (int32 SectionIndex = 0; SectionIndex < Mesh->GetNumSections(); ++SectionIndex)
			{
				if (const FProcMeshSection* Section = Mesh->GetProcMeshSection(SectionIndex))
				{
					OutTriangles += Section->ProcIndexBuffer.Num() / 3;
				}
			}
		}
	}
}

FString UQuick_RoadAutomationLibrary::ResetRoadInfrastructure()
{
	using namespace QuickRoadAutomation;
	constexpr TCHAR Operation[] = TEXT("ResetRoadInfrastructure");
	UWorld* World = nullptr;
	FString Error;
	if (!ValidateB1MutationContext(World, Error))
	{
		return ErrorJson(Operation, Error);
	}

	int32 Removed = 0;
	TArray<AActor*> ToRemove;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor && Actor->ActorHasTag(OwnerTag)
			&& (Actor->ActorHasTag(NetworkCategoryTag)
				|| Actor->ActorHasTag(EdgeCategoryTag)
				|| Actor->ActorHasTag(BakeCategoryTag)))
		{
			ToRemove.Add(Actor);
		}
	}
	for (AActor* Actor : ToRemove)
	{
		Actor->Modify();
		if (World->DestroyActor(Actor))
		{
			++Removed;
		}
	}
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		TInlineComponentArray<USplineComponent*> Splines;
		It->GetComponents(Splines);
		for (USplineComponent* Spline : Splines)
		{
			if (!Spline || Spline->GetOwner()->GetLevel() != World->GetCurrentLevel())
			{
				continue;
			}
			const int32 PreviousTagCount = Spline->ComponentTags.Num();
			Spline->Modify();
			Spline->ComponentTags.RemoveAll([](const FName Tag) { return IsWidthGroupTag(Tag); });
			if (Spline->ComponentTags.Num() != PreviousTagCount)
			{
				Spline->MarkPackageDirty();
			}
		}
	}
	TSharedRef<FJsonObject> Result = MakeResult(true, Operation);
	Result->SetNumberField(TEXT("removed_actor_count"), Removed);
	Result->SetStringField(TEXT("error"), TEXT(""));
	return ToJson(Result);
}

FString UQuick_RoadAutomationLibrary::EnsureLandscapeInfrastructure(
	const FString& ActorLabel,
	const int32 QuadsPerSection,
	const int32 SectionsPerComponent,
	const FIntPoint ComponentCount,
	const FVector DesiredGridCenterWorld,
	const FRotator DesiredRotationWorld,
	const FVector DesiredScale,
	const FName EditLayerName,
	const FString& MaterialAssetPath,
	const FString& AbsoluteHeightmapPath)
{
	using namespace QuickRoadAutomation;
	constexpr TCHAR Operation[] = TEXT("EnsureLandscapeInfrastructure");
	UWorld* World = nullptr;
	FString Error;
	if (!ValidateB1MutationContext(World, Error))
	{
		return ErrorJson(Operation, Error);
	}
	if (ActorLabel.IsEmpty() || ActorLabel.TrimStartAndEnd() != ActorLabel || EditLayerName.IsNone())
	{
		return ErrorJson(Operation, TEXT("actor label/edit layer name must be nonblank and trimmed"));
	}
	if (DesiredGridCenterWorld.ContainsNaN() || DesiredRotationWorld.ContainsNaN()
		|| DesiredScale.ContainsNaN() || DesiredScale.GetMin() <= KINDA_SMALL_NUMBER)
	{
		return ErrorJson(Operation, TEXT("Landscape center/rotation/scale must be finite and scale positive"));
	}
	UMaterialInterface* Material = LoadMaterial(MaterialAssetPath, Error);
	if (!Material)
	{
		return ErrorJson(Operation, Error);
	}
	if (AbsoluteHeightmapPath.IsEmpty() || FPaths::IsRelative(AbsoluteHeightmapPath))
	{
		return ErrorJson(Operation, TEXT("heightmap path must be an absolute path"));
	}
	FString NormalizedHeightmap = FPaths::ConvertRelativePathToFull(AbsoluteHeightmapPath);
	FPaths::NormalizeFilename(NormalizedHeightmap);
	if (!IFileManager::Get().FileExists(*NormalizedHeightmap))
	{
		return ErrorJson(Operation, FString::Printf(TEXT("heightmap does not exist: %s"), *NormalizedHeightmap));
	}

	int32 ResolutionX = 0;
	int32 ResolutionY = 0;
	int32 TotalComponents = 0;
	FQuick_RoadLandscapeCreator::ComputeGridStats(
		QuadsPerSection, SectionsPerComponent, ComponentCount,
		ResolutionX, ResolutionY, TotalComponents);
	if (!FQuick_RoadLandscapeCreator::IsValidSectionSize(QuadsPerSection)
		|| !FQuick_RoadLandscapeCreator::IsValidSectionsPerComponent(SectionsPerComponent)
		|| ComponentCount.X < 1 || ComponentCount.Y < 1 || ComponentCount.X > 32 || ComponentCount.Y > 32)
	{
		return ErrorJson(Operation, TEXT("invalid Landscape topology"));
	}

	const TArray<AActor*> LabelMatches = FindActorsByExactLabel(World, ActorLabel);
	if (LabelMatches.Num() > 1 || (LabelMatches.Num() == 1 && !Cast<ALandscape>(LabelMatches[0])))
	{
		return ErrorJson(Operation, TEXT("Landscape actor label is duplicated or owned by another actor class"));
	}
	ALandscape* Landscape = LabelMatches.Num() == 1 ? Cast<ALandscape>(LabelMatches[0]) : nullptr;
	bool bCreated = false;
	if (!Landscape)
	{
		const TSet<ALandscape*> Before = SnapshotLandscapes(World);
		FQuick_RoadGenerateTerrainSettings Settings;
		Settings.Material = Material;
		Settings.QuadsPerSection = QuadsPerSection;
		Settings.SectionsPerComponent = SectionsPerComponent;
		Settings.ComponentCount = ComponentCount;
		Settings.HeightmapFilePath = NormalizedHeightmap;
		FText CreationError;
		const bool bCreateResult = FQuick_RoadLandscapeCreator::TryCreateLandscape(World, Settings, CreationError);
		TArray<ALandscape*> NewLandscapes = DiffLandscapes(World, Before);
		if (!bCreateResult || NewLandscapes.Num() != 1)
		{
			DestroyNewLandscapes(World, NewLandscapes);
			return ErrorJson(
				Operation,
				FString::Printf(TEXT("Landscape creation failed or was ambiguous (%s; new=%d)"),
					*CreationError.ToString(), NewLandscapes.Num()));
		}
		Landscape = NewLandscapes[0];
		bCreated = true;
	}

	const int32 ExpectedComponentSizeQuads = QuadsPerSection * SectionsPerComponent;
	TSet<int32> ComponentBasesX;
	TSet<int32> ComponentBasesY;
	for (ULandscapeComponent* Component : Landscape->LandscapeComponents)
	{
		if (Component)
		{
			ComponentBasesX.Add(Component->GetSectionBase().X);
			ComponentBasesY.Add(Component->GetSectionBase().Y);
		}
	}
	const bool bTopologyMatches = Landscape->ComponentSizeQuads == ExpectedComponentSizeQuads
		&& Landscape->NumSubsections == SectionsPerComponent
		&& Landscape->SubsectionSizeQuads == QuadsPerSection
		&& Landscape->LandscapeComponents.Num() == TotalComponents
		&& ComponentBasesX.Num() == ComponentCount.X
		&& ComponentBasesY.Num() == ComponentCount.Y;
	if (!bTopologyMatches)
	{
		if (bCreated)
		{
			World->DestroyActor(Landscape);
		}
		return ErrorJson(Operation, TEXT("existing/created Landscape topology is incompatible"));
	}

	FString ExistingHeightmap = FPaths::ConvertRelativePathToFull(Landscape->ReimportHeightmapFilePath);
	FPaths::NormalizeFilename(ExistingHeightmap);
	if (!bCreated && !ExistingHeightmap.Equals(NormalizedHeightmap, ESearchCase::IgnoreCase))
	{
		return ErrorJson(Operation, TEXT("existing Landscape heightmap source does not match requested file"));
	}

	const FVector HalfGrid((ResolutionX - 1) * 0.5, (ResolutionY - 1) * 0.5, 0.0);
	const FTransform RotationScale(DesiredRotationWorld, FVector::ZeroVector, DesiredScale);
	const FVector ActorOriginWorld = DesiredGridCenterWorld - RotationScale.TransformVector(HalfGrid);
	Landscape->Modify();
	Landscape->SetActorTransform(FTransform(DesiredRotationWorld, ActorOriginWorld, DesiredScale));
	Landscape->SetActorLabel(ActorLabel);
	Landscape->LandscapeMaterial = Material;
	Landscape->ReimportHeightmapFilePath = NormalizedHeightmap;
	const FVector ReconstructedCenter = Landscape->GetActorTransform().TransformPosition(HalfGrid);
	if (!ReconstructedCenter.Equals(DesiredGridCenterWorld, 0.1))
	{
		if (bCreated)
		{
			World->DestroyActor(Landscape);
		}
		return ErrorJson(Operation, TEXT("Landscape grid center reconstruction failed"));
	}

	const ULandscapeEditLayerBase* RoadLayer = Landscape->GetEditLayerConst(EditLayerName);
	if (!RoadLayer)
	{
		const int32 LayerIndex = Landscape->CreateLayer(EditLayerName);
		if (LayerIndex == INDEX_NONE)
		{
			if (bCreated)
			{
				World->DestroyActor(Landscape);
			}
			return ErrorJson(Operation, TEXT("could not create Landscape road edit layer"));
		}
		RoadLayer = Landscape->GetEditLayerConst(LayerIndex);
	}
	if (!RoadLayer || !Cast<ULandscapeEditLayer>(RoadLayer) || RoadLayer->IsLocked())
	{
		if (bCreated)
		{
			World->DestroyActor(Landscape);
		}
		return ErrorJson(Operation, TEXT("Landscape road edit layer is missing, nonstandard, or locked"));
	}
	ULandscapeEditLayerBase* MutableRoadLayer = Landscape->GetEditLayer(EditLayerName);
	MutableRoadLayer->SetVisible(true, true);
	MutableRoadLayer->SetAlphaForTargetType(
		ELandscapeToolTargetType::Heightmap, 1.0f, true, EPropertyChangeType::ValueSet);
	Landscape->SetEditingLayer(RoadLayer->GetGuid());
	Landscape->MarkPackageDirty();
	Landscape->PostEditChange();

	TSharedRef<FJsonObject> Result = MakeResult(true, Operation);
	Result->SetStringField(TEXT("actor_label"), ActorLabel);
	Result->SetBoolField(TEXT("created"), bCreated);
	Result->SetNumberField(TEXT("resolution_x"), ResolutionX);
	Result->SetNumberField(TEXT("resolution_y"), ResolutionY);
	Result->SetNumberField(TEXT("component_count"), TotalComponents);
	Result->SetStringField(TEXT("edit_layer"), EditLayerName.ToString());
	Result->SetStringField(TEXT("material"), Material->GetPathName());
	Result->SetStringField(TEXT("heightmap"), NormalizedHeightmap);
	Result->SetStringField(TEXT("error"), TEXT(""));
	return ToJson(Result);
}

FString UQuick_RoadAutomationLibrary::GenerateRoadNetwork(
	const FName RoadTag,
	const float RoadWidthCm,
	const float InfluenceFalloffCm,
	const float InfluenceBlendStrength,
	const float RoadMeshSampleDistanceCm,
	const int32 RoadMeshWidthSubdivisions,
	const bool bAdaptiveCurvature,
	const float CurvatureThresholdDeg,
	const int32 MaxCurvatureSubdivisions,
	const float IntersectionExtendLengthCm,
	const float IntersectionDetectSampleCm,
	const FString& RoadMaterialPath)
{
	using namespace QuickRoadAutomation;
	constexpr TCHAR Operation[] = TEXT("GenerateRoadNetwork");
	UWorld* World = nullptr;
	FString Error;
	if (!ValidateB1MutationContext(World, Error))
	{
		return ErrorJson(Operation, Error);
	}
	if (!Quick_RoadLayoutRoadTags::IsValidUserRoadTag(RoadTag)
		|| !FMath::IsFinite(RoadWidthCm) || RoadWidthCm < 100.0f
		|| !FMath::IsFinite(InfluenceFalloffCm) || InfluenceFalloffCm < 0.0f
		|| !FMath::IsFinite(InfluenceBlendStrength) || InfluenceBlendStrength < 0.0f || InfluenceBlendStrength > 1.0f
		|| !FMath::IsFinite(RoadMeshSampleDistanceCm) || RoadMeshSampleDistanceCm < 25.0f
		|| RoadMeshWidthSubdivisions < 1 || RoadMeshWidthSubdivisions > 16
		|| !FMath::IsFinite(CurvatureThresholdDeg) || CurvatureThresholdDeg < 1.0f || CurvatureThresholdDeg > 45.0f
		|| MaxCurvatureSubdivisions < 0 || MaxCurvatureSubdivisions > 6
		|| !FMath::IsFinite(IntersectionExtendLengthCm) || IntersectionExtendLengthCm < 50.0f
		|| !FMath::IsFinite(IntersectionDetectSampleCm) || IntersectionDetectSampleCm < 25.0f)
	{
		return ErrorJson(Operation, TEXT("road generation parameters are outside the supported finite ranges"));
	}
	UMaterialInterface* RoadMaterial = LoadMaterial(RoadMaterialPath, Error);
	if (!RoadMaterial)
	{
		return ErrorJson(Operation, Error);
	}
	if (!RejectUnownedMatchingNetworks(World, RoadTag, Error))
	{
		return ErrorJson(Operation, Error);
	}

	TArray<USplineComponent*> Splines;
	FQuick_RoadLayoutRoadGenerator::CollectMainRoadSplines(World, Splines, RoadTag);
	if (Splines.Num() == 0)
	{
		return ErrorJson(Operation, TEXT("no input splines match the requested road tag"));
	}
	const float HalfWidthCm = RoadWidthCm * 0.5f;
	const FName RequestedWidthGroupTag = WidthGroupTag(RoadTag);
	for (USplineComponent* Spline : Splines)
	{
		if (!Spline)
		{
			return ErrorJson(Operation, TEXT("road tag resolved a null spline"));
		}
		for (const FName ExistingTag : Spline->ComponentTags)
		{
			if (IsWidthGroupTag(ExistingTag) && ExistingTag != RequestedWidthGroupTag)
			{
				return ErrorJson(Operation, TEXT("a spline is already assigned to a different road-width group"));
			}
		}
		Spline->Modify();
		Spline->ComponentTags.AddUnique(RequestedWidthGroupTag);
		UQuick_RoadRoadSplineWidthComponent::SetHalfWidthCm(Spline, HalfWidthCm);
		if (!FMath::IsNearlyEqual(
			UQuick_RoadRoadSplineWidthComponent::ResolveHalfWidthCm(Spline, HalfWidthCm),
			HalfWidthCm,
			0.01f))
		{
			return ErrorJson(Operation, TEXT("road spline width verification failed"));
		}
	}

	FQuick_RoadLayoutMainRoadStageParams StageParams;
	StageParams.SampleDistanceCm = FMath::Max(RoadMeshSampleDistanceCm, 25.0f);
	StageParams.MainRoadWidthCm = RoadWidthCm;
	StageParams.InfluenceFalloffCm = InfluenceFalloffCm;
	StageParams.InfluenceBlendStrength = InfluenceBlendStrength;
	StageParams.RoadTag = RoadTag;
	StageParams.DefaultRoadMaterial = RoadMaterial;
	StageParams.RoadMeshSampleDistanceCm = RoadMeshSampleDistanceCm;
	StageParams.RoadMeshWidthSubdivisions = RoadMeshWidthSubdivisions;
	StageParams.bRoadMeshAdaptiveCurvature = bAdaptiveCurvature;
	StageParams.RoadMeshCurvatureThresholdDeg = CurvatureThresholdDeg;
	StageParams.RoadMeshMaxCurvatureSubdivisions = MaxCurvatureSubdivisions;
	FQuick_RoadLayoutMainRoadIntersectionParams IntersectionParams;
	IntersectionParams.RoadTag = RoadTag;
	IntersectionParams.ExtendLengthCm = IntersectionExtendLengthCm;
	IntersectionParams.IntersectionDetectSampleCm = IntersectionDetectSampleCm;

	FScopedActorSelectionRestore SelectionRestore;
	TArray<FQuick_RoadLayoutRoadResult> Results;
	FText PluginError;
	if (!FQuick_RoadLayoutRoadGenerator::GenerateRoadMeshes(
		World, StageParams, IntersectionParams, Results, PluginError))
	{
		return ErrorJson(Operation, PluginError.ToString());
	}
	if (Results.Num() != 1 || !Results[0].RoadActor)
	{
		return ErrorJson(Operation, TEXT("QuickRoad generation did not return exactly one road network"));
	}
	AActor* Network = Results[0].RoadActor;
	Network->Modify();
	Network->Tags.AddUnique(OwnerTag);
	Network->Tags.AddUnique(NetworkCategoryTag);
	Network->Tags.AddUnique(SourceCategoryTag(RoadTag));
	Network->SetActorLabel(TEXT("QS_B1_QR_Network_") + SanitizedTag(RoadTag));
	int32 MaterializedMeshCount = 0;
	if (!ApplyRoadMaterial(Network, RoadMaterial, MaterializedMeshCount))
	{
		return ErrorJson(Operation, TEXT("generated road network has no materializable road mesh"));
	}
	if (UQuick_RoadRoadInfluenceComponent* Influence =
		Network->FindComponentByClass<UQuick_RoadRoadInfluenceComponent>())
	{
		Influence->Modify();
		Influence->HalfWidthCm = HalfWidthCm;
		Influence->InfluenceFalloffCm = InfluenceFalloffCm;
		Influence->BlendStrength = InfluenceBlendStrength;
		Influence->RoadSplineTag = RoadTag;
		Influence->ExtendLengthCm = IntersectionExtendLengthCm;
		Influence->IntersectionDetectSampleCm = IntersectionDetectSampleCm;
		Influence->RibbonParams = FQuick_RoadLayoutRoadGenerator::MakeRibbonBuildParamsFromStage(StageParams);
	}
	else
	{
		return ErrorJson(Operation, TEXT("generated road network has no influence component"));
	}
	Network->MarkPackageDirty();

	TSharedRef<FJsonObject> Result = MakeResult(true, Operation);
	Result->SetStringField(TEXT("road_tag"), RoadTag.ToString());
	Result->SetNumberField(TEXT("road_width_cm"), RoadWidthCm);
	Result->SetNumberField(TEXT("spline_count"), Results[0].SplineCount);
	Result->SetNumberField(TEXT("triangle_count"), Results[0].TriangleCount);
	Result->SetNumberField(TEXT("ribbon_segment_count"), Results[0].RibbonSegmentCount);
	Result->SetNumberField(TEXT("mesh_component_count"), MaterializedMeshCount);
	Result->SetStringField(TEXT("material"), RoadMaterial->GetPathName());
	Result->SetStringField(TEXT("network_label"), Network->GetActorLabel());
	Result->SetStringField(TEXT("error"), TEXT(""));
	return ToJson(Result);
}

FString UQuick_RoadAutomationLibrary::RebuildRoadIntersections(
	const FName RoadTagExpression,
	const float ExtendLengthCm,
	const float DetectSampleCm,
	const float GridCellSizeCm,
	const bool bGridRebuild,
	const float CornerRadiusScale,
	const float RoadMeshSampleDistanceCm,
	const int32 RoadMeshWidthSubdivisions,
	const bool bAdaptiveCurvature,
	const float CurvatureThresholdDeg,
	const int32 MaxCurvatureSubdivisions,
	const FString& RoadMaterialPath)
{
	using namespace QuickRoadAutomation;
	constexpr TCHAR Operation[] = TEXT("RebuildRoadIntersections");
	UWorld* World = nullptr;
	FString Error;
	if (!ValidateB1MutationContext(World, Error))
	{
		return ErrorJson(Operation, Error);
	}
	TArray<FName> SourceTags;
	Quick_RoadLayoutRoadTags::ParseRoadTagExpression(RoadTagExpression, SourceTags);
	if (SourceTags.Num() != 3
		|| !FMath::IsFinite(ExtendLengthCm) || ExtendLengthCm < 50.0f
		|| !FMath::IsFinite(DetectSampleCm) || DetectSampleCm < 25.0f
		|| !FMath::IsFinite(GridCellSizeCm) || GridCellSizeCm < 10.0f
		|| !FMath::IsFinite(CornerRadiusScale) || CornerRadiusScale < 0.25f || CornerRadiusScale > 4.0f
		|| !FMath::IsFinite(RoadMeshSampleDistanceCm) || RoadMeshSampleDistanceCm < 25.0f
		|| RoadMeshWidthSubdivisions < 1 || RoadMeshWidthSubdivisions > 16
		|| !FMath::IsFinite(CurvatureThresholdDeg) || CurvatureThresholdDeg < 1.0f || CurvatureThresholdDeg > 45.0f
		|| MaxCurvatureSubdivisions < 0 || MaxCurvatureSubdivisions > 6)
	{
		return ErrorJson(Operation, TEXT("intersection rebuild requires exactly three source tags and valid finite settings"));
	}
	UMaterialInterface* RoadMaterial = LoadMaterial(RoadMaterialPath, Error);
	if (!RoadMaterial)
	{
		return ErrorJson(Operation, Error);
	}
	if (!RejectUnownedMatchingNetworks(World, RoadTagExpression, Error))
	{
		return ErrorJson(Operation, Error);
	}
	TArray<AActor*> MatchingNetworks;
	FQuick_RoadLayoutRoadGenerator::CollectRoadNetworkActorsByTag(World, RoadTagExpression, MatchingNetworks);
	if (MatchingNetworks.Num() != 3)
	{
		return ErrorJson(
			Operation,
			FString::Printf(TEXT("expected exactly three generated width-group networks, found %d"), MatchingNetworks.Num()));
	}

	float MaximumHalfWidth = 50.0f;
	for (const FName SourceTag : SourceTags)
	{
		TArray<USplineComponent*> Splines;
		FQuick_RoadLayoutRoadGenerator::CollectMainRoadSplines(World, Splines, SourceTag);
		if (Splines.Num() == 0)
		{
			return ErrorJson(Operation, TEXT("a source road tag has no splines"));
		}
		for (USplineComponent* Spline : Splines)
		{
			if (!UQuick_RoadRoadSplineWidthComponent::HasStoredHalfWidthCm(Spline))
			{
				return ErrorJson(Operation, TEXT("a source road spline has no explicit width component"));
			}
			MaximumHalfWidth = FMath::Max(
				MaximumHalfWidth,
				UQuick_RoadRoadSplineWidthComponent::ResolveHalfWidthCm(Spline, 50.0f));
		}
	}

	FQuick_RoadRoadSplitRebuildSettings Settings;
	Settings.ExtendLengthCm = ExtendLengthCm;
	Settings.IntersectionDetectSampleCm = DetectSampleCm;
	Settings.RibbonParams.HalfWidthCm = MaximumHalfWidth;
	Settings.RibbonParams.SampleDistanceCm = RoadMeshSampleDistanceCm;
	Settings.RibbonParams.WidthSubdivisions = RoadMeshWidthSubdivisions;
	Settings.RibbonParams.bAdaptiveCurvature = bAdaptiveCurvature;
	Settings.RibbonParams.CurvatureThresholdDeg = CurvatureThresholdDeg;
	Settings.RibbonParams.MaxCurvatureSubdivisions = MaxCurvatureSubdivisions;
	Settings.IntersectionGridCellSizeCm = GridCellSizeCm;
	Settings.bGridRebuildIntersectionPatches = bGridRebuild;
	Settings.IntersectionCornerRadiusScale = CornerRadiusScale;

	FScopedActorSelectionRestore SelectionRestore;
	FText PluginError;
	int32 RemovedNetworkCount = 0;
	int32 IntersectionPatchCount = 0;
	int32 RibbonSegmentCount = 0;
	if (!FQuick_RoadLayoutRoadGenerator::SplitAndRebuildConsolidatedRoadNetworks(
		World,
		RoadTagExpression,
		Settings,
		PluginError,
		&RemovedNetworkCount,
		&IntersectionPatchCount,
		&RibbonSegmentCount))
	{
		return ErrorJson(Operation, PluginError.ToString());
	}
	AActor* Network = FindUniqueOwnedNetwork(World, Error);
	if (!Network)
	{
		return ErrorJson(Operation, Error);
	}
	Network->Modify();
	Network->SetActorLabel(TEXT("QS_B1_QR_RoadNetwork"));
	Network->Tags.AddUnique(OwnerTag);
	Network->Tags.AddUnique(NetworkCategoryTag);
	for (const FName SourceTag : SourceTags)
	{
		Network->Tags.AddUnique(SourceCategoryTag(SourceTag));
	}
	int32 MaterializedMeshCount = 0;
	if (!ApplyRoadMaterial(Network, RoadMaterial, MaterializedMeshCount))
	{
		return ErrorJson(Operation, TEXT("consolidated road network has no materializable road mesh"));
	}
	if (UQuick_RoadRoadInfluenceComponent* Influence =
		Network->FindComponentByClass<UQuick_RoadRoadInfluenceComponent>())
	{
		Influence->Modify();
		Influence->HalfWidthCm = MaximumHalfWidth;
		Influence->ExtendLengthCm = ExtendLengthCm;
		Influence->IntersectionDetectSampleCm = DetectSampleCm;
		Influence->RibbonParams = Settings.RibbonParams;
	}
	else
	{
		return ErrorJson(Operation, TEXT("consolidated road network has no influence component"));
	}
	Network->MarkPackageDirty();

	TSharedRef<FJsonObject> Result = MakeResult(true, Operation);
	Result->SetStringField(TEXT("road_tag_expression"), RoadTagExpression.ToString());
	Result->SetStringField(TEXT("network_label"), Network->GetActorLabel());
	Result->SetNumberField(TEXT("source_width_group_count"), SourceTags.Num());
	Result->SetNumberField(TEXT("removed_network_count"), RemovedNetworkCount);
	Result->SetNumberField(TEXT("intersection_patch_count"), IntersectionPatchCount);
	Result->SetNumberField(TEXT("ribbon_segment_count"), RibbonSegmentCount);
	Result->SetNumberField(TEXT("mesh_component_count"), MaterializedMeshCount);
	Result->SetStringField(TEXT("material"), RoadMaterial->GetPathName());
	Result->SetStringField(TEXT("error"), TEXT(""));
	return ToJson(Result);
}

FString UQuick_RoadAutomationLibrary::ClearAndApplyAllRoadInfluence(
	const FString& LandscapeLabel,
	const FName EditLayerName,
	const float InfluenceFalloffCm,
	const float BlendStrength,
	const float VerticalOffsetCm,
	const int32 SmoothIterations,
	const float SmoothStrength)
{
	using namespace QuickRoadAutomation;
	constexpr TCHAR Operation[] = TEXT("ClearAndApplyAllRoadInfluence");
	UWorld* World = nullptr;
	FString Error;
	if (!ValidateB1MutationContext(World, Error))
	{
		return ErrorJson(Operation, Error);
	}
	if (!FMath::IsFinite(InfluenceFalloffCm) || InfluenceFalloffCm < 0.0f
		|| !FMath::IsFinite(BlendStrength) || BlendStrength < 0.0f || BlendStrength > 1.0f
		|| !FMath::IsFinite(VerticalOffsetCm)
		|| SmoothIterations < 0 || SmoothIterations > 32
		|| !FMath::IsFinite(SmoothStrength) || SmoothStrength < 0.0f || SmoothStrength > 1.0f)
	{
		return ErrorJson(Operation, TEXT("road influence parameters are outside the supported finite ranges"));
	}
	ALandscape* Landscape = FindExactLandscape(World, LandscapeLabel, Error);
	if (!Landscape)
	{
		return ErrorJson(Operation, Error);
	}
	const ULandscapeEditLayerBase* RoadLayer = nullptr;
	if (!ValidateRoadLayer(Landscape, EditLayerName, RoadLayer, Error))
	{
		return ErrorJson(Operation, Error);
	}
	AActor* Network = FindUniqueOwnedNetwork(World, Error);
	if (!Network)
	{
		return ErrorJson(Operation, Error);
	}

	TInlineComponentArray<UProceduralMeshComponent*> CandidateMeshes;
	Network->GetComponents(CandidateMeshes);
	TArray<UProceduralMeshComponent*> RoadMeshes;
	for (UProceduralMeshComponent* Mesh : CandidateMeshes)
	{
		if (Mesh && Mesh->GetNumSections() > 0)
		{
			RoadMeshes.Add(Mesh);
		}
	}
	FQuick_RoadRoadMeshInfluenceField MeshField;
	MeshField.BuildFromProcMeshComponents(RoadMeshes);
	if (MeshField.IsEmpty())
	{
		return ErrorJson(Operation, TEXT("owned road network has no influence geometry"));
	}
	ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();
	if (!LandscapeInfo)
	{
		return ErrorJson(Operation, TEXT("Landscape has no LandscapeInfo"));
	}
	int32 MinX = 0;
	int32 MinY = 0;
	int32 MaxX = 0;
	int32 MaxY = 0;
	TArray<uint16> PreviousRawDelta;
	if (!ReadRawEditLayerHeights(
		Landscape, RoadLayer, MinX, MinY, MaxX, MaxY, PreviousRawDelta))
	{
		return ErrorJson(Operation, TEXT("could not snapshot the existing raw road edit-layer delta"));
	}
	FScopedRoadLayerDeltaRollback DeltaRollback(
		Landscape, RoadLayer->GetGuid(), MinX, MinY, MaxX, MaxY, PreviousRawDelta);

	FScopedActorSelectionRestore SelectionRestore;
	Landscape->Modify();
	Landscape->ClearEditLayer(
		RoadLayer->GetGuid(), nullptr,
		ELandscapeToolTargetTypeFlags::Heightmap, true);
	if (!ForceHeightMerge(Landscape))
	{
		return ErrorJson(Operation, TEXT("Landscape remained pending after clearing the road edit layer"));
	}
	if (!ValidateRawLayerData(Landscape, RoadLayer->GetGuid()))
	{
		return ErrorJson(Operation, TEXT("road edit layer lacks persistent raw height data"));
	}

	const int32 Width = MaxX - MinX + 1;
	const int32 Height = MaxY - MinY + 1;
	TArray<uint16> BasePacked;
	BasePacked.Init(LandscapeDataAccess::GetTexHeight(0.0f), Width * Height);
	{
		FLandscapeEditDataInterface BaseLandscapeEdit(LandscapeInfo, FGuid(), false);
		int32 ReadMinX = MinX;
		int32 ReadMinY = MinY;
		int32 ReadMaxX = MaxX;
		int32 ReadMaxY = MaxY;
		BaseLandscapeEdit.GetHeightData(
			ReadMinX, ReadMinY, ReadMaxX, ReadMaxY, BasePacked.GetData(), Width);
		if (ReadMinX != MinX || ReadMinY != MinY || ReadMaxX != MaxX || ReadMaxY != MaxY)
		{
			return ErrorJson(Operation, TEXT("could not read the complete merged Landscape base height buffer"));
		}
	}

	TArray<float> BaseLocalHeights;
	TArray<float> DesiredFinalHeights;
	TArray<float> SmoothWeights;
	BaseLocalHeights.SetNumUninitialized(BasePacked.Num());
	DesiredFinalHeights.SetNumUninitialized(BasePacked.Num());
	SmoothWeights.Init(0.0f, BasePacked.Num());
	for (int32 Index = 0; Index < BasePacked.Num(); ++Index)
	{
		BaseLocalHeights[Index] = LandscapeDataAccess::GetLocalHeight(BasePacked[Index]);
		DesiredFinalHeights[Index] = BaseLocalHeights[Index];
	}

	const FTransform LandscapeToWorld = Landscape->LandscapeActorToWorld();
	int32 InfluenceHitCount = 0;
	for (int32 VertexY = MinY; VertexY <= MaxY; ++VertexY)
	{
		for (int32 VertexX = MinX; VertexX <= MaxX; ++VertexX)
		{
			const int32 Index = (VertexY - MinY) * Width + (VertexX - MinX);
			const FVector WorldXY = LandscapeToWorld.TransformPosition(
				FVector(static_cast<float>(VertexX), static_cast<float>(VertexY), 0.0f));
			const FQuick_RoadRoadCorridorQuery Query = MeshField.Query(
				FVector2D(WorldXY.X, WorldXY.Y), InfluenceFalloffCm);
			if (Query.Weight <= KINDA_SMALL_NUMBER)
			{
				continue;
			}
			const float TargetLocalZ = LandscapeToWorld.InverseTransformPosition(
				FVector(WorldXY.X, WorldXY.Y, Query.TargetWorldZ + VerticalOffsetCm)).Z;
			DesiredFinalHeights[Index] = FMath::Lerp(
				BaseLocalHeights[Index], TargetLocalZ, Query.Weight * BlendStrength);
			if (Query.Weight < 1.0f)
			{
				SmoothWeights[Index] = SmoothStrength
					* (4.0f * Query.Weight * (1.0f - Query.Weight));
			}
			++InfluenceHitCount;
		}
	}
	if (InfluenceHitCount == 0)
	{
		return ErrorJson(Operation, TEXT("road geometry does not overlap the Landscape"));
	}

	for (int32 Iteration = 0; Iteration < SmoothIterations; ++Iteration)
	{
		TArray<float> NextHeights = DesiredFinalHeights;
		for (int32 LocalY = 0; LocalY < Height; ++LocalY)
		{
			for (int32 LocalX = 0; LocalX < Width; ++LocalX)
			{
				const int32 Index = LocalY * Width + LocalX;
				const float Weight = SmoothWeights[Index];
				if (Weight <= KINDA_SMALL_NUMBER)
				{
					continue;
				}
				double Sum = 0.0;
				int32 Count = 0;
				for (int32 OffsetY = -1; OffsetY <= 1; ++OffsetY)
				{
					for (int32 OffsetX = -1; OffsetX <= 1; ++OffsetX)
					{
						if ((OffsetX == 0 && OffsetY == 0)
							|| LocalX + OffsetX < 0 || LocalX + OffsetX >= Width
							|| LocalY + OffsetY < 0 || LocalY + OffsetY >= Height)
						{
							continue;
						}
						Sum += DesiredFinalHeights[(LocalY + OffsetY) * Width + (LocalX + OffsetX)];
						++Count;
					}
				}
				if (Count > 0)
				{
					NextHeights[Index] = FMath::Lerp(
						DesiredFinalHeights[Index], static_cast<float>(Sum / Count), Weight);
				}
			}
		}
		DesiredFinalHeights = MoveTemp(NextHeights);
	}

	TArray<uint16> PackedDelta;
	PackedDelta.SetNumUninitialized(BasePacked.Num());
	int32 SaturatedSampleCount = 0;
	int32 NonNeutralSampleCount = 0;
	for (int32 Index = 0; Index < PackedDelta.Num(); ++Index)
	{
		const float DeltaLocal = DesiredFinalHeights[Index] - BaseLocalHeights[Index];
		if (DeltaLocal <= -256.0f || DeltaLocal >= 255.9921875f)
		{
			++SaturatedSampleCount;
		}
		PackedDelta[Index] = LandscapeDataAccess::GetTexHeight(DeltaLocal);
		if (PackedDelta[Index] != static_cast<uint16>(LandscapeDataAccess::MidValue))
		{
			++NonNeutralSampleCount;
		}
	}
	if (SaturatedSampleCount > 0)
	{
		return ErrorJson(
			Operation,
			FString::Printf(TEXT("refusing clipped additive deltas: %d saturated samples"), SaturatedSampleCount));
	}
	{
		FLandscapeEditDataInterface DeltaLandscapeEdit(LandscapeInfo, RoadLayer->GetGuid(), true);
		DeltaLandscapeEdit.SetHeightData(
			MinX, MinY, MaxX, MaxY, PackedDelta.GetData(), Width, false);
		DeltaLandscapeEdit.Flush();
	}
	Landscape->SetEditingLayer(RoadLayer->GetGuid());
	if (!ForceHeightMerge(Landscape))
	{
		return ErrorJson(Operation, TEXT("Landscape remained pending after writing road delta heights"));
	}
	Landscape->MarkPackageDirty();
	Landscape->PostEditChange();
	DeltaRollback.Commit();

	TSharedRef<FJsonObject> Result = MakeResult(true, Operation);
	Result->SetNumberField(TEXT("road_mesh_count"), RoadMeshes.Num());
	Result->SetNumberField(TEXT("influence_hit_count"), InfluenceHitCount);
	Result->SetNumberField(TEXT("non_neutral_sample_count"), NonNeutralSampleCount);
	Result->SetNumberField(TEXT("saturated_sample_count"), SaturatedSampleCount);
	Result->SetStringField(TEXT("edit_layer_delta_sha256"), HeightDigest(MinX, MinY, MaxX, MaxY, PackedDelta));
	Result->SetStringField(TEXT("error"), TEXT(""));
	return ToJson(Result);
}

FString UQuick_RoadAutomationLibrary::ExtractRoadEdges(
	const float MinPolylineLengthCm,
	const float VertexWeldToleranceCm,
	const bool bIncludeIntersectionPatches,
	const bool bEnableSplineResample,
	const float SplineResampleDistanceCm)
{
	using namespace QuickRoadAutomation;
	constexpr TCHAR Operation[] = TEXT("ExtractRoadEdges");
	UWorld* World = nullptr;
	FString Error;
	if (!ValidateB1MutationContext(World, Error))
	{
		return ErrorJson(Operation, Error);
	}
	if (!FMath::IsFinite(MinPolylineLengthCm) || MinPolylineLengthCm < 10.0f
		|| !FMath::IsFinite(VertexWeldToleranceCm) || VertexWeldToleranceCm < 0.1f
		|| !FMath::IsFinite(SplineResampleDistanceCm) || SplineResampleDistanceCm < 1.0f)
	{
		return ErrorJson(Operation, TEXT("road-edge extraction parameters are outside the supported finite ranges"));
	}
	AActor* Network = FindUniqueOwnedNetwork(World, Error);
	if (!Network)
	{
		return ErrorJson(Operation, Error);
	}
	TArray<FRoadBoundaryPolyline> BoundaryPolylines;
	if (!ExtractAllRoadBoundaryPolylines(
		Network,
		MinPolylineLengthCm,
		VertexWeldToleranceCm,
		bIncludeIntersectionPatches,
		bEnableSplineResample,
		SplineResampleDistanceCm,
		BoundaryPolylines,
		Error))
	{
		return ErrorJson(Operation, Error);
	}
	TArray<AActor*> OldOwnedEdges = FindOwnedEdges(World);
	if (OldOwnedEdges.Num() > 1)
	{
		return ErrorJson(Operation, TEXT("refusing atomic edge replacement because multiple owned edge actors exist"));
	}
	for (AActor* MatchingLabelActor : FindActorsByExactLabel(World, TEXT("QS_B1_QR_Edge_000")))
	{
		if (!OldOwnedEdges.Contains(MatchingLabelActor))
		{
			return ErrorJson(Operation, TEXT("stable road-edge label is occupied by an unowned actor"));
		}
	}
	const TSet<AActor*> Before = SnapshotRoadEdgeActors(World);
	FScopedActorSelectionRestore SelectionRestore;
	FText PluginError;
	int32 PluginSplineCount = 0;
	if (!FQuick_RoadLayoutRoadGenerator::ExtractRoadEdgeSplines(
		World,
		Network,
		MinPolylineLengthCm,
		VertexWeldToleranceCm,
		bIncludeIntersectionPatches,
		ESplinePointType::Linear,
		bEnableSplineResample,
		SplineResampleDistanceCm,
		PluginError,
		&PluginSplineCount))
	{
		for (AActor* PartialActor : DiffRoadEdgeActors(World, Before))
		{
			World->DestroyActor(PartialActor);
		}
		return ErrorJson(Operation, PluginError.ToString());
	}
	TArray<AActor*> NewEdges = DiffRoadEdgeActors(World, Before);
	if (NewEdges.Num() != 1)
	{
		for (AActor* Actor : NewEdges)
		{
			World->DestroyActor(Actor);
		}
		return ErrorJson(Operation, TEXT("QuickRoad edge extraction did not create exactly one replacement edge actor"));
	}
	AActor* EdgeActor = NewEdges[0];
	int32 AuthoredSplineCount = 0;
	if (!AddBoundarySplineComponents(EdgeActor, BoundaryPolylines, AuthoredSplineCount))
	{
		World->DestroyActor(EdgeActor);
		return ErrorJson(Operation, TEXT("could not author all disconnected road boundaries on the edge actor"));
	}
	int32 CandidateSplineCount = 0;
	const FString CandidateDigest = CanonicalSplineDigestForActor(EdgeActor, CandidateSplineCount);
	if (CandidateSplineCount != BoundaryPolylines.Num() || CandidateDigest.Len() != 64)
	{
		World->DestroyActor(EdgeActor);
		return ErrorJson(Operation, TEXT("replacement road-edge geometry failed pre-commit validation"));
	}
	if (OldOwnedEdges.Num() == 1)
	{
		OldOwnedEdges[0]->Modify();
		if (!World->DestroyActor(OldOwnedEdges[0]))
		{
			World->DestroyActor(EdgeActor);
			return ErrorJson(Operation, TEXT("could not atomically replace the previous owned edge actor"));
		}
	}
	EdgeActor->Modify();
	EdgeActor->SetActorLabel(TEXT("QS_B1_QR_Edge_000"));
	EdgeActor->Tags.AddUnique(OwnerTag);
	EdgeActor->Tags.AddUnique(EdgeCategoryTag);
	EdgeActor->Tags.AddUnique(Quick_RoadLayoutSplineTags::RoadEdge);
	EdgeActor->MarkPackageDirty();

	int32 CanonicalSplineCount = 0;
	const FString Digest = CanonicalSplineDigest(World, CanonicalSplineCount);
	ensureMsgf(
		CanonicalSplineCount == BoundaryPolylines.Num()
			&& CanonicalSplineCount == AuthoredSplineCount
			&& Digest == CandidateDigest,
		TEXT("Committed QuickRoad edge actor did not preserve its validated canonical digest"));
	TSharedRef<FJsonObject> Result = MakeResult(true, Operation);
	Result->SetStringField(TEXT("edge_actor_label"), EdgeActor->GetActorLabel());
	Result->SetNumberField(TEXT("edge_spline_count"), CandidateSplineCount);
	Result->SetNumberField(TEXT("plugin_seed_spline_count"), PluginSplineCount);
	Result->SetStringField(TEXT("edge_geometry_sha256"), CandidateDigest);
	Result->SetStringField(TEXT("error"), TEXT(""));
	return ToJson(Result);
}

FString UQuick_RoadAutomationLibrary::BakeSingleRoadNetwork(
	const FString& SaveDirectory,
	const float StraightSegmentSplitDistanceCm,
	const bool bDeleteSourceProcMesh)
{
	using namespace QuickRoadAutomation;
	constexpr TCHAR Operation[] = TEXT("BakeSingleRoadNetwork");
	UWorld* World = nullptr;
	FString Error;
	if (!ValidateB1MutationContext(World, Error))
	{
		return ErrorJson(Operation, Error);
	}
	if (!FPackageName::IsValidLongPackageName(SaveDirectory)
		|| !SaveDirectory.StartsWith(TEXT("/Game/GameXXK/Environment/TownPCG/B1/"), ESearchCase::CaseSensitive)
		|| !FMath::IsFinite(StraightSegmentSplitDistanceCm) || StraightSegmentSplitDistanceCm < 0.0f)
	{
		return ErrorJson(Operation, TEXT("bake directory/settings are invalid or outside the B1 asset root"));
	}
	AActor* Network = FindUniqueOwnedNetwork(World, Error);
	if (!Network)
	{
		return ErrorJson(Operation, Error);
	}

	const TSet<AActor*> BeforeActors = SnapshotActors(World);
	FScopedActorSelectionRestore SelectionRestore;
	GEditor->SelectNone(false, true, false);
	GEditor->SelectActor(Network, true, true, true);
	FText PluginError;
	int32 BakedMeshCount = 0;
	if (!FQuick_RoadLayoutRoadGenerator::BakeRoadMeshesToStaticMeshes(
		World,
		SaveDirectory,
		StraightSegmentSplitDistanceCm,
		/*bDeleteSourceProcMesh=*/false,
		PluginError,
		&BakedMeshCount))
	{
		return ErrorJson(Operation, PluginError.ToString());
	}
	TArray<AActor*> NewActors = DiffActors(World, BeforeActors);
	if (NewActors.Num() != 1 || BakedMeshCount <= 0)
	{
		for (AActor* NewActor : NewActors)
		{
			if (IsValid(NewActor))
			{
				World->DestroyActor(NewActor);
			}
		}
		return ErrorJson(
			Operation,
			FString::Printf(
				TEXT("QuickRoad bake produced %d meshes/%d actors; expected positive meshes and one Blueprint actor"),
				BakedMeshCount,
				NewActors.Num()));
	}
	AActor* BakeActor = NewActors[0];
	TInlineComponentArray<UStaticMeshComponent*> BakedMeshComponents;
	BakeActor->GetComponents(BakedMeshComponents);
	BakedMeshComponents.RemoveAll([](const UStaticMeshComponent* Component)
	{
		return !Component || !Component->GetStaticMesh();
	});
	if (BakeActor->GetLevel() != World->GetCurrentLevel() || BakedMeshComponents.Num() == 0)
	{
		World->DestroyActor(BakeActor);
		return ErrorJson(Operation, TEXT("QuickRoad bake actor is not a valid current-level static-mesh assembly"));
	}
	BakeActor->Modify();
	BakeActor->SetActorLabel(TEXT("QS_B1_QR_Bake"));
	BakeActor->Tags.AddUnique(OwnerTag);
	BakeActor->Tags.AddUnique(FName(TEXT("QingshanB1QuickRoadBake")));
	BakeActor->MarkPackageDirty();

	FARFilter Filter;
	Filter.PackagePaths.Add(FName(*SaveDirectory));
	Filter.bRecursivePaths = true;
	TArray<FAssetData> AssetData;
	FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"))
		.Get().GetAssets(Filter, AssetData);
	TArray<FString> AssetPaths;
	for (const FAssetData& Asset : AssetData)
	{
		AssetPaths.Add(Asset.GetSoftObjectPath().ToString());
	}
	AssetPaths.Sort();
	const FString ExpectedBlueprintPackage = SaveDirectory / (TEXT("BP_") + Network->GetActorLabel());
	const FString ExpectedBlueprintObjectPath = ObjectPathForPackage(ExpectedBlueprintPackage);
	if (AssetPaths.Num() < BakedMeshCount + 1 || !AssetPaths.Contains(ExpectedBlueprintObjectPath))
	{
		World->DestroyActor(BakeActor);
		return ErrorJson(Operation, TEXT("QuickRoad bake assets did not include the expected mesh set and Blueprint assembly"));
	}

	bool bSourceDeleted = false;
	if (bDeleteSourceProcMesh)
	{
		Network->Modify();
		bSourceDeleted = World->DestroyActor(Network);
		if (!bSourceDeleted)
		{
			World->DestroyActor(BakeActor);
			return ErrorJson(Operation, TEXT("validated bake could not atomically delete the source road network"));
		}
	}
	TArray<TSharedPtr<FJsonValue>> JsonAssets;
	for (const FString& AssetPath : AssetPaths)
	{
		JsonAssets.Add(MakeShared<FJsonValueString>(AssetPath));
	}

	TSharedRef<FJsonObject> Result = MakeResult(true, Operation);
	Result->SetStringField(TEXT("bake_actor_label"), BakeActor->GetActorLabel());
	Result->SetNumberField(TEXT("baked_mesh_count"), BakedMeshCount);
	Result->SetArrayField(TEXT("asset_paths"), JsonAssets);
	Result->SetBoolField(TEXT("source_deleted"), bSourceDeleted);
	Result->SetStringField(TEXT("error"), TEXT(""));
	return ToJson(Result);
}

FString UQuick_RoadAutomationLibrary::AuditInfrastructure(
	const FString& LandscapeLabel,
	const FName EditLayerName,
	const FName RoadTagExpression)
{
	using namespace QuickRoadAutomation;
	constexpr TCHAR Operation[] = TEXT("AuditInfrastructure");
	UWorld* World = nullptr;
	FString Error;
	if (!ValidateB1MutationContext(World, Error))
	{
		return ErrorJson(Operation, Error);
	}
	TArray<FName> SourceTags;
	Quick_RoadLayoutRoadTags::ParseRoadTagExpression(RoadTagExpression, SourceTags);
	if (SourceTags.Num() != 3)
	{
		return ErrorJson(Operation, TEXT("audit requires exactly three source road tags"));
	}
	ALandscape* Landscape = FindExactLandscape(World, LandscapeLabel, Error);
	if (!Landscape)
	{
		return ErrorJson(Operation, Error);
	}
	const ULandscapeEditLayerBase* RoadLayer = nullptr;
	if (!ValidateRoadLayer(Landscape, EditLayerName, RoadLayer, Error))
	{
		return ErrorJson(Operation, Error);
	}
	if (!Landscape->IsUpToDate())
	{
		return ErrorJson(Operation, TEXT("Landscape is not up to date for audit"));
	}
	AActor* Network = FindUniqueOwnedNetwork(World, Error);
	if (!Network)
	{
		return ErrorJson(Operation, Error);
	}

	int32 AllNetworkCount = 0;
	int32 UnownedNetworkCount = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->ActorHasTag(Quick_RoadLayoutRoadTags::MainRoadNetwork))
		{
			++AllNetworkCount;
			if (!IsOwnedNetwork(*It))
			{
				++UnownedNetworkCount;
			}
		}
	}
	int32 RoadTriangleCount = 0;
	int32 IntersectionPatchCount = 0;
	CountRoadGeometry(Network, RoadTriangleCount, IntersectionPatchCount);

	TSet<FString> NetworkMaterialPaths;
	TInlineComponentArray<UQuick_RoadLayoutRoadMeshComponent*> NetworkMeshes;
	Network->GetComponents(NetworkMeshes);
	for (UQuick_RoadLayoutRoadMeshComponent* Mesh : NetworkMeshes)
	{
		if (Mesh && Mesh->GetNumSections() > 0)
		{
			NetworkMaterialPaths.Add(Mesh->GetMaterial(0) ? Mesh->GetMaterial(0)->GetPathName() : FString());
		}
	}
	TArray<FString> SortedMaterialPaths = NetworkMaterialPaths.Array();
	SortedMaterialPaths.Sort();
	TArray<TSharedPtr<FJsonValue>> JsonMaterialPaths;
	for (const FString& Path : SortedMaterialPaths)
	{
		JsonMaterialPaths.Add(MakeShared<FJsonValueString>(Path));
	}

	TArray<TSharedPtr<FJsonValue>> JsonSourceRecords;
	for (const FName SourceTag : SourceTags)
	{
		TArray<USplineComponent*> Splines;
		FQuick_RoadLayoutRoadGenerator::CollectMainRoadSplines(World, Splines, SourceTag);
		TSet<int32> QuantizedWidths;
		for (USplineComponent* Spline : Splines)
		{
			if (Spline && UQuick_RoadRoadSplineWidthComponent::HasStoredHalfWidthCm(Spline))
			{
				QuantizedWidths.Add(FMath::RoundToInt(
					UQuick_RoadRoadSplineWidthComponent::ResolveHalfWidthCm(Spline, 0.0f) * 2.0f));
			}
		}
		TArray<int32> Widths = QuantizedWidths.Array();
		Widths.Sort();
		TArray<TSharedPtr<FJsonValue>> JsonWidths;
		for (const int32 Width : Widths)
		{
			JsonWidths.Add(MakeShared<FJsonValueNumber>(Width));
		}
		TSharedRef<FJsonObject> SourceRecord = MakeShared<FJsonObject>();
		SourceRecord->SetStringField(TEXT("road_tag"), SourceTag.ToString());
		SourceRecord->SetNumberField(TEXT("spline_count"), Splines.Num());
		SourceRecord->SetArrayField(TEXT("road_widths_cm"), JsonWidths);
		SourceRecord->SetArrayField(TEXT("network_material_paths"), JsonMaterialPaths);
		JsonSourceRecords.Add(MakeShared<FJsonValueObject>(SourceRecord));
	}

	int32 MinX = 0;
	int32 MinY = 0;
	int32 MaxX = 0;
	int32 MaxY = 0;
	TArray<uint16> RawDelta;
	if (!ReadRawEditLayerHeights(
		Landscape, RoadLayer, MinX, MinY, MaxX, MaxY, RawDelta))
	{
		return ErrorJson(Operation, TEXT("could not read raw road edit-layer delta heights"));
	}
	int32 NonNeutralSampleCount = 0;
	for (const uint16 Value : RawDelta)
	{
		if (Value != static_cast<uint16>(LandscapeDataAccess::MidValue))
		{
			++NonNeutralSampleCount;
		}
	}

	TArray<FString> EdgeLabels;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->ActorHasTag(OwnerTag) && It->ActorHasTag(EdgeCategoryTag))
		{
			EdgeLabels.Add(It->GetActorLabel());
		}
	}
	EdgeLabels.Sort();
	TArray<TSharedPtr<FJsonValue>> JsonEdgeLabels;
	for (const FString& Label : EdgeLabels)
	{
		JsonEdgeLabels.Add(MakeShared<FJsonValueString>(Label));
	}
	int32 EdgeSplineCount = 0;
	const FString EdgeDigest = CanonicalSplineDigest(World, EdgeSplineCount);

	const FVector GridCenterLocal(
		(static_cast<double>(MinX) + static_cast<double>(MaxX)) * 0.5,
		(static_cast<double>(MinY) + static_cast<double>(MaxY)) * 0.5,
		0.0);
	const FVector ReconstructedCenter = Landscape->GetActorTransform().TransformPosition(GridCenterLocal);
	TArray<TSharedPtr<FJsonValue>> JsonCenter = {
		MakeShared<FJsonValueNumber>(ReconstructedCenter.X),
		MakeShared<FJsonValueNumber>(ReconstructedCenter.Y),
		MakeShared<FJsonValueNumber>(ReconstructedCenter.Z),
	};

	TSharedRef<FJsonObject> Result = MakeResult(true, Operation);
	Result->SetNumberField(TEXT("network_count"), AllNetworkCount);
	Result->SetNumberField(TEXT("owned_network_count"), 1);
	Result->SetNumberField(TEXT("unowned_network_count"), UnownedNetworkCount);
	Result->SetStringField(TEXT("network_label"), Network->GetActorLabel());
	Result->SetNumberField(TEXT("road_triangle_count"), RoadTriangleCount);
	Result->SetNumberField(TEXT("intersection_patch_count"), IntersectionPatchCount);
	Result->SetArrayField(TEXT("network_material_paths"), JsonMaterialPaths);
	Result->SetNumberField(TEXT("source_width_group_count"), SourceTags.Num());
	Result->SetArrayField(TEXT("source_records"), JsonSourceRecords);
	Result->SetStringField(TEXT("edit_layer"), EditLayerName.ToString());
	Result->SetBoolField(TEXT("edit_layer_active"), Landscape->GetEditingLayer() == RoadLayer->GetGuid());
	Result->SetBoolField(TEXT("edit_layer_visible"), RoadLayer->IsVisible());
	Result->SetBoolField(TEXT("edit_layer_locked"), RoadLayer->IsLocked());
	Result->SetNumberField(TEXT("non_neutral_sample_count"), NonNeutralSampleCount);
	Result->SetStringField(TEXT("edit_layer_delta_sha256"), HeightDigest(MinX, MinY, MaxX, MaxY, RawDelta));
	Result->SetArrayField(TEXT("reconstructed_grid_center_world"), JsonCenter);
	Result->SetArrayField(TEXT("edge_actor_labels"), JsonEdgeLabels);
	Result->SetNumberField(TEXT("edge_spline_count"), EdgeSplineCount);
	Result->SetStringField(TEXT("edge_geometry_sha256"), EdgeDigest);
	Result->SetStringField(TEXT("error"), TEXT(""));
	return ToJson(Result);
}
