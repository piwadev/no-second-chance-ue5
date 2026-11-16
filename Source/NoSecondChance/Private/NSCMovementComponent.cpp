#include "NSCMovementComponent.h"
#include "NSCCharacter.h"
#include "NSCHealthComponent.h"
#include "NSCWeaponComponent.h"
#include "GameFramework/Character.h"

UNSCMovementComponent::UNSCMovementComponent()
{
	bWantsToSprint = false;
	bWantsToWalk = false;
	MaxWalkSpeed = RunSpeed;
	MaxWalkSpeedCrouched = CrouchSpeed;
	MaxAcceleration = 4800.f;
	BrakingDecelerationWalking = 3200.f;
	GroundFriction = 9.f;
	JumpZVelocity = 460.f;
	AirControl = 0.12f;
	GravityScale = 1.5f;
	NavAgentProps.bCanCrouch = true;
	bCanWalkOffLedgesWhenCrouching = true;
	SetCrouchedHalfHeight(58.f);
}

float UNSCMovementComponent::GetMaxSpeed() const
{
	const ANSCCharacter* C = Cast<ANSCCharacter>(CharacterOwner);
	if (!C || (MovementMode != MOVE_Walking && MovementMode != MOVE_NavWalking && MovementMode != MOVE_Falling))
	{
		return Super::GetMaxSpeed();
	}
	if (C->GetHealth() && C->GetHealth()->IsDead()) { return 0.f; }
	if (C->GetHealth() && C->GetHealth()->IsDowned()) { return CrawlSpeed; }
	if (C->IsFrozen()) { return 0.f; }
	if (C->IsBusyWithAction()) { return 0.f; }

	float Speed = RunSpeed;
	if (C->IsDragging()) { Speed = DragSpeed; }
	else if (IsCrouching()) { Speed = CrouchSpeed; }
	else if (bWantsToWalk) { Speed = WalkSpeed; }
	else if (bWantsToSprint && C->CanSprint()) { Speed = SprintSpeed; }

	if (C->GetWeapons()) { Speed *= C->GetWeapons()->GetMovementMultiplier(); }
	if (C->GetHealth()) { Speed *= C->GetHealth()->GetMovementMultiplier(); }
	if (C->IsAimingWeapon()) { Speed *= 0.65f; }
	return Speed;
}

float UNSCMovementComponent::GetMaxAcceleration() const
{
	const ANSCCharacter* C = Cast<ANSCCharacter>(CharacterOwner);
	if (C && C->GetHealth() && C->GetHealth()->IsDowned()) { return 600.f; }
	return Super::GetMaxAcceleration();
}

bool UNSCMovementComponent::CanAttemptJump() const
{
	const ANSCCharacter* C = Cast<ANSCCharacter>(CharacterOwner);
	if (C && (C->IsFrozen() || C->IsBusyWithAction() || C->IsDragging() || (C->GetHealth() && !C->GetHealth()->IsAliveAndStanding()))) { return false; }
	return Super::CanAttemptJump();
}

bool UNSCMovementComponent::CanCrouchInCurrentState() const
{
	return Super::CanCrouchInCurrentState();
}

void UNSCMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);
	bWantsToSprint = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
	bWantsToWalk = (Flags & FSavedMove_Character::FLAG_Custom_1) != 0;
}

FNetworkPredictionData_Client* UNSCMovementComponent::GetPredictionData_Client() const
{
	if (ClientPredictionData == nullptr)
	{
		UNSCMovementComponent* MutableThis = const_cast<UNSCMovementComponent*>(this);
		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_NSC(*this);
	}
	return ClientPredictionData;
}

// ---------------------------------------------------------------- saved move
void FSavedMove_NSC::Clear()
{
	Super::Clear();
	bSavedWantsToSprint = false;
	bSavedWantsToWalk = false;
}

uint8 FSavedMove_NSC::GetCompressedFlags() const
{
	uint8 Result = Super::GetCompressedFlags();
	if (bSavedWantsToSprint) { Result |= FLAG_Custom_0; }
	if (bSavedWantsToWalk) { Result |= FLAG_Custom_1; }
	return Result;
}

bool FSavedMove_NSC::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const
{
	const FSavedMove_NSC* Other = static_cast<const FSavedMove_NSC*>(NewMove.Get());
	if (Other->bSavedWantsToSprint != bSavedWantsToSprint || Other->bSavedWantsToWalk != bSavedWantsToWalk) { return false; }
	return Super::CanCombineWith(NewMove, InCharacter, MaxDelta);
}

void FSavedMove_NSC::SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData)
{
	Super::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);
	if (const UNSCMovementComponent* Move = Cast<UNSCMovementComponent>(C->GetCharacterMovement()))
	{
		bSavedWantsToSprint = Move->bWantsToSprint;
		bSavedWantsToWalk = Move->bWantsToWalk;
	}
}

void FSavedMove_NSC::PrepMoveFor(ACharacter* C)
{
	Super::PrepMoveFor(C);
	if (UNSCMovementComponent* Move = Cast<UNSCMovementComponent>(C->GetCharacterMovement()))
	{
		Move->bWantsToSprint = bSavedWantsToSprint;
		Move->bWantsToWalk = bSavedWantsToWalk;
	}
}

FSavedMovePtr FNetworkPredictionData_Client_NSC::AllocateNewMove()
{
	return FSavedMovePtr(new FSavedMove_NSC());
}
