#pragma once
#include "CoreMinimal.h"

// A held stick cannot repeat or change height on tool/lifecycle activation.
struct FCityRoadHeightControl
{
	int32 Update(const double Axis, const bool bEnabled)
	{
		if (!bEnabled || !FMath::IsFinite(Axis))
		{
			bArmed = false;
			return 0;
		}
		if (FMath::Abs(Axis) < .25)
		{
			bArmed = true;
			return 0;
		}
		if (!bArmed || FMath::Abs(Axis) < .7)
			return 0;
		bArmed = false;
		return Axis > 0 ? 1 : -1;
	}

private:
	bool bArmed = false;
};
