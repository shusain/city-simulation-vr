#if WITH_DEV_AUTOMATION_TESTS
#include "Simulation/CityHealthSimulation.h"
#include "Persistence/CityPersistence.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityHealthFoundationTest, "CitySim.Health.CapacityArrivalRecoveryAndSave",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FCityHealthFoundationTest::RunTest(const FString& Parameters)
{
	FCityHealthSimulation Sim;
	FCityHealthContext C;
	FCityHealthTuning T;
	T.BaseRisk = 0;
	for (uint64 Id = 1; Id <= 4; ++Id)
		C.Citizens.Add({Id, 0, false});
	C.Hospitals.Add({99, 2, true});
	TestTrue(TEXT("Mild illness starts"), Sim.StartIllness(1, ECityIllness::Mild));
	TestFalse(TEXT("One patient per citizen"), Sim.StartIllness(1, ECityIllness::Severe));
	TestTrue(TEXT("Severe illness starts"), Sim.StartIllness(2, ECityIllness::Severe));
	Sim.StartIllness(3, ECityIllness::Mild);
	TestFalse(TEXT("Nine sidewalk steps cannot walk in"), Sim.ReserveCare(1,99,ECityHealthCare::WalkIn,9,C));
	TestTrue(TEXT("Eight sidewalk steps walk in"), Sim.ReserveCare(1,99,ECityHealthCare::WalkIn,8,C));
	TestFalse(TEXT("Severe cases cannot reserve outpatient care"), Sim.ReserveCare(2,99,ECityHealthCare::DriveIn,9,C));
	TestTrue(TEXT("Severe ambulance reserves a separate real bed"), Sim.ReserveCare(2,99,ECityHealthCare::AwaitingAmbulance,0,C));
	TestTrue(TEXT("Second outpatient does not consume a ward bed"), Sim.ReserveCare(3,99,ECityHealthCare::DriveIn,9,C));
	Sim.StartIllness(4,ECityIllness::Mild);
	TestFalse(TEXT("Outpatient reservations are finite"), Sim.ReserveCare(4,99,ECityHealthCare::DriveIn,9,C));
	Sim.AdvanceToStep(25,C,T);
	TestEqual(TEXT("Travel grants no treatment"), Sim.GetSnapshot().Find(1)->TreatmentSteps,int64(0));
	TestFalse(TEXT("Wrong hospital cannot dispense"),Sim.ConfirmArrival(1,98));
	TestTrue(TEXT("Real visit dispenses a home course"),Sim.ConfirmArrival(1,99));
	TestFalse(TEXT("Dispensing is idempotent"),Sim.ConfirmArrival(1,99));
	TestTrue(TEXT("Ambulance arrival admits severe case"),Sim.ConfirmArrival(2,99));
	TestFalse(TEXT("Admission is idempotent"),Sim.ConfirmArrival(2,99));
	TestFalse(TEXT("Mild returning patient uses no ward/clinic place"),Sim.GetSnapshot().Find(1)->UsesBed() || Sim.GetSnapshot().Find(1)->UsesOutpatientPlace());
	Sim.AdvanceToStep(35,C,T);
	TestEqual(TEXT("Return travel cannot heal at home early"),Sim.GetSnapshot().Find(1)->TreatmentSteps,int64(0));
	TestTrue(TEXT("Actual home arrival begins home recovery"),Sim.ConfirmHome(1));
	TestFalse(TEXT("Home arrival is idempotent"),Sim.ConfirmHome(1));
	C.Hospitals[0].bOperational=false;
	Sim.AdvanceToStep(45,C,T);
	TestEqual(TEXT("Medicine continues during later hospital outage"),Sim.GetSnapshot().Find(1)->TreatmentSteps,int64(10));
	TestEqual(TEXT("Ward outage pauses inpatient care"),Sim.GetSnapshot().Find(2)->TreatmentSteps,int64(10));
	FCityHealthSimulation Resumed;FString Error;
	TestTrue(TEXT("Home/ward/visit state restores"),Resumed.RestoreState(Sim.GetSnapshot(),Error));
	Sim.AdvanceToStep(60,C,T);Resumed.AdvanceToStep(60,C,T);
	TestFalse(TEXT("Mild course completes at home"),Sim.GetSnapshot().Find(1)!=nullptr);
	TestEqual(TEXT("Resume matches recovery"),Resumed.GetSnapshot().TotalRecovered,Sim.GetSnapshot().TotalRecovered);
	const auto Pending=Sim.GetSnapshot();
	C.Hospitals.Reset();Sim.AdvanceToStep(90,C,T);
	TestTrue(TEXT("Demolished provider releases outstanding visit without healing"),Sim.GetSnapshot().Find(3)->Care==ECityHealthCare::None);

	auto Bad = Sim.GetSnapshot();
	const auto Duplicate = Bad.Patients[0];
	Bad.Patients.Add(Duplicate);
	TestFalse(TEXT("Duplicate save patient rejects restore"), Resumed.RestoreState(Bad, Error));
	// Full current-city JSON/trip round trips are exercised by CurrentCityTravelHomeAndReload.

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityHealthIntroductionTest, "CitySim.Health.IntroductionGraceAndCatchup",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FCityHealthIntroductionTest::RunTest(const FString& Parameters)
{
	FCityHealthContext C;
	C.SecondaryResidents = 25;
	C.bSafetyEstablished = true;
	for (uint64 Id = 1; Id <= 250; ++Id)
		C.Citizens.Add({Id, 0, false});
	FCityHealthTuning T;
	T.BaseRisk = 100;
	FCityHealthSimulation Fine, Batched;
	C.bSafetyEstablished = false;
	Fine.AdvanceToStep(1, C, T);
	TestEqual(TEXT("Safety establishment precedes illness"), Fine.GetSnapshot().IntroductionStep, int64(-1));
	C.bSafetyEstablished = true;
	Fine.AdvanceToStep(2, C, T);
	Batched.AdvanceToStep(2, C, T);
	Fine.AdvanceToStep(151, C, T);
	TestEqual(TEXT("Introduction grace has no sickness"), Fine.GetSnapshot().Patients.Num(), 0);
	for (int64 Step = 152; Step <= 177; ++Step)
		Fine.AdvanceToStep(Step, C, T);
	Batched.AdvanceToStep(177, C, T);
	TestEqual(TEXT("Risk runs after grace"), Fine.GetSnapshot().Patients.Num(), 250);
	for (const auto& P : Fine.GetSnapshot().Patients)
	{
		const auto* Q = Batched.GetSnapshot().Find(P.CitizenId);
		if (TestNotNull(TEXT("Catchup keeps patient identity"), Q))
		{
			TestEqual(TEXT("Catchup keeps onset"), P.OnsetStep, Q->OnsetStep);
			TestEqual(TEXT("Catchup keeps illness"), P.Illness, Q->Illness);
		}
	}
	FCityTransitTripRecord Trip;
	Trip.Purpose = static_cast<ECityTripPurpose>(static_cast<uint8>(ECityTripPurpose::EmergencyReturn) + 1);
	TestFalse(TEXT("Future trip types do not silently gain emergency priority"), Trip.IsEmergency());
	FCityHealthSimulation OtherSeed;
	T.RandomSeed = 1337;
	OtherSeed.AdvanceToStep(2, C, T);
	OtherSeed.AdvanceToStep(177, C, T);
	bool Different = false;
	for (const auto& P : Fine.GetSnapshot().Patients)
		if (const auto* Q = OtherSeed.GetSnapshot().Find(P.CitizenId))
			Different |= Q->Illness != P.Illness;
	TestTrue(TEXT("Different city seeds produce different illness profiles"), Different);
	FString Error;
	TestTrue(TEXT("Catchup state is valid"),
	         FCityHealthSimulation::ValidateState(Batched.GetSnapshot(), Error));
	return true;
}
#endif
