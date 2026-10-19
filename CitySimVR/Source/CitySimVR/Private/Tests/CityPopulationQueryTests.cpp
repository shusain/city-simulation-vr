#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Commands/CityPlacementModel.h"
#include "Performance/CityEvaluationTiming.h"
#include "Persistence/CityPersistence.h"
#include "Simulation/CityGrowthSimulation.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityPreparedPopulationQueriesTest,
                                 "CitySim.Population.PreparedQueryEquivalence",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityPreparedPopulationQueriesTest::RunTest(const FString& Parameters)
{
	FCityPlacementModel City(1000000);
	FCityServiceSnapshot Services;
	TArray<uint64> Homes, Shops, Factories, Public;
	const auto Place = [&](const FName Id, const int32 Index) {
		FCityBuildingDefinition Definition;
		Definition.DefinitionId = Id;
		Definition.DisplayName = Id.ToString();
		Definition.Footprint = FCityCellFootprint(1, 1);
		Definition.LogicalHeightCm = 100;
		auto Preview = City.BeginPreview(Definition, {1 + Index % 30 * 2, 1 + Index / 30 * 2});
		const auto Result = City.CommitPlacement(Preview, Definition);
		TestTrue(TEXT("Fixture lot commits"), Result.IsSuccess());
		return Result.EntityId;
	};
	for (int32 Index = 0; Index < 40; ++Index)
	{
		const uint64 Id = Place(TEXT("Home"), Index);
		Homes.Add(Id);
		Services.EntityResidents.Add(Id, 10);
	}
	for (int32 Index = 0; Index < 12; ++Index)
	{
		const bool Industrial = Index >= 8;
		const uint64 Id = Place(Industrial ? TEXT("Factory") : TEXT("Shop"), 40 + Index);
		(Industrial ? Factories : Shops).Add(Id);
		Services.EntityJobs.Add(Id, 20);
		Services.EntitySkilledJobs.Add(Id, Index % 3 * 4);
		Services.EntityOutput.Add(Id, 40);
	}
	for (int32 Index = 0; Index < 12; ++Index)
	{
		const uint64 Id = Place(TEXT("School"), 52 + Index);
		Public.Add(Id);
		Services.PublicJobs.Add(Id, 3 + Index % 4);
		Services.PublicMinimumEducation.Add(Id, Index % 4);
	}
	FCityEconomyTuning Tuning;
	FCityEconomySnapshot Economy;
	Economy.Residents = 480;
	Economy.Jobs = 294;
	Economy.RetailCapacity = 320;
	Economy.GoodsProduced = 160;
	Economy.GoodsSold = 100;
	FCityPopulationSimulation Prepared, Reference;
	const auto Json = [&](const FCityPopulationSnapshot& State) {
		FCitySaveDocument Document;
		Document.Placement = City.ExportState();
		Document.Population = State;
		FString Result, Error;
		TestTrue(TEXT("Full population serialization: ") + Error,
		         FCityPersistence::Serialize(Document, Result, Error));
		return Result;
	};
	const auto EqualState = [&](const FCityPopulationSimulation& A, const FCityPopulationSimulation& B) {
		TestEqual(TEXT("Every serialized identity, wallet, ledger, reservation and trip is identical"),
		          Json(A.Snapshot), Json(B.Snapshot));
		TestEqual(TEXT("Citizen count"), A.Snapshot.Citizens.Num(), B.Snapshot.Citizens.Num());
		for (int32 Index = 0; Index < A.Snapshot.Citizens.Num(); ++Index)
			TestEqual(TEXT("Derived medical leave remains identical"),
			          A.Snapshot.Citizens[Index].bOnMedicalLeave, B.Snapshot.Citizens[Index].bOnMedicalLeave);
	};
	Prepared.Reconcile(City, Economy, Tuning, &Services, true);
	Reference.Reconcile(City, Economy, Tuning, &Services, false);
	EqualState(Prepared, Reference);
	for (int32 Round = 0; Round < 5; ++Round)
	{
		for (auto* Simulation : {&Prepared, &Reference})
		{
			for (auto& Citizen : Simulation->Snapshot.Citizens)
			{
				Citizen.Education = static_cast<ECityEducationLevel>((Citizen.Id + Round) % 4);
				Citizen.bOnMedicalLeave = Citizen.Id % 17 == 0;
				Citizen.AvailableMoney = Citizen.Id % 23;
				if (Citizen.Id % 19 == 0)
				{
					Citizen.JailStationId = Public[0];
					Citizen.JailReleaseStep = 1000;
				}
				if (Round == 1 && Citizen.Id % 3 == 0)
					Citizen.WorkplaceEntityId =
					    0; // Vacancies after retained workers and changing qualifications.
			}
			for (auto& Business : Simulation->Snapshot.Businesses)
			{
				Business.Balance = (Business.EntityId % 7) * 13;
				Business.Inventory = Round == 3 ? 0 : 53;
				Business.bInitialFreightReceived = true;
			}
		}
		if (Round == 2)
		{
			Services.PublicJobs.FindChecked(Public[1]) = 0;
			Services.EntityResidents.FindChecked(Homes[0]) =
			    0; // Temporary housing outage retains physical homes.
			Services.EntityJobs.FindChecked(Shops[0]) = 0;
		}
		if (Round == 3)
		{
			Services.PublicJobs.FindChecked(Public[1]) = 6;
			Services.EntityResidents.FindChecked(Homes[0]) = 10;
			Services.EntityJobs.FindChecked(Shops[0]) = 20;
		}
		if (Round == 4)
		{
			TestTrue(TEXT("Home demolition"), City.DemolishEntity(Homes.Last()).IsSuccess());
			TestTrue(TEXT("Employer demolition"), City.DemolishEntity(Factories.Last()).IsSuccess());
		}
		Economy.FixedStepId = Round == 4 ? 500 : Round * 25;
		Prepared.Reconcile(City, Economy, Tuning, &Services, true);
		Reference.Reconcile(City, Economy, Tuning, &Services, false);
		EqualState(Prepared, Reference);
		for (const int32 Wage : {0, 5, 9})
			for (const bool Freight : {false, true})
				for (const bool Reservations : {false, true})
					for (const int64 Budget : {int64(0), int64(1), int64(13), int64(100000)})
					{
						auto A = Prepared, B = Reference;
						Tuning.HouseholdIncomePerEmployedResident = Wage;
						Tuning.HouseholdIncomePerGoodsPurchase = Wage == 0 ? 0 : 5;
						A.SettleMonth(Economy, Tuning, Freight, Reservations, Budget, true);
						B.SettleMonth(Economy, Tuning, Freight, Reservations, Budget, false);
						EqualState(A, B);
						// A second settlement must rebuild indexes around existing reservations and depleted
						// balances.
						A.SettleMonth(Economy, Tuning, Freight, Reservations, Budget, true);
						B.SettleMonth(Economy, Tuning, Freight, Reservations, Budget, false);
						EqualState(A, B);
					}
		FCityForecastTiming Timing;
		int64 PayrollA, PayrollB, PublicA, PublicB;
		int32 OutputA, OutputB, EmployedA, EmployedB;
		const FString Before = Json(Prepared.Snapshot);
		TestEqual(TEXT("Forecast sales match"),
		          FCityPopulationSimulation::ForecastGoodsSales(City, Economy, Tuning, Services,
		                                                        Prepared.Snapshot, &PayrollA, &OutputA,
		                                                        &PublicA, &EmployedA, &Timing, true),
		          FCityPopulationSimulation::ForecastGoodsSales(City, Economy, Tuning, Services,
		                                                        Reference.Snapshot, &PayrollB, &OutputB,
		                                                        &PublicB, &EmployedB, &Timing, false));
		TestEqual(TEXT("Payroll forecast"), PayrollA, PayrollB);
		TestEqual(TEXT("Public payroll forecast"), PublicA, PublicB);
		TestEqual(TEXT("Output forecast"), OutputA, OutputB);
		TestEqual(TEXT("Employment forecast"), EmployedA, EmployedB);
		TestEqual(TEXT("Prepared path observed"), Timing.PreparedQueries, uint64(1));
		TestEqual(TEXT("Reference path observed"), Timing.ReferenceQueries, uint64(1));
		TestEqual(TEXT("Forecast never changes input population"), Json(Prepared.Snapshot), Before);
		EqualState(Prepared, Reference);
	}
	return true;
}
#endif
