// The demolition charge (carried, dropped, planted) and bomb-site volumes.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NSCBomb.generated.h"

class UBoxComponent;
class USphereComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class ANSCCharacter;

UENUM(BlueprintType)
enum class ENSCBombState : uint8
{
	Carried,
	Dropped,
	Planted,
	Defused,
	Exploded
};

UCLASS()
class NOSECONDCHANCE_API ANSCBombSite : public AActor
{
	GENERATED_BODY()
public:
	ANSCBombSite();

	UPROPERTY(VisibleAnywhere, Category = "NSC") TObjectPtr<UBoxComponent> Volume;
	/** "A" or "B" */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NSC") FName SiteName = TEXT("A");

	bool Contains(const FVector& Location) const;
	static ANSCBombSite* FindSiteAt(const UWorld* World, const FVector& Location);
};

UCLASS()
class NOSECONDCHANCE_API ANSCBomb : public AActor
{
	GENERATED_BODY()

public:
	ANSCBomb();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	// server
	void GiveTo(ANSCCharacter* NewCarrier);
	void Drop(const FVector& Location);
	void Plant(ANSCCharacter* Planter, FName Site);
	void SetDefused();
	void Explode();

	ENSCBombState GetState() const { return State; }
	ANSCCharacter* GetCarrier() const { return Carrier; }
	FName GetPlantedSite() const { return PlantedSite; }
	bool IsPlanted() const { return State == ENSCBombState::Planted; }

	UPROPERTY(VisibleAnywhere, Category = "NSC") TObjectPtr<USphereComponent> Pickup;
	UPROPERTY(VisibleAnywhere, Category = "NSC") TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY(VisibleAnywhere, Category = "NSC") TObjectPtr<UPointLightComponent> Blinker;

protected:
	virtual void BeginPlay() override;
	UFUNCTION() void OnPickupOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	UFUNCTION() void OnRep_State();
	UFUNCTION(NetMulticast, Reliable) void MulticastExplode();
	void UpdateVisibility();

	UPROPERTY(ReplicatedUsing = OnRep_State) ENSCBombState State = ENSCBombState::Dropped;
	UPROPERTY(Replicated) TObjectPtr<ANSCCharacter> Carrier = nullptr;
	UPROPERTY(Replicated) FName PlantedSite = NAME_None;

	float NextBeep = 0.f;
	float LocalPlantTime = 0.f;
};
