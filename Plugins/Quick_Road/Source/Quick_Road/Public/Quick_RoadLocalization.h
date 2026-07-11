// Copyright Aoife. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

DECLARE_MULTICAST_DELEGATE(FQuickRoadLocalizationChanged);

namespace QuickRoadLocalization
{
	/** Register engine culture change listener. Call from module startup. */
	void Initialize();

	/** Unregister culture change listener. Call from module shutdown. */
	void Shutdown();

	/** Fired when editor language / culture changes. */
	FQuickRoadLocalizationChanged& OnLocalizationChanged();

	/** True when the editor UI culture is English (en, en-US, ...). */
	bool UsesEnglishEditorCulture();

	FText GetText(const TCHAR* Key);

	FText GetEditorModeName();
	FText GetCategoryLabel(FName CategoryId);
	FText GetToolLabel(FName CommandName);
	FText GetToolTooltip(FName CommandName);

	FText GetSettingsSectionDescription();

}
