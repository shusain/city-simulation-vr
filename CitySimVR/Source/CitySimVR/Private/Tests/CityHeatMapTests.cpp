#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Presentation/CityBoardActor.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "ProceduralMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/World.h"
#include "Materials/Material.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityHeatMapTest, "CitySim.Presentation.ContinuousHeatMaps",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityHeatMapTest::RunTest(const FString& Parameters)
{
	FCityPlacementModel City(10000);
	auto Road = City.BeginRoadPreview(FCityPlacementModel::EntranceCell);
	City.UpdateRoadPreviewEnd(Road, {20, 8});
	TestTrue(TEXT("heat map road commits"), City.CommitRoad(Road).IsSuccess());
	FCityEconomyTuning Tuning;
	auto Services = FCityServiceEvaluator::BuildSnapshot(City, Tuning);
	FCityNeedsSnapshot Needs;
	FCityEnvironmentSnapshot Environment;
	Environment.Pollution[65] = 100;
	Environment.Pollution[66] = 500;
	FCityDiagnosticOverlayResult Result;
	FCityDiagnosticOverlayEvaluator::Build(City, Services, Needs, Tuning, nullptr, nullptr, nullptr, Environment,
	                                       ECityDiagnosticOverlay::Pollution, 0, Result);
	TestEqual(TEXT("source exposure is exact"), Result.Values[66], 500);
	TestTrue(TEXT("point explanation retains unclamped value"), Result.DescribeCell({2, 1}).Contains(TEXT("500")));
	TestTrue(TEXT("visual intensity clamps"), Result.ColorAt(66).R <= 1.0f);
	TestTrue(TEXT("higher exposure is brighter"),
	         Result.ColorAt(66).GetLuminance() > Result.ColorAt(65).GetLuminance());
	TestTrue(TEXT("invalid cell is safe"), Result.DescribeCell({-1, 100}).Contains(TEXT("Point")));
	FCityRoadNetworkSnapshot Network;
	FCityRoadNetworkBuilder::Build(City, FCityRoadDefinition::Local(), Network);
	FCityTrafficSnapshot Traffic;
	FCityTrafficLaneLoad Motor;
	Motor.EdgeId = 0;
	Motor.CongestionPermille = 625;
	FCityTrafficLaneLoad Walker = Motor;
	Walker.bPedestrian = true;
	Walker.CongestionPermille = 1000;
	Traffic.LaneLoads = {Motor, Walker};
	FCityDiagnosticOverlayEvaluator::Build(City, Services, Needs, Tuning, nullptr, nullptr, nullptr, Environment,
	                                       ECityDiagnosticOverlay::Traffic, 0, Result, &Traffic, &Network);
	TestEqual(TEXT("traffic uses motor lane source value, not walkers"), Result.Values[8 * FCityGrid::Width + 4], 625);
	TestEqual(TEXT("traffic off-road has no data"), Result.Status[0], uint8(0));
	for (const auto Overlay :
	     {ECityDiagnosticOverlay::Service, ECityDiagnosticOverlay::Power, ECityDiagnosticOverlay::ResidentialDemand,
	      ECityDiagnosticOverlay::CommercialDemand, ECityDiagnosticOverlay::IndustrialDemand})
	{
		FCityDiagnosticOverlayEvaluator::Build(City, Services, Needs, Tuning, nullptr, nullptr, nullptr, Environment,
		                                       Overlay, 0, Result);
		TestEqual(TEXT("all views have bounded cell data"), Result.Values.Num(), FCityGrid::CellCount);
		TestFalse(TEXT("all views explain their legend"), Result.Legend.IsEmpty());
	}
	FCityBuildingDefinition Home;
	Home.DefinitionId = TEXT("Home");
	Home.DisplayName = TEXT("Home");
	Home.Footprint = FCityCellFootprint(1, 1);
	Home.Cost = 0;
	auto Preview = City.BeginPreview(Home, {5, 9});
	const auto Placed = City.CommitPlacement(Preview, Home);
	TestTrue(TEXT("pressure fixture building placed"), Placed.IsSuccess());
	FCityDevelopmentSnapshot Development;
	Development.LatestPressures.FindOrAdd(Placed.EntityId).Total = 137;
	FCityDiagnosticOverlayEvaluator::Build(City, Services, Needs, Tuning, &Home, nullptr, nullptr, Environment,
	                                       ECityDiagnosticOverlay::ResidentialDemand, 0, Result, nullptr, nullptr,
	                                       &Development);
	TestEqual(TEXT("building pressure is exact even above visual range"), Result.Values[9 * FCityGrid::Width + 5], 137);
	TestTrue(TEXT("pointed building labels authoritative pressure"),
	         Result.DescribeCell({5, 9}).Contains(TEXT("upgrade pressure: 137")));
	const auto Init = UWorld::InitializationValues()
	                      .AllowAudioPlayback(false)
	                      .CreatePhysicsScene(false)
	                      .CreateNavigation(false)
	                      .CreateAISystem(false)
	                      .ShouldSimulatePhysics(false);
	UWorld* World =
	    UWorld::CreateWorld(EWorldType::Game, false, TEXT("HeatMapTest"), nullptr, true, ERHIFeatureLevel::Num, &Init);
	auto* Board = World->SpawnActor<ACityBoardActor>();
	auto* Simulation = World->GetSubsystem<UCitySimulationSubsystem>();
	Board->SimulationSubsystem = Simulation;
	Board->PlacementModel = Simulation->PlacementModel.Get();
	Board->RoadInstances->SetVisibility(false); // Respect a layer that was already hidden.
	Board->SetSelectedZone(ECityZoneType::Residential);
	Board->SetDiagnosticOverlay(ECityDiagnosticOverlay::Pollution);
	Board->UpdateServiceCoverage({1, 1}, true);
	TestTrue(TEXT("heat surface generated"), Board->HeatMapSurface->GetNumSections() == 1);
	const auto* Section = Board->HeatMapSurface->GetProcMeshSection(0);
	if (TestNotNull(TEXT("mesh section exists"), Section))
	{
		TestEqual(TEXT("mesh vertex budget is bounded"), Section->ProcVertexBuffer.Num(), FCityGrid::CellCount * 5);
		TestEqual(TEXT("neighbor cells share interpolated corner color"), Section->ProcVertexBuffer[1].Color,
		          Section->ProcVertexBuffer[5].Color);
		TestEqual(TEXT("neighbor cells share height"), Section->ProcVertexBuffer[1].Position.Z,
		          Section->ProcVertexBuffer[5].Position.Z);
	}
	TestFalse(TEXT("zones suppressed"), Board->ResidentialZoneInstances->IsVisible());
	TestFalse(TEXT("road surface suppressed"), Board->RoadSurfaceProfile->IsVisible());
	TestFalse(TEXT("sidewalks suppressed"), Board->SidewalkSurface->IsVisible());
	TestFalse(TEXT("curbs and edge paint suppressed"), Board->RoadDetailSurface->IsVisible());
	TestFalse(TEXT("construction cables suppressed"), Board->LiveConstructionCraneCables->IsVisible());
	TestTrue(TEXT("buildings faded"), Board->bBuildingsFaded);
	const int32 Rebuilds = Board->HeatMapRebuildCount;
	Board->UpdateServiceCoverage({2, 2}, true);
	TestEqual(TEXT("point movement does not rebuild geometry"), Board->HeatMapRebuildCount, Rebuilds);
	Simulation->Economy.SetPaused(false);
	Simulation->Tick(3.0f);
	Board->UpdateServiceCoverage({2, 2}, true);
	TestEqual(TEXT("unchanged field across simulation steps reuses mesh"), Board->HeatMapRebuildCount, Rebuilds);
	Board->SetHoveredCell({2, 2}, true);
	TestTrue(TEXT("legend includes pointed cell"), Board->DescribeHeatMap().Contains(TEXT("(2,2)")));
	Board->SetDiagnosticOverlay(ECityDiagnosticOverlay::Traffic);
	Board->UpdateServiceCoverage({2, 2}, true);
	TestFalse(TEXT("view switch keeps zones suppressed"), Board->ResidentialZoneInstances->IsVisible());
	Board->SetDiagnosticOverlay(ECityDiagnosticOverlay::None);
	Board->UpdateServiceCoverage({2, 2}, true);
	TestTrue(TEXT("zone visibility restored"), Board->ResidentialZoneInstances->IsVisible());
	TestFalse(TEXT("previously hidden road stays hidden"), Board->RoadInstances->IsVisible());
	TestTrue(TEXT("road profile restored"), Board->RoadSurfaceProfile->IsVisible());
	TestTrue(TEXT("curbs and edge paint restored"), Board->RoadDetailSurface->IsVisible());
	TestTrue(TEXT("construction cables restored"), Board->LiveConstructionCraneCables->IsVisible());
	TestFalse(TEXT("building fade restored"), Board->bBuildingsFaded);
	TestTrue(TEXT("clear removes heat geometry and legend"),
	         Board->HeatMapSurface->GetNumSections() == 0 && Board->DescribeHeatMap().IsEmpty());
	auto* Material = LoadObject<UMaterial>(nullptr, TEXT("/Game/CitySim/UI/M_CityHeatMap.M_CityHeatMap"));
	if (TestNotNull(TEXT("heat material loads"), Material))
		TestEqual(TEXT("heat intensity independent of lighting"), Material->GetShadingModels(),
		          FMaterialShadingModelField(MSM_Unlit));
	World->DestroyWorld(false);
	return true;
}
#endif
