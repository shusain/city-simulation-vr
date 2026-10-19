#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Presentation/CityBoardActor.h"
#include "Presentation/CitySeasonalDecorations.h"
#include "Presentation/CityBuildingSupports.h"
#include "Presentation/CityHomeWalk.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Simulation/CityDevelopmentSimulation.h"
#include "Simulation/CityGrowthSimulation.h"
#include "Interaction/CityBoardPicking.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "ProceduralMeshComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityBuildingFoundationTest,
                                 "CitySim.Terrain.ResidentialFoundationFineSamplesAndEntrances",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityBuildingFoundationTest::RunTest(const FString& Parameters)
{
	const auto Init = UWorld::InitializationValues()
	                      .AllowAudioPlayback(false)
	                      .CreatePhysicsScene(false)
	                      .CreateNavigation(false)
	                      .CreateAISystem(false)
	                      .ShouldSimulatePhysics(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("BuildingFoundationTest"), nullptr, true,
	                                  ERHIFeatureLevel::Num, &Init);
	auto* Simulation = World->GetSubsystem<UCitySimulationSubsystem>();
	auto* Board = World->SpawnActor<ACityBoardActor>();
	auto* Catalog = LoadObject<UCityBuildingCatalog>(
	    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
	Board->SimulationSubsystem = Simulation;
	Board->BuildingCatalog = Catalog;
	const auto& Terrain = Simulation->GetTerrain();
	const auto OriginalHeights = Terrain.GetQuantizedHeightsCm();
	bool bFinished = false;
	do
	{
		if (!TestNotNull(TEXT("native catalog supplies real residential geometry"), Catalog))
			break;
		const auto* AuthoredHome = Catalog->FindDefinition(TEXT("Home"));
		if (!TestNotNull(TEXT("native Home definition exists"), AuthoredHome))
			break;
		if (!TestNotNull(TEXT("reviewed smaller house mesh exists"), Board->SeasonalResidentialMesh.Get()))
			break;
		if (!TestNotNull(TEXT("residential concrete support mesh exists"),
		                 Board->ResidentialFoundationInstances->GetStaticMesh().Get()))
			break;
		TestEqual(TEXT("residential support uses concrete without an artificial lawn cap"),
		          Board->ResidentialFoundationInstances->GetStaticMesh()->GetFName(),
		          FName(TEXT("Cube")));
		const FBox PadBounds = Board->ResidentialFoundationInstances->GetStaticMesh()->GetBoundingBox();
		TestTrue(TEXT("support mesh has a centered100cm pivot contract"),
		         PadBounds.Min.Equals(FVector(-50), .01) && PadBounds.Max.Equals(FVector(50), .01));
		FCityBuildingDefinition Home = *AuthoredHome;
		// This isolated presentation fixture renders completed homes without
		// waiting for construction or changing the simulation's actual city.
		Home.ConstructionMesh.Reset();
		Home.ConstructionSteps = 0;
		Home.PresentationMesh = Board->SeasonalResidentialMesh.Get();
		// Match the same completed-home definition when entrance rebuilding
		// resolves this fixture through the catalog, rather than construction.
		Catalog = DuplicateObject<UCityBuildingCatalog>(Catalog, World);
		*Catalog->Definitions.FindByPredicate([](const auto& D) { return D.DefinitionId == TEXT("Home"); }) = Home;
		Board->BuildingCatalog = Catalog;
		const FIntPoint Size = Home.Footprint.GetRotatedSize(0);
		for (const bool bPeakFixture : {true, false})
		{
			TUniquePtr<FCityPlacementModel> City;
			FCityCellCoord Anchor;
			int32 Peak = MIN_int32, Trough = MAX_int32;
			// Select real buildable land where the full rendered grid has an
			// extremum that the old integer-corner calculation would miss.
			for (int32 Y = 12; Y < FCityGrid::Height - Size.Y - 1 && !City; ++Y)
				for (int32 X = 12; X < FCityGrid::Width - Size.X - 1 && !City; ++X)
				{
					int32 FineMax = MIN_int32, FineMin = MAX_int32, CoarseMax = MIN_int32,
					      CoarseMin = MAX_int32;
					for (int32 V = Y * Terrain.SamplesPerCell; V <= (Y + Size.Y) * Terrain.SamplesPerCell;
					     ++V)
						for (int32 U = X * Terrain.SamplesPerCell; U <= (X + Size.X) * Terrain.SamplesPerCell;
						     ++U)
						{
							const int32 Height = Terrain.GetSampleHeightCm(U, V);
							FineMax = FMath::Max(FineMax, Height);
							FineMin = FMath::Min(FineMin, Height);
							if (U % Terrain.SamplesPerCell == 0 && V % Terrain.SamplesPerCell == 0)
							{
								CoarseMax = FMath::Max(CoarseMax, Height);
								CoarseMin = FMath::Min(CoarseMin, Height);
							}
						}
					if ((bPeakFixture ? FineMax <= CoarseMax : FineMin >= CoarseMin) ||
					    FineMax - FineMin < 80 ||
					    FineMax - FineMin > FCityPlacementModel::MaximumFoundationReliefCm)
						continue;
					auto Candidate = MakeUnique<FCityPlacementModel>(1000000);
					Candidate->SetTerrain(&Terrain);
					if (!Candidate->ValidatePlacement(Candidate->BeginPreview(Home, {X, Y}), Home)
					         .IsSuccess())
						continue;
					const TArray<FCityCellCoord> Corners = {
					    {X - 1, Y - 1}, {X + Size.X, Y - 1}, {X + Size.X, Y + Size.Y}, {X - 1, Y + Size.Y}};
					bool bRoads = true;
					auto Connect = Candidate->BeginRoadPreview(Candidate->GetEntranceCell());
					Candidate->UpdateRoadPreviewEnd(Connect, Corners[0]);
					bRoads &= Candidate->CommitRoad(Connect).IsSuccess();
					for (int32 Side = 0; Side < 4 && bRoads; ++Side)
					{
						auto Road = Candidate->BeginRoadPreview(Corners[Side]);
						Candidate->UpdateRoadPreviewEnd(Road, Corners[(Side + 1) % 4]);
						bRoads &= Candidate->CommitRoad(Road).IsSuccess();
					}
					if (!bRoads || !Candidate->IsRoadConnected(Corners[0]))
						continue;
					Anchor = {X, Y};
					Peak = FineMax;
					Trough = FineMin;
					City = MoveTemp(Candidate);
				}
			if (!TestTrue(bPeakFixture
			                  ? TEXT("connected nonflat Home fixture contains a missed fine-sample peak")
			                  : TEXT("connected nonflat Home fixture contains a missed fine-sample trough"),
			              City.IsValid()))
				break;
			Board->PlacementModel = City.Get();
			AddInfo(FString::Printf(TEXT("%s fixture (%d,%d), rendered range%d..%dcm"),
			                        bPeakFixture ? TEXT("Peak") : TEXT("Trough"), Anchor.X, Anchor.Y, Trough,
			                        Peak));
			for (int32 Turns = 0; Turns < 4; ++Turns)
			{
				auto Preview = City->BeginPreview(Home, Anchor, Turns);
				const auto Placement = City->CommitPlacement(Preview, Home);
				if (!TestTrue(TEXT("rotated nonflat Home commits through real placement validation"),
				              Placement.IsSuccess()))
					continue;
				const auto Entity = City->GetEntities().FindChecked(Placement.EntityId);
				FVector Door, Road;
				TestTrue(TEXT("rotated Home has a connected road entrance"),
				         Board->FindEntranceConnection(Anchor, Home, Turns, Door, Road));
				const FVector Center((Anchor.X + Size.X * .5) * CityBoardPicking::LogicalCellSizeCm,
				                     (Anchor.Y + Size.Y * .5) * CityBoardPicking::LogicalCellSizeCm, 0);
				TestTrue(TEXT("Home ramp meets the centered portal rather than the first road-cell offset"),
				         FMath::IsNearlyEqual(Door.X, Center.X, .001) ||
				             FMath::IsNearlyEqual(Door.Y, Center.Y, .001));
				TestEqual(TEXT("all homes use the highest rendered sample as their base"),
				          Board->GetBuildingBaseHeight(Anchor, Home, Turns), static_cast<double>(Peak));
				const FTransform MeshPose = Board->MakeBuildingTransform(Anchor, Home, Turns);
				TestEqual(TEXT("completed mesh clears terrain with the existing40cm pivot offset"),
				          MeshPose.GetLocation().Z, Peak + 40.0);
				Board->RenderPlacement(Placement.EntityId, Entity, Home);
				const auto Supports = CityBuildingSupports::Find(Home.PresentationMesh.ToSoftObjectPath().GetLongPackageName());
				TestTrue(TEXT("Home has measured structural supports"), !Supports.IsEmpty());
				TestEqual(TEXT("one instance per measured Home structural region"), Board->ResidentialFoundationInstances->GetInstanceCount(), Supports.Num());
				double SupportArea = 0;
				for (int32 I = 0; I < Supports.Num(); ++I)
				{
					FTransform Pad; Board->ResidentialFoundationInstances->GetInstanceTransform(I, Pad, false);
					const auto& R = Supports[I];
					SupportArea += Pad.GetScale3D().X * Pad.GetScale3D().Y * 10000;
					const double Top = Pad.GetLocation().Z + Pad.GetScale3D().Z * 50;
					const double Bottom = Pad.GetLocation().Z - Pad.GetScale3D().Z * 50;
					TestTrue(TEXT("foundation meets the actual structural floor, without the old20cm gap"),
					         FMath::IsNearlyEqual(Top, MeshPose.GetLocation().Z + R.BottomZ + 1, .001));
					const FVector LocalCenter = MeshPose.InverseTransformPosition(Pad.GetLocation());
					TestTrue(TEXT("rotated support follows the authored body center"),
					         FMath::IsNearlyEqual(LocalCenter.X, (R.MinX + R.MaxX) * .5, .001) &&
					         FMath::IsNearlyEqual(LocalCenter.Y, (R.MinY + R.MaxY) * .5, .001));
					TestTrue(TEXT("support rotation matches building presentation orientation"), Pad.GetRotation().Equals(MeshPose.GetRotation()));
					TestTrue(TEXT("body-only concrete excludes outdoor yard and entrance walk"),
					         R.MinX >= -500 && R.MaxX <= 500 && R.MinY >= -500 && R.MaxY <= 500);
					for (int32 V = Anchor.Y * Terrain.SamplesPerCell; V <= (Anchor.Y + Size.Y) * Terrain.SamplesPerCell; ++V)
						for (int32 U = Anchor.X * Terrain.SamplesPerCell; U <= (Anchor.X + Size.X) * Terrain.SamplesPerCell; ++U)
						{
							const FVector P(U * 800.0 / Terrain.SamplesPerCell, V * 800.0 / Terrain.SamplesPerCell, 0);
							const FVector L = MeshPose.InverseTransformPosition(P);
							if (L.X >= R.MinX && L.X <= R.MaxX && L.Y >= R.MinY && L.Y <= R.MaxY)
								TestTrue(TEXT("support reaches below every covered fine terrain sample"), Bottom <= Terrain.GetSampleHeightCm(U, V) - 20.0 + .001);
						}
				}
				TestTrue(TEXT("support no longer fills the16m square reserved plot"), SupportArea < Size.X * Size.Y * 800.0 * 800.0 * .5);
				Board->PlacementPreview = City->BeginPreview(Home, Anchor, Turns);
				Board->UpdateGhost(Home, FCityPlacementResult());
				TestTrue(TEXT("placement ghost and completed mesh share the same slope clearance"),
				         Board->PlacementGhost->GetRelativeTransform().Equals(MeshPose));
				Board->RebuildBuildingEntranceProfiles();
				const auto* Ramp = Board->BuildingEntranceProfiles->GetProcMeshSection(1);
				if (TestNotNull(TEXT("sloping driveway geometry is generated"), Ramp))
				{
					TestTrue(TEXT("pilot walk begins at the real door floor"),
					         FMath::IsNearlyEqual(Ramp->ProcVertexBuffer[0].Position.Z, Peak + 40.0, .001));
					for (const auto& Vertex : Ramp->ProcVertexBuffer)
						if (Vertex.Normal.Z > .5)
						TestTrue(TEXT("driveway vertices clear actual terrain rather than passing through "
						              "uphill ground"),
						         Vertex.Position.Z > FCityHomeWalk::GroundHeight(Terrain, Vertex.Position));
				}
				FCityDevelopmentSimulation Development;
				Development.InitializeExistingAsOperational(*City, Catalog, 0);
				FCityServiceSnapshot Services;
				Services.EntityFailures.Add(Placement.EntityId, ECityOperationFailure::None);
				FCityEnvironmentSnapshot Autumn;
				Autumn.Season = ECitySeason::Autumn;
				TArray<FCitySeasonalDecorationSample> Accents;
				for (int32 Month = 1; Month <= 12 && Accents.IsEmpty(); ++Month)
					FCitySeasonalDecorations::Build(*City, Terrain, Services, Development.GetSnapshot(),
					                                Catalog, Autumn, Month, Accents);
				TestTrue(TEXT("nonflat Home supports reviewed seasonal accents"), !Accents.IsEmpty());
				for (const auto& Accent : Accents)
				{
					TestEqual(TEXT("wall props follow the building; yard props follow terrain"), Accent.bUseBuildingBase, Accent.bWallMounted);
					const double Ground = Terrain.SampleHeightCm(Accent.CellPosition.X, Accent.CellPosition.Y);
					const double AccentBase = (Accent.bUseBuildingBase ? MeshPose.GetLocation().Z : Ground) + Accent.SurfaceOffsetCm;
					if (!Accent.bWallMounted)
						TestTrue(TEXT("yard props stay two cm above terrain after slab removal"), FMath::IsNearlyEqual(AccentBase, Ground + 2, .001));
					TestTrue(TEXT("terrain cannot clip any grounded/wall accent pivot"), AccentBase > Ground);
				}
				TestTrue(TEXT("fixture Home cleanup succeeds"),
				         City->DemolishEntity(Placement.EntityId).IsSuccess());
				Board->RemoveRenderedEntity(Placement.EntityId);
				TestEqual(TEXT("demolition removes all residential structural supports before the next fixture"),
				          Board->ResidentialFoundationInstances->GetInstanceCount(), 0);
				TestFalse(TEXT("demolition rebuild clears the old placement ghost"),
				          Board->PlacementGhost->IsVisible());
			}
			Board->PlacementModel = nullptr;
			if (!bPeakFixture)
				bFinished = true;
		}
		TestTrue(TEXT("both peak and trough cases completed"), bFinished);
	} while (false);
	Board->PlacementModel = nullptr;
	TestTrue(TEXT("foundation presentation never changes generated terrain"),
	         Terrain.GetQuantizedHeightsCm() == OriginalHeights);
	World->DestroyWorld(false);
	return true;
}
#endif
