#include "Presentation/CityValueControl.h"

FCityBoundedValueControl::FCityBoundedValueControl(const int32 InMinimum, const int32 InMaximum, const int32 InStep)
    : Minimum(FMath::Min(InMinimum, InMaximum)), Maximum(FMath::Max(InMinimum, InMaximum)), Step(FMath::Max(1, InStep))
{
}

void FCityBoundedValueControl::Begin(const int32 CurrentValue)
{
	OriginalValue = SnapAndClamp(CurrentValue);
	PendingValue = OriginalValue;
	bEditing = true;
}

void FCityBoundedValueControl::StepBy(const int32 StepCount)
{
	if (bEditing)
	{
		PendingValue = SnapAndClamp(static_cast<int64>(PendingValue) + static_cast<int64>(StepCount) * Step);
	}
}

void FCityBoundedValueControl::SetFromTrack(const double NormalizedPosition)
{
	if (!bEditing || !FMath::IsFinite(NormalizedPosition))
	{
		return;
	}
	const double Clamped = FMath::Clamp(NormalizedPosition, 0.0, 1.0);
	PendingValue = SnapAndClamp(FMath::RoundToInt64(Minimum + Clamped * (Maximum - Minimum)));
}

int32 FCityBoundedValueControl::Commit()
{
	bEditing = false;
	OriginalValue = PendingValue;
	return PendingValue;
}

void FCityBoundedValueControl::Cancel()
{
	PendingValue = OriginalValue;
	bEditing = false;
}

double FCityBoundedValueControl::GetNormalizedPosition() const
{
	return Maximum > Minimum ? static_cast<double>(PendingValue - Minimum) / static_cast<double>(Maximum - Minimum)
	                         : 0.0;
}

int32 FCityBoundedValueControl::SnapAndClamp(const int64 Value) const
{
	const int64 Clamped = FMath::Clamp<int64>(Value, Minimum, Maximum);
	const int64 Offset = Clamped - Minimum;
	const int64 Snapped = Minimum + ((Offset + Step / 2) / Step) * Step;
	return static_cast<int32>(FMath::Clamp<int64>(Snapped, Minimum, Maximum));
}

double FCityHorizontalSliderPresentation::NormalizedFromLocalY(const double LocalY, const double CenterY,
                                                               const double HalfWidth)
{
	const double SafeHalfWidth = FMath::Max(FMath::Abs(HalfWidth), UE_DOUBLE_SMALL_NUMBER);
	return FMath::Clamp((CenterY + SafeHalfWidth - LocalY) / (SafeHalfWidth * 2.0), 0.0, 1.0);
}

double FCityHorizontalSliderPresentation::LocalYFromNormalized(const double NormalizedPosition, const double CenterY,
                                                               const double HalfWidth)
{
	const double SafeHalfWidth = FMath::Max(FMath::Abs(HalfWidth), UE_DOUBLE_SMALL_NUMBER);
	return CenterY + SafeHalfWidth - FMath::Clamp(NormalizedPosition, 0.0, 1.0) * SafeHalfWidth * 2.0;
}
