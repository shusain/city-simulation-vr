#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Presentation/CityScenery.h"
#include "Presentation/CityBoardActor.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Engine/World.h"
#include "ProceduralMeshComponent.h"
#include "Interaction/CityBoardPicking.h"
#include "Interaction/CityBoardManipulator.h"
#include "Presentation/CityWeatherParticles.h"
#include "Components/InstancedStaticMeshComponent.h"

namespace
{
	struct FPresentationDigest
	{
		uint64 Value = 14695981039346656037ull;
		void Add(int64 Number)
		{
			for (int32 Shift = 0; Shift < 64; Shift += 8)
				Value = (Value ^ uint8(uint64(Number) >> Shift)) * 1099511628211ull;
		}
		void Vector(const FVector& V)
		{
			Add(FMath::RoundToInt64(V.X * 10000));
			Add(FMath::RoundToInt64(V.Y * 10000));
			Add(FMath::RoundToInt64(V.Z * 10000));
		}
		void Mesh(const UProceduralMeshComponent* Component)
		{
			Add(Component->GetNumSections());
			for (int32 I = 0; I < Component->GetNumSections(); ++I)
			{
				const auto* S = const_cast<UProceduralMeshComponent*>(Component)->GetProcMeshSection(I);
				if (!S)
					continue;
				Add(S->ProcVertexBuffer.Num());
				Add(S->ProcIndexBuffer.Num());
				for (const auto& V : S->ProcVertexBuffer)
				{
					Vector(V.Position);
					Vector(V.Normal);
					Add(FMath::RoundToInt64(V.UV0.X * 10000));
					Add(FMath::RoundToInt64(V.UV0.Y * 10000));
					Add(V.Color.ToPackedARGB());
				}
				for (uint32 Index : S->ProcIndexBuffer)
					Add(Index);
			}
		}
	};
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityLegacySceneryDigestTest, "CitySim.Dimensions.LegacySceneryDigest",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityLegacySceneryDigestTest::RunTest(const FString& Parameters)
{
	const uint64 Expected[] = {2799442607070966893ull,  6244687301850309103ull, 3122581671110175830ull,
	                           12916115727138838229ull, 6246789109228037347ull, 13199196441274463384ull,
	                           13706387630890733999ull, 10999900758632405061ull};
	int32 Case = 0;
	for (int32 Generator : {0, 2})
		for (int32 Scenery : {0, 1})
			for (int32 Seed : {7429, 103})
			{
				FCityTerrainGenerationParameters P;
				P.GeneratorVersion = Generator;
				P.SceneryVersion = Scenery;
				P.Seed = Seed;
				FCityTerrainHeightfield Terrain;
				if (!TestTrue(TEXT("legacy terrain builds"), Terrain.Generate(P)))
					return false;
				FCitySceneryField Field;
				Field.Build(Terrain);
				FPresentationDigest Digest;
				for (int32 Y = 0; Y < 64; ++Y)
					for (int32 X = 0; X < 64; ++X)
					{
						const auto& S = Field.GetSample({X, Y});
						Digest.Add(S.WarmthPercent);
						Digest.Add(S.MountainAffinityPercent);
						Digest.Add(S.ShoreDistanceCells);
						Digest.Add(S.ElevationCm);
						Digest.Add(int32(S.Slope));
					}
				const auto Candidates = FCitySceneryField::SelectCandidates(Terrain, P.GetEntranceCell(), 420, 100);
				Digest.Add(Candidates.Num());
				for (const auto& C : Candidates)
				{
					Digest.Add(C.Cell.X);
					Digest.Add(C.Cell.Y);
					Digest.Add(C.Hash);
					Digest.Add(int32(C.Kind));
				}
				UE_LOG(LogTemp, Display, TEXT("CITYSIM_SCENERY_DIGEST Generator=%d Scenery=%d Seed=%d Digest=%llu"),
				       Generator, Scenery, Seed, Digest.Value);
				TestEqual(TEXT("legacy scenery matches frozen field/species/candidate order"), Digest.Value,
				          Expected[Case++]);
			}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityDimensionsPresentationTest, "CitySim.Dimensions.BoardPresentation",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityDimensionsPresentationTest::RunTest(const FString& Parameters)
{
	const auto Init = UWorld::InitializationValues()
	                      .AllowAudioPlayback(false)
	                      .CreatePhysicsScene(false)
	                      .CreateNavigation(false)
	                      .CreateAISystem(false)
	                      .ShouldSimulatePhysics(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("DimensionPresentation"), nullptr, true,
	                                  ERHIFeatureLevel::Num, &Init);
	auto* Sim = World->GetSubsystem<UCitySimulationSubsystem>();
	auto* Board = World->SpawnActor<ACityBoardActor>();
	Board->SimulationSubsystem = Sim;
	Board->PlacementModel = &Sim->GetPlacementModel();
	Board->RebuildTerrainSurface(false);
	Board->RebuildTerrainBase();
	FPresentationDigest Digest;
	Digest.Mesh(Board->BoardSurface);
	Digest.Mesh(Board->TerrainBase);
	UE_LOG(LogTemp, Display, TEXT("CITYSIM_BOARD_DIGEST Digest=%llu"), Digest.Value);
	TestEqual(TEXT("legacy terrain/base mesh digest remains exact"), Digest.Value, uint64(16587893004148083286ull));
	Board->RebuildTerrainSurface();
	FPresentationDigest Refined;
	Refined.Mesh(Board->BoardSurface);
	Refined.Mesh(Board->TerrainBase);
	for (const int32 Size : {80, 64, 80})
	{
		const FCityGridDimensions D(Size, Size);
		FCityTerrainGenerationParameters P;
		P.Dimensions = D;
		Sim->Terrain.Generate(P);
		Sim->PlacementModel = MakeUnique<FCityPlacementModel>(100000, D);
		Board->PlacementModel = Sim->PlacementModel.Get();
		Board->PlacementModel->SetTerrain(&Sim->Terrain);
		Board->RebuildTerrainSurface();
		Board->RebuildTerrainBase();
		Board->RebuildWaterSurface();
		const auto* Water = Board->WaterSurface->GetProcMeshSection(0);
		if (!TestNotNull(TEXT("water mesh exists after city switch"), Water))
			continue;
		TMap<FIntPoint, FColor> WaterColors;
		int32 WaterCells = 0, SharedWaterSamples = 0, BlendedWaterSamples = 0;
		for (const auto Class : Sim->Terrain.GetWaterClasses())
			WaterCells += Class == ECityWaterClass::ShallowWater || Class == ECityWaterClass::DeepWater;
		const auto Shallow = FLinearColor(0.04, 0.34, 0.52, 0.70).ToFColorSRGB();
		const auto Deep = FLinearColor(0.025, 0.16, 0.30, 0.78).ToFColorSRGB();
		TestTrue(TEXT("clipped water remains bounded by 256 triangles per cell"),
		         Water->ProcIndexBuffer.Num() <= D.GetCellCount() * 768);
		TestTrue(TEXT("wet logical cells retain visible water"), Water->ProcIndexBuffer.Num() > WaterCells * 3);
		for (const auto& V : Water->ProcVertexBuffer)
		{
			const FIntPoint Key(FMath::RoundToInt(V.Position.X), FMath::RoundToInt(V.Position.Y));
			if (const auto* Old = WaterColors.Find(Key))
			{
				++SharedWaterSamples;
				TestEqual(TEXT("adjacent water tiles have identical edge colors"), V.Color, *Old);
			}
			else
				WaterColors.Add(Key, V.Color);
			BlendedWaterSamples += V.Color != Shallow && V.Color != Deep;
			TestTrue(TEXT("water remains at sea level with only the existing offset"),
			         V.Position.Z == Sim->Terrain.GetSeaLevelCm() + 2.0);
			TestTrue(TEXT("water colors stay inside the accepted palette"),
			         V.Color.R >= Deep.R && V.Color.R <= Shallow.R && V.Color.G >= Deep.G && V.Color.G <= Shallow.G &&
			             V.Color.B >= Deep.B && V.Color.B <= Shallow.B);
		}
		TestTrue(TEXT("water seams exercised"), SharedWaterSamples > 0);
		TestTrue(TEXT("water has intermediate colors rather than two flat classes"), BlendedWaterSamples > 0);
		UE_LOG(LogTemp, Display, TEXT("CITYSIM_WATER_DEPTH_CHECK Size=%d Cells=%d Shared=%d Blended=%d"), Size,
		       WaterCells, SharedWaterSamples, BlendedWaterSamples);
		Board->bWindOverlayRequested = true;
		Board->UpdateWindPresentation();
		TestTrue(TEXT("compass follows city center"),
		         FMath::IsNearlyEqual(Board->WindVaneArrow->GetRelativeLocation().X, (Size / 2 + .5) * 800));
		TestEqual(TEXT("same-weather city switches rebuild wind extent"),
		          Board->WindOverlayInstances->GetInstanceCount(), ((Size - 6 + 9) / 10) * ((Size - 6 + 9) / 10));
		FPresentationDigest Current;
		Current.Mesh(Board->BoardSurface);
		Current.Mesh(Board->TerrainBase);
		if (Size == 64)
			TestEqual(TEXT("legacy mesh exact after expanded city"), Current.Value, Refined.Value);
		TestEqual(TEXT("whole terrain chunk count"), Board->BoardSurface->GetNumSections(), (Size / 16) * (Size / 16));
		TMap<FIntPoint, FProcMeshVertex> Shared;
		int32 SharedCount = 0;
		for (int32 I = 0; I < Board->BoardSurface->GetNumSections(); ++I)
			for (const auto& V : Board->BoardSurface->GetProcMeshSection(I)->ProcVertexBuffer)
			{
				const FIntPoint Key(FMath::RoundToInt(V.Position.X), FMath::RoundToInt(V.Position.Y));
				if (const auto* Old = Shared.Find(Key))
				{
					++SharedCount;
					if (!(Old->Position == V.Position && Old->Normal == V.Normal && Old->UV0 == V.UV0 &&
					      Old->Color == V.Color))
						UE_LOG(
						    LogTemp, Error,
						    TEXT(
						        "CITYSIM_BANK_SEAM XY=%s PosDelta=%.12g NormalDelta=%.12g UVDelta=%.12g ColorOld=%s ColorNew=%s"),
						    *Key.ToString(), (Old->Position - V.Position).Size(), (Old->Normal - V.Normal).Size(),
						    (Old->UV0 - V.UV0).Size(), *Old->Color.ToString(), *V.Color.ToString());
					TestTrue(TEXT("shared chunk geometry/color/normal/UV"),
					         Old->Position == V.Position && Old->Normal == V.Normal && Old->UV0 == V.UV0 &&
					             Old->Color == V.Color);
				}
				else
					Shared.Add(Key, V);
				TestTrue(TEXT("terrain bounded by city"), V.Position.X >= 0 && V.Position.Y >= 0 &&
				                                              V.Position.X <= Size * 800 && V.Position.Y <= Size * 800);
			}
		TestTrue(TEXT("far corner rendered"), Shared.Contains({Size * 800, Size * 800}));
		TestTrue(TEXT("shared borders checked"), SharedCount > 0);
		TestEqual(TEXT("whole base rim"), Board->TerrainBase->GetProcMeshSection(0)->ProcVertexBuffer.Num(),
		          Size * 8 * 4 + 4);
		Board->PlacementModel->SetZoneArea({Size - 2, Size - 2}, {2, 2}, ECityZoneType::Industrial);
		Board->RebuildZoneInstances();
		TestEqual(TEXT("far edge zoning rendered"), Board->IndustrialZoneInstances->GetInstanceCount(), 4);
		FCityEnvironmentSnapshot E(D);
		E.Pollution.Last() = 37;
		const auto S = FCityServiceEvaluator::BuildSnapshot(*Board->PlacementModel, FCityEconomyTuning());
		FCityDiagnosticOverlayEvaluator::Build(*Board->PlacementModel, S, FCityNeedsSnapshot(), FCityEconomyTuning(),
		                                       nullptr, nullptr, nullptr, E, ECityDiagnosticOverlay::Pollution, 0,
		                                       Board->HeatMapResult);
		Board->RebuildHeatMap();
		TestEqual(TEXT("whole heat map"), Board->HeatMapSurface->GetProcMeshSection(0)->ProcVertexBuffer.Num(),
		          Size * Size * 5);
		Board->WaterPlacementOverlay = FCityWaterPlacementOverlay::Build(*Board->PlacementModel, Sim->Terrain, S);
		Board->RebuildWaterPlacementOverlay();
		TestEqual(TEXT("whole groundwater film"),
		          Board->WaterPlacementSurface->GetProcMeshSection(0)->ProcVertexBuffer.Num(),
		          Sim->Terrain.GetSampleCount());
		Board->HeatMapResult.Dimensions = FCityGridDimensions(Size == 64 ? 80 : 64, Size == 64 ? 80 : 64);
		Board->RebuildHeatMap();
		TestEqual(TEXT("foreign heat map suppressed"), Board->HeatMapSurface->GetNumSections(), 0);
		auto* Weather = NewObject<UCityWeatherParticleComponent>(World);
		Weather->SetTerrain(Sim->Terrain);
		const auto Bounds = Weather->CalcBounds(FTransform::Identity).GetBox();
		TestTrue(TEXT("weather culling bounds include whole city"),
		         Bounds.Max.X == Size * 800 + 400 && Bounds.Max.Y == Size * 800 + 400);
		TestTrue(TEXT("weather extent follows city"), Weather->GetBoardSize() == FVector2D(Size * 800, Size * 800));
		FCitySceneryField Field;
		Field.Build(Sim->Terrain);
		const auto Candidates = FCitySceneryField::SelectCandidates(Sim->Terrain, P.GetEntranceCell(), 420, 100);
		TestTrue(TEXT("scenery repeatable"),
		         Candidates == FCitySceneryField::SelectCandidates(Sim->Terrain, P.GetEntranceCell(), 420, 100));
		if (Size == 80)
			TestTrue(TEXT("scenery beyond legacy bounds"),
			         Candidates.ContainsByPredicate([](const auto& C) { return C.Cell.X >= 64 || C.Cell.Y >= 64; }));
		int32 Trees = 0, Rocks = 0;
		for (const auto& C : Candidates)
		{
			TestTrue(TEXT("scenery in bounds"), D.IsValidCell(C.Cell));
			Rocks += C.Kind == ECitySceneryKind::Rock;
			Trees += C.Kind != ECitySceneryKind::Rock;
			TestTrue(TEXT("scenery field covers candidate"), Field.GetSample(C.Cell).WarmthPercent >= 0);
		}
		TestTrue(TEXT("scenery caps retained"), Trees <= 420 && Rocks <= 100);
	}
	FCityTerrainGenerationParameters EntryParameters;
	EntryParameters.Dimensions = FCityGridDimensions(80, 80);
	EntryParameters.GeneratorVersion = 2;
	EntryParameters.Seed = 107;
	Sim->Terrain.Generate(EntryParameters);
	Sim->PlacementModel = MakeUnique<FCityPlacementModel>(100000, EntryParameters.Dimensions);
	Sim->PlacementModel->SetTerrain(&Sim->Terrain);
	Board->PlacementModel = Sim->PlacementModel.Get();
	const auto Entry = Board->PlacementModel->GetEntranceCell();
	TestEqual(TEXT("right entrance fixture"), Entry.X, 79);
	auto Road = Board->PlacementModel->BeginRoadPreview(Entry);
	TestTrue(TEXT("right entrance road placed"), Board->PlacementModel->CommitRoad(Road).IsSuccess());
	Board->RebuildRoadInstances();
	const auto* Details = Board->RoadDetailSurface->GetProcMeshSection(0);
	if (TestNotNull(TEXT("entrance curb mesh"), Details))
		TestFalse(TEXT("right entry has no curb across incoming approach"),
		          Details->ProcVertexBuffer.ContainsByPredicate([](const auto& V)
		                                                        { return V.Normal.Equals(-FVector::ForwardVector); }));
	World->DestroyWorld(false);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityDimensionsPickingTest, "CitySim.Dimensions.PickingAndFraming",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityDimensionsPickingTest::RunTest(const FString& Parameters)
{
	for (int32 Size : {64, 80})
	{
		const FCityGridDimensions D(Size, Size);
		FCityTerrainGenerationParameters P;
		P.Dimensions = D;
		P.GeneratorVersion = 2;
		FCityTerrainHeightfield Terrain;
		Terrain.Generate(P);
		for (double Scale : {.002, .004, .007})
			for (double Yaw : {0., 37., -90.})
			{
				FTransform T(FRotator(0, Yaw, 0), FVector(80, -120, 100), FVector(Scale));
				const FCityCellCoord Expected(Size - 1, Size - 1);
				const auto Local = CityBoardPicking::GetCellCenterLocal(Expected);
				const auto Origin = T.TransformPosition(Local + FVector(0, 0, 10000));
				const auto Direction = T.TransformVector(-FVector::UpVector).GetSafeNormal();
				FCityCellCoord Hit;
				TestTrue(TEXT("flat corner ray"), CityBoardPicking::PickCellFromWorldRay(T, D, Origin, Direction, Hit));
				TestEqual(TEXT("flat corner address"), Hit, Expected);
				TestTrue(TEXT("terrain corner ray"),
				         CityBoardPicking::PickCellFromWorldRay(T, Terrain, Origin, Direction, Hit));
				TestEqual(TEXT("terrain corner address"), Hit, Expected);
				if (Size == 80)
					TestFalse(TEXT("legacy ray rejects expanded corner"),
					          CityBoardPicking::PickCellFromWorldRay(T, Origin, Direction, Hit));
				TestFalse(TEXT("outside flat ray"),
				          CityBoardPicking::PickCellFromWorldRay(
				              T, D, T.TransformPosition(FVector((Size + .01) * 800, (Size + .01) * 800, 10000)),
				              Direction, Hit));
			}
		FCityBoardManipulator Manip;
		TestTrue(TEXT("gesture dimensions accepted"), Manip.SetDimensions(D));
		FCityBoardPresentationPose Pose;
		Pose.Location = FVector(0, 0, 100);
		Pose.UniformScale = 200. / (Size * 800);
		Manip.Reinitialize(true, true, true, true, FVector(-50, 0, 100), FVector(50, 0, 100), Pose);
		TestTrue(TEXT("gesture updates"),
		         Manip.Update(true, true, true, true, FVector(0, -500, 100), FVector(0, 500, 100), Pose));
		TestTrue(TEXT("gesture clamps to4m physical width"),
		         FMath::IsNearlyEqual(Pose.UniformScale * Size * 800, 400., .001));
		const auto Recentered = FCityBoardManipulator::MakeRecenteredPose(FVector(10, 20, 160), FVector::ForwardVector,
		                                                                  200. / (Size * 800), 110, 55, D);
		TestTrue(TEXT("recenter uses actual center"), Recentered.ToTransform()
		                                                  .TransformPosition(FVector(Size * 400, Size * 400, 0))
		                                                  .Equals(FVector(120, 20, 105), .001));
		TestFalse(TEXT("invalid gesture size rejected"),
		          Manip.SetDimensions(FCityGridDimensions(MAX_int32, MAX_int32)));
	}
	FCityCellCoord Sentinel(7, 9);
	FVector Hit(1, 2, 3);
	TestFalse(TEXT("invalid picking size rejected"),
	          CityBoardPicking::PickCellFromWorldRay(FTransform::Identity, FCityGridDimensions(0, 0),
	                                                 FVector(0, 0, 100), -FVector::UpVector, Sentinel, &Hit));
	TestEqual(TEXT("failed ray preserves cell"), Sentinel, FCityCellCoord(7, 9));
	TestEqual(TEXT("failed ray preserves hit"), Hit, FVector(1, 2, 3));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityDimensionsOverlaysTest, "CitySim.Dimensions.OverlayFields",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityDimensionsOverlaysTest::RunTest(const FString& Parameters)
{
	for (int32 Size : {64, 80})
	{
		const FCityGridDimensions D(Size, Size);
		FCityPlacementModel City(100000, D);
		FCityBuildingDefinition Home;
		Home.DefinitionId = TEXT("Home");
		Home.DisplayName = TEXT("Home");
		Home.Footprint = {2, 2};
		auto HomePreview = City.BeginPreview(Home, {Size - 2, Size - 2});
		TestTrue(TEXT("corner overlay lot placed"), City.CommitPlacement(HomePreview, Home).IsSuccess());
		FCityTerrainGenerationParameters P;
		P.Dimensions = D;
		P.GeneratorVersion = 2;
		P.WaterRulesVersion = 1;
		FCityTerrainHeightfield Terrain;
		Terrain.Generate(P);
		FCityEconomyTuning Tuning;
		auto Services = FCityServiceEvaluator::BuildSnapshot(City, Tuning);
		FCityEnvironmentSnapshot Environment(D);
		Environment.Pollution.Last() = 123;
		for (int32 Kind = 1; Kind <= int32(ECityDiagnosticOverlay::Waste); ++Kind)
		{
			FCityDiagnosticOverlayResult Result;
			FCityDiagnosticOverlayEvaluator::Build(City, Services, FCityNeedsSnapshot(), Tuning, nullptr, nullptr,
			                                       nullptr, Environment, ECityDiagnosticOverlay(Kind), 0, Result);
			TestTrue(TEXT("overlay owns dimensions"), Result.Dimensions == D);
			TestEqual(TEXT("overlay cell count"), Result.Values.Num(), Size * Size);
			TestEqual(TEXT("overlay status count"), Result.Status.Num(), Size * Size);
			if (Kind == int32(ECityDiagnosticOverlay::Power))
				TestEqual(TEXT("corner footprint includes every occupied cell"),
				          Result.PositiveCells.Num() + Result.WarningCells.Num(), 4);
			if (Kind == int32(ECityDiagnosticOverlay::Pollution))
			{
				TestEqual(TEXT("corner pollution row stride"), Result.Values.Last(), 123);
				TestTrue(TEXT("corner exact details"), Result.DescribeCell({Size - 1, Size - 1}).Contains(TEXT("123")));
			}
			TestTrue(TEXT("invalid hover safe"), Result.DescribeCell({Size, Size}).Contains(TEXT("Point")));
		}
		const auto Water = FCityWaterPlacementOverlay::Build(City, Terrain, Services);
		TestEqual(TEXT("groundwater field cell count"), Water.GroundwaterYieldPercent.Num(), Size * Size);
		FCityDiagnosticOverlayResult Rejected;
		FCityEnvironmentSnapshot Foreign(FCityGridDimensions(Size == 64 ? 80 : 64, Size == 64 ? 80 : 64));
		FCityDiagnosticOverlayEvaluator::Build(City, Services, FCityNeedsSnapshot(), Tuning, nullptr, nullptr, nullptr,
		                                       Foreign, ECityDiagnosticOverlay::Pollution, 0, Rejected);
		TestTrue(TEXT("foreign overlay suppressed"), Rejected.Values.IsEmpty());
		P.Dimensions = Foreign.Dimensions;
		Terrain.Generate(P);
		TestTrue(TEXT("foreign siting suppressed"),
		         FCityWaterPlacementOverlay::Build(City, Terrain, Services).GroundwaterYieldPercent.IsEmpty());
	}
	return true;
}
#endif
