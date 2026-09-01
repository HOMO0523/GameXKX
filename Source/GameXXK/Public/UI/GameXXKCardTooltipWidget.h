#pragma once

#include "Blueprint/UserWidget.h"
#include "GameXXKCardText.h"
#include "GameXXKCardTooltipWidget.generated.h"

class UBorder;
class USizeBox;
class UTextBlock;
class UVerticalBox;
struct FGameXXKCardDefinition;

/**
 * Fixed-width parchment Tooltip shared by Backpack, companion/NPC decks and
 * the route merchant. The compact body is the default; holding Shift swaps in
 * the authoritative complete rule text without mutating card state.
 */
UCLASS()
class GAMEXXK_API UGameXXKCardTooltipWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Reads current physical Shift state; Windows uses GetAsyncKeyState rather than Slate's cached key events. */
	static bool IsPhysicalShiftDown();

	void ConfigureCard(
		const FGameXXKCardDefinition& Definition,
		EGameXXKCardQuality Quality,
		const FGameXXKCardPlayPreview* Preview,
		const FGameXXKCardTooltipContext& Context);
	void ConfigureDirect(
		const FText& InTitle,
		const FString& InCompactBody,
		const FString& InExpandedBody = FString());

	float GetFixedWidthForTest() const;
	FString GetDisplayedTextForTest() const;
	FString GetRenderedTextForTest() const;
	TArray<FString> GetPillTextsForTest() const;
	float GetPillFontSizeForTest(const FString& PillText) const;
	bool IsExpandedForTest() const;
	void SetExpandedForTest(bool bExpanded);
	/** Parent-window tick path; unlike a tooltip-window tick, it always observes key release. */
	void SetExpandedFromOwner(bool bExpanded);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void BuildProgrammaticLayout();
	void RefreshPresentation(bool bForce = false);
	bool ResolveExpandedState() const;
	static FString RemoveLeadingTitleLine(const FString& Title, const FString& Body);

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> RootSizeBox;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> PaperFrame;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> BodyBox;

	FText ConfiguredTitle;
	FString CompactBody;
	FString ExpandedBody;
	bool bExpanded = false;
	bool bUseOwnerExpandedState = false;
	bool bOwnerExpandedState = false;
	bool bExpandedOverrideForTest = false;
	bool bUseExpandedOverrideForTest = false;
};
