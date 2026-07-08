#include "GameXXKMVPRules.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "UI/GameXXKBattleBoardWidget.h"
#include "UI/GameXXKMVPHUD.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBattleBoardWidgetTest,
	"GameXXK.MVP.Battle.BoardWidget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBattleBoardWidgetTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	TestTrue(TEXT("new game starts"), Subsystem->StartGame());
	TestTrue(TEXT("Qingshan can be selected"), Subsystem->SelectWorldRegion(UGameXXKMVPRules::RegionQingshan()));
	TestTrue(TEXT("quest can be accepted"), Subsystem->AcceptQuest());
	TestTrue(TEXT("accepted quest enters route map"), Subsystem->OpenDungeonFromTownExit());
	TestTrue(TEXT("start node advances to battle route node"), Subsystem->SelectDungeonNode(EGameXXKNodeKind::Start));
	TestTrue(TEXT("battle node opens battle screen"), Subsystem->SelectDungeonNode(EGameXXKNodeKind::Battle));
	TestEqual(TEXT("battle screen is active"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::Battle);

	AGameXXKMVPHUD* HUD = NewObject<AGameXXKMVPHUD>();
	HUD->SetMVPSubsystemForTest(Subsystem);
	UGameXXKBattleBoardWidget* BattleWidget = HUD->CreateBattleBoardWidgetForTest();
	TestNotNull(TEXT("HUD creates battle board widget"), BattleWidget);
	TestTrue(TEXT("HUD retains battle board widget"), HUD->HasBattleBoardWidget());
	TestEqual(TEXT("battle widget receives same MVP subsystem"), BattleWidget ? BattleWidget->GetMVPSubsystem() : nullptr, Subsystem);

	TestTrue(TEXT("battle board initializes widget tree"), BattleWidget->Initialize());
	BattleWidget->NativeConstruct();
	BattleWidget->RefreshFromState();
	TestTrue(TEXT("battle board remains active as a battle input/status layer"), BattleWidget->IsBattleBoardVisible());
	TestEqual(TEXT("battle board leaves enemies to scene actors instead of UMG cards"), BattleWidget->GetEnemySlotCount(), 0);
	TestEqual(TEXT("battle board leaves party members to scene actors instead of UMG cards"), BattleWidget->GetPartySlotCount(), 0);
	TestTrue(TEXT("battle board status exposes HP"), BattleWidget->GetBattleStatusTextForTest().Contains(TEXT("HP")));
	TestTrue(TEXT("battle board status exposes MP"), BattleWidget->GetBattleStatusTextForTest().Contains(TEXT("MP")));
	TestFalse(TEXT("battle board command menu starts hidden"), BattleWidget->IsCommandMenuVisibleForTest());
	TestFalse(TEXT("battle board starts outside targeting mode"), BattleWidget->IsTargetingBattleActionForTest());
	const FVector2D EmbeddedPieCanvasSize(1540.0f, 720.0f);
	const FVector2D BrokenEmbeddedProjection(660.0f, 420.0f);
	const FVector2D CorrectedCommandSource = BattleWidget->ResolveCommandSourcePositionForTest(1, BrokenEmbeddedProjection, BrokenEmbeddedProjection, EmbeddedPieCanvasSize);
	TestTrue(TEXT("battle board corrects embedded PIE party command source back to the right-side party lane"), CorrectedCommandSource.X >= EmbeddedPieCanvasSize.X * 0.72f);
	TestTrue(TEXT("battle board keeps corrected party command source inside the canvas"), CorrectedCommandSource.X <= EmbeddedPieCanvasSize.X - 12.0f);
	const FVector2D EmbeddedPieWidgetOrigin(96.0f, 90.0f);
	const FVector2D SlateCursorPosition(602.0f, 236.0f);
	const FVector2D LocalCursorPosition = BattleWidget->ResolveSlateAbsolutePositionToLocalForTest(SlateCursorPosition, EmbeddedPieWidgetOrigin, EmbeddedPieCanvasSize);
	TestEqual(TEXT("battle board converts Slate absolute cursor to widget-local targeting coordinates"), LocalCursorPosition, FVector2D(506.0f, 146.0f));
	const FVector2D ScaledWidgetAbsoluteOrigin(100.0f, 80.0f);
	const FVector2D ScaledWidgetAbsoluteSize(1600.0f, 800.0f);
	const FVector2D ScaledWidgetLocalSize(800.0f, 400.0f);
	const FVector2D ScaledSlateCursorPosition(900.0f, 480.0f);
	const FVector2D ScaledLocalCursorPosition = BattleWidget->ResolveSlateAbsolutePositionToLocalForTest(
		ScaledSlateCursorPosition,
		ScaledWidgetAbsoluteOrigin,
		ScaledWidgetAbsoluteSize,
		ScaledWidgetLocalSize);
	TestEqual(TEXT("battle board converts scaled Slate absolute cursor to widget-local targeting coordinates"), ScaledLocalCursorPosition, FVector2D(400.0f, 200.0f));
	const FVector2D EnemySideClickPosition(360.0f, 390.0f);
	const FVector2D PartyUnitScreenPosition(940.0f, 420.0f);
	TestTrue(TEXT("battle board opens command menu for living party unit"), BattleWidget->OpenCommandMenuForPartyUnit(0, EnemySideClickPosition, PartyUnitScreenPosition));
	TestTrue(TEXT("battle board command menu becomes visible"), BattleWidget->IsCommandMenuVisibleForTest());
	TestEqual(TEXT("battle board records selected party index"), BattleWidget->GetSelectedPartyIndexForTest(), 0);
	const FVector2D CommandMenuAnchor = BattleWidget->GetCommandMenuAnchorForTest();
	constexpr float CommandMenuWidth = 260.0f;
	const FVector2D CommandMenuDefaultOffset(-500.0f, 0.0f);
	TestEqual(TEXT("battle board uses the tuned command-menu X offset"), CommandMenuAnchor.X, PartyUnitScreenPosition.X + CommandMenuDefaultOffset.X);
	TestEqual(TEXT("battle board uses the tuned command-menu Y offset"), CommandMenuAnchor.Y, PartyUnitScreenPosition.Y + CommandMenuDefaultOffset.Y);
	TestTrue(TEXT("battle board anchors command menu to the left of the selected party unit"), CommandMenuAnchor.X + CommandMenuWidth < PartyUnitScreenPosition.X);
	TestTrue(TEXT("battle board exposes basic attack action after party selection"), BattleWidget->HasBattleActionForTest(FName(TEXT("BattleBasicAttack")), true));
	TestTrue(TEXT("battle board exposes Crane Wing Slash action after party selection"), BattleWidget->HasBattleActionForTest(FName(TEXT("BattleCraneWingSlash")), true));
	TestTrue(TEXT("battle board exposes defend action after party selection"), BattleWidget->HasBattleActionForTest(FName(TEXT("BattleDefend")), true));
	TestTrue(TEXT("basic attack uses the shared ink button base"), BattleWidget->GetBattleActionButtonResourcePathForTest(FName(TEXT("BattleBasicAttack"))).Contains(TEXT("T_InkButtonBase")));
	TestTrue(TEXT("basic attack ink button is tinted toward attack red"), BattleWidget->GetBattleActionButtonTintForTest(FName(TEXT("BattleBasicAttack"))).R > BattleWidget->GetBattleActionButtonTintForTest(FName(TEXT("BattleBasicAttack"))).B);
	TestTrue(TEXT("defend ink button is tinted toward defense blue"), BattleWidget->GetBattleActionButtonTintForTest(FName(TEXT("BattleDefend"))).B > BattleWidget->GetBattleActionButtonTintForTest(FName(TEXT("BattleDefend"))).R);
	TestTrue(TEXT("battle targeting arrow head asset is loaded"), BattleWidget->GetTargetingArrowHeadResourcePathForTest().Contains(TEXT("T_BattleTargetArrowHead")));
	TestEqual(TEXT("battle targeting uses all generated ink dab pieces"), BattleWidget->GetTargetingInkDabTextureCountForTest(), 12);

	const int32 SkillEnemyHPBefore = Subsystem->GetRuntimeState().ActiveBattleEnemies[0].HP;
	const int32 SkillMPBefore = Subsystem->GetRuntimeState().PlayerMP;
	TestTrue(TEXT("battle board Crane Wing Slash enters targeting"), BattleWidget->ExecuteCraneWingSlashAction());
	TestTrue(TEXT("battle board is targeting after attack skill click"), BattleWidget->IsTargetingBattleActionForTest());
	TestEqual(TEXT("battle board records targeting action"), BattleWidget->GetTargetingActionNameForTest(), FName(TEXT("BattleCraneWingSlash")));
	TestEqual(TEXT("battle targeting arrow starts at the selected party unit instead of the click point"), BattleWidget->GetTargetingPointerPositionForTest(), PartyUnitScreenPosition);
	TestEqual(TEXT("Crane Wing Slash does not damage before target confirmation"), Subsystem->GetRuntimeState().ActiveBattleEnemies[0].HP, SkillEnemyHPBefore);
	TestEqual(TEXT("Crane Wing Slash does not spend MP before target confirmation"), Subsystem->GetRuntimeState().PlayerMP, SkillMPBefore);
	BattleWidget->UpdateTargetingPointer(FVector2D(520.0f, 360.0f));
	TestEqual(TEXT("battle board tracks targeting pointer"), BattleWidget->GetTargetingPointerPositionForTest(), FVector2D(520.0f, 360.0f));
	TestTrue(TEXT("empty click keeps targeting mode alive"), BattleWidget->KeepTargetingAfterEmptyClickForTest());
	TestTrue(TEXT("battle board still targets after empty click"), BattleWidget->IsTargetingBattleActionForTest());
	TestTrue(TEXT("battle board confirms targeted enemy"), BattleWidget->ConfirmTargetingEnemy(0));
	TestTrue(TEXT("battle board Crane Wing Slash damages confirmed enemy"), Subsystem->GetRuntimeState().ActiveBattleEnemies[0].HP < SkillEnemyHPBefore);
	TestTrue(TEXT("battle board Crane Wing Slash spends MP after confirmation"), Subsystem->GetRuntimeState().PlayerMP < SkillMPBefore);
	TestFalse(TEXT("battle board exits targeting after confirmed action"), BattleWidget->IsTargetingBattleActionForTest());
	BattleWidget->RefreshFromState();
	TestTrue(TEXT("battle board reopens command menu for support action"), BattleWidget->OpenCommandMenuForPartyUnit(0, FVector2D(900.0f, 520.0f), FVector2D(830.0f, 420.0f)));
	TestTrue(TEXT("battle board exposes Guiyuan after enemy response damages hero"), BattleWidget->HasBattleActionForTest(FName(TEXT("BattleGuiyuanArt")), true));
	for (FGameXXKBattleRuntimeUnit& Enemy : Subsystem->GetMutableRuntimeState().ActiveBattleEnemies)
	{
		Enemy.Attack = 0;
	}
	const int32 HealHPBefore = Subsystem->GetRuntimeState().PlayerHP;
	TestTrue(TEXT("battle board Guiyuan executes"), BattleWidget->ExecuteGuiyuanArtAction());
	TestTrue(TEXT("battle board Guiyuan leaves hero healthier than before cast"), Subsystem->GetRuntimeState().PlayerHP > HealHPBefore);

	const int32 EnemyHPBeforeAttack = Subsystem->GetRuntimeState().ActiveBattleEnemies[0].HP;
	BattleWidget->RefreshFromState();
	TestTrue(TEXT("battle board opens command menu for cancel flow"), BattleWidget->OpenCommandMenuForPartyUnit(0, FVector2D(900.0f, 520.0f), FVector2D(830.0f, 420.0f)));
	TestTrue(TEXT("battle board basic attack enters targeting"), BattleWidget->ExecuteBasicAttackAction());
	TestTrue(TEXT("battle board cancel from targeting returns to command menu"), BattleWidget->CancelBattleTargeting());
	TestTrue(TEXT("battle board command menu remains visible after targeting cancel"), BattleWidget->IsCommandMenuVisibleForTest());
	TestFalse(TEXT("battle board no longer targets after cancel"), BattleWidget->IsTargetingBattleActionForTest());
	TestTrue(TEXT("battle board cancel from command menu hides menu"), BattleWidget->CancelBattleTargeting());
	TestFalse(TEXT("battle board command menu hides after second cancel"), BattleWidget->IsCommandMenuVisibleForTest());
	TestEqual(TEXT("cancelled targeting does not damage enemy"), Subsystem->GetRuntimeState().ActiveBattleEnemies[0].HP, EnemyHPBeforeAttack);

	for (int32 EnemyIndex = 1; EnemyIndex < Subsystem->GetMutableRuntimeState().ActiveBattleEnemies.Num(); ++EnemyIndex)
	{
		Subsystem->GetMutableRuntimeState().ActiveBattleEnemies[EnemyIndex].HP = 0;
		Subsystem->GetMutableRuntimeState().ActiveBattleEnemies[EnemyIndex].bDefeated = true;
	}
	Subsystem->GetMutableRuntimeState().ActiveBattleEnemies[0].HP = 1;
	Subsystem->GetMutableRuntimeState().ActiveBattleEnemies[0].bDefeated = false;
	TestTrue(TEXT("battle board opens command menu for final blow"), BattleWidget->OpenCommandMenuForPartyUnit(0, FVector2D(900.0f, 520.0f), FVector2D(830.0f, 420.0f)));
	TestTrue(TEXT("battle board final basic attack enters targeting"), BattleWidget->ExecuteBasicAttackAction());
	TestTrue(TEXT("battle board final confirmed target resolves battle victory"), BattleWidget->ConfirmTargetingEnemy(0));
	TestEqual(TEXT("battle victory returns to dungeon route map"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::DungeonMap);
	TestEqual(TEXT("battle victory advances route index"), Subsystem->GetRuntimeState().DungeonNodeIndex, 2);

	BattleWidget->RefreshFromState();
	TestFalse(TEXT("battle board hides outside battle screen"), BattleWidget->IsBattleBoardVisible());
	return true;
}

#endif
