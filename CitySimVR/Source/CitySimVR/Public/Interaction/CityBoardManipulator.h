#pragma once

#include "CoreMinimal.h"
#include "Simulation/CityGrid.h"

enum class ECityBoardGestureMode : uint8
{
	None,
	OneHand,
	TwoHand
};

struct FCityBoardPresentationPose
{
	FVector Location = FVector::ZeroVector;
	double YawDegrees = 0.0;
	double UniformScale = 1.0;

	FTransform ToTransform() const;
	static FCityBoardPresentationPose FromTransform(const FTransform& Transform);
};

class CITYSIMVR_API FCityBoardManipulator
{
public:
	static constexpr double MinimumScale = 100.0 / (64.0 * 800.0);
	static constexpr double MaximumScale = 400.0 / (64.0 * 800.0);
	static constexpr double MinimumHeightCm = 35.0;
	static constexpr double MaximumHeightCm = 180.0;
	bool SetDimensions(FCityGridDimensions InDimensions);
	double GetMinimumScale() const;
	double GetMaximumScale() const;

	void Reinitialize(bool bLeftHeld, bool bRightHeld, bool bLeftTracked, bool bRightTracked,
	                  const FVector& LeftPosition, const FVector& RightPosition,
	                  const FCityBoardPresentationPose& CurrentPose);

	bool Update(bool bLeftHeld, bool bRightHeld, bool bLeftTracked, bool bRightTracked, const FVector& LeftPosition,
	            const FVector& RightPosition, FCityBoardPresentationPose& OutPose);

	void NudgeHeight(double DeltaHeightCm, FCityBoardPresentationPose& InOutPose);
	void Reset();
	ECityBoardGestureMode GetMode() const
	{
		return Mode;
	}

	static FCityBoardPresentationPose MakeRecenteredPose(const FVector& HeadLocation, const FVector& HeadForward,
	                                                     double UniformScale, double ForwardDistanceCm = 110.0,
	                                                     double BelowEyeHeightCm = 55.0,
	                                                     FCityGridDimensions Dimensions = FCityGridDimensions());

private:
	FCityGridDimensions Dimensions;
	ECityBoardGestureMode ResolveMode(bool bLeftHeld, bool bRightHeld, bool bLeftTracked, bool bRightTracked) const;
	FVector GetSingleHandPosition(const FVector& LeftPosition, const FVector& RightPosition) const;

	ECityBoardGestureMode Mode = ECityBoardGestureMode::None;
	bool bOneHandUsesLeft = false;
	FCityBoardPresentationPose StartPose;
	FVector StartSingleHandPosition = FVector::ZeroVector;
	FVector StartMidpoint = FVector::ZeroVector;
	FVector2D StartHandVector = FVector2D::ZeroVector;
	double StartHandDistance = 1.0;
};
