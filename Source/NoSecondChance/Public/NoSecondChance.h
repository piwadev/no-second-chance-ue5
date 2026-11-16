// NO SECOND CHANCE - module header
#pragma once

#include "CoreMinimal.h"

DECLARE_LOG_CATEGORY_EXTERN(LogNSC, Log, All);

// Custom trace channel "Weapon" (see Config/DefaultEngine.ini). Character capsules ignore it, meshes (physics assets) block it.
#define NSC_TRACE_WEAPON ECC_GameTraceChannel1
