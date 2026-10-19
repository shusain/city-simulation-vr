#pragma once

#include "CoreMinimal.h"

enum class ECityControllerHand : uint8
{
	Left,
	Right
};

enum class ECityTriggerRoute : uint8
{
	None,
	ConfirmTool,
	SelectPalette
};

class CITYSIMVR_API FCityToolInputRouter
{
public:
	static ECityControllerHand OtherHand(ECityControllerHand Hand);
	static ECityTriggerRoute RouteTrigger(ECityControllerHand PressedHand, ECityControllerHand DominantHand,
	                                      bool bPaletteOpen, bool bPaletteHit);
};

class CITYSIMVR_API FCityLifecycleInputGate
{
public:
	void Suspend()
	{
		bAwaitingRelease = true;
	}
	void Update(bool bAnyInputPressed)
	{
		if (bAwaitingRelease && !bAnyInputPressed)
		{
			bAwaitingRelease = false;
		}
	}
	bool CanAcceptInput() const
	{
		return !bAwaitingRelease;
	}

private:
	bool bAwaitingRelease = false;
};
