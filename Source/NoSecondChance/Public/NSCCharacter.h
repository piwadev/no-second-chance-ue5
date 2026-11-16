// First-person soldier: movement (run/walk/sprint/crouch/lean), injuries, downed crawling, dragging,
// reviving, bandages, planting/defusing, weapon presentation and animation state.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "NSCTypes.h"
#include "NSCWeaponData.h"
#include "NSCCharacter.generated.h"

class UCameraComponent;
class USceneComponent;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class UNSCHealthComponent;
class UNSCWeaponComponent;
class UNSCMovementComponent;
class UAnimSequence;
class ANSCBomb;
class ANSCPlayerState;
struct FInputActionValue;

USTRUCT()
struct FNSCPositionSample
{
	GENERATED_BODY()
	float Time = 0.f;
	FVector Location = FVector::ZeroVector;
};

UCLASS()
class NOSECONDCHANCE_API ANSCCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ANSCCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;
	virtual FVector GetPawnViewLocation() const override;
	virtual void Destroyed() override;

	// ------------------------------------------------------------ components
	UNSCHealthComponent* GetHealth() const { return Health; }
	UNSCWeaponComponent* GetWeapons() const { return Weapons; }
	UNSCMovementComponent* GetNSCMovement() const;
	UCameraComponent* GetFirstPersonCamera() const { return FirstPersonCamera; }

	// ------------------------------------------------------------ state queries
	ENSCTeam GetTeam() const;
	FString GetDisplayName() const;
	FVector GetEyeLocation() const;
	bool CanUseWeapons() const;
	bool CanSprint() const;
	bool IsFrozen() const;
	bool IsBusyWithAction() const { return CurrentAction != ENSCActionType::None && CurrentAction != ENSCActionType::Dragging; }
	bool IsDragging() const { return DragTarget != nullptr; }
	bool IsBeingDragged() const { return DraggedBy != nullptr; }
	bool IsAimingWeapon() const;
	bool HasBomb() const { return bHasBomb; }
	ENSCActionType GetCurrentAction() const { return CurrentAction; }
	float GetActionProgress() const { return ActionProgress; }
	float GetLean() const { return Lean; }
	float GetFlashAmount() const;
	bool IsFlashed() const;
	FString GetInteractPrompt() const { return InteractPrompt; }
	FVector GetLocationAtTime(float WorldTime) const;
	ANSCCharacter* GetDragTarget() const { return DragTarget; }

	// ------------------------------------------------------------ callbacks from components (all machines)
	void OnInjuryChanged();
	void OnDownedStateChanged(bool bNowDowned);
	void OnDeath();
	void RefreshWeaponVisuals();
	void PlayMuzzleFlash();
	void AddWeaponKick(const FNSCWeaponStats& Stats);
	void PlayReloadAnimation();
	void PlayThrowAnimation();
	void SpawnImpactParticles(const FVector& Point, const FVector& Normal, bool bFlesh) const;
	/** Server: blind this character for Duration seconds. */
	void ApplyFlash(float Duration);

	// ------------------------------------------------------------ server API (game mode / AI)
	void SetHasBomb(bool bNew);
	void SetFrozen(bool bNew) { bFrozen = bNew; }
	void ServerStartInteract_Internal();
	void ServerStopInteract_Internal();
	void ServerStartDrag_Internal();
	void StopDragging();
	void ServerUseBandage_Internal(bool bTourniquet);
	/** AI helpers */
	void SetWantsSprint(bool b);
	void SetWantsWalk(bool b);
	float BotFlashedUntil = 0.f;
	/** Server: last teammate who dragged this character while downed (for the Rescue stat). */
	TWeakObjectPtr<AController> LastDraggedBy;

	/** Third-person body color per team (sets "TeamColor" on the body material if it has one). */
	void ApplyTeamLook();

protected:
	virtual void BeginPlay() override;

	// input
	void InputMove(const FInputActionValue& V);
	void InputLook(const FInputActionValue& V);
	void InputJump();
	void InputSprintStart();
	void InputSprintStop();
	void InputWalkStart();
	void InputWalkStop();
	void InputCrouchStart();
	void InputCrouchStop();
	void InputFireStart();
	void InputFireStop();
	void InputAimStart();
	void InputAimStop();
	void InputReload();
	void InputPrimary();
	void InputSecondary();
	void InputSmoke();
	void InputFlash();
	void InputFrag();
	void InputCycle(const FInputActionValue& V);
	void InputLeanLeftStart();
	void InputLeanLeftStop();
	void InputLeanRightStart();
	void InputLeanRightStop();
	void InputInteractStart();
	void InputInteractStop();
	void InputDragStart();
	void InputDragStop();
	void InputBandage();
	void InputTourniquet();

	UFUNCTION(Server, Reliable) void ServerStartInteract();
	UFUNCTION(Server, Reliable) void ServerStopInteract();
	UFUNCTION(Server, Reliable) void ServerStartDrag();
	UFUNCTION(Server, Reliable) void ServerStopDrag();
	UFUNCTION(Server, Reliable) void ServerUseBandage(bool bTourniquet);
	UFUNCTION(Server, Unreliable) void ServerSetLean(int8 NewLean);
	UFUNCTION(Server, Reliable) void ServerSetMoveFlags(bool bSprint, bool bWalk);
	UFUNCTION(Client, Reliable) void ClientFlashed(float Duration);

	UFUNCTION() void OnRep_DraggedBy();
	UFUNCTION() void OnRep_Team();

	void TickAction(float DeltaSeconds);
	void TickDrag(float DeltaSeconds);
	void TickAnimation(float DeltaSeconds);
	void TickCamera(float DeltaSeconds);
	void TickFootsteps(float DeltaSeconds);
	void UpdateInteractPrompt();
	void CancelAction();
	ANSCCharacter* FindDownedTeammate(float Range) const;
	ANSCBomb* FindBomb() const;
	bool IsInBombSite() const;
	void PlayAnim(const TCHAR* Name, bool bLoop, float Rate = 1.f);
	UAnimSequence* FindAnim(const TCHAR* Name) const;
	void LoadCharacterMeshes();

	UPROPERTY(VisibleAnywhere, Category = "NSC") TObjectPtr<USceneComponent> CameraRoot;
	UPROPERTY(VisibleAnywhere, Category = "NSC") TObjectPtr<UCameraComponent> FirstPersonCamera;
	UPROPERTY(VisibleAnywhere, Category = "NSC") TObjectPtr<USkeletalMeshComponent> Mesh1P;
	UPROPERTY(VisibleAnywhere, Category = "NSC") TObjectPtr<UStaticMeshComponent> WeaponMesh1P;
	UPROPERTY(VisibleAnywhere, Category = "NSC") TObjectPtr<UStaticMeshComponent> WeaponMesh3P;
	UPROPERTY(VisibleAnywhere, Category = "NSC") TObjectPtr<UStaticMeshComponent> HelmetMesh;
	UPROPERTY(VisibleAnywhere, Category = "NSC") TObjectPtr<UPointLightComponent> MuzzleLight;
	UPROPERTY(VisibleAnywhere, Category = "NSC") TObjectPtr<UNSCHealthComponent> Health;
	UPROPERTY(VisibleAnywhere, Category = "NSC") TObjectPtr<UNSCWeaponComponent> Weapons;

	UPROPERTY(Replicated) ENSCActionType CurrentAction = ENSCActionType::None;
	UPROPERTY(Replicated) float ActionProgress = 0.f;
	UPROPERTY(Replicated) TObjectPtr<ANSCCharacter> ActionTarget = nullptr;
	UPROPERTY(Replicated) TObjectPtr<ANSCCharacter> DragTarget = nullptr;
	UPROPERTY(ReplicatedUsing = OnRep_DraggedBy) TObjectPtr<ANSCCharacter> DraggedBy = nullptr;
	UPROPERTY(Replicated) int8 LeanInput = 0;
	UPROPERTY(Replicated) bool bHasBomb = false;
	UPROPERTY(Replicated) bool bFrozen = false;
	UPROPERTY(ReplicatedUsing = OnRep_Team) ENSCTeam ReplicatedTeam = ENSCTeam::None;

	// local state
	float Lean = 0.f;
	float CurrentFOV = 90.f;
	float FlashEnd = 0.f;
	float FlashDuration = 1.f;
	float MuzzleLightOff = 0.f;
	float StepDistance = 0.f;
	float KickRecover = 0.f;
	FRotator WeaponSway = FRotator::ZeroRotator;
	FVector2D LastLookInput = FVector2D::ZeroVector;
	FString InteractPrompt;
	FName CurrentAnimName = NAME_None;
	float OneShotAnimUntil = 0.f;
	bool bWantsInteract = false;
	TArray<FNSCPositionSample> PositionHistory;
	FName VisualWeaponId = NAME_None;

	UPROPERTY() TMap<FName, TObjectPtr<UAnimSequence>> AnimCache;
};
