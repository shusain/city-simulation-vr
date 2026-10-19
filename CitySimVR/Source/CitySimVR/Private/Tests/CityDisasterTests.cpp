#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Simulation/CityDisasterSimulation.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Commands/CityPlacementModel.h"
#include "Persistence/CityPersistence.h"
#include "Engine/World.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleRequired.h"
#include "Particles/Spawn/ParticleModuleSpawn.h"
#include "Presentation/CityBoardActor.h"
namespace
{
uint64 Place(FCityPlacementModel& C, const TCHAR* Name, const FCityCellCoord Cell)
{
	FCityBuildingDefinition D;
	D.DefinitionId = Name;
	D.DisplayName = Name;
	D.Footprint = {1, 1};
	D.Cost = 100;
	auto P = C.BeginPreview(D, Cell);
	return C.CommitPlacement(P, D).EntityId;
}
void Populate(FCityPlacementModel& C)
{
	auto Road = C.BeginRoadPreview(FCityPlacementModel::EntranceCell);
	C.UpdateRoadPreviewEnd(Road, {30, 8});
	C.CommitRoad(Road);
	for (int32 X = 4; X < 18; ++X)
		Place(C, TEXT("Home"), {X, 9});
	Place(C, TEXT("Fire"), {5, 10});
	Place(C, TEXT("Power"), {6, 10});
}
} // namespace
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityNaturalDisasterTest, "CitySim.Disasters.WarningDamageResumeAndPolicy",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityNaturalDisasterTest::RunTest(const FString& Parameters)
{
	FCityPlacementModel City(100000);
	Populate(City);
	if (!TestEqual(TEXT("Standalone test city has valid placed targets"), City.GetEntities().Num(), 16))
		return false;
	FCityDevelopmentSnapshot D;
	FCityFireSimulation Damage;
	FCityIdentity I;
	I.DisasterFrequency = ECityDisasterFrequency::Off;
	FCityDisasterSimulation Quake;
	FString Error;
	TestTrue(TEXT("Manual quake works with random disasters off"),
	         Quake.StartTest(ECityNaturalDisaster::Earthquake, {5, 9}, 1));
	TestFalse(TEXT("One simultaneous event"), Quake.StartTest(ECityNaturalDisaster::Tornado, {5, 9}, 1));
	auto Legacy = Quake.GetSnapshot();
	Legacy.Events.Last().Profile = 1;
	Legacy.Events.Last().SeverityPercent = 0;
	Legacy.Events.Last().StartStep = 5;
	Legacy.Events.Last().EndStep = 11;
	Quake.RestoreState(Legacy, 0, Error);
	Quake.AdvanceToStep(4, City, D, Damage, I, 1000, 25, 1);
	TestTrue(TEXT("Warning does not damage"), Damage.GetSnapshot().DamagedBuildings.IsEmpty());
	const auto Pending = Quake.GetSnapshot();
	FCityDisasterSimulation Restored;
	TestTrue(TEXT("Warning restores"), Restored.RestoreState(Pending, 4, Error));
	Quake.AdvanceToStep(5, City, D, Damage, I, 1000, 25, 1);
	TestEqual(TEXT("Quake is capped at three repairable buildings"),
	          Damage.GetSnapshot().DamagedBuildings.Num(), 3);
	for (const auto& P : Damage.GetSnapshot().DamagedBuildings)
	{
		TestEqual(TEXT("Cause is earthquake"), P.Value.Cause, 1);
		TestEqual(TEXT("Moderate repair damage"), P.Value.Damage, 40);
	}
	TestTrue(TEXT("Natural damage validates without fire introduction"),
	         FCityFireSimulation::ValidateState(Damage.GetSnapshot(), 0, Error));
	FCityFireSimulation RestoredDamage;
	Restored.AdvanceToStep(5, City, D, RestoredDamage, I, 1000, 25, 1);
	TestEqual(TEXT("Restored warning applies damage once"),
	          RestoredDamage.GetSnapshot().DamagedBuildings.Num(),
	          Damage.GetSnapshot().DamagedBuildings.Num());
	const auto Damaged = Damage.GetSnapshot();
	const int32 Count = Damaged.DamagedBuildings.Num();
	Quake.AdvanceToStep(11, City, D, Damage, I, 1000, 25, 1);
	TestEqual(TEXT("Committed damage is not repeated"), Damage.GetSnapshot().DamagedBuildings.Num(), Count);
	TestFalse(TEXT("Event ends at its exact boundary"), Quake.GetSnapshot().Current() != nullptr);
	TestTrue(TEXT("Finished history remains valid"),
	         FCityDisasterSimulation::ValidateState(Quake.GetSnapshot(), 11, Error));
	FCityFireSimulation TornadoDamage;
	FCityDisasterSimulation Tornado;
	Tornado.StartTest(ECityNaturalDisaster::Tornado, {5, 9}, 1);
	Legacy = Tornado.GetSnapshot();
	Legacy.Events.Last().Profile = 1;
	Legacy.Events.Last().SeverityPercent = 0;
	Legacy.Events.Last().StartStep = 5;
	Legacy.Events.Last().EndStep = 17;
	Tornado.RestoreState(Legacy, 0, Error);
	Tornado.AdvanceToStep(5, City, D, TornadoDamage, I, 1000, 25, 1);
	TestTrue(TEXT("Tornado begins with actual consequences"),
	         !TornadoDamage.GetSnapshot().DamagedBuildings.IsEmpty());
	const auto E = Tornado.GetSnapshot().Events.Last();
	TestEqual(TEXT("Wind-directed path advances east"), FCityDisasterSimulation::Position(E, 8).X, 8);
	TestEqual(TEXT("Path stays on map"), FCityDisasterSimulation::Position(E, 1000).X, 16);
	Tornado.SetFrequency(ECityDisasterFrequency::Off, 25);
	TestTrue(TEXT("Committed active event survives Off"), Tornado.GetSnapshot().Current() != nullptr);
	Tornado.AdvanceToStep(17, City, D, TornadoDamage, I, 1000, 25, 1);
	TestTrue(TEXT("Tornado total affected cap"), Tornado.GetSnapshot().Events.Last().Affected.Num() <= 3);
	auto Bad = Tornado.GetSnapshot();
	if (!TestTrue(TEXT("Tornado history has an affected identity"), !Bad.Events.Last().Affected.IsEmpty()))
		return false;
	const uint64 Duplicate = Bad.Events.Last().Affected[0];
	Bad.Events.Last().Affected.Add(Duplicate);
	TestFalse(TEXT("Duplicate consequence rejected"), FCityDisasterSimulation::ValidateState(Bad, 17, Error));
	FCityDisasterSimulation Off;
	const uint32 Seed = Off.GetSnapshot().RandomState;
	Off.AdvanceToStep(10000, City, D, TornadoDamage, I, 1000, 25, 1);
	TestEqual(TEXT("Off has no random events"), Off.GetSnapshot().TotalEvents, int64(0));
	TestEqual(TEXT("Off consumes no disaster RNG"), Off.GetSnapshot().RandomState, Seed);
	// Fixed-step catch-up and restore use the same event/RNG schedule.
	I.DisasterFrequency = ECityDisasterFrequency::High;
	FCityDisasterSimulation Fine, Coarse;
	FCityFireSimulation FD, CD;
	for (int64 Step = 1; Step <= 3000; ++Step)
		Fine.AdvanceToStep(Step, City, D, FD, I, 1000, 25, 1);
	Coarse.AdvanceToStep(3000, City, D, CD, I, 1000, 25, 1);
	TestTrue(TEXT("Seeded random events occur"), Fine.GetSnapshot().TotalEvents > 0);
	TestEqual(TEXT("Catch-up RNG identical"), Fine.GetSnapshot().RandomState,
	          Coarse.GetSnapshot().RandomState);
	TestEqual(TEXT("Catch-up event totals identical"), Fine.GetSnapshot().TotalEvents,
	          Coarse.GetSnapshot().TotalEvents);
	TestEqual(TEXT("Catch-up consequence totals identical"), FD.GetSnapshot().DamagedBuildings.Num(),
	          CD.GetSnapshot().DamagedBuildings.Num());
	TestTrue(TEXT("Bounded history validates"),
	         FCityDisasterSimulation::ValidateState(Fine.GetSnapshot(), 3000, Error));
	auto Warning = Pending;
	Warning.Events.Last().bTest = false;
	FCityDisasterSimulation Cancel;
	Cancel.RestoreState(Warning, 4, Error);
	Cancel.SetFrequency(ECityDisasterFrequency::Off, 25);
	TestFalse(TEXT("Off cancels an uncommitted randomized warning"),
	          Cancel.GetSnapshot().Current() != nullptr);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityNaturalSeverityTest, "CitySim.Disasters.CityScaledSeverityAndResume",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityNaturalSeverityTest::RunTest(const FString& Parameters)
{
	FCityPlacementModel City(100000);
	auto Road = City.BeginRoadPreview(FCityPlacementModel::EntranceCell);
	City.UpdateRoadPreviewEnd(Road, {30, 8});
	City.CommitRoad(Road);
	for (int32 Y = 9; Y < 19; ++Y)
		for (int32 X = 4; X < 14; ++X)
			Place(City, TEXT("Home"), {X, Y});
	if (!TestEqual(TEXT("100 completed urban buildings"), City.GetEntities().Num(), 100))
		return false;
	FCityDevelopmentSnapshot D;
	FCityIdentity I;
	I.DisasterFrequency = ECityDisasterFrequency::Low;
	FString Error;
	for (const auto Kind : {ECityNaturalDisaster::Earthquake, ECityNaturalDisaster::Tornado})
		for (const int32 Severity : {5, 25, 50})
		{
			FCityDisasterSimulation Fine, Coarse;
			FCityFireSimulation Damage, ResumedDamage;
			Fine.StartTest(Kind, {4, 9}, 1);
			auto Initial = Fine.GetSnapshot();
			auto& E = Initial.Events.Last();
			E.Profile = 2;
			E.StartStep = E.WarningStep + 5;
			E.SeverityPercent = Severity;
			E.EndStep =
			    E.StartStep + (Kind == ECityNaturalDisaster::Earthquake ? 10 + Severity : 20 + 2 * Severity);
			if (!TestTrue(TEXT("Configured severity valid"), Fine.RestoreState(Initial, 0, Error)))
				return false;
			Coarse.RestoreState(Initial, 0, Error);
			Fine.AdvanceToStep(4, City, D, Damage, I, 1000, 25, 1);
			TestTrue(TEXT("Warning still protects buildings"),
			         Damage.GetSnapshot().DamagedBuildings.IsEmpty());
			const auto Planned = Fine.GetSnapshot();
			TestEqual(TEXT("Impact budget scales with city"), Planned.Events.Last().PlannedTargets.Num(),
			          Severity);
			const int64 End = Planned.Events.Last().EndStep;
			Fine.AdvanceToStep(End / 2, City, D, Damage, I, 1000, 25, 1);
			const auto Mid = Fine.GetSnapshot();
			FCityDisasterSimulation Resume;
			Resume.RestoreState(Mid, Mid.LastFixedStepId, Error);
			ResumedDamage.RestoreState(Damage.GetSnapshot(), End, Error);
			for (int64 Step = Mid.LastFixedStepId + 1; Step <= End; ++Step)
				Fine.AdvanceToStep(Step, City, D, Damage, I, 1000, 25, 1);
			Resume.AdvanceToStep(End, City, D, ResumedDamage, I, 1000, 25, 1);
			FCityFireSimulation CoarseDamage;
			Coarse.AdvanceToStep(End, City, D, CoarseDamage, I, 1000, 25, 1);
			TestEqual(TEXT("Low frequency does not shrink severity"),
			          Damage.GetSnapshot().DamagedBuildings.Num(), Severity);
			TestEqual(TEXT("Resume does not repeat or omit destruction"),
			          ResumedDamage.GetSnapshot().DamagedBuildings.Num(), Severity);
			TestEqual(TEXT("Catch-up applies the same destruction"),
			          CoarseDamage.GetSnapshot().DamagedBuildings.Num(), Severity);
			for (const auto& Pair : Damage.GetSnapshot().DamagedBuildings)
				TestEqual(TEXT("Destroyed buildings need full repair"), Pair.Value.Damage, 100);
			TestTrue(TEXT("Large structural state validates"),
			         FCityFireSimulation::ValidateState(Damage.GetSnapshot(), End, Error));
			TestTrue(TEXT("Saved event validates"),
			         FCityDisasterSimulation::ValidateState(Fine.GetSnapshot(), End, Error));
			auto Invalid = Fine.GetSnapshot();
			Invalid.Events.Last().SeverityPercent = 51;
			TestFalse(TEXT("Severity above half the city rejected"),
			          FCityDisasterSimulation::ValidateState(Invalid, End, Error));
			Invalid = Fine.GetSnapshot();
			Invalid.Events.Last().PlannedTargets[1] = Invalid.Events.Last().PlannedTargets[0];
			TestFalse(TEXT("Duplicate planned target rejected"),
			          FCityDisasterSimulation::ValidateState(Invalid, End, Error));
		}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityNaturalMotionTest, "CitySim.Disasters.RadialQuakeAndContinuousTornado",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityNaturalMotionTest::RunTest(const FString& Parameters)
{
	FCityPlacementModel City(100000);
	auto Road = City.BeginRoadPreview(FCityPlacementModel::EntranceCell);
	City.UpdateRoadPreviewEnd(Road, {30, 8});
	City.CommitRoad(Road);
	for (int32 Y = 9; Y < 19; ++Y)
		for (int32 X = 4; X < 14; ++X)
			if (X != 8 || Y != 13)
				Place(City, TEXT("Home"), {X, Y});
	const uint64 Station = Place(City, TEXT("Fire"), {8, 13});
	FCityDevelopmentSnapshot D;
	FCityIdentity I;
	I.DisasterFrequency = ECityDisasterFrequency::Off;
	FString Error;
	FCityDisasterSimulation Quake;
	FCityFireSimulation Damage;
	Quake.StartTest(ECityNaturalDisaster::Earthquake, {8, 13}, 1);
	auto LegacyMotion = Quake.GetSnapshot();
	auto& LegacyQuake = LegacyMotion.Events.Last();
	LegacyQuake.Profile = 3;
	LegacyQuake.StartStep = 5;
	LegacyQuake.EndStep = LegacyQuake.StartStep + 10 + LegacyQuake.SeverityPercent;
	Quake.RestoreState(LegacyMotion, 0, Error);
	Quake.AdvanceToStep(4, City, D, Damage, I, 1000, 25, 1);
	const auto Warning = Quake.GetSnapshot();
	const auto E = Warning.Events.Last();
	int32 Expected = 0;
	for (const auto& Pair : City.GetEntities())
	{
		const auto C = Pair.Value.Anchor;
		if (FMath::Square(C.X - E.Center.X) + FMath::Square(C.Y - E.Center.Y) <= E.RadiusSquared)
			++Expected;
	}
	TestTrue(TEXT("No quake damage during warning"), Damage.GetSnapshot().DamagedBuildings.IsEmpty());
	Quake.AdvanceToStep(E.StartStep, City, D, Damage, I, 1000, 25, 1);
	TestEqual(TEXT("Every in-radius building destroyed simultaneously"),
	          Damage.GetSnapshot().DamagedBuildings.Num(), Expected);
	Damage.RestoreFireproofStations(City);
	TestTrue(TEXT("Fireproof station still receives earthquake damage"),
	         Damage.GetSnapshot().IsDisabled(Station));
	for (const auto& Pair : City.GetEntities())
	{
		const auto C = Pair.Value.Anchor;
		TestEqual(TEXT("Outside blast radius remains unaffected"), Damage.GetSnapshot().IsDisabled(Pair.Key),
		          FMath::Square(C.X - E.Center.X) + FMath::Square(C.Y - E.Center.Y) <= E.RadiusSquared);
	}
	TestTrue(TEXT("Quake visual stays at epicenter"),
	         FCityDisasterSimulation::ContinuousPosition(E, E.StartStep + .5) ==
	             FVector2D(E.Center.X, E.Center.Y));
	Quake.AdvanceToStep(E.EndStep, City, D, Damage, I, 1000, 25, 1);
	TestEqual(TEXT("No sequential aftershock damage"), Damage.GetSnapshot().DamagedBuildings.Num(), Expected);
	FCityDisasterSimulation Tornado;
	FCityFireSimulation TD;
	Tornado.StartTest(ECityNaturalDisaster::Tornado, {4, 9}, 1);
	LegacyMotion = Tornado.GetSnapshot();
	auto& LegacyTornado = LegacyMotion.Events.Last();
	LegacyTornado.Profile = 3;
	LegacyTornado.StartStep = 5;
	LegacyTornado.EndStep = LegacyTornado.StartStep + 20 + 2 * LegacyTornado.SeverityPercent;
	Tornado.RestoreState(LegacyMotion, 0, Error);
	Tornado.AdvanceToStep(4, City, D, TD, I, 1000, 25, 1);
	const auto T = Tornado.GetSnapshot().Events.Last();
	TestTrue(TEXT("Multiple tornado destinations"), T.Path.Num() > 1);
	double Minimum = MAX_dbl, Maximum = 0;
	auto Prev = FCityDisasterSimulation::ContinuousPosition(T, T.StartStep);
	for (double Step = T.StartStep + .05; Step < T.EndStep - 1; Step += .05)
	{
		const auto P = FCityDisasterSimulation::ContinuousPosition(T, Step);
		const double Travel = (P - Prev).Size();
		Minimum = FMath::Min(Minimum, Travel);
		Maximum = FMath::Max(Maximum, Travel);
		Prev = P;
	}
	TestTrue(TEXT("Continuous travel has no dwell periods"), Minimum > .0001);
	TestTrue(TEXT("Travel speed stays roughly uniform"), Maximum < Minimum * 1.3);
	for (int64 Step = T.StartStep + 1; Step < T.EndStep - 1; ++Step)
		TestTrue(TEXT("No jump at fixed-step boundary"),
		         (FCityDisasterSimulation::ContinuousPosition(T, Step - .00001) -
		          FCityDisasterSimulation::ContinuousPosition(T, Step + .00001))
		                 .Size() < .001);
	const double MidDistance = T.TravelDistances[1] * .5;
	const double MidStep =
	    T.StartStep + (T.EndStep - T.StartStep - 1) * MidDistance / T.TravelDistances.Last();
	const FVector2D A(T.Path[0].X, T.Path[0].Y), B(T.Path[1].X, T.Path[1].Y);
	TestTrue(TEXT("Travel bends away from straight line"),
	         (FCityDisasterSimulation::ContinuousPosition(T, MidStep) - (A + B) * .5).Size() > .1);
	for (int64 Step = T.StartStep; Step < T.EndStep; ++Step)
	{
		Tornado.AdvanceToStep(Step, City, D, TD, I, 1000, 25, 1);
		const double Distance = static_cast<double>(Step - T.StartStep) / (T.EndStep - T.StartStep - 1) *
		                        T.TravelDistances.Last();
		for (int32 Index = 0; Index < T.Path.Num(); ++Index)
			TestEqual(TEXT("Damage follows arrival along curve"),
			          TD.GetSnapshot().IsDisabled(T.PlannedTargets[Index]),
			          T.TravelDistances[Index] <= Distance);
	}
	TestTrue(TEXT("New event validates"),
	         FCityDisasterSimulation::ValidateState(Tornado.GetSnapshot(), T.EndStep, Error));
	auto Bad = Tornado.GetSnapshot();
	Bad.Events.Last().TravelDistances[1] += 1;
	TestFalse(TEXT("Altered curve travel timing rejected"),
	          FCityDisasterSimulation::ValidateState(Bad, T.EndStep, Error));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityNaturalIntegrationTest,
                                 "CitySim.Disasters.CityCapacityRepairSaveAndNormalFires",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityNaturalIntegrationTest::RunTest(const FString& Parameters)
{
	const auto V = UWorld::InitializationValues()
	                   .AllowAudioPlayback(false)
	                   .CreatePhysicsScene(false)
	                   .CreateNavigation(false)
	                   .CreateAISystem(false)
	                   .ShouldSimulatePhysics(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("NaturalDisasterTest"), nullptr, true,
	                                  ERHIFeatureLevel::Num, &V);
	auto* Sim = World->GetSubsystem<UCitySimulationSubsystem>();
	auto* Catalog = LoadObject<UCityBuildingCatalog>(
	    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
	Sim->RegisterBuildingCatalog(Catalog);
	auto& C = Sim->GetPlacementModel();
	C.SetTerrain(nullptr);
	auto Road = C.BeginRoadPreview(FCityPlacementModel::EntranceCell);
	C.UpdateRoadPreviewEnd(Road, {30, 8});
	C.CommitRoad(Road);
	const TCHAR* Names[] = {TEXT("Power"), TEXT("Service"), TEXT("Home"), TEXT("Shop")};
	const FCityCellCoord Cells[] = {{1, 9}, {3, 9}, {6, 9}, {10, 9}};
	uint64 Home = 0;
	for (int32 N = 0; N < 4; ++N)
	{
		const auto* Def = Catalog->FindDefinition(Names[N]);
		auto P = C.BeginPreview(*Def, Cells[N]);
		const auto R = C.CommitPlacement(P, *Def);
		TestTrue(TEXT("Real utility/home/jobs place"), R.IsSuccess());
		if (N == 2)
			Home = R.EntityId;
	}
	Sim->Development.InitializeExistingAsOperational(C, Catalog, 0);
	Sim->EconomyTuning.FireBaseRisk = 0;
	Sim->EconomyTuning.FixedStepSeconds = .125;
	Sim->SetDisasterFrequency(ECityDisasterFrequency::Off);
	Sim->StartScenario();
	Sim->Tick(.125f);
	FString PreviewError;
	Sim->SaveCityToPath(
	    FPaths::Combine(FPaths::ProjectDir(), TEXT("../Artifacts/Preview/NaturalDisasters-NaturalBaseline.json")),
	    PreviewError);
	const int32 Capacity = Sim->GetServiceSnapshot().HousingCapacity;
	TestTrue(TEXT("Baseline housing operates"), Capacity > 0);
	Sim->SetSimulationPaused(true);
	Sim->SetSimulationSpeed(5);
	const int64 OnsetBefore = Sim->Economy.GetSnapshot().FixedStepId;
	TestTrue(TEXT("Public test starts without advance warning"), Sim->StartEarthquakeTest());
	TestEqual(TEXT("Manual onset is next simulation step"),
	          Sim->GetDisasterSnapshot().Events.Last().StartStep, OnsetBefore + 1);
	Sim->Tick(1.f);
	TestEqual(TEXT("Paused time stays paused"), Sim->Economy.GetSnapshot().FixedStepId, OnsetBefore);
	Sim->SetSimulationPaused(false);
	Sim->Tick(.125f);
	TestEqual(TEXT("Disaster onset resets speed to 1x"), Sim->Economy.GetSpeedMultiplier(), 1);
	TestEqual(TEXT("Same-frame fast catch-up slows at onset"), Sim->Economy.GetSnapshot().FixedStepId,
	          OnsetBefore + 1);
	Sim->SetSimulationSpeed(3);
	Sim->Tick(.001f);
	TestEqual(TEXT("Player can change speed after onset"), Sim->Economy.GetSpeedMultiplier(), 3);
	Sim->SetSimulationSpeed(1);
	for (int32 N = 0; N < 4; ++N)
		Sim->Tick(.125f);
	TestTrue(TEXT("Quake disables real housing capacity"),
	         Sim->GetServiceSnapshot().HousingCapacity < Capacity);
	TestEqual(TEXT("Home damage identifies earthquake"),
	          Sim->GetFireSnapshot().DamagedBuildings.FindRef(Home).Cause, 1);
	const FString Path = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/NaturalDisaster"),
	                                     FGuid::NewGuid().ToString() + TEXT(".json"));
	FString Error;
	TestTrue(TEXT("Active event and damage save"), Sim->SaveCityToPath(Path, Error));
	const auto Before = Sim->GetDisasterSnapshot();
	TestTrue(TEXT("Active event reloads"), Sim->LoadCityFromPath(Path, Error));
	TestEqual(TEXT("Saved RNG preserved"), Sim->GetDisasterSnapshot().RandomState, Before.RandomState);
	TestEqual(TEXT("Saved severity preserved"), Sim->GetDisasterSnapshot().Events.Last().SeverityPercent,
	          Before.Events.Last().SeverityPercent);
	TestTrue(TEXT("Saved destruction plan preserved"),
	         Sim->GetDisasterSnapshot().Events.Last().PlannedTargets == Before.Events.Last().PlannedTargets);
	TestTrue(TEXT("Saved curve distances preserved"),
	         Sim->GetDisasterSnapshot().Events.Last().TravelDistances ==
	             Before.Events.Last().TravelDistances);
	TestEqual(TEXT("Saved radius preserved"), Sim->GetDisasterSnapshot().Events.Last().RadiusSquared,
	          Before.Events.Last().RadiusSquared);
	TestEqual(TEXT("Saved duration preserved"), Sim->GetDisasterSnapshot().Events.Last().EndStep,
	          Before.Events.Last().EndStep);
	TestEqual(TEXT("Saved damage preserved"), Sim->GetFireSnapshot().DamagedBuildings.FindRef(Home).Cause, 1);
	TArray<uint64> RepairIds;
	Sim->GetFireSnapshot().DamagedBuildings.GetKeys(RepairIds);
	for (uint64 Id : RepairIds)
		TestTrue(TEXT("Affordable existing repair works for all quake losses"), Sim->RepairBuilding(Id));
	Sim->SetSimulationPaused(false);
	for (int32 N = 0; N < Sim->EconomyTuning.FireRepairSteps + 1; ++N)
		Sim->Tick(.125f);
	TestFalse(TEXT("Repair restores the home"), Sim->GetFireSnapshot().IsDisabled(Home));
	TestTrue(TEXT("Housing supply restores"), Sim->GetServiceSnapshot().HousingCapacity >= Capacity);
	TestTrue(TEXT("Ordinary fire can start with natural disasters Off"),
	         Sim->StartSafetyTest(Home, true, false));
	TestTrue(TEXT("Ordinary fire creates separate fire cause"),
	         Sim->GetFireSnapshot().IsBurning(Home) &&
	             Sim->GetFireSnapshot().DamagedBuildings.FindRef(Home).Cause == 0);
	const int64 FiresBefore = Sim->GetFireSnapshot().TotalIncidents;
	Sim->EconomyTuning.FirePopulationThreshold = 0;
	Sim->EconomyTuning.FireGraceMonths = 0;
	Sim->EconomyTuning.FireBaseRisk = 100;
	Sim->EconomyTuning.FireCheckIntervalSteps = 1;
	Sim->Tick(.125f);
	TestTrue(TEXT("Spontaneous ordinary fires still run with natural disasters Off"),
	         Sim->GetFireSnapshot().TotalIncidents > FiresBefore);
	IFileManager::Get().Delete(*Path);
	IFileManager::Get().Delete(*(Path + TEXT(".bak")));
	IFileManager::Get().Delete(*(Path + TEXT(".tmp")));
	World->DestroyWorld(false);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityTornadoUtilityTest,
                                 "CitySim.Disasters.UtilityTornadoOutageRepairAndResume",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityTornadoUtilityTest::RunTest(const FString& Parameters)
{
	auto* Catalog = LoadObject<UCityBuildingCatalog>(
	    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
	if (!TestNotNull(TEXT("Utility catalog"), Catalog))
		return false;
	FCityPlacementModel City(100000);
	auto Road = City.BeginRoadPreview(FCityPlacementModel::EntranceCell);
	City.UpdateRoadPreviewEnd(Road, {63, 8});
	City.CommitRoad(Road);
	const auto PlaceReal = [&](const TCHAR* Name, int32 X) {
		const auto* Def = Catalog->FindDefinition(Name);
		auto P = City.BeginPreview(*Def, {X, 9});
		return City.CommitPlacement(P, *Def).EntityId;
	};
	const uint64 Power = PlaceReal(TEXT("Power"), 8), Water = PlaceReal(TEXT("Service"), 12);
	for (int32 X = 24; X < 48; X += 4)
		PlaceReal(TEXT("Home"), X);
	const uint64 Station = PlaceReal(TEXT("Fire"), 52);
	if (!TestTrue(TEXT("Real utilities placed"), Power != 0 && Water != 0 && Station != 0))
		return false;
	FCityDevelopmentSimulation Dev;
	Dev.InitializeExistingAsOperational(City, Catalog, 0);
	auto Development = Dev.GetSnapshot();
	Development.States.FindChecked(Power).DensityLevel = ECityDensityLevel::Medium;
	Development.States.FindChecked(Water).DensityLevel = ECityDensityLevel::High;
	FCityEconomyTuning Tuning;
	Tuning.FireBaseRisk = 0;
	FCityIdentity Identity;
	Identity.DisasterFrequency = ECityDisasterFrequency::Off;
	FString Error;
	FCityFireSimulation Damage;
	FCityDisasterSimulation Tornado;
	Tornado.StartTest(ECityNaturalDisaster::Tornado, {8, 9}, 1);
	auto Initial = Tornado.GetSnapshot();
	Initial.Events.Last().SeverityPercent = 25;
	Initial.Events.Last().EndStep = Initial.Events.Last().StartStep + 70;
	if (!TestTrue(TEXT("Configured tornado restores"), Tornado.RestoreState(Initial, 0, Error)))
	{
		AddError(Error);
		return false;
	}
	Tornado.AdvanceToStep(1, City, Development, Damage, Identity, 1000, 25, 1);
	const auto Planned = Tornado.GetSnapshot();
	const auto& Event = Planned.Events.Last();
	TestEqual(TEXT("Severity includes utilities but not fire station"), Event.InitialBuildings, 8);
	TestEqual(TEXT("Same percentage damage budget"), Event.PlannedTargets.Num(), 2);
	TestTrue(TEXT("Nearby power and water are planned"),
	         Event.PlannedTargets.Contains(Power) && Event.PlannedTargets.Contains(Water));
	TestTrue(TEXT("First utility damaged at arrival"), Damage.GetSnapshot().IsDisabled(Power));
	TestFalse(TEXT("Second utility not damaged before arrival"), Damage.GetSnapshot().IsDisabled(Water));
	FCityDisasterSimulation Resumed;
	FCityFireSimulation ResumedDamage;
	TestTrue(TEXT("Active utility path resumes"), Resumed.RestoreState(Planned, 1, Error));
	TestTrue(TEXT("Utility damage resumes"), ResumedDamage.RestoreState(Damage.GetSnapshot(), 1, Error));
	for (int64 Step = 2; Step <= Event.EndStep; ++Step)
		Tornado.AdvanceToStep(Step, City, Development, Damage, Identity, 1000, 25, 1);
	Resumed.AdvanceToStep(Event.EndStep, City, Development, ResumedDamage, Identity, 1000, 25, 1);
	TestTrue(TEXT("Resumed plan remains identical"),
	         Resumed.GetSnapshot().Events.Last().PlannedTargets == Event.PlannedTargets);
	TestEqual(TEXT("Catch-up and fine stepping damage same count"),
	          ResumedDamage.GetSnapshot().DamagedBuildings.Num(),
	          Damage.GetSnapshot().DamagedBuildings.Num());
	for (uint64 Id : {Power, Water})
	{
		TestEqual(TEXT("Utility destruction cause is tornado"),
		          Damage.GetSnapshot().DamagedBuildings.FindRef(Id).Cause, 2);
		TestEqual(TEXT("Resumed utility damage matches"),
		          ResumedDamage.GetSnapshot().DamagedBuildings.FindRef(Id).Damage, 100);
	}
	TestFalse(TEXT("Fire station targeting unchanged"), Damage.GetSnapshot().IsDisabled(Station));
	FCityPopulationSnapshot Population;
	const auto Build = [&]() {
		return FCityServiceEvaluator::BuildSnapshot(City, Tuning, Catalog, &Development, nullptr, &Population,
		                                            &Damage.GetSnapshot());
	};
	auto Services = Build();
	TestEqual(TEXT("Tornado stops power supply"), Services.PowerCapacity, 0);
	TestEqual(TEXT("Tornado stops water supply"), Services.ServiceCapacity, 0);
	const int64 Before = City.GetFunds();
	int64 Deadline = 0;
	for (uint64 Id : {Power, Water})
	{
		const auto* Def = Catalog->FindDefinition(City.GetEntities().FindChecked(Id).DefinitionId);
		TestTrue(TEXT("Existing paid repair accepts tornado utility"),
		         Damage.Repair(Id, City, *Def, Tuning, Error));
		Deadline =
		    FMath::Max(Deadline, Damage.GetSnapshot().DamagedBuildings.FindChecked(Id).RepairCompleteStep);
	}
	TestTrue(TEXT("Repair charges treasury"), City.GetFunds() < Before);
	TestEqual(TEXT("Power stays off during repair"), Build().PowerCapacity, 0);
	Damage.AdvanceToStep(Deadline, City, Services, Development, {}, Tuning, false);
	Services = Build();
	TestEqual(TEXT("Repair restores medium power floor"), Services.PowerCapacity, 200);
	TestEqual(TEXT("Repair restores high water floor"), Services.ServiceCapacity, 300);
	FCityDisasterSimulation Legacy;
	FCityFireSimulation LegacyDamage;
	Initial.Events.Last().Profile = 3;
	Initial.Events.Last().StartStep = 5;
	Initial.Events.Last().EndStep = 75;
	if (!TestTrue(TEXT("Older unplanned tornado restores"), Legacy.RestoreState(Initial, 0, Error)))
	{
		AddError(Error);
		return false;
	}
	Legacy.AdvanceToStep(4, City, Development, LegacyDamage, Identity, 1000, 25, 1);
	TestFalse(TEXT("Legacy plan still excludes power"),
	          Legacy.GetSnapshot().Events.Last().PlannedTargets.Contains(Power));
	TestFalse(TEXT("Legacy plan still excludes water"),
	          Legacy.GetSnapshot().Events.Last().PlannedTargets.Contains(Water));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityNaturalAssetsTest, "CitySim.Disasters.PooledParticleAssets",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityNaturalAssetsTest::RunTest(const FString& Parameters)
{
	for (const TCHAR* Name : {TEXT("PS_CityEarthquakeDust"), TEXT("PS_CityTornado")})
	{
		auto* P = LoadObject<UParticleSystem>(
		    nullptr, *FString::Printf(TEXT("/Game/CitySim/Art/Effects/%s.%s"), Name, Name));
		if (!TestNotNull(TEXT("Disaster emitter asset"), P))
			continue;
		TestTrue(TEXT("Bounded emitter count"), P->Emitters.Num() >= 1 && P->Emitters.Num() <= 8);
		TestTrue(TEXT("Fixed effect bounds"), P->bUseFixedRelativeBoundingBox);
		for (const auto& E : P->Emitters)
			TestTrue(TEXT("Unlit smoke material and bounded lifetime modules"),
			         E && E->LODLevels.Num() == 1 && E->LODLevels[0]->RequiredModule->Material &&
			             E->InitialAllocationCount <= 96);
	}
	return true;
}
#endif
