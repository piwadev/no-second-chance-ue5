// Main menu map: no pawn, menu widget, UI-only input.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "NSCMenuGameMode.generated.h"

class UNSCMainMenuWidget;

UCLASS()
class NOSECONDCHANCE_API ANSCMenuPlayerController : public APlayerController
{
	GENERATED_BODY()
public:
	virtual void BeginPlay() override;
protected:
	UPROPERTY() TObjectPtr<UNSCMainMenuWidget> Menu;
};

UCLASS()
class NOSECONDCHANCE_API ANSCMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	ANSCMenuGameMode();
};
