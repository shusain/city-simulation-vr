#pragma once

#include "CoreMinimal.h"
#include "Presentation/CityParkMotion.h"

class UCityBuildingCatalog;
struct FCityServiceSnapshot;
class FCityPlacementModel;

enum class ECityParkActivityProp : uint8
{
	Visitor,
	Basketball,
	FieldBall,
	Skateboard,
	Bat,
	Baseball
};

struct FCityParkActivitySample
{
	uint64 EntityId = 0;
	ECityParkActivityProp Prop = ECityParkActivityProp::Visitor;
	// Authored Blender-local coordinates, in centimeters above the park origin.
	FVector Position = FVector::ZeroVector;
	FRotator Rotation = FRotator::ZeroRotator;
	FVector Scale = FVector::OneVector;
	ECityParkMotion Motion = ECityParkMotion::None;
	float AnimationSeconds = 0;
	bool bNativeSpace = false;
};

// Bounded cosmetic samples. Never create citizens, trips, attendance or rewards.
class CITYSIMVR_API FCityParkActivity
{
public:
	static FTransform ToImportedMeshSpace(const FCityParkActivitySample& Sample);
	static constexpr int32 MaximumParks = 16;
	static constexpr int32 MaximumSamples = MaximumParks * 9;
	static void Build(const FCityPlacementModel& City, const FCityServiceSnapshot& Services,
	                  const UCityBuildingCatalog* Catalog, double Seconds, float Daylight,
	                  TArray<FCityParkActivitySample>& OutSamples);
};
