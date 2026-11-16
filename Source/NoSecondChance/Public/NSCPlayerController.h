// Human player controller: Enhanced Input context, HUD / buy menu / scoreboard / pause widgets,
// buy RPCs and spectating teammates after death.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "NSCTypes.h"
#include "NSCWeaponData.h"
#include "NSCPlayerController.generated.h"

class UNSCInputSet;
class UNSCHUDWidget;
class UNSCBuyMenuWidget;
class UNSCScoreboardWidget;
class UNSCPauseMenuWidget;
class ANSCCharacter;

UCLASS()
class NOSECONDCHANCE_API ANSCPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ANSCPlayerController();

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	UNSCInputSet* GetInputSet();
	bool IsMenuOpen() const { return bBuyMenuOpen || bPauseOpen; }
	bool IsBuyMenuOpen() const { return bBuyMenuOpen; }
	bool IsScoreboardShown() const { return bScoreboardShown; }
	ANSCCharacter* GetSpectatedCharacter() const;

	void ToggleBuyMenu();
	void CloseMenus();
	void TogglePause();
	void LeaveMatch();
	void SpectateNext();

	void RequestBuyWeapon(FName WeaponId) { ServerBuyWeapon(WeaponId); }
	void RequestBuyEquipment(ENSCEquipment Item) { ServerBuyEquipment(Item); }

protected:
	UFUNCTION(Server, Reliable) void ServerBuyWeapon(FName WeaponId);
	UFUNCTION(Server, Reliable) void ServerBuyEquipment(ENSCEquipment Item);

	void OnFirePressed();
	void OnScoreboardStart() { bScoreboardShown = true; UpdateWidgetVisibility(); }
	void OnScoreboardStop() { bScoreboardShown = false; UpdateWidgetVisibility(); }
	void UpdateWidgetVisibility();
	void ApplyInputMode();
	void AddMappingContext();
	bool IsOwnPawnAlive() const;

	UPROPERTY() TObjectPtr<UNSCInputSet> InputSet;
	UPROPERTY() TObjectPtr<UNSCHUDWidget> HUDWidget;
	UPROPERTY() TObjectPtr<UNSCBuyMenuWidget> BuyWidget;
	UPROPERTY() TObjectPtr<UNSCScoreboardWidget> ScoreWidget;
	UPROPERTY() TObjectPtr<UNSCPauseMenuWidget> PauseWidget;
	UPROPERTY() TObjectPtr<AActor> SpectateTarget;

	bool bBuyMenuOpen = false;
	bool bPauseOpen = false;
	bool bScoreboardShown = false;
	bool bMappingAdded = false;
	float DeathTime = -1.f;
};
