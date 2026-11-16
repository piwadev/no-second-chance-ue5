// Data-driven weapons. Every weapon is an FNSCWeaponStats row. The game works out of the box with the built-in
// table below (UNSCWeaponLibrary::GetBuiltinWeapons). To tune without recompiling, create a DataTable with row
// struct NSCWeaponStats at /Game/NSC/Data/DT_Weapons (Tools/Setup.bat creates it), or UNSCWeaponData assets.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Engine/DataAsset.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "NSCTypes.h"
#include "NSCWeaponData.generated.h"

class UStaticMesh;
class USoundBase;

USTRUCT(BlueprintType)
struct FNSCWeaponStats : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon") FName Id = NAME_None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon") FString DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon") FString ShortName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon") ENSCWeaponClass WeaponClass = ENSCWeaponClass::Rifle;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon") ENSCWeaponSlot Slot = ENSCWeaponSlot::Primary;
	/** None = both teams may buy it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon") ENSCTeam TeamOnly = ENSCTeam::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Economy") int32 Price = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Economy") int32 KillReward = 300;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage") float Damage = 30.f;
	/** Fraction of damage that goes through armor / helmet (CS "armor penetration") */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage") float ArmorPen = 0.75f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage") float HeadMultiplier = 4.f;
	/** Damage multiplier per 10 m of distance (CS range modifier) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage") float RangeModifier = 0.98f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage") float MaxRange = 8000.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage") int32 Pellets = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage") float PelletSpread = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Handling") float FireInterval = 0.1f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Handling") bool bAutomatic = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Handling") int32 MagSize = 30;
	/** Magazines carried, including the one in the gun */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Handling") int32 MagCount = 4;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Handling") float ReloadTime = 2.5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Handling") float EmptyReloadTime = 2.9f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Handling") float DrawTime = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Handling") float MoveSpeedMultiplier = 0.9f;
	/** Radians. Standing still, first shot. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Handling") float BaseSpread = 0.0015f;
	/** Radians added at full running speed */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Handling") float MoveSpread = 0.08f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Handling") float JumpSpread = 0.2f;
	/** Shots of spray recovered per second when not firing */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Handling") float SprayRecovery = 11.f;
	/** Visual camera kick as a fraction of the spray offset */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Handling") float ViewKick = 0.45f;
	/** 0 = no scope. Otherwise the aim-down-sights FOV */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Handling") float AimFOV = 70.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Handling") bool bScoped = false;
	/** Spread is only accurate when scoped (snipers) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Handling") float UnscopedSpread = 0.f;

	/** Deterministic spray pattern: cumulative (yaw, pitch) offset in degrees for shot i. Pitch is upward. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recoil") TArray<FVector2D> SprayPattern;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation") TSoftObjectPtr<UStaticMesh> Mesh;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation") FVector MeshOffset1P = FVector(35.f, 14.f, -18.f);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation") FRotator MeshRotation = FRotator::ZeroRotator;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation") float MeshScale = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation") TSoftObjectPtr<USoundBase> FireSound;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation") TSoftObjectPtr<USoundBase> ReloadSound;

	bool IsValid() const { return Id != NAME_None; }
};

/** Optional per-weapon data asset (alternative to a DataTable row). */
UCLASS(BlueprintType)
class NOSECONDCHANCE_API UNSCWeaponData : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon") FNSCWeaponStats Stats;
};

UENUM(BlueprintType)
enum class ENSCEquipment : uint8
{
	None,
	Armor,
	ArmorHelmet,
	Medkit,
	Bandage,
	Tourniquet,
	Smoke,
	Flash,
	Frag,
	DefuseKit,
	Resupply
};

USTRUCT(BlueprintType)
struct FNSCEquipmentInfo
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) ENSCEquipment Type = ENSCEquipment::None;
	UPROPERTY(BlueprintReadOnly) FString Name;
	UPROPERTY(BlueprintReadOnly) int32 Price = 0;
	UPROPERTY(BlueprintReadOnly) int32 MaxCarry = 1;
	UPROPERTY(BlueprintReadOnly) ENSCTeam TeamOnly = ENSCTeam::None;
};

UCLASS()
class NOSECONDCHANCE_API UNSCWeaponLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/** Looks the weapon up in /Game/NSC/Data/DT_Weapons first, then in the built-in table. */
	UFUNCTION(BlueprintCallable, Category = "NSC|Weapons")
	static bool GetWeaponStats(FName Id, FNSCWeaponStats& OutStats);

	static const TArray<FNSCWeaponStats>& GetAllWeapons();
	static const TArray<FNSCEquipmentInfo>& GetAllEquipment();
	static const FNSCEquipmentInfo* FindEquipment(ENSCEquipment Type);

	/** Deterministic, learnable spray: pitch rises exponentially to PitchMax, yaw follows keyframes. */
	static TArray<FVector2D> MakePattern(int32 Shots, float PitchMax, float K, const TArray<float>& YawKeys);

	static FName DefaultPistol(ENSCTeam Team);

private:
	static TArray<FNSCWeaponStats> BuildBuiltinWeapons();
};
