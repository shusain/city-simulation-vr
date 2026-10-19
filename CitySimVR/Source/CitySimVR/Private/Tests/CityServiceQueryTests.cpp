#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Performance/CityEvaluationTiming.h"
#include "Simulation/CityEducationSimulation.h"
#include "Simulation/CityGrowthSimulation.h"
#include "Simulation/CityPopulationSimulation.h"

namespace
{
	uint64 PlaceQueryBuilding(FCityPlacementModel& City, const FName Name, const FCityCellCoord Cell)
	{
		FCityBuildingDefinition Definition;
		Definition.DefinitionId = Name;
		Definition.DisplayName = Name.ToString();
		Definition.Footprint = FCityCellFootprint(1, 1);
		Definition.LogicalHeightCm = 100;
		auto Preview = City.BeginPreview(Definition, Cell);
		return City.CommitPlacement(Preview, Definition).EntityId;
	}
	bool QueryRoad(FCityPlacementModel& City, const FCityCellCoord A, const FCityCellCoord B)
	{
		auto Preview = City.BeginRoadPreview(A);
		City.UpdateRoadPreviewEnd(Preview, B);
		return City.CommitRoad(Preview).IsSuccess();
	}
	bool SameStudents(const TSet<uint64>& A, const TSet<uint64>& B)
	{
		if (A.Num() != B.Num())
			return false;
		for (const auto Id : A)
			if (!B.Contains(Id))
				return false;
		return true;
	}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityPreparedWaterQueryTest, "CitySim.Water.PreparedQueryEquivalence",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityPreparedWaterQueryTest::RunTest(const FString& Parameters)
{
	FCityPlacementModel City(1000000);
	if (!TestTrue(TEXT("Connected road"), QueryRoad(City, City.GetEntranceCell(), {63, 8})))
		return false;
	const uint64 Sources[] = {PlaceQueryBuilding(City, TEXT("Service"), {0, 9}),
	                          PlaceQueryBuilding(City, TEXT("WaterTower"), {20, 9}),
	                          PlaceQueryBuilding(City, TEXT("Groundwater"), {40, 9})};
	for (const int32 Rules : {0, 1})
		for (const bool Reverse : {false, true})
		{
			FCityWaterSnapshot Water;
			Water.RulesVersion = Rules;
			for (int32 Index = 0; Index < 3; ++Index)
			{
				FCityWaterSource Source;
				Source.EntityId = Sources[Index];
				Source.Capacity = 37 + Index * 13;
				Source.SourcePressurePercent = Index == 1 ? 120 : 100;
				Source.bOperational = true;
				Water.Sources.Add(Source);
			}
			if (Reverse)
				Swap(Water.Sources[0], Water.Sources[2]);
			FCityWaterEvaluator::BuildNetwork(City, Water, 32);
			for (int32 State = 0; State < 4; ++State)
			{
				if (State == 1)
					Water.Sources[1].bOperational = false;
				if (State == 2)
				{
					Water.Sources[1].bOperational = true;
					Water.Sources[0].Capacity = 0;
				}
				if (State == 3)
					Water.Sources.Reset();
				FCityWaterEvaluator::BuildNetwork(City, Water, 32);
				auto Prepared = Water, Reference = Water;
				Reference.bPrepareQueries = false;
				FCityServiceEvaluationTiming Timing;
				Prepared.EvaluationTiming = Reference.EvaluationTiming = &Timing;
				uint64 Consumer = 100;
				for (const int32 X : {0, 5, 10, 20, 30, 40, 50, 62})
					for (const FIntPoint Size : {FIntPoint(1, 1), FIntPoint(2, 2)})
						for (const int32 Demand : {0, 1, 19, 37, 200})
						{
							const FCityCellCoord Anchor(X, 9);
							TestEqual(TEXT("Pressure"), FCityWaterEvaluator::GetPressure(City, Prepared, Anchor, Size),
							          FCityWaterEvaluator::GetPressure(City, Reference, Anchor, Size));
							TestEqual(TEXT("Available supply"),
							          FCityWaterEvaluator::GetAvailableAt(City, Prepared, Anchor, Size, Demand),
							          FCityWaterEvaluator::GetAvailableAt(City, Reference, Anchor, Size, Demand));
							TestEqual(TEXT("Candidate cause"),
							          FCityWaterEvaluator::EvaluateAt(City, Prepared, Anchor, Size, Demand),
							          FCityWaterEvaluator::EvaluateAt(City, Reference, Anchor, Size, Demand));
							TestEqual(TEXT("Finite ordered allocation"),
							          FCityWaterEvaluator::Allocate(City, Prepared, Consumer, Anchor, Size, Demand),
							          FCityWaterEvaluator::Allocate(City, Reference, Consumer, Anchor, Size, Demand));
							TestEqual(TEXT("Allocated cause"), Prepared.EntityCauses.FindRef(Consumer),
							          Reference.EntityCauses.FindRef(Consumer));
							TestEqual(TEXT("Physical pressure"), Prepared.EntityPressurePercent.FindRef(Consumer),
							          Reference.EntityPressurePercent.FindRef(Consumer));
							for (int32 Index = 0; Index < Prepared.Sources.Num(); ++Index)
							{
								TestEqual(TEXT("Each source budget"), Prepared.Sources[Index].Used,
								          Reference.Sources[Index].Used);
								TestTrue(TEXT("Supply is bounded"),
								         Prepared.Sources[Index].Used <= Prepared.Sources[Index].Capacity);
							}
							++Consumer;
							// Refill between consumers so distant/partial delivery gets
							// tested as well as exhausted shared supply.
							if (Consumer % 7 == 0)
								for (int32 Index = 0; Index < Prepared.Sources.Num(); ++Index)
									Prepared.Sources[Index].Used = Reference.Sources[Index].Used = 0;
						}
				TestTrue(TEXT("Prepared and reference queries were exercised"),
				         Timing.PreparedWaterQueries > 0 && Timing.ReferenceWaterQueries > 0);
				TestTrue(TEXT("All delivered records match"),
				         Prepared.EntityDelivered.OrderIndependentCompareEqual(Reference.EntityDelivered));
			}
		}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityPreparedEducationQueryTest, "CitySim.Education.PreparedAccessEquivalence",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityPreparedEducationQueryTest::RunTest(const FString& Parameters)
{
	FCityPlacementModel City(1000000);
	if (!TestTrue(TEXT("Connected road"), QueryRoad(City, City.GetEntranceCell(), {40, 8})))
		return false;
	const uint64 Homes[] = {PlaceQueryBuilding(City, TEXT("Home"), {2, 9}),
	                        PlaceQueryBuilding(City, TEXT("Home"), {20, 9}),
	                        PlaceQueryBuilding(City, TEXT("Home"), {35, 9})};
	const uint64 Schools[] = {PlaceQueryBuilding(City, TEXT("School"), {0, 9}),
	                          PlaceQueryBuilding(City, TEXT("SecondarySchool"), {10, 9}),
	                          PlaceQueryBuilding(City, TEXT("College"), {30, 9})};
	FCityPopulationSnapshot Population;
	for (int32 Index = 0; Index < 96; ++Index)
	{
		auto& Citizen = Population.Citizens.Emplace_GetRef();
		Citizen.Id = Index + 1;
		Citizen.HomeEntityId = Index % 13 == 0 ? 0 : Homes[Index % 3];
		Citizen.Education = static_cast<ECityEducationLevel>(Index % 4);
	}
	FCityEconomyTuning Tuning;
	Tuning.SchoolCapacity = 9;
	Tuning.SchoolRoadRangeHops = 24;
	FCityServiceEvaluationTiming Timing;
	for (int32 State = 0; State < 6; ++State)
	{
		FCityServiceSnapshot Initial;
		for (const auto Home : Homes)
			Initial.EntityResidents.Add(Home, 32);
		for (const auto School : Schools)
			Initial.PublicStaffPercent.Add(School, State == 1 ? 50 : 100);
		if (State == 2)
			Initial.EntityFailures.Add(Schools[0], ECityOperationFailure::NoStaff);
		if (State == 3)
		{
			TestTrue(TEXT("Road removal"), City.RemoveRoad({15, 8}));
			for (auto& Citizen : Population.Citizens)
				Citizen.Education = ECityEducationLevel::Basic;
		}
		if (State == 4)
			TestTrue(TEXT("Road reconnection"), QueryRoad(City, {14, 8}, {16, 8}));
		if (State == 5)
		{
			Tuning.SchoolCapacity = 2;
			Initial.EntityResidents.Add(Homes[1], 0);
		}
		auto Prepared = Initial, Reference = Initial;
		FCityEducationEvaluator::Build(City, Prepared, Tuning, &Population, nullptr, true, &Timing);
		FCityEducationEvaluator::Build(City, Reference, Tuning, &Population, nullptr, false, &Timing);
		TestEqual(TEXT("Demand"), Prepared.EducationDemand, Reference.EducationDemand);
		TestEqual(TEXT("Served"), Prepared.EducationServed, Reference.EducationServed);
		TestEqual(TEXT("Capacity"), Prepared.EducationCapacity, Reference.EducationCapacity);
		TestTrue(TEXT("Stable student allocation"),
		         SameStudents(Prepared.EducationCitizenIds, Reference.EducationCitizenIds));
		TestTrue(TEXT("Higher student allocation"),
		         SameStudents(Prepared.HigherEducationCitizenIds, Reference.HigherEducationCitizenIds));
		TestTrue(TEXT("Finite school usage"),
		         Prepared.SchoolUsedCapacity.OrderIndependentCompareEqual(Reference.SchoolUsedCapacity));
		TestTrue(TEXT("Home access"),
		         Prepared.HomeEducationPlaces.OrderIndependentCompareEqual(Reference.HomeEducationPlaces));
		TestTrue(TEXT("Coverage"), Prepared.EducationCoverage == Reference.EducationCoverage);
	}
	TestTrue(TEXT("Preparation is bounded by home count rather than resident count"),
	         Timing.PreparedEducationQueries > 0 &&
	             Timing.PreparedEducationQueries < Timing.ReferenceEducationCitizens);
	return true;
}

#endif
