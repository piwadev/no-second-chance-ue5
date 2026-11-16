#include "NSCWeaponData.h"
#include "Engine/StaticMesh.h"
#include "Sound/SoundBase.h"
#include "UObject/SoftObjectPath.h"

static const TCHAR* GWeaponTablePath = TEXT("/Game/NSC/Data/DT_Weapons.DT_Weapons");

TArray<FVector2D> UNSCWeaponLibrary::MakePattern(int32 Shots, float PitchMax, float K, const TArray<float>& YawKeys)
{
	TArray<FVector2D> Out;
	Out.Reserve(Shots);
	for (int32 i = 0; i < Shots; ++i)
	{
		const float Pitch = PitchMax * (1.f - FMath::Exp(-static_cast<float>(i) / FMath::Max(K, 0.01f)));
		float Yaw = 0.f;
		if (YawKeys.Num() > 0)
		{
			// Keys are evenly spaced over the magazine; linear interpolation in between.
			const float T = (Shots > 1) ? static_cast<float>(i) / static_cast<float>(Shots - 1) * static_cast<float>(YawKeys.Num() - 1) : 0.f;
			const int32 A = FMath::Clamp(FMath::FloorToInt(T), 0, YawKeys.Num() - 1);
			const int32 B = FMath::Min(A + 1, YawKeys.Num() - 1);
			Yaw = FMath::Lerp(YawKeys[A], YawKeys[B], T - static_cast<float>(A));
		}
		Out.Add(FVector2D(Yaw, Pitch));
	}
	return Out;
}

namespace
{
	FNSCWeaponStats W(const TCHAR* Id, const TCHAR* Name, const TCHAR* Short, ENSCWeaponClass Class, ENSCWeaponSlot Slot, int32 Price)
	{
		FNSCWeaponStats S;
		S.Id = FName(Id);
		S.DisplayName = Name;
		S.ShortName = Short;
		S.WeaponClass = Class;
		S.Slot = Slot;
		S.Price = Price;
		const FString MeshName = FString::Printf(TEXT("/Game/NSC/Weapons/SM_%s.SM_%s"), Id, Id);
		S.Mesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(MeshName));
		const FString Fire = FString::Printf(TEXT("/Game/NSC/Audio/Weapons/SW_%s_Fire.SW_%s_Fire"), Id, Id);
		const FString Reload = FString::Printf(TEXT("/Game/NSC/Audio/Weapons/SW_%s_Reload.SW_%s_Reload"), Id, Id);
		S.FireSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(Fire));
		S.ReloadSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(Reload));
		return S;
	}
}

TArray<FNSCWeaponStats> UNSCWeaponLibrary::BuildBuiltinWeapons()
{
	TArray<FNSCWeaponStats> L;

	// ---------------------------------------------------------------- rifles
	{
		FNSCWeaponStats S = W(TEXT("AK"), TEXT("AK-74 RIFLE"), TEXT("AK"), ENSCWeaponClass::Rifle, ENSCWeaponSlot::Primary, 2700);
		S.TeamOnly = ENSCTeam::Attackers; S.Damage = 36; S.ArmorPen = 0.775f; S.FireInterval = 0.1f; S.MagSize = 30; S.MagCount = 4;
		S.ReloadTime = 2.5f; S.EmptyReloadTime = 2.9f; S.MoveSpeedMultiplier = 0.86f; S.BaseSpread = 0.0016f; S.MoveSpread = 0.09f; S.SprayRecovery = 11.f;
		S.SprayPattern = MakePattern(30, 6.6f, 5.2f, { 0.f, 0.2f, -0.1f, -1.4f, -2.6f, -3.1f, -1.2f, 0.8f, 1.9f, 0.6f, -0.4f });
		L.Add(S);
	}
	{
		FNSCWeaponStats S = W(TEXT("M4"), TEXT("M4 CARBINE"), TEXT("M4"), ENSCWeaponClass::Rifle, ENSCWeaponSlot::Primary, 3100);
		S.TeamOnly = ENSCTeam::Defenders; S.Damage = 33; S.ArmorPen = 0.70f; S.FireInterval = 0.09f; S.MagSize = 30; S.MagCount = 4;
		S.ReloadTime = 3.1f; S.EmptyReloadTime = 3.4f; S.MoveSpeedMultiplier = 0.9f; S.BaseSpread = 0.0012f; S.MoveSpread = 0.08f; S.SprayRecovery = 12.f;
		S.SprayPattern = MakePattern(30, 5.0f, 5.8f, { 0.f, -0.1f, 0.4f, 1.2f, 1.8f, 0.9f, -0.6f, -1.6f, -2.2f, -1.4f, -0.8f });
		L.Add(S);
	}
	{
		FNSCWeaponStats S = W(TEXT("HK416"), TEXT("HK-416 RIFLE"), TEXT("416"), ENSCWeaponClass::Rifle, ENSCWeaponSlot::Primary, 3000);
		S.Damage = 34; S.ArmorPen = 0.72f; S.FireInterval = 0.092f; S.MagSize = 30; S.MagCount = 4;
		S.ReloadTime = 2.8f; S.EmptyReloadTime = 3.2f; S.MoveSpeedMultiplier = 0.88f; S.BaseSpread = 0.0013f; S.MoveSpread = 0.085f; S.SprayRecovery = 11.5f;
		S.SprayPattern = MakePattern(30, 5.4f, 5.5f, { 0.f, 0.15f, -0.5f, -1.5f, -1.0f, 0.6f, 1.6f, 1.1f, -0.2f, -1.0f, -0.4f });
		L.Add(S);
	}
	{
		FNSCWeaponStats S = W(TEXT("DMR"), TEXT("MK-20 MARKSMAN"), TEXT("DMR"), ENSCWeaponClass::Sniper, ENSCWeaponSlot::Primary, 4200);
		S.Damage = 70; S.ArmorPen = 0.85f; S.HeadMultiplier = 3.f; S.FireInterval = 0.25f; S.bAutomatic = false; S.MagSize = 20; S.MagCount = 3;
		S.ReloadTime = 3.0f; S.EmptyReloadTime = 3.4f; S.MoveSpeedMultiplier = 0.8f; S.BaseSpread = 0.0006f; S.MoveSpread = 0.12f; S.SprayRecovery = 5.f;
		S.bScoped = true; S.AimFOV = 30.f; S.UnscopedSpread = 0.03f;
		S.SprayPattern = MakePattern(20, 9.f, 2.5f, { 0.f, 0.3f, -0.4f, 0.5f, -0.3f });
		L.Add(S);
	}
	{
		FNSCWeaponStats S = W(TEXT("Bolt"), TEXT("BOLT-ACTION .338"), TEXT("BOLT"), ENSCWeaponClass::Sniper, ENSCWeaponSlot::Primary, 4750);
		S.Damage = 115; S.ArmorPen = 0.97f; S.HeadMultiplier = 4.f; S.RangeModifier = 0.99f; S.FireInterval = 1.45f; S.bAutomatic = false; S.MagSize = 5; S.MagCount = 4;
		S.ReloadTime = 3.6f; S.EmptyReloadTime = 3.6f; S.MoveSpeedMultiplier = 0.78f; S.BaseSpread = 0.0002f; S.MoveSpread = 0.2f; S.SprayRecovery = 2.f;
		S.bScoped = true; S.AimFOV = 18.f; S.UnscopedSpread = 0.06f; S.KillReward = 100;
		S.SprayPattern = MakePattern(5, 4.f, 1.f, { 0.f });
		L.Add(S);
	}
	{
		FNSCWeaponStats S = W(TEXT("LMG"), TEXT("M240 MACHINE GUN"), TEXT("LMG"), ENSCWeaponClass::LMG, ENSCWeaponSlot::Primary, 5200);
		S.Damage = 35; S.ArmorPen = 0.8f; S.FireInterval = 0.08f; S.MagSize = 100; S.MagCount = 2;
		S.ReloadTime = 5.5f; S.EmptyReloadTime = 6.0f; S.MoveSpeedMultiplier = 0.72f; S.BaseSpread = 0.0035f; S.MoveSpread = 0.12f; S.SprayRecovery = 9.f;
		S.SprayPattern = MakePattern(100, 7.5f, 9.f, { 0.f, -0.6f, -1.8f, -0.5f, 1.4f, 2.2f, 0.4f, -1.6f, -2.4f, -0.6f, 1.2f, 2.0f, 0.2f });
		L.Add(S);
	}

	// ---------------------------------------------------------------- SMGs (higher kill reward like CS)
	{
		FNSCWeaponStats S = W(TEXT("MP5"), TEXT("MP5 SUBMACHINE GUN"), TEXT("MP5"), ENSCWeaponClass::SMG, ENSCWeaponSlot::Primary, 1500);
		S.Damage = 27; S.ArmorPen = 0.62f; S.RangeModifier = 0.85f; S.FireInterval = 0.075f; S.MagSize = 30; S.MagCount = 4; S.KillReward = 600;
		S.ReloadTime = 2.4f; S.EmptyReloadTime = 2.8f; S.MoveSpeedMultiplier = 0.95f; S.BaseSpread = 0.0035f; S.MoveSpread = 0.04f; S.SprayRecovery = 14.f;
		S.SprayPattern = MakePattern(30, 3.8f, 6.f, { 0.f, 0.3f, -0.4f, 0.6f, -0.6f, 0.4f, -0.2f });
		L.Add(S);
	}
	{
		FNSCWeaponStats S = W(TEXT("MP7"), TEXT("MP7 PDW"), TEXT("MP7"), ENSCWeaponClass::SMG, ENSCWeaponSlot::Primary, 1600);
		S.Damage = 26; S.ArmorPen = 0.75f; S.RangeModifier = 0.86f; S.FireInterval = 0.07f; S.MagSize = 30; S.MagCount = 4; S.KillReward = 600;
		S.ReloadTime = 2.2f; S.EmptyReloadTime = 2.6f; S.MoveSpeedMultiplier = 0.96f; S.BaseSpread = 0.0032f; S.MoveSpread = 0.04f; S.SprayRecovery = 14.f;
		S.SprayPattern = MakePattern(30, 4.2f, 5.f, { 0.f, -0.3f, 0.5f, -0.5f, 0.7f, -0.3f, 0.2f });
		L.Add(S);
	}
	{
		FNSCWeaponStats S = W(TEXT("UMP"), TEXT("UMP-45"), TEXT("UMP"), ENSCWeaponClass::SMG, ENSCWeaponSlot::Primary, 1200);
		S.Damage = 35; S.ArmorPen = 0.65f; S.RangeModifier = 0.75f; S.FireInterval = 0.09f; S.MagSize = 25; S.MagCount = 4; S.KillReward = 600;
		S.ReloadTime = 3.0f; S.EmptyReloadTime = 3.4f; S.MoveSpeedMultiplier = 0.94f; S.BaseSpread = 0.004f; S.MoveSpread = 0.045f; S.SprayRecovery = 12.f;
		S.SprayPattern = MakePattern(25, 4.8f, 5.f, { 0.f, 0.4f, 0.9f, 0.2f, -0.7f, -0.3f });
		L.Add(S);
	}

	// ---------------------------------------------------------------- shotguns
	{
		FNSCWeaponStats S = W(TEXT("Pump"), TEXT("R870 PUMP SHOTGUN"), TEXT("PUMP"), ENSCWeaponClass::Shotgun, ENSCWeaponSlot::Primary, 1100);
		S.Damage = 26; S.Pellets = 9; S.PelletSpread = 0.045f; S.ArmorPen = 0.5f; S.RangeModifier = 0.45f; S.MaxRange = 3000.f;
		S.FireInterval = 0.88f; S.bAutomatic = false; S.MagSize = 8; S.MagCount = 4; S.KillReward = 900;
		S.ReloadTime = 3.2f; S.EmptyReloadTime = 3.6f; S.MoveSpeedMultiplier = 0.9f; S.BaseSpread = 0.01f; S.MoveSpread = 0.02f; S.SprayRecovery = 3.f;
		S.SprayPattern = MakePattern(8, 4.f, 1.f, { 0.f });
		L.Add(S);
	}
	{
		FNSCWeaponStats S = W(TEXT("AutoShotgun"), TEXT("M4 SEMI-AUTO SHOTGUN"), TEXT("AUTO SG"), ENSCWeaponClass::Shotgun, ENSCWeaponSlot::Primary, 1900);
		S.Damage = 20; S.Pellets = 8; S.PelletSpread = 0.05f; S.ArmorPen = 0.5f; S.RangeModifier = 0.45f; S.MaxRange = 3000.f;
		S.FireInterval = 0.25f; S.bAutomatic = false; S.MagSize = 7; S.MagCount = 4; S.KillReward = 900;
		S.ReloadTime = 3.4f; S.EmptyReloadTime = 3.8f; S.MoveSpeedMultiplier = 0.88f; S.BaseSpread = 0.012f; S.MoveSpread = 0.02f; S.SprayRecovery = 4.f;
		S.SprayPattern = MakePattern(7, 6.f, 2.f, { 0.f, 0.4f, -0.4f });
		L.Add(S);
	}

	// ---------------------------------------------------------------- pistols
	{
		FNSCWeaponStats S = W(TEXT("Glock"), TEXT("G17 PISTOL"), TEXT("G17"), ENSCWeaponClass::Pistol, ENSCWeaponSlot::Secondary, 200);
		S.TeamOnly = ENSCTeam::Attackers; S.Damage = 28; S.ArmorPen = 0.47f; S.RangeModifier = 0.9f; S.FireInterval = 0.15f; S.bAutomatic = false; S.MagSize = 17; S.MagCount = 4;
		S.ReloadTime = 2.2f; S.EmptyReloadTime = 2.4f; S.DrawTime = 0.6f; S.MoveSpeedMultiplier = 1.f; S.BaseSpread = 0.0025f; S.MoveSpread = 0.045f; S.SprayRecovery = 9.f;
		S.SprayPattern = MakePattern(17, 4.2f, 4.f, { 0.f, 0.1f, -0.12f, 0.18f, -0.2f, 0.25f, -0.15f, 0.3f });
		L.Add(S);
	}
	{
		FNSCWeaponStats S = W(TEXT("USP"), TEXT("USP TACTICAL"), TEXT("USP"), ENSCWeaponClass::Pistol, ENSCWeaponSlot::Secondary, 200);
		S.TeamOnly = ENSCTeam::Defenders; S.Damage = 35; S.ArmorPen = 0.5f; S.RangeModifier = 0.91f; S.FireInterval = 0.17f; S.bAutomatic = false; S.MagSize = 12; S.MagCount = 4;
		S.ReloadTime = 2.2f; S.EmptyReloadTime = 2.4f; S.DrawTime = 0.6f; S.MoveSpeedMultiplier = 1.f; S.BaseSpread = 0.0022f; S.MoveSpread = 0.045f; S.SprayRecovery = 8.f;
		S.SprayPattern = MakePattern(12, 4.6f, 3.5f, { 0.f, -0.12f, 0.15f, -0.2f, 0.22f, -0.2f });
		L.Add(S);
	}
	{
		FNSCWeaponStats S = W(TEXT("P226"), TEXT("P226 PISTOL"), TEXT("P226"), ENSCWeaponClass::Pistol, ENSCWeaponSlot::Secondary, 300);
		S.Damage = 38; S.ArmorPen = 0.6f; S.RangeModifier = 0.8f; S.FireInterval = 0.15f; S.bAutomatic = false; S.MagSize = 13; S.MagCount = 4;
		S.ReloadTime = 2.2f; S.EmptyReloadTime = 2.5f; S.DrawTime = 0.65f; S.MoveSpeedMultiplier = 1.f; S.BaseSpread = 0.0028f; S.MoveSpread = 0.05f; S.SprayRecovery = 8.f;
		S.SprayPattern = MakePattern(13, 5.f, 3.5f, { 0.f, 0.2f, -0.2f, 0.25f, -0.3f });
		L.Add(S);
	}
	{
		FNSCWeaponStats S = W(TEXT("Deagle"), TEXT("R50 HEAVY PISTOL"), TEXT("R50"), ENSCWeaponClass::Pistol, ENSCWeaponSlot::Secondary, 700);
		S.Damage = 53; S.ArmorPen = 0.93f; S.RangeModifier = 0.81f; S.FireInterval = 0.3f; S.bAutomatic = false; S.MagSize = 7; S.MagCount = 5;
		S.ReloadTime = 2.4f; S.EmptyReloadTime = 2.6f; S.DrawTime = 0.7f; S.MoveSpeedMultiplier = 0.95f; S.BaseSpread = 0.002f; S.MoveSpread = 0.11f; S.SprayRecovery = 4.f;
		S.SprayPattern = MakePattern(7, 6.5f, 2.2f, { 0.f, 0.25f, -0.3f, 0.45f, -0.4f, 0.5f, -0.45f });
		L.Add(S);
	}
	return L;
}

const TArray<FNSCWeaponStats>& UNSCWeaponLibrary::GetAllWeapons()
{
	static TArray<FNSCWeaponStats> Cached;
	static bool bBuilt = false;
	if (!bBuilt)
	{
		bBuilt = true;
		Cached = BuildBuiltinWeapons();
		// Rows in the optional DataTable override / extend the built-in table.
		if (UDataTable* Table = LoadObject<UDataTable>(nullptr, GWeaponTablePath, nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			if (Table->GetRowStruct() == FNSCWeaponStats::StaticStruct())
			{
				for (const TPair<FName, uint8*>& Row : Table->GetRowMap())
				{
					const FNSCWeaponStats* R = reinterpret_cast<const FNSCWeaponStats*>(Row.Value);
					if (!R) { continue; }
					FNSCWeaponStats Copy = *R;
					if (Copy.Id == NAME_None) { Copy.Id = Row.Key; }
					if (Copy.SprayPattern.Num() == 0) { Copy.SprayPattern = MakePattern(FMath::Max(1, Copy.MagSize), 5.f, 5.f, { 0.f }); }
					const int32 Existing = Cached.IndexOfByPredicate([&](const FNSCWeaponStats& X) { return X.Id == Copy.Id; });
					if (Existing != INDEX_NONE) { Cached[Existing] = Copy; } else { Cached.Add(Copy); }
				}
			}
		}
	}
	return Cached;
}

bool UNSCWeaponLibrary::GetWeaponStats(FName Id, FNSCWeaponStats& OutStats)
{
	for (const FNSCWeaponStats& S : GetAllWeapons())
	{
		if (S.Id == Id) { OutStats = S; return true; }
	}
	return false;
}

const TArray<FNSCEquipmentInfo>& UNSCWeaponLibrary::GetAllEquipment()
{
	static TArray<FNSCEquipmentInfo> L;
	if (L.Num() == 0)
	{
		auto Add = [](ENSCEquipment T, const TCHAR* N, int32 P, int32 Max, ENSCTeam Team = ENSCTeam::None)
		{
			FNSCEquipmentInfo I; I.Type = T; I.Name = N; I.Price = P; I.MaxCarry = Max; I.TeamOnly = Team; L.Add(I);
		};
		Add(ENSCEquipment::Armor, TEXT("Body armor"), 650, 1);
		Add(ENSCEquipment::ArmorHelmet, TEXT("Armor + helmet"), 1000, 1);
		Add(ENSCEquipment::Medkit, TEXT("Medical kit"), 700, 1);
		Add(ENSCEquipment::Bandage, TEXT("Bandage"), 150, 2);
		Add(ENSCEquipment::Tourniquet, TEXT("Tourniquet"), 250, 1);
		Add(ENSCEquipment::Smoke, TEXT("Smoke grenade"), 300, 1);
		Add(ENSCEquipment::Flash, TEXT("Flashbang"), 200, 2);
		Add(ENSCEquipment::Frag, TEXT("Frag grenade"), 350, 1);
		Add(ENSCEquipment::DefuseKit, TEXT("Defuse kit"), 400, 1, ENSCTeam::Defenders);
		Add(ENSCEquipment::Resupply, TEXT("Resupply magazines"), 200, 1);
	}
	return L;
}

const FNSCEquipmentInfo* UNSCWeaponLibrary::FindEquipment(ENSCEquipment Type)
{
	return GetAllEquipment().FindByPredicate([Type](const FNSCEquipmentInfo& I) { return I.Type == Type; });
}

FName UNSCWeaponLibrary::DefaultPistol(ENSCTeam Team)
{
	return Team == ENSCTeam::Defenders ? FName(TEXT("USP")) : FName(TEXT("Glock"));
}
