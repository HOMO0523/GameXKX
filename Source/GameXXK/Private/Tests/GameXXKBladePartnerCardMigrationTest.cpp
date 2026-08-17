#include "GameXXKCompanionRules.h"
#include "GameXXKMVPRules.h"
#include "MVP/GameXXKSaveMigration.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKBladePartnerCardMigrationTest
{
	struct FLegacyBladePair
	{
		const TCHAR* LegacyId = nullptr;
		const TCHAR* CanonicalId = nullptr;
	};

	const TArray<FLegacyBladePair>& LegacyPairs()
	{
		static const TArray<FLegacyBladePair> Pairs = {
			{ TEXT("Profession.Blade.YiShangHuanShi"), TEXT("Profession.Blade.JingHongChuQiao") },
			{ TEXT("Profession.Blade.DaoYiShouShu"), TEXT("Profession.Blade.HengYunKaiFeng") },
			{ TEXT("Profession.Blade.XiaoJiaLianJi"), TEXT("Profession.Blade.LianXiGuiQiao") },
			{ TEXT("Profession.Blade.CanYueSanDie"), TEXT("Profession.Blade.BaoDaoShouYe") },
		};
		return Pairs;
	}

	FName MapLegacyId(const FName CardId)
	{
		for (const FLegacyBladePair& Pair : LegacyPairs())
		{
			if (CardId == FName(Pair.LegacyId))
			{
				return FName(Pair.CanonicalId);
			}
		}
		return CardId;
	}

	uint32 NextLegacyRandom(uint32& InOutState)
	{
		if (InOutState == 0)
		{
			InOutState = 0x6D2B79F5U;
		}
		InOutState ^= InOutState << 13;
		InOutState ^= InOutState >> 17;
		InOutState ^= InOutState << 5;
		return InOutState;
	}

	TArray<FName> BuildMappedLegacyV13Pool(const int32 CardSeed)
	{
		TMap<FName, TArray<FName>> CandidatesByArchetype;
		CandidatesByArchetype.Add(TEXT("Archetype.Blade.BloodEdge"), {
			TEXT("Profession.Blade.FengHou"), TEXT("Profession.Blade.JiYuLianZhan"),
			TEXT("Profession.Blade.YinXueDao"), TEXT("Profession.Blade.LangDuan") });
		CandidatesByArchetype.Add(TEXT("Archetype.Blade.MomentumBreak"), {
			TEXT("Profession.Blade.DuanYue"), TEXT("Profession.Blade.PoJun"),
			TEXT("Profession.Blade.ZhanYiFeiTeng"), TEXT("Profession.Blade.ZhanJin") });
		CandidatesByArchetype.Add(TEXT("Archetype.Blade.Counterflow"), {
			TEXT("Profession.Blade.JieShiHuiFeng"), TEXT("Profession.Blade.ZhuYing"),
			TEXT("Profession.Blade.PoLangTuJin"), TEXT("Profession.Blade.YiShiDuanJiang") });
		CandidatesByArchetype.Add(TEXT("Archetype.Blade.Sheathed"), {
			TEXT("Profession.Blade.YiShangHuanShi"), TEXT("Profession.Blade.CanYueSanDie"),
			TEXT("Profession.Blade.XiaoJiaLianJi"), TEXT("Profession.Blade.DaoYiShouShu") });

		TArray<FName> ArchetypeIds;
		CandidatesByArchetype.GetKeys(ArchetypeIds);
		ArchetypeIds.Sort([](const FName Left, const FName Right)
		{
			return Left.ToString() < Right.ToString();
		});
		for (TPair<FName, TArray<FName>>& Pair : CandidatesByArchetype)
		{
			Pair.Value.Sort([](const FName Left, const FName Right)
			{
				return Left.ToString() < Right.ToString();
			});
		}

		uint32 RandomState = static_cast<uint32>(CardSeed);
		const auto PickAndRemove = [&RandomState](TArray<FName>& Candidates)
		{
			const int32 Index = static_cast<int32>(NextLegacyRandom(RandomState) % static_cast<uint32>(Candidates.Num()));
			const FName Selected = Candidates[Index];
			Candidates.RemoveAt(Index);
			return Selected;
		};

		TArray<FName> Result = {
			TEXT("Profession.Blade.HuiFengJiaShi"),
			TEXT("Profession.Blade.LieFengZhan") };
		const FName PrimaryArchetype = ArchetypeIds[static_cast<int32>(
			NextLegacyRandom(RandomState) % static_cast<uint32>(ArchetypeIds.Num()))];
		TArray<FName> PrimaryCandidates = CandidatesByArchetype.FindChecked(PrimaryArchetype);
		for (int32 Index = 0; Index < 3; ++Index)
		{
			Result.Add(PickAndRemove(PrimaryCandidates));
		}
		const FName FreeArchetype = ArchetypeIds[static_cast<int32>(
			NextLegacyRandom(RandomState) % static_cast<uint32>(ArchetypeIds.Num()))];
		TArray<FName> FreeCandidates = CandidatesByArchetype.FindChecked(FreeArchetype);
		FreeCandidates.RemoveAll([&Result](const FName CardId)
		{
			return Result.Contains(CardId);
		});
		Result.Add(PickAndRemove(FreeCandidates));
		for (FName& CardId : Result)
		{
			CardId = MapLegacyId(CardId);
		}
		return Result;
	}

	void ReplaceCardId(TArray<FName>& CardIds, const FName From, const FName To)
	{
		for (FName& CardId : CardIds)
		{
			if (CardId == From)
			{
				CardId = To;
			}
		}
	}

	bool BuildProfileContaining(
		const FName TemplateId,
		const FName RequiredCardId,
		const int32 SeedBase,
		FGameXXKPermanentCompanion& OutProfile,
		FString& OutError)
	{
		for (int32 SeedOffset = 0; SeedOffset < 4096; ++SeedOffset)
		{
			FGameXXKCompanionRosterState TemporaryRoster;
			FGameXXKCompanionRecruitResult RecruitResult;
			if (!FGameXXKCompanionRules::RecruitPermanentCompanion(
				TemporaryRoster,
				TemplateId,
				SeedBase + SeedOffset,
				RecruitResult,
				&OutError))
			{
				return false;
			}
			if (!RecruitResult.Companion.PersonalCardIds.Contains(RequiredCardId))
			{
				continue;
			}

			TArray<FName> Selection = { RequiredCardId };
			for (const FName CardId : RecruitResult.Companion.PersonalCardIds)
			{
				if (Selection.Num() >= 5)
				{
					break;
				}
				if (CardId != RequiredCardId)
				{
					Selection.Add(CardId);
				}
			}
			OutProfile = RecruitResult.Companion;
			if (!FGameXXKCompanionRules::SetSelectedPersonalCards(OutProfile, Selection, &OutError))
			{
				return false;
			}
			return true;
		}

		OutError = FString::Printf(
			TEXT("No deterministic seed generated %s for %s."),
			*RequiredCardId.ToString(),
			*TemplateId.ToString());
		return false;
	}

	bool ProfileContainsOnlyCanonicalIds(const FGameXXKPermanentCompanion& Profile)
	{
		for (const FLegacyBladePair& Pair : LegacyPairs())
		{
			const FName LegacyId(Pair.LegacyId);
			if (Profile.PersonalCardIds.Contains(LegacyId)
				|| Profile.UnlockedPersonalCardIds.Contains(LegacyId)
				|| Profile.SelectedCardIds.Contains(LegacyId))
			{
				return false;
			}
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBladePartnerCardMigrationTest,
	"GameXXK.MVP.SaveGame.BladePartnerCardsV14",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBladePartnerCardMigrationTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKBladePartnerCardMigrationTest;

	TestEqual(
		TEXT("the replacement Blade partner cards introduce save version fourteen"),
		FGameXXKSaveMigration::BladePartnerCardsIntroducedSaveVersion,
		14);
	TestEqual(TEXT("the current save version advances to eighteen"), FGameXXKSaveMigration::CurrentSaveVersion, 18);
	FString Error;
	for (int32 Seed = 1; Seed <= 256; ++Seed)
	{
		TArray<FName> CurrentPool;
		if (!FGameXXKCompanionRules::BuildPersonalCardPool(EGameXXKCharacterRole::Blade, Seed, CurrentPool, &Error))
		{
			AddError(Error);
			return false;
		}
		const TArray<FName> MappedLegacyPool = BuildMappedLegacyV13Pool(Seed);
		if (CurrentPool != MappedLegacyPool)
		{
			AddError(FString::Printf(
				TEXT("Blade birth seed %d changed when the four retired IDs were mapped to their replacements."),
				Seed));
			return false;
		}
	}

	TArray<FGameXXKPermanentCompanion> Profiles;
	for (int32 PairIndex = 0; PairIndex < LegacyPairs().Num(); ++PairIndex)
	{
		FGameXXKPermanentCompanion Profile;
		const FName TemplateId(*FString::Printf(TEXT("Companion.Blade.%02d"), PairIndex + 1));
		const FName CanonicalId(LegacyPairs()[PairIndex].CanonicalId);
		if (!TestTrue(
			FString::Printf(TEXT("a deterministic profile contains replacement card %s"), *CanonicalId.ToString()),
			BuildProfileContaining(TemplateId, CanonicalId, 14000 + PairIndex * 5000, Profile, Error)))
		{
			AddError(Error);
			return false;
		}

		ReplaceCardId(Profile.PersonalCardIds, CanonicalId, FName(LegacyPairs()[PairIndex].LegacyId));
		ReplaceCardId(Profile.UnlockedPersonalCardIds, CanonicalId, FName(LegacyPairs()[PairIndex].LegacyId));
		ReplaceCardId(Profile.SelectedCardIds, CanonicalId, FName(LegacyPairs()[PairIndex].LegacyId));
		Profiles.Add(MoveTemp(Profile));
	}

	FGameXXKRuntimeState LegacyRuntime = UGameXXKMVPRules::CreateNewGame();
	FGameXXKCompanionRosterState& Roster = LegacyRuntime.CardRun.CompanionRoster;
	Roster.PermanentCompanions = { Profiles[0], Profiles[1], Profiles[2] };
	for (FGameXXKPermanentCompanion& Companion : Roster.PermanentCompanions)
	{
		Companion.bIsActive = false;
	}
	Roster.PermanentCompanions[0].bIsActive = true;
	LegacyRuntime.CardRun.PartySelection.ActivePermanentCompanionInstanceId =
		Roster.PermanentCompanions[0].InstanceId;
	Roster.PendingRecruitment.bHasPendingRecruitment = true;
	Roster.PendingRecruitment.Candidate = Profiles[3];
	Roster.PendingRecruitment.Candidate.bIsActive = false;

	FGameXXKSaveState LegacySave = UGameXXKMVPRules::MakeSaveState(LegacyRuntime);
	LegacySave.SaveVersion = 13;
	FGameXXKSaveState Migrated;
	FGameXXKSaveMigrationReport Report;
	if (!TestTrue(
		TEXT("a valid v13 roster with retired Blade IDs migrates to the replacement catalog"),
		FGameXXKSaveMigration::MigrateToCurrent(LegacySave, Migrated, Report)))
	{
		AddError(Report.Error);
		return false;
	}

	TestEqual(TEXT("the migrated save writes version eighteen"), Migrated.SaveVersion, 18);
	const FGameXXKCompanionRosterState& MigratedRoster = Migrated.RuntimeState.CardRun.CompanionRoster;
	TestEqual(TEXT("the three permanent Blade profiles survive migration"), MigratedRoster.PermanentCompanions.Num(), 3);
	for (int32 PairIndex = 0; PairIndex < MigratedRoster.PermanentCompanions.Num(); ++PairIndex)
	{
		const FGameXXKPermanentCompanion& Profile = MigratedRoster.PermanentCompanions[PairIndex];
		const FName CanonicalId(LegacyPairs()[PairIndex].CanonicalId);
		TestTrue(FString::Printf(TEXT("permanent profile %d maps its replacement card"), PairIndex),
			Profile.PersonalCardIds.Contains(CanonicalId)
			&& Profile.UnlockedPersonalCardIds.Contains(CanonicalId)
			&& Profile.SelectedCardIds.Contains(CanonicalId));
		TestTrue(FString::Printf(TEXT("permanent profile %d contains no retired Blade ID"), PairIndex),
			ProfileContainsOnlyCanonicalIds(Profile));
		TestTrue(FString::Printf(TEXT("permanent profile %d remains valid"), PairIndex),
			FGameXXKCompanionRules::ValidatePermanentCompanionProfile(Profile, nullptr));
	}

	const FGameXXKPermanentCompanion& MigratedCandidate = MigratedRoster.PendingRecruitment.Candidate;
	TestTrue(TEXT("the pending replacement remains present"), MigratedRoster.PendingRecruitment.bHasPendingRecruitment);
	TestTrue(TEXT("the pending replacement maps all three card arrays"),
		MigratedCandidate.PersonalCardIds.Contains(TEXT("Profession.Blade.BaoDaoShouYe"))
		&& MigratedCandidate.UnlockedPersonalCardIds.Contains(TEXT("Profession.Blade.BaoDaoShouYe"))
		&& MigratedCandidate.SelectedCardIds.Contains(TEXT("Profession.Blade.BaoDaoShouYe")));
	TestTrue(TEXT("the pending replacement contains no retired Blade ID"), ProfileContainsOnlyCanonicalIds(MigratedCandidate));
	TestTrue(TEXT("the pending replacement remains valid"),
		FGameXXKCompanionRules::ValidatePermanentCompanionProfile(MigratedCandidate, nullptr));

	FGameXXKSaveState RoundTrip;
	FGameXXKSaveMigrationReport RoundTripReport;
	TestTrue(TEXT("the migrated v14 save roundtrips without another rewrite"),
		FGameXXKSaveMigration::MigrateToCurrent(Migrated, RoundTrip, RoundTripReport));
	TestTrue(TEXT("the current-version roundtrip is stable"),
		FGameXXKSaveState::StaticStruct()->CompareScriptStruct(&Migrated, &RoundTrip, PPF_None));
	return true;
}

#endif
