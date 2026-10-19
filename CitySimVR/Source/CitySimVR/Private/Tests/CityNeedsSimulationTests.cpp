#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Simulation/CityGrowthSimulation.h"
#include "Simulation/CityNeedsSimulation.h"
#include "Simulation/CityTerrain.h"

namespace
{
	FCityBuildingDefinition NeedsDefinition(const FName Id, const int64 Cost = 0)
	{
		FCityBuildingDefinition Definition;
		Definition.DefinitionId = Id;
		Definition.DisplayName = Id.ToString();
		Definition.Footprint = FCityCellFootprint(1, 1);
		Definition.Cost = Cost;
		Definition.LogicalHeightCm = 100.0;
		return Definition;
	}

	uint64 PlaceNeedsFixture(FCityPlacementModel& City, const FCityBuildingDefinition& Definition,
	                         const FCityCellCoord& Anchor)
	{
		FCityPlacementPreview Preview = City.BeginPreview(Definition, Anchor);
		return City.CommitPlacement(Preview, Definition).EntityId;
	}

	void AddNeedsRoad(FCityPlacementModel& City, const int32 EndX)
	{
		FCityRoadPreview Preview = City.BeginRoadPreview(FCityPlacementModel::EntranceCell);
		City.UpdateRoadPreviewEnd(Preview, FCityCellCoord(EndX, FCityPlacementModel::EntranceCell.Y));
		City.CommitRoad(Preview);
	}

	void AddNeedsZone(FCityPlacementModel& City, const FCityCellCoord& Anchor, const ECityZoneType Zone)
	{
		City.SetZoneArea(Anchor, FIntPoint(2, 2), Zone);
	}

	FCityNeedsSnapshot MakeNeedsSnapshot(const FCityPlacementModel& City, const FCityEconomyTuning& Tuning,
	                                     const FCityBuildingDefinition& Home, const FCityBuildingDefinition& Shop,
	                                     const bool bShortfall = false)
	{
		const FCityServiceSnapshot Services = FCityServiceEvaluator::BuildSnapshot(City, Tuning);
		return FCityNeedsEvaluator::BuildSnapshot(City, Services, Tuning, &Home, &Shop, bShortfall);
	}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityNeedsSimulationTest, "CitySim.Simulation.AggregateNeeds",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityNeedsSimulationTest::RunTest(const FString& Parameters)
{
	FCityEconomyTuning Tuning;
	Tuning.WaterRangeRoadHops = 50;
	Tuning.PowerCapacityPerBuilding = 1000;
	Tuning.WaterCapacityPerTreatment = 1000;
	const FCityBuildingDefinition Home = NeedsDefinition(TEXT("Home"), 200);
	const FCityBuildingDefinition Shop = NeedsDefinition(TEXT("Shop"), 300);
	const FCityBuildingDefinition Power = NeedsDefinition(TEXT("Power"));
	const FCityBuildingDefinition Service = NeedsDefinition(TEXT("Service"));

	FCityPlacementModel EmptyCity(10000);
	const FCityNeedsSnapshot Empty = MakeNeedsSnapshot(EmptyCity, Tuning, Home, Shop);
	TestEqual(TEXT("empty city bootstraps one housing increment of demand"), Empty.ResidentialDemand, 10);
	TestEqual(TEXT("empty city has no commercial demand"), Empty.CommercialDemand, 0);
	TestEqual(TEXT("empty city distinguishes missing zoned land"), Empty.ResidentialConstraint,
	          ECityDemandConstraint::NoZonedFootprint);
	TestEqual(TEXT("zero commercial demand is labeled low rather than missing land"), Empty.CommercialConstraint,
	          ECityDemandConstraint::LowDemand);

	FCityPlacementModel BalancedCity(100000);
	AddNeedsRoad(BalancedCity, 40);
	PlaceNeedsFixture(BalancedCity, Power, FCityCellCoord(0, 9));
	PlaceNeedsFixture(BalancedCity, Service, FCityCellCoord(1, 9));
	PlaceNeedsFixture(BalancedCity, Home, FCityCellCoord(2, 9));
	PlaceNeedsFixture(BalancedCity, Shop, FCityCellCoord(3, 9));
	AddNeedsZone(BalancedCity, FCityCellCoord(5, 9), ECityZoneType::Residential);
	AddNeedsZone(BalancedCity, FCityCellCoord(8, 9), ECityZoneType::Commercial);
	const FCityNeedsSnapshot Balanced = MakeNeedsSnapshot(BalancedCity, Tuning, Home, Shop);
	TestEqual(TEXT("balanced housing capacity"), Balanced.HousingCapacity, 10);
	TestEqual(TEXT("instant occupancy fills operational housing"), Balanced.OccupiedResidents, 10);
	TestEqual(TEXT("balanced job capacity"), Balanced.JobCapacity, 10);
	TestEqual(TEXT("balanced employment"), Balanced.EmployedJobs, 10);
	TestEqual(TEXT("balanced city has no housing shortage"), Balanced.HousingShortage, 0);
	TestEqual(TEXT("balanced city has no job shortage"), Balanced.JobShortage, 0);
	TestEqual(TEXT("balanced loop requests the next residential increment"), Balanced.RecommendedGrowthZone,
	          ECityZoneType::Residential);
	TestTrue(TEXT("connected serviced residential zone is eligible"), Balanced.ResidentialEligibleFootprints > 0);
	TestEqual(TEXT("eligible funded residential growth reports ready"), Balanced.ResidentialConstraint,
	          ECityDemandConstraint::None);

	FCityEconomyTuning MeteredWater = Tuning;
	MeteredWater.WaterDemandPerResident = 2;
	MeteredWater.WaterDemandPerCommercialJob = 3;
	const FCityServiceSnapshot MeteredWaterSnapshot = FCityServiceEvaluator::BuildSnapshot(BalancedCity, MeteredWater);
	TestEqual(TEXT("water demand derives separately from residents and commercial jobs"),
	          MeteredWaterSnapshot.ServiceDemand, 50);

	FCityPlacementModel HousingShortageCity(100000);
	AddNeedsRoad(HousingShortageCity, 40);
	PlaceNeedsFixture(HousingShortageCity, Power, FCityCellCoord(0, 9));
	PlaceNeedsFixture(HousingShortageCity, Service, FCityCellCoord(1, 9));
	PlaceNeedsFixture(HousingShortageCity, Home, FCityCellCoord(2, 9));
	PlaceNeedsFixture(HousingShortageCity, Shop, FCityCellCoord(3, 9));
	PlaceNeedsFixture(HousingShortageCity, Shop, FCityCellCoord(4, 9));
	const FCityNeedsSnapshot HousingShortage = MakeNeedsSnapshot(HousingShortageCity, Tuning, Home, Shop);
	TestEqual(TEXT("excess jobs produce housing shortage"), HousingShortage.HousingShortage, 10);
	TestEqual(TEXT("excess jobs increase residential demand"), HousingShortage.ResidentialDemand, 20);

	FCityPlacementModel JobShortageCity(100000);
	AddNeedsRoad(JobShortageCity, 40);
	PlaceNeedsFixture(JobShortageCity, Power, FCityCellCoord(0, 9));
	PlaceNeedsFixture(JobShortageCity, Service, FCityCellCoord(1, 9));
	PlaceNeedsFixture(JobShortageCity, Home, FCityCellCoord(2, 9));
	PlaceNeedsFixture(JobShortageCity, Home, FCityCellCoord(3, 9));
	PlaceNeedsFixture(JobShortageCity, Shop, FCityCellCoord(4, 9));
	const FCityNeedsSnapshot JobShortage = MakeNeedsSnapshot(JobShortageCity, Tuning, Home, Shop);
	TestEqual(TEXT("excess residents produce job shortage"), JobShortage.JobShortage, 10);
	TestEqual(TEXT("job shortage and retail capacity drive commercial demand"), JobShortage.CommercialDemand, 5);
	TestEqual(TEXT("independent residential pressure can settle at zero"), JobShortage.ResidentialDemandBalance, 0);
	TestEqual(TEXT("commercial pressure remains independently positive"), JobShortage.CommercialDemandBalance, 5);
	TestEqual(TEXT("commercial becomes recommended growth"), JobShortage.RecommendedGrowthZone,
	          ECityZoneType::Commercial);

	FCityPlacementModel PowerShortageCity(100000);
	FCityEconomyTuning PowerLimited = Tuning;
	PowerLimited.PowerCapacityPerBuilding = 10;
	AddNeedsRoad(PowerShortageCity, 40);
	PlaceNeedsFixture(PowerShortageCity, Power, FCityCellCoord(0, 9));
	PlaceNeedsFixture(PowerShortageCity, Service, FCityCellCoord(1, 9));
	PlaceNeedsFixture(PowerShortageCity, Home, FCityCellCoord(2, 9));
	const uint64 PowerBlockedHome = PlaceNeedsFixture(PowerShortageCity, Home, FCityCellCoord(3, 9));
	const FCityServiceSnapshot PowerServices = FCityServiceEvaluator::BuildSnapshot(PowerShortageCity, PowerLimited);
	const FCityNeedsSnapshot PowerShortage =
	    FCityNeedsEvaluator::BuildSnapshot(PowerShortageCity, PowerServices, PowerLimited, &Home, &Shop, false);
	TestEqual(TEXT("power demand includes disabled demand"), PowerShortage.PowerDemand, 20);
	TestEqual(TEXT("power shortage is demand minus capacity"), PowerShortage.PowerShortage, 10);
	TestEqual(TEXT("stable allocation reports power capacity blocker"), PowerServices.GetFailure(PowerBlockedHome),
	          ECityOperationFailure::PowerCapacity);

	FCityPlacementModel ServiceGapCity(100000);
	FCityEconomyTuning ShortReach = Tuning;
	ShortReach.WaterRangeRoadHops = 2;
	AddNeedsRoad(ServiceGapCity, 20);
	PlaceNeedsFixture(ServiceGapCity, Power, FCityCellCoord(0, 9));
	PlaceNeedsFixture(ServiceGapCity, Service, FCityCellCoord(1, 9));
	PlaceNeedsFixture(ServiceGapCity, Home, FCityCellCoord(10, 9));
	const FCityNeedsSnapshot ServiceGap = MakeNeedsSnapshot(ServiceGapCity, ShortReach, Home, Shop);
	TestEqual(TEXT("service demand includes uncovered buildings"), ServiceGap.ServiceDemand, 10);
	TestEqual(TEXT("uncovered demand is separated from capacity"), ServiceGap.ServiceCoverageGap, 10);
	TestEqual(TEXT("uncovered demand remains unserved"), ServiceGap.UnservedServiceDemand, 10);

	FCityPlacementModel ServiceCapacityCity(100000);
	FCityEconomyTuning ServiceLimited = Tuning;
	ServiceLimited.WaterCapacityPerTreatment = 10;
	AddNeedsRoad(ServiceCapacityCity, 20);
	PlaceNeedsFixture(ServiceCapacityCity, Power, FCityCellCoord(0, 9));
	PlaceNeedsFixture(ServiceCapacityCity, Service, FCityCellCoord(1, 9));
	PlaceNeedsFixture(ServiceCapacityCity, Home, FCityCellCoord(2, 9));
	const uint64 ServiceBlockedHome = PlaceNeedsFixture(ServiceCapacityCity, Home, FCityCellCoord(3, 9));
	const FCityServiceSnapshot LimitedServices =
	    FCityServiceEvaluator::BuildSnapshot(ServiceCapacityCity, ServiceLimited);
	const FCityNeedsSnapshot ServiceShortage =
	    FCityNeedsEvaluator::BuildSnapshot(ServiceCapacityCity, LimitedServices, ServiceLimited, &Home, &Shop, false);
	TestEqual(TEXT("covered demand is tracked before capacity allocation"), ServiceShortage.ServiceCoveredDemand, 20);
	TestEqual(TEXT("service capacity shortage is explicit"), ServiceShortage.ServiceCapacityShortage, 10);
	TestEqual(TEXT("service usage never exceeds capacity"), ServiceShortage.ServiceUsage, 10);
	TestEqual(TEXT("stable service allocation identifies overload"), LimitedServices.GetFailure(ServiceBlockedHome),
	          ECityOperationFailure::ServiceCapacity);

	FCityPlacementModel InfrastructureBlockedCity(10000);
	AddNeedsZone(InfrastructureBlockedCity, FCityCellCoord(10, 10), ECityZoneType::Residential);
	const FCityNeedsSnapshot InfrastructureBlocked = MakeNeedsSnapshot(InfrastructureBlockedCity, Tuning, Home, Shop);
	TestEqual(TEXT("zoned land without infrastructure is not low demand"), InfrastructureBlocked.ResidentialConstraint,
	          ECityDemandConstraint::NoRoad);
	FCityPlacementModel UnaffordableCity(300);
	AddNeedsRoad(UnaffordableCity, 20);
	PlaceNeedsFixture(UnaffordableCity, Power, FCityCellCoord(0, 9));
	PlaceNeedsFixture(UnaffordableCity, Service, FCityCellCoord(1, 9));
	AddNeedsZone(UnaffordableCity, FCityCellCoord(5, 9), ECityZoneType::Residential);
	const FCityNeedsSnapshot Unaffordable = MakeNeedsSnapshot(UnaffordableCity, Tuning, Home, Shop);
	TestTrue(TEXT("unaffordable fixture still has infrastructure-eligible land"),
	         Unaffordable.ResidentialEligibleFootprints > 0);
	TestEqual(TEXT("treasury shortage does not block private development"), Unaffordable.ResidentialConstraint,
	          ECityDemandConstraint::None);
	TestEqual(TEXT("needs snapshot exposes the exact available construction cash"),
	          Unaffordable.AvailableConstructionFunds, UnaffordableCity.GetFunds());
	TestEqual(TEXT("private construction has no city cost"), Unaffordable.ResidentialGrowthCost, int64(0));

	FCityTerrainHeightfield Terrain;
	TestTrue(TEXT("terrain blocker fixture generates"), Terrain.Generate(FCityTerrainGenerationParameters()));
	FCityCellCoord WaterAnchor(-1, -1);
	for (int32 Y = 0; Y < FCityGrid::Height - 1 && WaterAnchor.X < 0; ++Y)
	{
		for (int32 X = 0; X < FCityGrid::Width - 1; ++X)
		{
			if (Terrain.IsWaterCell(FCityCellCoord(X, Y)) && Terrain.IsWaterCell(FCityCellCoord(X + 1, Y)) &&
			    Terrain.IsWaterCell(FCityCellCoord(X, Y + 1)) && Terrain.IsWaterCell(FCityCellCoord(X + 1, Y + 1)))
			{
				WaterAnchor = FCityCellCoord(X, Y);
				break;
			}
		}
	}
	TestTrue(TEXT("terrain blocker fixture has a 2x2 water footprint"), WaterAnchor.X >= 0);
	if (WaterAnchor.X >= 0)
	{
		FCityPlacementModel TerrainBlockedCity(100000);
		const FCityCellCoord WaterRoadEnd(WaterAnchor.X > 0 ? WaterAnchor.X - 1 : WaterAnchor.X + 2, WaterAnchor.Y);
		FCityRoadPreview WaterRoad = TerrainBlockedCity.BeginRoadPreview(FCityPlacementModel::EntranceCell);
		TerrainBlockedCity.UpdateRoadPreviewEnd(WaterRoad, WaterRoadEnd);
		TerrainBlockedCity.CommitRoad(WaterRoad);
		PlaceNeedsFixture(TerrainBlockedCity, Power, FCityCellCoord(0, 9));
		PlaceNeedsFixture(TerrainBlockedCity, Service, FCityCellCoord(1, 9));
		AddNeedsZone(TerrainBlockedCity, WaterAnchor, ECityZoneType::Residential);
		TerrainBlockedCity.SetTerrain(&Terrain);
		FCityEconomyTuning TerrainTuning = Tuning;
		TerrainTuning.WaterRangeRoadHops = 200;
		const FCityNeedsSnapshot TerrainBlocked = MakeNeedsSnapshot(TerrainBlockedCity, TerrainTuning, Home, Shop);
		TestEqual(TEXT("invalid terrain is not mislabeled as insufficient funds"), TerrainBlocked.ResidentialConstraint,
		          ECityDemandConstraint::InvalidTerrain);

		// A reachable but terrain-invalid parcel must not conceal that all other
		// candidate lots are outside the water network's road-reach range.
		FCityPlacementModel MixedBlockerCity(100000);
		AddNeedsRoad(MixedBlockerCity, 62);
		PlaceNeedsFixture(MixedBlockerCity, Power, FCityCellCoord(WaterRoadEnd.X, WaterRoadEnd.Y - 2));
		PlaceNeedsFixture(MixedBlockerCity, Service, FCityCellCoord(WaterRoadEnd.X, WaterRoadEnd.Y - 1));
		AddNeedsZone(MixedBlockerCity, WaterAnchor, ECityZoneType::Residential);
		AddNeedsZone(MixedBlockerCity, FCityCellCoord(50, 9), ECityZoneType::Residential);
		MixedBlockerCity.SetTerrain(&Terrain);
		FCityEconomyTuning MixedTuning = Tuning;
		MixedTuning.WaterRangeRoadHops = 12;
		const FCityNeedsSnapshot MixedBlocker = MakeNeedsSnapshot(MixedBlockerCity, MixedTuning, Home, Shop);
		TestEqual(TEXT("water reach outranks terrain when it blocks usable lots"), MixedBlocker.ResidentialConstraint,
		          ECityDemandConstraint::NoService);
	}
	const FCityNeedsSnapshot OperatingBlocked = FCityNeedsEvaluator::BuildSnapshot(
	    BalancedCity, FCityServiceEvaluator::BuildSnapshot(BalancedCity, Tuning), Tuning, &Home, &Shop, true);
	TestEqual(TEXT("operating shortfall supersedes otherwise eligible demand"), OperatingBlocked.ResidentialConstraint,
	          ECityDemandConstraint::OperatingShortfall);
	TestTrue(TEXT("all aggregate counts remain nonnegative"),
	         ServiceShortage.AvailableHousing >= 0 && ServiceShortage.AvailableJobs >= 0 &&
	             ServiceShortage.PowerShortage >= 0 && ServiceShortage.UnservedServiceDemand >= 0);

	FCityServiceSnapshot SupplyChainServices;
	SupplyChainServices.Residents = 100;
	SupplyChainServices.HousingCapacity = 100;
	SupplyChainServices.Jobs = 20;
	SupplyChainServices.CommercialOutput = 10;
	SupplyChainServices.IndustrialJobs = 20;
	SupplyChainServices.IndustrialOutput = 20;
	const FCityBuildingDefinition Factory = NeedsDefinition(TEXT("Factory"), 350);
	const FCityNeedsSnapshot SupplyChain = FCityNeedsEvaluator::BuildSnapshot(EmptyCity, SupplyChainServices, Tuning,
	                                                                          &Home, &Shop, false, &Factory, nullptr);
	TestEqual(TEXT("one hundred residents request fifty goods units"), SupplyChain.HouseholdGoodsDemand, 50);
	TestEqual(TEXT("industry exposes produced goods"), SupplyChain.GoodsProduced, 20);
	TestEqual(TEXT("shops expose retail throughput"), SupplyChain.RetailCapacity, 10);
	TestEqual(TEXT("employment income allows ten completed purchases"), SupplyChain.GoodsSold, 10);
	TestEqual(TEXT("population drives commercial expansion independently"), SupplyChain.CommercialDemandBalance, 40);
	TestEqual(TEXT("population and job shortage drive industrial expansion independently"),
	          SupplyChain.IndustrialDemandBalance, 40);
	const FCityNeedsSnapshot LowTaxSupplyChain = FCityNeedsEvaluator::BuildSnapshot(
	    EmptyCity, SupplyChainServices, Tuning, &Home, &Shop, false, &Factory, nullptr, 1000, 0);
	TestEqual(TEXT("zero commercial tax doubles commercial preference pressure"),
	          LowTaxSupplyChain.CommercialDemandBalance, 80);
	const auto LowIndustrialTax = FCityNeedsEvaluator::BuildSnapshot(
	    EmptyCity, SupplyChainServices, Tuning, &Home, &Shop, false, &Factory, nullptr, 1000, 1000, nullptr, 0);
	const auto HighIndustrialTax = FCityNeedsEvaluator::BuildSnapshot(
	    EmptyCity, SupplyChainServices, Tuning, &Home, &Shop, false, &Factory, nullptr, 1000, 1000, nullptr, 2500);
	TestEqual(TEXT("Zero industrial tax doubles only industrial demand"), LowIndustrialTax.IndustrialDemand, 80);
	TestEqual(TEXT("High industrial tax retains bounded positive industrial demand"),
	          HighIndustrialTax.IndustrialDemand, 10);
	TestEqual(TEXT("Industrial policy does not alter commercial preference"), LowIndustrialTax.CommercialDemand,
	          SupplyChain.CommercialDemand);
	return true;
}

#endif
