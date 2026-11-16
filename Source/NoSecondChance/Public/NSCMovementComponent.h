// Character movement with client-predicted sprint / walk flags (saved moves), and speed rules for
// injuries, weapons, crouch, downed crawling and dragging.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NSCMovementComponent.generated.h"

UCLASS()
class NOSECONDCHANCE_API UNSCMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UNSCMovementComponent();

	virtual float GetMaxSpeed() const override;
	virtual float GetMaxAcceleration() const override;
	virtual bool CanAttemptJump() const override;
	virtual bool CanCrouchInCurrentState() const override;
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;

	uint8 bWantsToSprint : 1;
	uint8 bWantsToWalk : 1;

	static constexpr float RunSpeed = 500.f;
	static constexpr float SprintSpeed = 640.f;
	static constexpr float WalkSpeed = 260.f;
	static constexpr float CrouchSpeed = 230.f;
	static constexpr float CrawlSpeed = 65.f;
	static constexpr float DragSpeed = 170.f;
};

class FSavedMove_NSC : public FSavedMove_Character
{
public:
	typedef FSavedMove_Character Super;

	uint8 bSavedWantsToSprint : 1;
	uint8 bSavedWantsToWalk : 1;

	FSavedMove_NSC() : bSavedWantsToSprint(0), bSavedWantsToWalk(0) {}

	virtual void Clear() override;
	virtual uint8 GetCompressedFlags() const override;
	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const override;
	virtual void SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, class FNetworkPredictionData_Client_Character& ClientData) override;
	virtual void PrepMoveFor(ACharacter* C) override;
};

class FNetworkPredictionData_Client_NSC : public FNetworkPredictionData_Client_Character
{
public:
	typedef FNetworkPredictionData_Client_Character Super;
	explicit FNetworkPredictionData_Client_NSC(const UCharacterMovementComponent& ClientMovement) : Super(ClientMovement) {}
	virtual FSavedMovePtr AllocateNewMove() override;
};
