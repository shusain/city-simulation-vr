#pragma once
#include "CoreMinimal.h"
struct FCityPopulationSnapshot;
struct FCityServiceSnapshot;
class FCityPlacementModel;
enum class ECityHousingStatus : uint8;

struct FCityHousingAdmissionPlan
{
	int32 TargetResidents = 0;
	TSet<uint64> ReservedHomes;
};

class CITYSIMVR_API FCityHousingMigration
{
public:
	static constexpr int32 CooldownMonths = 3;
	static constexpr int32 DepartureMonths = 6;
	static constexpr int32 MinimumImprovement = 10;
	static FCityHousingAdmissionPlan AdmissionPlan(const FCityPopulationSnapshot& Population,
	                                               const FCityServiceSnapshot& Services);
	static bool Validate(const FCityPopulationSnapshot& Population, FString& Error,
	                     const FCityPlacementModel* City = nullptr);
	static const TCHAR* Describe(ECityHousingStatus Status);
};
