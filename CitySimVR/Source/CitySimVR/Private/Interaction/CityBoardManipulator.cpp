#include "Interaction/CityBoardManipulator.h"

#include "Interaction/CityBoardPicking.h"

bool FCityBoardManipulator::SetDimensions(const FCityGridDimensions InDimensions)
{
	if (!InDimensions.IsValid())
		return false;
	if (Dimensions != InDimensions)
	{
		Dimensions = InDimensions;
		Reset();
	}
	return true;
}
double FCityBoardManipulator::GetMinimumScale() const
{
	return 100.0 / CityBoardPicking::GetLogicalBoardSize(Dimensions).X;
}
double FCityBoardManipulator::GetMaximumScale() const
{
	return 400.0 / CityBoardPicking::GetLogicalBoardSize(Dimensions).X;
}

FTransform FCityBoardPresentationPose::ToTransform() const
{
	return FTransform(FRotator(0.0, YawDegrees, 0.0), Location, FVector(UniformScale));
}

FCityBoardPresentationPose FCityBoardPresentationPose::FromTransform(const FTransform& Transform)
{
	FCityBoardPresentationPose Pose;
	Pose.Location = Transform.GetLocation();
	Pose.YawDegrees = Transform.Rotator().Yaw;
	Pose.UniformScale = Transform.GetScale3D().X;
	return Pose;
}

void FCityBoardManipulator::Reinitialize(const bool bLeftHeld, const bool bRightHeld, const bool bLeftTracked,
                                         const bool bRightTracked, const FVector& LeftPosition,
                                         const FVector& RightPosition, const FCityBoardPresentationPose& CurrentPose)
{
	Mode = ResolveMode(bLeftHeld, bRightHeld, bLeftTracked, bRightTracked);
	StartPose = CurrentPose;
	StartPose.UniformScale = FMath::Clamp(StartPose.UniformScale, GetMinimumScale(), GetMaximumScale());
	StartPose.Location.Z = FMath::Clamp(StartPose.Location.Z, MinimumHeightCm, MaximumHeightCm);

	if (Mode == ECityBoardGestureMode::OneHand)
	{
		bOneHandUsesLeft = bLeftHeld && bLeftTracked;
		StartSingleHandPosition = GetSingleHandPosition(LeftPosition, RightPosition);
	}
	else if (Mode == ECityBoardGestureMode::TwoHand)
	{
		StartMidpoint = (LeftPosition + RightPosition) * 0.5;
		StartHandVector = FVector2D(RightPosition.X - LeftPosition.X, RightPosition.Y - LeftPosition.Y);
		StartHandDistance = FMath::Max(FVector::Distance(LeftPosition, RightPosition), 1.0);
	}
}

bool FCityBoardManipulator::Update(const bool bLeftHeld, const bool bRightHeld, const bool bLeftTracked,
                                   const bool bRightTracked, const FVector& LeftPosition, const FVector& RightPosition,
                                   FCityBoardPresentationPose& OutPose)
{
	const ECityBoardGestureMode CurrentMode = ResolveMode(bLeftHeld, bRightHeld, bLeftTracked, bRightTracked);
	const bool bChangedSingleHand =
	    CurrentMode == ECityBoardGestureMode::OneHand && bOneHandUsesLeft != (bLeftHeld && bLeftTracked);
	if (CurrentMode != Mode || bChangedSingleHand)
	{
		Reinitialize(bLeftHeld, bRightHeld, bLeftTracked, bRightTracked, LeftPosition, RightPosition, OutPose);
		return false;
	}

	if (Mode == ECityBoardGestureMode::None)
	{
		return false;
	}

	if (Mode == ECityBoardGestureMode::OneHand)
	{
		OutPose = StartPose;
		OutPose.Location += GetSingleHandPosition(LeftPosition, RightPosition) - StartSingleHandPosition;
	}
	else
	{
		const FVector Midpoint = (LeftPosition + RightPosition) * 0.5;
		const FVector2D HandVector(RightPosition.X - LeftPosition.X, RightPosition.Y - LeftPosition.Y);
		const double CurrentDistance = FMath::Max(FVector::Distance(LeftPosition, RightPosition), 1.0);
		const double StartAngle = FMath::Atan2(StartHandVector.Y, StartHandVector.X);
		const double CurrentAngle = FMath::Atan2(HandVector.Y, HandVector.X);

		OutPose = StartPose;
		OutPose.YawDegrees =
		    FMath::UnwindDegrees(StartPose.YawDegrees + FMath::RadiansToDegrees(CurrentAngle - StartAngle));
		OutPose.UniformScale = FMath::Clamp(StartPose.UniformScale * CurrentDistance / StartHandDistance,
		                                    GetMinimumScale(), GetMaximumScale());
		// The actor origin is a terrain corner. Transform that offset about the
		// grabbed world-space midpoint, using the effective (clamped) scale ratio.
		// This keeps the same logical point between the controllers during yaw/zoom.
		const FQuat DeltaRotation(FRotator(0.0, OutPose.YawDegrees - StartPose.YawDegrees, 0.0));
		OutPose.Location = Midpoint + DeltaRotation.RotateVector((StartPose.Location - StartMidpoint) *
		                                                         (OutPose.UniformScale / StartPose.UniformScale));
	}

	OutPose.Location.Z = FMath::Clamp(OutPose.Location.Z, MinimumHeightCm, MaximumHeightCm);
	return true;
}

void FCityBoardManipulator::NudgeHeight(const double DeltaHeightCm, FCityBoardPresentationPose& InOutPose)
{
	InOutPose.Location.Z = FMath::Clamp(InOutPose.Location.Z + DeltaHeightCm, MinimumHeightCm, MaximumHeightCm);
	StartPose.Location.Z = InOutPose.Location.Z;
}

void FCityBoardManipulator::Reset()
{
	Mode = ECityBoardGestureMode::None;
}

FCityBoardPresentationPose FCityBoardManipulator::MakeRecenteredPose(
    const FVector& HeadLocation, const FVector& HeadForward, const double UniformScale, const double ForwardDistanceCm,
    const double BelowEyeHeightCm, const FCityGridDimensions Dimensions)
{
	FVector FlatForward(HeadForward.X, HeadForward.Y, 0.0);
	if (!FlatForward.Normalize())
	{
		FlatForward = FVector::ForwardVector;
	}

	FCityBoardPresentationPose Pose;
	const auto Size = CityBoardPicking::GetLogicalBoardSize(Dimensions.IsValid() ? Dimensions : FCityGridDimensions());
	Pose.UniformScale = FMath::Clamp(UniformScale, 100.0 / Size.X, 400.0 / Size.X);
	Pose.YawDegrees = FlatForward.Rotation().Yaw - 90.0;
	const FVector DesiredCenter(HeadLocation.X + FlatForward.X * ForwardDistanceCm,
	                            HeadLocation.Y + FlatForward.Y * ForwardDistanceCm,
	                            FMath::Clamp(HeadLocation.Z - BelowEyeHeightCm, MinimumHeightCm, MaximumHeightCm));
	const FQuat Rotation(FRotator(0.0, Pose.YawDegrees, 0.0));
	const FVector LocalCenterOffset =
	    Rotation.RotateVector(FVector(Size.X * 0.5 * Pose.UniformScale, Size.Y * 0.5 * Pose.UniformScale, 0.0));
	Pose.Location = DesiredCenter - LocalCenterOffset;
	return Pose;
}

ECityBoardGestureMode FCityBoardManipulator::ResolveMode(const bool bLeftHeld, const bool bRightHeld,
                                                         const bool bLeftTracked, const bool bRightTracked) const
{
	const bool bUsableLeft = bLeftHeld && bLeftTracked;
	const bool bUsableRight = bRightHeld && bRightTracked;
	if (bUsableLeft && bUsableRight)
	{
		return ECityBoardGestureMode::TwoHand;
	}
	return bUsableLeft || bUsableRight ? ECityBoardGestureMode::OneHand : ECityBoardGestureMode::None;
}

FVector FCityBoardManipulator::GetSingleHandPosition(const FVector& LeftPosition, const FVector& RightPosition) const
{
	return bOneHandUsesLeft ? LeftPosition : RightPosition;
}
