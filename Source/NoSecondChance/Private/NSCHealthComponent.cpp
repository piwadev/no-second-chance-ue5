#include "NSCHealthComponent.h"
#include "NSCCharacter.h"
#include "NSCGameMode.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"

UNSCHealthComponent::UNSCHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f;
	SetIsReplicatedByDefault(true);
}

void UNSCHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UNSCHealthComponent, Health);
	DOREPLIFETIME(UNSCHealthComponent, Armor);
	DOREPLIFETIME(UNSCHealthComponent, bHelmet);
	DOREPLIFETIME(UNSCHealthComponent, Bleed);
	DOREPLIFETIME(UNSCHealthComponent, bLegInjured);
	DOREPLIFETIME(UNSCHealthComponent, bArmInjured);
	DOREPLIFETIME(UNSCHealthComponent, bDowned);
	DOREPLIFETIME(UNSCHealthComponent, bDead);
	DOREPLIFETIME(UNSCHealthComponent, BleedoutRemaining);
	DOREPLIFETIME(UNSCHealthComponent, ReviveProgress);
	DOREPLIFETIME(UNSCHealthComponent, bRescuedThisRound);
}

void UNSCHealthComponent::BeginPlay()
{
	Super::BeginPlay();
}

ANSCCharacter* UNSCHealthComponent::GetNSCOwner() const
{
	return Cast<ANSCCharacter>(GetOwner());
}

ENSCInjuryState UNSCHealthComponent::GetInjuryState() const
{
	if (bDead) { return ENSCInjuryState::Dead; }
	if (bDowned) { return ENSCInjuryState::Downed; }
	if (Health <= 30.f || Bleed == ENSCBleed::Heavy) { return ENSCInjuryState::Critical; }
	if (Health < 70.f || Bleed != ENSCBleed::None || bLegInjured || bArmInjured) { return ENSCInjuryState::Wounded; }
	return ENSCInjuryState::Healthy;
}

float UNSCHealthComponent::GetMovementMultiplier() const
{
	float M = 1.f;
	if (bLegInjured) { M *= 0.78f; }
	if (GetInjuryState() == ENSCInjuryState::Critical) { M *= 0.9f; }
	return M;
}

ENSCHitZone UNSCHealthComponent::ZoneFromBone(FName Bone)
{
	if (Bone == NAME_None) { return ENSCHitZone::None; }
	const FString B = Bone.ToString().ToLower();
	const bool bLeft = B.EndsWith(TEXT("_l")) || B.Contains(TEXT("left")) || B.EndsWith(TEXT(".l"));
	if (B.Contains(TEXT("head")) || B.Contains(TEXT("neck"))) { return ENSCHitZone::Head; }
	if (B.Contains(TEXT("thigh")) || B.Contains(TEXT("calf")) || B.Contains(TEXT("leg")) || B.Contains(TEXT("foot")) || B.Contains(TEXT("ball")) || B.Contains(TEXT("knee")) || B.Contains(TEXT("toe")))
	{
		return bLeft ? ENSCHitZone::LeftLeg : ENSCHitZone::RightLeg;
	}
	if (B.Contains(TEXT("arm")) || B.Contains(TEXT("hand")) || B.Contains(TEXT("clavicle")) || B.Contains(TEXT("shoulder")) || B.Contains(TEXT("elbow"))
		|| B.Contains(TEXT("index")) || B.Contains(TEXT("middle")) || B.Contains(TEXT("pinky")) || B.Contains(TEXT("ring")) || B.Contains(TEXT("thumb")))
	{
		return bLeft ? ENSCHitZone::LeftArm : ENSCHitZone::RightArm;
	}
	if (B.Contains(TEXT("pelvis")) || B.Contains(TEXT("hips")) || B.Contains(TEXT("spine_01")) || B.Contains(TEXT("spine1")) || B == TEXT("spine")) { return ENSCHitZone::Stomach; }
	return ENSCHitZone::Chest;
}

ENSCHitZone UNSCHealthComponent::ZoneFromHeight(float Z01, float SideSign)
{
	if (Z01 > 0.86f) { return ENSCHitZone::Head; }
	if (Z01 > 0.62f) { return FMath::Abs(SideSign) > 0.75f ? (SideSign < 0.f ? ENSCHitZone::LeftArm : ENSCHitZone::RightArm) : ENSCHitZone::Chest; }
	if (Z01 > 0.48f) { return ENSCHitZone::Stomach; }
	return SideSign < 0.f ? ENSCHitZone::LeftLeg : ENSCHitZone::RightLeg;
}

float UNSCHealthComponent::ApplyHit(const FNSCHitInfo& Hit)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || bDead) { return 0.f; }

	// Zone multipliers (head uses the weapon's head multiplier)
	float Dmg = Hit.BaseDamage;
	bool bProtected = false;
	switch (Hit.Zone)
	{
	case ENSCHitZone::Head: Dmg *= Hit.HeadMultiplier; bProtected = bHelmet; break;
	case ENSCHitZone::Chest: bProtected = Armor > 0.f; break;
	case ENSCHitZone::Stomach: Dmg *= 1.25f; bProtected = Armor > 0.f; break;
	case ENSCHitZone::LeftArm:
	case ENSCHitZone::RightArm: Dmg *= 0.75f; bProtected = Armor > 0.f; break;
	case ENSCHitZone::LeftLeg:
	case ENSCHitZone::RightLeg: Dmg *= 0.6f; break;
	default: break;
	}
	if (Hit.bExplosive) { bProtected = Armor > 0.f; }

	// Armor: CS-style. ArmorPen fraction reaches health, armor soaks the rest at 50%.
	if (bProtected)
	{
		const float ToHealth = Dmg * Hit.ArmorPen;
		float ArmorDamage = (Dmg - ToHealth) * 0.5f;
		if (ArmorDamage > Armor)
		{
			// Armor broke: the part it could not absorb goes through.
			const float Unabsorbed = (ArmorDamage - Armor) * 2.f;
			ArmorDamage = Armor;
			Dmg = ToHealth + Unabsorbed;
		}
		else
		{
			Dmg = ToHealth;
		}
		Armor = FMath::Max(0.f, Armor - ArmorDamage);
		if (Hit.Zone == ENSCHitZone::Head && !Hit.bExplosive)
		{
			// a helmet saves you once; it is knocked loose by a head hit that it absorbs
		}
	}
	Dmg = FMath::RoundToFloat(Dmg);
	if (Dmg <= 0.f) { return 0.f; }

	LastHit = Hit;
	if (Hit.Instigator)
	{
		DamageByController.FindOrAdd(TWeakObjectPtr<AController>(Hit.Instigator.Get())) += Dmg;
	}

	ANSCCharacter* Me = GetNSCOwner();
	ANSCGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ANSCGameMode>() : nullptr;

	if (bDowned)
	{
		// A downed player can be finished off.
		DownedHealth -= Dmg;
		if (GM && Me) { GM->NotifyDamage(Me, Hit, Dmg); }
		if (DownedHealth <= 0.f) { Kill(Hit); }
		return Dmg;
	}

	const float Before = Health;
	Health -= Dmg;
	const float Dealt = FMath::Min(Dmg, Before);
	if (GM && Me) { GM->NotifyDamage(Me, Hit, Dealt); }
	OnDamaged.Broadcast(Me, Hit, Dealt);

	if (Health <= 0.f)
	{
		const float Overkill = -Health;
		Health = 0.f;
		const bool bHeadshot = Hit.Zone == ENSCHitZone::Head && !Hit.bExplosive;
		// Headshots, catastrophic overkill and a second "down" in the same round are instant eliminations.
		if (bHeadshot || Overkill >= NSCRules::CatastrophicOverkill || bRescuedThisRound)
		{
			Kill(Hit);
		}
		else
		{
			GoDown(Hit);
		}
		return Dealt;
	}

	// Non-lethal injuries
	const bool bLeg = Hit.Zone == ENSCHitZone::LeftLeg || Hit.Zone == ENSCHitZone::RightLeg;
	const bool bArm = Hit.Zone == ENSCHitZone::LeftArm || Hit.Zone == ENSCHitZone::RightArm;
	if (bLeg && Dmg >= 15.f) { bLegInjured = true; }
	if (bArm && Dmg >= 15.f) { bArmInjured = true; }
	if ((bLeg || bArm) && Dmg >= 25.f) { Bleed = ENSCBleed::Heavy; }
	else if (Dmg >= 20.f && Bleed == ENSCBleed::None) { Bleed = ENSCBleed::Light; }

	if (Me) { Me->OnInjuryChanged(); }
	return Dealt;
}

void UNSCHealthComponent::GoDown(const FNSCHitInfo& Hit)
{
	bDowned = true;
	DownedHealth = NSCRules::DownedHealth;
	BleedoutRemaining = NSCRules::BleedoutTime;
	ReviveProgress = 0.f;
	Bleed = ENSCBleed::None;
	if (ANSCCharacter* Me = GetNSCOwner())
	{
		Me->OnDownedStateChanged(true);
		OnDowned.Broadcast(Me, Hit);
		if (ANSCGameMode* GM = GetWorld()->GetAuthGameMode<ANSCGameMode>()) { GM->NotifyDowned(Me, Hit); }
	}
}

void UNSCHealthComponent::Kill(const FNSCHitInfo& Hit)
{
	if (bDead) { return; }
	bDead = true;
	const bool bWasDowned = bDowned;
	bDowned = false;
	Health = 0.f;
	BleedoutRemaining = 0.f;
	ReviveProgress = 0.f;
	Bleed = ENSCBleed::None;
	if (ANSCCharacter* Me = GetNSCOwner())
	{
		if (bWasDowned) { Me->OnDownedStateChanged(false); }
		Me->OnDeath();
		OnDied.Broadcast(Me, Hit);
		if (ANSCGameMode* GM = GetWorld()->GetAuthGameMode<ANSCGameMode>()) { GM->NotifyKilled(Me, Hit); }
	}
}

void UNSCHealthComponent::Revive(AController* Rescuer)
{
	if (!bDowned || bDead) { return; }
	bDowned = false;
	bRescuedThisRound = true;
	Health = NSCRules::ReviveHealth;
	BleedoutRemaining = 0.f;
	ReviveProgress = 0.f;
	Bleed = ENSCBleed::None;
	if (ANSCCharacter* Me = GetNSCOwner())
	{
		Me->OnDownedStateChanged(false);
		if (ANSCGameMode* GM = GetWorld()->GetAuthGameMode<ANSCGameMode>()) { GM->NotifyRevived(Me, Rescuer); }
	}
}

bool UNSCHealthComponent::ApplyBandage()
{
	if (Bleed == ENSCBleed::None || bDowned || bDead) { return false; }
	Bleed = (Bleed == ENSCBleed::Heavy) ? ENSCBleed::Light : ENSCBleed::None;
	if (ANSCCharacter* Me = GetNSCOwner()) { Me->OnInjuryChanged(); }
	return true;
}

bool UNSCHealthComponent::ApplyTourniquet()
{
	if (Bleed != ENSCBleed::Heavy || bDowned || bDead) { return false; }
	Bleed = ENSCBleed::None;
	if (ANSCCharacter* Me = GetNSCOwner()) { Me->OnInjuryChanged(); }
	return true;
}

void UNSCHealthComponent::ResetForRound(bool bKeepArmor)
{
	Health = NSCRules::MaxHealth;
	if (!bKeepArmor) { Armor = 0.f; bHelmet = false; }
	Bleed = ENSCBleed::None;
	bLegInjured = false;
	bArmInjured = false;
	bDowned = false;
	bDead = false;
	BleedoutRemaining = 0.f;
	ReviveProgress = 0.f;
	bRescuedThisRound = false;
	DownedHealth = NSCRules::DownedHealth;
	DamageByController.Reset();
}

void UNSCHealthComponent::SetArmor(float NewArmor, bool bNewHelmet)
{
	Armor = FMath::Clamp(NewArmor, 0.f, 100.f);
	bHelmet = bHelmet || bNewHelmet;
}

void UNSCHealthComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!GetOwner() || !GetOwner()->HasAuthority() || bDead) { return; }

	if (bDowned)
	{
		// Bleed-out pauses while a teammate is actively reviving.
		if (ReviveProgress <= 0.f)
		{
			BleedoutRemaining -= DeltaTime;
			if (BleedoutRemaining <= 0.f)
			{
				FNSCHitInfo Bleedout = LastHit;
				Bleedout.WeaponId = FName(TEXT("Bleedout"));
				Kill(Bleedout);
			}
		}
		return;
	}

	if (Bleed != ENSCBleed::None)
	{
		const float Dps = Bleed == ENSCBleed::Heavy ? NSCRules::HeavyBleedDps : NSCRules::LightBleedDps;
		Health -= Dps * DeltaTime;
		if (Health <= 0.f)
		{
			Health = 0.f;
			FNSCHitInfo BleedHit = LastHit;
			BleedHit.WeaponId = FName(TEXT("Bleeding"));
			if (bRescuedThisRound) { Kill(BleedHit); } else { GoDown(BleedHit); }
		}
	}
}

void UNSCHealthComponent::OnRep_Downed()
{
	if (ANSCCharacter* Me = GetNSCOwner()) { Me->OnDownedStateChanged(bDowned); }
}

void UNSCHealthComponent::OnRep_Dead()
{
	if (bDead)
	{
		if (ANSCCharacter* Me = GetNSCOwner()) { Me->OnDeath(); }
	}
}
