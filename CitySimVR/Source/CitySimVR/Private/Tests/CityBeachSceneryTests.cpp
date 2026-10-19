#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Presentation/CityBeachScenery.h"
#include "Presentation/CityAtmosphereComponent.h"
#include "Presentation/CityBoardActor.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "StaticMeshResources.h"

namespace
{
	const int32 BeachTriangles[] = {94, 112, 60, 92, 70};
	const FVector BeachDimensions[] = {{22, 18, 4.5}, {16, 12, 11}, {30, 29, 4.5}, {120, 32, 22}, {45, 35, 45}};
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityBeachPlacementTest, "CitySim.Presentation.BeachPlacement",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityBeachPlacementTest::RunTest(const FString&)
{
	int32 RepresentedKinds = 0;
	bool bScatteredTile = false, bMultipleGrass = false, bMultipleShells = false;
	// Known wet generator2 layouts: scattered lakes/river, lowland river, lake/river.
	for (const int32 Seed : {1, 5, 6})
	{
		FCityTerrainGenerationParameters P;
		P.GeneratorVersion = 2;
		P.SceneryVersion = 1;
		P.Seed = Seed;
		FCityTerrainHeightfield Terrain, Reload;
		TestTrue(TEXT("Beach terrain fixture"), Terrain.Generate(P));
		TestTrue(TEXT("Reload fixture reproduces terrain"), Reload.Generate(P));
		const auto Heights = Terrain.GetQuantizedHeightsCm();
		const auto Water = Terrain.GetWaterClasses();
		const auto First = FCityBeachScenery::SelectCandidates(Terrain);
		const auto Second = FCityBeachScenery::SelectCandidates(Reload);
		TestTrue(TEXT("Sparse beach props exist within one fixed cap"),
		         First.Num() > 0 && First.Num() <= FCityBeachScenery::MaxProps);
		TestEqual(TEXT("Same seeded source count after regeneration"), First.Num(), Second.Num());
		TSet<uint32> Clusters;
		for (int32 I = 0; I < First.Num(); ++I)
		{
			const auto& C = First[I];
			const auto Position = C.Transform.GetLocation();
			const double X = Position.X / 800., Y = Position.Y / 800.;
			TestTrue(TEXT("Ground is above its actual neighboring water surface"), Position.Z >= C.WaterSurfaceCm + 3);
			TestTrue(TEXT("Close to low shoreline, not distant cliff tops"), Position.Z <= C.WaterSurfaceCm + 301);
			const double AboveWater = Position.Z - .5 - C.WaterSurfaceCm;
			if (C.Kind == ECityBeachKind::Starfish)
				TestTrue(TEXT("Starfish favor low wet sand"), AboveWater <= 70);
			if (C.Kind == ECityBeachKind::FanShell || C.Kind == ECityBeachKind::SpiralShell)
				TestTrue(TEXT("Shells stay on the waterward beach"), AboveWater < 140);
			if (C.Kind == ECityBeachKind::DuneGrass)
				TestTrue(TEXT("Grass grows at the inland transition"), AboveWater >= 140);
			if (C.Kind == ECityBeachKind::Driftwood)
				TestTrue(TEXT("Driftwood favors drier beach"), AboveWater > 70 && AboveWater < 260);
			TestTrue(TEXT("Prop follows ground without giant display scaling"),
			         C.Transform.GetScale3D().Equals(FVector::OneVector, 1.e-9));
			FVector Normal;
			const double Ground = FCityBeachScenery::SampleGround(Terrain, X, Y, Normal);
			TestTrue(TEXT("Ground contact matches the rendered terrain facet"),
			         FMath::Abs(Position.Z - Ground - .5) < 1.e-6);
			TestTrue(TEXT("Local up follows the rendered bank facet"),
			         C.Transform.GetUnitAxis(EAxis::Z).Equals(Normal, 1.e-6));
			if (Second.IsValidIndex(I))
				TestTrue(TEXT("Regeneration preserves exact identity/order/pose"),
				         C.Kind == Second[I].Kind && C.Cell == Second[I].Cell &&
				             C.ClusterHash == Second[I].ClusterHash && C.Transform.Equals(Second[I].Transform, 1.e-9));
			Clusters.Add(C.ClusterHash);
			RepresentedKinds |= 1 << static_cast<int32>(C.Kind);
		}
		for (const uint32 Hash : Clusters)
		{
			int32 Grass = 0, Shells = 0;
			FBox Bounds(ForceInit);
			for (const auto& C : First)
				if (C.ClusterHash == Hash)
				{
					Bounds += C.Transform.GetLocation();
					Grass += C.Kind == ECityBeachKind::DuneGrass;
					Shells += C.Kind == ECityBeachKind::FanShell || C.Kind == ECityBeachKind::SpiralShell;
				}
			bScatteredTile |= Bounds.GetSize().X > 300 || Bounds.GetSize().Y > 300;
			bMultipleGrass |= Grass >= 2;
			bMultipleShells |= Shells >= 2;
		}
		TestTrue(TEXT("Cluster budget is independent of city area"), Clusters.Num() <= FCityBeachScenery::MaxClusters);
		TestTrue(TEXT("Cosmetics preserve terrain and water authority"),
		         Heights == Terrain.GetQuantizedHeightsCm() && Water == Terrain.GetWaterClasses());
	}
	TestEqual(TEXT("All five approved kinds participate"), RepresentedKinds, 31);
	TestTrue(TEXT("Props span several metres within a tile"), bScatteredTile);
	TestTrue(TEXT("Inland tiles can contain several grass sprouts"), bMultipleGrass);
	TestTrue(TEXT("Wet sand tiles can contain several shells"), bMultipleShells);
	FCityTerrainGenerationParameters DryParameters;
	DryParameters.GeneratorVersion = 2;
	DryParameters.Seed = 7429;
	FCityTerrainHeightfield Dry;
	TestTrue(TEXT("Dry landscape fixture"), Dry.Generate(DryParameters));
	TestEqual(TEXT("Dry landscapes do not manufacture beach decorations"),
	          FCityBeachScenery::SelectCandidates(Dry).Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityBeachPresentationTest, "CitySim.Presentation.BeachAssetsAndLifecycle",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter |
                                     EAutomationTestFlags::NonNullRHI)
bool FCityBeachPresentationTest::RunTest(const FString&)
{
	const auto Init = UWorld::InitializationValues()
	                      .AllowAudioPlayback(false)
	                      .CreatePhysicsScene(false)
	                      .CreateNavigation(false)
	                      .CreateAISystem(false)
	                      .ShouldSimulatePhysics(false)
	                      .CreateFXSystem(true);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("CityBeachTest"), nullptr, true,
	                                  ERHIFeatureLevel::Num, &Init);
	auto* Sim = World->GetSubsystem<UCitySimulationSubsystem>();
	auto* Board = World->SpawnActor<ACityBoardActor>();
	auto* Atmosphere = Board->FindComponentByClass<UCityAtmosphereComponent>();
	if (!TestNotNull(TEXT("Existing shared atmosphere component"), Atmosphere))
	{
		World->DestroyWorld(false);
		return false;
	}
	Atmosphere->BeginPlay();
	Sim->SetEntryMenuSuspended(false);
	Sim->SetSimulationPaused(true);
	const auto Heights = Sim->GetTerrain().GetQuantizedHeightsCm();
	const uint64 Revision = Sim->GetPlacementModel().GetRevision();
	Atmosphere->Update(Sim, 0);
	TestEqual(TEXT("Five reusable instanced beach pools"), Atmosphere->BeachPools.Num(), 5);
	TestTrue(TEXT("Seeded beach has props at the fixed cap"),
	         Atmosphere->GetBeachDecorationCount() > 0 &&
	             Atmosphere->GetBeachDecorationCount() <= FCityBeachScenery::MaxProps);
	TArray<FTransform> Original;
	for (int32 Kind = 0; Kind < Atmosphere->BeachPools.Num(); ++Kind)
	{
		auto* Pool = Atmosphere->BeachPools[Kind].Get();
		auto* Mesh = Pool->GetStaticMesh().Get();
		if (!TestNotNull(TEXT("Approved cooked beach source"), Mesh))
			continue;
		TestEqual(TEXT("Exact reviewed triangles"), Mesh->GetNumTriangles(0), BeachTriangles[Kind]);
		TestEqual(TEXT("One opaque material section"), Mesh->GetNumSections(0), 1);
		TestTrue(TEXT("Small metric dimensions survive native import"),
		         Mesh->GetBoundingBox().GetSize().Equals(BeachDimensions[Kind], .2));
		TestTrue(TEXT("Source ground pivot"), FMath::Abs(Mesh->GetBoundingBox().Min.Z) < .2);
		TestTrue(TEXT("Reviewed UV0 retained"),
		         Mesh->GetRenderData() && Mesh->GetRenderData()->LODResources[0].GetNumTexCoords() > 0);
		auto* Material = Mesh->GetMaterial(0);
		if (TestNotNull(TEXT("Explicit approved-color instance"), Material))
		{
			TestTrue(TEXT("Native beach binding"),
			         Material->GetPathName().StartsWith(TEXT("/Game/CitySim/Art/Beach/Materials/MI_Beach")));
			TestEqual(TEXT("Opaque grass and shells"), Material->GetBlendMode(), BLEND_Opaque);
		}
		TestTrue(TEXT("Cosmetic props have no collision/overlap/per-prop ticking"),
		         Pool->GetCollisionEnabled() == ECollisionEnabled::NoCollision && !Pool->GetGenerateOverlapEvents() &&
		             !Pool->PrimaryComponentTick.bCanEverTick);
		for (int32 I = 0; I < Pool->GetInstanceCount(); ++I)
		{
			FTransform Pose;
			Pool->GetInstanceTransform(I, Pose);
			Original.Add(Pose);
			TestTrue(TEXT("Source size is never enlarged for visibility"),
			         Pose.GetScale3D().Equals(FVector::OneVector));
		}
	}
	TestTrue(TEXT("Presentation preserves city authority"), Heights == Sim->GetTerrain().GetQuantizedHeightsCm() &&
	                                                            Revision == Sim->GetPlacementModel().GetRevision());
	Sim->SetEntryMenuSuspended(true);
	Atmosphere->Update(Sim, 100);
	for (const auto& Pool : Atmosphere->BeachPools)
		TestFalse(TEXT("Entry menu hides shore props with scenery"), Pool->IsVisible());
	Sim->SetEntryMenuSuspended(false);
	Atmosphere->Update(Sim, 100);
	int32 PoseIndex = 0;
	for (const auto& Pool : Atmosphere->BeachPools)
	{
		TestTrue(TEXT("Returning restores pooled visibility"), Pool->IsVisible());
		for (int32 I = 0; I < Pool->GetInstanceCount(); ++I)
		{
			FTransform Pose;
			Pool->GetInstanceTransform(I, Pose);
			if (Original.IsValidIndex(PoseIndex))
				TestTrue(TEXT("Pause/menu does not move props"), Pose.Equals(Original[PoseIndex], 1.e-9));
			++PoseIndex;
		}
	}
	TestEqual(TEXT("No duplicated instances on menu return"), PoseIndex, Original.Num());
	bool Cleared = false;
	for (const auto& Candidate : Atmosphere->BeachCandidates)
	{
		for (int32 Dy = -1; Dy <= 1 && !Cleared; ++Dy)
			for (int32 Dx = -1; Dx <= 1 && !Cleared; ++Dx)
			{
				const FCityCellCoord Cell(Candidate.Cell.X + Dx, Candidate.Cell.Y + Dy);
				if (Sim->GetTerrain().IsCellBuildable(Cell) &&
				    FCityBeachScenery::IsClear(Sim->GetPlacementModel(), Candidate.Cell) &&
				    Sim->GetPlacementModel().SetZone(Cell, ECityZoneType::Residential))
				{
					TestFalse(TEXT("Development clears its bank and full-cell margin"),
					          FCityBeachScenery::IsClear(Sim->GetPlacementModel(), Candidate.Cell));
					Cleared = true;
				}
			}
		if (Cleared)
			break;
	}
	TestTrue(TEXT("Real adjacent zoning clearance fixture"), Cleared);
	Atmosphere->Update(Sim, 0);
	TestTrue(TEXT("Development actually removes props without replacement clusters"),
	         Atmosphere->GetBeachDecorationCount() < Original.Num());
	for (const auto& Pool : Atmosphere->BeachPools)
		for (int32 I = 0; I < Pool->GetInstanceCount(); ++I)
		{
			FTransform Pose;
			Pool->GetInstanceTransform(I, Pose);
			TestTrue(TEXT("Remaining props are original candidates"),
			         Original.ContainsByPredicate([&](const FTransform& P) { return P.Equals(Pose, 1.e-9); }));
		}
	World->DestroyWorld(false);
	return true;
}
#endif
