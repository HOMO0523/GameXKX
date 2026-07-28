#include "MVP/GameXXKSaveGame.h"

UGameXXKSaveGame::UGameXXKSaveGame()
{
	// Serialization-neutral by design: only the explicit new-game path creates starter content.
	SaveState = FGameXXKSaveState();
}
