// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/Quick_RoadLandscapeCreator.h"

#include "Landscape.h"
#include "LandscapeImportHelper.h"
#include "LandscapeProxy.h"
#include "LandscapeInfo.h"
#include "Editor.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "Quick_RoadLandscapeCreator"

void FQuick_RoadLandscapeCreator::ComputeGridStats(int32 QuadsPerSection, int32 SectionsPerComponent, FIntPoint ComponentCount, int32& OutResolutionX, int32& OutResolutionY, int32& OutTotalComponents)
{
	const int32 QuadsPerComponent = SectionsPerComponent * QuadsPerSection;
	OutResolutionX = ComponentCount.X * QuadsPerComponent + 1;
	OutResolutionY = ComponentCount.Y * QuadsPerComponent + 1;
	OutTotalComponents = ComponentCount.X * ComponentCount.Y;
}

bool FQuick_RoadLandscapeCreator::IsValidSectionSize(int32 QuadsPerSection)
{
	switch (QuadsPerSection)
	{
	case 7:
	case 15:
	case 31:
	case 63:
	case 127:
	case 255:
		return true;
	default:
		return false;
	}
}

bool FQuick_RoadLandscapeCreator::IsValidSectionsPerComponent(int32 SectionsPerComponent)
{
	return SectionsPerComponent == 1 || SectionsPerComponent == 2;
}

bool FQuick_RoadLandscapeCreator::TryCreateLandscape(UWorld* World, const FQuick_RoadGenerateTerrainSettings& Settings, FText& OutErrorMessage)
{
	if (!World)
	{
		OutErrorMessage = LOCTEXT("NoWorld", "No valid world found.");
		return false;
	}

	if (!World->GetCurrentLevel() || !World->GetCurrentLevel()->bIsVisible)
	{
		OutErrorMessage = LOCTEXT("LevelNotVisible", "Current level is not visible.");
		return false;
	}

	if (!IsValidSectionSize(Settings.QuadsPerSection))
	{
		OutErrorMessage = LOCTEXT("InvalidSectionSize", "Invalid section size.");
		return false;
	}

	if (!IsValidSectionsPerComponent(Settings.SectionsPerComponent))
	{
		OutErrorMessage = LOCTEXT("InvalidSectionsPerComponent", "Invalid sections per component.");
		return false;
	}

	const FIntPoint ComponentCount(
		FMath::Clamp(Settings.ComponentCount.X, 1, 32),
		FMath::Clamp(Settings.ComponentCount.Y, 1, 32));

	const int32 QuadsPerSection = Settings.QuadsPerSection;
	const int32 SectionsPerComponent = Settings.SectionsPerComponent;
	const int32 QuadsPerComponent = SectionsPerComponent * QuadsPerSection;
	const int32 SizeX = ComponentCount.X * QuadsPerComponent + 1;
	const int32 SizeY = ComponentCount.Y * QuadsPerComponent + 1;

	TArray<uint16> HeightData;
	FString HeightmapFilePathForReimport;

	if (!Settings.HeightmapFilePath.IsEmpty())
	{
		FLandscapeImportDescriptor ImportDescriptor;
		FText ImportMessage;
		const ELandscapeImportResult DescriptorResult = FLandscapeImportHelper::GetHeightmapImportDescriptor(
			Settings.HeightmapFilePath,
			true,
			false,
			ImportDescriptor,
			ImportMessage);

		if (DescriptorResult == ELandscapeImportResult::Error)
		{
			OutErrorMessage = ImportMessage;
			return false;
		}

		if (ImportDescriptor.ImportResolutions.Num() == 0)
		{
			OutErrorMessage = LOCTEXT("NoImportResolution", "Could not determine heightmap resolution.");
			return false;
		}

		const int32 DescriptorIndex = 0;
		TArray<uint16> RawImportData;
		const ELandscapeImportResult DataResult = FLandscapeImportHelper::GetHeightmapImportData(
			ImportDescriptor,
			DescriptorIndex,
			RawImportData,
			ImportMessage);

		if (DataResult == ELandscapeImportResult::Error)
		{
			OutErrorMessage = ImportMessage;
			return false;
		}

		const FLandscapeImportResolution ImportResolution(
			ImportDescriptor.ImportResolutions[DescriptorIndex].Width,
			ImportDescriptor.ImportResolutions[DescriptorIndex].Height);
		const FLandscapeImportResolution RequiredResolution(SizeX, SizeY);

		FLandscapeImportHelper::TransformHeightmapImportData(
			RawImportData,
			HeightData,
			ImportResolution,
			RequiredResolution,
			ELandscapeImportTransformType::ExpandCentered);

		HeightmapFilePathForReimport = Settings.HeightmapFilePath;
	}

	TMap<FGuid, TArray<uint16>> HeightDataPerLayers;
	HeightDataPerLayers.Add(FGuid(), MoveTemp(HeightData));

	TArray<FLandscapeImportLayerInfo> MaterialImportLayers;
	TMap<FGuid, TArray<FLandscapeImportLayerInfo>> MaterialLayerDataPerLayers;
	MaterialLayerDataPerLayers.Add(FGuid(), MoveTemp(MaterialImportLayers));

	const FVector DefaultScale(100.0, 100.0, 100.0);
	const FRotator DefaultRotation(0.0, 0.0, 0.0);
	const FVector DefaultLocation(0.0, 0.0, 100.0);

	const FVector Offset = FTransform(DefaultRotation, FVector::ZeroVector, DefaultScale).TransformVector(
		FVector(-ComponentCount.X * QuadsPerComponent / 2.0, -ComponentCount.Y * QuadsPerComponent / 2.0, 0.0));

	FScopedTransaction Transaction(LOCTEXT("CreateLandscapeTransaction", "Quick Road Create Landscape"));

	ALandscape* Landscape = World->SpawnActor<ALandscape>(DefaultLocation + Offset, DefaultRotation);
	if (!Landscape)
	{
		OutErrorMessage = LOCTEXT("SpawnFailed", "Failed to spawn landscape actor.");
		return false;
	}

	Landscape->LandscapeMaterial = Settings.Material;
	Landscape->SetActorRelativeScale3D(DefaultScale);
	Landscape->StaticLightingLOD = FMath::DivideAndRoundUp(FMath::CeilLogTwo((SizeX * SizeY) / (2048 * 2048) + 1), static_cast<uint32>(2));

	Landscape->Import(
		FGuid::NewGuid(),
		0,
		0,
		SizeX - 1,
		SizeY - 1,
		SectionsPerComponent,
		QuadsPerSection,
		HeightDataPerLayers,
		*HeightmapFilePathForReimport,
		MaterialLayerDataPerLayers,
		ELandscapeImportAlphamapType::Additive,
#if (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 5)
		nullptr);
#else
		TArrayView<const FLandscapeLayer>());
#endif

	ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();
	if (!LandscapeInfo)
	{
		OutErrorMessage = LOCTEXT("NoLandscapeInfo", "Failed to create landscape info.");
		return false;
	}

	LandscapeInfo->UpdateLayerInfoMap(Landscape);
	FActorLabelUtilities::SetActorLabelUnique(Landscape, ALandscape::StaticClass()->GetName());

	return true;
}

#undef LOCTEXT_NAMESPACE
