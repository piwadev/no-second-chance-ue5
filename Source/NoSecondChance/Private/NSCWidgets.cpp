#include "NSCWidgets.h"
#include "NoSecondChance.h"
#include "NSCPlayerController.h"
#include "NSCCharacter.h"
#include "NSCHealthComponent.h"
#include "NSCWeaponComponent.h"
#include "NSCGameState.h"
#include "NSCPlayerState.h"
#include "NSCGameInstance.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/EditableTextBox.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "Engine/Font.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GeneralProjectSettings.h"

// ============================================================================ helpers

namespace NSCUI
{
	FSlateFontInfo Font(int32 Size, bool bBold)
	{
		// Optional project font imported by the editor bootstrap (Rajdhani, OFL). Falls back to the engine font.
		static TWeakObjectPtr<UObject> Cached;
		static bool bTried = false;
		if (!bTried)
		{
			bTried = true;
			Cached = LoadObject<UObject>(nullptr, TEXT("/Game/NSC/UI/Fonts/F_NSC.F_NSC"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		}
		if (UObject* F = Cached.Get())
		{
			return FSlateFontInfo(F, Size, bBold ? FName(TEXT("Bold")) : FName(TEXT("Regular")));
		}
		return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size);
	}

	UTextBlock* Text(UWidgetTree* Tree, const FString& S, int32 Size, const FLinearColor& Color, bool bBold)
	{
		UTextBlock* T = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		T->SetText(FText::FromString(S));
		T->SetFont(Font(Size, bBold));
		T->SetColorAndOpacity(FSlateColor(Color));
		T->SetShadowOffset(FVector2D(1.f, 1.f));
		T->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.6f));
		return T;
	}

	UBorder* Panel(UWidgetTree* Tree, const FLinearColor& Fill, const FMargin& Padding)
	{
		UBorder* B = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
		B->SetBrushColor(Fill);
		B->SetPadding(Padding);
		return B;
	}

	UNSCButton* Button(UWidgetTree* Tree, const FString& Label, TFunction<void()> OnClick, int32 Size, const FLinearColor& Fill)
	{
		UNSCButton* B = Tree->ConstructWidget<UNSCButton>(UNSCButton::StaticClass());
		B->Setup(Tree, Label, Size, Fill);
		B->OnClickFn = MoveTemp(OnClick);
		return B;
	}

	FLinearColor WithAlpha(const FLinearColor& C, float A) { FLinearColor O = C; O.A = A; return O; }

	FString Money(int32 M) { return FString::Printf(TEXT("$%d"), M); }

	FString Clock(float Seconds)
	{
		const int32 S = FMath::CeilToInt(FMath::Max(0.f, Seconds));
		return FString::Printf(TEXT("%d:%02d"), S / 60, S % 60);
	}

	static FSlateBrush SolidBrush(const FLinearColor& C)
	{
		FSlateBrush B;
		B.DrawAs = ESlateBrushDrawType::Box;
		B.TintColor = FSlateColor(C);
		B.Margin = FMargin(0.f);
		return B;
	}

	static UCanvasPanelSlot* Place(UCanvasPanel* Canvas, UWidget* W, const FAnchors& Anchors, const FVector2D& Alignment, const FVector2D& Pos, bool bAutoSize = true, const FVector2D& Size = FVector2D::ZeroVector)
	{
		UCanvasPanelSlot* S = Canvas->AddChildToCanvas(W);
		S->SetAnchors(Anchors);
		S->SetAlignment(Alignment);
		S->SetPosition(Pos);
		S->SetAutoSize(bAutoSize);
		if (!bAutoSize) { S->SetSize(Size); }
		return S;
	}

	static void AddV(UVerticalBox* Box, UWidget* W, const FMargin& Pad = FMargin(0.f, 2.f), EHorizontalAlignment H = HAlign_Fill)
	{
		UVerticalBoxSlot* S = Box->AddChildToVerticalBox(W);
		S->SetPadding(Pad);
		S->SetHorizontalAlignment(H);
	}

	static void AddH(UHorizontalBox* Box, UWidget* W, const FMargin& Pad = FMargin(4.f, 0.f), bool bFill = false, EVerticalAlignment V = VAlign_Center)
	{
		UHorizontalBoxSlot* S = Box->AddChildToHorizontalBox(W);
		S->SetPadding(Pad);
		S->SetVerticalAlignment(V);
		if (bFill) { S->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); }
	}

	static UWidget* Fixed(UWidgetTree* Tree, UWidget* Inner, float Width)
	{
		USizeBox* SB = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		SB->SetWidthOverride(Width);
		SB->AddChild(Inner);
		return SB;
	}
}

// ============================================================================ button

void UNSCButton::Setup(UWidgetTree* Tree, const FString& Text, int32 FontSize, const FLinearColor& Fill)
{
	FButtonStyle Style = GetStyle();
	Style.SetNormal(NSCUI::SolidBrush(Fill));
	Style.SetHovered(NSCUI::SolidBrush(NSCColors::Slate()));
	Style.SetPressed(NSCUI::SolidBrush(NSCColors::Gold()));
	Style.SetDisabled(NSCUI::SolidBrush(NSCUI::WithAlpha(Fill, 0.35f)));
	Style.SetNormalPadding(FMargin(14.f, 8.f));
	Style.SetPressedPadding(FMargin(14.f, 9.f, 14.f, 7.f));
	SetStyle(Style);
	Label = NSCUI::Text(Tree, Text, FontSize, NSCColors::Light());
	AddChild(Label);
}

void UNSCButton::SetLabel(const FString& Text)
{
	if (Label) { Label->SetText(FText::FromString(Text)); }
}

TSharedRef<SWidget> UNSCButton::RebuildWidget()
{
	OnClicked.AddUniqueDynamic(this, &UNSCButton::HandleClicked);
	return Super::RebuildWidget();
}

void UNSCButton::HandleClicked()
{
	if (OnClickFn) { OnClickFn(); }
}

// ============================================================================ base

void UNSCWidgetBase::EnsureBuilt()
{
	if (bBuilt) { return; }
	if (!WidgetTree) { WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient); }
	bBuilt = true;
	BuildTree();
}

void UNSCWidgetBase::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	EnsureBuilt();
}

TSharedRef<SWidget> UNSCWidgetBase::RebuildWidget()
{
	EnsureBuilt();
	return Super::RebuildWidget();
}

ANSCPlayerController* UNSCWidgetBase::GetNSCPC() const
{
	return Cast<ANSCPlayerController>(GetOwningPlayer());
}

// ============================================================================ HUD

void UNSCHUDWidget::BuildTree()
{
	using namespace NSCUI;
	UWidgetTree* T = WidgetTree;
	Root = T->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	T->RootWidget = Root;
	SetVisibility(ESlateVisibility::HitTestInvisible);

	// full-screen overlays
	InjuryVignette = T->ConstructWidget<UImage>(UImage::StaticClass());
	InjuryVignette->SetColorAndOpacity(WithAlpha(NSCColors::Injury(), 0.f));
	Place(Root, InjuryVignette, FAnchors(0.f, 0.f, 1.f, 1.f), FVector2D::ZeroVector, FVector2D::ZeroVector, false);
	if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(InjuryVignette->Slot)) { S->SetOffsets(FMargin(0.f)); }

	// top centre: scores + timer
	{
		UHorizontalBox* Top = T->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		ScoreOwn = Text(T, TEXT("0"), 30, NSCColors::Defenders());
		ScoreEnemy = Text(T, TEXT("0"), 30, NSCColors::Attackers());
		UBorder* TimerBox = Panel(T, WithAlpha(NSCColors::Graphite(), 0.85f), FMargin(18.f, 4.f));
		UVerticalBox* TV = T->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		TimerText = Text(T, TEXT("0:00"), 28, NSCColors::Light());
		PhaseText = Text(T, TEXT(""), 12, NSCColors::Gold());
		AddV(TV, TimerText, FMargin(0.f), HAlign_Center);
		AddV(TV, PhaseText, FMargin(0.f), HAlign_Center);
		TimerBox->SetContent(TV);
		UBorder* A = Panel(T, WithAlpha(NSCColors::Graphite(), 0.85f), FMargin(16.f, 6.f)); A->SetContent(ScoreOwn);
		UBorder* B = Panel(T, WithAlpha(NSCColors::Graphite(), 0.85f), FMargin(16.f, 6.f)); B->SetContent(ScoreEnemy);
		AddH(Top, A, FMargin(2.f));
		AddH(Top, TimerBox, FMargin(2.f));
		AddH(Top, B, FMargin(2.f));
		Place(Root, Top, FAnchors(0.5f, 0.f), FVector2D(0.5f, 0.f), FVector2D(0.f, 16.f));
	}

	// top left: money + hint
	{
		UVerticalBox* V = T->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		MoneyText = Text(T, TEXT("$800"), 26, NSCColors::Gold());
		HintText = Text(T, TEXT(""), 13, NSCColors::Light(), false);
		AddV(V, MoneyText);
		AddV(V, HintText);
		Place(Root, V, FAnchors(0.f, 0.f), FVector2D::ZeroVector, FVector2D(28.f, 20.f));
	}

	// left: team list
	{
		TeamBox = T->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Place(Root, TeamBox, FAnchors(0.f, 0.3f), FVector2D::ZeroVector, FVector2D(28.f, 0.f));
	}

	// top right: kill feed
	{
		KillFeedBox = T->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Place(Root, KillFeedBox, FAnchors(1.f, 0.f), FVector2D(1.f, 0.f), FVector2D(-24.f, 20.f));
	}

	// bottom left: vitals
	{
		UBorder* P = Panel(T, WithAlpha(NSCColors::Graphite(), 0.8f), FMargin(16.f, 10.f));
		UVerticalBox* V = T->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		UHorizontalBox* Row = T->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		HealthText = Text(T, TEXT("100"), 40, NSCColors::Light());
		ArmorText = Text(T, TEXT(""), 16, NSCColors::Light(), false);
		AddH(Row, HealthText, FMargin(0.f, 0.f, 14.f, 0.f));
		AddH(Row, ArmorText);
		HealthBar = T->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
		HealthBar->SetPercent(1.f);
		HealthBar->SetFillColorAndOpacity(NSCColors::Light());
		StateText = Text(T, TEXT("HEALTHY"), 14, NSCColors::Light());
		InjuryText = Text(T, TEXT(""), 13, NSCColors::Injury(), false);
		MedText = Text(T, TEXT(""), 13, NSCColors::Light(), false);
		AddV(V, Row);
		AddV(V, Fixed(T, HealthBar, 260.f));
		AddV(V, StateText);
		AddV(V, InjuryText);
		AddV(V, MedText);
		P->SetContent(V);
		Place(Root, P, FAnchors(0.f, 1.f), FVector2D(0.f, 1.f), FVector2D(24.f, -24.f));
	}

	// bottom right: weapon
	{
		UBorder* P = Panel(T, WithAlpha(NSCColors::Graphite(), 0.8f), FMargin(16.f, 10.f));
		UVerticalBox* V = T->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		WeaponName = Text(T, TEXT(""), 15, NSCColors::Gold());
		AmmoText = Text(T, TEXT(""), 40, NSCColors::Light());
		MagsText = Text(T, TEXT(""), 14, NSCColors::Light(), false);
		GearText = Text(T, TEXT(""), 13, NSCColors::Light(), false);
		AddV(V, WeaponName, FMargin(0.f, 2.f), HAlign_Right);
		AddV(V, AmmoText, FMargin(0.f, 0.f), HAlign_Right);
		AddV(V, MagsText, FMargin(0.f, 2.f), HAlign_Right);
		AddV(V, GearText, FMargin(0.f, 2.f), HAlign_Right);
		P->SetContent(V);
		Place(Root, P, FAnchors(1.f, 1.f), FVector2D(1.f, 1.f), FVector2D(-24.f, -24.f));
	}

	// crosshair: 4 lines + dot
	for (int32 i = 0; i < 5; ++i)
	{
		UImage* I = T->ConstructWidget<UImage>(UImage::StaticClass());
		I->SetColorAndOpacity(FLinearColor(0.9f, 0.95f, 0.9f, 0.9f));
		const bool bVertical = i < 2;
		const FVector2D Size = i == 4 ? FVector2D(2.f, 2.f) : (bVertical ? FVector2D(2.f, 8.f) : FVector2D(8.f, 2.f));
		Place(Root, I, FAnchors(0.5f, 0.5f), FVector2D(0.5f, 0.5f), FVector2D::ZeroVector, false, Size);
		Cross.Add(I);
	}

	// centre: big message, prompt, progress
	{
		UVerticalBox* V = T->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		BigText = Text(T, TEXT(""), 40, NSCColors::Light());
		SubText = Text(T, TEXT(""), 18, NSCColors::Light(), false);
		BigText->SetJustification(ETextJustify::Center);
		SubText->SetJustification(ETextJustify::Center);
		AddV(V, BigText, FMargin(0.f), HAlign_Center);
		AddV(V, SubText, FMargin(0.f, 6.f), HAlign_Center);
		Place(Root, V, FAnchors(0.5f, 0.28f), FVector2D(0.5f, 0.5f), FVector2D::ZeroVector);

		UVerticalBox* PV = T->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		PromptText = Text(T, TEXT(""), 16, NSCColors::Light());
		ActionBar = T->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
		ActionBar->SetFillColorAndOpacity(NSCColors::Gold());
		AddV(PV, PromptText, FMargin(0.f, 2.f), HAlign_Center);
		AddV(PV, Fixed(T, ActionBar, 280.f), FMargin(0.f, 4.f), HAlign_Center);
		Place(Root, PV, FAnchors(0.5f, 0.68f), FVector2D(0.5f, 0.5f), FVector2D::ZeroVector);

		SpectateText = Text(T, TEXT(""), 16, NSCColors::Light());
		Place(Root, SpectateText, FAnchors(0.5f, 1.f), FVector2D(0.5f, 1.f), FVector2D(0.f, -140.f));
	}

	FlashOverlay = T->ConstructWidget<UImage>(UImage::StaticClass());
	FlashOverlay->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.f));
	Place(Root, FlashOverlay, FAnchors(0.f, 0.f, 1.f, 1.f), FVector2D::ZeroVector, FVector2D::ZeroVector, false);
	if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(FlashOverlay->Slot)) { S->SetOffsets(FMargin(0.f)); }
}

void UNSCHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	ANSCPlayerController* PC = GetNSCPC();
	if (!PC || !Root) { return; }
	const ANSCCharacter* C = PC->GetSpectatedCharacter();
	UpdateTop();
	UpdateVitals(C);
	UpdateWeapon(C);
	UpdateCrosshair(C);
	UpdateKillFeed();
	UpdateCenter(C);
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now >= NextTeamUpdate) { NextTeamUpdate = Now + 0.25f; UpdateTeamList(); }
}

void UNSCHUDWidget::UpdateTop()
{
	const ANSCGameState* GS = GetWorld()->GetGameState<ANSCGameState>();
	const ANSCPlayerState* PS = GetOwningPlayerState<ANSCPlayerState>();
	if (!GS) { return; }
	const ENSCTeam Mine = PS && PS->GetTeam() != ENSCTeam::None ? PS->GetTeam() : ENSCTeam::Defenders;
	const ENSCTeam Theirs = NSCText::Opposite(Mine);
	ScoreOwn->SetText(FText::AsNumber(GS->GetScoreForTeam(Mine)));
	ScoreOwn->SetColorAndOpacity(FSlateColor(NSCColors::Team(Mine)));
	ScoreEnemy->SetText(FText::AsNumber(GS->GetScoreForTeam(Theirs)));
	ScoreEnemy->SetColorAndOpacity(FSlateColor(NSCColors::Team(Theirs)));

	const ENSCRoundPhase P = GS->GetPhase();
	const bool bPlanted = P == ENSCRoundPhase::BombPlanted;
	TimerText->SetText(FText::FromString(NSCUI::Clock(bPlanted ? GS->GetBombTimeRemaining() : GS->GetPhaseTimeRemaining())));
	TimerText->SetColorAndOpacity(FSlateColor(bPlanted ? NSCColors::Injury() : NSCColors::Light()));
	FString Phase;
	switch (P)
	{
	case ENSCRoundPhase::WaitingForPlayers: Phase = TEXT("WAITING FOR PLAYERS"); break;
	case ENSCRoundPhase::Buy: Phase = TEXT("BUY PHASE"); break;
	case ENSCRoundPhase::Preparation: Phase = TEXT("PREPARATION"); break;
	case ENSCRoundPhase::Live: Phase = FString::Printf(TEXT("ROUND %d"), GS->GetRoundNumber()); break;
	case ENSCRoundPhase::BombPlanted: Phase = FString::Printf(TEXT("CHARGE PLANTED  %s"), *GS->GetPlantedSite().ToString()); break;
	case ENSCRoundPhase::RoundEnd: Phase = TEXT("ROUND OVER"); break;
	case ENSCRoundPhase::HalfTime: Phase = TEXT("HALF TIME"); break;
	case ENSCRoundPhase::MatchEnd: Phase = TEXT("MATCH OVER"); break;
	}
	if (GS->IsOvertime()) { Phase += TEXT("  OT"); }
	PhaseText->SetText(FText::FromString(Phase));

	MoneyText->SetText(FText::FromString(PS ? NSCUI::Money(PS->GetMoney()) : TEXT("")));
	const bool bCanBuy = P == ENSCRoundPhase::Buy || P == ENSCRoundPhase::Preparation;
	FString Hint = PS ? FString::Printf(TEXT("%s"), *NSCText::TeamName(PS->GetTeam())) : FString();
	if (bCanBuy) { Hint += TEXT("   [B] buy"); }
	HintText->SetText(FText::FromString(Hint));
	HintText->SetColorAndOpacity(FSlateColor(NSCColors::Team(Mine)));
}

void UNSCHUDWidget::UpdateVitals(const ANSCCharacter* C)
{
	if (!C)
	{
		HealthText->SetText(FText::FromString(TEXT("-")));
		StateText->SetText(FText::GetEmpty());
		InjuryText->SetText(FText::GetEmpty());
		MedText->SetText(FText::GetEmpty());
		ArmorText->SetText(FText::GetEmpty());
		InjuryVignette->SetColorAndOpacity(NSCUI::WithAlpha(NSCColors::Injury(), 0.f));
		return;
	}
	const UNSCHealthComponent* H = C->GetHealth();
	const UNSCWeaponComponent* W = C->GetWeapons();
	const ENSCInjuryState S = H->GetInjuryState();
	const bool bDowned = H->IsDowned();
	HealthText->SetText(FText::AsNumber(bDowned ? 0 : FMath::CeilToInt(H->GetHealth())));
	HealthBar->SetPercent(bDowned ? H->GetBleedoutRemaining() / NSCRules::BleedoutTime : H->GetHealth() / NSCRules::MaxHealth);
	const FLinearColor StateCol = (S == ENSCInjuryState::Healthy) ? NSCColors::Light() : (S == ENSCInjuryState::Wounded ? NSCColors::Gold() : NSCColors::Injury());
	HealthText->SetColorAndOpacity(FSlateColor(StateCol));
	HealthBar->SetFillColorAndOpacity(StateCol);
	ArmorText->SetText(FText::FromString(FString::Printf(TEXT("ARMOR %d%s"), FMath::RoundToInt(H->GetArmor()), H->HasHelmet() ? TEXT(" + HELMET") : TEXT(""))));

	const TCHAR* StateNames[] = { TEXT("HEALTHY"), TEXT("WOUNDED"), TEXT("CRITICAL"), TEXT("DOWNED"), TEXT("DEAD") };
	StateText->SetText(FText::FromString(StateNames[FMath::Clamp(static_cast<int32>(S), 0, 4)]));
	StateText->SetColorAndOpacity(FSlateColor(StateCol));

	TArray<FString> Inj;
	if (H->GetBleed() == ENSCBleed::Heavy) { Inj.Add(TEXT("HEAVY BLEED - [V] tourniquet")); }
	else if (H->GetBleed() == ENSCBleed::Light) { Inj.Add(TEXT("BLEEDING - [X] bandage")); }
	if (H->HasLegInjury()) { Inj.Add(TEXT("LEG INJURY - slower")); }
	if (H->HasArmInjury()) { Inj.Add(TEXT("ARM INJURY - shaky aim")); }
	if (H->WasRescuedThisRound()) { Inj.Add(TEXT("NO SECOND CHANCE - next down is final")); }
	InjuryText->SetText(FText::FromString(FString::Join(Inj, TEXT("\n"))));
	MedText->SetText(FText::FromString(FString::Printf(TEXT("Bandage %d   Tourniquet %d   Medkit %d"), W->Bandages, W->Tourniquets, W->Medkits)));

	const float Alpha = bDowned ? 0.35f : (S == ENSCInjuryState::Critical ? 0.18f : (S == ENSCInjuryState::Wounded ? 0.06f : 0.f));
	InjuryVignette->SetColorAndOpacity(NSCUI::WithAlpha(NSCColors::Injury(), Alpha));
}

void UNSCHUDWidget::UpdateWeapon(const ANSCCharacter* C)
{
	FNSCWeaponStats S;
	const UNSCWeaponComponent* W = C ? C->GetWeapons() : nullptr;
	const FNSCWeaponInstance* Inst = W ? W->GetActiveWeapon() : nullptr;
	if (!W || !Inst || !W->GetActiveStats(S))
	{
		WeaponName->SetText(FText::GetEmpty());
		AmmoText->SetText(FText::GetEmpty());
		MagsText->SetText(FText::GetEmpty());
	}
	else
	{
		WeaponName->SetText(FText::FromString(S.DisplayName.ToUpper()));
		AmmoText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), Inst->Rounds(), Inst->ReserveRounds())));
		AmmoText->SetColorAndOpacity(FSlateColor(Inst->Rounds() <= S.MagSize / 4 ? NSCColors::Injury() : NSCColors::Light()));
		// magazine memory: show every magazine you carry
		FString Mags;
		for (int32 i = 0; i < Inst->Mags.Num(); ++i)
		{
			Mags += FString::Printf(i == Inst->CurrentMag ? TEXT("[%d] ") : TEXT("%d "), Inst->Mags[i]);
		}
		if (W->IsReloading()) { Mags = FString::Printf(TEXT("RELOADING %d%%   "), FMath::RoundToInt(W->GetReloadFraction() * 100.f)) + Mags; }
		MagsText->SetText(FText::FromString(Mags));
	}
	if (W)
	{
		FString Gear = FString::Printf(TEXT("[3] Smoke %d   [4] Flash %d   [5] Frag %d"), W->Smokes, W->Flashes, W->Frags);
		if (W->bDefuseKit) { Gear += TEXT("   KIT"); }
		if (C->HasBomb()) { Gear += TEXT("   CHARGE"); }
		GearText->SetText(FText::FromString(Gear));
	}
	else
	{
		GearText->SetText(FText::GetEmpty());
	}
}

void UNSCHUDWidget::UpdateCrosshair(const ANSCCharacter* C)
{
	const bool bShow = C && !C->GetHealth()->IsDowned() && !C->GetHealth()->IsDead() && !(C->GetWeapons()->IsAiming());
	const float Spread = C ? C->GetWeapons()->GetCurrentSpread() : 0.f;
	const float Gap = 5.f + FMath::Clamp(Spread * 6.f, 0.f, 60.f);
	const FVector2D Offsets[4] = { FVector2D(0.f, -Gap - 4.f), FVector2D(0.f, Gap + 4.f), FVector2D(-Gap - 4.f, 0.f), FVector2D(Gap + 4.f, 0.f) };
	for (int32 i = 0; i < Cross.Num(); ++i)
	{
		Cross[i]->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (i < 4) { if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(Cross[i]->Slot)) { S->SetPosition(Offsets[i]); } }
	}
	const float Flash = C ? C->GetFlashAmount() : 0.f;
	FlashOverlay->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, FMath::Clamp(Flash, 0.f, 1.f)));
}

void UNSCHUDWidget::UpdateKillFeed()
{
	const ANSCGameState* GS = GetWorld()->GetGameState<ANSCGameState>();
	if (!GS) { return; }
	const TArray<FNSCKillFeedEntry>& Feed = GS->GetKillFeed();
	const float LastTime = Feed.Num() ? Feed.Last().Time : 0.f;
	if (Feed.Num() == LastFeedCount && LastTime == LastFeedTime) { return; }
	LastFeedCount = Feed.Num();
	LastFeedTime = LastTime;
	KillFeedBox->ClearChildren();
	for (const FNSCKillFeedEntry& E : Feed)
	{
		UBorder* Row = NSCUI::Panel(WidgetTree, NSCUI::WithAlpha(NSCColors::Graphite(), 0.75f), FMargin(10.f, 3.f));
		UHorizontalBox* H = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (!E.Killer.IsEmpty()) { NSCUI::AddH(H, NSCUI::Text(WidgetTree, E.Killer, 14, NSCColors::Team(E.KillerTeam))); }
		FString Mid = E.bRescue ? TEXT("+ revived") : (E.bDowned ? FString::Printf(TEXT("[%s] downed"), *E.Weapon) : FString::Printf(TEXT("[%s]%s"), *E.Weapon, E.bHeadshot ? TEXT(" HS") : TEXT("")));
		NSCUI::AddH(H, NSCUI::Text(WidgetTree, Mid, 13, E.bRescue ? NSCColors::Gold() : (E.bDowned ? NSCColors::Injury() : NSCColors::Light()), false));
		NSCUI::AddH(H, NSCUI::Text(WidgetTree, E.Victim, 14, NSCColors::Team(E.VictimTeam)));
		Row->SetContent(H);
		NSCUI::AddV(KillFeedBox, Row, FMargin(0.f, 2.f), HAlign_Right);
	}
}

void UNSCHUDWidget::UpdateTeamList()
{
	const ANSCGameState* GS = GetWorld()->GetGameState<ANSCGameState>();
	const ANSCPlayerState* Me = GetOwningPlayerState<ANSCPlayerState>();
	if (!GS || !Me) { return; }
	TeamBox->ClearChildren();
	for (ANSCPlayerState* PS : GS->GetPlayersOnTeam(Me->GetTeam()))
	{
		const ENSCInjuryState S = PS->GetLifeState();
		const TCHAR* Tag = S == ENSCInjuryState::Dead ? TEXT("DEAD") : (S == ENSCInjuryState::Downed ? TEXT("DOWNED") : (S == ENSCInjuryState::Critical ? TEXT("CRIT") : TEXT("")));
		const FLinearColor Col = S == ENSCInjuryState::Dead ? FLinearColor(0.4f, 0.4f, 0.4f, 1.f) : (S == ENSCInjuryState::Downed || S == ENSCInjuryState::Critical ? NSCColors::Injury() : NSCColors::Team(Me->GetTeam()));
		NSCUI::AddV(TeamBox, NSCUI::Text(WidgetTree, FString::Printf(TEXT("%s  %s"), *PS->GetPlayerName(), Tag), 13, Col, PS == Me));
	}
}

void UNSCHUDWidget::UpdateCenter(const ANSCCharacter* C)
{
	const ANSCGameState* GS = GetWorld()->GetGameState<ANSCGameState>();
	ANSCPlayerController* PC = GetNSCPC();
	FString Big, Sub;
	FLinearColor BigCol = NSCColors::Light();
	if (GS)
	{
		const ENSCRoundPhase P = GS->GetPhase();
		const FNSCRoundSummary& R = GS->GetLastRound();
		if (P == ENSCRoundPhase::RoundEnd)
		{
			Big = FString::Printf(TEXT("%s WIN"), *NSCText::TeamName(R.Winner));
			BigCol = NSCColors::Team(R.Winner);
			Sub = NSCText::ReasonText(R.Reason);
			if (!R.MVPName.IsEmpty()) { Sub += FString::Printf(TEXT("   MVP %s"), *R.MVPName); }
			if (R.Rescues > 0) { Sub += FString::Printf(TEXT("   Rescues %d"), R.Rescues); }
		}
		else if (P == ENSCRoundPhase::HalfTime) { Big = TEXT("HALF TIME"); Sub = TEXT("Switching sides"); BigCol = NSCColors::Gold(); }
		else if (P == ENSCRoundPhase::MatchEnd)
		{
			const ANSCPlayerState* Me = GetOwningPlayerState<ANSCPlayerState>();
			const int32 W = GS->GetMatchWinnerSquad();
			Big = W == 2 ? TEXT("DRAW") : (Me && Me->GetSquad() == W ? TEXT("VICTORY") : TEXT("DEFEAT"));
			BigCol = NSCColors::Gold();
			Sub = FString::Printf(TEXT("%d - %d   Hold [Tab] for stats"), GS->GetSquadScore(0), GS->GetSquadScore(1));
		}
		else if (P == ENSCRoundPhase::Preparation && C && C->GetTeam() == ENSCTeam::Attackers) { Sub = TEXT("Defenders are setting up"); }
		else if (P == ENSCRoundPhase::WaitingForPlayers) { Sub = TEXT("Match starting..."); }
	}
	if (C && C->GetHealth()->IsDowned() && C == (PC ? PC->GetPawn() : nullptr))
	{
		Big = TEXT("DOWNED");
		BigCol = NSCColors::Injury();
		Sub = FString::Printf(TEXT("Bleeding out %ds - a teammate with a medkit can revive you"), FMath::CeilToInt(C->GetHealth()->GetBleedoutRemaining()));
		if (C->GetHealth()->GetReviveProgress() > 0.f) { Sub = TEXT("Being revived..."); }
	}
	BigText->SetText(FText::FromString(Big));
	BigText->SetColorAndOpacity(FSlateColor(BigCol));
	SubText->SetText(FText::FromString(Sub));

	const FString Prompt = C ? C->GetInteractPrompt() : FString();
	PromptText->SetText(FText::FromString(Prompt));
	const float Progress = C ? C->GetActionProgress() : 0.f;
	const bool bAction = C && C->GetCurrentAction() != ENSCActionType::None && C->GetCurrentAction() != ENSCActionType::Dragging;
	ActionBar->SetVisibility(bAction ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	ActionBar->SetPercent(Progress);

	const bool bSpectating = PC && C && C != PC->GetPawn();
	SpectateText->SetText(bSpectating ? FText::FromString(FString::Printf(TEXT("SPECTATING  %s   [LMB] next"), *C->GetDisplayName())) : FText::GetEmpty());
}

// ============================================================================ buy menu

void UNSCBuyMenuWidget::BuildTree()
{
	using namespace NSCUI;
	UWidgetTree* T = WidgetTree;
	UCanvasPanel* Root = T->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	T->RootWidget = Root;
	UBorder* P = Panel(T, WithAlpha(NSCColors::Graphite(), 0.94f), FMargin(24.f));
	UVerticalBox* V = T->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UHorizontalBox* Head = T->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Header = Text(T, TEXT("BUY"), 24, NSCColors::Gold());
	AddH(Head, Header, FMargin(0.f), true);
	AddH(Head, Button(T, TEXT("CLOSE  [B]"), [this]() { if (ANSCPlayerController* PC = GetNSCPC()) { PC->CloseMenus(); } }, 14));
	AddV(V, Head, FMargin(0.f, 0.f, 0.f, 12.f));
	Columns = T->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddV(V, Columns);
	P->SetContent(V);
	Place(Root, P, FAnchors(0.5f, 0.5f), FVector2D(0.5f, 0.5f), FVector2D::ZeroVector);
}

void UNSCBuyMenuWidget::RebuildItems(ENSCTeam Team)
{
	using namespace NSCUI;
	UWidgetTree* T = WidgetTree;
	BuiltFor = Team;
	Columns->ClearChildren();
	ItemButtons.Reset();
	ItemPrices.Reset();

	auto MakeColumn = [&](const FString& Title) -> UVerticalBox*
	{
		UVerticalBox* Col = T->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		AddV(Col, Text(T, Title, 14, NSCColors::Gold()), FMargin(0.f, 0.f, 0.f, 6.f));
		AddH(Columns, Fixed(T, Col, 220.f), FMargin(6.f, 0.f), false, VAlign_Top);
		return Col;
	};
	UVerticalBox* Pistols = MakeColumn(TEXT("PISTOLS"));
	UVerticalBox* Smgs = MakeColumn(TEXT("SMG / SHOTGUN"));
	UVerticalBox* Rifles = MakeColumn(TEXT("RIFLES"));
	UVerticalBox* Heavy = MakeColumn(TEXT("SNIPER / LMG"));
	UVerticalBox* Gear = MakeColumn(TEXT("GEAR / MEDICAL"));

	for (const FNSCWeaponStats& S : UNSCWeaponLibrary::GetAllWeapons())
	{
		if (S.TeamOnly != ENSCTeam::None && S.TeamOnly != Team) { continue; }
		UVerticalBox* Col = Rifles;
		switch (S.WeaponClass)
		{
		case ENSCWeaponClass::Pistol: Col = Pistols; break;
		case ENSCWeaponClass::SMG: case ENSCWeaponClass::Shotgun: Col = Smgs; break;
		case ENSCWeaponClass::Sniper: case ENSCWeaponClass::LMG: Col = Heavy; break;
		default: break;
		}
		const FName Id = S.Id;
		UNSCButton* B = Button(T, FString::Printf(TEXT("%s   $%d"), *S.DisplayName, S.Price), [this, Id]() { if (ANSCPlayerController* PC = GetNSCPC()) { PC->RequestBuyWeapon(Id); } }, 14);
		AddV(Col, B, FMargin(0.f, 2.f));
		ItemButtons.Add(B);
		ItemPrices.Add(S.Price);
	}
	for (const FNSCEquipmentInfo& E : UNSCWeaponLibrary::GetAllEquipment())
	{
		if (E.TeamOnly != ENSCTeam::None && E.TeamOnly != Team) { continue; }
		const ENSCEquipment Type = E.Type;
		UNSCButton* B = Button(T, FString::Printf(TEXT("%s   $%d"), *E.Name, E.Price), [this, Type]() { if (ANSCPlayerController* PC = GetNSCPC()) { PC->RequestBuyEquipment(Type); } }, 14);
		AddV(Gear, B, FMargin(0.f, 2.f));
		ItemButtons.Add(B);
		ItemPrices.Add(E.Price);
	}
}

void UNSCBuyMenuWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const ANSCPlayerState* PS = GetOwningPlayerState<ANSCPlayerState>();
	const ANSCGameState* GS = GetWorld()->GetGameState<ANSCGameState>();
	if (!PS) { return; }
	if (PS->GetTeam() != BuiltFor) { RebuildItems(PS->GetTeam()); }
	const int32 Money = PS->GetMoney();
	Header->SetText(FText::FromString(FString::Printf(TEXT("BUY   %s   %s left"), *NSCUI::Money(Money), GS ? *NSCUI::Clock(GS->GetPhaseTimeRemaining()) : TEXT(""))));
	for (int32 i = 0; i < ItemButtons.Num(); ++i)
	{
		if (ItemButtons[i]) { ItemButtons[i]->SetIsEnabled(Money >= ItemPrices[i]); }
	}
}

// ============================================================================ scoreboard

void UNSCScoreboardWidget::BuildTree()
{
	using namespace NSCUI;
	UWidgetTree* T = WidgetTree;
	UCanvasPanel* Root = T->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	T->RootWidget = Root;
	UBorder* P = Panel(T, WithAlpha(NSCColors::Graphite(), 0.92f), FMargin(24.f));
	UVerticalBox* V = T->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Title = Text(T, TEXT("SCOREBOARD"), 22, NSCColors::Gold());
	TopBox = T->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	BottomBox = T->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	AddV(V, Title, FMargin(0.f, 0.f, 0.f, 10.f));
	AddV(V, TopBox, FMargin(0.f, 4.f));
	AddV(V, BottomBox, FMargin(0.f, 14.f, 0.f, 4.f));
	AddV(V, Text(T, TEXT("ADR damage/round   DN downs   REV revives (medkit)   RES rescues (dragged out, then revived)   TPS tactical performance score"), 11, NSCColors::Light(), false), FMargin(0.f, 10.f, 0.f, 0.f));
	P->SetContent(V);
	Place(Root, P, FAnchors(0.5f, 0.45f), FVector2D(0.5f, 0.5f), FVector2D::ZeroVector);
}

void UNSCScoreboardWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now < NextRefresh || GetVisibility() == ESlateVisibility::Collapsed) { return; }
	NextRefresh = Now + 0.3f;
	const ANSCGameState* GS = GetWorld()->GetGameState<ANSCGameState>();
	const ANSCPlayerState* Me = GetOwningPlayerState<ANSCPlayerState>();
	if (!GS) { return; }
	const ENSCTeam Mine = Me && Me->GetTeam() != ENSCTeam::None ? Me->GetTeam() : ENSCTeam::Defenders;
	Title->SetText(FText::FromString(FString::Printf(TEXT("%s   ROUND %d   FIRST TO %d"), *GS->GetModeName().ToUpper(), GS->GetRoundNumber(), GS->GetRoundsToWin())));
	FillTeam(TopBox, Mine, true);
	FillTeam(BottomBox, NSCText::Opposite(Mine), false);
}

void UNSCScoreboardWidget::FillTeam(UVerticalBox* Box, ENSCTeam Team, bool bShowMoney)
{
	using namespace NSCUI;
	UWidgetTree* T = WidgetTree;
	const ANSCGameState* GS = GetWorld()->GetGameState<ANSCGameState>();
	Box->ClearChildren();
	const FLinearColor Col = NSCColors::Team(Team);
	auto Row = [&](const TArray<FString>& Cells, const FLinearColor& C, bool bBold)
	{
		UHorizontalBox* H = T->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		const float Widths[] = { 220.f, 46.f, 46.f, 46.f, 60.f, 46.f, 50.f, 50.f, 60.f, 80.f, 80.f };
		for (int32 i = 0; i < Cells.Num() && i < static_cast<int32>(UE_ARRAY_COUNT(Widths)); ++i)
		{
			AddH(H, Fixed(T, Text(T, Cells[i], 14, C, bBold), Widths[i]), FMargin(2.f, 0.f));
		}
		AddV(Box, H, FMargin(0.f, 1.f));
	};
	Row({ FString::Printf(TEXT("%s  %d"), *NSCText::TeamName(Team), GS ? GS->GetScoreForTeam(Team) : 0), TEXT("K"), TEXT("D"), TEXT("A"), TEXT("ADR"), TEXT("DN"), TEXT("REV"), TEXT("RES"), TEXT("TPS"), bShowMoney ? TEXT("MONEY") : TEXT(""), TEXT("") }, Col, true);
	if (!GS) { return; }
	TArray<ANSCPlayerState*> Players = GS->GetPlayersOnTeam(Team);
	Players.Sort([](const ANSCPlayerState& A, const ANSCPlayerState& B) { return A.GetTPS() > B.GetTPS(); });
	for (const ANSCPlayerState* PS : Players)
	{
		const ENSCInjuryState S = PS->GetLifeState();
		const FString Status = S == ENSCInjuryState::Dead ? TEXT("DEAD") : (S == ENSCInjuryState::Downed ? TEXT("DOWNED") : TEXT(""));
		Row({ PS->GetPlayerName(), FString::FromInt(PS->Kills), FString::FromInt(PS->Deaths), FString::FromInt(PS->Assists),
			FString::Printf(TEXT("%.0f"), PS->GetADR()), FString::FromInt(PS->Downs), FString::FromInt(PS->Revives), FString::FromInt(PS->Rescues),
			FString::Printf(TEXT("%.2f"), PS->GetTPS()), bShowMoney ? NSCUI::Money(PS->GetMoney()) : FString(), Status },
			S == ENSCInjuryState::Dead ? FLinearColor(0.5f, 0.5f, 0.5f, 1.f) : NSCColors::Light(), PS == GetOwningPlayerState());
	}
}

// ============================================================================ pause menu

void UNSCPauseMenuWidget::BuildTree()
{
	using namespace NSCUI;
	UWidgetTree* T = WidgetTree;
	UCanvasPanel* Root = T->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	T->RootWidget = Root;
	UImage* Dim = T->ConstructWidget<UImage>(UImage::StaticClass());
	Dim->SetColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.55f));
	UCanvasPanelSlot* DS = Place(Root, Dim, FAnchors(0.f, 0.f, 1.f, 1.f), FVector2D::ZeroVector, FVector2D::ZeroVector, false);
	DS->SetOffsets(FMargin(0.f));

	UBorder* P = Panel(T, NSCColors::Graphite(), FMargin(30.f));
	UVerticalBox* V = T->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	AddV(V, Text(T, TEXT("PAUSED"), 28, NSCColors::Gold()), FMargin(0.f, 0.f, 0.f, 14.f));
	AddV(V, Button(T, TEXT("RESUME"), [this]() { if (ANSCPlayerController* PC = GetNSCPC()) { PC->CloseMenus(); } }));

	auto Stepper = [&](TObjectPtr<UTextBlock>& Out, TFunction<void(float)> Step)
	{
		UHorizontalBox* H = T->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Out = Text(T, TEXT(""), 16, NSCColors::Light());
		AddH(H, Button(T, TEXT("-"), [this, Step]() { Step(-1.f); RefreshSettings(); }, 16), FMargin(0.f, 0.f, 6.f, 0.f));
		AddH(H, Fixed(T, Out, 220.f));
		AddH(H, Button(T, TEXT("+"), [this, Step]() { Step(1.f); RefreshSettings(); }, 16));
		AddV(V, H, FMargin(0.f, 6.f));
	};
	UNSCGameInstance* GI = GetGameInstance<UNSCGameInstance>();
	TWeakObjectPtr<UNSCGameInstance> WeakGI = GI;
	Stepper(SensText, [WeakGI](float D) { if (UNSCGameInstance* G = WeakGI.Get()) { G->SetMouseSensitivity(G->GetMouseSensitivity() + 0.1f * D); G->SaveSettings(); } });
	Stepper(FovText, [WeakGI](float D) { if (UNSCGameInstance* G = WeakGI.Get()) { G->SetFieldOfView(G->GetFieldOfView() + 5.f * D); G->SaveSettings(); } });

	AddV(V, Button(T, TEXT("LEAVE MATCH"), [this]() { if (ANSCPlayerController* PC = GetNSCPC()) { PC->LeaveMatch(); } }), FMargin(0.f, 14.f, 0.f, 2.f));
	AddV(V, Button(T, TEXT("QUIT TO DESKTOP"), [this]() { UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false); }));
	P->SetContent(V);
	Place(Root, P, FAnchors(0.5f, 0.5f), FVector2D(0.5f, 0.5f), FVector2D::ZeroVector);
	RefreshSettings();
}

void UNSCPauseMenuWidget::RefreshSettings()
{
	const UNSCGameInstance* GI = GetGameInstance<UNSCGameInstance>();
	if (!GI) { return; }
	if (SensText) { SensText->SetText(FText::FromString(FString::Printf(TEXT("Mouse sensitivity %.1f"), GI->GetMouseSensitivity()))); }
	if (FovText) { FovText->SetText(FText::FromString(FString::Printf(TEXT("Field of view %.0f"), GI->GetFieldOfView()))); }
}

// ============================================================================ main menu

void UNSCMainMenuWidget::BuildTree()
{
	using namespace NSCUI;
	UWidgetTree* T = WidgetTree;
	UCanvasPanel* Root = T->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	T->RootWidget = Root;

	UImage* Bg = T->ConstructWidget<UImage>(UImage::StaticClass());
	Bg->SetColorAndOpacity(NSCUI::WithAlpha(NSCColors::Graphite(), 0.82f));
	Place(Root, Bg, FAnchors(0.f, 0.f, 1.f, 1.f), FVector2D::ZeroVector, FVector2D::ZeroVector, false)->SetOffsets(FMargin(0.f));
	UImage* Stripe = T->ConstructWidget<UImage>(UImage::StaticClass());
	Stripe->SetColorAndOpacity(NSCColors::Gold());
	Place(Root, Stripe, FAnchors(0.f, 0.f, 0.f, 1.f), FVector2D::ZeroVector, FVector2D::ZeroVector, false)->SetOffsets(FMargin(64.f, 0.f, 6.f, 0.f));

	UNSCGameInstance* GI = GetGameInstance<UNSCGameInstance>();
	TWeakObjectPtr<UNSCGameInstance> WeakGI = GI;

	UVerticalBox* Left = T->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	AddV(Left, Text(T, TEXT("NO SECOND CHANCE"), 54, NSCColors::Light()), FMargin(0.f));
	AddV(Left, Text(T, TEXT("TACTICAL 5v5  -  INJURIES MATTER  -  ONE RESCUE PER ROUND"), 15, NSCColors::Gold()), FMargin(0.f, 0.f, 0.f, 30.f));

	auto Big = [&](const FString& L, TFunction<void()> Fn) { AddV(Left, Fixed(T, Button(T, L, MoveTemp(Fn), 20), 380.f), FMargin(0.f, 4.f)); };
	Big(TEXT("PLAY  -  OPERATIONS VS BOTS"), [WeakGI]() { if (UNSCGameInstance* G = WeakGI.Get()) { G->PlayOffline(false); } });
	Big(TEXT("QUICK MATCH  -  CASUAL VS BOTS"), [WeakGI]() { if (UNSCGameInstance* G = WeakGI.Get()) { G->PlayOffline(true); } });
	Big(TEXT("HOST LAN GAME"), [WeakGI]() { if (UNSCGameInstance* G = WeakGI.Get()) { G->HostLan(true, false); } });
	Big(TEXT("FIND LAN GAMES"), [WeakGI, this]() { if (ResultsBox) { ResultsBox->ClearChildren(); } if (UNSCGameInstance* G = WeakGI.Get()) { G->FindLanGames(); } });

	UHorizontalBox* JoinRow = T->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddressBox = T->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
	AddressBox->SetHintText(FText::FromString(TEXT("IP address, e.g. 192.168.1.20")));
	AddH(JoinRow, Fixed(T, AddressBox, 260.f), FMargin(0.f, 0.f, 6.f, 0.f));
	AddH(JoinRow, Button(T, TEXT("JOIN IP"), [WeakGI, this]() { if (UNSCGameInstance* G = WeakGI.Get()) { G->JoinByAddress(AddressBox ? AddressBox->GetText().ToString() : FString()); } }, 16));
	AddV(Left, JoinRow, FMargin(0.f, 8.f));

	auto Stepper = [&](TObjectPtr<UTextBlock>& Out, TFunction<void(float)> Step)
	{
		UHorizontalBox* H = T->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Out = Text(T, TEXT(""), 15, NSCColors::Light(), false);
		AddH(H, Button(T, TEXT("-"), [this, Step]() { Step(-1.f); RefreshSettings(); }, 14), FMargin(0.f, 0.f, 6.f, 0.f));
		AddH(H, Fixed(T, Out, 240.f));
		AddH(H, Button(T, TEXT("+"), [this, Step]() { Step(1.f); RefreshSettings(); }, 14));
		AddV(Left, H, FMargin(0.f, 3.f));
	};
	AddV(Left, Text(T, TEXT("SETTINGS"), 14, NSCColors::Gold()), FMargin(0.f, 22.f, 0.f, 4.f));
	Stepper(SensText, [WeakGI](float D) { if (UNSCGameInstance* G = WeakGI.Get()) { G->SetMouseSensitivity(G->GetMouseSensitivity() + 0.1f * D); G->SaveSettings(); } });
	Stepper(FovText, [WeakGI](float D) { if (UNSCGameInstance* G = WeakGI.Get()) { G->SetFieldOfView(G->GetFieldOfView() + 5.f * D); G->SaveSettings(); } });
	Stepper(SkillText, [WeakGI](float D) { if (UNSCGameInstance* G = WeakGI.Get()) { G->SetBotSkill(G->GetBotSkill() + static_cast<int32>(10.f * D)); G->SaveSettings(); } });

	AddV(Left, Fixed(T, Button(T, TEXT("QUIT"), [this]() { UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false); }, 18), 380.f), FMargin(0.f, 22.f, 0.f, 0.f));
	Place(Root, Left, FAnchors(0.f, 0.5f), FVector2D(0.f, 0.5f), FVector2D(110.f, 0.f));

	// right: LAN browser + status
	UBorder* RP = Panel(T, NSCUI::WithAlpha(NSCColors::Slate(), 0.9f), FMargin(18.f));
	UVerticalBox* RV = T->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	AddV(RV, Text(T, TEXT("LAN GAMES"), 16, NSCColors::Gold()));
	StatusText = Text(T, GI ? GI->ConsumeMenuMessage() : FString(), 14, NSCColors::Light(), false);
	AddV(RV, StatusText, FMargin(0.f, 6.f));
	ResultsBox = T->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	AddV(RV, ResultsBox);
	RP->SetContent(Fixed(T, RV, 420.f));
	Place(Root, RP, FAnchors(1.f, 0.5f), FVector2D(1.f, 0.5f), FVector2D(-90.f, 0.f));

	const FString Version = GetDefault<UGeneralProjectSettings>()->ProjectVersion;
	Place(Root, Text(T, FString::Printf(TEXT("v%s  |  Unreal Engine 5"), *Version), 12, NSCColors::Light(), false), FAnchors(1.f, 1.f), FVector2D(1.f, 1.f), FVector2D(-24.f, -16.f));

	if (GI)
	{
		LanHandle = GI->OnLanSearchDone.AddUObject(this, &UNSCMainMenuWidget::OnLanResults);
		StatusHandle = GI->OnStatus.AddUObject(this, &UNSCMainMenuWidget::OnStatus);
	}
	RefreshSettings();
}

void UNSCMainMenuWidget::NativeDestruct()
{
	if (UNSCGameInstance* GI = GetGameInstance<UNSCGameInstance>())
	{
		GI->OnLanSearchDone.Remove(LanHandle);
		GI->OnStatus.Remove(StatusHandle);
	}
	Super::NativeDestruct();
}

void UNSCMainMenuWidget::RefreshSettings()
{
	const UNSCGameInstance* GI = GetGameInstance<UNSCGameInstance>();
	if (!GI) { return; }
	if (SensText) { SensText->SetText(FText::FromString(FString::Printf(TEXT("Mouse sensitivity  %.1f"), GI->GetMouseSensitivity()))); }
	if (FovText) { FovText->SetText(FText::FromString(FString::Printf(TEXT("Field of view  %.0f"), GI->GetFieldOfView()))); }
	if (SkillText) { SkillText->SetText(FText::FromString(FString::Printf(TEXT("Bot skill  %d%%"), GI->GetBotSkill()))); }
}

void UNSCMainMenuWidget::OnStatus(const FString& Msg)
{
	if (StatusText) { StatusText->SetText(FText::FromString(Msg)); }
}

void UNSCMainMenuWidget::OnLanResults(const TArray<FNSCLanGame>& Games)
{
	if (!ResultsBox) { return; }
	ResultsBox->ClearChildren();
	OnStatus(Games.Num() ? FString::Printf(TEXT("%d game(s) found"), Games.Num()) : TEXT("No LAN games found. Ask the host for their IP and use JOIN IP."));
	TWeakObjectPtr<UNSCGameInstance> WeakGI = GetGameInstance<UNSCGameInstance>();
	for (const FNSCLanGame& G : Games)
	{
		const int32 Idx = G.ResultIndex;
		NSCUI::AddV(ResultsBox, NSCUI::Button(WidgetTree, FString::Printf(TEXT("%s   %d/%d   %d ms"), *G.HostName, G.Players, G.MaxPlayers, G.PingMs),
			[WeakGI, Idx]() { if (UNSCGameInstance* GI = WeakGI.Get()) { GI->JoinLanGame(Idx); } }, 15), FMargin(0.f, 3.f));
	}
}
