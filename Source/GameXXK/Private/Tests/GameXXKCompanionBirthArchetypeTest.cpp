#include "Misc/AutomationTest.h"

#include "GameXXKCardCatalog.h"
#include "GameXXKCompanionRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKCompanionBirthArchetypeTest
{
	const TSet<FName>& FormationSwitchCardIds()
	{
		static const TSet<FName> CardIds = {
			TEXT("Profession.FormationMaster.GuanShi"),
			TEXT("Profession.FormationMaster.DingZhen"),
			TEXT("Profession.FormationMaster.YinShuiHuiYuan"),
			TEXT("Profession.FormationMaster.KunZhen"),
			TEXT("Profession.FormationMaster.LinYingMiZong"),
			TEXT("Profession.FormationMaster.JieShanWeiZhang")};
		return CardIds;
	}

	bool NameLess(const FName Left, const FName Right)
	{
		return Left.ToString() < Right.ToString();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCompanionBirthArchetypeTest,
	"GameXXK.Data.CompanionBirthArchetypes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCompanionBirthArchetypeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKCompanionBirthArchetypeTest;
	const TArray<EGameXXKCharacterRole> Roles = {
		EGameXXKCharacterRole::Blade,
		EGameXXKCharacterRole::Guard,
		EGameXXKCharacterRole::Healer,
		EGameXXKCharacterRole::Hunter,
		EGameXXKCharacterRole::Sorcerer,
		EGameXXKCharacterRole::FormationMaster};

	for (const EGameXXKCharacterRole Role : Roles)
	{
		TMap<FName, TSet<FName>> CardsByArchetype;
		int32 CoreCount = 0;
		int32 SwitchCount = 0;
		int32 CandidateCount = 0;
		bool bMetadataValid = true;
		for (const FGameXXKCardDefinition& Definition : FGameXXKCardCatalog::GetAllCardDefinitions())
		{
			if (Definition.Owner != EGameXXKCardOwner::Profession || Definition.Role != Role)
			{
				continue;
			}
			const bool bSwitch = Role == EGameXXKCharacterRole::FormationMaster
				&& FormationSwitchCardIds().Contains(Definition.Id);
			CoreCount += Definition.bCoreProfessionCard ? 1 : 0;
			SwitchCount += bSwitch ? 1 : 0;
			if (Definition.bCoreProfessionCard || bSwitch)
			{
				bMetadataValid &= Definition.ProfessionArchetypeIds.IsEmpty();
				continue;
			}
			++CandidateCount;
			TSet<FName> SeenCardArchetypes;
			bMetadataValid &= !Definition.ProfessionArchetypeIds.IsEmpty();
			for (const FName ArchetypeId : Definition.ProfessionArchetypeIds)
			{
				bMetadataValid &= !ArchetypeId.IsNone() && !SeenCardArchetypes.Contains(ArchetypeId);
				SeenCardArchetypes.Add(ArchetypeId);
				CardsByArchetype.FindOrAdd(ArchetypeId).Add(Definition.Id);
			}
		}

		TestTrue(FString::Printf(TEXT("role %d has complete unique profession-archetype metadata"), static_cast<int32>(Role)), bMetadataValid);
		TestEqual(FString::Printf(TEXT("role %d retains sixteen normal candidates or twelve formation benefits"), static_cast<int32>(Role)), CandidateCount,
			Role == EGameXXKCharacterRole::FormationMaster ? 12 : 16);
		TestEqual(FString::Printf(TEXT("role %d has the correct core-card count"), static_cast<int32>(Role)),
			CoreCount,
			Role == EGameXXKCharacterRole::FormationMaster ? 0 : 2);
		TestEqual(FString::Printf(TEXT("role %d has the correct terrain-switch catalog count"), static_cast<int32>(Role)),
			SwitchCount,
			Role == EGameXXKCharacterRole::FormationMaster ? 6 : 0);
		TestTrue(FString::Printf(TEXT("role %d declares at least two equally selectable archetypes"), static_cast<int32>(Role)), CardsByArchetype.Num() >= 2);
		for (const TPair<FName, TSet<FName>>& Pair : CardsByArchetype)
		{
			TestTrue(FString::Printf(TEXT("archetype %s has enough distinct candidates"), *Pair.Key.ToString()),
				Pair.Value.Num() >= (Role == EGameXXKCharacterRole::FormationMaster ? 3 : 4));
		}

		TMap<FName, int32> PrimaryCounts;
		TMap<FName, int32> FormationSwitchCounts;
		bool bAllSeedsValid = true;
		for (int32 Seed = 1; Seed <= 1024; ++Seed)
		{
			TArray<FName> Pool;
			FName PrimaryArchetypeId;
			FString Error;
			if (!FGameXXKCompanionRules::BuildPersonalCardPool(Role, Seed, Pool, &Error, &PrimaryArchetypeId)
				|| Pool.Num() != 6
				|| TSet<FName>(Pool).Num() != 6
				|| PrimaryArchetypeId.IsNone()
				|| !CardsByArchetype.Contains(PrimaryArchetypeId))
			{
				bAllSeedsValid = false;
				break;
			}

			int32 PrimaryCardsInPool = 0;
			int32 FormationSwitchesInPool = 0;
			int32 FormationBenefitsInPool = 0;
			for (const FName CardId : Pool)
			{
				const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(CardId);
				PrimaryCardsInPool += Definition && Definition->ProfessionArchetypeIds.Contains(PrimaryArchetypeId) ? 1 : 0;
				if (Role == EGameXXKCharacterRole::FormationMaster)
				{
					const bool bFormationSwitch = FormationSwitchCardIds().Contains(CardId);
					FormationSwitchesInPool += bFormationSwitch ? 1 : 0;
					FormationBenefitsInPool += bFormationSwitch ? 0 : 1;
					if (bFormationSwitch)
					{
						++FormationSwitchCounts.FindOrAdd(CardId);
					}
				}
			}
			if (PrimaryCardsInPool < 3 || PrimaryCardsInPool > 4
				|| (Role == EGameXXKCharacterRole::FormationMaster
					&& (FormationSwitchesInPool != 2 || FormationBenefitsInPool != 4)))
			{
				bAllSeedsValid = false;
				break;
			}
			++PrimaryCounts.FindOrAdd(PrimaryArchetypeId);
		}
		TestTrue(FString::Printf(TEXT("role %d produces deterministic 3+1 archetype birth pools for every audited seed"), static_cast<int32>(Role)), bAllSeedsValid);

		const double ExpectedPerArchetype = CardsByArchetype.IsEmpty()
			? 0.0
			: 1024.0 / static_cast<double>(CardsByArchetype.Num());
		bool bDistributionBalanced = PrimaryCounts.Num() == CardsByArchetype.Num();
		for (const TPair<FName, TSet<FName>>& Pair : CardsByArchetype)
		{
			const int32 Count = PrimaryCounts.FindRef(Pair.Key);
			bDistributionBalanced &= Count >= FMath::FloorToInt(ExpectedPerArchetype * 0.70)
				&& Count <= FMath::CeilToInt(ExpectedPerArchetype * 1.30);
		}
		TestTrue(FString::Printf(TEXT("role %d selects every declared primary archetype at approximately equal probability"), static_cast<int32>(Role)), bDistributionBalanced);

		if (Role == EGameXXKCharacterRole::FormationMaster)
		{
			const double ExpectedPerSwitch = 1024.0 * 2.0 / static_cast<double>(FormationSwitchCardIds().Num());
			bool bSwitchDistributionBalanced = FormationSwitchCounts.Num() == FormationSwitchCardIds().Num();
			for (const FName SwitchCardId : FormationSwitchCardIds())
			{
				const int32 Count = FormationSwitchCounts.FindRef(SwitchCardId);
				bSwitchDistributionBalanced &= Count >= FMath::FloorToInt(ExpectedPerSwitch * 0.70)
					&& Count <= FMath::CeilToInt(ExpectedPerSwitch * 1.30);
			}
			TestTrue(TEXT("formation birth pools select exactly two switches and distribute all six switches approximately equally"), bSwitchDistributionBalanced);
		}

		TArray<FName> FirstPool;
		TArray<FName> SecondPool;
		FName FirstPrimary;
		FName SecondPrimary;
		TestTrue(TEXT("the audit seed builds once"), FGameXXKCompanionRules::BuildPersonalCardPool(Role, 7331, FirstPool, nullptr, &FirstPrimary));
		TestTrue(TEXT("the audit seed rebuilds"), FGameXXKCompanionRules::BuildPersonalCardPool(Role, 7331, SecondPool, nullptr, &SecondPrimary));
		TestEqual(TEXT("same role and seed preserve the exact six-card order"), FirstPool, SecondPool);
		TestEqual(TEXT("same role and seed preserve the primary archetype"), FirstPrimary, SecondPrimary);
	}
	return true;
}

#endif
