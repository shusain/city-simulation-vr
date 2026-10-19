#pragma once
#include "CoreMinimal.h"

// Measured residential thresholds, retained porch/deck edges and the reserved
// approach shape. Structural bodies remain separate from terrain paving.
struct CITYSIMVR_API FCityResidentialEntrance
{
	const TCHAR* OriginalMesh;
	const TCHAR* TargetMesh;
	const TCHAR* SupportMesh;
	FVector EntryCm;
	double LandingCm;
	double GroundReserveCm;
	double WidthCm = 130.;
	bool bFrontageFlight = false;
	double DoorWidthCm = 130.;
	// Shorten the frontage route when a canopy post occupies the outer edge.
	double FrontageHalfSpanCm = 800.;
};

namespace CityResidentialEntrances
{
	CITYSIMVR_API TConstArrayView<FCityResidentialEntrance> All();
	CITYSIMVR_API const FCityResidentialEntrance* Find(const FString& MeshName);
	CITYSIMVR_API FString ObjectPath(const FCityResidentialEntrance& Entry);
}
