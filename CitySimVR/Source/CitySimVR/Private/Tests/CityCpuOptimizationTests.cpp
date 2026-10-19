#if WITH_DEV_AUTOMATION_TESTS

#include "Simulation/CitySimulationSubsystem.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Presentation/CityBoardActor.h"
#include "ProceduralMeshComponent.h"
#include "Misc/CommandLine.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityDerivedInvalidationTest, "CitySim.Performance.DerivedInvalidation",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityDerivedInvalidationTest::RunTest(const FString& Parameters)
{
	const auto Values = UWorld::InitializationValues()
	                        .AllowAudioPlayback(false)
	                        .CreatePhysicsScene(false)
	                        .CreateNavigation(false)
	                        .CreateAISystem(false)
	                        .ShouldSimulatePhysics(false);
	auto* World =
	    UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	auto* Sim = World->GetSubsystem<UCitySimulationSubsystem>();
	auto* Catalog = LoadObject<UCityBuildingCatalog>(
	    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
	Sim->RegisterBuildingCatalog(Catalog);
	Sim->BenchmarkTiming = MakeUnique<FCityBenchmarkSimulationTiming>();
	Sim->Tick(0);
	Sim->Tick(0);
	TestEqual(TEXT("Unchanged paused frames reuse derived state"), Sim->BenchmarkTiming->Derived.Count, uint64(0));
	auto& City = Sim->GetPlacementModel();
	auto Road = City.BeginRoadPreview({0, 8});
	Road.End = {12, 8};
	TestTrue(TEXT("Paused road edit commits"), City.CommitRoad(Road).IsSuccess());
	Sim->Tick(0);
	TestEqual(TEXT("Paused edit refreshes graph immediately"), Sim->RoadNetworkSnapshot.SourceRevision,
	          City.GetRevision());
	TestEqual(TEXT("Paused edit refreshes once"), Sim->BenchmarkTiming->Derived.Count, uint64(1));
	Sim->Tick(0);
	TestEqual(TEXT("Next paused frame reuses edit"), Sim->BenchmarkTiming->Derived.Count, uint64(1));
	TestTrue(TEXT("Tax changes succeed while paused"), Sim->SetTaxRates(2000, 1500, 1000));
	TestEqual(TEXT("Tax action refreshes immediately"), Sim->BenchmarkTiming->Derived.Count, uint64(2));
	++Sim->PresentationGeneration;
	Sim->Tick(0);
	TestEqual(TEXT("Out-of-step presentation/state invalidation refreshes"), Sim->BenchmarkTiming->Derived.Count,
	          uint64(3));
	const uint64 Before = Sim->BenchmarkTiming->Derived.Count;
	Sim->SetSimulationPaused(false);
	const float HalfStep = Sim->EconomyTuning.FixedStepSeconds * .51f;
	Sim->Tick(HalfStep);
	TestEqual(TEXT("Empty half-day changes do not rebuild services"), Sim->BenchmarkTiming->Derived.Count, Before);
	Sim->Tick(HalfStep);
	TestTrue(TEXT("Fixed step always refreshes authority"), Sim->BenchmarkTiming->Derived.Count > Before);
	City.SetTerrain(nullptr);
	const auto* PoliceDefinition = Catalog->FindDefinition(TEXT("Police"));
	auto PolicePreview = City.BeginPreview(*PoliceDefinition, {2, 9});
	const uint64 Police = City.CommitPlacement(PolicePreview, *PoliceDefinition).EntityId;
	TestTrue(TEXT("Station placement commits"), Police != 0);
	Sim->Tick(0);
	TestFalse(TEXT("New station is initially under construction"),
	          Sim->Development.GetSnapshot().IsOperational(Police));
	const uint64 BeforeStation = Sim->BenchmarkTiming->Derived.Count;
	const int64 BeforeStationStep = Sim->Economy.GetSnapshot().FixedStepId;
	Sim->Tick(HalfStep);
	TestEqual(TEXT("Station deadline is reached between full steps"), Sim->Economy.GetSnapshot().FixedStepId,
	          BeforeStationStep);
	TestTrue(TEXT("Half-day station completion is immediate"), Sim->Development.GetSnapshot().IsOperational(Police));
	TestEqual(TEXT("Real station completion invalidates services once"), Sim->BenchmarkTiming->Derived.Count,
	          BeforeStation + 1);
	Sim->ResetCity(false);
	Sim->Tick(0);
	TestTrue(TEXT("Reset discards cached roads even if revision repeats"), Sim->RoadNetworkSnapshot.Edges.IsEmpty());
	const FString OriginalCommandLine = FCommandLine::Get();
	FCommandLine::Set(
	    *(OriginalCommandLine +
	      TEXT(" -CityBenchmarkUncachedRouting -CityBenchmarkReferenceServices -CityBenchmarkReferencePopulation")));
	Sim->BenchmarkResult.Config.RequestedLots = 1;
	Sim->BenchmarkResult.Config.bPopulated = true;
	Sim->BenchmarkResult.Config.bReferenceServiceReuse = true;
	Sim->BenchmarkResult.Config.bReferenceCare = true;
	const uint64 StableRevision = Sim->RoadNetworkSnapshot.SourceRevision;
	Sim->RefreshRoadNetwork();
	TestEqual(TEXT("Activating comparison needs no geometry change"), Sim->RoadNetworkSnapshot.SourceRevision,
	          StableRevision);
	TestFalse(TEXT("Comparison binds even when snapshot predates benchmark activation"),
	          Sim->RoadNetworkSnapshot.bUsePreparedRouting);
	Sim->RefreshDerivedSnapshots();
	TestFalse(TEXT("Service comparison uses current evaluation state"), Sim->ServiceSnapshot.Water.bPrepareQueries);
	TestFalse(TEXT("Generated benchmark can select reference population queries"), Sim->UsePreparedPopulationQueries());
	TestFalse(TEXT("Generated benchmark can disable adjacent service reuse"), Sim->UseInitialServiceReuse());
	TestFalse(TEXT("Generated benchmark can select reference care queries"), Sim->UsePreparedCareQueries());
	Sim->ResetBenchmarkTimings();
	TestTrue(TEXT("Capture reset rebinds water observer"),
	         Sim->ServiceSnapshot.Water.EvaluationTiming == &Sim->BenchmarkTiming->ServiceEvaluation);
	Sim->BenchmarkResult.Config.bPopulated = false;
	Sim->RefreshRoadNetwork();
	TestTrue(TEXT("Saved-city profiles retain prepared routing"), Sim->RoadNetworkSnapshot.bUsePreparedRouting);
	TestTrue(TEXT("Saved-city profiles retain prepared service queries"), Sim->UsePreparedServiceQueries());
	TestTrue(TEXT("Saved-city profiles retain prepared population queries"), Sim->UsePreparedPopulationQueries());
	TestTrue(TEXT("Saved-city profiles retain adjacent service reuse"), Sim->UseInitialServiceReuse());
	TestTrue(TEXT("Saved-city profiles retain prepared care queries"), Sim->UsePreparedCareQueries());
	Sim->BenchmarkResult = FCityBenchmarkResult();
	Sim->RefreshRoadNetwork();
	TestTrue(TEXT("A comparison flag cannot disable normal gameplay routing"),
	         Sim->RoadNetworkSnapshot.bUsePreparedRouting);
	TestTrue(TEXT("A comparison flag cannot disable normal gameplay service queries"),
	         Sim->UsePreparedServiceQueries());
	TestTrue(TEXT("A comparison flag cannot disable normal gameplay population queries"),
	         Sim->UsePreparedPopulationQueries());
	TestTrue(TEXT("A comparison flag cannot disable normal gameplay service reuse"), Sim->UseInitialServiceReuse());
	TestTrue(TEXT("A comparison flag cannot disable normal gameplay care queries"), Sim->UsePreparedCareQueries());
	FCommandLine::Set(*OriginalCommandLine);
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityHeapRouteTest, "CitySim.Roads.WeightedHeapRouteDeterminism",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityHeapRouteTest::RunTest(const FString& Parameters)
{
	FCityRoadNetworkSnapshot Network;
	auto Edge = [&](FCityCellCoord A, FCityCellCoord B, int32 Speed, ECityRoadAccess Access)
	{
		FCityRoadEdge E;
		E.Cells = {A, B};
		E.SpeedUnitsPerStep = Speed;
		E.Access = Access;
		Network.Edges.Add(E);
	};
	const auto All = ECityRoadAccess::Car | ECityRoadAccess::Pedestrian | ECityRoadAccess::Freight;
	// Two equal-cost routes: the previous row-major predecessor tie must survive.
	Edge({1, 1}, {2, 1}, 4, All);
	Edge({2, 1}, {2, 2}, 4, All);
	Edge({1, 1}, {1, 2}, 4, All);
	Edge({1, 2}, {2, 2}, 4, All);
	TArray<FCityCellCoord> Route;
	for (int32 Repeat = 0; Repeat < 10; ++Repeat)
	{
		TestTrue(TEXT("Equal-cost route exists"), Network.FindRoute({1, 1}, {2, 2}, ECityRoadAccess::Car, Route));
		TestTrue(TEXT("Row-major tie remains stable"), Route == TArray<FCityCellCoord>{{1, 1}, {2, 1}, {2, 2}});
	}
	// A faster raised detour must beat fewer slow cells, without leaking to pedestrians.
	Edge({1, 1}, {1, 1, 1}, 20, ECityRoadAccess::Car);
	Edge({1, 1, 1}, {2, 2, 1}, 20, ECityRoadAccess::Car);
	Edge({2, 2, 1}, {2, 2}, 20, ECityRoadAccess::Car);
	TestTrue(TEXT("Weighted car route exists"), Network.FindRoute({1, 1}, {2, 2}, ECityRoadAccess::Car, Route));
	TestTrue(TEXT("Weighted search chooses faster raised route"),
	         Route == TArray<FCityCellCoord>{{1, 1}, {1, 1, 1}, {2, 2, 1}, {2, 2}});
	TestTrue(TEXT("Pedestrian route exists"), Network.FindRoute({1, 1}, {2, 2}, ECityRoadAccess::Pedestrian, Route));
	TestTrue(TEXT("Pedestrians retain ground path"), Route == TArray<FCityCellCoord>{{1, 1}, {2, 1}, {2, 2}});
	TestFalse(TEXT("Disconnected target fails and clears output"), Network.FindRoute({1, 1}, {9, 9}, All, Route));
	TestTrue(TEXT("Failed path has no stale cells"), Route.IsEmpty());
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityTerrainPresentationCacheTest, "CitySim.Presentation.TerrainMeshInvalidation",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityTerrainPresentationCacheTest::RunTest(const FString& Parameters)
{
	const auto Values = UWorld::InitializationValues()
	                        .AllowAudioPlayback(false)
	                        .CreatePhysicsScene(false)
	                        .CreateNavigation(false)
	                        .CreateAISystem(false)
	                        .ShouldSimulatePhysics(false);
	auto* World =
	    UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	auto* Sim = World->GetSubsystem<UCitySimulationSubsystem>();
	auto* Board = World->SpawnActor<ACityBoardActor>();
	Board->BuildingCatalog = LoadObject<UCityBuildingCatalog>(
	    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
	Board->DispatchBeginPlay();
	auto* Section = Board->BoardSurface->GetProcMeshSection(0);
	if (!TestNotNull(TEXT("Terrain mesh exists"), Section))
	{
		World->DestroyWorld(false);
		return false;
	}
	// A sentinel in the actual buffer proves a building-only rebuild leaves
	// the existing mesh intact; a real terrain/session change must replace it.
	Section->ProcVertexBuffer[0].Color = FColor::Magenta;
	Board->RebuildAllPresentation();
	TestEqual(TEXT("Building state rebuild reuses terrain buffer"),
	          Board->BoardSurface->GetProcMeshSection(0)->ProcVertexBuffer[0].Color, FColor::Magenta);
	auto TerrainParameters = Sim->Terrain.GetParameters();
	++TerrainParameters.Seed;
	FString Error;
	TestTrue(TEXT("New terrain generates"), Sim->Terrain.Generate(TerrainParameters, &Error));
	Board->RebuildAllPresentation();
	TestTrue(TEXT("New terrain replaces cached geometry"),
	         Board->BoardSurface->GetProcMeshSection(0)->ProcVertexBuffer[0].Color != FColor::Magenta);
	Board->BoardSurface->GetProcMeshSection(0)->ProcVertexBuffer[0].Color = FColor::Magenta;
	++Sim->CitySessionGeneration;
	Board->RebuildAllPresentation();
	TestTrue(TEXT("City switch rebuilds even with identical terrain parameters"),
	         Board->BoardSurface->GetProcMeshSection(0)->ProcVertexBuffer[0].Color != FColor::Magenta);
	World->DestroyWorld(false);
	return true;
}
#endif
