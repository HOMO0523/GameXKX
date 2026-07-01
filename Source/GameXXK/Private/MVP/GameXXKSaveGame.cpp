#include "MVP/GameXXKSaveGame.h"

UGameXXKSaveGame::UGameXXKSaveGame()
{
	SaveState = UGameXXKMVPRules::MakeSaveState(UGameXXKMVPRules::CreateNewGame());
}
