#pragma once
#include "CoreMinimal.h"

class FCityPlacementModel;
class UCityBuildingCatalog;
struct FCityServiceSnapshot;
struct FCityPopulationSnapshot;
struct FCityDevelopmentSnapshot;
struct FCityEnvironmentSnapshot;

// Capabilities are derived from operating facilities, never banked research points.
class CITYSIMVR_API FCityTechnology
{
  public:
	static bool IsFacility(FName Id);
	static TArray<FName> FacilityIds();
	static FString Review(FName Id, const FCityServiceSnapshot& Services,
	                      const UCityBuildingCatalog* Catalog);
	static void Evaluate(const FCityPlacementModel& City, FCityServiceSnapshot& Services,
	                     const UCityBuildingCatalog* Catalog, const FCityPopulationSnapshot* Population,
	                     const FCityDevelopmentSnapshot* Development,
	                     const FCityEnvironmentSnapshot* Environment);
};
