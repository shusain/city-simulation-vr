#pragma once
#include "CoreMinimal.h"

// Presentation-only camera; moving it never changes board coordinates or saved city state.
struct CITYSIMVR_API FCityDesktopCamera
{
	FVector Focus = FVector::ZeroVector;
	FVector Home = FVector::ZeroVector;
	double Yaw = 90;
	double Pitch = -55;
	double Distance = 260;
	double BoardWidth = 200;
	void Reset(const FVector& Center, double Width);
	void Orbit(const FVector2D& Delta);
	void Zoom(double Steps);
	void Pan(const FVector2D& Delta);
	void Drag(const FVector2D& MouseDelta);
	FVector Location() const;
	float NearClipPlane() const
	{
		return static_cast<float>(FMath::Clamp(Distance * .01, .001, 10.));
	}
	FRotator Rotation() const
	{
		return FRotator(Pitch, Yaw, 0);
	}
};
