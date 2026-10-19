#pragma once

#include "CoreMinimal.h"

// Pure, presentation-independent edit model shared by VR controls and desktop tests.
class CITYSIMVR_API FCityBoundedValueControl
{
public:
	FCityBoundedValueControl(int32 InMinimum, int32 InMaximum, int32 InStep);

	void Begin(int32 CurrentValue);
	void StepBy(int32 StepCount);
	void SetFromTrack(double NormalizedPosition);
	int32 Commit();
	void Cancel();

	bool IsEditing() const
	{
		return bEditing;
	}
	int32 GetPendingValue() const
	{
		return PendingValue;
	}
	int32 GetOriginalValue() const
	{
		return OriginalValue;
	}
	int32 GetMinimum() const
	{
		return Minimum;
	}
	int32 GetMaximum() const
	{
		return Maximum;
	}
	double GetNormalizedPosition() const;

private:
	int32 SnapAndClamp(int64 Value) const;

	int32 Minimum = 0;
	int32 Maximum = 0;
	int32 Step = 1;
	int32 OriginalValue = 0;
	int32 PendingValue = 0;
	bool bEditing = false;
};

// Converts between a viewer-facing horizontal track and palette-local Y,
// where +Y appears on the viewer's left and -Y appears on the right.
class CITYSIMVR_API FCityHorizontalSliderPresentation
{
public:
	static double NormalizedFromLocalY(double LocalY, double CenterY, double HalfWidth);
	static double LocalYFromNormalized(double NormalizedPosition, double CenterY, double HalfWidth);
};
