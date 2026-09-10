#pragma once

// Only the explicit GameXXKDev target opts into F10 in Shipping.
// Use the same switch for the command surface and player-save write protection.
#if !UE_BUILD_SHIPPING || defined(GAMEXXK_SHIPPING_WITH_DEV_TOOLS)
#define GAMEXXK_WITH_DEV_TOOLS 1
#else
#define GAMEXXK_WITH_DEV_TOOLS 0
#endif
