#!/usr/bin/env python3
"""One-shot patch: Phase-3 tiered reward UI (subsystem facade, board options, click path)."""

from __future__ import annotations

import sys
from pathlib import Path

SUB_H = Path(r"D:\UE5 demo\GameXXK\Source\GameXXK\Public\MVP\GameXXKMVPSubsystem.h")
SUB_CPP = Path(r"D:\UE5 demo\GameXXK\Source\GameXXK\Private\MVP\GameXXKMVPSubsystem.cpp")
BOARD_H = Path(r"D:\UE5 demo\GameXXK\Source\GameXXK\Public\UI\GameXXKBattleBoardWidget.h")
BOARD_CPP = Path(r"D:\UE5 demo\GameXXK\Source\GameXXK\Private\UI\GameXXKBattleBoardWidget.cpp")

BOARD_NEW_METHOD = '''bool UGameXXKBattleBoardWidget::ChoosePendingBattleRewardOption(int32 OptionIndex, FName ReplacementEntryId)
{
	if (RejectBattleHudFixtureMutation())
	{
		return false;
	}

	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || !HasPendingRouteReward())
	{
		LastCardInteractionError = TEXT("当前没有可选取的战后奖励。");
		return false;
	}
	const TArray<FGameXXKBattleRewardOption>& Options = Subsystem->GetRuntimeState().CardRun.PendingReward.Options;
	if (!Options.IsValidIndex(OptionIndex))
	{
		LastCardInteractionError = TEXT("所选奖励不在当前战后三选一内。");
		RefreshProgrammaticLayout();
		return false;
	}
	const FGameXXKBattleRewardOption Option = Options[OptionIndex];

	if (Option.Kind == EGameXXKBattleRewardKind::BossCard)
	{
		FGameXXKRouteCardAcquisitionPreview Preview;
		FString Error;
		if (!FGameXXKCardBattleAdapter::PreviewPendingRouteReward(
			Subsystem->GetRuntimeState(),
			Option.CardId,
			NAME_None,
			Preview,
			&Error))
		{
			LastCardInteractionError = Error;
			RefreshProgrammaticLayout();
			return false;
		}
		if (Preview.Decision == EGameXXKRouteCardAcquisitionDecision::RequiresReplacement)
		{
			if (RouteRewardCardIdAwaitingReplacement != Option.CardId)
			{
				RouteRewardCardIdAwaitingReplacement = Option.CardId;
				SelectedRouteRewardReplacementEntryId = NAME_None;
				LastCardInteractionError.Reset();
				RefreshProgrammaticLayout();
				return false;
			}
			if (ReplacementEntryId.IsNone()
				|| ReplacementEntryId != SelectedRouteRewardReplacementEntryId
				|| !Preview.EligibleReplacementEntryIds.Contains(ReplacementEntryId))
			{
				LastCardInteractionError = TEXT("路线临时牌已满，请先选择一张可替换的路线牌实例。");
				RefreshProgrammaticLayout();
				return false;
			}
		}
		else if (Preview.Decision != EGameXXKRouteCardAcquisitionDecision::CanCommit)
		{
			LastCardInteractionError = Error.IsEmpty() ? TEXT("当前奖励候选不可提交。") : Error;
			RefreshProgrammaticLayout();
			return false;
		}
		else
		{
			ReplacementEntryId = NAME_None;
			SelectedRouteRewardReplacementEntryId = NAME_None;
			RouteRewardCardIdAwaitingReplacement = NAME_None;
		}
	}

	FString Error;
	if (!Subsystem->ResolvePendingBattleRewardChoiceAndFinish(OptionIndex, ReplacementEntryId, &Error))
	{
		LastCardInteractionError = Error;
		RefreshProgrammaticLayout();
		return false;
	}
	SelectedRouteRewardReplacementEntryId = NAME_None;
	RouteRewardCardIdAwaitingReplacement = NAME_None;
	return ResolveAndRefreshCardBattleAfterMutation();
}

'''

PATCHES = [
    (
        SUB_H,
        (
            "\tbool ResolvePendingRouteRewardChoiceAndFinish(\n"
            "\t\tFName RewardCardId,\n"
            "\t\tFName ReplacementEntryId = NAME_None,\n"
            "\t\tFString* OutError = nullptr);\n"
        ),
        (
            "\tbool ResolvePendingRouteRewardChoiceAndFinish(\n"
            "\t\tFName RewardCardId,\n"
            "\t\tFName ReplacementEntryId = NAME_None,\n"
            "\t\tFString* OutError = nullptr);\n"
            "\n"
            "\t/** Commits one tiered battle reward option through the rules, then the victory gate advances the route. */\n"
            "\tbool ResolvePendingBattleRewardChoiceAndFinish(\n"
            "\t\tint32 OptionIndex,\n"
            "\t\tFName ReplacementEntryId = NAME_None,\n"
            "\t\tFString* OutError = nullptr);\n"
        ),
    ),
    (
        SUB_CPP,
        (
            "\treturn UGameXXKMVPRules::ResolvePendingRouteRewardChoiceAndFinish(\n"
            "\t\tRuntimeState,\n"
            "\t\tRewardCardId,\n"
            "\t\tReplacementEntryId,\n"
            "\t\tOutError);\n"
            "}\n"
        ),
        (
            "\treturn UGameXXKMVPRules::ResolvePendingRouteRewardChoiceAndFinish(\n"
            "\t\tRuntimeState,\n"
            "\t\tRewardCardId,\n"
            "\t\tReplacementEntryId,\n"
            "\t\tOutError);\n"
            "}\n"
            "\n"
            "bool UGameXXKMVPSubsystem::ResolvePendingBattleRewardChoiceAndFinish(\n"
            "\tconst int32 OptionIndex,\n"
            "\tconst FName ReplacementEntryId,\n"
            "\tFString* OutError)\n"
            "{\n"
            "\tBeginRuntimeStateMutation(BattleHudFixtureView);\n"
            "\treturn UGameXXKMVPRules::ResolvePendingBattleRewardChoiceAndFinish(\n"
            "\t\tRuntimeState,\n"
            "\t\tOptionIndex,\n"
            "\t\tReplacementEntryId,\n"
            "\t\tOutError);\n"
            "}\n"
        ),
    ),
    (
        BOARD_H,
        (
            "\t/** Commits an existing saved route reward through the adapter, then lets the existing Rules victory gate advance the route. */\n"
            "\tUFUNCTION(BlueprintCallable, Category = \"GameXXK|Battle|Rewards\")\n"
            "\tbool ChoosePendingRouteReward(FName RewardCardId, FName ReplacementEntryId);\n"
        ),
        (
            "\t/** Commits an existing saved route reward through the adapter, then lets the existing Rules victory gate advance the route. */\n"
            "\tUFUNCTION(BlueprintCallable, Category = \"GameXXK|Battle|Rewards\")\n"
            "\tbool ChoosePendingRouteReward(FName RewardCardId, FName ReplacementEntryId);\n"
            "\n"
            "\t/** Commits one tiered battle reward option (card upgrade, boss card, relic, or attribute bonus). */\n"
            "\tUFUNCTION(BlueprintCallable, Category = \"GameXXK|Battle|Rewards\")\n"
            "\tbool ChoosePendingBattleRewardOption(int32 OptionIndex, FName ReplacementEntryId = NAME_None);\n"
        ),
    ),
    (
        BOARD_H,
        "\tTArray<FName> PendingRewardCardIds;\n",
        (
            "\tTArray<FName> PendingRewardCardIds;\n"
            "\n"
            "\tTArray<FGameXXKBattleRewardOption> PendingRewardOptions;\n"
        ),
    ),
    (
        BOARD_CPP,
        '#include "GameXXKRunDeckRules.h"\n',
        '#include "GameXXKRunDeckRules.h"\n#include "GameXXKRelicCatalog.h"\n',
    ),
    (
        BOARD_CPP,
        (
            "\tSelectedRouteRewardReplacementEntryId = NAME_None;\n"
            "\tRouteRewardCardIdAwaitingReplacement = NAME_None;\n"
            "\treturn ResolveAndRefreshCardBattleAfterMutation();\n"
            "}\n"
            "\n"
            "bool UGameXXKBattleBoardWidget::SkipPendingRouteReward()\n"
        ),
        (
            "\tSelectedRouteRewardReplacementEntryId = NAME_None;\n"
            "\tRouteRewardCardIdAwaitingReplacement = NAME_None;\n"
            "\treturn ResolveAndRefreshCardBattleAfterMutation();\n"
            "}\n"
            "\n"
            + BOARD_NEW_METHOD
            + "\n"
            + "bool UGameXXKBattleBoardWidget::SkipPendingRouteReward()\n"
        ),
    ),
    (
        BOARD_CPP,
        (
            "\tconst UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();\n"
            "\treturn Subsystem\n"
            "\t\t&& Subsystem->GetRuntimeState().Screen == EGameXXKScreen::Battle\n"
            "\t\t&& Subsystem->GetRuntimeState().CardRun.PendingReward.CardIds.Num() > 0;\n"
        ),
        (
            "\tconst UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();\n"
            "\treturn Subsystem\n"
            "\t\t&& Subsystem->GetRuntimeState().Screen == EGameXXKScreen::Battle\n"
            "\t\t&& (Subsystem->GetRuntimeState().CardRun.PendingReward.Options.Num() > 0\n"
            "\t\t\t|| Subsystem->GetRuntimeState().CardRun.PendingReward.CardIds.Num() > 0);\n"
        ),
    ),
    (
        BOARD_CPP,
        (
            "\tconst UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();\n"
            "\treturn Subsystem ? Subsystem->GetRuntimeState().CardRun.PendingReward.CardIds : TArray<FName>();\n"
        ),
        (
            "\tconst UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();\n"
            "\tTArray<FName> Result;\n"
            "\tif (!Subsystem)\n"
            "\t{\n"
            "\t\treturn Result;\n"
            "\t}\n"
            "\t// One slot id per tiered option; non-card options report None so the\n"
            "\t// three-slot shape and replacement bookkeeping stay stable.\n"
            "\tconst TArray<FGameXXKBattleRewardOption>& Options = Subsystem->GetRuntimeState().CardRun.PendingReward.Options;\n"
            "\tResult.Reserve(Options.Num());\n"
            "\tfor (const FGameXXKBattleRewardOption& Option : Options)\n"
            "\t{\n"
            "\t\tResult.Add(Option.CardId);\n"
            "\t}\n"
            "\treturn Result;\n"
        ),
    ),
    (
        BOARD_CPP,
        (
            "\tconst bool bFixtureReadOnly = IsBattleHudFixtureReadOnly();\n"
            "\tPendingRewardCardIds = GetPendingRouteRewardCardIds();\n"
            "\tconst bool bShowRewards = PendingRewardCardIds.Num() > 0;\n"
        ),
        (
            "\tconst bool bFixtureReadOnly = IsBattleHudFixtureReadOnly();\n"
            "\tconst UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();\n"
            "\tPendingRewardOptions = Subsystem ? Subsystem->GetRuntimeState().CardRun.PendingReward.Options : TArray<FGameXXKBattleRewardOption>();\n"
            "\tPendingRewardCardIds = GetPendingRouteRewardCardIds();\n"
            "\tconst bool bShowRewards = PendingRewardOptions.Num() > 0;\n"
        ),
    ),
    (
        BOARD_CPP,
        (
            "\t\tconst FName RewardCardId = PendingRewardCardIds[SlotIndex];\n"
            "\t\tconst FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(RewardCardId);\n"
            "\t\tFGameXXKRouteCardAcquisitionPreview Preview;\n"
            "\t\tFString PreviewError;\n"
            "\t\tconst UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();\n"
            "\t\tconst bool bPreviewValid = Subsystem\n"
            "\t\t\t&& FGameXXKCardBattleAdapter::PreviewPendingRouteReward(\n"
            "\t\t\t\tSubsystem->GetRuntimeState(),\n"
            "\t\t\t\tRewardCardId,\n"
            "\t\t\t\tNAME_None,\n"
            "\t\t\t\tPreview,\n"
            "\t\t\t\t&PreviewError)\n"
            "\t\t\t&& (Preview.Decision == EGameXXKRouteCardAcquisitionDecision::CanCommit\n"
            "\t\t\t\t|| Preview.Decision == EGameXXKRouteCardAcquisitionDecision::RequiresReplacement);\n"
            "\t\tif (RewardButton)\n"
            "\t\t{\n"
            "\t\t\tRewardButton->SetIsEnabled(\n"
            "\t\t\t\tbPreviewValid && !bFixtureReadOnly && !IsBattlePresentationPending());\n"
            "\t\t}\n"
            "\t\tApplyCardPresentation(RewardButton, RewardLabel, RewardPortrait, RewardInfoStrip, Definition);\n"
            "\t\tif (RewardLabel)\n"
            "\t\t{\n"
            "\t\t\tconst FString DisplayName = Definition ? Definition->DisplayName.ToString() : RewardCardId.ToString();\n"
            "\t\t\tconst int32 Energy = Definition ? Definition->EnergyCost : 0;\n"
            "\t\t\tconst int32 Mana = Definition ? Definition->ManaCost : 0;\n"
            "\t\t\tconst FString Quality = Definition\n"
            "\t\t\t\t? FGameXXKCardQualityRules::GetDisplayName(Definition->BaseQuality).ToString()\n"
            "\t\t\t\t: FString();\n"
            "\t\t\tRewardLabel->SetText(FText::FromString(FString::Printf(\n"
            "\t\t\t\tTEXT(\"%s\\n[%s] %d 气 / %d 内\"),\n"
            "\t\t\t\t*DisplayName,\n"
            "\t\t\t\t*Quality,\n"
            "\t\t\t\tEnergy,\n"
            "\t\t\t\tMana)));\n"
            "\t\t}\n"
        ),
        (
            "\t\tconst FGameXXKBattleRewardOption& Option = PendingRewardOptions[SlotIndex];\n"
            "\t\tconst bool bIsCardOption = Option.Kind == EGameXXKBattleRewardKind::DeckCardUpgrade\n"
            "\t\t\t|| Option.Kind == EGameXXKBattleRewardKind::BossCard;\n"
            "\t\tconst FGameXXKCardDefinition* Definition = !Option.CardId.IsNone()\n"
            "\t\t\t? FGameXXKCardCatalog::FindCardDefinition(Option.CardId)\n"
            "\t\t\t: nullptr;\n"
            "\t\tFGameXXKRouteCardAcquisitionPreview Preview;\n"
            "\t\tFString PreviewError;\n"
            "\t\tbool bPreviewValid = !bIsCardOption;\n"
            "\t\tif (bIsCardOption)\n"
            "\t\t{\n"
            "\t\t\tbPreviewValid = Subsystem\n"
            "\t\t\t\t&& FGameXXKCardBattleAdapter::PreviewPendingRouteReward(\n"
            "\t\t\t\t\tSubsystem->GetRuntimeState(),\n"
            "\t\t\t\t\tOption.CardId,\n"
            "\t\t\t\t\tNAME_None,\n"
            "\t\t\t\t\tPreview,\n"
            "\t\t\t\t\t&PreviewError)\n"
            "\t\t\t\t&& (Preview.Decision == EGameXXKRouteCardAcquisitionDecision::CanCommit\n"
            "\t\t\t\t\t|| Preview.Decision == EGameXXKRouteCardAcquisitionDecision::RequiresReplacement);\n"
            "\t\t}\n"
            "\t\tif (RewardButton)\n"
            "\t\t{\n"
            "\t\t\tRewardButton->SetIsEnabled(\n"
            "\t\t\t\tbPreviewValid && !bFixtureReadOnly && !IsBattlePresentationPending());\n"
            "\t\t}\n"
            "\t\tif (bIsCardOption)\n"
            "\t\t{\n"
            "\t\t\tApplyCardPresentation(RewardButton, RewardLabel, RewardPortrait, RewardInfoStrip, Definition);\n"
            "\t\t}\n"
            "\t\telse\n"
            "\t\t{\n"
            "\t\t\tif (RewardPortrait)\n"
            "\t\t\t{\n"
            "\t\t\t\tRewardPortrait->SetVisibility(ESlateVisibility::Collapsed);\n"
            "\t\t\t}\n"
            "\t\t\tif (RewardInfoStrip)\n"
            "\t\t\t{\n"
            "\t\t\t\tRewardInfoStrip->SetVisibility(ESlateVisibility::Collapsed);\n"
            "\t\t\t}\n"
            "\t\t}\n"
            "\t\tif (RewardLabel)\n"
            "\t\t{\n"
            "\t\t\tif (Option.Kind == EGameXXKBattleRewardKind::EnergyCapBonus)\n"
            "\t\t\t{\n"
            "\t\t\t\tRewardLabel->SetText(FText::FromString(TEXT(\"气力上限 +1\\n[属性奖励]\")));\n"
            "\t\t\t}\n"
            "\t\t\telse if (Option.Kind == EGameXXKBattleRewardKind::DrawBonus)\n"
            "\t\t\t{\n"
            "\t\t\t\tRewardLabel->SetText(FText::FromString(TEXT(\"每回合抽牌 +1\\n[属性奖励]\")));\n"
            "\t\t\t}\n"
            "\t\t\telse if (Option.Kind == EGameXXKBattleRewardKind::Relic)\n"
            "\t\t\t{\n"
            "\t\t\t\tFString RelicName = Option.RelicId.ToString();\n"
            "\t\t\t\tfor (const FGameXXKRelicDefinition& RelicDefinition : FGameXXKRelicCatalog::GetAllDefinitions())\n"
            "\t\t\t\t{\n"
            "\t\t\t\t\tif (RelicDefinition.Id == Option.RelicId)\n"
            "\t\t\t\t\t{\n"
            "\t\t\t\t\t\tRelicName = RelicDefinition.DisplayName.ToString();\n"
            "\t\t\t\t\t\tbreak;\n"
            "\t\t\t\t\t}\n"
            "\t\t\t\t}\n"
            "\t\t\t\tRewardLabel->SetText(FText::FromString(FString::Printf(TEXT(\"%s\\n[遗物]\"), *RelicName)));\n"
            "\t\t\t}\n"
            "\t\t\telse\n"
            "\t\t\t{\n"
            "\t\t\t\tconst FString DisplayName = Definition ? Definition->DisplayName.ToString() : Option.CardId.ToString();\n"
            "\t\t\t\tconst int32 Energy = Definition ? Definition->EnergyCost : 0;\n"
            "\t\t\t\tconst int32 Mana = Definition ? Definition->ManaCost : 0;\n"
            "\t\t\t\tEGameXXKCardQuality ShownQuality = Definition ? Definition->BaseQuality : EGameXXKCardQuality::Common;\n"
            "\t\t\t\tif (Option.Kind == EGameXXKBattleRewardKind::DeckCardUpgrade)\n"
            "\t\t\t\t{\n"
            "\t\t\t\t\tShownQuality = FGameXXKCardBattleAdapter::GetNextCardQuality(ShownQuality);\n"
            "\t\t\t\t}\n"
            "\t\t\t\tconst FString Quality = FGameXXKCardQualityRules::GetDisplayName(ShownQuality).ToString();\n"
            "\t\t\t\tRewardLabel->SetText(FText::FromString(FString::Printf(\n"
            "\t\t\t\t\tTEXT(\"%s\\n[%s] %d 气 / %d 内\"),\n"
            "\t\t\t\t\t*DisplayName,\n"
            "\t\t\t\t\t*Quality,\n"
            "\t\t\t\t\tEnergy,\n"
            "\t\t\t\t\tMana)));\n"
            "\t\t\t}\n"
            "\t\t}\n"
        ),
    ),
    (
        BOARD_CPP,
        (
            "\tif (PendingRewardCardIds.IsValidIndex(SlotIndex))\n"
            "\t{\n"
            "\t\tChoosePendingRouteReward(PendingRewardCardIds[SlotIndex], SelectedRouteRewardReplacementEntryId);\n"
            "\t}\n"
        ),
        (
            "\tif (PendingRewardCardIds.IsValidIndex(SlotIndex))\n"
            "\t{\n"
            "\t\tChoosePendingBattleRewardOption(SlotIndex, SelectedRouteRewardReplacementEntryId);\n"
            "\t}\n"
        ),
    ),
]


def main() -> int:
    for index, (path, old, new) in enumerate(PATCHES):
        raw = path.read_bytes()
        crlf = b"\r\n" in raw
        eol = "\r\n" if crlf else "\n"
        old_b = old.replace("\n", eol).encode("utf-8")
        new_b = new.replace("\n", eol).encode("utf-8")
        count = raw.count(old_b)
        if count != 1:
            print(f"ABORT patch {index} on {path.name}: expected 1 occurrence, found {count}")
            return 1
        path.write_bytes(raw.replace(old_b, new_b, 1))
        print(f"patched {index} {path.name} (crlf={crlf})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
