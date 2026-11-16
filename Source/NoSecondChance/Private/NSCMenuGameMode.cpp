#include "NSCMenuGameMode.h"
#include "NSCWidgets.h"
#include "Blueprint/UserWidget.h"

ANSCMenuGameMode::ANSCMenuGameMode()
{
	DefaultPawnClass = nullptr;
	PlayerControllerClass = ANSCMenuPlayerController::StaticClass();
}

void ANSCMenuPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalController()) { return; }
	Menu = CreateWidget<UNSCMainMenuWidget>(this, UNSCMainMenuWidget::StaticClass());
	if (Menu)
	{
		Menu->AddToViewport();
		FInputModeUIOnly Mode;
		Mode.SetWidgetToFocus(Menu->TakeWidget());
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(Mode);
	}
	bShowMouseCursor = true;
}
