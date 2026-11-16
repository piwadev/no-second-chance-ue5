// Weapons, magazines (with memory), grenades and medical consumables carried by a character.
// Firing: the owning client simulates cosmetics and sends ServerFire; the server validates (rate, ammo, origin,
// aim), applies the deterministic spray pattern + spread, traces (with a simple lag-compensation rewind) and
// applies damage. Bots fire on the server directly.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/NetSerialization.h"
#include "NSCTypes.h"
#include "NSCWeaponData.h"
#include "NSCWeaponComponent.generated.h"

class ANSCCharacter;
class ANSCGrenade;

USTRUCT(BlueprintType)
struct FNSCWeaponInstance
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FName Id = NAME_None;
	/** Rounds left in every magazine you carry. Partially used magazines are kept (tactical reload). */
	UPROPERTY(BlueprintReadOnly) TArray<int32> Mags;
	UPROPERTY(BlueprintReadOnly) int32 CurrentMag = 0;

	bool IsValid() const { return Id != NAME_None; }
	int32 Rounds() const { return Mags.IsValidIndex(CurrentMag) ? Mags[CurrentMag] : 0; }
	int32 SpareMags() const;
	int32 ReserveRounds() const;
	void Init(const FNSCWeaponStats& S);
	void Clear() { Id = NAME_None; Mags.Reset(); CurrentMag = 0; }
};

/** What a surviving player carries into the next round (CS rules: survivors keep their gear). */
USTRUCT(BlueprintType)
struct FNSCLoadout
{
	GENERATED_BODY()
	UPROPERTY() FNSCWeaponInstance Primary;
	UPROPERTY() FNSCWeaponInstance Secondary;
	UPROPERTY() int32 Smokes = 0;
	UPROPERTY() int32 Flashes = 0;
	UPROPERTY() int32 Frags = 0;
	UPROPERTY() int32 Bandages = 0;
	UPROPERTY() int32 Tourniquets = 0;
	UPROPERTY() int32 Medkits = 0;
	UPROPERTY() bool bDefuseKit = false;
	UPROPERTY() float Armor = 0.f;
	UPROPERTY() bool bHelmet = false;
	UPROPERTY() bool bValid = false;
};

UCLASS(ClassGroup = (NSC), meta = (BlueprintSpawnableComponent))
class NOSECONDCHANCE_API UNSCWeaponComponent : public UActorComponent
{
	GENERATED_BODY()
	friend class ANSCGameMode;
	friend class ANSCAIController;
	friend class UNSCHUDWidget;

public:
	UNSCWeaponComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ------------------------------------------------------------ input (owning client or server for bots)
	void StartFire();
	void StopFire();
	void Reload();
	void SelectSlot(ENSCWeaponSlot Slot);
	void SelectGrenade(ENSCGrenadeType Type);
	void CycleWeapon(int32 Direction);
	void SetAiming(bool bAim);
	bool IsAiming() const { return bAiming; }

	// ------------------------------------------------------------ server / authority
	void GiveWeapon(FName WeaponId);
	void GiveDefaultLoadout(ENSCTeam Team);
	void ApplyLoadout(const FNSCLoadout& L);
	FNSCLoadout MakeLoadout() const;
	void ClearAll();
	void RefillAmmo();
	/** Bot entry point: fire one shot (or pellet burst) along AimRotation from the eyes. */
	void BotFireOnce(const FRotator& AimRotation);
	bool CanFire() const;

	UPROPERTY(Replicated, BlueprintReadOnly) int32 Smokes = 0;
	UPROPERTY(Replicated, BlueprintReadOnly) int32 Flashes = 0;
	UPROPERTY(Replicated, BlueprintReadOnly) int32 Frags = 0;
	UPROPERTY(Replicated, BlueprintReadOnly) int32 Bandages = 0;
	UPROPERTY(Replicated, BlueprintReadOnly) int32 Tourniquets = 0;
	UPROPERTY(Replicated, BlueprintReadOnly) int32 Medkits = 0;
	UPROPERTY(Replicated, BlueprintReadOnly) bool bDefuseKit = false;

	// ------------------------------------------------------------ queries
	const FNSCWeaponInstance* GetActiveWeapon() const;
	FNSCWeaponInstance* GetActiveWeaponMutable();
	bool GetActiveStats(FNSCWeaponStats& Out) const;
	const FNSCWeaponInstance& GetPrimary() const { return Primary; }
	const FNSCWeaponInstance& GetSecondary() const { return Secondary; }
	ENSCWeaponSlot GetActiveSlot() const { return ActiveSlot; }
	ENSCGrenadeType GetSelectedGrenade() const { return SelectedGrenade; }
	int32 GetGrenadeCount(ENSCGrenadeType T) const;
	bool IsReloading() const { return bReloading; }
	float GetReloadFraction() const;
	float GetCurrentSpread() const;
	float GetSprayHeat() const { return SprayHeat; }
	/** Visual camera recoil (pitch, yaw degrees) for the owning client. */
	FVector2D GetViewKick() const { return ViewKick; }
	float GetMovementMultiplier() const;
	FName GetActiveWeaponId() const;

protected:
	virtual void BeginPlay() override;

	ANSCCharacter* GetNSCOwner() const;
	void FireOnceLocal();
	void FireAuthority(const FVector& Origin, const FRotator& AimRotation, float ClientTime);
	void ThrowGrenadeAuthority(const FVector& Origin, const FRotator& Aim, bool bLob);
	FRotator ApplySprayToAim(const FRotator& Aim, const FNSCWeaponStats& S, int32 ShotIndex) const;
	void PlayFireCosmetics(const FNSCWeaponStats& S);
	void SpawnImpactFX(const FVector& Point, const FVector& Normal, bool bHitPawn) const;
	void FinishReload();
	bool TraceShot(const FVector& Start, const FVector& Dir, float Range, FHitResult& OutHit) const;
	/** Lag-compensation rewind: tests the ray against where other characters were ClientLatency seconds ago. */
	class ANSCCharacter* RewindTest(const FVector& Start, const FVector& Dir, float Range, float RewindSeconds, FVector& OutPoint, float& OutZ01, float& OutSide) const;

	UFUNCTION(Server, Reliable) void ServerFire(FVector_NetQuantize Origin, FVector_NetQuantizeNormal Direction, float ClientTime);
	UFUNCTION(Server, Reliable) void ServerReload();
	UFUNCTION(Server, Reliable) void ServerSelectSlot(ENSCWeaponSlot Slot, ENSCGrenadeType Grenade);
	UFUNCTION(Server, Reliable) void ServerThrowGrenade(FVector_NetQuantize Origin, FRotator Aim, bool bLob);
	UFUNCTION(Server, Unreliable) void ServerSetAiming(bool bAim);
	UFUNCTION(NetMulticast, Unreliable) void MulticastShotFX(FVector_NetQuantize End, FVector_NetQuantizeNormal Normal, bool bHitPawn, bool bHasImpact);

	UFUNCTION() void OnRep_Weapons();
	UFUNCTION() void OnRep_Reloading();

	UPROPERTY(ReplicatedUsing = OnRep_Weapons) FNSCWeaponInstance Primary;
	UPROPERTY(ReplicatedUsing = OnRep_Weapons) FNSCWeaponInstance Secondary;
	UPROPERTY(ReplicatedUsing = OnRep_Weapons) ENSCWeaponSlot ActiveSlot = ENSCWeaponSlot::Secondary;
	UPROPERTY(ReplicatedUsing = OnRep_Weapons) ENSCGrenadeType SelectedGrenade = ENSCGrenadeType::None;
	UPROPERTY(ReplicatedUsing = OnRep_Reloading) bool bReloading = false;
	UPROPERTY(Replicated) bool bAiming = false;

	bool bWantsFire = false;
	bool bFiredSinceTrigger = false;
	float NextFireTime = 0.f;
	float LastShotTime = -100.f;
	float SprayHeat = 0.f;            // owning client + server each keep their own (same rules)
	float DrawEndTime = 0.f;
	float ReloadStartTime = 0.f;
	float ReloadDuration = 0.f;
	FTimerHandle ReloadTimer;
	FVector2D ViewKick = FVector2D::ZeroVector;
	int32 ServerShotCounter = 0;
	FRandomStream SpreadStream;
};
