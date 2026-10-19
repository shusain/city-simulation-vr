#if WITH_DEV_AUTOMATION_TESTS

#include "Interaction/CityBoardPicking.h"
#include "Misc/AutomationTest.h"
#include "Simulation/CityTerrain.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Presentation/CityBoardActor.h"
#include "ProceduralMeshComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityTerrainDeterminismTest, "CitySim.Terrain.DeterminismAndFixtures",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityTerrainDeterminismTest::RunTest(const FString& Parameters)
{
	FCityTerrainGenerationParameters Generation;
	FCityTerrainHeightfield First;
	FCityTerrainHeightfield Second;
	TestTrue(TEXT("default parameters generate"), First.Generate(Generation));
	TestTrue(TEXT("same parameters generate again"), Second.Generate(Generation));
	TestTrue(TEXT("same seed and parameters produce identical quantized heights"),
	         First.GetQuantizedHeightsCm() == Second.GetQuantizedHeightsCm());
	TestTrue(TEXT("same seed and parameters produce identical water topology"),
	         First.GetWaterClasses() == Second.GetWaterClasses());
	TestEqual(TEXT("terrain uses two samples per logical cell"), First.GetQuantizedHeightsCm().Num(),
	          FCityTerrainHeightfield::SampleCount);
	TestEqual(TEXT("starter entrance is flat"), First.GetCellSlope(FCityCellCoord(0, 8)), ECityTerrainSlope::Flat);
	TestEqual(TEXT("starter interior is flat"), First.GetCellSlope(FCityCellCoord(4, 4)), ECityTerrainSlope::Flat);
	TestEqual(TEXT("starter entrance remains dry land"), First.GetWaterClass(FCityCellCoord(0, 8)),
	          ECityWaterClass::Land);
	TestEqual(TEXT("generated terrain retains the configured sea level"), First.GetSeaLevelCm(), Generation.SeaLevelCm);
	TestTrue(TEXT("starter terrain remains above sea level"),
	         First.GetCellCenterHeightCm(FCityCellCoord(0, 8)) > First.GetSeaLevelCm());
	int32 BuildableCells = 0;
	int32 ShallowCells = 0;
	int32 DeepCells = 0;
	for (int32 Y = 0; Y < FCityGrid::Height; ++Y)
	{
		for (int32 X = 0; X < FCityGrid::Width; ++X)
		{
			const FCityCellCoord Cell(X, Y);
			BuildableCells += First.IsCellBuildable(Cell) ? 1 : 0;
			ShallowCells += First.GetWaterClass(Cell) == ECityWaterClass::ShallowWater ? 1 : 0;
			DeepCells += First.GetWaterClass(Cell) == ECityWaterClass::DeepWater ? 1 : 0;
			if (First.IsWaterCell(Cell))
			{
				TestEqual(TEXT("all exposed water uses one sea-level surface"), First.GetWaterSurfaceHeightCm(Cell),
				          First.GetSeaLevelCm());
				TestTrue(TEXT("water is exposed only where terrain falls below sea level"),
				         First.GetCellCenterHeightCm(Cell) < First.GetSeaLevelCm());
			}
		}
	}
	TestTrue(TEXT("default terrain retains at least 70 percent buildable land"),
	         BuildableCells * 100 >= FCityGrid::CellCount * 70);
	TestTrue(TEXT("gentler default terrain retains at least 80 percent buildable land"),
	         BuildableCells * 100 >= FCityGrid::CellCount * 80);
	int32 DryFoundationCandidates = 0;
	int32 BuildableFoundationCandidates = 0;
	for (int32 Y = 0; Y < FCityGrid::Height - 1; ++Y)
	{
		for (int32 X = 0; X < FCityGrid::Width - 1; ++X)
		{
			int32 Minimum = MAX_int32;
			int32 Maximum = MIN_int32;
			bool bDry = true;
			for (int32 OffsetY = 0; OffsetY < 2; ++OffsetY)
			{
				for (int32 OffsetX = 0; OffsetX < 2; ++OffsetX)
				{
					const FCityCellCoord Cell(X + OffsetX, Y + OffsetY);
					bDry &= First.GetWaterClass(Cell) == ECityWaterClass::Land;
					Minimum = FMath::Min(Minimum, First.GetCellMinHeightCm(Cell));
					Maximum = FMath::Max(Maximum, First.GetCellMaxHeightCm(Cell));
				}
			}
			if (bDry)
			{
				++DryFoundationCandidates;
				BuildableFoundationCandidates += Maximum - Minimum <= 320 ? 1 : 0;
			}
		}
	}
	TestTrue(TEXT("at least 95 percent of dry 2x2 parcels accept a normal foundation"),
	         BuildableFoundationCandidates * 100 >= DryFoundationCandidates * 95);
	TestTrue(TEXT("generated hydrology includes shallow water"), ShallowCells > 0);
	TestTrue(TEXT("generated hydrology includes deep water"), DeepCells > 0);
	TestFalse(TEXT("deterministic river has cells"), First.GetRiverCells().IsEmpty());
	TestEqual(TEXT("river drains to board edge"), First.GetRiverCells().Last().X, FCityGrid::Width - 1);
	FCityCellCoord PreviousRiverCell = First.GetRiverCells()[0];
	for (int32 RiverIndex = 0; RiverIndex < First.GetRiverCells().Num(); ++RiverIndex)
	{
		const FCityCellCoord& RiverCell = First.GetRiverCells()[RiverIndex];
		TestTrue(TEXT("river remains within board bounds"), RiverCell.X >= 0 && RiverCell.Y >= 0 &&
		                                                        RiverCell.X < FCityGrid::Width &&
		                                                        RiverCell.Y < FCityGrid::Height);
		TestTrue(TEXT("river path is below sea level"), First.IsWaterCell(RiverCell));
		TestEqual(TEXT("river surface is the shared sea level"), First.GetWaterSurfaceHeightCm(RiverCell),
		          First.GetSeaLevelCm());
		if (RiverIndex > 0)
		{
			const int32 ManhattanDistance =
			    FMath::Abs(RiverCell.X - PreviousRiverCell.X) + FMath::Abs(RiverCell.Y - PreviousRiverCell.Y);
			TestEqual(TEXT("river path remains orthogonally connected"), ManhattanDistance, 1);
		}
		PreviousRiverCell = RiverCell;
	}
	int32 MaximumStarterTransitionStepCm = 0;
	FIntPoint MaximumStarterTransitionStepLocation = FIntPoint::ZeroValue;
	for (int32 Y = 0; Y < 24 * FCityTerrainHeightfield::SamplesPerCell; ++Y)
	{
		for (int32 X = 0; X < 20 * FCityTerrainHeightfield::SamplesPerCell; ++X)
		{
			const int32 Height = First.GetSampleHeightCm(X, Y);
			const int32 StepX = FMath::Abs(Height - First.GetSampleHeightCm(X + 1, Y));
			const int32 StepY = FMath::Abs(Height - First.GetSampleHeightCm(X, Y + 1));
			if (StepX > MaximumStarterTransitionStepCm)
			{
				MaximumStarterTransitionStepCm = StepX;
				MaximumStarterTransitionStepLocation = FIntPoint(X, Y);
			}
			if (StepY > MaximumStarterTransitionStepCm)
			{
				MaximumStarterTransitionStepCm = StepY;
				MaximumStarterTransitionStepLocation = FIntPoint(X, Y);
			}
		}
	}
	AddInfo(FString::Printf(TEXT("Maximum starter transition sample step: %d cm at (%d,%d)"),
	                        MaximumStarterTransitionStepCm, MaximumStarterTransitionStepLocation.X,
	                        MaximumStarterTransitionStepLocation.Y));
	TestTrue(TEXT("starter area feathers into terrain without a retaining-wall step"),
	         MaximumStarterTransitionStepCm <= 200);
	TestTrue(TEXT("hill fixture rises above starter"),
	         First.GetCellCenterHeightCm(FCityCellCoord(15, 45)) > First.GetCellCenterHeightCm(FCityCellCoord(0, 8)));
	TestTrue(TEXT("ridge fixture rises above nearby terrain"),
	         First.GetCellCenterHeightCm(FCityCellCoord(48, 34)) > First.GetCellCenterHeightCm(FCityCellCoord(40, 34)));
	TestTrue(TEXT("edge samples remain finite"), FMath::IsFinite(First.SampleHeightCm(64.0, 64.0)));
	bool bHasSubCellVariation = false;
	for (int32 Y = 12; Y < FCityGrid::Height && !bHasSubCellVariation; ++Y)
	{
		for (int32 X = 8; X < FCityGrid::Width; ++X)
		{
			const int32 Midpoint = First.GetSampleHeightCm(X * FCityTerrainHeightfield::SamplesPerCell + 1,
			                                               Y * FCityTerrainHeightfield::SamplesPerCell + 1);
			if (Midpoint != First.GetVertexHeightCm(X, Y))
			{
				bHasSubCellVariation = true;
				break;
			}
		}
	}
	TestTrue(TEXT("non-starter terrain varies inside logical cells"), bHasSubCellVariation);
	const FVector HillNormal = First.GetSurfaceNormal(15.5, 45.5);
	TestTrue(TEXT("hill surface supplies a normalized tilted road normal"),
	         HillNormal.IsNormalized() && !HillNormal.Equals(FVector::UpVector, 0.001));
	TestEqual(TEXT("adjacent road profiles share first border endpoint"),
	          First.GetCellCornerHeightCm(FCityCellCoord(10, 20), 1, 0),
	          First.GetCellCornerHeightCm(FCityCellCoord(11, 20), 0, 0));
	TestEqual(TEXT("adjacent road profiles share second border endpoint"),
	          First.GetCellCornerHeightCm(FCityCellCoord(10, 20), 1, 1),
	          First.GetCellCornerHeightCm(FCityCellCoord(11, 20), 0, 1));

	FCityCellCoord Picked;
	const FTransform Transform(FRotator(0.0, 31.0, 0.0), FVector(50.0, -80.0, 110.0), FVector(0.004));
	const FCityCellCoord Expected(15, 45);
	FVector LocalCenter = CityBoardPicking::GetCellCenterLocal(Expected);
	LocalCenter.Z = First.GetCellCenterHeightCm(Expected);
	const FVector WorldCenter = Transform.TransformPosition(LocalCenter);
	const FVector WorldUp = Transform.TransformVectorNoScale(FVector::UpVector);
	TestTrue(TEXT("transformed terrain ray resolves"),
	         CityBoardPicking::PickCellFromWorldRay(Transform, First, WorldCenter + WorldUp * 100.0, -WorldUp, Picked));
	TestEqual(TEXT("terrain ray keeps logical cell"), Picked, Expected);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityTerrainPresentationTest, "CitySim.Terrain.PresentationContinuity",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityTerrainPresentationTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues Values = UWorld::InitializationValues()
	                                                .AllowAudioPlayback(false)
	                                                .CreatePhysicsScene(false)
	                                                .CreateNavigation(false)
	                                                .CreateAISystem(false)
	                                                .ShouldSimulatePhysics(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("CityTerrainPresentationTest"), nullptr, true,
	                                    ERHIFeatureLevel::Num, &Values);
	ACityBoardActor* Board = World->SpawnActor<ACityBoardActor>();
	UCitySimulationSubsystem* Simulation = World->GetSubsystem<UCitySimulationSubsystem>();
	Board->SimulationSubsystem = Simulation;
	const TArray<int32> OriginalHeights = Simulation->GetTerrain().GetQuantizedHeightsCm();
	Board->RebuildTerrainSurface();
	TMap<FIntPoint, FProcMeshVertex> SharedVertices;
	bool bContinuous = true, bLogicalUVs = true;
	int32 SharedCount = 0;
	for (int32 SectionIndex = 0; SectionIndex < Board->BoardSurface->GetNumSections(); ++SectionIndex)
	{
		const FProcMeshSection* Section = Board->BoardSurface->GetProcMeshSection(SectionIndex);
		if (!Section)
			continue;
		for (const FProcMeshVertex& Vertex : Section->ProcVertexBuffer)
		{
			const FIntPoint Key(FMath::RoundToInt(Vertex.Position.X), FMath::RoundToInt(Vertex.Position.Y));
			bLogicalUVs &= Vertex.UV0.Equals(
			    FVector2D(Vertex.Position.X, Vertex.Position.Y) / CityBoardPicking::LogicalCellSizeCm, 0.0001);
			if (const FProcMeshVertex* Existing = SharedVertices.Find(Key))
			{
				++SharedCount;
				bContinuous &= Existing->Position.Equals(Vertex.Position) && Existing->Normal.Equals(Vertex.Normal) &&
				               Existing->UV0.Equals(Vertex.UV0) && Existing->Color == Vertex.Color;
			}
			else
				SharedVertices.Add(Key, Vertex);
		}
	}
	TestTrue(TEXT("terrain fixture includes shared chunk borders"), SharedCount > 0);
	TestTrue(TEXT("chunk borders share height, normal, texture phase and color"), bContinuous);
	TestTrue(TEXT("terrain detail uses continuous board coordinates"), bLogicalUVs);
	TestTrue(TEXT("visual terrain refinement leaves authoritative heights unchanged"),
	         OriginalHeights == Simulation->GetTerrain().GetQuantizedHeightsCm());
	Board->PlacementModel = &Simulation->GetPlacementModel();
	Board->SetToolMode(ECityToolMode::Home);
	Board->SetHoveredCell({12, 14}, true);
	if (TestNotNull(TEXT("local grid has a dynamic terrain material"), Board->TerrainMaterial.Get()))
	{
		auto Reveal = [&]() { return Board->TerrainMaterial->K2_GetScalarParameterValue(TEXT("GridReveal")); };
		TestEqual(TEXT("building placement reveals a local grid"), Reveal(), 1.0f);
		const float BuildingRadius = Board->TerrainMaterial->K2_GetScalarParameterValue(TEXT("GridRadius"));
		Board->SetToolMode(ECityToolMode::Zone);
		Board->SetHoveredCell({20, 22}, true);
		TestTrue(TEXT("zoning reveals a wider region"),
		         Board->TerrainMaterial->K2_GetScalarParameterValue(TEXT("GridRadius")) > BuildingRadius);
		const FLinearColor Center = Board->TerrainMaterial->K2_GetVectorParameterValue(TEXT("GridCenter"));
		TestTrue(TEXT("grid follows the targeted cell"),
		         FMath::IsNearlyEqual(Center.R, 20.5f) && FMath::IsNearlyEqual(Center.G, 22.5f));
		Board->SetHoveredCell({}, false);
		TestEqual(TEXT("losing the target hides the grid"), Reveal(), 0.0f);
		Board->SetToolMode(ECityToolMode::Inspect);
		Board->SetHoveredCell({20, 22}, true);
		TestEqual(TEXT("inspection keeps terrain clear"), Reveal(), 0.0f);
		Board->SetToolMode(ECityToolMode::Home);
		Board->SetManipulationActive(true);
		Board->SetHoveredCell({20, 22}, true);
		TestEqual(TEXT("board manipulation hides the grid"), Reveal(), 0.0f);
		Board->SetManipulationActive(false);
	}
	Board->SetControllerRay(FVector::ZeroVector, FVector::UpVector, false, true);
	TestTrue(TEXT("UI focus suppresses desktop picking fallback"), Board->bSuppressDesktopRay);
	Board->SetControllerRay(FVector::ZeroVector, FVector::UpVector, false);
	TestFalse(TEXT("non-XR mode retains desktop picking"), Board->bSuppressDesktopRay);
	const FVector Vane = Board->WindVaneArrow->GetComponentLocation();
	Board->SetControllerRay(Vane + FVector(0, 0, 100), -FVector::UpVector, true);
	Board->UpdateWindPresentation();
	TestFalse(TEXT("wind hover alone shows no overlay"), Board->bWindOverlayVisible);
	TestTrue(TEXT("trigger on wind vane consumes the action"), Board->TryToggleWindOverlay());
	TestTrue(TEXT("first press shows wind arrows"), Board->bWindOverlayVisible);
	Board->SetControllerRay(FVector::ZeroVector, FVector::UpVector, false);
	Board->UpdateWindPresentation();
	TestTrue(TEXT("wind remains visible after pointing away"), Board->bWindOverlayVisible);
	TestFalse(TEXT("press away does not toggle wind"), Board->TryToggleWindOverlay());
	Board->SetControllerRay(Vane + FVector(0, 0, 100), -FVector::UpVector, true);
	TestTrue(TEXT("second vane press is consumed"), Board->TryToggleWindOverlay());
	TestFalse(TEXT("second press hides wind arrows"), Board->bWindOverlayVisible);
	const FVector Gap = Vane + FVector(15, 15, 0);
	Board->SetControllerRay(Gap + FVector(0, -100, 100), FVector(0, 1, -1).GetSafeNormal(), true);
	TestFalse(TEXT("oblique rays beside the visible vane do not toggle it"), Board->TryToggleWindOverlay());
	TestFalse(TEXT("empty space around vane leaves wind off"), Board->bWindOverlayVisible);

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityTerrainBaseTest, "CitySim.Terrain.EnclosedBase",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityTerrainBaseTest::RunTest(const FString& Parameters)
{
	const auto Values = UWorld::InitializationValues()
	                        .AllowAudioPlayback(false)
	                        .CreatePhysicsScene(false)
	                        .CreateNavigation(false)
	                        .CreateAISystem(false)
	                        .ShouldSimulatePhysics(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("CityTerrainBaseTest"), nullptr, true,
	                                    ERHIFeatureLevel::Num, &Values);
	auto* Board = World->SpawnActor<ACityBoardActor>();
	auto* Simulation = World->GetSubsystem<UCitySimulationSubsystem>();
	Board->SimulationSubsystem = Simulation;
	constexpr int32 RimCount = 2 * (FCityTerrainHeightfield::SampleWidth + FCityTerrainHeightfield::SampleHeight - 2);
	for (const int32 Version : {0, 1, 2})
	{
		FCityTerrainGenerationParameters Generation;
		Generation.GeneratorVersion = Version;
		Generation.WaterRulesVersion = Version > 0 ? 1 : 0;
		if (Version == 0)
			Generation.BroadElevationAmplitudeCm = Generation.HillAmplitudeCm = Generation.RidgeAmplitudeCm = 0;
		TestTrue(TEXT("flat, varied and rugged fixtures generate"), Simulation->Terrain.Generate(Generation));
		const auto Heights = Simulation->Terrain.GetQuantizedHeightsCm();
		Board->RebuildTerrainSurface();
		const auto* Section = Board->TerrainBase->GetProcMeshSection(0);
		if (!TestNotNull(TEXT("opaque base built with terrain"), Section))
			continue;
		TestEqual(TEXT("bounded side strip plus bottom quad"), Section->ProcVertexBuffer.Num(), RimCount * 4 + 4);
		TestEqual(TEXT("one thousand and twenty-six triangles, independent of city population"),
		          Section->ProcIndexBuffer.Num(), (RimCount + 1) * 6);
		bool bSealed = true, bSamplesMatch = true, bOutward = true;
		int32 Lowest = MAX_int32;
		for (const int32 Height : Heights)
			Lowest = FMath::Min(Lowest, Height);
		const double Bottom = Lowest - FCityGrid::Width * CityBoardPicking::LogicalCellSizeCm * .025;
		for (int32 Edge = 0; Edge < RimCount; ++Edge)
		{
			const auto& A = Section->ProcVertexBuffer[Edge * 4].Position;
			const auto& B = Section->ProcVertexBuffer[Edge * 4 + 1].Position;
			const auto& C = Section->ProcVertexBuffer[Edge * 4 + 2].Position;
			const auto& D = Section->ProcVertexBuffer[Edge * 4 + 3].Position;
			const auto& Next = Section->ProcVertexBuffer[((Edge + 1) % RimCount) * 4].Position;
			bSealed &= B.Equals(Next) && C.Equals(FVector(B.X, B.Y, Bottom)) && D.Equals(FVector(A.X, A.Y, Bottom));
			bSamplesMatch &= FMath::IsNearlyEqual(
			    A.Z, Simulation->Terrain.SampleHeightCm(A.X / CityBoardPicking::LogicalCellSizeCm,
			                                            A.Y / CityBoardPicking::LogicalCellSizeCm));
		}
		for (int32 Index = 0; Index < Section->ProcIndexBuffer.Num(); Index += 3)
		{
			const auto& A = Section->ProcVertexBuffer[Section->ProcIndexBuffer[Index]];
			const auto& B = Section->ProcVertexBuffer[Section->ProcIndexBuffer[Index + 1]];
			const auto& C = Section->ProcVertexBuffer[Section->ProcIndexBuffer[Index + 2]];
			bOutward &= FVector::DotProduct(FVector::CrossProduct(B.Position - A.Position, C.Position - A.Position),
			                                A.Normal) < 0;
		}
		TestTrue(TEXT("all four sides close without gaps and join one bottom level"), bSealed);
		TestTrue(TEXT("every half-cell boundary sample exactly meets the rendered terrain"), bSamplesMatch);
		TestTrue(TEXT("sides and underside face outward with Unreal winding"), bOutward);
		TestTrue(TEXT("bottom below deepest terrain including water basins"), Section->SectionLocalBox.Min.Z == Bottom);
		TestTrue(TEXT("presentation does not alter saved terrain heights"),
		         Heights == Simulation->Terrain.GetQuantizedHeightsCm());
		const FTransform Moved(FRotator(12, 75, 0), FVector(200, -50, 140), FVector(.008));
		Board->SetActorTransform(Moved);
		TestTrue(TEXT("base follows the same translation, rotation and scale as terrain"),
		         Board->TerrainBase->GetComponentTransform().Equals(Board->BoardSurface->GetComponentTransform()) &&
		             Board->TerrainBase->GetComponentTransform().Equals(Moved));
	}
	TestTrue(TEXT("solid base uses an opaque material"),
	         Board->TerrainBase->GetMaterial(0)->GetBlendMode() == BLEND_Opaque);
	TestTrue(TEXT("base cannot intercept tools, overlap, navigate or cast stray shadows"),
	         Board->TerrainBase->GetCollisionEnabled() == ECollisionEnabled::NoCollision &&
	             !Board->TerrainBase->GetGenerateOverlapEvents() && !Board->TerrainBase->CanEverAffectNavigation() &&
	             !Board->TerrainBase->CastShadow);
	TestTrue(TEXT("city foundation casters remain enabled"),
	         Board->FoundationInstances->CastShadow && Board->ResidentialFoundationInstances->CastShadow);
	World->DestroyWorld(false);
	return true;
}

#endif
