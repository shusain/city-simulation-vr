#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Presentation/CityServiceGuidance.h"
#include "Presentation/CityDiagnosticOverlay.h"
#include "Presentation/CityWaterPlacementOverlay.h"
#include "Simulation/CityEducationSimulation.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Simulation/CityDevelopmentSimulation.h"
#include "Simulation/CityGrowthSimulation.h"
#include "Simulation/CityPopulationSimulation.h"
#include "Simulation/CityTerrain.h"
#include "Presentation/CityBoardActor.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "ProceduralMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

namespace
{
	uint64 Place(FCityPlacementModel& City, const TCHAR* Name, int32 X)
	{
		FCityBuildingDefinition D;
		D.DefinitionId = Name;
		D.DisplayName = Name;
		D.Footprint = {1, 1};
		D.Cost = 1;
		auto Preview = City.BeginPreview(D, {X, 9});
		return City.CommitPlacement(Preview, D).EntityId;
	}
	void Road(FCityPlacementModel& City)
	{
		auto Preview = City.BeginRoadPreview(City.GetEntranceCell());
		City.UpdateRoadPreviewEnd(Preview, {50, 8});
		City.CommitRoad(Preview);
	}
	FCityPopulationSnapshot People(uint64 Home)
	{
		FCityPopulationSnapshot P;
		for (int32 I = 0; I < 4; ++I)
		{
			FCityCitizenRecord C;
			C.Id = I + 1;
			C.HomeEntityId = Home;
			C.Education = static_cast<ECityEducationLevel>(I);
			P.Citizens.Add(C);
		}
		return P;
	}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityEducationGuidanceTest, "CitySim.ServiceGuidance.EducationTierCapacityRouteOutage",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityEducationGuidanceTest::RunTest(const FString& Parameters)
{
	FCityPlacementModel City(100000);
	Road(City);
	const uint64 Home = Place(City, TEXT("Home"), 6), Primary = Place(City, TEXT("PrimarySchool"), 2);
	auto P = People(Home);
	FCityEconomyTuning T;
	FCityDevelopmentSnapshot Development;
	FCityServiceSnapshot S;
	S.EntityResidents.Add(Home, 4);
	S.PublicStaffPercent.Add(Primary, 100);
	FCityEducationEvaluator::Build(City, S, T, &P, nullptr);
	auto Detail = FCityServiceGuidance::Education(City, S, P, T, Development, Home);
	TestTrue(TEXT("A nearby primary cannot teach Basic residents"),
	         Detail.Contains(TEXT("build Secondary School or School")));
	TestTrue(TEXT("A college is required to advance Secondary residents"), Detail.Contains(TEXT("build College")));
	TestTrue(TEXT("Higher residents are not described as lacking access"),
	         Detail.Contains(TEXT("Higher educated 1: no further schooling needed")));
	TestFalse(TEXT("Actually allocated primary resident is not reported short"),
	          Detail.Contains(TEXT("need None to Basic")));
	const uint64 Secondary = Place(City, TEXT("SecondarySchool"), 3);
	S = {};
	S.EntityResidents.Add(Home, 4);
	S.PublicStaffPercent.Add(Primary, 100);
	S.PublicStaffPercent.Add(Secondary, 100);
	S.EntityFailures.Add(Secondary, ECityOperationFailure::NoPower);
	FCityEducationEvaluator::Build(City, S, T, &P, nullptr);
	Detail = FCityServiceGuidance::Education(City, S, P, T, Development, Home);
	TestTrue(TEXT("Nearby compatible school outage names cause and remedy"),
	         Detail.Contains(TEXT("at 3 road steps")) && Detail.Contains(TEXT("offline:")) &&
	             Detail.Contains(TEXT("restore service")));
	S = {};
	S.EntityResidents.Add(Home, 4);
	S.PublicStaffPercent.Add(Primary, 100);
	S.PublicStaffPercent.Add(Secondary, 25);
	T.SchoolCapacity = 1;
	FCityEducationEvaluator::Build(City, S, T, &P, nullptr);
	Detail = FCityServiceGuidance::Education(City, S, P, T, Development, Home);
	TestTrue(TEXT("Zero capacity due to staff scaling does not claim enough places"),
	         Detail.Contains(TEXT("no staffed places")));
	// A second Basic resident competes for the school's finite one place.
	const auto Extra = P.Citizens[1];
	P.Citizens.Add(Extra);
	P.Citizens.Last().Id = 5;
	S = {};
	S.EntityResidents.Add(Home, 5);
	S.PublicStaffPercent.Add(Primary, 100);
	S.PublicStaffPercent.Add(Secondary, 100);
	FCityEducationEvaluator::Build(City, S, T, &P, nullptr);
	Detail = FCityServiceGuidance::Education(City, S, P, T, Development, Home);
	TestTrue(TEXT("Finite education shortage reports actually consumed places"),
	         Detail.Contains(TEXT("places used 1/1")) && Detail.Contains(TEXT("add another compatible school")));
	T.SchoolRoadRangeHops = 2;
	S = {};
	S.EntityResidents.Add(Home, 5);
	S.PublicStaffPercent.Add(Primary, 100);
	S.PublicStaffPercent.Add(Secondary, 100);
	FCityEducationEvaluator::Build(City, S, T, &P, nullptr);
	Detail = FCityServiceGuidance::Education(City, S, P, T, Development, Home);
	TestTrue(TEXT("Out-of-range diagnosis uses real road distance"),
	         Detail.Contains(TEXT("is 3 road steps away")) && Detail.Contains(TEXT("build closer")));
	City.RemoveRoad({4, 8});
	S = {};
	S.EntityResidents.Add(Home, 5);
	S.PublicStaffPercent.Add(Primary, 100);
	S.PublicStaffPercent.Add(Secondary, 100);
	FCityEducationEvaluator::Build(City, S, T, &P, nullptr);
	Detail = FCityServiceGuidance::Education(City, S, P, T, Development, Home);
	TestTrue(TEXT("A nearby disconnected school has a route problem"), Detail.Contains(TEXT("no connected road path")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityWaterGuidanceTest, "CitySim.ServiceGuidance.BluePipesFiniteOutput",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityWaterGuidanceTest::RunTest(const FString& Parameters)
{
	FCityPlacementModel City(100000);
	Road(City);
	const uint64 SourceId = Place(City, TEXT("Service"), 2), Home = Place(City, TEXT("Home"), 6);
	FCityServiceSnapshot S;
	S.Water.RulesVersion = 1;
	FCityWaterSource Source;
	Source.EntityId = SourceId;
	Source.Capacity = 2;
	Source.bOperational = true;
	S.Water.Sources.Add(Source);
	FCityWaterEvaluator::BuildNetwork(City, S.Water, 32);
	FCityWaterEvaluator::Allocate(City, S.Water, Home, {6, 9}, {1, 1}, 5);
	TestEqual(TEXT("Actual allocation is capacity limited beside a blue pipe"), S.Water.EntityCauses.FindRef(Home),
	          ECityWaterFailure::SourceCapacity);
	const auto Revision = City.GetRevision();
	const auto Detail = FCityServiceGuidance::Water(City, S, Home);
	TestTrue(TEXT("Inspection distinguishes pressure and delivery"),
	         Detail.Contains(TEXT("delivered 2/5")) && Detail.Contains(TEXT("92%")));
	TestTrue(TEXT("Inspection gives source location, path length and exhausted output"),
	         Detail.Contains(TEXT("(2,9): 4 road steps")) && Detail.Contains(TEXT("used 2/2 | remaining 0")));
	TestTrue(TEXT("Capacity remedy does not prescribe shortening a healthy pipe"),
	         Detail.Contains(TEXT("add/upgrade")) && !Detail.Contains(TEXT("shorten")));
	TestEqual(TEXT("Explanation consumes no source supply"), S.Water.Sources[0].Used, 2);
	TestEqual(TEXT("Explanation does not edit city"), City.GetRevision(), Revision);
	FCityTerrainHeightfield Terrain;
	Terrain.Generate(FCityTerrainGenerationParameters());
	const auto Overlay = FCityWaterPlacementOverlay::Build(City, Terrain, S);
	TestTrue(TEXT("Placement legend explicitly separates pressure from spare supply"),
	         Overlay.Legend.Contains(TEXT("not spare supply")) && Overlay.Legend.Contains(TEXT("1 buildings short")));
	const auto* RoadSample =
	    Overlay.Roads.FindByPredicate([](const auto& R) { return R.Cell == FCityCellCoord(6, 8); });
	if (TestNotNull(TEXT("Consumer frontage pressure sample"), RoadSample))
	{
		TestTrue(TEXT("Pressure remains blue"), RoadSample->PressurePercent >= 60);
		TestEqual(TEXT("Road hover reports no spare output"), RoadSample->RemainingSourceOutput, 0);
	}
	S.Water.EntityCauses.Add(Home, ECityWaterFailure::LowPressure);
	TestTrue(TEXT("Low-pressure remedy is distance-specific"),
	         FCityServiceGuidance::Water(City, S, Home).Contains(TEXT("shorten the pipe route")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCitySchoolReachGuidanceTest, "CitySim.ServiceGuidance.PotentialReachAndActualPlaces",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCitySchoolReachGuidanceTest::RunTest(const FString& Parameters)
{
	FCityPlacementModel City(100000);
	Road(City);
	const uint64 School = Place(City, TEXT("School"), 2), Home = Place(City, TEXT("Home"), 6);
	FCityServiceSnapshot S;
	S.HomeEducationDemand.Add(Home, 4);
	S.HomeEducationPlaces.Add(Home, 1);
	FCityServiceReach Reach;
	FCityServiceEvaluator::BuildProviderReach(City, School, 5, true, Reach);
	FCityDiagnosticOverlayResult Overlay;
	FCityDiagnosticOverlayEvaluator::BuildEducationReach(City, S, Reach, TEXT("Proposed School"), 5, Overlay);
	TestTrue(TEXT("Boundary road reached"), Overlay.RoadCells.Contains(FCityCellCoord(7, 8)));
	TestFalse(TEXT("Beyond boundary road not reached"), Overlay.RoadCells.Contains(FCityCellCoord(8, 8)));
	TestTrue(TEXT("Hover explains exact route limit"), Overlay.DescribeCell({7, 8}).Contains(TEXT("5/5 steps")));
	TestEqual(TEXT("Actual partial access is distinct from potential reach"),
	          Overlay.Status[9 * S.Dimensions.GetWidth() + 6], uint8(3));
	TestTrue(TEXT("Home hover labels allocation from all schools"),
	         Overlay.DescribeCell({6, 9}).Contains(TEXT("all schools 1/4")));
	TestTrue(TEXT("Proposed range never promises available places"),
	         Overlay.Legend.Contains(TEXT("potential reach")) && Overlay.Legend.Contains(TEXT("matching tier")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityEmploymentGuidanceTest, "CitySim.ServiceGuidance.WorkQualificationsAndVacancies",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityEmploymentGuidanceTest::RunTest(const FString& Parameters)
{
	auto P = People(1);
	FCityBusinessLedgerRecord B;
	B.EntityId = 10;
	B.JobCapacity = 5;
	B.SkilledJobCapacity = 2;
	P.Businesses.Add(B);
	FCityPublicEmployerRecord E;
	E.EntityId = 20;
	E.JobCapacity = 2;
	P.PublicEmployers.Add(E);
	FCityServiceSnapshot S;
	S.PublicMinimumEducation.Add(20, 3);
	P.Citizens[0].WorkplaceEntityId = 10;
	P.Citizens[0].bOnMedicalLeave = true;
	P.Citizens[3].JailStationId = 30;
	const auto Home = FCityServiceGuidance::Employment(P, S, 1, true);
	TestTrue(TEXT("Home sees entry and skilled vacancies separately"),
	         Home.Contains(TEXT("entry 2 | skilled (Secondary+) 2")));
	TestTrue(TEXT("Higher civic vacancies expose minimum qualification"), Home.Contains(TEXT("Higher 2")));
	TestTrue(TEXT("Ready unemployed excludes custody and medical leave"),
	         Home.Contains(TEXT("None 0 | Basic 1 | Secondary 1 | Higher 0")));
	TestTrue(TEXT("Inspection does not invent a commute hiring radius"), Home.Contains(TEXT("Hiring is citywide")));
	const auto Business = FCityServiceGuidance::Employment(P, S, 10, false);
	TestTrue(TEXT("Leave explained separately from assignment"),
	         Business.Contains(TEXT("Active staff 0; assigned staff on leave/in custody 1")));
	const auto Public = FCityServiceGuidance::Employment(P, S, 20, false);
	TestTrue(TEXT("Unfilled Higher job explains education rather than distance"),
	         Public.Contains(TEXT("Higher or higher; 0 eligible unemployed")));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCitySchoolReachPresentationTest,
                                 "CitySim.ServiceGuidance.SchoolPlacementInspectionAndCleanup",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCitySchoolReachPresentationTest::RunTest(const FString& Parameters)
{
	const auto Init = UWorld::InitializationValues()
	                      .AllowAudioPlayback(false)
	                      .CreatePhysicsScene(false)
	                      .CreateNavigation(false)
	                      .CreateAISystem(false)
	                      .ShouldSimulatePhysics(false);
	auto* W = UWorld::CreateWorld(EWorldType::Game, false, TEXT("SchoolReachPresentation"), nullptr, true,
	                              ERHIFeatureLevel::Num, &Init);
	auto* Board = W->SpawnActor<ACityBoardActor>();
	auto* Sim = W->GetSubsystem<UCitySimulationSubsystem>();
	Board->SimulationSubsystem = Sim;
	Board->PlacementModel = &Sim->GetPlacementModel();
	Board->BuildingCatalog = LoadObject<UCityBuildingCatalog>(
	    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
	if (!TestNotNull(TEXT("Actual building catalog"), Board->BuildingCatalog.Get()))
	{
		W->DestroyWorld(false);
		return false;
	}
	Sim->RegisterBuildingCatalog(Board->BuildingCatalog);
	Road(Sim->GetPlacementModel());
	Board->SetCommandInputBound(true);
	auto* Controller = W->SpawnActor<APlayerController>();
	Board->SetSelectedFacility(TEXT("PrimarySchool"));
	Board->UpdatePlacementPreview(Controller, {6, 9}, true, false, false, false);
	TestTrue(TEXT("Selecting a real school automatically shows proposed reach"),
	         Board->DescribeHeatMap().Contains(TEXT("proposed reach")));
	TestTrue(TEXT("Visible geometry exists"), Board->HeatMapSurface->GetNumSections() > 0);
	TestTrue(TEXT("Placement help explains the taught tier and route units"),
	         Board->GetConstructionStatus().Contains(TEXT("None to Basic")) &&
	             Board->GetConstructionStatus().Contains(TEXT("road steps")));
	Board->UpdatePlacementPreview(Controller, {6, 9}, true, false, true, false);
	TestTrue(TEXT("Cancel clears proposed geometry/legend"),
	         Board->DescribeHeatMap().IsEmpty() && Board->HeatMapSurface->GetNumSections() == 0);
	Board->UpdatePlacementPreview(Controller, {7, 9}, true, false, false, false);
	TestTrue(TEXT("Moving after cancellation resumes reach"),
	         Board->DescribeHeatMap().Contains(TEXT("proposed reach")));
	Board->UpdatePlacementPreview(Controller, {}, false, false, false, false);
	TestTrue(TEXT("Pointing away clears stale preview range"), Board->DescribeHeatMap().IsEmpty());
	Board->SetSelectedFacility(TEXT("College"));
	Board->UpdatePlacementPreview(Controller, {8, 9}, true, false, false, false);
	TestTrue(TEXT("Switching school type refreshes tier and label"),
	         Board->GetConstructionStatus().Contains(TEXT("Secondary to Higher")) &&
	             Board->GetSelectedFacilityId() == TEXT("College") &&
	             Board->DescribeHeatMap().Contains(TEXT("proposed reach")));
	Board->SetToolMode(ECityToolMode::Inspect);
	const uint64 School = Place(Sim->GetPlacementModel(), TEXT("School"), 2);
	Board->SetInspectionEntity(School);
	Board->UpdateServiceCoverage({2, 9}, true);
	TestTrue(TEXT("Inspecting a school automatically exposes its potential road reach"),
	         Board->DescribeHeatMap().Contains(TEXT("School potential reach")));
	Board->SetSelectedFacility(TEXT("Service"));
	Board->UpdateWaterPlacementOverlay();
	TestFalse(TEXT("Switching to water has no stale school legend"),
	          Board->DescribeHeatMap().Contains(TEXT("School potential reach")));
	W->DestroyWorld(false);
	return true;
}
#endif
