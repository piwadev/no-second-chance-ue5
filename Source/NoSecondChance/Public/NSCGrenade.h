// Smoke, flash and frag grenades. Spawned and simulated on the server, replicated movement for everyone.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/NetSerialization.h"
#include "NSCTypes.h"
#include "NSCGrenade.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;
class UPointLightComponent;
class AController;

UCLASS()
class NOSECONDCHANCE_API ANSCGrenade : public AActor
{
	GENERATED_BODY()

public:
	ANSCGrenade();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	void Launch(ENSCGrenadeType InType, const FVector& Velocity, AController* InInstigator);

	/** True if the segment passes through an active smoke cloud (bots use this for line of sight). */
	static bool IsLineBlockedBySmoke(const UWorld* World, const FVector& From, const FVector& To);

	UPROPERTY(VisibleAnywhere) TObjectPtr<USphereComponent> Collision;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UProjectileMovementComponent> Movement;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> FlashLight;

	static constexpr float SmokeRadius = 380.f;
	static constexpr float SmokeDuration = 18.f;

protected:
	virtual void BeginPlay() override;
	void Detonate();
	void DoFrag();
	void DoFlash();
	void StartSmoke();
	void BuildSmokeVisual();

	UFUNCTION() void OnRep_Detonated();
	UFUNCTION() void OnRep_Type();
	void UpdateMesh();
	UFUNCTION(NetMulticast, Reliable) void MulticastDetonateFX(ENSCGrenadeType InType, FVector_NetQuantize Where);

	UPROPERTY(ReplicatedUsing = OnRep_Type) ENSCGrenadeType Type = ENSCGrenadeType::Frag;
	UPROPERTY(ReplicatedUsing = OnRep_Detonated) bool bDetonated = false;
	/** Local clock on every machine (server: gameplay, clients: visuals) */
	float SmokeEndTime = 0.f;

	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> SmokePuffs;
	TWeakObjectPtr<AController> InstigatorController;
	FTimerHandle FuseTimer;
	float FlashLightOff = 0.f;
};
