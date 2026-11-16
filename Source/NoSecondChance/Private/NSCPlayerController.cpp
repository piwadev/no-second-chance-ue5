#include "NSCPlayerController.h"
#include "NoSecondChance.h"
#include "NSCInputSet.h"
#include "NSCWidgets.h"
#include "NSCCharacter.h"
#include "NSCHealthComponent.h"
#include "NSCWeaponComponent.h"
#include "NSCGameMode.h"
#include "NSCGameState.h"
#include "NSCPlayerState.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"

ANSCPlayerController::ANSCPlayerController()
{
	bShowMouseCursor = false;
}

UNSCInputSet* ANSCPlayerController::GetInputSet()
{
	if (!InputSet) { InputSet = UNSCInputSet::Create(this); }
	return InputSet;
}

void ANSCPlayerController::AddMappingContext()
{
	if (bMappingAdded || !IsLocalController()) { return; }
	if (UEnhancedInputLocalPlayerSubsystem* Sub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (UNSCInputSet* In = GetInputSet())
		{
			Sub->AddMappingContext(In->Context, 0);
			bMappingAdded = true;
		}
	}
}

void ANSCPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalController()) { return; }
	AddMappingContext();

	HUDWidget = CreateWidget<UNSCHUDWidget>(this, UNSCHUDWidget::StaticClass());
	BuyWidget = CreateWidget<UNSCBuyMenuWidget>(this, UNSCBuyMenuWidget::StaticClass());
	ScoreWidget = CreateWidget<UNSCScoreboardWidget>(this, UNSCScoreboardWidget::StaticClass());
	PauseWidget = CreateWidget<UNSCPauseMenuWidget>(this, UNSCPauseMenuWidget::StaticClass());
	if (HUDWidget) { HUDWidget->AddToViewport(0); }
	if (ScoreWidget) { ScoreWidget->AddToViewport(5); }
	if (BuyWidget) { BuyWidget->AddToViewport(10); }
	if (PauseWidget) { PauseWidget->AddToViewport(20); }
	UpdateWidgetVisibility();
	ApplyInputMode();
}

void ANSCPlayerController::EndPlay(const EEndPlayReason::Type Reason)
{
	if (HUDWidget) { HUDWidget->RemoveFromParent(); }
	if (BuyWidget) { BuyWidget->RemoveFromParent(); }
	if (ScoreWidget) { ScoreWidget->RemoveFromParent(); }
	if (PauseWidget) { PauseWidget->RemoveFromParent(); }
	Super::EndPlay(Reason);
}

void ANSCPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent);
	UNSCInputSet* In = GetInputSet();
	if (!EIC || !In) { return; }
	EIC->BindAction(In->BuyMenu, ETriggerEvent::Started, this, &ANSCPlayerController::ToggleBuyMenu);
	EIC->BindAction(In->Scoreboard, ETriggerEvent::Started, this, &ANSCPlayerController::OnScoreboardStart);
	EIC->BindAction(In->Scoreboard, ETriggerEvent::Completed, this, &ANSCPlayerController::OnScoreboardStop);
	EIC->BindAction(In->Pause, ETriggerEvent::Started, this, &ANSCPlayerController::TogglePause);
	EIC->BindAction(In->Fire, ETriggerEvent::Started, this, &ANSCPlayerController::OnFirePressed);
}

void ANSCPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	DeathTime = -1.f;
	SpectateTarget = nullptr;
}

bool ANSCPlayerController::IsOwnPawnAlive() const
{
	const ANSCCharacter* C = Cast<ANSCCharacter>(GetPawn());
	return C && !C->GetHealth()->IsDead();
}

ANSCCharacter* ANSCPlayerController::GetSpectatedCharacter() const
{
	if (IsOwnPawnAlive()) { return Cast<ANSCCharacter>(GetPawn()); }
	return Cast<ANSCCharacter>(SpectateTarget);
}

void ANSCPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	if (!IsLocalController()) { return; }
	AddMappingContext();

	// Close the buy menu once buying is over
	if (bBuyMenuOpen)
	{
		const ANSCGameState* GS = GetWorld()->GetGameState<ANSCGameState>();
		const ENSCRoundPhase P = GS ? GS->GetPhase() : ENSCRoundPhase::Live;
		if (P != ENSCRoundPhase::Buy && P != ENSCRoundPhase::Preparation) { bBuyMenuOpen = false; UpdateWidgetVisibility(); ApplyInputMode(); }
	}

	// Spectate a living teammate after death
	if (!IsOwnPawnAlive())
	{
		const float Now = GetWorld()->GetTimeSeconds();
		if (DeathTime < 0.f) { DeathTime = Now; }
		const ANSCCharacter* T = Cast<ANSCCharacter>(SpectateTarget);
		const bool bTargetValid = T && !T->GetHealth()->IsDead();
		if (Now - DeathTime > 2.5f && !bTargetValid) { SpectateNext(); }
	}
	else
	{
		DeathTime = -1.f;
	}
	if (IsOwnPawnAlive() && SpectateTarget)
	{
		SpectateTarget = nullptr;
		SetViewTargetWithBlend(GetPawn(), 0.2f);
	}
}

void ANSCPlayerController::SpectateNext()
{
	const ANSCPlayerState* MyPS = GetPlayerState<ANSCPlayerState>();
	const ENSCTeam MyTeam = MyPS ? MyPS->GetTeam() : ENSCTeam::None;
	TArray<ANSCCharacter*> Candidates;
	for (TActorIterator<ANSCCharacter> It(GetWorld()); It; ++It)
	{
		if (*It != GetPawn() && !It->GetHealth()->IsDead() && (MyTeam == ENSCTeam::None || It->GetTeam() == MyTeam)) { Candidates.Add(*It); }
	}
	if (Candidates.Num() == 0) { return; }
	int32 Idx = Candidates.IndexOfByKey(Cast<ANSCCharacter>(SpectateTarget));
	Idx = (Idx + 1) % Candidates.Num();
	SpectateTarget = Candidates[Idx];
	SetViewTargetWithBlend(SpectateTarget, 0.25f);
}

void ANSCPlayerController::OnFirePressed()
{
	if (!IsOwnPawnAlive() && !IsMenuOpen()) { SpectateNext(); }
}

void ANSCPlayerController::ToggleBuyMenu()
{
	if (bPauseOpen) { return; }
	const ANSCGameState* GS = GetWorld()->GetGameState<ANSCGameState>();
	const ENSCRoundPhase P = GS ? GS->GetPhase() : ENSCRoundPhase::Live;
	const bool bCanBuy = (P == ENSCRoundPhase::Buy || P == ENSCRoundPhase::Preparation) && IsOwnPawnAlive();
	bBuyMenuOpen = !bBuyMenuOpen && bCanBuy;
	UpdateWidgetVisibility();
	ApplyInputMode();
}

void ANSCPlayerController::TogglePause()
{
	if (bBuyMenuOpen) { CloseMenus(); return; }
	bPauseOpen = !bPauseOpen;
	UpdateWidgetVisibility();
	ApplyInputMode();
}

void ANSCPlayerController::CloseMenus()
{
	bBuyMenuOpen = false;
	bPauseOpen = false;
	UpdateWidgetVisibility();
	ApplyInputMode();
}

void ANSCPlayerController::LeaveMatch()
{
	UGameplayStatics::OpenLevel(this, FName(TEXT("/Game/NSC/Maps/L_MainMenu")));
}

void ANSCPlayerController::UpdateWidgetVisibility()
{
	if (BuyWidget) { BuyWidget->SetVisibility(bBuyMenuOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed); }
	if (PauseWidget) { PauseWidget->SetVisibility(bPauseOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed); }
	if (ScoreWidget) { ScoreWidget->SetVisibility(bScoreboardShown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed); }
}

void ANSCPlayerController::ApplyInputMode()
{
	if (IsMenuOpen())
	{
		FInputModeGameAndUI Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways);
		Mode.SetHideCursorDuringCapture(false);
		SetInputMode(Mode);
		bShowMouseCursor = true;
		if (ANSCCharacter* C = Cast<ANSCCharacter>(GetPawn())) { C->GetWeapons()->StopFire(); }
	}
	else
	{
		SetInputMode(FInputModeGameOnly());
		bShowMouseCursor = false;
	}
}

void ANSCPlayerController::ServerBuyWeapon_Implementation(FName WeaponId)
{
	if (ANSCGameMode* GM = GetWorld()->GetAuthGameMode<ANSCGameMode>()) { GM->TryBuyWeapon(this, WeaponId); }
}

void ANSCPlayerController::ServerBuyEquipment_Implementation(ENSCEquipment Item)
{
	if (ANSCGameMode* GM = GetWorld()->GetAuthGameMode<ANSCGameMode>()) { GM->TryBuyEquipment(this, Item); }
}
