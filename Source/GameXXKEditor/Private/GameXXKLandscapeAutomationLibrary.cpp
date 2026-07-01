#include "GameXXKLandscapeAutomationLibrary.h"

#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Landscape.h"
#include "LandscapeComponent.h"
#include "LandscapeInfo.h"
#include "LandscapeProxy.h"

DEFINE_LOG_CATEGORY_STATIC(LogGameXXKEditorAutomation, Log, All);

namespace
{
constexpr TCHAR QingshanLandscapeLabel[] = TEXT("QingshanInn_Landscape_Ground");
constexpr TCHAR QingshanLandscapeReplacementLabel[] = TEXT("QingshanInn_Landscape_Ground_Replacement");
constexpr int32 ComponentCountXY = 4;
constexpr int32 SectionsPerComponent = 1;
constexpr int32 QuadsPerSection = 63;
constexpr float LandscapeScaleXY = 20.0f;
constexpr float LandscapeScaleZ = 40.0f;
constexpr float LandscapeOriginZ = -4.0f;
constexpr float TransformTolerance = 0.1f;

int32 ExpectedComponentCount()
{
	return ComponentCountXY * ComponentCountXY;
}

int32 QuadsPerComponent()
{
	return SectionsPerComponent * QuadsPerSection;
}

FVector ExpectedScale()
{
	return FVector(LandscapeScaleXY, LandscapeScaleXY, LandscapeScaleZ);
}

FVector ExpectedLocation()
{
	const FVector Center(0.0, 0.0, LandscapeOriginZ);
	const FVector Offset = FTransform(FRotator::ZeroRotator, FVector::ZeroVector, ExpectedScale())
		.TransformVector(FVector(-ComponentCountXY * QuadsPerComponent() / 2.0, -ComponentCountXY * QuadsPerComponent() / 2.0, 0.0));
	return Center + Offset;
}

void AppendTdd(FString& Report, const TCHAR* Name, const FString& Message)
{
	const FString Line = FString::Printf(TEXT("[TDD] %s: %s"), Name, *Message);
	UE_LOG(LogGameXXKEditorAutomation, Log, TEXT("%s"), *Line);
	Report += Line;
	Report += TEXT("\n");
}

ALandscape* FindQingshanLandscape(UWorld* World)
{
	for (TActorIterator<ALandscape> It(World); It; ++It)
	{
		ALandscape* Landscape = *It;
		if (Landscape && Landscape->GetActorLabel() == QingshanLandscapeLabel)
		{
			return Landscape;
		}
	}

	return nullptr;
}

TArray<AActor*> FindConflictingLabelActors(UWorld* World)
{
	TArray<AActor*> Conflicts;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor && !Actor->IsA<ALandscape>() && Actor->GetActorLabel() == QingshanLandscapeLabel)
		{
			Conflicts.Add(Actor);
		}
	}

	return Conflicts;
}

bool DestroyActors(UWorld* World, const TArray<AActor*>& Actors)
{
	bool bDestroyedAny = false;
	for (AActor* Actor : Actors)
	{
		if (Actor && World->DestroyActor(Actor))
		{
			bDestroyedAny = true;
		}
	}

	return bDestroyedAny;
}

bool HasExpectedShape(const ALandscape* Landscape)
{
	return Landscape
		&& Landscape->LandscapeComponents.Num() == ExpectedComponentCount()
		&& Landscape->ComponentSizeQuads == QuadsPerComponent()
		&& Landscape->NumSubsections == SectionsPerComponent
		&& Landscape->SubsectionSizeQuads == QuadsPerSection;
}

bool HasExpectedTransform(const ALandscape* Landscape)
{
	return Landscape
		&& Landscape->GetActorLocation().Equals(ExpectedLocation(), TransformTolerance)
		&& Landscape->GetActorRotation().Equals(FRotator::ZeroRotator, TransformTolerance)
		&& Landscape->GetActorScale3D().Equals(ExpectedScale(), TransformTolerance);
}

bool ApplyExpectedTransform(ALandscape* Landscape)
{
	if (!Landscape || HasExpectedTransform(Landscape))
	{
		return false;
	}

	Landscape->Modify();
	Landscape->SetActorLocation(ExpectedLocation(), false);
	Landscape->SetActorRotation(FRotator::ZeroRotator, ETeleportType::None);
	Landscape->SetActorRelativeScale3D(ExpectedScale());
	Landscape->PostEditChange();
	return true;
}

FString JsonBool(bool bValue)
{
	return bValue ? TEXT("true") : TEXT("false");
}

ALandscape* CreateQingshanLandscape(UWorld* World, const TCHAR* Label)
{
	const int32 SizeX = ComponentCountXY * QuadsPerComponent() + 1;
	const int32 SizeY = ComponentCountXY * QuadsPerComponent() + 1;

	ALandscape* Landscape = World->SpawnActor<ALandscape>(ExpectedLocation(), FRotator::ZeroRotator);
	if (!Landscape)
	{
		return nullptr;
	}

	Landscape->SetActorLabel(Label);
	Landscape->SetActorRelativeScale3D(ExpectedScale());
	Landscape->StaticLightingLOD = FMath::DivideAndRoundUp(
		FMath::CeilLogTwo((SizeX * SizeY) / (2048 * 2048) + 1),
		static_cast<uint32>(2));

	TArray<uint16> HeightData;
	HeightData.Init(32768, SizeX * SizeY);

	TMap<FGuid, TArray<uint16>> HeightmapDataPerLayers;
	HeightmapDataPerLayers.Add(FGuid(), MoveTemp(HeightData));

	TMap<FGuid, TArray<FLandscapeImportLayerInfo>> MaterialLayerDataPerLayers;
	MaterialLayerDataPerLayers.Add(FGuid(), TArray<FLandscapeImportLayerInfo>());

	TArray<FLandscapeLayer> EmptyLandscapeLayers;
	Landscape->Import(
		FGuid::NewGuid(),
		0,
		0,
		SizeX - 1,
		SizeY - 1,
		SectionsPerComponent,
		QuadsPerSection,
		HeightmapDataPerLayers,
		nullptr,
		MaterialLayerDataPerLayers,
		ELandscapeImportAlphamapType::Additive,
		MakeArrayView(EmptyLandscapeLayers));

	if (ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo())
	{
		LandscapeInfo->UpdateLayerInfoMap(Landscape);
	}

	Landscape->PostEditChange();
	Landscape->MarkPackageDirty();
	return Landscape;
}
}

FString UGameXXKLandscapeAutomationLibrary::EnsureQingshanTownLandscape()
{
	FString Report;
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		AppendTdd(Report, TEXT("QingshanLandscapeWorld"), TEXT("result=FAIL"));
		return Report;
	}

	bool bChanged = false;
	TArray<AActor*> Conflicts = FindConflictingLabelActors(World);
	ALandscape* Landscape = FindQingshanLandscape(World);
	if (Landscape && HasExpectedShape(Landscape))
	{
		bChanged |= ApplyExpectedTransform(Landscape);
		bChanged |= DestroyActors(World, Conflicts);
	}
	else
	{
		ALandscape* Replacement = CreateQingshanLandscape(World, QingshanLandscapeReplacementLabel);
		if (!Replacement || !HasExpectedShape(Replacement))
		{
			if (Replacement)
			{
				World->DestroyActor(Replacement);
			}
			AppendTdd(Report, TEXT("QingshanLandscapeExists"), TEXT("result=FAIL"));
			AppendTdd(Report, TEXT("QingshanLandscapeComponentCount"), FString::Printf(
				TEXT("actual=%d expected=%d"),
				Replacement ? Replacement->LandscapeComponents.Num() : 0,
				ExpectedComponentCount()));
			return Report;
		}

		if (Landscape)
		{
			bChanged |= World->DestroyActor(Landscape);
		}
		bChanged |= DestroyActors(World, Conflicts);
		Replacement->SetActorLabel(QingshanLandscapeLabel);
		Landscape = Replacement;
		bChanged = true;
	}

	const bool bHasLandscape = Landscape != nullptr;
	const int32 LandscapeComponentCount = Landscape ? Landscape->LandscapeComponents.Num() : 0;
	AppendTdd(Report, TEXT("QingshanLandscapeExists"), bHasLandscape ? TEXT("result=PASS") : TEXT("result=FAIL"));
	AppendTdd(Report, TEXT("QingshanLandscapeComponentCount"), FString::Printf(
		TEXT("actual=%d expected=%d"),
		LandscapeComponentCount,
		ExpectedComponentCount()));
	AppendTdd(Report, TEXT("QingshanLandscapeShape"), HasExpectedShape(Landscape) ? TEXT("result=PASS") : TEXT("result=FAIL"));
	AppendTdd(Report, TEXT("QingshanLandscapeTransform"), HasExpectedTransform(Landscape) ? TEXT("result=PASS") : TEXT("result=FAIL"));

	if (bChanged && Landscape)
	{
		Landscape->MarkPackageDirty();
		World->MarkPackageDirty();
	}

	return Report;
}

FString UGameXXKLandscapeAutomationLibrary::AuditQingshanTownLandscape()
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	ALandscape* Landscape = World ? FindQingshanLandscape(World) : nullptr;
	const bool bExists = Landscape != nullptr;
	const bool bShape = HasExpectedShape(Landscape);
	const bool bTransform = HasExpectedTransform(Landscape);
	const bool bOk = bExists && bShape && bTransform;
	const FVector Location = Landscape ? Landscape->GetActorLocation() : FVector::ZeroVector;
	const FRotator Rotation = Landscape ? Landscape->GetActorRotation() : FRotator::ZeroRotator;
	const FVector Scale = Landscape ? Landscape->GetActorScale3D() : FVector::ZeroVector;

	return FString::Printf(
		TEXT("{")
		TEXT("\"ok\":%s,")
		TEXT("\"exists\":%s,")
		TEXT("\"shape_ok\":%s,")
		TEXT("\"transform_ok\":%s,")
		TEXT("\"component_count\":%d,")
		TEXT("\"expected_component_count\":%d,")
		TEXT("\"component_size_quads\":%d,")
		TEXT("\"expected_component_size_quads\":%d,")
		TEXT("\"num_subsections\":%d,")
		TEXT("\"expected_num_subsections\":%d,")
		TEXT("\"subsection_size_quads\":%d,")
		TEXT("\"expected_subsection_size_quads\":%d,")
		TEXT("\"location\":[%.3f,%.3f,%.3f],")
		TEXT("\"expected_location\":[%.3f,%.3f,%.3f],")
		TEXT("\"rotation\":[%.3f,%.3f,%.3f],")
		TEXT("\"expected_rotation\":[0.000,0.000,0.000],")
		TEXT("\"scale\":[%.3f,%.3f,%.3f],")
		TEXT("\"expected_scale\":[%.3f,%.3f,%.3f]")
		TEXT("}"),
		*JsonBool(bOk),
		*JsonBool(bExists),
		*JsonBool(bShape),
		*JsonBool(bTransform),
		Landscape ? Landscape->LandscapeComponents.Num() : 0,
		ExpectedComponentCount(),
		Landscape ? Landscape->ComponentSizeQuads : 0,
		QuadsPerComponent(),
		Landscape ? Landscape->NumSubsections : 0,
		SectionsPerComponent,
		Landscape ? Landscape->SubsectionSizeQuads : 0,
		QuadsPerSection,
		Location.X,
		Location.Y,
		Location.Z,
		ExpectedLocation().X,
		ExpectedLocation().Y,
		ExpectedLocation().Z,
		Rotation.Pitch,
		Rotation.Yaw,
		Rotation.Roll,
		Scale.X,
		Scale.Y,
		Scale.Z,
		ExpectedScale().X,
		ExpectedScale().Y,
		ExpectedScale().Z);
}
