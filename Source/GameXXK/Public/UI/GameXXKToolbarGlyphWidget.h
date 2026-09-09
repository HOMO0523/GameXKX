#pragma once
#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "GameXXKToolbarGlyphWidget.generated.h"

class SGameXXKToolbarGlyph;

/** Small scalable toolbar symbols; no font glyph or locale dependency. */
UCLASS()
class GAMEXXK_API UGameXXKToolbarGlyphWidget : public UWidget
{
	GENERATED_BODY()
public:
	void Configure(int32 InSymbol, bool bInActive);
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
private:
	int32 Symbol = 0;
	bool bActive = false;
	TSharedPtr<SGameXXKToolbarGlyph> Glyph;
};
