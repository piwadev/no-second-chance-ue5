// Injury, armor, bleeding, downed and rescue state. Server authoritative, replicated to everyone.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NSCTypes.h"
#include "NSCHealthComponent.generated.h"

class ANSCCharacter;
class AController;

USTRUCT(BlueprintType)
struct FNSCHitInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite) float BaseDamage = 0.f;     // weapon damage after range falloff, before zone/armor
	UPROPERTY(BlueprintReadWrite) float ArmorPen = 0.75f;
	UPROPERTY(BlueprintReadWrite) float HeadMultiplier = 4.f;
	UPROPERTY(BlueprintReadWrite) ENSCHitZone Zone = ENSCHitZone::Chest;
	UPROPERTY(BlueprintReadWrite) FName WeaponId = NAME_None;
	UPROPERTY(BlueprintReadWrite) bool bExplosive = false;
	UPROPERTY(BlueprintReadWrite) FVector HitLocation = FVector::ZeroVector;
	UPROPERTY(BlueprintReadWrite) FVector HitDirection = FVector::ForwardVector;
	UPROPERTY() TObjectPtr<AController> Instigator = nullptr;
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FNSCOnDownedNative, ANSCCharacter* /*Victim*/, const FNSCHitInfo& /*Hit*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FNSCOnDiedNative, ANSCCharacter* /*Victim*/, const FNSCHitInfo& /*Hit*/);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FNSCOnDamagedNative, ANSCCharacter* /*Victim*/, const FNSCHitInfo& /*Hit*/, float /*HealthDamage*/);

UCLASS(ClassGroup = (NSC), meta = (BlueprintSpawnableComponent))
class NOSECONDCHANCE_API UNSCHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNSCHealthComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ------------------------------------------------------------ server API
	/** Applies one bullet / pellet / explosion. Returns the health damage dealt. Server only. */
	float ApplyHit(const FNSCHitInfo& Hit);
	/** Full reset for a new round (keeps armor/helmet if bKeepArmor). */
	void ResetForRound(bool bKeepArmor);
	void SetArmor(float NewArmor, bool bHelmet);
	/** Teammate finished a revive. */
	void Revive(AController* Rescuer);
	/** Bandage: stops a light bleed, turns a heavy bleed into a light one. */
	bool ApplyBandage();
	/** Tourniquet: stops a heavy limb bleed. */
	bool ApplyTourniquet();
	void Kill(const FNSCHitInfo& Hit);

	// ------------------------------------------------------------ queries
	UFUNCTION(BlueprintPure, Category = "NSC|Health") float GetHealth() const { return Health; }
	UFUNCTION(BlueprintPure, Category = "NSC|Health") float GetArmor() const { return Armor; }
	UFUNCTION(BlueprintPure, Category = "NSC|Health") bool HasHelmet() const { return bHelmet; }
	UFUNCTION(BlueprintPure, Category = "NSC|Health") bool IsDowned() const { return bDowned; }
	UFUNCTION(BlueprintPure, Category = "NSC|Health") bool IsDead() const { return bDead; }
	UFUNCTION(BlueprintPure, Category = "NSC|Health") bool IsAliveAndStanding() const { return !bDead && !bDowned; }
	UFUNCTION(BlueprintPure, Category = "NSC|Health") ENSCInjuryState GetInjuryState() const;
	UFUNCTION(BlueprintPure, Category = "NSC|Health") ENSCBleed GetBleed() const { return Bleed; }
	UFUNCTION(BlueprintPure, Category = "NSC|Health") bool HasLegInjury() const { return bLegInjured; }
	UFUNCTION(BlueprintPure, Category = "NSC|Health") bool HasArmInjury() const { return bArmInjured; }
	UFUNCTION(BlueprintPure, Category = "NSC|Health") float GetBleedoutRemaining() const { return BleedoutRemaining; }
	UFUNCTION(BlueprintPure, Category = "NSC|Health") float GetReviveProgress() const { return ReviveProgress; }
	UFUNCTION(BlueprintPure, Category = "NSC|Health") bool WasRescuedThisRound() const { return bRescuedThisRound; }

	/** Movement speed multiplier from injuries (leg wound, critical state). */
	float GetMovementMultiplier() const;
	/** Spread / recoil multiplier from an arm wound. */
	float GetHandlingMultiplier() const { return bArmInjured ? 1.35f : 1.f; }
	float GetReloadMultiplier() const { return bArmInjured ? 1.25f : 1.f; }

	/** Maps a physics-asset bone name to a hit zone (works for UE mannequin, Quaternius and Mixamo rigs). */
	static ENSCHitZone ZoneFromBone(FName Bone);
	/** Fallback when the hit has no bone (capsule hit): use the height on the capsule. */
	static ENSCHitZone ZoneFromHeight(float RelativeZ01, float SideSign);

	void SetReviveProgress(float P) { ReviveProgress = P; }

	FNSCOnDownedNative OnDowned;
	FNSCOnDiedNative OnDied;
	FNSCOnDamagedNative OnDamaged;

	/** Last time we took damage (server), used for assists */
	TMap<TWeakObjectPtr<AController>, float> DamageByController;

protected:
	virtual void BeginPlay() override;

	UFUNCTION() void OnRep_Downed();
	UFUNCTION() void OnRep_Dead();

	void GoDown(const FNSCHitInfo& Hit);
	ANSCCharacter* GetNSCOwner() const;

	UPROPERTY(Replicated) float Health = NSCRules::MaxHealth;
	UPROPERTY(Replicated) float Armor = 0.f;
	UPROPERTY(Replicated) bool bHelmet = false;
	UPROPERTY(Replicated) ENSCBleed Bleed = ENSCBleed::None;
	UPROPERTY(Replicated) bool bLegInjured = false;
	UPROPERTY(Replicated) bool bArmInjured = false;
	UPROPERTY(ReplicatedUsing = OnRep_Downed) bool bDowned = false;
	UPROPERTY(ReplicatedUsing = OnRep_Dead) bool bDead = false;
	UPROPERTY(Replicated) float BleedoutRemaining = 0.f;
	UPROPERTY(Replicated) float ReviveProgress = 0.f;
	UPROPERTY(Replicated) bool bRescuedThisRound = false;

	float DownedHealth = NSCRules::DownedHealth;
	FNSCHitInfo LastHit;
};
