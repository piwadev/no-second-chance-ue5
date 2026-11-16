// C++ bot: perception (FOV cone + line of sight + smoke + flash), reaction time, burst/spray control,
// objective play (plant, defuse, carry/pick up the charge, hold sites), reviving and bandaging.
// No Behavior Tree assets needed. Uses the NavMesh when present and falls back to direct steering.
#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "NSCTypes.h"
#include "NSCAIController.generated.h"

class ANSCCharacter;
class ANSCBombSite;

UENUM()
enum class ENSCBotGoal : uint8
{
	Idle,
	Hold,
	MoveToSite,
	PlantCharge,
	GuardCharge,
	Defuse,
	PickUpCharge,
	Revive,
	Investigate,
	Retake
};

UCLASS()
class NOSECONDCHANCE_API ANSCAIController : public AAIController
{
	GENERATED_BODY()

public:
	ANSCAIController();
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnPossess(APawn* InPawn) override;

protected:
	ANSCCharacter* GetBot() const;
	float Skill() const;

	void UpdatePerception(ANSCCharacter* Me, float Now);
	bool CanSee(const ANSCCharacter* Me, const ANSCCharacter* Other, FVector& OutAimPoint) const;
	void Combat(ANSCCharacter* Me, float DeltaSeconds, float Now);
	void ChooseGoal(ANSCCharacter* Me, float Now);
	void PursueGoal(ANSCCharacter* Me, float DeltaSeconds, float Now);
	void DownedBehaviour(ANSCCharacter* Me);
	void MoveTowards(ANSCCharacter* Me, const FVector& Dest, float AcceptRadius, bool bAllowSprint);
	void FaceTowards(const FVector& Point, float DeltaSeconds, float Speed);
	bool RebuildPath(const ANSCCharacter* Me, const FVector& Dest);
	void ManageWeapon(ANSCCharacter* Me, bool bInCombat);
	ANSCBombSite* FindSite(FName Name) const;
	FVector RandomReachable(const FVector& Center, float Radius) const;
	ANSCCharacter* FindDownedTeammate(const ANSCCharacter* Me, float MaxDist) const;

	UPROPERTY() TObjectPtr<ANSCCharacter> Target;
	ENSCBotGoal Goal = ENSCBotGoal::Idle;
	FVector GoalLocation = FVector::ZeroVector;
	FVector LastKnownEnemy = FVector::ZeroVector;
	FVector LookAtWhenIdle = FVector::ZeroVector;
	TArray<FVector> PathPoints;
	int32 PathIndex = 0;
	FVector PathDest = FVector(MAX_flt);
	float NextPathTime = 0.f;
	float NextPerception = 0.f;
	float NextGoalTime = 0.f;
	float ReactionReadyTime = 0.f;
	float LastSawEnemyTime = -100.f;
	float BurstPauseUntil = 0.f;
	int32 BurstShots = 0;
	int32 BurstLength = 4;
	FVector AimError = FVector::ZeroVector;
	float StuckCheckTime = 0.f;
	FVector StuckCheckLocation = FVector::ZeroVector;
	float StrafeSign = 1.f;
	float NextStrafeFlip = 0.f;
	float StartDelayUntil = 0.f;
	bool bThrewSmoke = false;
	bool bPendingThrow = false;
	FVector PendingThrowTarget = FVector::ZeroVector;
	float PendingThrowGiveUp = 0.f;
	int32 RoundSeen = -1;
};
