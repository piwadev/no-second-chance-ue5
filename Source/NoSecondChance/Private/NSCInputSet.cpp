#include "NSCInputSet.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "InputCoreTypes.h"

namespace
{
	UInputAction* MakeAction(UObject* Outer, const TCHAR* Name, EInputActionValueType Type)
	{
		UInputAction* A = NewObject<UInputAction>(Outer, FName(Name));
		A->ValueType = Type;
		return A;
	}

	void Map(UInputMappingContext* Ctx, UInputAction* A, const FKey& Key, bool bNegate = false, bool bSwizzle = false)
	{
		FEnhancedActionKeyMapping& M = Ctx->MapKey(A, Key);
		if (bSwizzle)
		{
			UInputModifierSwizzleAxis* Swizzle = NewObject<UInputModifierSwizzleAxis>(Ctx);
			Swizzle->Order = EInputAxisSwizzle::YXZ;
			M.Modifiers.Add(Swizzle);
		}
		if (bNegate)
		{
			M.Modifiers.Add(NewObject<UInputModifierNegate>(Ctx));
		}
	}
}

UNSCInputSet* UNSCInputSet::Create(UObject* Outer)
{
	UNSCInputSet* S = NewObject<UNSCInputSet>(Outer);
	using T = EInputActionValueType;

	S->Move = MakeAction(S, TEXT("IA_Move"), T::Axis2D);
	S->Look = MakeAction(S, TEXT("IA_Look"), T::Axis2D);
	S->Jump = MakeAction(S, TEXT("IA_Jump"), T::Boolean);
	S->Sprint = MakeAction(S, TEXT("IA_Sprint"), T::Boolean);
	S->Walk = MakeAction(S, TEXT("IA_Walk"), T::Boolean);
	S->Crouch = MakeAction(S, TEXT("IA_Crouch"), T::Boolean);
	S->Fire = MakeAction(S, TEXT("IA_Fire"), T::Boolean);
	S->Aim = MakeAction(S, TEXT("IA_Aim"), T::Boolean);
	S->Reload = MakeAction(S, TEXT("IA_Reload"), T::Boolean);
	S->SlotPrimary = MakeAction(S, TEXT("IA_SlotPrimary"), T::Boolean);
	S->SlotSecondary = MakeAction(S, TEXT("IA_SlotSecondary"), T::Boolean);
	S->SlotSmoke = MakeAction(S, TEXT("IA_SlotSmoke"), T::Boolean);
	S->SlotFlash = MakeAction(S, TEXT("IA_SlotFlash"), T::Boolean);
	S->SlotFrag = MakeAction(S, TEXT("IA_SlotFrag"), T::Boolean);
	S->CycleWeapon = MakeAction(S, TEXT("IA_CycleWeapon"), T::Axis1D);
	S->LeanLeft = MakeAction(S, TEXT("IA_LeanLeft"), T::Boolean);
	S->LeanRight = MakeAction(S, TEXT("IA_LeanRight"), T::Boolean);
	S->Interact = MakeAction(S, TEXT("IA_Interact"), T::Boolean);
	S->Drag = MakeAction(S, TEXT("IA_Drag"), T::Boolean);
	S->Bandage = MakeAction(S, TEXT("IA_Bandage"), T::Boolean);
	S->Tourniquet = MakeAction(S, TEXT("IA_Tourniquet"), T::Boolean);
	S->BuyMenu = MakeAction(S, TEXT("IA_BuyMenu"), T::Boolean);
	S->Scoreboard = MakeAction(S, TEXT("IA_Scoreboard"), T::Boolean);
	S->Pause = MakeAction(S, TEXT("IA_Pause"), T::Boolean);
	S->SpectateNext = MakeAction(S, TEXT("IA_SpectateNext"), T::Boolean);

	UInputMappingContext* C = NewObject<UInputMappingContext>(S, TEXT("IMC_NSC_Default"));
	S->Context = C;

	// WASD -> 2D (X = right, Y = forward)
	Map(C, S->Move, EKeys::W, false, true);
	Map(C, S->Move, EKeys::S, true, true);
	Map(C, S->Move, EKeys::D);
	Map(C, S->Move, EKeys::A, true);
	Map(C, S->Look, EKeys::Mouse2D);
	Map(C, S->Jump, EKeys::SpaceBar);
	Map(C, S->Sprint, EKeys::LeftShift);
	Map(C, S->Walk, EKeys::LeftAlt);
	Map(C, S->Crouch, EKeys::LeftControl);
	Map(C, S->Crouch, EKeys::C);
	Map(C, S->Fire, EKeys::LeftMouseButton);
	Map(C, S->Aim, EKeys::RightMouseButton);
	Map(C, S->Reload, EKeys::R);
	Map(C, S->SlotPrimary, EKeys::One);
	Map(C, S->SlotSecondary, EKeys::Two);
	Map(C, S->SlotSmoke, EKeys::Three);
	Map(C, S->SlotFlash, EKeys::Four);
	Map(C, S->SlotFrag, EKeys::Five);
	Map(C, S->CycleWeapon, EKeys::MouseWheelAxis, true);
	Map(C, S->LeanLeft, EKeys::Q);
	Map(C, S->LeanRight, EKeys::E);
	Map(C, S->Interact, EKeys::F);
	Map(C, S->Drag, EKeys::G);
	Map(C, S->Bandage, EKeys::X);
	Map(C, S->Tourniquet, EKeys::V);
	Map(C, S->BuyMenu, EKeys::B);
	Map(C, S->Scoreboard, EKeys::Tab);
	Map(C, S->Pause, EKeys::Escape);
	Map(C, S->Pause, EKeys::P);
	Map(C, S->SpectateNext, EKeys::LeftMouseButton);
	return S;
}
