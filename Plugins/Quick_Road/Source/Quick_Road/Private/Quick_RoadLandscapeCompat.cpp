// Copyright Aoife. All Rights Reserved.

#include "Quick_RoadLandscapeCompat.h"

#include "Landscape.h"

#if (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6)
#include "LandscapeEditLayer.h"
#endif

namespace QuickRoadLandscapeCompat
{
	bool IsLayerEditable(const ALandscape* Landscape, const FGuid& LayerGuid)
	{
		if (!Landscape || !LayerGuid.IsValid())
		{
			return false;
		}

#if (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6)
		const ULandscapeEditLayerBase* EditLayer = Landscape->GetEditLayerConst(LayerGuid);
		return EditLayer && !EditLayer->IsLocked();
#else
		const FLandscapeLayer* Layer = Landscape->GetLayer(LayerGuid);
		return Layer && !Layer->bLocked;
#endif
	}

	FGuid ResolveEditingLayerGuid(ALandscape* Landscape)
	{
		if (!Landscape)
		{
			return FGuid();
		}

		const FGuid ActiveLayer = Landscape->GetEditingLayer();
		if (IsLayerEditable(Landscape, ActiveLayer))
		{
			return ActiveLayer;
		}

		static const FName PreferredLayerName(TEXT("Layer"));

#if (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6)
		if (const ULandscapeEditLayerBase* PreferredLayer = Landscape->GetEditLayerConst(PreferredLayerName))
		{
			if (!PreferredLayer->IsLocked())
			{
				return PreferredLayer->GetGuid();
			}
		}

		FGuid FirstUnlockedLayer;
		Landscape->ForEachEditLayerConst([&FirstUnlockedLayer](const ULandscapeEditLayerBase* EditLayer)
		{
			if (!FirstUnlockedLayer.IsValid() && EditLayer && !EditLayer->IsLocked())
			{
				FirstUnlockedLayer = EditLayer->GetGuid();
			}
			return true;
		});
		if (FirstUnlockedLayer.IsValid())
		{
			return FirstUnlockedLayer;
		}

		if (const ULandscapeEditLayerBase* DefaultLayer = Landscape->GetEditLayerConst(0))
		{
			return DefaultLayer->GetGuid();
		}
#elif (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5)
		if (const FLandscapeLayer* PreferredLayer = Landscape->GetLayer(PreferredLayerName))
		{
			if (!PreferredLayer->bLocked)
			{
				return PreferredLayer->Guid;
			}
		}

		FGuid FirstUnlockedLayer;
		Landscape->ForEachLayerConst([&FirstUnlockedLayer](const FLandscapeLayer& Layer)
		{
			if (!FirstUnlockedLayer.IsValid() && !Layer.bLocked)
			{
				FirstUnlockedLayer = Layer.Guid;
			}
			return true;
		});
		if (FirstUnlockedLayer.IsValid())
		{
			return FirstUnlockedLayer;
		}

		if (const FLandscapeLayer* DefaultLayer = Landscape->GetLayer(0))
		{
			return DefaultLayer->Guid;
		}
#else
		if (const FLandscapeLayer* PreferredLayer = Landscape->GetLayer(PreferredLayerName))
		{
			if (!PreferredLayer->bLocked)
			{
				return PreferredLayer->Guid;
			}
		}

		FGuid FirstUnlockedLayer;
		const uint8 LayerCount = Landscape->GetLayerCount();
		for (uint8 LayerIndex = 0; LayerIndex < LayerCount; ++LayerIndex)
		{
			if (const FLandscapeLayer* Layer = Landscape->GetLayer(LayerIndex))
			{
				if (!FirstUnlockedLayer.IsValid() && !Layer->bLocked)
				{
					FirstUnlockedLayer = Layer->Guid;
				}
			}
		}
		if (FirstUnlockedLayer.IsValid())
		{
			return FirstUnlockedLayer;
		}

		if (const FLandscapeLayer* DefaultLayer = Landscape->GetLayer(0))
		{
			return DefaultLayer->Guid;
		}
#endif

		return FGuid();
	}

	bool IsLayerLocked(const ALandscape* Landscape, const FGuid& LayerGuid)
	{
		if (!Landscape || !LayerGuid.IsValid())
		{
			return true;
		}

#if (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6)
		if (const ULandscapeEditLayerBase* TargetLayer = Landscape->GetEditLayerConst(LayerGuid))
		{
			return TargetLayer->IsLocked();
		}
#else
		if (const FLandscapeLayer* TargetLayer = Landscape->GetLayer(LayerGuid))
		{
			return TargetLayer->bLocked;
		}
#endif

		return true;
	}

	FString GetLayerDisplayName(const ALandscape* Landscape, const FGuid& LayerGuid)
	{
		if (!Landscape || !LayerGuid.IsValid())
		{
			return FString();
		}

#if (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6)
		if (const ULandscapeEditLayerBase* TargetLayer = Landscape->GetEditLayerConst(LayerGuid))
		{
			return TargetLayer->GetName().ToString();
		}
#else
		if (const FLandscapeLayer* TargetLayer = Landscape->GetLayer(LayerGuid))
		{
			return TargetLayer->Name.ToString();
		}
#endif

		return FString();
	}
}
