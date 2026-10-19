#if WITH_DEV_AUTOMATION_TESTS

#include "Commands/CityPlacementModel.h"
#include "Misc/AutomationTest.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Simulation/CityEconomySimulation.h"
#include "Simulation/CityEnvironmentSimulation.h"
#include "Simulation/CityGrowthSimulation.h"
#include "Simulation/CityNeedsSimulation.h"

namespace
{
	FCityBuildingDefinition MakeEnvironmentDefinition(const FName Id, const int32 Width = 2, const int32 Height = 2)
	{
		FCityBuildingDefinition Definition;
		Definition.DefinitionId = Id;
		Definition.DisplayName = Id.ToString();
		Definition.Footprint = FCityCellFootprint(Width, Height);
		Definition.LogicalHeightCm = 100.0;
		return Definition;
	}

	uint64 PlaceEnvironmentFixture(FCityPlacementModel& City, const FCityBuildingDefinition& Definition,
	                               const FCityCellCoord& Anchor)
	{
		FCityPlacementPreview Preview = City.BeginPreview(Definition, Anchor);
		return City.CommitPlacement(Preview, Definition).EntityId;
	}

	void AddEnvironmentRoad(FCityPlacementModel& City)
	{
		FCityRoadPreview Preview = City.BeginRoadPreview(FCityPlacementModel::EntranceCell);
		City.UpdateRoadPreviewEnd(Preview, FCityCellCoord(30, FCityPlacementModel::EntranceCell.Y));
		City.CommitRoad(Preview);
	}

	void ZoneEnvironmentFootprint(FCityPlacementModel& City, const FCityCellCoord& Anchor, const ECityZoneType Zone)
	{
		for (int32 Y = Anchor.Y; Y < Anchor.Y + 2; ++Y)
		{
			for (int32 X = Anchor.X; X < Anchor.X + 2; ++X)
			{
				City.SetZone(FCityCellCoord(X, Y), Zone);
			}
		}
	}

	struct FEnvironmentFixture
	{
		FCityPlacementModel City{100000};
		FCityEconomyTuning Tuning;
		uint64 FactoryId = FCityGrid::EmptyEntityId;

		FEnvironmentFixture()
		{
			Tuning.WaterRangeRoadHops = 100;
			Tuning.PowerCapacityPerBuilding = 500;
			Tuning.WaterCapacityPerTreatment = 500;
			AddEnvironmentRoad(City);
			PlaceEnvironmentFixture(City, MakeEnvironmentDefinition(Tuning.PowerDefinitionId), FCityCellCoord(0, 9));
			PlaceEnvironmentFixture(City, MakeEnvironmentDefinition(Tuning.WaterTreatmentDefinitionId),
			                        FCityCellCoord(2, 9));
			FactoryId = PlaceEnvironmentFixture(City, MakeEnvironmentDefinition(Tuning.IndustrialDefinitionId),
			                                    FCityCellCoord(4, 9));
		}
	};
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityDeterministicEnvironmentTest,
                                 "CitySim.Simulation.DeterministicPollutionAndDesirability",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityDeterministicEnvironmentTest::RunTest(const FString& Parameters)
{
	FEnvironmentFixture First;
	FEnvironmentFixture Second;
	const FCityServiceSnapshot FirstServices = FCityServiceEvaluator::BuildSnapshot(First.City, First.Tuning);
	const FCityServiceSnapshot SecondServices = FCityServiceEvaluator::BuildSnapshot(Second.City, Second.Tuning);
	TestEqual(TEXT("industrial building is operational"), FirstServices.OperationalIndustries, 1);
	TestEqual(TEXT("industry contributes bounded jobs"), FirstServices.IndustrialJobs,
	          First.Tuning.JobsPerOperationalIndustry);
	TestEqual(TEXT("industry consumes declared power"), FirstServices.PowerUsed, First.Tuning.IndustrialPowerDemand);
	TestEqual(TEXT("industry consumes declared water"), FirstServices.ServiceUsed, First.Tuning.IndustrialWaterDemand);

	FCityEnvironmentSimulation Coarse;
	FCityEnvironmentSimulation Incremental;
	Coarse.AdvanceToStep(12, First.City, FirstServices, First.Tuning);
	for (int32 Step = 1; Step <= 12; ++Step)
	{
		Incremental.AdvanceToStep(Step, Second.City, SecondServices, Second.Tuning);
	}
	TestTrue(TEXT("operating industry emits pollution"), Coarse.GetSnapshot().TotalPollution > 0);
	TestTrue(TEXT("pollution remains cell bounded"),
	         Coarse.GetSnapshot().PeakPollution <= First.Tuning.MaximumPollutionPerCell);
	TestTrue(TEXT("coarse and incremental fixed steps match"),
	         Coarse.GetSnapshot().Pollution == Incremental.GetSnapshot().Pollution);

	const FCityEnvironmentSnapshot Clean;
	const FCityDesirabilityResult CleanDesirability = FCityEnvironmentSimulation::EvaluateResidentialDesirability(
	    First.City, FirstServices, Clean, FCityCellCoord(4, 9), FIntPoint(2, 2), First.Tuning);
	const FCityDesirabilityResult PollutedDesirability = FCityEnvironmentSimulation::EvaluateResidentialDesirability(
	    First.City, FirstServices, Coarse.GetSnapshot(), FCityCellCoord(4, 9), FIntPoint(2, 2), First.Tuning);
	TestTrue(TEXT("pollution lowers named residential desirability"),
	         PollutedDesirability.Score < CleanDesirability.Score);
	TestTrue(TEXT("pollution penalty is reported"), PollutedDesirability.PollutionPenalty > 0);
	const FCityBuildingDefinition Park = MakeEnvironmentDefinition(First.Tuning.ParkDefinitionId, 3, 3);
	PlaceEnvironmentFixture(First.City, Park, FCityCellCoord(8, 9));
	const FCityServiceSnapshot ParkServices = FCityServiceEvaluator::BuildSnapshot(First.City, First.Tuning);
	const FCityDesirabilityResult ParkDesirability = FCityEnvironmentSimulation::EvaluateResidentialDesirability(
	    First.City, ParkServices, Clean, FCityCellCoord(6, 9), FIntPoint(2, 2), First.Tuning);
	TestEqual(TEXT("nearby park contributes its named desirability bonus"), ParkDesirability.ParkBonus,
	          First.Tuning.ParkDesirabilityBonus);
	TestEqual(TEXT("park contributes monthly civic upkeep"), ParkServices.ParkUpkeep, First.Tuning.ParkUpkeep);

	const int32 PollutedTotal = Coarse.GetSnapshot().TotalPollution;
	First.City.DemolishEntity(First.FactoryId);
	const FCityServiceSnapshot AfterDemolition = FCityServiceEvaluator::BuildSnapshot(First.City, First.Tuning);
	Coarse.AdvanceToStep(80, First.City, AfterDemolition, First.Tuning);
	TestTrue(TEXT("pollution decays after the source is removed"), Coarse.GetSnapshot().TotalPollution < PollutedTotal);

	FCityEnvironmentSimulation WeatherCoarse;
	FCityEnvironmentSimulation WeatherIncremental;
	WeatherCoarse.AdvanceToStep(80, Second.City, SecondServices, Second.Tuning);
	for (int32 Step = 1; Step <= 80; ++Step)
	{
		WeatherIncremental.AdvanceToStep(Step, Second.City, SecondServices, Second.Tuning);
	}
	TestEqual(TEXT("coarse and incremental weather states match"), WeatherCoarse.GetSnapshot().WeatherRandomState,
	          WeatherIncremental.GetSnapshot().WeatherRandomState);
	TestEqual(TEXT("coarse and incremental weather pollution matches"), WeatherCoarse.GetSnapshot().Pollution,
	          WeatherIncremental.GetSnapshot().Pollution);
	TestTrue(TEXT("weather water modifier is bounded"),
	         WeatherCoarse.GetSnapshot().GetWaterDemandModifierPercent() >= 90 &&
	             WeatherCoarse.GetSnapshot().GetWaterDemandModifierPercent() <= 110);
	TestTrue(TEXT("weather desirability modifier is bounded"),
	         WeatherCoarse.GetSnapshot().GetDesirabilityModifier() >= -5 &&
	             WeatherCoarse.GetSnapshot().GetDesirabilityModifier() <= 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityIndustrialGrowthTest, "CitySim.Simulation.IndustrialGrowth",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityIndustrialGrowthTest::RunTest(const FString& Parameters)
{
	FCityEconomyTuning Tuning;
	Tuning.WaterRangeRoadHops = 100;
	FCityPlacementModel City(100000);
	AddEnvironmentRoad(City);
	PlaceEnvironmentFixture(City, MakeEnvironmentDefinition(Tuning.PowerDefinitionId), FCityCellCoord(0, 9));
	PlaceEnvironmentFixture(City, MakeEnvironmentDefinition(Tuning.WaterTreatmentDefinitionId), FCityCellCoord(2, 9));
	const FCityCellCoord IndustrialAnchor(6, 9);
	ZoneEnvironmentFootprint(City, IndustrialAnchor, ECityZoneType::Industrial);
	const FCityServiceSnapshot Before = FCityServiceEvaluator::BuildSnapshot(City, Tuning);
	FCityNeedsSnapshot Needs;
	Needs.RecommendedGrowthZone = ECityZoneType::Industrial;
	const FCityBuildingDefinition Factory = MakeEnvironmentDefinition(Tuning.IndustrialDefinitionId);
	FCityGrowthSimulation Growth(42);
	const FCityGrowthResult Result = Growth.AdvanceToStep(Tuning.GrowthIntervalSteps, City, Before, Needs, Tuning,
	                                                      nullptr, nullptr, false, &Factory);
	TestTrue(TEXT("yellow industrial zoning grows its factory family"), Result.IsSuccess());
	TestEqual(TEXT("growth records industrial zone type"), Result.Zone, ECityZoneType::Industrial);
	const FCityServiceSnapshot After = FCityServiceEvaluator::BuildSnapshot(City, Tuning);
	TestEqual(TEXT("grown industry becomes operational"), After.OperationalIndustries, 1);
	TestEqual(TEXT("grown industry supplies jobs"), After.IndustrialJobs, Tuning.JobsPerOperationalIndustry);
	return true;
}

#endif
