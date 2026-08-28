#include "Narrative/GameXXKBattleProfile.h"

namespace GameXXKBattleProfilePrivate
{
	bool SetError(FString* OutError, const FString& Error)
	{
		if (OutError)
		{
			*OutError = Error;
		}
		return false;
	}

	FGameXXKBattleAnchor MakeAnchor(const TCHAR* AnchorId, const double X, const double Y)
	{
		FGameXXKBattleAnchor Anchor;
		Anchor.AnchorId = AnchorId;
		Anchor.NormalizedPosition = FVector2D(X, Y);
		return Anchor;
	}

	const TArray<FGameXXKBattleProfileDefinition>& Definitions()
	{
		static const TArray<FGameXXKBattleProfileDefinition> Result = []
		{
			FGameXXKBattleProfileDefinition Tutorial;
			Tutorial.BattleProfileId = TEXT("BattleProfile.Tutorial.0-1");
			Tutorial.PartyAnchors = {
				MakeAnchor(TEXT("Party.1P"), 0.66, 0.43),
				MakeAnchor(TEXT("Party.2P"), 0.76, 0.58),
				MakeAnchor(TEXT("Party.3P"), 0.86, 0.72)};
			Tutorial.EnemyAnchors = {
				MakeAnchor(TEXT("Enemy.1P"), 0.34, 0.43),
				MakeAnchor(TEXT("Enemy.2P"), 0.24, 0.58),
				MakeAnchor(TEXT("Enemy.3P"), 0.14, 0.72)};
			Tutorial.CameraAnchors = {
				MakeAnchor(TEXT("Camera.Center"), 0.50, 0.50)};
			Tutorial.VfxAnchors = {
				MakeAnchor(TEXT("Vfx.PartyCenter"), 0.76, 0.56),
				MakeAnchor(TEXT("Vfx.EnemyCenter"), 0.24, 0.56)};
			return TArray<FGameXXKBattleProfileDefinition>{MoveTemp(Tutorial)};
		}();
		return Result;
	}

	bool ValidateAnchors(
		const TArray<FGameXXKBattleAnchor>& Anchors,
		const TCHAR* GroupName,
		TSet<FName>& InOutAnchorIds,
		FString* OutError)
	{
		for (const FGameXXKBattleAnchor& Anchor : Anchors)
		{
			if (Anchor.AnchorId.IsNone() || InOutAnchorIds.Contains(Anchor.AnchorId))
			{
				return SetError(OutError, FString::Printf(
					TEXT("BattleProfile %s anchor IDs must be non-empty and globally unique."),
					GroupName));
			}
			if (Anchor.NormalizedPosition.X < 0.0
				|| Anchor.NormalizedPosition.X > 1.0
				|| Anchor.NormalizedPosition.Y < 0.0
				|| Anchor.NormalizedPosition.Y > 1.0)
			{
				return SetError(OutError, FString::Printf(
					TEXT("BattleProfile %s anchor %s is outside normalized viewport space."),
					GroupName,
					*Anchor.AnchorId.ToString()));
			}
			InOutAnchorIds.Add(Anchor.AnchorId);
		}
		return true;
	}
}

const FGameXXKBattleProfileDefinition* FGameXXKBattleProfileCatalog::Find(const FName BattleProfileId)
{
	return GameXXKBattleProfilePrivate::Definitions().FindByPredicate(
		[BattleProfileId](const FGameXXKBattleProfileDefinition& Definition)
		{
			return Definition.BattleProfileId == BattleProfileId;
		});
}

bool FGameXXKBattleProfileCatalog::Validate(
	const FGameXXKBattleProfileDefinition& Profile,
	FString* OutError)
{
	using namespace GameXXKBattleProfilePrivate;
	if (OutError)
	{
		OutError->Reset();
	}
	if (Profile.BattleProfileId.IsNone())
	{
		return SetError(OutError, TEXT("BattleProfile ID must not be empty."));
	}
	if (Profile.PartyAnchors.Num() != 3 || Profile.EnemyAnchors.Num() != 3)
	{
		return SetError(OutError, TEXT("BattleProfile must provide exactly three party and three enemy anchors."));
	}
	if (Profile.CameraAnchors.IsEmpty() || Profile.VfxAnchors.IsEmpty())
	{
		return SetError(OutError, TEXT("BattleProfile must provide camera and VFX anchors."));
	}

	TSet<FName> AnchorIds;
	return ValidateAnchors(Profile.PartyAnchors, TEXT("party"), AnchorIds, OutError)
		&& ValidateAnchors(Profile.EnemyAnchors, TEXT("enemy"), AnchorIds, OutError)
		&& ValidateAnchors(Profile.CameraAnchors, TEXT("camera"), AnchorIds, OutError)
		&& ValidateAnchors(Profile.VfxAnchors, TEXT("VFX"), AnchorIds, OutError);
}
