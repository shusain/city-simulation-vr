#include "Simulation/CityNeedsSimulation.h"

#include "Simulation/CityDevelopmentSimulation.h"
#include "Simulation/CityEconomySimulation.h"
#include "Simulation/CityGrowthSimulation.h"

namespace
{
	int32 Bounded(const int64 Value)
	{
		return static_cast<int32>(FMath::Clamp<int64>(Value, 0, MAX_int32));
	}

	FCityDensityDemand BuildMarket(const FCityPlacementModel& City, const FCityDevelopmentSnapshot& Development,
	                               const FCityBuildingDefinition* Definition, const int32 Wanted,
	                               const int32 EmploymentPercent, const int32 SkilledPercent)
	{
		FCityDensityDemand Market;
		if (!Definition)
			return Market;
		const auto Type = Definition->DevelopmentDemandType;
		for (const auto Density : {ECityDensityLevel::Low, ECityDensityLevel::Medium, ECityDensityLevel::High})
			if (const auto* Level = Definition->FindDensityLevel(Density))
				Market.BuildingCapacity[static_cast<int32>(Density)] = FCityDensityDemand::Capacity(*Level, Type);
		for (const auto& Pair : City.GetEntities())
		{
			if (Pair.Value.DefinitionId != Definition->DefinitionId)
				continue;
			const auto* State = Development.Find(Pair.Key);
			const auto Density = State ? State->DensityLevel : ECityDensityLevel::Low;
			const auto* Level = Definition->FindDensityLevel(Density);
			if (Level)
			{
				const int32 Index = static_cast<int32>(Density);
				Market.Reserved[Index] =
				    Bounded(static_cast<int64>(Market.Reserved[Index]) + FCityDensityDemand::Capacity(*Level, Type));
			}
		}
		// Small markets cannot sustain fractional apartment blocks or large firms.
		// Round denser targets down to whole buildings; the remainder stays low.
		const int32 MediumPercent = Wanted >= 40 ? 20 + SkilledPercent / 10 + EmploymentPercent / 20 : 0;
		const int32 HighPercent = Wanted >= 200 ? (5 + SkilledPercent / 10) * EmploymentPercent / 100 : 0;
		for (const auto Density : {ECityDensityLevel::Medium, ECityDensityLevel::High})
		{
			const auto* Level = Definition->FindDensityLevel(Density);
			const int32 Capacity = Level ? FCityDensityDemand::Capacity(*Level, Type) : 0;
			const int32 Percent = Density == ECityDensityLevel::Medium ? MediumPercent : HighPercent;
			const int32 Share = Bounded(static_cast<int64>(Wanted) * Percent / 100);
			Market.Desired[static_cast<int32>(Density)] = Capacity > 0 ? Share / Capacity * Capacity : 0;
		}
		Market.Desired[0] = FMath::Max(0, Wanted - Market.Desired[1] - Market.Desired[2]);
		const int32 LowCapacity = Market.BuildingCapacity[0];
		if (LowCapacity > 0)
		{
			Market.Desired[0] =
			    Bounded(static_cast<int64>(FMath::DivideAndRoundUp(Market.Desired[0], LowCapacity)) * LowCapacity);
			int64 ExistingLots = 0, PreferredLots = 0;
			for (int32 Index = 0; Index < 3; ++Index)
				if (Market.BuildingCapacity[Index] > 0)
				{
					ExistingLots += Market.Reserved[Index] / Market.BuildingCapacity[Index];
					PreferredLots += FMath::DivideAndRoundUp(Market.Desired[Index], Market.BuildingCapacity[Index]);
				}
			// Existing lots are retained. Fit the preferred denser mix into real
			// additional demand instead of inflating population to replace low lots.
			const int64 QuantizedWanted = static_cast<int64>(Market.Desired[0]) + Market.Desired[1] + Market.Desired[2];
			int64 UpgradeBudget =
			    FMath::Max<int64>(0, QuantizedWanted - FMath::Max(ExistingLots, PreferredLots) * LowCapacity);
			for (const int32 Index : {2, 1})
			{
				const int32 Extra = Market.BuildingCapacity[Index] - LowCapacity;
				if (Extra <= 0)
				{
					Market.Desired[Index] = 0;
					continue;
				}
				const int32 Lots = static_cast<int32>(
				    FMath::Min<int64>(Market.Desired[Index] / Market.BuildingCapacity[Index], UpgradeBudget / Extra));
				Market.Desired[Index] = Lots * Market.BuildingCapacity[Index];
				UpgradeBudget -= static_cast<int64>(Lots) * Extra;
			}
			Market.Desired[0] =
			    Bounded(static_cast<int64>(FMath::DivideAndRoundUp(
			                FMath::Max(0, Wanted - Market.Desired[1] - Market.Desired[2]), LowCapacity)) *
			            LowCapacity);
		}
		return Market;
	}
} // namespace

int32 FCityDensityDemand::Capacity(const FCityBuildingLevelDefinition& Level, const ECityDevelopmentDemandType Type)
{
	return FMath::Max(0,
	                  Type == ECityDevelopmentDemandType::Residential ? Level.ResidentsProvided : Level.JobsProvided);
}

int32 FCityDensityDemand::AdditionalCapacity() const
{
	return Bounded(static_cast<int64>(Desired[0]) + Desired[1] + Desired[2] - Reserved[0] - Reserved[1] - Reserved[2]);
}

int32 FCityDensityDemand::Balance(const ECityDensityLevel Level) const
{
	const int32 Index = static_cast<int32>(Level);
	return Desired[Index] - Reserved[Index];
}

int32 FCityDensityDemand::NewLowDemand() const
{
	int64 MissingLots = 0;
	for (int32 Index = 0; Index < 3; ++Index)
		if (BuildingCapacity[Index] > 0)
			MissingLots += FMath::DivideAndRoundUp(Desired[Index], BuildingCapacity[Index]) -
			               Reserved[Index] / BuildingCapacity[Index];
	// Denser buildings start as low lots too. Reserve only the eventual missing
	// lot count, within the remaining citywide capacity, for those precursors.
	const int32 Precursor = Bounded(FMath::Max<int64>(0, MissingLots) * BuildingCapacity[0]);
	return FMath::Min(FMath::Max(FMath::Max(0, Balance(ECityDensityLevel::Low)), Precursor), AdditionalCapacity());
}

int32 FCityDensityDemand::UpgradeDemand(const ECityDensityLevel Target) const
{
	if (Target == ECityDensityLevel::Medium && BuildingCapacity[2] > 0)
	{
		const int32 HighLots = FMath::Max(0, Balance(ECityDensityLevel::High)) / BuildingCapacity[2];
		return Bounded(static_cast<int64>(Desired[1]) + static_cast<int64>(HighLots) * BuildingCapacity[1]) -
		       Reserved[1];
	}
	return Balance(Target);
}

bool FCityDensityDemand::CanUpgrade(const FCityBuildingLevelDefinition& Current,
                                    const FCityBuildingLevelDefinition& Target,
                                    const ECityDevelopmentDemandType Type) const
{
	const int32 CurrentCapacity = Capacity(Current, Type);
	const int32 TargetCapacity = Capacity(Target, Type);
	return TargetCapacity > CurrentCapacity && UpgradeDemand(Target.Level) >= TargetCapacity &&
	       Reserved[static_cast<int32>(Current.Level)] - CurrentCapacity >=
	           Desired[static_cast<int32>(Current.Level)] &&
	       AdditionalCapacity() >= TargetCapacity - CurrentCapacity;
}

const FCityDensityDemand& FCityNeedsSnapshot::DensityFor(const ECityDevelopmentDemandType Type) const
{
	return Type == ECityDevelopmentDemandType::Commercial   ? CommercialDensity
	       : Type == ECityDevelopmentDemandType::Industrial ? IndustrialDensity
	                                                        : ResidentialDensity;
}

void FCityNeedsEvaluator::BuildDensityDemand(FCityNeedsSnapshot& Needs, const FCityPlacementModel& City,
                                             const FCityServiceSnapshot& Services,
                                             const FCityDevelopmentSnapshot& Development,
                                             const FCityEconomyTuning& Tuning, const FCityBuildingDefinition& Home,
                                             const FCityBuildingDefinition* Shop,
                                             const FCityBuildingDefinition* Industry,
                                             const FCityPopulationSnapshot* Population)
{
	Needs.bHasDensityDemand = true;
	Needs.DensityEmploymentPercent =
	    Services.Residents > 0 ? Bounded(static_cast<int64>(Needs.EmployedJobs) * 100 / Services.Residents) : 0;
	Needs.DensityEmploymentPercent = FMath::Min(100, Needs.DensityEmploymentPercent);
	const auto& Education = Needs.WorkforceEducation;
	Needs.DensitySkilledPercent =
	    Education.Total() > 0
	        ? Bounded(static_cast<int64>(Education.Secondary + Education.Higher) * 100 / Education.Total())
	        : 0;
	const int32 Occupancy = FMath::Clamp(Tuning.PrivateJobTargetOccupancyPercent, 1, 100);
	const int32 ExpansionPopulation =
	    Bounded((static_cast<int64>(Services.Residents) * 100 + Occupancy - 1) / Occupancy);
	// Planned employers attract households before their deliveries finish, just
	// as pending homes reserve supply. Bound this signal to actual residents so
	// a huge disconnected job stock cannot cause an immediate building rush.
	const int32 HousingWanted = Bounded(
	    static_cast<int64>(FMath::Max(Needs.JobCapacity, FMath::Min(Needs.BuiltJobCapacity, ExpansionPopulation))) +
	    FMath::Max(1, Tuning.ResidentsPerOccupiedHome));
	int32 PublicWorkers = 0;
	if (Population)
		for (const auto& Citizen : Population->Citizens)
			if (!Citizen.IsDetained() && Services.PublicJobs.Contains(Citizen.WorkplaceEntityId))
				++PublicWorkers;
	// Vacant/pending homes also signal near-term labor to new employers. Limit
	// that lookahead to the same expansion allowance: a huge old housing surplus
	// must not immediately create jobs for every empty or disconnected home.
	// With no residents, only the starter home bootstraps the market.
	const int32 PotentialResidents =
	    Services.Residents > 0
	        ? FMath::Max(Services.Residents, FMath::Min(Services.BuiltHousingCapacity, ExpansionPopulation))
	        : 0;
	const int32 PrivateWorkers = FMath::Max(0, PotentialResidents - Services.DetainedResidents - PublicWorkers);
	const auto BusinessTarget = [&Needs, PrivateWorkers, &Tuning](const FCityBuildingDefinition* Definition)
	{
		const auto* Low = Definition ? Definition->FindDensityLevel(ECityDensityLevel::Low) : nullptr;
		const int32 GoodsJobs = Low && Low->EconomicOutput > 0
		                            ? Bounded((static_cast<int64>(Needs.HouseholdGoodsDemand) * Low->JobsProvided +
		                                       Low->EconomicOutput - 1) /
		                                      Low->EconomicOutput)
		                            : 0;
		// A closed market with exactly one job per worker reaches a permanent
		// equilibrium: no new employers, therefore no jobs to attract households.
		// Reserve a bounded share for expansion/export businesses. Count existing
		// and pending capacity below so pauses/deliveries cannot multiply it.
		const int32 Occupancy = FMath::Clamp(Tuning.PrivateJobTargetOccupancyPercent, 1, 100);
		const int32 JobTarget = Bounded((static_cast<int64>(PrivateWorkers) * 100 + Occupancy - 1) / Occupancy);
		return FMath::Max(FMath::DivideAndRoundUp(JobTarget, 2), GoodsJobs);
	};
	Needs.ResidentialDensity = BuildMarket(City, Development, &Home, HousingWanted, Needs.DensityEmploymentPercent,
	                                       Needs.DensitySkilledPercent);
	Needs.CommercialDensity = BuildMarket(City, Development, Shop, BusinessTarget(Shop), Needs.DensityEmploymentPercent,
	                                      Needs.DensitySkilledPercent);
	Needs.IndustrialDensity = BuildMarket(City, Development, Industry, BusinessTarget(Industry),
	                                      Needs.DensityEmploymentPercent, Needs.DensitySkilledPercent);
	Needs.bStarterHousingDemand =
	    Needs.JobCapacity == 0 && Services.Residents == 0 && Needs.ResidentialDensity.NewLowDemand() > 0;
}
