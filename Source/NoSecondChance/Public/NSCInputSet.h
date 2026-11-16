// Enhanced Input actions + mapping context built in code, so the game needs no input .uasset files.
// (Tools/Setup.bat additionally creates editable IA_/IMC_ assets under /Game/NSC/Input for designers;
//  the C++ runtime set below is what the game uses.)
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "NSCInputSet.generated.h"

class UInputAction;
class UInputMappingContext;

UCLASS()
class NOSECONDCHANCE_API UNSCInputSet : public UObject
{
	GENERATED_BODY()

public:
	/** Creates all actions and the default keyboard/mouse mapping context. */
	static UNSCInputSet* Create(UObject* Outer);

	UPROPERTY() TObjectPtr<UInputMappingContext> Context;

	UPROPERTY() TObjectPtr<UInputAction> Move;
	UPROPERTY() TObjectPtr<UInputAction> Look;
	UPROPERTY() TObjectPtr<UInputAction> Jump;
	UPROPERTY() TObjectPtr<UInputAction> Sprint;
	UPROPERTY() TObjectPtr<UInputAction> Walk;
	UPROPERTY() TObjectPtr<UInputAction> Crouch;
	UPROPERTY() TObjectPtr<UInputAction> Fire;
	UPROPERTY() TObjectPtr<UInputAction> Aim;
	UPROPERTY() TObjectPtr<UInputAction> Reload;
	UPROPERTY() TObjectPtr<UInputAction> SlotPrimary;
	UPROPERTY() TObjectPtr<UInputAction> SlotSecondary;
	UPROPERTY() TObjectPtr<UInputAction> SlotSmoke;
	UPROPERTY() TObjectPtr<UInputAction> SlotFlash;
	UPROPERTY() TObjectPtr<UInputAction> SlotFrag;
	UPROPERTY() TObjectPtr<UInputAction> CycleWeapon;
	UPROPERTY() TObjectPtr<UInputAction> LeanLeft;
	UPROPERTY() TObjectPtr<UInputAction> LeanRight;
	UPROPERTY() TObjectPtr<UInputAction> Interact;
	UPROPERTY() TObjectPtr<UInputAction> Drag;
	UPROPERTY() TObjectPtr<UInputAction> Bandage;
	UPROPERTY() TObjectPtr<UInputAction> Tourniquet;
	UPROPERTY() TObjectPtr<UInputAction> BuyMenu;
	UPROPERTY() TObjectPtr<UInputAction> Scoreboard;
	UPROPERTY() TObjectPtr<UInputAction> Pause;
	UPROPERTY() TObjectPtr<UInputAction> SpectateNext;
};
