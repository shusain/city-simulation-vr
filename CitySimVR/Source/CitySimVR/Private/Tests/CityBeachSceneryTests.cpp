#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Presentation/CityBeachScenery.h"
#include "Presentation/CityScenery.h"
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
	const int32 BeachTriangles[] = {94, 112, 60, 92, 70, 216, 46, 60, 58, 468, 332, 208, 164, 236, 244, 124};
	const int32 BeachSections[] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 1, 2, 1, 1};
	const FVector BeachDimensions[] = {{22, 18, 4.5},
	                                   {16, 12, 11},
	                                   {30, 29, 4.5},
	                                   {120, 32, 22},
	                                   {45, 35, 45},
	                                   {85, 45, 12},
	                                   {12, 9, 3.5},
	                                   {18, 13, 7},
	                                   {24, 18, 10},
	                                   {220, 220, 220},
	                                   {67.4, 188.9045, 83.5273},
	                                   {70, 62.0375, 88.7015},
	                                   {75, 179.9671, 11.9854},
	                                   {55, 38.2, 42},
	                                   {45, 22, 47.7806},
	                                   {70, 35, 10}};
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityBeachPlacementTest, "CitySim.Presentation.BeachPlacement",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityBeachPlacementTest::RunTest(const FString&)
{
	int32 RepresentedKinds = 0;
	bool bScatteredTile = false, bMultipleGrass = false, bMultipleShells = false;
	// Ordinary rugged shores exercise natural props and unsafe-site rejection;
	// supported gentle terrain supplies real dry/flat leisure footprints.
	const int32 Seeds[] = {1, 5, 6};
	for (int32 Fixture = 0; Fixture < 6; ++Fixture)
	{
		FCityTerrainGenerationParameters P;
		P.GeneratorVersion = 2;
		P.SceneryVersion = 1;
		P.Seed = Seeds[Fixture % 3];
		if (Fixture >= 3)
		{
			P.BroadElevationAmplitudeCm = 0;
			P.HillAmplitudeCm = 0;
			P.RidgeAmplitudeCm = 0;
			P.SeaLevelCm = -160;
			P.QuantizationCm = 1;
		}
		FCityTerrainHeightfield Terrain, Reload;
		TestTrue(TEXT("Beach terrain fixture"), Terrain.Generate(P));
		TestTrue(TEXT("Reload fixture reproduces terrain"), Reload.Generate(P));
		const auto Heights = Terrain.GetQuantizedHeightsCm();
		const auto Water = Terrain.GetWaterClasses();
		const auto First = FCityBeachScenery::SelectCandidates(Terrain);
		const auto Second = FCityBeachScenery::SelectCandidates(Reload);
		TestTrue(TEXT("Suitable shores have natural props"), First.Num() > 0);
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
			const double Scale = C.Transform.GetScale3D().X;
			TestTrue(TEXT("Only pebbles have bounded natural scale variation"),
			         FCityBeachScenery::IsPebble(C.Kind) ? Scale >= .9 && Scale <= 1.1
			                                             : C.Transform.GetScale3D().Equals(FVector::OneVector, 1.e-9));
			if (FCityBeachScenery::IsRecreation(C.Kind))
			{
				TestTrue(TEXT("Household props occupy dry gentle shore"), AboveWater >= 100);
				for (int32 Other = I + 1; Other < First.Num(); ++Other)
					if (First[Other].ClusterHash == C.ClusterHash)
					{
						const auto& B = First[Other];
						const double Radius = FCityBeachScenery::ClearanceRadiusCm(C.Kind) +
						                      FCityBeachScenery::ClearanceRadiusCm(B.Kind) * B.Transform.GetScale3D().X;
						TestTrue(TEXT("Recreation arrangement has clear surrounding props"),
						         FVector::DistSquared2D(Position, B.Transform.GetLocation()) + .001 >=
						             FMath::Square(Radius));
					}
			}
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
			int32 Grass = 0, Shells = 0, Recreation = 0;
			FBox Bounds(ForceInit);
			for (const auto& C : First)
				if (C.ClusterHash == Hash)
				{
					Bounds += C.Transform.GetLocation();
					Grass += C.Kind == ECityBeachKind::DuneGrass;
					Recreation += FCityBeachScenery::IsRecreation(C.Kind);
					Shells += C.Kind == ECityBeachKind::FanShell || C.Kind == ECityBeachKind::SpiralShell;
				}
			bScatteredTile |= Bounds.GetSize().X > 300 || Bounds.GetSize().Y > 300;
			bMultipleGrass |= Grass >= 2;
			bMultipleShells |= Shells >= 2;
			TestTrue(TEXT("Every leisure arrangement remains complete"), Recreation == 0 || Recreation == 6);
		}
		TestTrue(TEXT("Cosmetics preserve terrain and water authority"),
		         Heights == Terrain.GetQuantizedHeightsCm() && Water == Terrain.GetWaterClasses());
	}
	TestEqual(TEXT("All fifteen approved kinds participate across wet fixtures"), RepresentedKinds,
	          (1 << FCityBeachScenery::KindCount) - 1);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityBeachCoverageTest, "CitySim.Presentation.BeachShorelineCoverage",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityBeachCoverageTest::RunTest(const FString&)
{
	FCityTerrainGenerationParameters P;
	P.GeneratorVersion = 2;
	P.SceneryVersion = 1;
	P.Seed = 6;
	P.BroadElevationAmplitudeCm = P.HillAmplitudeCm = P.RidgeAmplitudeCm = 0;
	P.SeaLevelCm = -160;
	P.QuantizationCm = 1;
	FCityTerrainHeightfield Terrain;
	TestTrue(TEXT("Same gentle city used for the player's beach review"), Terrain.Generate(P));
	const auto Trees = FCitySceneryField::SelectCandidates(Terrain, P.GetEntranceCell(), 420, 100);
	const auto Plain = FCityBeachScenery::SelectCandidates(Terrain);
	const auto Wooded = FCityBeachScenery::SelectCandidates(Terrain, Trees);
	TMap<FCityCellCoord, int32> Counts, Kelp;
	int32 NaturalCount = 0, PlainWood = 0, WoodedWood = 0, DriftwoodCount = 0, SmallScatterCount = 0;
	int32 OriginalSeaweedCount = 0, CurledKelpCount = 0;
	const auto NearTree = [&](const FCityCellCoord& Cell)
	{
		return Trees.ContainsByPredicate(
		    [&](const FCitySceneryCandidate& Tree)
		    {
			    return Tree.Kind != ECitySceneryKind::Rock && FMath::Abs(Tree.Cell.X - Cell.X) <= 1 &&
			           FMath::Abs(Tree.Cell.Y - Cell.Y) <= 1;
		    });
	};
	for (const auto& C : Plain)
		PlainWood += C.Kind == ECityBeachKind::Driftwood && NearTree(C.Cell);
	for (const auto& C : Wooded)
	{
		if (FCityBeachScenery::IsRecreation(C.Kind))
			continue;
		++Counts.FindOrAdd(C.Cell);
		++NaturalCount;
		OriginalSeaweedCount += C.Kind == ECityBeachKind::Seaweed;
		CurledKelpCount += C.Kind == ECityBeachKind::CurledKelp;
		DriftwoodCount += C.Kind == ECityBeachKind::Driftwood;
		SmallScatterCount += FCityBeachScenery::IsPebble(C.Kind) || C.Kind == ECityBeachKind::DuneGrass ||
		                     FCityBeachScenery::IsSeaweed(C.Kind);
		if (FCityBeachScenery::IsSeaweed(C.Kind))
			++Kelp.FindOrAdd(C.Cell);
		WoodedWood += C.Kind == ECityBeachKind::Driftwood && NearTree(C.Cell);
	}
	// An independent centre-of-tile shoreline census catches the previous
	// quarter-tile lottery and32-tile truncation, not merely a higher total.
	int32 Suitable = 0, Covered = 0;
	const auto D = Terrain.GetDimensions();
	for (int32 Y = 1; Y < D.GetHeight() - 1; ++Y)
		for (int32 X = 1; X < D.GetWidth() - 1; ++X)
		{
			FVector Normal;
			const double Ground = FCityBeachScenery::SampleGround(Terrain, X + .5, Y + .5, Normal);
			bool bShore = false;
			for (int32 DY = -1; DY <= 1; ++DY)
				for (int32 DX = -1; DX <= 1; ++DX)
				{
					const FCityCellCoord Wet(X + DX, Y + DY);
					const double Above = Ground - Terrain.GetWaterSurfaceHeightCm(Wet);
					bShore |= Terrain.IsWaterCell(Wet) && Above >= 20 && Above <= 280 && Normal.Z >= .97;
				}
			if (bShore)
			{
				++Suitable;
				Covered += Counts.Contains({X, Y});
			}
		}
	TSet<int32> Densities;
	int32 MaxKelp = 0;
	for (const auto& Pair : Counts)
		Densities.Add(Pair.Value);
	for (const auto& Pair : Kelp)
		MaxKelp = FMath::Max(MaxKelp, Pair.Value);
	TestTrue(TEXT("Coverage extends well beyond the old32 populated tiles"), Counts.Num() > 64);
	TestTrue(TEXT("Suitable neighboring shore tiles usually receive natural props"),
	         Suitable > 0 && Covered >= Suitable * .8);
	TestTrue(TEXT("Coverage is a light scatter with locally varied counts"),
	         NaturalCount > Counts.Num() * 3 && Densities.Num() >= 3);
	TestTrue(TEXT("Some shore tiles retain concentrations of kelp"), MaxKelp >= 3);
	TestTrue(TEXT("Existing seaweed and curled kelp share natural sites"),
	         OriginalSeaweedCount > 0 && CurledKelpCount > 0 &&
	             !FCityBeachScenery::IsRecreation(ECityBeachKind::CurledKelp));
	TestEqual(TEXT("Variant retains existing seaweed clearance"),
	          FCityBeachScenery::ClearanceRadiusCm(ECityBeachKind::CurledKelp),
	          FCityBeachScenery::ClearanceRadiusCm(ECityBeachKind::Seaweed));
	TestTrue(TEXT("Actual nearby tree candidates increase driftwood"), WoodedWood > PlainWood);
	TestTrue(TEXT("Driftwood is occasional; pebbles, sprouts and kelp dominate the scatter"),
	         DriftwoodCount > 0 && DriftwoodCount * 10 < NaturalCount && SmallScatterCount > NaturalCount / 2);
	UE_LOG(
	    LogTemp, Display,
	    TEXT(
	        "CITYSIM_BEACH_COVERAGE Tiles=%d Suitable=%d Covered=%d Naturals=%d Densities=%d MaxKelp=%d Wood=%d/%d Driftwood=%d SmallScatter=%d Seaweed=%d CurledKelp=%d"),
	    Counts.Num(), Suitable, Covered, NaturalCount, Densities.Num(), MaxKelp, WoodedWood, PlainWood, DriftwoodCount,
	    SmallScatterCount, OriginalSeaweedCount, CurledKelpCount);
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
	TestEqual(TEXT("Fifteen reusable instanced beach pools"), Atmosphere->BeachPools.Num(),
	          FCityBeachScenery::KindCount);
	TestTrue(TEXT("Seeded beach has props"), Atmosphere->GetBeachDecorationCount() > 0);
	TestEqual(TEXT("Every clear generated candidate is presented, without a citywide cutoff"),
	          Atmosphere->GetBeachDecorationCount(),
	          Atmosphere->BeachCandidates
	              .FilterByPredicate([&](const FCityBeachCandidate& C)
	                                 { return FCityBeachScenery::IsClear(Sim->GetPlacementModel(), C.Cell); })
	              .Num());
	TArray<FTransform> Original;
	for (int32 Kind = 0; Kind < Atmosphere->BeachPools.Num(); ++Kind)
	{
		auto* Pool = Atmosphere->BeachPools[Kind].Get();
		auto* Mesh = Pool->GetStaticMesh().Get();
		if (!TestNotNull(TEXT("Approved cooked beach source"), Mesh))
			continue;
		TestEqual(TEXT("Exact reviewed triangles"), Mesh->GetNumTriangles(0), BeachTriangles[Kind]);
		TestEqual(TEXT("Exact semantic material sections"), Mesh->GetNumSections(0), BeachSections[Kind]);
		TestTrue(TEXT("Small metric dimensions survive native import"),
		         Mesh->GetBoundingBox().GetSize().Equals(BeachDimensions[Kind], .2));
		TestTrue(TEXT("Source ground pivot"), FMath::Abs(Mesh->GetBoundingBox().Min.Z) < .2);
		TestTrue(TEXT("Reviewed UV0 retained"),
		         Mesh->GetRenderData() && Mesh->GetRenderData()->LODResources[0].GetNumTexCoords() > 0);
		if (Kind == static_cast<int32>(ECityBeachKind::Umbrella) && Mesh->GetRenderData())
		{
			const auto& Colors = Mesh->GetRenderData()->LODResources[0].VertexBuffers.ColorVertexBuffer;
			TSet<uint32> Palette;
			for (uint32 Vertex = 0; Vertex < Colors.GetNumVertices(); ++Vertex)
				Palette.Add(Colors.VertexColor(Vertex).DWColor());
			TestTrue(TEXT("The reviewed umbrella panel palette survives native import"), Palette.Num() >= 4);
		}
		for (int32 Slot = 0; Slot < BeachSections[Kind]; ++Slot)
		{
			auto* Material = Mesh->GetMaterial(Slot);
			if (TestNotNull(TEXT("Explicit approved-color instance"), Material))
			{
				TestTrue(TEXT("Native beach binding"),
				         Material->GetPathName().StartsWith(TEXT("/Game/CitySim/Art/Beach/Materials/MI_Beach")));
				TestEqual(TEXT("Opaque grass and shells"), Material->GetBlendMode(), BLEND_Opaque);
			}
		}
		TestTrue(TEXT("Cosmetic props have no collision/overlap/per-prop ticking"),
		         Pool->GetCollisionEnabled() == ECollisionEnabled::NoCollision && !Pool->GetGenerateOverlapEvents() &&
		             !Pool->PrimaryComponentTick.bCanEverTick);
		for (int32 I = 0; I < Pool->GetInstanceCount(); ++I)
		{
			FTransform Pose;
			Pool->GetInstanceTransform(I, Pose);
			Original.Add(Pose);
			const double Scale = Pose.GetScale3D().X;
			TestTrue(TEXT("Source scale or bounded pebble variation"),
			         FCityBeachScenery::IsPebble(static_cast<ECityBeachKind>(Kind))
			             ? Scale >= .9 && Scale <= 1.1
			             : Pose.GetScale3D().Equals(FVector::OneVector));
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
