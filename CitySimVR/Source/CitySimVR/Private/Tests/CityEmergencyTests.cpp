#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Commands/CityPlacementModel.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Simulation/CityPopulationSimulation.h"
#include "Simulation/CityGrowthSimulation.h"
#include "Simulation/CityEconomySimulation.h"
#include "Simulation/CityDevelopmentSimulation.h"
#include "Simulation/CityEnvironmentSimulation.h"
#include "Simulation/CityFireSimulation.h"
#include "Simulation/CityCrimeSimulation.h"
#include "Simulation/CityTrafficSimulation.h"
#include "Persistence/CityPersistence.h"
#include "Engine/StaticMesh.h"
#include "Presentation/CityEmergencyPresentation.h"
#include "Presentation/CityBoardActor.h"
#include "Particles/ParticleSystemComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/World.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Interaction/CityPaletteModel.h"
#include "Misc/Paths.h"
#include "Misc/App.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleEmitter.h"
namespace
{
FCityBuildingDefinition CivicDef(const TCHAR* Id)
{
	FCityBuildingDefinition D;
	D.DefinitionId = Id;
	D.DisplayName = Id;
	D.Footprint = {1, 1};
	D.Cost = 200;
	if (D.DefinitionId == TEXT("Fire") || D.DefinitionId == TEXT("Police"))
	{
		D.PowerDemand = 10;
		D.WaterDemand = 5;
		D.MonthlyUpkeep = 20;
	}
	return D;
}
uint64 CivicPlace(FCityPlacementModel& City, const TCHAR* Id, FCityCellCoord C)
{
	const auto D = CivicDef(Id);
	auto P = City.BeginPreview(D, C);
	return City.CommitPlacement(P, D).EntityId;
}
void CivicRoad(FCityPlacementModel& City)
{
	auto P = City.BeginRoadPreview(FCityPlacementModel::EntranceCell);
	City.UpdateRoadPreviewEnd(P, {63, 8});
	City.CommitRoad(P);
}
FCityFireSnapshot CivicBurning(uint64 Home)
{
	FCityFireSnapshot S;
	S.LastFixedStepId = 1;
	S.ActivationStep = 0;
	S.GraceEndStep = 0;
	S.NextIgnitionStep = 1000;
	S.TotalIncidents = 1;
	S.RecentIncidents.Add({1, Home, 0, 1, -1, 0});
	S.DamagedBuildings.Add(Home, {1, -1});
	return S;
}
} // namespace
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityPublicPayrollTest, "CitySim.Safety.PublicStaffTreasuryPayroll",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityPublicPayrollTest::RunTest(const FString& Parameters)
{
	FCityPlacementModel City(100000);
	CivicRoad(City);
	CivicPlace(City, TEXT("Power"), {1, 9});
	CivicPlace(City, TEXT("Service"), {2, 9});
	CivicPlace(City, TEXT("Home"), {4, 9});
	const uint64 Police = CivicPlace(City, TEXT("Police"), {8, 9}),
	             Fire = CivicPlace(City, TEXT("Fire"), {12, 9});
	auto* Catalog = NewObject<UCityBuildingCatalog>();
	Catalog->Definitions = {CivicDef(TEXT("Police")), CivicDef(TEXT("Fire"))};
	FCityEconomyTuning T;
	T.NewArrivalUneducatedPercent = 0; // Exact payroll fixture.
	T.FixedStepSeconds = .125;
	T.FixedStepsPerMonth = 4;
	FCityEconomySimulation Economy;
	Economy.SetTaxRates(0, 0);
	FCityPopulationSimulation Population;
	FCityDevelopmentSnapshot D;
	const int64 Before = City.GetFunds();
	auto Advance = [&]() {
		Economy.Advance(.125, City, T, Catalog, &D, &Population.GetSnapshot(), [&]() {
			const auto Services = FCityServiceEvaluator::BuildSnapshot(City, T, Catalog, &D, nullptr,
			                                                           &Population.GetSnapshot());
			Population.AdvanceToMonth(City, Economy.GetSnapshot(), T, &Services, true, true);
		});
	};
	for (int32 I = 0; I < 4; ++I)
		Advance();
	TestEqual(TEXT("Civic job slots join city total"), Economy.GetSnapshot().Jobs, 12);
	TestEqual(TEXT("Existing residents fill civic work without duplication"),
	          Population.BuildAggregate(T).EmployedResidents, 10);
	TestEqual(TEXT("Salary paid from treasury"), Economy.GetSnapshot().LastPublicPayrollPaid, int64(50));
	int64 Wallets = 0;
	for (const auto& C : Population.GetSnapshot().Citizens)
		Wallets += C.AvailableMoney;
	TestEqual(TEXT("Exact treasury salary reaches wallets"), Wallets, int64(50));
	TestEqual(TEXT("Single treasury debit includes salaries and operating bills"), Before - City.GetFunds(),
	          Economy.GetSnapshot().LastUpkeep);
	TestTrue(TEXT("Civic monthly accounting reconciles"),
	         Population.MatchesEconomy(Economy.GetSnapshot(), T));
	TestEqual(TEXT("Full police staffing"), Population.GetSnapshot().StaffPercent(Police), 100);
	TestEqual(TEXT("Partial fire staffing scales capacity"), Population.GetSnapshot().StaffPercent(Fire), 50);
	auto State = Population.GetSnapshot();
	FString Error;
	FCityPopulationSimulation Restored;
	TestTrue(TEXT("Public ledger restore"), Restored.RestoreState(State, Error));
	State.Citizens[0].Education = ECityEducationLevel::None;
	TestTrue(TEXT("Education state restores"), Restored.RestoreState(State, Error));
	const auto EligibilityServices =
	    FCityServiceEvaluator::BuildSnapshot(City, T, Catalog, &D, nullptr, &Restored.GetSnapshot());
	Restored.Reconcile(City, Economy.GetSnapshot(), T, &EligibilityServices);
	TestEqual(TEXT("Public starter role requires Basic education"),
	          Restored.GetSnapshot().Citizens[0].WorkplaceEntityId, uint64(0));
	const auto Services =
	    FCityServiceEvaluator::BuildSnapshot(City, T, Catalog, &D, nullptr, &Population.GetSnapshot());
	TestEqual(TEXT("Paid Basic utility workers produce full power"), Services.PowerCapacity, 200);
	TestEqual(TEXT("Paid Basic utility workers produce full water"), Services.ServiceCapacity, 200);
	Population.AdvanceToMonth(City, Economy.GetSnapshot(), T, &Services, true, true);
	int64 Again = 0;
	for (const auto& C : Population.GetSnapshot().Citizens)
		Again += C.AvailableMoney;
	TestEqual(TEXT("No duplicate salary on same month/save clock"), Again, Wallets);
	const int64 OrdinaryBills = Economy.GetSnapshot().LastUpkeep - Economy.GetSnapshot().LastPublicPayrollDue;
	City.TrySpendFunds(City.GetFunds() - OrdinaryBills - 35);
	for (int32 I = 0; I < 4; ++I)
		Advance();
	TestEqual(TEXT("Partial treasury funding transfers exactly affordable wages"),
	          Economy.GetSnapshot().LastPublicPayrollPaid, int64(35));
	TestEqual(TEXT("Part-paid police salaries scale staff output"),
	          Population.GetSnapshot().StaffPercent(Police), 75);
	TestFalse(TEXT("Salary gap does not pretend operating bills are unpaid"),
	          Economy.GetSnapshot().HasOperatingUpkeepShortfall());
	TestTrue(TEXT("Partial public payroll also reconciles"),
	         Population.MatchesEconomy(Economy.GetSnapshot(), T));
	City.TrySpendFunds(City.GetFunds());
	for (int32 I = 0; I < 4; ++I)
		Advance();
	TestEqual(TEXT("No treasury means no invented salary"), Economy.GetSnapshot().LastPublicPayrollPaid,
	          int64(0));
	TestEqual(TEXT("Unpaid department has zero capacity"), Population.GetSnapshot().StaffPercent(Police), 0);
	const auto UnpaidServices =
	    FCityServiceEvaluator::BuildSnapshot(City, T, Catalog, &D, nullptr, &Population.GetSnapshot());
	TestEqual(TEXT("Settled unpaid power retains growth baseline"), UnpaidServices.PowerCapacity, 100);
	TestEqual(TEXT("Settled unpaid water retains growth baseline"), UnpaidServices.ServiceCapacity, 100);
	TestTrue(TEXT("Shortfall monthly accounting reconciles"),
	         Population.MatchesEconomy(Economy.GetSnapshot(), T));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityUtilityBaselineTest, "CitySim.Growth.UtilityMinimumSupply",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityUtilityBaselineTest::RunTest(const FString& Parameters)
{
	FCityPlacementModel City(100000);
	CivicRoad(City);
	const uint64 Power = CivicPlace(City, TEXT("Power"), {1, 9}),
	             Water = CivicPlace(City, TEXT("Service"), {3, 9});
	FCityEconomyTuning T;
	FCityPopulationSnapshot Population;
	auto Services = [&]() {
		return FCityServiceEvaluator::BuildSnapshot(City, T, nullptr, nullptr, nullptr, &Population);
	};
	TestEqual(TEXT("No residents/staff still supplies 100 power"), Services().PowerCapacity, 100);
	TestEqual(TEXT("No residents/staff still supplies 100 water"), Services().ServiceCapacity, 100);
	for (const uint64 Id : {Power, Water})
	{
		Population.PublicEmployers.Add({Id, 2, 10, 0});
		for (int32 I = 0; I < 2; ++I)
		{
			FCityCitizenRecord C;
			C.Id = Population.Citizens.Num() + 1;
			C.WorkplaceEntityId = Id;
			Population.Citizens.Add(C);
		}
	}
	TestEqual(TEXT("Staff present but unpaid retains baseline power"), Services().PowerCapacity, 100);
	TestEqual(TEXT("Staff present but unpaid retains baseline water"), Services().ServiceCapacity, 100);
	for (auto& E : Population.PublicEmployers)
		E.PayrollPaid = E.PayrollDue = 20;
	TestEqual(TEXT("Fully staffed power still reaches nominal 200"), Services().PowerCapacity, 200);
	TestEqual(TEXT("Fully staffed water still reaches nominal 200"), Services().ServiceCapacity, 200);
	for (auto& E : Population.PublicEmployers)
		E.PayrollPaid = 15;
	TestEqual(TEXT("Paid staffing above the floor still scales power"), Services().PowerCapacity, 150);
	TestEqual(TEXT("Paid staffing above the floor still scales water"), Services().ServiceCapacity, 150);
	Population.Citizens.Reset();
	T.PowerCapacityPerBuilding = 20;
	T.WaterCapacityPerTreatment = 10;
	TestEqual(TEXT("Small authored power capacity respects its own scale"), Services().PowerCapacity, 10);
	TestEqual(TEXT("Small authored water capacity respects its own scale"), Services().ServiceCapacity, 5);
	T = FCityEconomyTuning();
	FCityFireSnapshot Fire;
	Fire.DamagedBuildings.Add(Power, {1, -1});
	const auto Damaged =
	    FCityServiceEvaluator::BuildSnapshot(City, T, nullptr, nullptr, nullptr, &Population, &Fire);
	TestEqual(TEXT("Damaged power cannot generate baseline supply"), Damaged.PowerCapacity, 0);
	TestEqual(TEXT("Independent water retains baseline during power damage"), Damaged.ServiceCapacity, 100);
	TestTrue(TEXT("Road outage fixture removes provider frontage"), City.RemoveRoad({1, 8}));
	TestEqual(TEXT("Disconnected power cannot generate baseline supply"), Services().PowerCapacity, 0);
	TestEqual(TEXT("Disconnected water cannot generate baseline supply"), Services().ServiceCapacity, 0);
	CivicRoad(City);
	CivicPlace(City, TEXT("Power"), {5, 9});
	CivicPlace(City, TEXT("Service"), {7, 9});
	TestEqual(TEXT("Baseline is per power building after road restoration"), Services().PowerCapacity, 200);
	TestEqual(TEXT("Baseline is per water building after road restoration"), Services().ServiceCapacity, 200);
	TestEqual(TEXT("Legacy snapshot without population keeps nominal capacity"),
	          FCityServiceEvaluator::BuildSnapshot(City, T).PowerCapacity, 400);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityEmergencyTravelTest, "CitySim.Safety.EmergencyRoadArrivalDamageResume",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityEmergencyTravelTest::RunTest(const FString& Parameters)
{
	int32 NearDamage = 0, FarDamage = 0;
	int64 NearArrival = 0, FarArrival = 0;
	for (int32 Case = 0; Case < 2; ++Case)
	{
		FCityPlacementModel City(100000);
		CivicRoad(City);
		const uint64 Home = CivicPlace(City, TEXT("Home"), {Case ? 60 : 6, 9}),
		             Station = CivicPlace(City, TEXT("Fire"), {2, 9});
		FCityRoadNetworkSnapshot Network;
		FCityRoadNetworkBuilder::Build(City, FCityRoadDefinition::Local(), Network);
		FCityServiceSnapshot Services;
		Services.bUsesPublicStaffing = true;
		Services.PublicJobs.Add(Station, 4);
		Services.PublicStaffPercent.Add(Station, 100);
		Services.EntityResidents.Add(Home, 10);
		FCityDevelopmentSnapshot D;
		FCityEnvironmentSnapshot W;
		FCityEconomyTuning T;
		T.FireBaseRisk = 0;
		T.FireSpreadChance = 0;
		FCityFireSimulation Fire;
		FString Error;
		Fire.RestoreState(CivicBurning(Home), 1, Error);
		FCityCrimeSnapshot Crime;
		FCityPopulationSimulation Population;
		FCityTrafficSimulation Traffic;
		Traffic.AdvanceOneStep(Network, 1);
		int64 Arrival = 0;
		for (int64 Step = 2; Step <= 20; ++Step)
		{
			Population.PruneTransitTrips(City);
			Population.UpdateEmergencyResponses(City, nullptr, Network, Fire.GetSnapshot(), Crime, Services,
			                                    Step, false);
			if (!Case && Step == 3)
			{
				City.RemoveRoad({4, 8});
				FCityRoadNetworkBuilder::Build(City, FCityRoadDefinition::Local(), Network);
			}
			if (!Case && Step == 5)
			{
				CivicRoad(City);
				FCityRoadNetworkBuilder::Build(City, FCityRoadDefinition::Local(), Network);
			}
			Traffic.UpdateTrips(City, nullptr, Population.GetSnapshot(), Network, Step);
			Traffic.AdvanceOneStep(Network, Step);
			for (const auto& V : Traffic.GetSnapshot().Vehicles)
				if (V.bCompleted)
					Population.CompleteTransitTrip(V.SourceId, Step);
			for (const auto& Trip : Population.GetSnapshot().TransitTrips)
				if (Trip.Purpose == ECityTripPurpose::FireResponse && Trip.bCompleted && !Arrival)
					Arrival = Step;
			Fire.AdvanceToStep(Step, City, Services, D, W, T, false, &Population.GetSnapshot());
			if (!Case && Step == 4)
			{
				TestEqual(TEXT("Removed road cannot deliver crew"), Arrival, int64(0));
				TestFalse(TEXT("No phantom response during road outage"),
				          Fire.GetSnapshot().ResponseStations.Contains(Home));
			}
			if (Step == 2)
			{
				TestFalse(TEXT("Dispatch does not instantly extinguish"),
				          Fire.GetSnapshot().ResponseStations.Contains(Home));
				FCityPopulationSimulation P;
				FCityTrafficSimulation V;
				TestTrue(TEXT("Saved emergency trip validates"),
				         P.RestoreState(Population.GetSnapshot(), Error));
				TestTrue(TEXT("Saved moving engine retains route and distance"),
				         V.RestoreState(Traffic.GetSnapshot(), Network, Step, Error));
				FCityFireSimulation F;
				TestTrue(TEXT("Saved burning state validates"),
				         F.RestoreState(Fire.GetSnapshot(), Step, Error));
				Population = MoveTemp(P);
				Traffic = MoveTemp(V);
				Fire = MoveTemp(F);
			}
		}
		AddInfo(FString::Printf(TEXT("Case=%d Arrival=%lld Damage=%d"), Case, Arrival,
		                        Fire.GetSnapshot().DamagedBuildings.FindRef(Home).Damage));
		if (Case)
		{
			FarDamage = Fire.GetSnapshot().DamagedBuildings.FindRef(Home).Damage;
			FarArrival = Arrival;
		}
		else
		{
			NearDamage = Fire.GetSnapshot().DamagedBuildings.FindRef(Home).Damage;
			NearArrival = Arrival;
		}
		TestTrue(TEXT("Only one crew/trip per station including return"),
		         Population.GetSnapshot().TransitTrips.Num() <= 1);
	}
	TestTrue(TEXT("Long road route delays actual arrival"), FarArrival > NearArrival && NearArrival > 0);
	TestTrue(TEXT("Longer response causes more damage"), FarDamage > NearDamage);
	TestEqual(TEXT("Nearby actual road response recovers for free"), NearDamage, 0);
	TestTrue(TEXT("Long response leaves structural damage needing paid repair"),
	         FarDamage > FCityEconomyTuning().FireMinorDamageAutoRepairLimit && FarDamage < 100);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityFireLocalRiskTest, "CitySim.Safety.FireCrimeIndustryFactors",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityFireLocalRiskTest::RunTest(const FString& Parameters)
{
	FCityPlacementModel City(100000);
	const uint64 Home = CivicPlace(City, TEXT("Home"), {10, 10});
	FCityFireSimulation Fire;
	FString Error;
	Fire.RestoreState(CivicBurning(Home), 1, Error);
	FCityEconomyTuning T;
	T.FireIndustryRadiusCells = 1;
	FCityServiceSnapshot S;
	FCityDevelopmentSnapshot D;
	FCityEnvironmentSnapshot W;
	Fire.Refresh(City, S, D, W, T, false);
	const int32 Base = Fire.GetSnapshot().EntityRisk.FindRef(Home);
	S.CrimeRisk.Init(100, FCityGrid::CellCount);
	CivicPlace(City, TEXT("Factory"), {11, 10});
	Fire.Refresh(City, S, D, W, T, false);
	TestEqual(TEXT("Crime contribution exact"), Fire.GetSnapshot().CrimeRiskContribution.FindRef(Home), 20);
	TestTrue(TEXT("Industrial density adds local ignition risk"),
	         Fire.GetSnapshot().IndustryRiskContribution.FindRef(Home) > 0);
	TestTrue(TEXT("Combined pressure increases risk"), Fire.GetSnapshot().EntityRisk.FindRef(Home) > Base);
	W.bPrecipitation = true;
	Fire.Refresh(City, S, D, W, T, false);
	TestEqual(TEXT("Rain halves combined unprotected factors"), Fire.GetSnapshot().EntityRisk.FindRef(Home),
	          (Base + 22) / 2);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityPoliceArrivalTest, "CitySim.Safety.PoliceArrivalEndsIncident",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityPoliceArrivalTest::RunTest(const FString& Parameters)
{
	FCityPlacementModel City(100000);
	CivicRoad(City);
	const uint64 Home = CivicPlace(City, TEXT("Home"), {6, 9}),
	             Police = CivicPlace(City, TEXT("Police"), {2, 9});
	FCityRoadNetworkSnapshot Network;
	FCityRoadNetworkBuilder::Build(City, FCityRoadDefinition::Local(), Network);
	FCityServiceSnapshot S;
	S.bUsesPublicStaffing = true;
	S.PublicJobs.Add(Police, 4);
	S.PublicStaffPercent.Add(Police, 100);
	FCityCrimeSnapshot State;
	State.ActivationStep = 0;
	State.GraceEndStep = 0;
	State.TotalIncidents = 1;
	State.RecentIncidents.Add({1, Home, 0, 5});
	FCityCrimeSimulation Crime;
	FString Error;
	if (!TestTrue(TEXT("Crime incident fixture valid"), Crime.RestoreState(State, 0, Error)))
		return false;
	FCityPopulationSimulation Population;
	FCityTrafficSimulation Traffic;
	FCityFireSnapshot Fire;
	FCityDevelopmentSnapshot D;
	FCityEconomyTuning T;
	T.CrimeBaseRisk = 0;
	T.CrimeJobShortageWeight = 0;
	for (int64 Step = 1; Step <= 4; ++Step)
	{
		Population.PruneTransitTrips(City);
		Population.UpdateEmergencyResponses(City, nullptr, Network, Fire, Crime.GetSnapshot(), S, Step,
		                                    false);
		Traffic.UpdateTrips(City, nullptr, Population.GetSnapshot(), Network, Step);
		Traffic.AdvanceOneStep(Network, Step);
		for (const auto& V : Traffic.GetSnapshot().Vehicles)
			if (V.bCompleted)
				Population.CompleteTransitTrip(V.SourceId, Step);
		Crime.AdvanceToStep(Step, City, S, D, T, false, &Population.GetSnapshot());
		if (Step == 1)
			TestEqual(TEXT("Dispatch alone leaves incident deadline intact"),
			          Crime.GetSnapshot().RecentIncidents[0].EndStep, int64(5));
	}
	TestEqual(TEXT("Arrived police finish incident earlier"), Crime.GetSnapshot().RecentIncidents[0].EndStep,
	          int64(4));
	FCityCrimeSimulation Restored;
	TestTrue(TEXT("Shortened police incident saves/restores"),
	         Restored.RestoreState(Crime.GetSnapshot(), 4, Error));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCitySafetyArtTest, "CitySim.Art.DamagedVariantsParticles",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCitySafetyArtTest::RunTest(const FString& Parameters)
{
	const TCHAR* Names[] = {TEXT("Residential_A"),
	                        TEXT("Residential_Medium_A"),
	                        TEXT("Residential_High_A"),
	                        TEXT("Residential_Construction_A"),
	                        TEXT("Shop_A"),
	                        TEXT("Shop_Medium_A"),
	                        TEXT("Shop_High_A"),
	                        TEXT("Factory_A"),
	                        TEXT("Factory_Medium_A"),
	                        TEXT("Factory_High_A"),
	                        TEXT("Park_A"),
	                        TEXT("Power_A"),
	                        TEXT("Service_A"),
	                        TEXT("Police_A"),
	                        TEXT("Fire_A")};
	const TMap<FString, int32> SurfaceSections = {
	    {TEXT("Residential_A"), 3}, {TEXT("Residential_Medium_A"), 3}, {TEXT("Residential_High_A"), 4},
	    {TEXT("Shop_A"), 4},        {TEXT("Shop_Medium_A"), 4},        {TEXT("Shop_High_A"), 4},
	    {TEXT("Factory_A"), 4},     {TEXT("Factory_Medium_A"), 4},     {TEXT("Factory_High_A"), 4}};
	for (const auto* Name : Names)
	{
		const FString Path = FString(TEXT("/Game/CitySim/Art/Meshes/SM_Building_")) + Name + TEXT("_Damaged");
		auto* Mesh = LoadObject<UStaticMesh>(nullptr, *Path);
		if (!TestNotNull(Path, Mesh))
			continue;
		if (const int32* Sections = SurfaceSections.Find(Name))
			TestEqual(TEXT("Damaged BuildingSurfaces form retains its specific surface sections"),
			          Mesh->GetStaticMaterials().Num(), *Sections);
		else
			TestTrue(TEXT("Other damage retains the original two-material bound"),
			         Mesh->GetStaticMaterials().Num() <= 2);
		for (const auto& M : Mesh->GetStaticMaterials())
			TestTrue(TEXT("Damaged windows do not retain live glow material"),
			         M.MaterialInterface && M.MaterialInterface->GetName().Contains(TEXT("Charred")));
	}
	for (const TCHAR* Name : {TEXT("PS_CityFire"), TEXT("PS_CityWaterSpray")})
	{
		const FString Path = FString(TEXT("/Game/CitySim/Art/Effects/")) + Name;
		auto* System = LoadObject<UParticleSystem>(nullptr, *Path);
		if (!TestNotNull(Path, System))
			continue;
		TestEqual(TEXT("Bounded emitter count"), System->Emitters.Num(),
		          FString(Name) == TEXT("PS_CityFire") ? 2 : 1);
		for (const auto& E : System->Emitters)
		{
			TestTrue(TEXT("CPU sprite allocation bounded"), E && E->InitialAllocationCount <= 96);
			if (E)
				TestTrue(TEXT("Particle material present"),
				         E->LODLevels.Num() == 1 && E->LODLevels[0]->RequiredModule);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityCriticalUtilitiesTest, "CitySim.Safety.CriticalUtilityPriority",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityCriticalUtilitiesTest::RunTest(const FString& Parameters)
{
	FCityPlacementModel City(100000);
	CivicRoad(City);
	CivicPlace(City, TEXT("Power"), {1, 9});
	CivicPlace(City, TEXT("Service"), {2, 9});
	const uint64 Home = CivicPlace(City, TEXT("Home"), {4, 9}), Shop = CivicPlace(City, TEXT("Shop"), {5, 9}),
	             Factory = CivicPlace(City, TEXT("Factory"), {6, 9});
	const uint64 Police = CivicPlace(City, TEXT("Police"), {8, 9}),
	             Fire = CivicPlace(City, TEXT("Fire"), {12, 9});
	auto* Catalog = NewObject<UCityBuildingCatalog>();
	Catalog->Definitions = {CivicDef(TEXT("Police")), CivicDef(TEXT("Fire"))};
	FCityEconomyTuning T;
	T.WaterRangeRoadHops = 64;
	for (int32 Mode = 0; Mode < 2; ++Mode)
	{
		T.PowerCapacityPerBuilding = Mode ? 200 : 20;
		T.WaterCapacityPerTreatment = Mode ? 10 : 200;
		const auto S = FCityServiceEvaluator::BuildSnapshot(City, T, Catalog);
		TestEqual(TEXT("Later police receives emergency utilities first"), S.GetFailure(Police),
		          ECityOperationFailure::None);
		TestEqual(TEXT("Later fire receives emergency utilities first"), S.GetFailure(Fire),
		          ECityOperationFailure::None);
		for (const uint64 Id : {Home, Shop, Factory})
			TestEqual(TEXT("Ordinary lot sheds before emergency services"), S.GetFailure(Id),
			          Mode ? ECityOperationFailure::ServiceCapacity : ECityOperationFailure::PowerCapacity);
		TestTrue(TEXT("Allocated utilities never exceed capacity"),
		         S.PowerUsed <= S.PowerCapacity && S.ServiceUsed <= S.ServiceCapacity);
	}
	T.PowerCapacityPerBuilding = T.WaterCapacityPerTreatment = 200;
	const auto Restored = FCityServiceEvaluator::BuildSnapshot(City, T, Catalog);
	for (const uint64 Id : {Home, Shop, Factory, Police, Fire})
		TestEqual(TEXT("Capacity recovery restores everyone"), Restored.GetFailure(Id),
		          ECityOperationFailure::None);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCitySafetyControlsTest, "CitySim.Safety.TestControlsFireproofRecovery",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCitySafetyControlsTest::RunTest(const FString& Parameters)
{
	const auto Values = UWorld::InitializationValues()
	                        .AllowAudioPlayback(false)
	                        .CreatePhysicsScene(false)
	                        .CreateNavigation(false)
	                        .CreateAISystem(false)
	                        .ShouldSimulatePhysics(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("SafetyControlsTest"), nullptr, true,
	                                    ERHIFeatureLevel::Num, &Values);
	auto* Sim = World->GetSubsystem<UCitySimulationSubsystem>();
	auto* Catalog = NewObject<UCityBuildingCatalog>();
	Catalog->Definitions = {CivicDef(TEXT("Power")), CivicDef(TEXT("Service")), CivicDef(TEXT("Home")),
	                        CivicDef(TEXT("Police")), CivicDef(TEXT("Fire"))};
	Sim->RegisterBuildingCatalog(Catalog);
	auto& City = Sim->GetPlacementModel();
	City.SetTerrain(nullptr);
	CivicRoad(City);
	CivicPlace(City, TEXT("Power"), {1, 9});
	CivicPlace(City, TEXT("Service"), {2, 9});
	const uint64 Home = CivicPlace(City, TEXT("Home"), {6, 9}),
	             Police = CivicPlace(City, TEXT("Police"), {4, 9}),
	             Station = CivicPlace(City, TEXT("Fire"), {8, 9});
	Sim->EconomyTuning.NewArrivalUneducatedPercent = 0; // Fully qualified fire response fixture.
	Sim->Development.InitializeExistingAsOperational(City, Catalog, 0);
	Sim->EconomyTuning.FixedStepSeconds = .125;
	Sim->EconomyTuning.FixedStepsPerMonth = 4;
	Sim->EconomyTuning.FirePopulationThreshold = Sim->EconomyTuning.CrimePopulationThreshold = 100000;
	Sim->EconomyTuning.FireBaseRisk = 0;
	Sim->EconomyTuning.FireSpreadChance = 0;
	Sim->EconomyTuning.CrimeBaseRisk = 0;
	Sim->StartScenario();
	Sim->Tick(.125f);
	Sim->SetSimulationPaused(true);
	TestFalse(TEXT("Fireproof station rejects test ignition"), Sim->StartSafetyTest(Station, true, false));
	TestTrue(TEXT("Arson test works below natural thresholds"), Sim->StartSafetyTest(Home, true, true));
	TestFalse(TEXT("Manual tests preserve natural population gates"),
	          Sim->GetFireSnapshot().IsIntroduced() || Sim->GetCrimeSnapshot().IsIntroduced());
	TestTrue(TEXT("Manual fire marked for save"), Sim->GetFireSnapshot().RecentIncidents.Last().bTest);
	const int64 BeforeFire = Sim->GetFireSnapshot().TotalIncidents,
	            BeforeCrime = Sim->GetCrimeSnapshot().TotalIncidents;
	TestFalse(TEXT("Duplicate arson is rejected atomically"), Sim->StartSafetyTest(Home, true, true));
	TestEqual(TEXT("Rejected arson creates no extra fire"), Sim->GetFireSnapshot().TotalIncidents,
	          BeforeFire);
	TestEqual(TEXT("Rejected arson creates no extra crime"), Sim->GetCrimeSnapshot().TotalIncidents,
	          BeforeCrime);
	FString Error;
	const FString Path = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/SafetyTests.json"));
	TestTrue(TEXT("Pre-threshold test incidents save"), Sim->SaveCityToPath(Path, Error));
	TestTrue(TEXT("Pre-threshold incidents load transactionally"), Sim->LoadCityFromPath(Path, Error, false));
	const int64 Clock = Sim->GetFireSnapshot().LastFixedStepId;
	Sim->Tick(1.f);
	TestEqual(TEXT("Pause holds damage clock and dispatch"), Sim->GetFireSnapshot().LastFixedStepId, Clock);
	Sim->SetSimulationPaused(false);
	bool Arrived = false, Returned = false;
	for (int32 I = 0; I < 16; ++I)
	{
		Sim->Tick(.125f);
		Arrived |= Sim->GetFireSnapshot().ResponseStations.Contains(Home);
		Returned |= Sim->GetPopulationSnapshot().TransitTrips.ContainsByPredicate(
		    [](const auto& Trip) { return Trip.Purpose == ECityTripPurpose::EmergencyReturn; });
	}
	if (!Arrived)
	{
		for (const auto& V : Sim->GetTrafficSnapshot().Vehicles)
			AddInfo(FString::Printf(
			    TEXT("Safety response diagnostic id=%llu type=%d response=%d yield=%d completed=%d "
			         "distance=%d end=%d origin=%llu target=%llu edge=%d reverse=%d"),
			    V.Id, int32(V.VehicleType), V.bEmergencyResponding, V.bYielding, V.bCompleted,
			    V.DistanceUnits, V.EndDistanceUnits, V.OriginEntityId, V.DestinationEntityId,
			    V.Route[V.RouteLegIndex].EdgeId, V.Route[V.RouteLegIndex].bReverse));
		for (const auto& Trip : Sim->GetPopulationSnapshot().TransitTrips)
			AddInfo(FString::Printf(
			    TEXT("Safety trip diagnostic id=%llu purpose=%d completed=%d origin=%llu target=%llu"),
			    Trip.Id, int32(Trip.Purpose), Trip.bCompleted, Trip.OriginEntityId,
			    Trip.DestinationEntityId));
	}
	TestTrue(TEXT("Test fire uses actual engine arrival"), Arrived);
	if (const auto* Damage = Sim->GetFireSnapshot().DamagedBuildings.Find(Home))
		TestTrue(TEXT("Delayed arson response leaves only damage above the recovery limit"),
		         Damage->Damage > Sim->EconomyTuning.FireMinorDamageAutoRepairLimit);
	else
		TestEqual(TEXT("Automatically recovered home is operational in the live city"),
		          Sim->GetServiceSnapshot().GetFailure(Home), ECityOperationFailure::None);
	TestTrue(TEXT("Crew returns after response"), Returned);
	TestTrue(TEXT("All crews return and become available"),
	         !Sim->GetPopulationSnapshot().TransitTrips.ContainsByPredicate(
	             [](const auto& T) { return T.IsEmergency(); }));
	TestTrue(TEXT("Resolved police test saves after shortened deadline"), Sim->SaveCityToPath(Path, Error));
	TestTrue(TEXT("Resolved police test reloads"), Sim->LoadCityFromPath(Path, Error, false));
	// Simulate the old version's fire-station incident and migration, preserving history.
	auto Legacy = CivicBurning(Station);
	Legacy.LastFixedStepId = Sim->GetEconomySnapshot().FixedStepId;
	Legacy.RecentIncidents[0].StartStep = Legacy.LastFixedStepId;
	Legacy.DamagedBuildings[Station].Damage = 100;
	TestTrue(TEXT("Legacy station damage fixture valid"),
	         Sim->Fire.RestoreState(Legacy, Legacy.LastFixedStepId, Error));
	TestTrue(TEXT("Legacy burnt station city saves"), Sim->SaveCityToPath(Path, Error));
	TestTrue(TEXT("Legacy burnt station city loads"), Sim->LoadCityFromPath(Path, Error, false));
	TestFalse(TEXT("Migration clears fireproof station damage"), Sim->GetFireSnapshot().IsDisabled(Station));
	Sim->Tick(.125f);
	TestEqual(TEXT("Restored station gets staff and utilities"),
	          Sim->GetServiceSnapshot().GetFailure(Station), ECityOperationFailure::None);
	auto& ReloadedCity = Sim->GetPlacementModel();
	ReloadedCity.SetTerrain(nullptr);
	const uint64 Other = CivicPlace(ReloadedCity, TEXT("Home"), {10, 9}),
	             Neighbor = CivicPlace(ReloadedCity, TEXT("Home"), {13, 9});
	Sim->Development.InitializeExistingAsOperational(ReloadedCity, Catalog,
	                                                 Sim->GetEconomySnapshot().FixedStepId);
	TestTrue(TEXT("New fire after station recovery"), Sim->StartSafetyTest(Other, true, false));
	TestTrue(TEXT("Nearby second fire starts"), Sim->StartSafetyTest(Neighbor, true, false));
	bool Dispatched = false, ChainLoaded = false;
	for (int32 I = 0; I < 24; ++I)
	{
		Sim->Tick(.125f);
		Dispatched |= Sim->GetFireSnapshot().ResponseStations.Contains(Other);
		if (!ChainLoaded && Sim->GetPopulationSnapshot().TransitTrips.ContainsByPredicate([&](const auto& T) {
			    return T.Purpose == ECityTripPurpose::FireResponse && T.OriginEntityId == Other &&
			           T.DestinationEntityId == Neighbor;
		    }))
		{
			TestTrue(TEXT("Full city saves scene-origin journey"), Sim->SaveCityToPath(Path, Error));
			ChainLoaded = Sim->LoadCityFromPath(Path, Error, false);
			TestTrue(TEXT("Full city validates/resumes scene-origin journey"), ChainLoaded);
		}
	}
	TestTrue(TEXT("Full city reaches chained response save/load"), ChainLoaded);
	TestTrue(TEXT("Recovered station responds again"), Dispatched);
	const auto TestMenu = FCityPaletteModel::Entries(ECityPalettePage::SafetyTests);
	TestEqual(TEXT("Debug drawer has direct home/shop placement, nine event tools and back"), TestMenu.Num(),
	          12);
	TestEqual(TEXT("Tests belong under game settings"),
	          FCityPaletteModel::ParentPage(ECityPalettePage::SafetyTests), ECityPalettePage::Settings);
	TestTrue(TEXT("Police fixture present"), Sim->GetPlacementModel().GetEntities().Contains(Police));
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityEmergencyChainTest, "CitySim.Safety.NearbyFireResponseChain",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityEmergencyChainTest::RunTest(const FString& Parameters)
{
	FCityPlacementModel City(100000);
	CivicRoad(City);
	const uint64 Station = CivicPlace(City, TEXT("Fire"), {2, 9}),
	             First = CivicPlace(City, TEXT("Home"), {6, 9}),
	             Near = CivicPlace(City, TEXT("Home"), {9, 9}), Far = CivicPlace(City, TEXT("Home"), {50, 9});
	FCityRoadNetworkSnapshot Network;
	FCityRoadNetworkBuilder::Build(City, FCityRoadDefinition::Local(), Network);
	FCityServiceSnapshot S;
	S.bUsesPublicStaffing = true;
	S.PublicJobs.Add(Station, 4);
	S.PublicStaffPercent.Add(Station, 100);
	FCityDevelopmentSnapshot D;
	FCityEnvironmentSnapshot W;
	FCityEconomyTuning T;
	T.FirePopulationThreshold = 100000;
	T.FireSpreadChance = 0;
	FCityFireSimulation Fire;
	FCityPopulationSimulation Population;
	FCityTrafficSimulation Traffic;
	FCityCrimeSnapshot Crime;
	FString Error;
	TestTrue(TEXT("First fire starts"), Fire.StartTest(First, Error));
	TestTrue(TEXT("Nearby fire starts"), Fire.StartTest(Near, Error));
	TestTrue(TEXT("Distant fire starts"), Fire.StartTest(Far, Error));
	int64 OnwardStep = 0, NearArrival = 0;
	uint64 Crew = 0;
	bool Restored = false, Returned = false, FarRedispatched = false;
	for (int64 Step = 1; Step <= 24; ++Step)
	{
		Population.PruneTransitTrips(City);
		Population.UpdateEmergencyResponses(City, nullptr, Network, Fire.GetSnapshot(), Crime, S, Step,
		                                    false);
		for (const auto& Trip : Population.GetSnapshot().TransitTrips)
		{
			if (!Crew)
				Crew = Trip.Id;
			if (Trip.DestinationEntityId != Far)
				TestEqual(TEXT("Same crew persists through chain and return"), Trip.Id, Crew);
			if (Trip.Purpose == ECityTripPurpose::FireResponse && Trip.DestinationEntityId == Near &&
			    !OnwardStep)
			{
				OnwardStep = Step;
				TestEqual(TEXT("Next journey starts at previous fire"), Trip.OriginEntityId, First);
				TestFalse(TEXT("Chaining never invents arrival"), Trip.bCompleted);
			}
			if (Trip.Purpose == ECityTripPurpose::FireResponse && Trip.DestinationEntityId == Far)
			{
				TestTrue(TEXT("Distant fire waits for local crew return before a fresh dispatch"),
				         Returned && Trip.Id != Crew);
				TestEqual(TEXT("Distant response starts at station, not previous fire"), Trip.OriginEntityId,
				          Station);
				FarRedispatched = true;
			}
			if (Trip.Purpose == ECityTripPurpose::EmergencyReturn)
			{
				Returned = true;
				TestEqual(TEXT("Return goes to home station, not previous fire"), Trip.DestinationEntityId,
				          Station);
			}
		}
		Traffic.UpdateTrips(City, nullptr, Population.GetSnapshot(), Network, Step);
		Traffic.AdvanceOneStep(Network, Step);
		for (const auto& V : Traffic.GetSnapshot().Vehicles)
			if (V.bCompleted)
				Population.CompleteTransitTrip(V.SourceId, Step);
		Fire.AdvanceToStep(Step, City, S, D, W, T, false, &Population.GetSnapshot());
		if (Fire.GetSnapshot().ResponseStations.Contains(Near) && !NearArrival)
			NearArrival = Step;
		if (OnwardStep == Step)
		{
			FCitySaveDocument Doc;
			Doc.Placement = City.ExportState();
			Doc.Economy.FixedStepId = Step;
			Doc.Population = Population.GetSnapshot();
			Doc.Fire = Fire.GetSnapshot();
			Doc.Traffic = Traffic.GetSnapshot();
			FString Json;
			FCitySaveDocument Read;
			TestTrue(TEXT("Scene-origin trip serializes"), FCityPersistence::Serialize(Doc, Json, Error));
			const bool bRead = FCityPersistence::Deserialize(Json, Read, Error);
			if (TestTrue(*FString::Printf(TEXT("Scene-origin trip reads (%s)"), *Error), bRead))
			{
				TestTrue(TEXT("Save retains the active nearby incident"), Read.Fire.IsBurning(Near));
				TestTrue(TEXT("Chained population restores"),
				         Population.RestoreState(Read.Population, Error));
				TestTrue(TEXT("Chained traffic restores at real road progress"),
				         Traffic.RestoreState(Read.Traffic, Network, Step, Error));
				TestTrue(TEXT("Chained fire restores"), Fire.RestoreState(Read.Fire, Step, Error));
				Restored = true;
			}
		}
	}
	TestTrue(TEXT("Crew moves directly to nearby fire"), OnwardStep > 0 && NearArrival > OnwardStep);
	TestTrue(TEXT("Onward travel resumes after save"), Restored);
	TestTrue(TEXT("Both local fires extinguished before burnout"),
	         Fire.GetSnapshot().DamagedBuildings.FindRef(First).Damage < 100 &&
	             Fire.GetSnapshot().DamagedBuildings.FindRef(Near).Damage < 100);
	TestTrue(TEXT("Crew returns then responds to surviving distant fire"), Returned && FarRedispatched);
	// A competing engine already owns the nearest incident; take a different nearby target.
	const uint64 OtherStation = CivicPlace(City, TEXT("Fire"), {20, 9}),
	             Third = CivicPlace(City, TEXT("Home"), {10, 9});
	FCityPopulationSnapshot P;
	P.NextTransitTripId = 3;
	FCityTransitTripRecord A;
	A.Id = 1;
	A.OriginEntityId = A.StationEntityId = Station;
	A.DestinationEntityId = First;
	A.IncidentId = 1;
	A.Purpose = ECityTripPurpose::FireResponse;
	A.VehicleType = ECityTripVehicleType::FireEngine;
	A.bCompleted = true;
	P.TransitTrips.Add(A);
	FCityTransitTripRecord B = A;
	B.Id = 2;
	B.OriginEntityId = B.StationEntityId = OtherStation;
	B.DestinationEntityId = Near;
	B.IncidentId = 2;
	B.bCompleted = false;
	P.TransitTrips.Add(B);
	FCityFireSnapshot F;
	F.RecentIncidents = {{1, First, 0, 0, 1, 3}, {2, Near, 0, 0, -1, 0}, {3, Third, 0, 0, -1, 0}};
	TestTrue(TEXT("Competing crews fixture restores"), Population.RestoreState(P, Error));
	Population.UpdateEmergencyResponses(City, nullptr, Network, F, Crime, S, 2, false);
	TestEqual(TEXT("Never retarget a fire claimed by another crew"),
	          Population.GetSnapshot().TransitTrips[0].DestinationEntityId, Third);
	Population.RestoreState(P, Error);
	S.EntityFailures.Add(Station, ECityOperationFailure::NoPower);
	Population.UpdateEmergencyResponses(City, nullptr, Network, F, Crime, S, 2, false);
	TestEqual(TEXT("Station outage sends crew home instead of chaining"),
	          Population.GetSnapshot().TransitTrips[0].Purpose, ECityTripPurpose::EmergencyReturn);
	Population.RestoreState(P, Error);
	S.EntityFailures.Reset();
	Population.UpdateEmergencyResponses(City, nullptr, Network, F, Crime, S, 2, true);
	TestEqual(TEXT("Operating-bill shortfall skips onward dispatch"),
	          Population.GetSnapshot().TransitTrips[0].Purpose, ECityTripPurpose::EmergencyReturn);
	Population.RestoreState(P, Error);
	City.RemoveRoad({8, 8});
	FCityRoadNetworkBuilder::Build(City, FCityRoadDefinition::Local(), Network);
	Population.UpdateEmergencyResponses(City, nullptr, Network, F, Crime, S, 2, false);
	TestEqual(TEXT("Disconnected nearby fire skips onward dispatch"),
	          Population.GetSnapshot().TransitTrips[0].Purpose, ECityTripPurpose::EmergencyReturn);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityEmergencySprayTest,
                                 "CitySim.Presentation.SprayWaitsForVisibleEngineArrival",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityEmergencySprayTest::RunTest(const FString& Parameters)
{
	const auto Values = UWorld::InitializationValues()
	                        .AllowAudioPlayback(false)
	                        .CreatePhysicsScene(false)
	                        .CreateNavigation(false)
	                        .CreateAISystem(false)
	                        .ShouldSimulatePhysics(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("EmergencySprayTest"), nullptr, true,
	                                    ERHIFeatureLevel::Num, &Values);
	auto* Sim = World->GetSubsystem<UCitySimulationSubsystem>();
	auto* Board = World->SpawnActor<ACityBoardActor>();
	auto& City = Sim->GetPlacementModel();
	City.SetTerrain(nullptr);
	CivicRoad(City);
	const uint64 Station = CivicPlace(City, TEXT("Fire"), {2, 9}),
	             Home = CivicPlace(City, TEXT("Home"), {6, 9});
	FCityRoadNetworkBuilder::Build(City, FCityRoadDefinition::Local(), Sim->RoadNetworkSnapshot);
	FCityTrafficVehicleState V;
	V.Id = 3;
	V.SourceId = 1;
	V.OriginEntityId = Station;
	V.DestinationEntityId = Home;
	V.bTransitTrip = true;
	V.VehicleType = ECityTripVehicleType::FireEngine;
	TestTrue(TEXT("Engine route builds"),
	         FCityTrafficSimulation::BuildRoute(Sim->RoadNetworkSnapshot, {2, 8}, {6, 8}, V));
	TestTrue(TEXT("Engine enters traffic"), Sim->Traffic.AddVehicle(V, Sim->RoadNetworkSnapshot));
	Sim->Traffic.AdvanceOneStep(Sim->RoadNetworkSnapshot, 1);
	Sim->Traffic.AdvanceOneStep(Sim->RoadNetworkSnapshot, 2);
	Sim->PreviousTraffic = Sim->Traffic.GetSnapshot();
	Sim->Traffic.AdvanceOneStep(Sim->RoadNetworkSnapshot, 3);
	const auto& Arrived = Sim->Traffic.GetSnapshot().Vehicles[0];
	const auto& Before = Sim->PreviousTraffic.Vehicles[0];
	if (!TestTrue(TEXT("Authority arrives while previous visual pose approaches"),
	              Arrived.bCompleted && !Before.bCompleted))
	{
		World->DestroyWorld(false);
		return false;
	}
	FVector Pos, Direction;
	for (double Alpha : {0., .25, .5, .99})
		TestFalse(TEXT("Final interpolated segment does not enable spray"),
		          CityEmergencyPresentation::SampleArrivedVehicle(Arrived, &Before, Sim->RoadNetworkSnapshot,
		                                                          Alpha, Pos, Direction));
	TestTrue(TEXT("Visible endpoint enables spray"),
	         CityEmergencyPresentation::SampleArrivedVehicle(Arrived, &Before, Sim->RoadNetworkSnapshot, 1.,
	                                                         Pos, Direction));
	TestTrue(TEXT("Saved/held endpoint enables spray at next frame"),
	         CityEmergencyPresentation::SampleArrivedVehicle(Arrived, &Arrived, Sim->RoadNetworkSnapshot, 0.,
	                                                         Pos, Direction));
	FCityPopulationSnapshot P;
	P.NextTransitTripId = 2;
	FCityTransitTripRecord Trip;
	Trip.Id = 1;
	Trip.OriginEntityId = Trip.StationEntityId = Station;
	Trip.DestinationEntityId = Home;
	Trip.IncidentId = 1;
	Trip.Purpose = ECityTripPurpose::FireResponse;
	Trip.VehicleType = ECityTripVehicleType::FireEngine;
	Trip.bCompleted = true;
	Trip.StartStep = 1;
	Trip.ArrivalStep = 3;
	P.TransitTrips.Add(Trip);
	FString Error;
	Sim->Population.RestoreState(P, Error);
	Sim->Fire.RestoreState(CivicBurning(Home), 1, Error);
	FCityServiceSnapshot S;
	S.bUsesPublicStaffing = true;
	S.PublicStaffPercent.Add(Station, 100);
	Sim->Fire.Refresh(City, S, Sim->Development.GetSnapshot(), Sim->Environment.GetSnapshot(),
	                  Sim->EconomyTuning, false, &Sim->Population.GetSnapshot());
	Board->SimulationSubsystem = Sim;
	Board->PlacementModel = &City;
	Board->DisplayedDamagedIds.Add(Home);
	Board->UpdatePopulationTraffic();
	Board->UpdateFirePresentation();
	TestEqual(TEXT("Visible engine has a pooled instance"), Board->FireResponseInstances->GetInstanceCount(),
	          1);
	TestFalse(TEXT("Actual water pool waits behind interpolated truck"),
	          Board->WaterParticlePools[0]->IsActive());
	TestTrue(TEXT("Water origin remains unset before displayed arrival"),
	         Board->WaterParticlePools[0]->GetRelativeLocation().IsNearlyZero());
	Sim->PreviousTraffic = Sim->Traffic.GetSnapshot();
	Board->UpdatePopulationTraffic();
	Board->UpdateFirePresentation();
	TestFalse(TEXT("Actual water pool gets arrived engine origin"),
	          Board->WaterParticlePools[0]->GetRelativeLocation().IsNearlyZero());
	if (FApp::CanEverRender())
		TestTrue(TEXT("Actual water pool starts after visible arrival"),
		         Board->WaterParticlePools[0]->IsActive());
	// Null-RHI intentionally cannot activate sprites; verify the same production placement gate.
	Board->WaterParticlePools[0]->SetRelativeLocation(FVector::ZeroVector);
	Board->DiagnosticOverlay = ECityDiagnosticOverlay::Traffic;
	Board->SetHeatMapLayers(true);
	Board->UpdateFirePresentation();
	TestFalse(TEXT("Hidden traffic layer cannot leave an orphan water jet"),
	          Board->WaterParticlePools[0]->IsActive());
	TestTrue(TEXT("Hidden engine cannot position a water jet"),
	         Board->WaterParticlePools[0]->GetRelativeLocation().IsNearlyZero());
	Board->DiagnosticOverlay = ECityDiagnosticOverlay::Fire;
	Board->SetHeatMapLayers(true);
	Board->UpdateFirePresentation();
	TestTrue(TEXT("Fire view positions water at the visible engine"),
	         Board->FireResponseInstances->IsVisible() &&
	             !Board->WaterParticlePools[0]->GetRelativeLocation().IsNearlyZero());
	if (FApp::CanEverRender())
		TestTrue(TEXT("Fire view shows engine and water together"), Board->WaterParticlePools[0]->IsActive());
	Board->DiagnosticOverlay = ECityDiagnosticOverlay::Crime;
	Board->SetHeatMapLayers(true);
	TestTrue(TEXT("Crime view keeps emergency vehicles visible"),
	         Board->PoliceResponseInstances->IsVisible());
	Board->SetHeatMapLayers(false);
	TestTrue(TEXT("Clearing overlay restores response visibility"),
	         Board->FireResponseInstances->IsVisible());
	// A completed prompt response must return the actual building instance to
	// its original mesh, not just clear a simulation status or overlay marker.
	auto* Catalog = NewObject<UCityBuildingCatalog>();
	auto HomeDefinition = CivicDef(TEXT("Home"));
	HomeDefinition.ConstructionSteps = 1;
	HomeDefinition.PresentationMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(
	    TEXT("/Game/CitySim/Art/Meshes/SM_Building_Residential_A.SM_Building_Residential_A")));
	CivicPlace(City, TEXT("Power"), {12, 9});
	CivicPlace(City, TEXT("Service"), {16, 9});
	Catalog->Definitions = {HomeDefinition, CivicDef(TEXT("Fire")), CivicDef(TEXT("Power")),
	                        CivicDef(TEXT("Service"))};
	Board->BuildingCatalog = Catalog;
	Sim->Development.InitializeExistingAsOperational(City, Catalog, 1);
	Board->RebuildAllPresentation();
	const auto DamagedDefinition = Board->ResolvePresentationDefinition(Home, HomeDefinition);
	auto* DamagedPool = Board->BuildingInstances.FindRef(DamagedDefinition.DefinitionId).Get();
	if (TestNotNull(TEXT("Burning home uses a real damaged pool"), DamagedPool))
	{
		TestEqual(TEXT("Damaged pool has the burning home"), DamagedPool->GetInstanceCount(), 1);
		TestTrue(TEXT("Burning home selects imported damaged mesh"),
		         DamagedPool->GetStaticMesh()->GetName().EndsWith(TEXT("_Damaged")));
	}
	Sim->Fire.AdvanceToStep(4, City, S, Sim->Development.GetSnapshot(), Sim->Environment.GetSnapshot(),
	                        Sim->EconomyTuning, false, &Sim->Population.GetSnapshot());
	Board->UpdateFirePresentation();
	TestFalse(TEXT("Automatic recovery clears displayed damaged ID"),
	          Board->DisplayedDamagedIds.Contains(Home));
	if (DamagedPool)
		TestEqual(TEXT("Damaged pool releases restored home"), DamagedPool->GetInstanceCount(), 0);
	const auto RestoredDefinition = Board->ResolvePresentationDefinition(Home, HomeDefinition);
	auto* RestoredPool = Board->BuildingInstances.FindRef(RestoredDefinition.DefinitionId).Get();
	if (TestNotNull(TEXT("Recovered home has its normal instance pool"), RestoredPool))
	{
		TestEqual(TEXT("Normal pool contains restored home"), RestoredPool->GetInstanceCount(), 1);
		TestTrue(TEXT("Original imported mesh returns"),
		         RestoredPool->GetStaticMesh() == HomeDefinition.PresentationMesh.LoadSynchronous());
	}
	TestFalse(TEXT("Completed response clears visible water"), Board->WaterParticlePools[0]->IsActive());
	const auto RecoveredServices = FCityServiceEvaluator::BuildSnapshot(
	    City, Sim->EconomyTuning, Catalog, &Sim->Development.GetSnapshot(), &Sim->Environment.GetSnapshot(),
	    nullptr, &Sim->Fire.GetSnapshot());
	TestEqual(TEXT("Recovered building capacity is operational"), RecoveredServices.GetFailure(Home),
	          ECityOperationFailure::None);
	TestEqual(TEXT("Prompt recovery returns actual housing capacity"), RecoveredServices.HousingCapacity, 10);

	World->DestroyWorld(false);
	return true;
}
#endif
