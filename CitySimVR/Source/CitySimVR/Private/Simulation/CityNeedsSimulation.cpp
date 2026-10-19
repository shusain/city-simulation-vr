#include "Simulation/CityNeedsSimulation.h"
#include "Performance/CityEvaluationTiming.h"

#include "Simulation/CityBuildingDefinition.h"
#include "Simulation/CityDevelopmentSimulation.h"
#include "Simulation/CityEconomySimulation.h"
#include "Simulation/CityEnvironmentSimulation.h"
#include "Simulation/CityGrowthSimulation.h"
#include "Simulation/CityPopulationSimulation.h"

namespace
{
	int32 SaturatingMultiply(const int32 Left, const int32 Right)
	{
		if (Left <= 0 || Right <= 0)
		{
			return 0;
		}
		return static_cast<int32>(FMath::Min<int64>(MAX_int32, static_cast<int64>(Left) * Right));
	}

	bool IsUniformEmptyZone(const FCityPlacementModel& City, const FCityCellCoord& Anchor, const ECityZoneType Zone)
	{
		for (int32 Y = Anchor.Y; Y < Anchor.Y + 2; ++Y)
		{
			for (int32 X = Anchor.X; X < Anchor.X + 2; ++X)
			{
				const FCityCellCoord Cell(X, Y);
				if (City.GetZone(Cell) != Zone || City.GetGrid().GetOccupant(Cell) != FCityGrid::EmptyEntityId)
				{
					return false;
				}
			}
		}
		return true;
	}

	ECityDemandConstraint OperationFailureToConstraint(const ECityOperationFailure Failure)
	{
		switch (Failure)
		{
		case ECityOperationFailure::NoRoad:
			return ECityDemandConstraint::NoRoad;
		case ECityOperationFailure::NoService:
			return ECityDemandConstraint::NoService;
		case ECityOperationFailure::LowWaterPressure:
			return ECityDemandConstraint::NoService;
		case ECityOperationFailure::ServiceCapacity:
			return ECityDemandConstraint::ServiceCapacity;
		case ECityOperationFailure::NoPower:
			return ECityDemandConstraint::NoPower;
		case ECityOperationFailure::PowerCapacity:
			return ECityDemandConstraint::PowerCapacity;
		default:
			return ECityDemandConstraint::None;
		}
	}

	struct FZoneOpportunity
	{
		int32 Zoned = 0;
		int32 Eligible = 0;
		int32 Affordable = 0;
		int32 TerrainBlocked = 0;
		ECityDemandConstraint FirstInfrastructureConstraint = ECityDemandConstraint::None;
	};

	FZoneOpportunity EvaluateZone(const FCityPlacementModel& City, const FCityServiceSnapshot& Services,
	                              const FCityEconomyTuning& Tuning, const ECityZoneType Zone,
	                              const FCityBuildingDefinition* Definition,
	                              const FCityEnvironmentSnapshot* Environment)
	{
		FZoneOpportunity Result;
		if (!Definition)
		{
			Result.FirstInfrastructureConstraint = ECityDemandConstraint::MissingDefinition;
			return Result;
		}
		const FIntPoint GrowthSize(2, 2);
		FCityBuildingDefinition GrowthDefinition = *Definition;
		GrowthDefinition.Footprint = FCityCellFootprint(2, 2);
		for (int32 Y = 0; Y <= City.GetGrid().GetDimensions().GetHeight() - GrowthSize.Y; ++Y)
		{
			for (int32 X = 0; X <= City.GetGrid().GetDimensions().GetWidth() - GrowthSize.X; ++X)
			{
				const FCityCellCoord Anchor(X, Y);
				if (!IsUniformEmptyZone(City, Anchor, Zone))
				{
					continue;
				}
				++Result.Zoned;
				const ECityOperationFailure OperationFailure = FCityServiceEvaluator::EvaluateCandidate(
				    City, Services, Tuning, Definition->DefinitionId, Anchor, GrowthSize);
				if (OperationFailure != ECityOperationFailure::None)
				{
					if (Result.FirstInfrastructureConstraint == ECityDemandConstraint::None)
					{
						Result.FirstInfrastructureConstraint = OperationFailureToConstraint(OperationFailure);
					}
					continue;
				}
				if (Zone == ECityZoneType::Residential && Environment)
				{
					const FCityDesirabilityResult Desirability =
					    FCityEnvironmentSimulation::EvaluateResidentialDesirability(City, Services, *Environment,
					                                                                Anchor, GrowthSize, Tuning);
					if (!Desirability.IsResidentialEligible(Tuning.MinimumResidentialDesirability))
					{
						if (Result.FirstInfrastructureConstraint == ECityDemandConstraint::None)
						{
							Result.FirstInfrastructureConstraint = Desirability.PollutionExposure > 0
							                                           ? ECityDemandConstraint::PollutionPressure
							                                           : ECityDemandConstraint::LowDesirability;
						}
						continue;
					}
				}
				++Result.Eligible;
				const FCityPlacementPreview Preview = City.BeginPreview(GrowthDefinition, Anchor);
				switch (City.ValidatePlacement(Preview, GrowthDefinition, ECityConstructionFunding::PrivateDeveloper)
				            .Failure)
				{
				case ECityPlacementFailure::None:
					++Result.Affordable;
					break;
				case ECityPlacementFailure::WaterOrShore:
				case ECityPlacementFailure::FoundationRelief:
					++Result.TerrainBlocked;
					break;
				default:
					break;
				}
			}
		}
		return Result;
	}

	ECityDemandConstraint ResolveConstraint(const int32 Demand, const FZoneOpportunity& Opportunity,
	                                        const bool bOperatingShortfall)
	{
		if (Demand <= 0)
		{
			return ECityDemandConstraint::LowDemand;
		}
		if (bOperatingShortfall)
		{
			return ECityDemandConstraint::OperatingShortfall;
		}
		if (Opportunity.FirstInfrastructureConstraint == ECityDemandConstraint::MissingDefinition)
		{
			return ECityDemandConstraint::MissingDefinition;
		}
		if (Opportunity.Zoned <= 0)
		{
			return ECityDemandConstraint::NoZonedFootprint;
		}
		if (Opportunity.Eligible <= 0)
		{
			return Opportunity.FirstInfrastructureConstraint;
		}
		if (Opportunity.Affordable <= 0)
		{
			// A terrain-rejected lot must not hide a more actionable network or
			// capacity failure affecting the remaining zoned lots. This is common
			// on a large city: some parcels are too uneven while the usable ones
			// are beyond water-treatment road reach.
			if (Opportunity.FirstInfrastructureConstraint != ECityDemandConstraint::None)
			{
				return Opportunity.FirstInfrastructureConstraint;
			}
			if (Opportunity.TerrainBlocked > 0)
			{
				return ECityDemandConstraint::InvalidTerrain;
			}
			return ECityDemandConstraint::NoZonedFootprint;
		}
		return ECityDemandConstraint::None;
	}
} // namespace

FCityNeedsSnapshot FCityNeedsEvaluator::BuildSnapshot(
    const FCityPlacementModel& City, const FCityServiceSnapshot& Services, const FCityEconomyTuning& Tuning,
    const FCityBuildingDefinition* HomeDefinition, const FCityBuildingDefinition* ShopDefinition,
    const bool bOperatingShortfall, const FCityBuildingDefinition* IndustryDefinition,
    const FCityEnvironmentSnapshot* Environment, const int32 ResidentialTaxBasisPoints,
    const int32 CommercialTaxBasisPoints, const FCityPopulationSnapshot* Population,
    const int32 IndustrialTaxBasisPoints, const FCityDevelopmentSnapshot* Development,
    FCityNeedsEvaluationTiming* Timing, const bool bPreparePopulationQueries)
{
	double PhaseStart = Timing ? FPlatformTime::Seconds() : 0;
	FCityNeedsSnapshot Snapshot;
	Snapshot.bHasWorkforceEducation = Population != nullptr;
	if (Population)
		Snapshot.WorkforceEducation = Population->BuildWorkforceEducation();
	Snapshot.BuiltHousingCapacity = Services.BuiltHousingCapacity;
	Snapshot.HousingCapacity = Services.HousingCapacity;
	Snapshot.OccupiedResidents =
	    FMath::Clamp(Services.Residents - Services.HomelessResidents, 0, Snapshot.HousingCapacity);
	Snapshot.AvailableHousing = FMath::Max(0, Snapshot.HousingCapacity - Snapshot.OccupiedResidents);

	Snapshot.BuiltJobCapacity = Services.BuiltJobs;
	Snapshot.JobCapacity = FMath::Max(0, Services.Jobs);
	Snapshot.EmployedJobs =
	    Services.UnemployedResidents >= 0
	        ? FMath::Max(0, Services.Residents - Services.UnemployedResidents - Services.DetainedResidents)
	        : FMath::Min(Services.Residents, Snapshot.JobCapacity);
	Snapshot.AvailableJobs = FMath::Max(0, Snapshot.JobCapacity - Snapshot.EmployedJobs);
	Snapshot.HousingShortage = FMath::Max(0, Snapshot.JobCapacity - Snapshot.HousingCapacity);
	Snapshot.JobShortage = Services.UnemployedResidents >= 0 ? Services.UnemployedResidents
	                                                         : FMath::Max(0, Services.Residents - Snapshot.JobCapacity);

	Snapshot.PowerCapacity = FMath::Max(0, Services.PowerCapacity);
	Snapshot.PowerDemand = FMath::Max(0, Services.PowerDemand);
	Snapshot.PowerUsage = FMath::Clamp(Services.PowerUsed, 0, Snapshot.PowerCapacity);
	Snapshot.PowerShortage = FMath::Max(0, Snapshot.PowerDemand - Snapshot.PowerCapacity);

	Snapshot.ServiceCapacity = FMath::Max(0, Services.ServiceCapacity);
	Snapshot.ServiceDemand = FMath::Max(0, Services.ServiceDemand);
	Snapshot.ServiceCoveredDemand = FMath::Clamp(Services.ServiceCoveredDemand, 0, Snapshot.ServiceDemand);
	Snapshot.ServiceUsage = FMath::Clamp(Services.ServiceUsed, 0, Snapshot.ServiceCapacity);
	Snapshot.ServiceCoverageGap = FMath::Max(0, Snapshot.ServiceDemand - Snapshot.ServiceCoveredDemand);
	Snapshot.ServiceCapacityShortage = FMath::Max(0, Snapshot.ServiceCoveredDemand - Snapshot.ServiceCapacity);
	Snapshot.UnservedServiceDemand = FMath::Max(0, Snapshot.ServiceDemand - Snapshot.ServiceUsage);

	const int32 HousingStep = FMath::Max(1, Tuning.ResidentsPerOccupiedHome);
	const int32 BaseResidentialDemand =
	    Snapshot.OccupiedResidents == 0 ? HousingStep
	                                    : FMath::Max(0, Snapshot.JobCapacity + HousingStep - Snapshot.HousingCapacity);
	const auto ApplyTaxPreference = [](const int32 Demand, const int32 TaxBasisPoints)
	{
		const int32 MultiplierPercent = FMath::Clamp(200 - TaxBasisPoints / 10, 25, 200);
		return static_cast<int32>(
		    FMath::Min<int64>(MAX_int32, (static_cast<int64>(FMath::Max(0, Demand)) * MultiplierPercent + 99) / 100));
	};
	Snapshot.ResidentialDemand = ApplyTaxPreference(BaseResidentialDemand, ResidentialTaxBasisPoints);
	Snapshot.IndustrialJobCapacity = Services.IndustrialJobs;
	Snapshot.IndustrialOutput = Services.IndustrialOutput;
	Snapshot.HouseholdGoodsDemand =
	    Services.Residents > 0
	        ? FMath::DivideAndRoundUp(Services.Residents, FMath::Max(1, Tuning.ResidentsPerGoodsUnit))
	        : 0;
	Snapshot.GoodsProduced = Services.IndustrialOutput;
	Snapshot.RetailCapacity = Services.CommercialOutput;
	const int32 AffordablePurchases = Snapshot.EmployedJobs * Tuning.HouseholdIncomePerEmployedResident /
	                                  FMath::Max(1, Tuning.HouseholdIncomePerGoodsPurchase);
	Snapshot.GoodsSold = FMath::Min3(Snapshot.HouseholdGoodsDemand, Snapshot.GoodsProduced,
	                                 FMath::Min(Snapshot.RetailCapacity, AffordablePurchases));
	if (Population)
	{
		FCityEconomySnapshot Projection;
		Projection.Residents = Services.Residents;
		Projection.Jobs = Snapshot.JobCapacity;
		Projection.EmployedResidents = Snapshot.EmployedJobs;
		Projection.RetailCapacity = Snapshot.RetailCapacity;
		Projection.GoodsProduced = Services.IndustrialOutput;
		Snapshot.GoodsSold = FCityPopulationSimulation::ForecastGoodsSales(
		    City, Projection, Tuning, Services, *Population, nullptr, &Snapshot.GoodsProduced, nullptr, nullptr,
		    Timing ? &Timing->Forecast : nullptr, bPreparePopulationQueries);
	}
	Snapshot.UnmetGoodsDemand = FMath::Max(0, Snapshot.HouseholdGoodsDemand - Snapshot.GoodsSold);
	if (Population)
	{
		const int32 Shortage = FCityPopulationSimulation::StoredEssentialShortage(*Population);
		Snapshot.bUsesStoredSupplies = Shortage >= 0;
		if (Snapshot.bUsesStoredSupplies) Snapshot.UnmetGoodsDemand = Shortage;
	}
	Snapshot.UnfilledIndustrialOrders = FMath::Max(0, Snapshot.HouseholdGoodsDemand - Snapshot.GoodsProduced);
	const int32 EmploymentPressure = FMath::DivideAndRoundUp(Snapshot.JobShortage, 2);
	const int32 BaseCommercialDemand =
	    FMath::Max(FMath::Max(0, Snapshot.HouseholdGoodsDemand - Snapshot.RetailCapacity), EmploymentPressure);
	Snapshot.CommercialDemand = ApplyTaxPreference(BaseCommercialDemand, CommercialTaxBasisPoints);
	Snapshot.IndustrialDemand =
	    IndustryDefinition ? ApplyTaxPreference(FMath::Max(Snapshot.UnfilledIndustrialOrders, EmploymentPressure),
	                                            IndustrialTaxBasisPoints)
	                       : 0;
	if (Development && HomeDefinition && !HomeDefinition->DensityLevels.IsEmpty())
	{
		BuildDensityDemand(Snapshot, City, Services, *Development, Tuning, *HomeDefinition, ShopDefinition,
		                   IndustryDefinition, Population);
		Snapshot.ResidentialDemand =
		    ApplyTaxPreference(Snapshot.ResidentialDensity.NewLowDemand(), ResidentialTaxBasisPoints);
		Snapshot.CommercialDemand =
		    ApplyTaxPreference(Snapshot.CommercialDensity.NewLowDemand(), CommercialTaxBasisPoints);
		Snapshot.IndustrialDemand =
		    ApplyTaxPreference(Snapshot.IndustrialDensity.NewLowDemand(), IndustrialTaxBasisPoints);
	}
	const int32 MaximumDemand =
	    FMath::Max3(Snapshot.ResidentialDemand, Snapshot.CommercialDemand, Snapshot.IndustrialDemand);
	Snapshot.ResidentialDemandBalance = Snapshot.ResidentialDemand;
	Snapshot.CommercialDemandBalance = Snapshot.CommercialDemand;
	Snapshot.IndustrialDemandBalance = Snapshot.IndustrialDemand;
	if (Snapshot.bHasDensityDemand)
	{
		Snapshot.ResidentialDemandBalance = Snapshot.ResidentialDensity.AdditionalCapacity();
		Snapshot.CommercialDemandBalance = Snapshot.CommercialDensity.AdditionalCapacity();
		Snapshot.IndustrialDemandBalance = Snapshot.IndustrialDensity.AdditionalCapacity();
	}
	Snapshot.RecommendedGrowthZone = MaximumDemand <= 0                           ? ECityZoneType::None
	                                 : Snapshot.CommercialDemand == MaximumDemand ? ECityZoneType::Commercial
	                                 : Snapshot.IndustrialDemand == MaximumDemand ? ECityZoneType::Industrial
	                                                                              : ECityZoneType::Residential;

	if (Timing)
	{
		Timing->Market.Add((FPlatformTime::Seconds() - PhaseStart) * 1000);
		PhaseStart = FPlatformTime::Seconds();
	}
	const FZoneOpportunity Residential =
	    EvaluateZone(City, Services, Tuning, ECityZoneType::Residential, HomeDefinition, Environment);
	const FZoneOpportunity Commercial =
	    EvaluateZone(City, Services, Tuning, ECityZoneType::Commercial, ShopDefinition, Environment);
	const FZoneOpportunity Industrial =
	    EvaluateZone(City, Services, Tuning, ECityZoneType::Industrial, IndustryDefinition, Environment);
	Snapshot.ResidentialZonedFootprints = Residential.Zoned;
	Snapshot.ResidentialEligibleFootprints = Residential.Eligible;
	Snapshot.ResidentialAffordableFootprints = Residential.Affordable;
	Snapshot.CommercialZonedFootprints = Commercial.Zoned;
	Snapshot.CommercialEligibleFootprints = Commercial.Eligible;
	Snapshot.CommercialAffordableFootprints = Commercial.Affordable;
	Snapshot.IndustrialZonedFootprints = Industrial.Zoned;
	Snapshot.IndustrialEligibleFootprints = Industrial.Eligible;
	Snapshot.IndustrialAffordableFootprints = Industrial.Affordable;
	Snapshot.AvailableConstructionFunds = City.GetFunds();
	Snapshot.GrowthCheckIntervalDays = FMath::Max(1, Tuning.GrowthIntervalSteps);
	// Zoned construction is privately funded; public purchase previews still use catalog costs.
	Snapshot.ResidentialGrowthCost = 0;
	Snapshot.CommercialGrowthCost = 0;
	Snapshot.IndustrialGrowthCost = 0;
	Snapshot.ResidentialConstraint = ResolveConstraint(Snapshot.ResidentialDemand, Residential, bOperatingShortfall);
	Snapshot.CommercialConstraint = ResolveConstraint(Snapshot.CommercialDemand, Commercial, bOperatingShortfall);
	Snapshot.IndustrialConstraint = ResolveConstraint(Snapshot.IndustrialDemand, Industrial, bOperatingShortfall);
	// A blocked higher-demand family must not starve another ready market.
	int32 ReadyDemand = 0;
	for (const auto Zone : {ECityZoneType::Commercial, ECityZoneType::Industrial, ECityZoneType::Residential})
	{
		const auto Constraint = Zone == ECityZoneType::Commercial   ? Snapshot.CommercialConstraint
		                        : Zone == ECityZoneType::Industrial ? Snapshot.IndustrialConstraint
		                                                            : Snapshot.ResidentialConstraint;
		const int32 Demand = Zone == ECityZoneType::Commercial   ? Snapshot.CommercialDemand
		                     : Zone == ECityZoneType::Industrial ? Snapshot.IndustrialDemand
		                                                         : Snapshot.ResidentialDemand;
		if (Constraint == ECityDemandConstraint::None && Demand > ReadyDemand)
		{
			ReadyDemand = Demand;
			Snapshot.RecommendedGrowthZone = Zone;
		}
	}
	if (Timing)
	{
		Timing->Zones.Add((FPlatformTime::Seconds() - PhaseStart) * 1000);
		PhaseStart = FPlatformTime::Seconds();
	}
	if (Environment)
	{
		int64 DesirabilityTotal = 0;
		int64 PollutionTotal = 0;
		int32 HomeCount = 0;
		for (const TPair<uint64, FCityPlacedEntity>& Pair : City.GetEntities())
		{
			if (Pair.Value.DefinitionId != Tuning.HomeDefinitionId)
				continue;
			const FCityDesirabilityResult Result = FCityEnvironmentSimulation::EvaluateResidentialDesirability(
			    City, Services, *Environment, Pair.Value.Anchor, Pair.Value.FootprintSize, Tuning);
			DesirabilityTotal += Result.Score;
			PollutionTotal += Result.PollutionExposure;
			++HomeCount;
		}
		if (HomeCount > 0)
		{
			Snapshot.AverageResidentialDesirability = static_cast<int32>(DesirabilityTotal / HomeCount);
			Snapshot.ResidentialPollutionExposure = static_cast<int32>(PollutionTotal / HomeCount);
		}
	}
	if (Timing)
		Timing->Desirability.Add((FPlatformTime::Seconds() - PhaseStart) * 1000);
	return Snapshot;
}

FString FCityNeedsEvaluator::ConstraintToString(const ECityDemandConstraint Constraint)
{
	switch (Constraint)
	{
	case ECityDemandConstraint::None:
		return TEXT("Ready to grow");
	case ECityDemandConstraint::LowDemand:
		return TEXT("Low demand");
	case ECityDemandConstraint::OperatingShortfall:
		return TEXT("Unpaid monthly bills: City > TAXES or reduce upkeep");
	case ECityDemandConstraint::NoZonedFootprint:
		return TEXT("No empty zoned 2x2 footprint");
	case ECityDemandConstraint::NoRoad:
		return TEXT("Zoned land needs a connected road");
	case ECityDemandConstraint::NoService:
		return TEXT("Zoned land lacks usable water; inspect pipes and pressure");
	case ECityDemandConstraint::ServiceCapacity:
		return TEXT("Water capacity exhausted");
	case ECityDemandConstraint::NoPower:
		return TEXT("No operational power");
	case ECityDemandConstraint::PowerCapacity:
		return TEXT("Power capacity exhausted");
	case ECityDemandConstraint::LowDesirability:
		return TEXT("Residential desirability too low");
	case ECityDemandConstraint::PollutionPressure:
		return TEXT("Residential pollution pressure too high");
	case ECityDemandConstraint::InvalidTerrain:
		return TEXT("Zoned land has invalid terrain foundation");
	case ECityDemandConstraint::InsufficientFunds:
		return TEXT("Insufficient construction funds");
	case ECityDemandConstraint::MissingDefinition:
		return TEXT("Growth definition missing");
	default:
		return TEXT("Growth unavailable");
	}
}
