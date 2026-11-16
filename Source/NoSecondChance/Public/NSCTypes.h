// Shared enums, structs and tuning constants for NO SECOND CHANCE.
#pragma once

#include "CoreMinimal.h"
#include "NSCTypes.generated.h"

UENUM(BlueprintType)
enum class ENSCTeam : uint8
{
	None,
	Attackers,
	Defenders
};

UENUM(BlueprintType)
enum class ENSCHitZone : uint8
{
	None,
	Head,
	Chest,
	Stomach,
	LeftArm,
	RightArm,
	LeftLeg,
	RightLeg
};

UENUM(BlueprintType)
enum class ENSCInjuryState : uint8
{
	Healthy,
	Wounded,
	Critical,
	Downed,
	Dead
};

UENUM(BlueprintType)
enum class ENSCBleed : uint8
{
	None,
	Light,   // stopped by a bandage
	Heavy    // limb arterial bleed: needs a tourniquet (a bandage only slows it)
};

UENUM(BlueprintType)
enum class ENSCRoundPhase : uint8
{
	WaitingForPlayers,
	Buy,
	Preparation,
	Live,
	BombPlanted,
	RoundEnd,
	HalfTime,
	MatchEnd
};

UENUM(BlueprintType)
enum class ENSCRoundEndReason : uint8
{
	None,
	AttackersEliminated,
	DefendersEliminated,
	BombDetonated,
	BombDefused,
	TimeExpired
};

UENUM(BlueprintType)
enum class ENSCWeaponSlot : uint8
{
	Primary,
	Secondary,
	Grenade
};

UENUM(BlueprintType)
enum class ENSCWeaponClass : uint8
{
	Rifle,
	SMG,
	Pistol,
	Shotgun,
	Sniper,
	LMG
};

UENUM(BlueprintType)
enum class ENSCGrenadeType : uint8
{
	None,
	Smoke,
	Flash,
	Frag
};

UENUM(BlueprintType)
enum class ENSCStance : uint8
{
	Standing,
	Crouching,
	Downed
};

UENUM(BlueprintType)
enum class ENSCActionType : uint8
{
	None,
	Reviving,
	Bandaging,
	Tourniquet,
	Planting,
	Defusing,
	Dragging
};

// Economy / match constants from the design document.
namespace NSCRules
{
	static constexpr int32 StartMoney = 800;
	static constexpr int32 MaxMoney = 16000;
	static constexpr int32 WinReward = 3000;
	static constexpr int32 LossBase = 1400;
	static constexpr int32 LossStep = 500;
	static constexpr int32 LossMax = 3400;
	static constexpr int32 PlantBonus = 300;       // planter
	static constexpr int32 PlantTeamBonusOnLoss = 800;
	static constexpr int32 DefuseBonus = 300;
	static constexpr int32 RescueReward = 200;
	static constexpr int32 OvertimeMoney = 10000;

	static constexpr float BuyTime = 20.f;
	static constexpr float PrepTime = 15.f;
	static constexpr float RoundTime = 120.f;
	static constexpr float BombTime = 40.f;
	static constexpr float PlantTime = 4.f;
	static constexpr float DefuseTime = 7.f;
	static constexpr float DefuseTimeKit = 4.f;
	static constexpr float RoundEndTime = 7.f;
	static constexpr float HalfTimeDuration = 12.f;
	static constexpr float MatchEndTime = 15.f;

	static constexpr float MaxHealth = 100.f;
	static constexpr float BleedoutTime = 20.f;
	static constexpr float DownedHealth = 45.f;       // damage a downed player absorbs before being finished
	static constexpr float CatastrophicOverkill = 55.f;// overkill beyond 0 HP in one hit = instant elimination
	static constexpr float ReviveTime = 6.5f;
	static constexpr float ReviveHealth = 35.f;
	static constexpr float BandageTime = 2.5f;
	static constexpr float TourniquetTime = 2.0f;
	static constexpr float LightBleedDps = 1.0f;
	static constexpr float HeavyBleedDps = 3.0f;
	static constexpr float InteractRange = 160.f;     // cm

	static constexpr int32 RegulationRounds = 24;
	static constexpr int32 RoundsToWin = 13;
	static constexpr int32 OvertimeHalf = 3;           // MR6 overtime: 3 rounds per side
}

USTRUCT(BlueprintType)
struct FNSCKillFeedEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FString Killer;
	UPROPERTY(BlueprintReadOnly) FString Victim;
	UPROPERTY(BlueprintReadOnly) FString Weapon;
	UPROPERTY(BlueprintReadOnly) ENSCTeam KillerTeam = ENSCTeam::None;
	UPROPERTY(BlueprintReadOnly) ENSCTeam VictimTeam = ENSCTeam::None;
	UPROPERTY(BlueprintReadOnly) bool bHeadshot = false;
	UPROPERTY(BlueprintReadOnly) bool bDowned = false;   // "downed" entry rather than a kill
	UPROPERTY(BlueprintReadOnly) bool bRescue = false;   // "revived" entry
	UPROPERTY(BlueprintReadOnly) float Time = 0.f;
};

USTRUCT(BlueprintType)
struct FNSCRoundSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) int32 Round = 0;
	UPROPERTY(BlueprintReadOnly) ENSCTeam Winner = ENSCTeam::None;
	UPROPERTY(BlueprintReadOnly) ENSCRoundEndReason Reason = ENSCRoundEndReason::None;
	UPROPERTY(BlueprintReadOnly) FString MVPName;
	UPROPERTY(BlueprintReadOnly) int32 Rescues = 0;
};

namespace NSCText
{
	inline FString TeamName(ENSCTeam T)
	{
		switch (T)
		{
		case ENSCTeam::Attackers: return TEXT("ATTACKERS");
		case ENSCTeam::Defenders: return TEXT("DEFENDERS");
		default: return TEXT("SPECTATORS");
		}
	}
	inline FString ReasonText(ENSCRoundEndReason R)
	{
		switch (R)
		{
		case ENSCRoundEndReason::AttackersEliminated: return TEXT("Attackers eliminated");
		case ENSCRoundEndReason::DefendersEliminated: return TEXT("Defenders eliminated");
		case ENSCRoundEndReason::BombDetonated: return TEXT("Charge detonated");
		case ENSCRoundEndReason::BombDefused: return TEXT("Charge defused");
		case ENSCRoundEndReason::TimeExpired: return TEXT("Time expired");
		default: return TEXT("");
		}
	}
	inline ENSCTeam Opposite(ENSCTeam T)
	{
		return T == ENSCTeam::Attackers ? ENSCTeam::Defenders : (T == ENSCTeam::Defenders ? ENSCTeam::Attackers : ENSCTeam::None);
	}
}

// Palette (#171A1D #252B32 #D5A64A #E7E9EB, red injury, blue/orange teams)
namespace NSCColors
{
	inline FLinearColor Hex(const TCHAR* H) { return FLinearColor(FColor::FromHex(H)); }
	inline FLinearColor Graphite() { return Hex(TEXT("171A1DFF")); }
	inline FLinearColor Slate() { return Hex(TEXT("252B32FF")); }
	inline FLinearColor Gold() { return Hex(TEXT("D5A64AFF")); }
	inline FLinearColor Light() { return Hex(TEXT("E7E9EBFF")); }
	inline FLinearColor Injury() { return Hex(TEXT("D8433BFF")); }
	inline FLinearColor Defenders() { return Hex(TEXT("3F8FE0FF")); }
	inline FLinearColor Attackers() { return Hex(TEXT("E58A2EFF")); }
	inline FLinearColor Team(ENSCTeam T) { return T == ENSCTeam::Attackers ? Attackers() : (T == ENSCTeam::Defenders ? Defenders() : Light()); }
}
