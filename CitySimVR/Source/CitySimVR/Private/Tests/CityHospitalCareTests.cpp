#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Commands/CityPlacementModel.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Simulation/CityHealthSimulation.h"
#include "Simulation/CityPopulationSimulation.h"
#include "Simulation/CityTrafficSimulation.h"
#include "Simulation/CityGrowthSimulation.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityHospitalCareTest, "CitySim.Health.ActualRoutesPickupAdmissionAndRecovery",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FCityHospitalCareTest::RunTest(const FString& Parameters)
{
	for (const bool Prepared : {false, true})
	{
		FCityPlacementModel City(100000);
		auto R = City.BeginRoadPreview(FCityPlacementModel::EntranceCell);
		City.UpdateRoadPreviewEnd(R, {30, 8});
		TestTrue(TEXT("Road committed"), City.CommitRoad(R).IsSuccess());
		FCityBuildingDefinition D;
		D.DefinitionId = TEXT("Home");
		D.DisplayName = TEXT("Home");
		D.Footprint = {1, 1};
		D.Cost = 0;
		auto Place = [&](int32 X)
		{
			auto P = City.BeginPreview(D, {X, 9});
			return City.CommitPlacement(P, D).EntityId;
		};
		const uint64 Near = Place(4), Far = Place(3), Severe = Place(20);
		D.DefinitionId = TEXT("Hospital");
		const uint64 Hospital = Place(12);
		FCityRoadNetworkSnapshot Network;
		FCityRoadNetworkBuilder::Build(City, FCityRoadDefinition::Local(), Network);
		FCityPopulationSnapshot Initial;
		Initial.NextCitizenId = 4;
		for (int32 I = 0; I < 3; ++I)
		{
			auto& C = Initial.Citizens.Emplace_GetRef();
			C.Id = I + 1;
			C.HomeEntityId = I == 0 ? Near : I == 1 ? Far : Severe;
			C.HouseholdId = C.HomeEntityId;
			C.CurrentLocationEntityId = C.HomeEntityId;
			C.AvailableMoney = 37;
			C.EducationMonths = 2;
			C.UnemployedSinceStep = 0;
			auto& H = Initial.Households.Emplace_GetRef();
			H.Id = H.HomeEntityId = C.HomeEntityId;
		}
		FCityPopulationSimulation People;
		FString Error;
		TestTrue(TEXT("Population fixture valid"), People.RestoreState(Initial, Error));
		FCityServiceSnapshot Services;
		Services.HospitalCapacities.Add(Hospital, 3);
		Services.HospitalFleetCapacities.Add(Hospital, 1);
		Services.EntityFailures.Add(Hospital, ECityOperationFailure::None);
		FCityHealthSimulation Health;
		Health.StartIllness(1, ECityIllness::Mild);
		Health.StartIllness(2, ECityIllness::Mild);
		Health.StartIllness(3, ECityIllness::Severe);
		People.UpdateHospitalCare(Health, City, nullptr, Network, Services, 1, 25, 7429, false, true, nullptr, nullptr,
		                          Prepared);
		TestTrue(TEXT("Eight sidewalk hops walks"), Health.GetSnapshot().Find(1)->Care == ECityHealthCare::WalkIn);
		TestTrue(TEXT("Nine hops uses a personal car"),
		         Health.GetSnapshot().Find(2)->Care == ECityHealthCare::DriveIn);
		TestTrue(TEXT("Severe reserves ambulance"),
		         Health.GetSnapshot().Find(3)->Care == ECityHealthCare::AwaitingAmbulance);
		TestEqual(TEXT("No healing before travel"), Health.GetSnapshot().Find(3)->TreatmentSteps, int64(0));
		TestEqual(TEXT("One walker, one personal car and one ambulance"), People.GetSnapshot().TransitTrips.Num(), 3);
		FCityTrafficSimulation Traffic;
		bool SawPickup = false, SawAdmission = false, SawHospital = false, SawHomeReturn = false;
		for (int64 Step = 1; Step <= 300; ++Step)
		{
			People.UpdateHospitalCare(Health, City, nullptr, Network, Services, Step, 25, 7429, false, true, nullptr,
			                          nullptr, Prepared);
			Traffic.UpdateTrips(City, nullptr, People.GetSnapshot(), Network, Step);
			Traffic.AdvanceOneStep(Network, Step);
			for (const auto& V : Traffic.GetSnapshot().Vehicles)
				if (V.bTransitTrip && V.bCompleted)
					People.CompleteTransitTrip(V.SourceId, Step);
			const auto* Pickup = People.GetSnapshot().TransitTrips.FindByPredicate(
			    [](const auto& T) { return T.Purpose == ECityTripPurpose::HealthPickup && T.bCompleted; });
			if (Pickup)
			{
				SawPickup = true;
				TestTrue(TEXT("Pickup has not admitted"),
				         Health.GetSnapshot().Find(3)->Care == ECityHealthCare::AwaitingAmbulance);
			}
			People.UpdateHospitalCare(Health, City, nullptr, Network, Services, Step, 25, 7429, false, false, nullptr,
			                          nullptr, Prepared);
			SawAdmission |= People.GetSnapshot().TransitTrips.ContainsByPredicate(
			    [](const auto& T) { return T.Purpose == ECityTripPurpose::HealthAdmission; });
			if (const auto* P = Health.GetSnapshot().Find(3))
				SawHospital |= P->Care == ECityHealthCare::Hospital;
			for (const auto& Patient:Health.GetSnapshot().Patients)
				if (Patient.Illness==ECityIllness::Mild)
				{
					TestFalse(TEXT("Mild cases never reserve or occupy beds"),Patient.UsesBed());
					SawHomeReturn |= Patient.Care==ECityHealthCare::ReturningHome;
					if (Patient.Care!=ECityHealthCare::HomeRecovery) TestEqual(TEXT("No healing before home arrival"),Patient.TreatmentSteps,int64(0));
				}
			Traffic.UpdateTrips(City, nullptr, People.GetSnapshot(), Network, Step);
		}
		TestTrue(TEXT("Outpatients have a real homeward journey"),SawHomeReturn);
		TestTrue(TEXT("Visible traffic reported pickup"), SawPickup);
		TestTrue(TEXT("Distinct transport journey"), SawAdmission);
		TestTrue(TEXT("Admitted only after second arrival"), SawHospital);
		TestEqual(TEXT("All real treatment completes"), Health.GetSnapshot().TotalRecovered, int64(3));
		TestEqual(TEXT("No leftover beds or journeys"),
		          People.GetSnapshot().TransitTrips.Num() + Health.GetSnapshot().Patients.Num(), 0);
		for (const auto& C : People.GetSnapshot().Citizens)
		{
			TestEqual(TEXT("Wallet retained"), C.AvailableMoney, int64(37));
			TestEqual(TEXT("Partial education retained"), C.EducationMonths, 2);
			TestFalse(TEXT("Leave clears after recovery"), C.bOnMedicalLeave);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityHospitalCareOutageTest, "CitySim.Health.TransportOutageRouteLossAndRetry",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FCityHospitalCareOutageTest::RunTest(const FString& Parameters)
{
	for (const bool Prepared : {false, true})
	{
		FCityPlacementModel City(100000);
		auto R = City.BeginRoadPreview(FCityPlacementModel::EntranceCell);
		City.UpdateRoadPreviewEnd(R, {20, 8});
		City.CommitRoad(R);
		FCityBuildingDefinition D;
		D.DefinitionId = TEXT("Home");
		D.DisplayName = TEXT("Home");
		D.Footprint = {1, 1};
		D.Cost = 0;
		auto P = City.BeginPreview(D, {5, 9});
		const uint64 Home = City.CommitPlacement(P, D).EntityId;
		D.DefinitionId = TEXT("Hospital");
		P = City.BeginPreview(D, {15, 9});
		const uint64 Hospital = City.CommitPlacement(P, D).EntityId;
		FCityPopulationSnapshot S;
		S.NextCitizenId = 2;
		auto& C = S.Citizens.Emplace_GetRef();
		C.Id = 1;
		C.HomeEntityId = C.HouseholdId = Home;
		C.CurrentLocationEntityId = Home;
		C.UnemployedSinceStep = 0;
		auto& H = S.Households.Emplace_GetRef();
		H.Id = H.HomeEntityId = Home;
		FCityPopulationSimulation People;
		FString Error;
		TestTrue(TEXT("People restore"), People.RestoreState(S, Error));
		FCityHealthSimulation Health;
		Health.StartIllness(1, ECityIllness::Severe);
		FCityServiceSnapshot Services;
		Services.HospitalCapacities.Add(Hospital, 1);
		Services.HospitalFleetCapacities.Add(Hospital, 1);
		FCityRoadNetworkSnapshot Network;
		FCityRoadNetworkBuilder::Build(City, FCityRoadDefinition::Local(), Network);
		People.UpdateHospitalCare(Health, City, nullptr, Network, Services, 1, 25, 1, false, true, nullptr, nullptr,
		                          Prepared);
		TestEqual(TEXT("Single reserved dispatch"), People.GetSnapshot().TransitTrips.Num(), 1);
		People.UpdateHospitalCare(Health, City, nullptr, Network, Services, 1, 25, 1, false, false, nullptr, nullptr,
		                          Prepared);
		TestEqual(TEXT("Retry is idempotent"), People.GetSnapshot().TransitTrips.Num(), 1);
		Services.HospitalCapacities.Add(Hospital, 0);
		People.UpdateHospitalCare(Health, City, nullptr, Network, Services, 2, 25, 1, false, true, nullptr, nullptr,
		                          Prepared);
		TestEqual(TEXT("Outage removes journey"), People.GetSnapshot().TransitTrips.Num(), 0);
		TestTrue(TEXT("Outage releases bed without healing"),
		         Health.GetSnapshot().Find(1)->HospitalId == 0 && Health.GetSnapshot().Find(1)->TreatmentSteps == 0);
		Services.HospitalCapacities.Add(Hospital, 1);
		People.UpdateHospitalCare(Health, City, nullptr, Network, Services, 3, 25, 1, false, true, nullptr, nullptr,
		                          Prepared);
		TestEqual(TEXT("Restored hospital retries"), People.GetSnapshot().TransitTrips.Num(), 1);
		Network = FCityRoadNetworkSnapshot();
		People.UpdateHospitalCare(Health, City, nullptr, Network, Services, 4, 25, 1, false, true, nullptr, nullptr,
		                          Prepared);
		TestEqual(TEXT("No route leaves no stranded reserved bed"), Health.GetSnapshot().Find(1)->HospitalId,
		          uint64(0));
		TestEqual(TEXT("No route no vehicle"), People.GetSnapshot().TransitTrips.Num(), 0);
		TestEqual(TEXT("Unemployment clock pauses"), People.GetSnapshot().Citizens[0].UnemployedSinceStep, int64(3));
	}
	return true;
}
#endif
