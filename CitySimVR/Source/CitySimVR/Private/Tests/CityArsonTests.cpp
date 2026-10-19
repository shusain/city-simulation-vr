#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Commands/CityPlacementModel.h"
#include "Simulation/CityCrimeSimulation.h"
#include "Simulation/CityFireSimulation.h"
#include "Simulation/CityPopulationSimulation.h"
#include "Simulation/CityGrowthSimulation.h"
#include "Simulation/CityDevelopmentSimulation.h"
#include "Simulation/CityReportHistory.h"
#include "Persistence/CityPersistence.h"

namespace
{
struct FArsonFixture
{
	FCityPlacementModel City{10000};
	FCityCrimeSimulation Crime;
	FCityFireSimulation Fire;
	FCityPopulationSimulation People;
	FCityDevelopmentSnapshot Development;
	FCityEconomyTuning Tuning;
	FCityServiceSnapshot Services;
	uint64 Station = 0, Home = 0;
	FString Error;
	bool Ready = false;
	FArsonFixture(int32 Occupied = 0, bool Upgraded = false, bool Arrived = true)
	{
		auto Road = City.BeginRoadPreview(FCityPlacementModel::EntranceCell);
		City.UpdateRoadPreviewEnd(Road, {30, 8});
		City.CommitRoad(Road);
		auto Place = [&](const TCHAR* Name, FCityCellCoord Cell) {
			FCityBuildingDefinition D;
			D.DefinitionId = Name;
			D.DisplayName = Name;
			D.Cost = 1;
			D.Footprint = {1, 1};
			auto P = City.BeginPreview(D, Cell);
			return City.CommitPlacement(P, D).EntityId;
		};
		Station = Place(TEXT("Police"), {2, 9});
		Home = Place(TEXT("Home"), {6, 9});
		auto& State = Development.States.Add(Station);
		State.EntityId = Station;
		State.DensityLevel = Upgraded ? ECityDensityLevel::Medium : ECityDensityLevel::Low;
		Services.PublicStaffPercent.Add(Station, 100);
		FCityPopulationSnapshot P;
		P.NextCitizenId = 10;
		P.NextTransitTripId = 2;
		for (uint64 Id = 1; Id < 10; ++Id)
		{
			auto& C = P.Citizens.Emplace_GetRef();
			C.Id = Id;
			if (Id <= static_cast<uint64>(Occupied))
			{
				C.JailStationId = Station;
				C.JailReleaseStep = 1000;
				C.CurrentLocationEntityId = Station;
			}
		}
		auto& Response = P.TransitTrips.Emplace_GetRef();
		Response.Id = 1;
		Response.OriginEntityId = Response.StationEntityId = Station;
		Response.DestinationEntityId = Home;
		Response.IncidentId = 1;
		Response.Purpose = ECityTripPurpose::PoliceResponse;
		Response.VehicleType = ECityTripVehicleType::PoliceCar;
		Response.bCompleted = Arrived;
		Ready = People.RestoreState(P, Error);
		auto C = Crime.GetSnapshot();
		C.RandomState = 1;
		Crime.RestoreState(C, 0, Error);
		Crime.StartTest(Home, Error);
		Fire.StartTest(Home, Error);
		Crime.LinkArson(Home, People.GetSnapshot(), Fire);
	}
	void Advance(int64 Step)
	{
		Crime.AdvanceToStep(Step, City, Services, Development, Tuning, false, &People.GetSnapshot());
		Crime.AdvanceArson(City, Development, Tuning, Services, false, People, Fire);
	}
};
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityArsonCustodyTest, "CitySim.Safety.ArsonCaptureCustodyRelease",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityArsonCustodyTest::RunTest(const FString& Parameters)
{
	FArsonFixture F;
	TestTrue(TEXT("Fixture restores"), F.Ready);
	TestTrue(TEXT("Police/home placed"), F.Station && F.Home);
	const uint64 Offender = F.Crime.GetSnapshot().RecentIncidents.Last().OffenderCitizenId;
	TestTrue(TEXT("Arson has an actual citizen"), Offender > 0);
	F.Advance(1);
	TestEqual(TEXT("Prompt actual arrival captures"),
	          F.Crime.GetSnapshot().RecentIncidents.Last().ArsonOutcome, ECityArsonOutcome::Detained);
	const auto* C =
	    F.People.GetSnapshot().Citizens.FindByPredicate([&](const auto& P) { return P.Id == Offender; });
	if (!TestNotNull(TEXT("Offender retained"), C))
		return false;
	TestEqual(TEXT("Three month custody"), C->JailReleaseStep, int64(76));
	TestTrue(TEXT("Custody suspends jobs/travel"),
	         !C->WorkplaceEntityId && C->TripPurpose == ECityTripPurpose::None);
	F.Services.PublicJobs.Add(F.Station, 4);
	FCityEconomySnapshot Economy;
	Economy.Residents = 9;
	F.People.Reconcile(F.City, Economy, F.Tuning, &F.Services);
	C = F.People.GetSnapshot().Citizens.FindByPredicate([&](const auto& P) { return P.Id == Offender; });
	TestTrue(TEXT("Reconciliation does not rehire prisoner"), C && C->IsDetained() && !C->WorkplaceEntityId);
	F.Advance(75);
	TestTrue(TEXT("No early scheduled release"), C->IsDetained());
	F.Advance(76);
	TestFalse(TEXT("Citizen released on deadline"), C->IsDetained());
	TestEqual(TEXT("Reform prevents repeat arson for six months"), C->ReformedUntilStep, int64(226));
	TestEqual(TEXT("History reflects release"), F.Crime.GetSnapshot().RecentIncidents.Last().ArsonOutcome,
	          ECityArsonOutcome::Released);
	F.Advance(77);
	TestEqual(TEXT("Release is applied once"), C->ReformedUntilStep, int64(226));
	FArsonFixture Partial;
	Partial.Advance(1);
	auto PartialFire = Partial.Fire.GetSnapshot();
	PartialFire.DamagedBuildings.Add(Partial.Station, {20, -1, 1});
	Partial.Fire.RestoreState(PartialFire, 1, Partial.Error);
	Partial.Advance(2);
	TestEqual(TEXT("Repairable station damage retains custody"),
	          Partial.Crime.GetSnapshot().RecentIncidents.Last().ArsonOutcome, ECityArsonOutcome::Detained);
	FArsonFixture Lost;
	Lost.Advance(1);
	auto FireState = Lost.Fire.GetSnapshot();
	FireState.DamagedBuildings.Add(Lost.Station, {100, -1, 1});
	Lost.Fire.RestoreState(FireState, 1, Lost.Error);
	Lost.Advance(2);
	TestEqual(TEXT("Damaged station releases without granting reform"),
	          Lost.Crime.GetSnapshot().RecentIncidents.Last().ArsonOutcome, ECityArsonOutcome::ReleasedEarly);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityArsonCapacityTest, "CitySim.Safety.JailUpgradeAndEscape",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityArsonCapacityTest::RunTest(const FString& Parameters)
{
	FArsonFixture Full(4);
	Full.Advance(1);
	TestEqual(TEXT("Full starter cannot admit fifth prisoner"),
	          Full.Crime.GetSnapshot().RecentIncidents.Last().ArsonOutcome, ECityArsonOutcome::JailFull);
	TestEqual(TEXT("Caught offender is still counted"), Full.Crime.GetSnapshot().TotalArrests, int64(1));
	FArsonFixture Upgrade(4, true);
	Upgrade.Advance(1);
	TestEqual(TEXT("Completed upgrade admits fifth prisoner"),
	          Upgrade.Crime.GetSnapshot().RecentIncidents.Last().ArsonOutcome, ECityArsonOutcome::Detained);
	TestEqual(TEXT("Upgrade provides eight spaces"),
	          FCityCrimeSimulation::JailCapacity(Upgrade.Development, Upgrade.Station), 8);
	auto& Pending = Full.Development.States.FindChecked(Full.Station);
	Pending.PendingCivicLevel = 1;
	TestEqual(TEXT("Pending upgrade still has four spaces"),
	          FCityCrimeSimulation::JailCapacity(Full.Development, Full.Station), 4);
	FArsonFixture Outbound(0, false, false);
	Outbound.Advance(1);
	TestEqual(TEXT("Outbound car cannot arrest"), Outbound.Crime.GetSnapshot().TotalArrests, int64(0));
	Outbound.Advance(10);
	TestEqual(TEXT("Offender escapes after deadline"),
	          Outbound.Crime.GetSnapshot().RecentIncidents.Last().ArsonOutcome, ECityArsonOutcome::Escaped);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityArsonSaveTest, "CitySim.Persistence.ArsonCustodyAndReports",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityArsonSaveTest::RunTest(const FString& Parameters)
{
	FArsonFixture F;
	F.Advance(1);
	FCitySaveDocument D;
	D.Placement = F.City.ExportState();
	D.Population = F.People.GetSnapshot();
	D.Crime = F.Crime.GetSnapshot();
	D.Fire = F.Fire.GetSnapshot();
	D.Economy.FixedStepId = 1;
	D.Economy.Residents = 9;
	D.Crime.HomelessResidents = 9;
	FString Json, Error;
	FCitySaveDocument Loaded;
	if (!TestTrue(TEXT("Custody serializes"), FCityPersistence::Serialize(D, Json, Error)))
	{
		AddError(Error);
		return false;
	}
	if (!TestTrue(TEXT("Custody deserializes"), FCityPersistence::Deserialize(Json, Loaded, Error)))
	{
		AddError(Error);
		return false;
	}
	TestEqual(TEXT("Saved offender retained"), Loaded.Crime.RecentIncidents.Last().OffenderCitizenId,
	          D.Crime.RecentIncidents.Last().OffenderCitizenId);
	TestTrue(TEXT("Saved detention retained"),
	         Loaded.Population.Citizens.ContainsByPredicate(
	             [](const auto& C) { return C.IsDetained() && C.JailReleaseStep == 76; }));
	TestTrue(TEXT("Fire cause survives"), Loaded.Fire.RecentIncidents.Last().bArson);
	D.SchemaVersion = 44;
	TestFalse(TEXT("Downgrade cannot lose custody"), FCityPersistence::Serialize(D, Json, Error));
	FCityReportHistory Reports;
	FCityEconomySnapshot E;
	auto P = F.People.GetSnapshot();
	E.LastBilledMonthId = P.LastSettledMonthId = 1;
	F.Services.DetainedResidents = 1;
	TestTrue(TEXT("Safety report captures"),
	         Reports.Capture(E, P, F.Services, {}, 100, &F.Crime.GetSnapshot()));
	TestTrue(TEXT("Report census reconciles detained separately"), Reports.Validate(1, Error));
	TestEqual(TEXT("Report custody count"), Reports.Months.Last().Get(ECityReportMetric::Jailed), int64(1));
	TestEqual(TEXT("Annual arrests are end-year cumulative stock"),
	          Reports.Annual().Last().Get(ECityReportMetric::Arrests), int64(1));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityNaturalArsonTest, "CitySim.Safety.SeededNaturalArson",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityNaturalArsonTest::RunTest(const FString& Parameters)
{
	FArsonFixture A, B;
	auto Run = [](FArsonFixture& F) {
		F.Crime.Reset();
		F.Fire.Reset();
		auto People = F.People.GetSnapshot();
		People.TransitTrips.Reset();
		F.People.RestoreState(People, F.Error);
		auto& Home = F.Development.States.Add(F.Home);
		Home.EntityId = F.Home;
		F.Services.Residents = 200;
		F.Services.UnemployedResidents = 150;
		F.Services.HomelessResidents = 50;
		F.Services.PublicStaffPercent.Reset();
		F.Tuning.CrimePopulationThreshold = F.Tuning.FirePopulationThreshold = 1;
		F.Tuning.CrimeGraceMonths = F.Tuning.FireGraceMonths = 0;
		F.Tuning.FixedStepsPerMonth = 4;
		F.Tuning.CrimeCheckIntervalSteps = 1;
		F.Tuning.CrimeBaseRisk = 100;
		F.Tuning.FireBaseRisk = 0;
		F.Tuning.FireCrimeRiskWeight = 0;
		F.Tuning.FireSpreadChance = 0;
		for (int64 Step = 1; Step <= 300; ++Step)
		{
			F.Crime.AdvanceToStep(Step, F.City, F.Services, F.Development, F.Tuning, false,
			                      &F.People.GetSnapshot());
			F.Fire.AdvanceToStep(Step, F.City, F.Services, F.Development, {}, F.Tuning, false,
			                     &F.People.GetSnapshot());
			F.Crime.AdvanceArson(F.City, F.Development, F.Tuning, F.Services, false, F.People, F.Fire);
		}
	};
	Run(A);
	Run(B);
	TestTrue(TEXT("Natural crimes occasionally escalate to arson"), A.Crime.GetSnapshot().TotalArsons > 0);
	TestEqual(TEXT("Seeded arson counts repeat"), A.Crime.GetSnapshot().TotalArsons,
	          B.Crime.GetSnapshot().TotalArsons);
	TestEqual(TEXT("Crime RNG repeats"), A.Crime.GetSnapshot().RandomState,
	          B.Crime.GetSnapshot().RandomState);
	TestEqual(TEXT("Fire RNG repeats"), A.Fire.GetSnapshot().RandomState, B.Fire.GetSnapshot().RandomState);
	TestTrue(TEXT("Shared fire caps remain bounded"),
	         A.Fire.GetSnapshot().FireDamageCount() <= FCityFireSimulation::MaximumDamagedBuildings);
	TestTrue(TEXT("Natural arson carries a citizen and real-event cause"),
	         A.Fire.GetSnapshot().RecentIncidents.ContainsByPredicate(
	             [](const auto& I) { return I.bArson && I.OffenderCitizenId && !I.bTest; }));
	return true;
}
#endif
