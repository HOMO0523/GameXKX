#include "GameXXKAllCardRuntimeTestUtils.h"
#include "GameXXKCardText.h"
#include "GameXXKCardQualityRules.h"
#include "GameXXKEnemyCatalog.h"
#include "GameXXKEquipmentCatalog.h"
#include "GameXXKEquipmentSetCatalog.h"
#include "GameXXKAffixCatalog.h"
#include "GameXXKGemRules.h"
#include "GameXXKCombatGemRules.h"
#include "GameXXKResistanceRules.h"
#include "MVP/GameXXKSaveMigration.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace DesignTableAudit
{
using Object=TSharedPtr<FJsonObject>;
template<typename T> Object Json(const T& V){auto O=MakeShared<FJsonObject>();FJsonObjectConverter::UStructToJsonObject(T::StaticStruct(),&V,O,0,0);return O;}
template<typename T> TArray<TSharedPtr<FJsonValue>> JsonArray(const TArray<T>& Values)
{TArray<TSharedPtr<FJsonValue>> A;for(const auto& V:Values)A.Add(MakeShared<FJsonValueObject>(Json(V)));return A;}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKDesignTableAuditTest,"GameXXK.Data.DesignTables.AllQualitiesAndRuntimeExport",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKDesignTableAuditTest::RunTest(const FString&)
{
 using namespace GameXXKAllCardPlayabilityAuditTest;using namespace DesignTableAudit;
 auto Root=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Rows,Branches;int32 Executed=0,Variants=0;
 Root->SetNumberField(TEXT("schema_version"),2);Root->SetNumberField(TEXT("save_version"),FGameXXKSaveMigration::CurrentSaveVersion);Root->SetNumberField(TEXT("gem_type_count"),FGameXXKGemRules::MaximumTypeRank);
 for(const auto& D:FGameXXKCardCatalog::GetAllCardDefinitions())for(int32 Q=static_cast<int32>(D.BaseQuality);Q<=static_cast<int32>(EGameXXKCardQuality::Epic);++Q)
 {
  ++Variants;const auto Quality=static_cast<EGameXXKCardQuality>(Q);auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("id"),D.Id.ToString());Row->SetStringField(TEXT("name"),D.DisplayName.ToString());Row->SetStringField(TEXT("quality"),FGameXXKCardQualityRules::GetDisplayName(Quality).ToString());
  const auto Effective=FGameXXKCardQualityRules::BuildEffectiveDefinition(D,Quality);Row->SetObjectField(TEXT("definition"),Json(Effective));
  Row->SetStringField(TEXT("damage_element"),FGameXXKCombatGemRules::GetElementLabel(FGameXXKCombatGemRules::GetCardElement(Effective)));
  Row->SetBoolField(TEXT("inherits_previous_element"),D.SorcererRule.SequenceRule==EGameXXKSorcererSequenceRule::UniversalScalingAttack);
  int32 Passed=0;
  for(const auto Terrain:EveryTerrain())
  {
   FGameXXKCardBattleRuntime R;FName Id;FString Error;
   if(!BuildRuntime(*this,D,Terrain,86000+Variants*10+static_cast<int32>(Terrain),R,Id))continue;
   R.Deck.Hand[0].CurrentQuality=Quality;FGameXXKCardPlayPreview Preview;
   if(!GameXXKCardRules::BuildCardPlayPreview(R,Id,Preview,&Error)||!Preview.bCanPlay){AddError(D.Id.ToString()+TEXT(" preview: ")+Error);continue;}
   FName Target=NAME_None;if(Preview.TargetRequest.bRequiresManualSelection)for(const auto& V:Preview.TargetRequest.CandidateViews)if(V.bCanSelect){Target=V.UnitId;break;}
   FGameXXKCardPlayResult Result;
   if(!GameXXKCardRules::ResolveCardPlay(R,Id,Target,Result,&Error)||!DrainChoicesAndAutomaticQueue(*this,R,D.Id.ToString())||!GameXXKCardRules::ValidateCardBattleRuntime(R,&Error))
   {AddError(D.Id.ToString()+TEXT(" resolution: ")+Error);continue;}
   ++Passed;++Executed;
  }
  Row->SetNumberField(TEXT("passed_terrains"),Passed);
  FGameXXKCardBattleRuntime Display;FName DisplayId;
  if(BuildRuntime(*this,D,EGameXXKCardTerrain::Plain,86001,Display,DisplayId))
  {
   Display.Deck.Hand[0].CurrentQuality=Quality;Display.TeamMaxLevelSnapshot=100;
   for(auto& U:Display.Units){U.Attack=100;U.Defense=100;U.CombatLevel=100;U.Armor=0;U.Statuses.Reset();U.HP=U.MaxHP;U.Mana=U.MaxMana=U.Role==EGameXXKCharacterRole::Sorcerer?34:30;}
   FGameXXKCardPlayPreview Preview;FString Error;const bool HasPreview=GameXXKCardRules::BuildCardPlayPreview(Display,DisplayId,Preview,&Error);
   FGameXXKCardTooltipContext Context;
   Row->SetStringField(TEXT("target"),GameXXKCardText::DescribeTargetHeading(Effective));
   Row->SetStringField(TEXT("compact"),GameXXKCardText::DescribeCompactTooltipBody(D,Quality,HasPreview?&Preview:nullptr,Context));
   Row->SetStringField(TEXT("detail"),GameXXKCardText::DescribeExpandedTooltipBody(D,Quality,HasPreview?&Preview:nullptr,Context));
   Row->SetStringField(TEXT("pills"),GameXXKCardText::DescribePillTooltipBody(D,Quality,Context));
   Row->SetBoolField(TEXT("preview_ok"),HasPreview);
   if(D.SorcererRule.Family==EGameXXKSorcererCardFamily::Universal)
   {
    for(const auto Branch:{EGameXXKSorcererTaskBranch::Normal,EGameXXKSorcererTaskBranch::Fire,EGameXXKSorcererTaskBranch::Ice,EGameXXKSorcererTaskBranch::Lightning})
    {
     FGameXXKCardTooltipContext BranchContext;BranchContext.LockedSpellBranch=Branch;
     auto B=MakeShared<FJsonObject>();B->SetStringField(TEXT("id"),D.Id.ToString());B->SetStringField(TEXT("name"),D.DisplayName.ToString());
     B->SetStringField(TEXT("quality"),FGameXXKCardQualityRules::GetDisplayName(Quality).ToString());
     B->SetStringField(TEXT("branch"),Branch==EGameXXKSorcererTaskBranch::Fire?TEXT("火"):Branch==EGameXXKSorcererTaskBranch::Ice?TEXT("冰"):Branch==EGameXXKSorcererTaskBranch::Lightning?TEXT("雷"):TEXT("普通"));
     B->SetStringField(TEXT("target"),GameXXKCardText::DescribeTargetHeading(Effective));
     B->SetStringField(TEXT("compact"),GameXXKCardText::DescribeCompactTooltipBody(D,Quality,HasPreview?&Preview:nullptr,BranchContext));
     B->SetStringField(TEXT("detail"),GameXXKCardText::DescribeExpandedTooltipBody(D,Quality,HasPreview?&Preview:nullptr,BranchContext));
     B->SetStringField(TEXT("pills"),GameXXKCardText::DescribePillTooltipBody(D,Quality,BranchContext));Branches.Add(MakeShared<FJsonValueObject>(B));
    }
   }
  }
  Rows.Add(MakeShared<FJsonValueObject>(Row));
 }
 Root->SetArrayField(TEXT("cards"),Rows);Root->SetNumberField(TEXT("variants"),Variants);Root->SetNumberField(TEXT("executions"),Executed);
 Root->SetArrayField(TEXT("branches"),Branches);
 TestEqual(TEXT("all 419 table qualities covered"),Variants,419);TestEqual(TEXT("419 qualities across seven terrains"),Executed,419*7);
 Root->SetArrayField(TEXT("equipment"),JsonArray(FGameXXKEquipmentCatalog::GetPackageDefinitions()));Root->SetArrayField(TEXT("affixes"),JsonArray(FGameXXKAffixCatalog::GetAllDefinitions()));Root->SetArrayField(TEXT("sets"),JsonArray(FGameXXKEquipmentSetCatalog::GetDefinitions()));
 TArray<TSharedPtr<FJsonValue>> Gems;
 for(FName Id:FGameXXKGemRules::GetAllItemIds())
 {
  EGameXXKGemType Type;EGameXXKGemQuality Quality;if(!FGameXXKGemRules::TryParseItemId(Id,Type,Quality))continue;auto O=MakeShared<FJsonObject>();
  O->SetStringField(TEXT("id"),Id.ToString());O->SetStringField(TEXT("name"),FGameXXKGemRules::GetDisplayName(Type,Quality).ToString());
  O->SetNumberField(TEXT("value"),FGameXXKGemRules::IsPercentType(Type)?FGameXXKGemRules::GetBonusBasisPoints(Type,Quality)/100.0:FGameXXKGemRules::GetStatBonus(Type,Quality));
  O->SetStringField(TEXT("unit"),FGameXXKGemRules::IsResistanceType(Type)?TEXT("名义抗性百分点"):FGameXXKGemRules::IsPercentType(Type)?TEXT("名义百分比"):TEXT("平坦值"));
  O->SetStringField(TEXT("icon"),FGameXXKGemRules::GetIconTexturePath(Type,Quality).ToString());
  O->SetStringField(TEXT("description"),FGameXXKGemRules::GetDescription(Type,Quality).ToString());
  O->SetNumberField(TEXT("sockets"),FGameXXKGemRules::GetSocketCapacity(static_cast<EGameXXKEquipmentQuality>(Quality)));Gems.Add(MakeShared<FJsonValueObject>(O));
 }
 Root->SetArrayField(TEXT("gems"),Gems);
 TArray<TSharedPtr<FJsonValue>> Profiles;
 for(FName Key:FGameXXKResistanceRules::GetProfileKeys())
 {
  const auto P=FGameXXKResistanceRules::GetBaseProfile(Key,EGameXXKCharacterRole::Invalid);auto O=MakeShared<FJsonObject>();
  O->SetStringField(TEXT("key"),Key.ToString());O->SetNumberField(TEXT("fire"),P.FireBasisPoints/100.0);O->SetNumberField(TEXT("frost"),P.FrostBasisPoints/100.0);O->SetNumberField(TEXT("lightning"),P.LightningBasisPoints/100.0);Profiles.Add(MakeShared<FJsonValueObject>(O));
 }
 Root->SetArrayField(TEXT("resistance_profiles"),Profiles);
 TArray<TSharedPtr<FJsonValue>> Enemies;
 for(const auto& E:FGameXXKEnemyCatalog::GetAllDefinitions()){auto O=Json(E);for(int32 L:{100,125,135})O->SetObjectField(FString::Printf(TEXT("level%d"),L),Json(FGameXXKEnemyCatalog::ComputeStats(E.Id,L)));Enemies.Add(MakeShared<FJsonValueObject>(O));}
 Root->SetArrayField(TEXT("enemies"),Enemies);
 TArray<TSharedPtr<FJsonValue>> Stages;
 for(const auto& Stage:FGameXXKTrainingRules::GetStageDefinitions()){auto O=Json(Stage);O->SetArrayField(TEXT("encounters"),JsonArray(FGameXXKTrainingRules::BuildEncounterSequence(Stage.StageId)));Stages.Add(MakeShared<FJsonValueObject>(O));}
 Root->SetArrayField(TEXT("stages"),Stages);
 const FString Dir=FPaths::ProjectSavedDir()/TEXT("Automation/DesignTableRuntime");IFileManager::Get().MakeDirectory(*Dir,true);
 FString Text;FJsonSerializer::Serialize(Root,TJsonWriterFactory<>::Create(&Text));TestTrue(TEXT("runtime audit exported"),FFileHelper::SaveStringToFile(Text,*(Dir/TEXT("runtime.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
 return true;
}
#endif
