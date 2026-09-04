#include "GameXXKAllCardRuntimeTestUtils.h"
#include "GameXXKCardBattleAdapter.h"
#include "GameXXKCardCatalog.h"
#include "GameXXKCardOutcomePreview.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKCardOutcomePreviewCatalogTest
{
	struct FCardObservation
	{
		bool bSawManual = false;
		bool bSawPureEnemyGroup = false;
		FString FirstManualContext;
		FString FirstPureEnemyGroupContext;
	};

	const TCHAR* ClassificationName(const EGameXXKCardOutcomePreviewClass Classification)
	{
		switch (Classification)
		{
		case EGameXXKCardOutcomePreviewClass::None: return TEXT("None");
		case EGameXXKCardOutcomePreviewClass::ManualUnit: return TEXT("ManualUnit");
		case EGameXXKCardOutcomePreviewClass::PureEnemyGroup: return TEXT("PureEnemyGroup");
		default: return TEXT("Unknown");
		}
	}

	FString MakeContext(
		const FGameXXKCardDefinition& Definition,
		const EGameXXKCardTerrain Terrain,
		const FName TargetUnitId = NAME_None)
	{
		return FString::Printf(
			TEXT("CardId=%s Terrain=%d TargetUnitId=%s"),
			*Definition.Id.ToString(),
			static_cast<int32>(Terrain),
			TargetUnitId.IsNone() ? TEXT("None") : *TargetUnitId.ToString());
	}

	bool ObserveClassification(
		FAutomationTestBase& Test,
		const FGameXXKCardOutcomePreview& Preview,
		const FString& Context,
		FCardObservation& OutObservation)
	{
		switch (Preview.Classification)
		{
		case EGameXXKCardOutcomePreviewClass::ManualUnit:
			OutObservation.bSawManual = true;
			if (OutObservation.FirstManualContext.IsEmpty())
			{
				OutObservation.FirstManualContext = Context;
			}
			return true;
		case EGameXXKCardOutcomePreviewClass::PureEnemyGroup:
			OutObservation.bSawPureEnemyGroup = true;
			if (OutObservation.FirstPureEnemyGroupContext.IsEmpty())
			{
				OutObservation.FirstPureEnemyGroupContext = Context;
			}
			return true;
		case EGameXXKCardOutcomePreviewClass::None:
			return true;
		default:
			Test.AddError(FString::Printf(TEXT("%s returned an unknown outcome-preview classification"), *Context));
			return false;
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCardOutcomePreviewCatalogCoverageTest,
	"GameXXK.Data.CardOutcomePreview.Catalog.All173ClassifiedAndPlayable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardOutcomePreviewCatalogCoverageTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKAllCardPlayabilityAuditTest;
	using namespace GameXXKCardOutcomePreviewCatalogTest;

	const TArray<FGameXXKCardDefinition>& Definitions = FGameXXKCardCatalog::GetAllCardDefinitions();
	TestEqual(TEXT("outcome-preview catalog sees exactly 173 active definitions"), Definitions.Num(), 173);
	TestEqual(TEXT("outcome-preview catalog sees exactly seven terrains"), EveryTerrain().Num(), 7);

	TSet<FName> UniqueCardIds;
	for (const FGameXXKCardDefinition& Definition : Definitions)
	{
		if (Definition.Id.IsNone())
		{
			AddError(TEXT("outcome-preview catalog contains an empty CardId"));
		}
		else if (UniqueCardIds.Contains(Definition.Id))
		{
			AddError(FString::Printf(TEXT("outcome-preview catalog contains duplicate CardId=%s"), *Definition.Id.ToString()));
		}
		UniqueCardIds.Add(Definition.Id);
	}
	TestEqual(TEXT("all 173 outcome-preview CardIds are unique"), UniqueCardIds.Num(), 173);

	TMap<FName, FCardObservation> Observations;
	TSet<FName> AutomaticNonPreviewGroup;
	TArray<FString> ManualTargetFailures;
	TArray<FString> GroupFailures;
	TArray<FString> Conflicts;

	for (int32 DefinitionIndex = 0; DefinitionIndex < Definitions.Num(); ++DefinitionIndex)
	{
		const FGameXXKCardDefinition& Definition = Definitions[DefinitionIndex];
		FCardObservation& Observation = Observations.FindOrAdd(Definition.Id);
		for (int32 TerrainIndex = 0; TerrainIndex < EveryTerrain().Num(); ++TerrainIndex)
		{
			const EGameXXKCardTerrain Terrain = EveryTerrain()[TerrainIndex];
			const FString TerrainContext = MakeContext(Definition, Terrain);
			FGameXXKRuntimeState State;
			FName PlayedInstanceId = NAME_None;
			FString Error;
			if (!BuildRuntimeState(
					*this,
					Definition,
					Terrain,
					79000 + DefinitionIndex * 10 + TerrainIndex,
					State,
					PlayedInstanceId,
					Error))
			{
				AddError(FString::Printf(TEXT("%s fixture build failed: %s"), *TerrainContext, *Error));
			}
			else
			{
				FGameXXKCardPlayPreview Playability;
				Error.Reset();
				if (!FGameXXKCardBattleAdapter::BuildCardPlayPreview(State, PlayedInstanceId, Playability, &Error))
				{
					AddError(FString::Printf(TEXT("%s playability preview failed: %s"), *TerrainContext, *Error));
				}
				else if (!Playability.bCanPlay)
				{
					AddError(FString::Printf(
						TEXT("%s is not playable in the shared all-card fixture: %s"),
						*TerrainContext,
						*Playability.FailureReason));
				}
				else if (Playability.TargetRequest.bRequiresManualSelection)
				{
					int32 LegalCandidateCount = 0;
					for (const FGameXXKCardTargetCandidateView& Candidate : Playability.TargetRequest.CandidateViews)
					{
						if (!Candidate.bCanSelect)
						{
							continue;
						}

						++LegalCandidateCount;
						const FString TargetContext = MakeContext(Definition, Terrain, Candidate.UnitId);
						const FGameXXKRuntimeState TargetState = State;
						FGameXXKCardOutcomePreview OutcomePreview;
						Error.Reset();
						if (!FGameXXKCardOutcomePreviewRules::Build(
								TargetState,
								PlayedInstanceId,
								Candidate.UnitId,
								OutcomePreview,
								&Error)
							|| !OutcomePreview.bSuccess)
						{
							const FString Failure = FString::Printf(
								TEXT("%s manual outcome preview failed: %s"),
								*TargetContext,
								*Error);
							ManualTargetFailures.Add(Failure);
							AddError(Failure);
						}
						else
						{
							ObserveClassification(*this, OutcomePreview, TargetContext, Observation);
							const EGameXXKCardOutcomePreviewClass ExpectedClassification =
								EGameXXKCardOutcomePreviewClass::ManualUnit;
							const EGameXXKCardOutcomePreviewClass ActualClassification = OutcomePreview.Classification;
							if (ActualClassification != ExpectedClassification)
							{
								const FString Failure = FString::Printf(
									TEXT("%s classification mismatch: expected=%s actual=%s"),
									*TargetContext,
									ClassificationName(ExpectedClassification),
									ClassificationName(ActualClassification));
								ManualTargetFailures.Add(Failure);
								AddError(Failure);
							}
						}
					}

					if (LegalCandidateCount == 0)
					{
						const FString Failure = FString::Printf(TEXT("%s manual card has no legal target"), *TerrainContext);
						ManualTargetFailures.Add(Failure);
						AddError(Failure);
					}
				}
				else
				{
					const FGameXXKRuntimeState AutomaticState = State;
					FGameXXKCardOutcomePreview OutcomePreview;
					Error.Reset();
					if (!FGameXXKCardOutcomePreviewRules::Build(
							AutomaticState,
							PlayedInstanceId,
							NAME_None,
							OutcomePreview,
							&Error)
						|| !OutcomePreview.bSuccess)
					{
						AddError(FString::Printf(TEXT("%s automatic outcome preview failed: %s"), *TerrainContext, *Error));
					}
					else
					{
						ObserveClassification(*this, OutcomePreview, TerrainContext, Observation);
						const bool bExpectedPureEnemyGroup =
							Playability.TargetRequest.EffectiveMode == EGameXXKCardTargetMode::AllEnemies
							&& OutcomePreview.bUsesEnemyPositionList;
						const EGameXXKCardOutcomePreviewClass ExpectedClassification = bExpectedPureEnemyGroup
							? EGameXXKCardOutcomePreviewClass::PureEnemyGroup
							: EGameXXKCardOutcomePreviewClass::None;
						if (OutcomePreview.Classification != ExpectedClassification)
						{
							const FString Failure = FString::Printf(
								TEXT("%s classification mismatch: expected=%s actual=%s"),
								*TerrainContext,
								ClassificationName(ExpectedClassification),
								ClassificationName(OutcomePreview.Classification));
							if (bExpectedPureEnemyGroup)
							{
								GroupFailures.Add(Failure);
							}
							AddError(Failure);
						}
						const bool bAutomaticNonAllEnemiesGroup = OutcomePreview.bUsesEnemyPositionList
							&& Playability.TargetRequest.EffectiveMode != EGameXXKCardTargetMode::AllEnemies;
						if (bAutomaticNonAllEnemiesGroup)
						{
							AutomaticNonPreviewGroup.Add(Definition.Id);
							if (OutcomePreview.Classification != EGameXXKCardOutcomePreviewClass::None
								|| !OutcomePreview.FocusedLines.IsEmpty()
								|| !OutcomePreview.EnemyPositionLines.IsEmpty())
							{
								AddError(FString::Printf(
									TEXT("%s AutomaticNonPreviewGroup must classify None with empty FocusedLines/EnemyPositionLines"),
									*TerrainContext));
							}
						}
					}
				}
			}
		}

		if (Observation.bSawManual && Observation.bSawPureEnemyGroup)
		{
			Conflicts.Add(FString::Printf(
				TEXT("CardId=%s classification conflict: Manual at [%s], PureEnemyGroup at [%s]"),
				*Definition.Id.ToString(),
				*Observation.FirstManualContext,
				*Observation.FirstPureEnemyGroupContext));
		}
	}

	TSet<FName> Manual;
	TSet<FName> Group;
	TSet<FName> None;
	for (const FGameXXKCardDefinition& Definition : Definitions)
	{
		const FCardObservation* Observation = Observations.Find(Definition.Id);
		if (!Observation)
		{
			AddError(FString::Printf(TEXT("CardId=%s has no catalog observation"), *Definition.Id.ToString()));
			None.Add(Definition.Id);
		}
		else if (Observation->bSawManual)
		{
			Manual.Add(Definition.Id);
		}
		else if (Observation->bSawPureEnemyGroup)
		{
			Group.Add(Definition.Id);
		}
		else
		{
			None.Add(Definition.Id);
		}
	}

	for (const FString& Conflict : Conflicts)
	{
		AddError(Conflict);
	}

	AddInfo(FString::Printf(
		TEXT("Outcome preview catalog counts: Manual=%d Group=%d None=%d AutomaticNonPreviewGroup=%d"),
		Manual.Num(),
		Group.Num(),
		None.Num(),
		AutomaticNonPreviewGroup.Num()));
	TestEqual(TEXT("all active catalog cards are classified"), Manual.Num() + Group.Num() + None.Num(), 173);
	TestTrue(TEXT("manual category is exercised"), Manual.Num() > 0);
	TestTrue(TEXT("group category is exercised"), Group.Num() > 0);
	TestEqual(TEXT("classification conflicts"), Conflicts.Num(), 0);
	TestEqual(TEXT("manual target attempts that failed preview"), ManualTargetFailures.Num(), 0);
	TestEqual(TEXT("group cards without a successful preview"), GroupFailures.Num(), 0);
	return true;
}

#endif
