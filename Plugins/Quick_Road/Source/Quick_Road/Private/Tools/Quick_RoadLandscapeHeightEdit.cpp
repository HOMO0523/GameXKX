// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/Quick_RoadLandscapeHeightEdit.h"

#include "Quick_RoadLandscapeCompat.h"
#include "Landscape.h"
#include "LandscapeEdit.h"
#include "LandscapeDataAccess.h"
#include "LandscapeProxy.h"
#include "LandscapeComponent.h"

#include "Editor.h"
#include "ScopedTransaction.h"
#include "Selection.h"

#define LOCTEXT_NAMESPACE "Quick_RoadLandscapeHeightEdit"

FGuid FQuick_RoadLandscapeHeightEdit::ResolveEditingLayerGuid(ALandscape* Landscape)
{
	return QuickRoadLandscapeCompat::ResolveEditingLayerGuid(Landscape);
}

namespace Quick_RoadHeightEditLocals
{
	bool ReadHeightsWithEditInterface(
		ALandscape* Landscape,
		ULandscapeInfo* LandscapeInfo,
		FQuick_RoadLandscapeHeightBuffer& OutBuffer)
	{
		const FGuid EditLayerGuid = FQuick_RoadLandscapeHeightEdit::ResolveEditingLayerGuid(Landscape);

#if WITH_EDITOR
		FScopedSetLandscapeEditingLayer EditLayerScope(Landscape, EditLayerGuid);
#endif

		FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo);
		OutBuffer.HeightData.SetNumUninitialized(OutBuffer.Width * OutBuffer.Height);
		LandscapeEdit.GetHeightData(
			OutBuffer.MinX, OutBuffer.MinY, OutBuffer.MaxX, OutBuffer.MaxY, OutBuffer.HeightData.GetData(), 0);

		OutBuffer.FloatHeights.SetNumUninitialized(OutBuffer.HeightData.Num());
		for (int32 Index = 0; Index < OutBuffer.HeightData.Num(); ++Index)
		{
			OutBuffer.FloatHeights[Index] = LandscapeDataAccess::GetLocalHeight(OutBuffer.HeightData[Index]);
		}

		return true;
	}

	bool WriteHeightsWithEditInterface(
		ALandscape* Landscape,
		ULandscapeInfo* LandscapeInfo,
		const FQuick_RoadLandscapeHeightBuffer& Buffer,
		const TArray<uint16>& HeightData)
	{
		const FGuid EditLayerGuid = FQuick_RoadLandscapeHeightEdit::ResolveEditingLayerGuid(Landscape);
		if (EditLayerGuid.IsValid() && Landscape->GetEditingLayer() != EditLayerGuid)
		{
			Landscape->SetEditingLayer(EditLayerGuid);
		}

#if WITH_EDITOR
		FScopedSetLandscapeEditingLayer EditLayerScope(
			Landscape,
			EditLayerGuid,
			[Landscape]()
			{
				if (Landscape)
				{
					Landscape->RequestLayersContentUpdateForceAll(ELandscapeLayerUpdateMode::Update_Heightmap_All);
				}
			});
#endif

		FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo);
		LandscapeEdit.SetHeightData(
			Buffer.MinX, Buffer.MinY, Buffer.MaxX, Buffer.MaxY, HeightData.GetData(), 0, true);
		LandscapeEdit.Flush();
		return true;
	}
}

ALandscape* FQuick_RoadLandscapeHeightEdit::FindSelectedLandscape()
{
	if (!GEditor)
	{
		return nullptr;
	}

	for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
	{
		if (ALandscape* Landscape = Cast<ALandscape>(*It))
		{
			return Landscape;
		}

		if (ALandscapeProxy* Proxy = Cast<ALandscapeProxy>(*It))
		{
			if (ALandscape* Landscape = Proxy->GetLandscapeActor())
			{
				return Landscape;
			}
		}
	}

	for (FSelectionIterator It(GEditor->GetSelectedComponentIterator()); It; ++It)
	{
		if (ULandscapeComponent* Component = Cast<ULandscapeComponent>(*It))
		{
			if (ALandscape* Landscape = Component->GetLandscapeActor())
			{
				return Landscape;
			}
		}
	}

	return nullptr;
}

bool FQuick_RoadLandscapeHeightEdit::ReadSelectedLandscapeHeights(
	ALandscape* Landscape,
	FQuick_RoadLandscapeHeightBuffer& OutBuffer,
	FText& OutErrorMessage)
{
	if (!Landscape)
	{
		OutErrorMessage = LOCTEXT("NoLandscape", "请先在视口中选中一个 Landscape Actor。");
		return false;
	}

	ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();
	if (!LandscapeInfo)
	{
		OutErrorMessage = LOCTEXT("NoLandscapeInfo", "无法获取 Landscape 信息。");
		return false;
	}

	const FGuid EditLayerGuid = ResolveEditingLayerGuid(Landscape);
	if (!EditLayerGuid.IsValid())
	{
		OutErrorMessage = LOCTEXT("NoEditLayer", "未找到可用的 Landscape Edit Layer。");
		return false;
	}

	if (QuickRoadLandscapeCompat::IsLayerLocked(Landscape, EditLayerGuid))
	{
		OutErrorMessage = LOCTEXT(
			"LayerLocked",
			"目标 Edit Layer 已锁定，请在 Landscape 模式中解锁后再贴合。");
		return false;
	}

	if (!LandscapeInfo->GetLandscapeExtent(OutBuffer.MinX, OutBuffer.MinY, OutBuffer.MaxX, OutBuffer.MaxY))
	{
		OutErrorMessage = LOCTEXT("InvalidExtent", "Landscape 高度数据范围无效，请确认地形已正确创建并包含组件。");
		return false;
	}

	OutBuffer.Width = OutBuffer.MaxX - OutBuffer.MinX + 1;
	OutBuffer.Height = OutBuffer.MaxY - OutBuffer.MinY + 1;

	return Quick_RoadHeightEditLocals::ReadHeightsWithEditInterface(Landscape, LandscapeInfo, OutBuffer);
}

bool FQuick_RoadLandscapeHeightEdit::WriteLandscapeHeights(
	ALandscape* Landscape,
	const FQuick_RoadLandscapeHeightBuffer& Buffer,
	const FText& TransactionDescription)
{
	if (!Landscape || Buffer.FloatHeights.Num() != Buffer.Width * Buffer.Height)
	{
		return false;
	}

	ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();
	if (!LandscapeInfo)
	{
		return false;
	}

	const FGuid EditLayerGuid = ResolveEditingLayerGuid(Landscape);
	if (!EditLayerGuid.IsValid())
	{
		return false;
	}

	if (QuickRoadLandscapeCompat::IsLayerLocked(Landscape, EditLayerGuid))
	{
		return false;
	}

	TArray<uint16> HeightData;
	HeightData.SetNumUninitialized(Buffer.FloatHeights.Num());
	for (int32 Index = 0; Index < Buffer.FloatHeights.Num(); ++Index)
	{
		HeightData[Index] = LandscapeDataAccess::GetTexHeight(Buffer.FloatHeights[Index]);
	}

	FScopedTransaction Transaction(TransactionDescription);
	Landscape->Modify();
	LandscapeInfo->Modify();

	if (!Quick_RoadHeightEditLocals::WriteHeightsWithEditInterface(Landscape, LandscapeInfo, Buffer, HeightData))
	{
		return false;
	}

	Landscape->MarkPackageDirty();
	Landscape->PostEditChange();
	return true;
}

#undef LOCTEXT_NAMESPACE
