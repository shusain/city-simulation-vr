#pragma once
#include "Simulation/CityBuildingDefinition.h"
#include "Simulation/CityPopulationSimulation.h"
#include "Simulation/CityEnvironmentSimulation.h"

// Qualified staffing and paid payroll both limit specialized utility output.
inline int32 CityUtilityStaffPercent(const FCityPopulationSnapshot* Population, uint64 Id, int32 Jobs, int32 Education)
{
	if (!Population)
		return 100;
	int32 Qualified = 0;
	for (const auto& Citizen : Population->Citizens)
		if (!Citizen.IsDetained() && Citizen.WorkplaceEntityId == Id &&
		    static_cast<int32>(Citizen.Education) >= Education)
			++Qualified;
	const int32 Percent = Jobs > 0 ? FMath::Min(100, Qualified * 100 / Jobs) : 0;
	const auto* Employer =
	    Population->PublicEmployers.FindByPredicate([Id](const auto& E) { return E.EntityId == Id; });
	return Employer && Employer->PayrollDue > 0
	           ? static_cast<int32>(Percent * Employer->PayrollPaid / Employer->PayrollDue)
	           : Percent;
}

inline int32 CityUtilityOutputPercent(const FCityBuildingDefinition& Definition, int32 StaffPercent)
{
	return Definition.UtilityMinimumOutputPercent +
	       (100 - Definition.UtilityMinimumOutputPercent) * FMath::Clamp(StaffPercent, 0, 100) / 100;
}

// Turbines yaw into the wind; direction affects presentation rather than efficiency.
inline int32 CityUtilityWeatherPercent(FName Id, const FCityEnvironmentSnapshot* Weather)
{
	if (!Weather)
		return 100;
	if (Id == TEXT("WindPower"))
		return Weather->WindStrength == 0 ? 0 : Weather->WindStrength == 1 ? 75 : 100;
	if (Id == TEXT("SolarPower") && Weather->bPrecipitation)
		return Weather->WindStrength == 2 ? 35 : 65;
	return 100;
}
