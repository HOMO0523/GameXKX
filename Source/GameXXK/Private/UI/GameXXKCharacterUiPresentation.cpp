#include "UI/GameXXKCharacterUiPresentation.h"

#include "GameXXKCompanionRules.h"
#include "GameXXKEquipmentRules.h"
#include "MVP/GameXXKMVPSubsystem.h"

FString GameXXKCharacterUiPresentation::GetDisplayName(const UGameXXKMVPSubsystem* Subsystem, const FName CharacterId)
{
	if (CharacterId == FGameXXKEquipmentRules::HeroCharacterId()) return TEXT("主角");
	if (CharacterId == TEXT("Npc.TusiChief")) return TEXT("土司首领");
	if (CharacterId == TEXT("Npc.SongJinBao")) return TEXT("宋金宝");
	if (CharacterId == TEXT("Npc.YueBai")) return TEXT("月白");
	if (CharacterId == TEXT("Npc.ZhouGuangZu")) return TEXT("周光祖");
	if (CharacterId == TEXT("Npc.JinGui")) return TEXT("金贵");
	if (CharacterId == TEXT("Npc.QiongMeiEr")) return TEXT("琼梅儿");
	FGameXXKPermanentCompanion Companion;
	if (Subsystem && Subsystem->TryGetPermanentCompanionView(CharacterId, Companion))
	{
		return FGameXXKCompanionRules::GetCompanionDisplayName(Companion.Role, Companion.NameSeed);
	}
	return CharacterId.ToString().StartsWith(TEXT("Npc.")) ? TEXT("同行角色") : TEXT("伙伴");
}
