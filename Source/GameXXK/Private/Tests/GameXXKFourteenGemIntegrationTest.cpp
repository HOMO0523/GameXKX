#include "Misc/AutomationTest.h"
#include "GameXXKGemRules.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKFourteenGemCatalogIntegrationTest,
	"GameXXK.Equipment.Gems.SeventeenTypesShareQualityIndependentIcons",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKFourteenGemCatalogIntegrationTest::RunTest(const FString& Parameters)
{
	const auto Ids = FGameXXKGemRules::GetAllItemIds();
	TestEqual(TEXT("seventeen gem types each expose ten quality item variants"), Ids.Num(), 170);
	TSet<FName> UniqueIds;
	TSet<FString> Icons;
	for (const FName Id : Ids)
	{
		UniqueIds.Add(Id);
		EGameXXKGemType Type;
		EGameXXKGemQuality Quality;
		if (!TestTrue(TEXT("every gem ID resolves through the shared catalog"), FGameXXKGemRules::TryParseItemId(Id, Type, Quality))) continue;
		const FString Icon = FGameXXKGemRules::GetIconTexturePath(Type, Quality).ToString();
		TestEqual(TEXT("all qualities of one gem reuse the same approved type icon"), Icon,
			FGameXXKGemRules::GetIconTexturePath(Type, EGameXXKGemQuality::Common).ToString());
		Icons.Add(Icon);
	}
	TestEqual(TEXT("item IDs remain unique"), UniqueIds.Num(), 170);
	TestEqual(TEXT("only seventeen type graphics are used"), Icons.Num(), 17);
	return true;
}

#endif
