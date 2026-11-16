// All UMG widgets, built in C++ (no Widget Blueprints needed): HUD, buy menu, scoreboard, pause menu
// and main menu. Palette: graphite #171A1D, slate #252B32, gold #D5A64A, light #E7E9EB; red = injury,
// blue = defenders, orange = attackers.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Fonts/SlateFontInfo.h"
#include "NSCTypes.h"
#include "NSCWeaponData.h"
#include "NSCWidgets.generated.h"

class UTextBlock;
class UCanvasPanel;
class UVerticalBox;
class UHorizontalBox;
class UBorder;
class UImage;
class UEditableTextBox;
class UProgressBar;
class UWidgetTree;
class UPanelWidget;
class ANSCCharacter;
class ANSCPlayerController;
struct FNSCLanGame;

/** Button with a text label and a native click callback. */
UCLASS()
class NOSECONDCHANCE_API UNSCButton : public UButton
{
	GENERATED_BODY()
public:
	TFunction<void()> OnClickFn;
	UPROPERTY() TObjectPtr<UTextBlock> Label;
	void Setup(UWidgetTree* Tree, const FString& Text, int32 FontSize, const FLinearColor& Fill);
	void SetLabel(const FString& Text);
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	UFUNCTION() void HandleClicked();
};

namespace NSCUI
{
	FSlateFontInfo Font(int32 Size, bool bBold = true);
	UTextBlock* Text(UWidgetTree* Tree, const FString& S, int32 Size, const FLinearColor& Color, bool bBold = true);
	UBorder* Panel(UWidgetTree* Tree, const FLinearColor& Fill, const FMargin& Padding);
	UNSCButton* Button(UWidgetTree* Tree, const FString& Label, TFunction<void()> OnClick, int32 Size = 18, const FLinearColor& Fill = FLinearColor(0.04f, 0.05f, 0.06f, 1.f));
	FLinearColor WithAlpha(const FLinearColor& C, float A);
	FString Money(int32 M);
	FString Clock(float Seconds);
}

/** Base: builds its tree once in NativeOnInitialized. */
UCLASS(Abstract)
class NOSECONDCHANCE_API UNSCWidgetBase : public UUserWidget
{
	GENERATED_BODY()
protected:
	virtual void NativeOnInitialized() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void BuildTree() {}
	ANSCPlayerController* GetNSCPC() const;
	bool bBuilt = false;
	void EnsureBuilt();
};

UCLASS()
class NOSECONDCHANCE_API UNSCHUDWidget : public UNSCWidgetBase
{
	GENERATED_BODY()
protected:
	virtual void BuildTree() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	void UpdateTop();
	void UpdateVitals(const ANSCCharacter* C);
	void UpdateWeapon(const ANSCCharacter* C);
	void UpdateCrosshair(const ANSCCharacter* C);
	void UpdateKillFeed();
	void UpdateTeamList();
	void UpdateCenter(const ANSCCharacter* C);

	UPROPERTY() TObjectPtr<UCanvasPanel> Root;
	UPROPERTY() TObjectPtr<UTextBlock> ScoreOwn;
	UPROPERTY() TObjectPtr<UTextBlock> ScoreEnemy;
	UPROPERTY() TObjectPtr<UTextBlock> TimerText;
	UPROPERTY() TObjectPtr<UTextBlock> PhaseText;
	UPROPERTY() TObjectPtr<UTextBlock> MoneyText;
	UPROPERTY() TObjectPtr<UTextBlock> HintText;
	UPROPERTY() TObjectPtr<UTextBlock> HealthText;
	UPROPERTY() TObjectPtr<UTextBlock> ArmorText;
	UPROPERTY() TObjectPtr<UTextBlock> StateText;
	UPROPERTY() TObjectPtr<UTextBlock> InjuryText;
	UPROPERTY() TObjectPtr<UTextBlock> MedText;
	UPROPERTY() TObjectPtr<UProgressBar> HealthBar;
	UPROPERTY() TObjectPtr<UTextBlock> WeaponName;
	UPROPERTY() TObjectPtr<UTextBlock> AmmoText;
	UPROPERTY() TObjectPtr<UTextBlock> MagsText;
	UPROPERTY() TObjectPtr<UTextBlock> GearText;
	UPROPERTY() TObjectPtr<UVerticalBox> KillFeedBox;
	UPROPERTY() TObjectPtr<UVerticalBox> TeamBox;
	UPROPERTY() TObjectPtr<UTextBlock> PromptText;
	UPROPERTY() TObjectPtr<UProgressBar> ActionBar;
	UPROPERTY() TObjectPtr<UTextBlock> BigText;
	UPROPERTY() TObjectPtr<UTextBlock> SubText;
	UPROPERTY() TObjectPtr<UTextBlock> SpectateText;
	UPROPERTY() TObjectPtr<UImage> InjuryVignette;
	UPROPERTY() TObjectPtr<UImage> FlashOverlay;
	UPROPERTY() TArray<TObjectPtr<UImage>> Cross;

	int32 LastFeedCount = -1;
	float LastFeedTime = -1.f;
	float NextTeamUpdate = 0.f;
};

UCLASS()
class NOSECONDCHANCE_API UNSCBuyMenuWidget : public UNSCWidgetBase
{
	GENERATED_BODY()
protected:
	virtual void BuildTree() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	void RebuildItems(ENSCTeam Team);

	UPROPERTY() TObjectPtr<UTextBlock> Header;
	UPROPERTY() TObjectPtr<UHorizontalBox> Columns;
	UPROPERTY() TArray<TObjectPtr<UNSCButton>> ItemButtons;
	TArray<int32> ItemPrices;
	ENSCTeam BuiltFor = ENSCTeam::None;
};

UCLASS()
class NOSECONDCHANCE_API UNSCScoreboardWidget : public UNSCWidgetBase
{
	GENERATED_BODY()
protected:
	virtual void BuildTree() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	void FillTeam(UVerticalBox* Box, ENSCTeam Team, bool bShowMoney);
	UPROPERTY() TObjectPtr<UTextBlock> Title;
	UPROPERTY() TObjectPtr<UVerticalBox> TopBox;
	UPROPERTY() TObjectPtr<UVerticalBox> BottomBox;
	float NextRefresh = 0.f;
};

UCLASS()
class NOSECONDCHANCE_API UNSCPauseMenuWidget : public UNSCWidgetBase
{
	GENERATED_BODY()
protected:
	virtual void BuildTree() override;
	void RefreshSettings();
	UPROPERTY() TObjectPtr<UTextBlock> SensText;
	UPROPERTY() TObjectPtr<UTextBlock> FovText;
};

UCLASS()
class NOSECONDCHANCE_API UNSCMainMenuWidget : public UNSCWidgetBase
{
	GENERATED_BODY()
public:
	virtual void NativeDestruct() override;
protected:
	virtual void BuildTree() override;
	void RefreshSettings();
	void OnLanResults(const TArray<FNSCLanGame>& Games);
	void OnStatus(const FString& Msg);
	UPROPERTY() TObjectPtr<UTextBlock> StatusText;
	UPROPERTY() TObjectPtr<UVerticalBox> ResultsBox;
	UPROPERTY() TObjectPtr<UEditableTextBox> AddressBox;
	UPROPERTY() TObjectPtr<UTextBlock> SensText;
	UPROPERTY() TObjectPtr<UTextBlock> FovText;
	UPROPERTY() TObjectPtr<UTextBlock> SkillText;
	FDelegateHandle LanHandle;
	FDelegateHandle StatusHandle;
};
