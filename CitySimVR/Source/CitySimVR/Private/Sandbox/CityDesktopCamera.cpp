#include "Sandbox/CityDesktopCamera.h"

void FCityDesktopCamera::Reset(const FVector& Center, const double Width)
{
	Home = Focus = Center;
	BoardWidth = FMath::Max(1.0, Width);
	Distance = BoardWidth * 1.3;
	Yaw = 90;
	Pitch = -55;
}
void FCityDesktopCamera::Orbit(const FVector2D& Delta)
{
	Yaw = FMath::UnwindDegrees(Yaw + Delta.X);
	Pitch = FMath::Clamp(Pitch + Delta.Y, -85.0, -20.0);
}
void FCityDesktopCamera::Zoom(const double Steps)
{
	Distance = FMath::Clamp(Distance * FMath::Pow(.85, Steps), BoardWidth * .005, BoardWidth * 4.0);
}
void FCityDesktopCamera::Pan(const FVector2D& Delta)
{
	const FRotationMatrix Axes(FRotator(0, Yaw, 0));
	Focus += (Axes.GetUnitAxis(EAxis::Y) * Delta.X + Axes.GetUnitAxis(EAxis::X) * Delta.Y) * Distance;
	Focus.X = FMath::Clamp(Focus.X, Home.X - BoardWidth, Home.X + BoardWidth);
	Focus.Y = FMath::Clamp(Focus.Y, Home.Y - BoardWidth, Home.Y + BoardWidth);
	Focus.Z = Home.Z;
}
void FCityDesktopCamera::Drag(const FVector2D& MouseDelta)
{
	// Unreal's mouse Y delta is positive upward. Move the board with the drag
	// on both screen axes, independently of the camera's current orbit yaw.
	Pan(-MouseDelta * .006);
}
FVector FCityDesktopCamera::Location() const
{
	return Focus - Rotation().Vector() * Distance;
}
