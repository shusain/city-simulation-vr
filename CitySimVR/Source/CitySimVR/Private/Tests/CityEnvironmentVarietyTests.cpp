#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Simulation/CityTerrain.h"
#include "Simulation/CityEnvironmentSimulation.h"
#include "Simulation/CityTrafficSimulation.h"
#include "Simulation/CityEconomySimulation.h"
#include "Persistence/CityPersistence.h"
#include "Presentation/CityAtmosphereMotion.h"
#include "Presentation/CityAtmosphereComponent.h"
#include "Presentation/CityWeatherParticles.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleRequired.h"
#include "Particles/Spawn/ParticleModuleSpawn.h"
#include "Distributions/DistributionFloatConstant.h"
#if WITH_EDITOR
#include "Materials/Material.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionParticleColor.h"
#endif
#include "ParticleEmitterInstances.h"
#include "Presentation/CityBoardActor.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Interaction/CityBoardPicking.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Misc/Paths.h"
#include "Misc/Crc.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityMapVarietyTest, "CitySim.Terrain.VarietyEntrancesAndCompatibility",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityMapVarietyTest::RunTest(const FString& Parameters)
{
	struct FFrozenTerrain
	{
		int32 Version, Seed;
		uint32 Heights, Water;
	};
	// Recorded from the pre-refinement native generator. Geometry is a save contract.
	const FFrozenTerrain Frozen[] = {{0, 1, 3118719259u, 257967085u},  {0, 2, 2509831228u, 1338892911u},
	                                 {0, 5, 4005126841u, 116796809u},  {0, 12, 3384221727u, 1471973427u},
	                                 {1, 1, 2873106277u, 1730029356u}, {1, 2, 418059202u, 1297064396u},
	                                 {1, 5, 59387736u, 3751508013u},   {1, 12, 1493743879u, 560808012u}};
	for (const auto& Expected : Frozen)
	{
		FCityTerrainGenerationParameters P;
		P.GeneratorVersion = Expected.Version;
		P.Seed = Expected.Seed;
		FCityTerrainHeightfield Terrain;
		Terrain.Generate(P);
		TestEqual(TEXT("frozen terrain heightfield stays byte-identical"),
		          FCrc::MemCrc32(Terrain.GetQuantizedHeightsCm().GetData(),
		                         Terrain.GetQuantizedHeightsCm().Num() * sizeof(int32)),
		          Expected.Heights);
		TestEqual(TEXT("frozen water classification stays byte-identical"),
		          FCrc::MemCrc32(Terrain.GetWaterClasses().GetData(),
		                         Terrain.GetWaterClasses().Num() * sizeof(ECityWaterClass)),
		          Expected.Water);
	}
	TSet<int32> Rotations;
	TSet<FString> Landscapes;
	TSet<FCityCellCoord> Entries;
	TSet<FString> PreviewProfiles;
	int32 MinWater = MAX_int32, MaxWater = 0, MinSteep = MAX_int32, MaxSteep = 0, MinBuildable = MAX_int32,
	      MaxHighland = 0;
	int32 DryMaps = 0, AsymmetricBasins = 0, RuggedSamples = 0, MaxWaterBodies = 0;
	for (int32 Seed = 1; Seed <= 96; ++Seed)
	{
		FCityTerrainGenerationParameters P;
		P.Seed = Seed;
		P.GeneratorVersion = 2;
		FCityTerrainHeightfield First, Second;
		TestTrue(TEXT("valid new map generates"), First.Generate(P));
		Second.Generate(P);
		TestTrue(TEXT("same seed/version reproduces exact terrain and water"),
		         First.GetQuantizedHeightsCm() == Second.GetQuantizedHeightsCm() &&
		             First.GetWaterClasses() == Second.GetWaterClasses());
		auto WritePreview = [&](const FString& Profile) {
			if (PreviewProfiles.Contains(Profile))
				return;
			PreviewProfiles.Add(Profile);
			FCitySaveDocument Fixture;
			Fixture.Terrain = P;
			Fixture.Placement = FCityPlacementModel(10000).ExportState();
			Fixture.bSimulationPaused = true;
			Fixture.Identity.Name = Profile + TEXT(" preview");
			Fixture.Environment.WindStrength = 2;
			Fixture.Environment.bPrecipitation = Profile == TEXT("Rain") || Profile == TEXT("Snow");
			Fixture.Environment.Season = Profile == TEXT("Snow") ? ECitySeason::Winter : ECitySeason::Summer;
			Fixture.Environment.Temperature =
			    Profile == TEXT("Snow") ? ECityTemperatureBand::Cold : ECityTemperatureBand::Mild;
			FString Json, Error;
			TestTrue(TEXT("isolated environment visual fixture serializes"),
			         FCityPersistence::Serialize(Fixture, Json, Error));
			TestTrue(
			    TEXT("isolated environment fixture writes outside player saves"),
			    FCityPersistence::WriteAtomic(
			        FPaths::ConvertRelativePathToFull(
			            FPaths::ProjectDir() / TEXT("../Artifacts/Preview/EnvironmentVariety-") + Profile + TEXT(".json")),
			        Json, Error));
		};
		const FString Landscape = P.DescribeLandscape();
		WritePreview(Landscape.StartsWith(TEXT("Lowlands"))        ? TEXT("Lowlands")
		             : Landscape.StartsWith(TEXT("Rolling hills")) ? TEXT("Rain")
		                                                           : TEXT("Snow"));
		if (Landscape.StartsWith(TEXT("Mountain")))
			WritePreview(TEXT("Mountains"));
		if (Landscape.EndsWith(TEXT("lake chain")))
			WritePreview(TEXT("Lakes"));
		if (Landscape.EndsWith(TEXT("river valley")))
			WritePreview(TEXT("River"));
		if (Landscape.EndsWith(TEXT("dry landscape")))
			WritePreview(TEXT("Dry"));
		const auto Entry = P.GetEntranceCell();
		Rotations.Add(P.GetMapRotation());
		Landscapes.Add(P.DescribeLandscape());
		Entries.Add(Entry);
		TestTrue(TEXT("entry is on the map boundary"),
		         Entry.X == 0 || Entry.Y == 0 || Entry.X == 63 || Entry.Y == 63);
		TestTrue(TEXT("entry is dry and buildable"), First.IsCellBuildable(Entry));
		FCityCellCoord Inside = Entry;
		if (Entry.X == 0)
			Inside.X += 10;
		else if (Entry.X == 63)
			Inside.X -= 10;
		else if (Entry.Y == 0)
			Inside.Y += 10;
		else
			Inside.Y -= 10;
		FCityPlacementModel City(100000);
		City.SetTerrain(&First);
		auto Road = City.BeginRoadPreview(Entry);
		City.UpdateRoadPreviewEnd(Road, Inside);
		TestTrue(TEXT("guaranteed starter road builds for every orientation"),
		         City.CommitRoad(Road).IsSuccess());
		TestTrue(TEXT("inward starter road is connected"), City.IsRoadConnected(Inside));
		FCityCellCoord Portal;
		TestTrue(TEXT("freight/newcomer portal resolves the generated entrance"),
		         FCityTrafficSimulation::FindPortal(City, nullptr, 0, Portal) && Portal == Entry);
		int32 Water = 0, Steep = 0, Buildable = 0, Highland = 0;
		FVector2D WaterSum = FVector2D::ZeroVector;
		TSet<FCityCellCoord> WaterCells;
		for (int32 Y = 0; Y < 64; ++Y)
			for (int32 X = 0; X < 64; ++X)
			{
				const FCityCellCoord Cell(X, Y);
				Water += First.IsWaterCell(Cell);
				Steep += First.GetCellSlope(Cell) == ECityTerrainSlope::Steep;
				Buildable += First.IsCellBuildable(Cell);
				Highland += First.GetCellCenterHeightCm(Cell) >= 2000;
				if (First.IsWaterCell(Cell))
				{
					WaterCells.Add(Cell);
					WaterSum += FVector2D(X, Y);
				}
			}
		TestTrue(TEXT("compact terrain features preserve at least 70 percent buildable land"),
		         Buildable * 100 >= FCityGrid::CellCount * 70);
		TestTrue(TEXT("high mountain land stays under 15 percent of the map"),
		         Highland * 100 <= FCityGrid::CellCount * 15);
		MinBuildable = FMath::Min(MinBuildable, Buildable);
		MaxHighland = FMath::Max(MaxHighland, Highland);
		if (Landscape.EndsWith(TEXT("dry landscape")))
		{
			++DryMaps;
			TestEqual(TEXT("dry profiles have no incidental lake/river"), Water, 0);
		}
		if (Landscape.EndsWith(TEXT("lake basin")) && Water > 0)
		{
			const FVector2D Center = WaterSum / Water;
			int32 Asymmetry = 0;
			for (const auto& Cell : WaterCells)
			{
				Asymmetry += !WaterCells.Contains({FMath::RoundToInt32(Center.X * 2 - Cell.X), Cell.Y});
				Asymmetry += !WaterCells.Contains({Cell.X, FMath::RoundToInt32(Center.Y * 2 - Cell.Y)});
			}
			AsymmetricBasins += Asymmetry > Water / 4;
		}
		int32 Bodies = 0;
		while (!WaterCells.IsEmpty())
		{
			++Bodies;
			TArray<FCityCellCoord> Flood;
			Flood.Add(*WaterCells.CreateConstIterator());
			WaterCells.Remove(Flood[0]);
			for (int32 Index = 0; Index < Flood.Num(); ++Index)
				for (const FCityCellCoord Offset : {FCityCellCoord(1, 0), FCityCellCoord(-1, 0),
				                                    FCityCellCoord(0, 1), FCityCellCoord(0, -1)})
				{
					const FCityCellCoord Neighbor(Flood[Index].X + Offset.X, Flood[Index].Y + Offset.Y);
					if (WaterCells.Remove(Neighbor))
						Flood.Add(Neighbor);
				}
		}
		MaxWaterBodies = FMath::Max(MaxWaterBodies, Bodies);
		for (int32 Y = 1; Y < FCityTerrainHeightfield::SampleHeight - 1; ++Y)
			for (int32 X = 1; X < FCityTerrainHeightfield::SampleWidth - 1; ++X)
			{
				const int32 Height = First.GetSampleHeightCm(X, Y);
				if (Height < 2000 || First.GetWaterClass({X / 2, Y / 2}) != ECityWaterClass::Land)
					continue;
				const int32 Curvature = FMath::Abs(First.GetSampleHeightCm(X - 1, Y) +
				                                   First.GetSampleHeightCm(X + 1, Y) - 2 * Height) +
				                        FMath::Abs(First.GetSampleHeightCm(X, Y - 1) +
				                                   First.GetSampleHeightCm(X, Y + 1) - 2 * Height);
				RuggedSamples += Curvature >= 300;
			}
		MinWater = FMath::Min(MinWater, Water);
		MaxWater = FMath::Max(MaxWater, Water);
		MinSteep = FMath::Min(MinSteep, Steep);
		MaxSteep = FMath::Max(MaxSteep, Steep);
		for (int32 I = 0; I < First.GetRiverCells().Num(); ++I)
		{
			const auto Cell = First.GetRiverCells()[I];
			TestTrue(TEXT("river bed remains below the shared water surface"), First.IsWaterCell(Cell));
			if (I > 0)
			{
				const auto Previous = First.GetRiverCells()[I - 1];
				TestEqual(TEXT("river remains orthogonally connected after rotation"),
				          FMath::Abs(Cell.X - Previous.X) + FMath::Abs(Cell.Y - Previous.Y), 1);
			}
		}
	}
	TestEqual(TEXT("seed sample reaches all four entrance edges"), Rotations.Num(), 4);
	TestEqual(TEXT("all relief/water combinations represented"), Landscapes.Num(), 18);
	TestTrue(TEXT("entry position varies along edges"), Entries.Num() > 20);
	TestTrue(TEXT("water and steep terrain coverage vary materially"),
	         MaxWater - MinWater > 100 && MaxSteep - MinSteep > 100);
	TestTrue(TEXT("lake silhouettes include asymmetric overlapping basins"), AsymmetricBasins >= 5);
	TestTrue(TEXT("water availability and separate lake counts vary"),
	         DryMaps > 5 && MinWater == 0 && MaxWaterBodies >= 3);
	TestTrue(TEXT("compact mountains contain higher-frequency ridges and gullies"), RuggedSamples > 100);
	AddInfo(FString::Printf(
	    TEXT("96 seeds: water %d..%d; steep %d..%d; entries %d; minimum buildable %d/4096; maximum highland "
	         "%d/4096; asymmetric basins %d; rugged samples %d; water bodies up to %d"),
	    MinWater, MaxWater, MinSteep, MaxSteep, Entries.Num(), MinBuildable, MaxHighland, AsymmetricBasins,
	    RuggedSamples, MaxWaterBodies));
	FCitySaveDocument Doc, Read;
	Doc.Placement = FCityPlacementModel(10000).ExportState();
	Doc.Terrain.GeneratorVersion = 2;
	Doc.Terrain.Seed = 12;
	FString Json, Error;
	TestTrue(TEXT("new terrain saves"), FCityPersistence::Serialize(Doc, Json, Error));
	const bool bLoaded = FCityPersistence::Deserialize(Json, Read, Error);
	TestTrue(*FString::Printf(TEXT("new terrain reloads: %s"), *Error), bLoaded);
	TestTrue(TEXT("version and entry preserved"),
	         Read.Terrain == Doc.Terrain && Read.Terrain.GetEntranceCell() == Doc.Terrain.GetEntranceCell());
	Doc.SchemaVersion = 54;
	TestFalse(TEXT("downgrade cannot silently move saved terrain"),
	          FCityPersistence::Serialize(Doc, Json, Error));
	Doc.SchemaVersion = FCitySaveDocument::CurrentSchemaVersion;
	Doc.Terrain.GeneratorVersion = 0;
	TestTrue(TEXT("legacy terrain still saves"), FCityPersistence::Serialize(Doc, Json, Error));
	TestTrue(TEXT("legacy terrain still reloads"), FCityPersistence::Deserialize(Json, Read, Error));
	TestTrue(TEXT("old maps keep original terrain and entrance"),
	         Read.Terrain.GeneratorVersion == 0 &&
	             Read.Terrain.GetEntranceCell() == FCityPlacementModel::EntranceCell);
	Doc.SchemaVersion = FCitySaveDocument::CurrentSchemaVersion;
	Doc.Terrain.GeneratorVersion = 1;
	TestTrue(TEXT("rounded generator1 maps still save"),
	         FCityPersistence::Serialize(Doc, Json, Error) &&
	             FCityPersistence::Deserialize(Json, Read, Error));
	TestEqual(TEXT("rounded generator1 remains selected after reload"), Read.Terrain.GeneratorVersion, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityProgressiveWeatherTest,
                                 "CitySim.Environment.ProgressiveSeasonalWeather",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityProgressiveWeatherTest::RunTest(const FString& Parameters)
{
	FCityPlacementModel City;
	FCityServiceSnapshot Services;
	FCityEconomyTuning Tuning;
	FCityEnvironmentSimulation Weather;
	int32 WetWinters = 0, WetSummers = 0;
	FCityEnvironmentSnapshot Previous = Weather.GetSnapshot();
	for (int32 Month = 1; Month <= 144; ++Month)
	{
		Weather.AdvanceToStep(Month * Tuning.FixedStepsPerMonth, City, Services, Tuning);
		const auto& Current = Weather.GetSnapshot();
		TestTrue(TEXT("wind never reverses in one authoritative change"),
		         FMath::Abs(static_cast<int32>(Current.WindDirection) -
		                    static_cast<int32>(Previous.WindDirection)) != 2);
		TestTrue(TEXT("wind strength only changes by one band"),
		         FMath::Abs(Current.WindStrength - Previous.WindStrength) <= 1);
		if (Current.Season == ECitySeason::Winter)
		{
			TestTrue(TEXT("winter has no hot random days"), Current.Temperature != ECityTemperatureBand::Hot);
			WetWinters += Current.bPrecipitation && FCityAtmosphereMotion::IsSnow(Current);
		}
		if (Current.Season == ECitySeason::Summer)
		{
			TestTrue(TEXT("summer has no cold random days"),
			         Current.Temperature != ECityTemperatureBand::Cold);
			WetSummers += Current.bPrecipitation;
		}
		Previous = Current;
	}
	TestTrue(TEXT("seeded climate includes summer rain and winter snow"), WetWinters > 0 && WetSummers > 0);
	FCityEnvironmentSimulation Restored;
	FString Error;
	TestTrue(TEXT("weather state restores"),
	         Restored.RestoreState(Previous, Previous.LastFixedStepId, Error));
	Weather.AdvanceToStep(150 * Tuning.FixedStepsPerMonth, City, Services, Tuning);
	Restored.AdvanceToStep(150 * Tuning.FixedStepsPerMonth, City, Services, Tuning);
	TestTrue(TEXT("save replay keeps weather and pollution deterministic"),
	         Weather.GetSnapshot().WeatherRandomState == Restored.GetSnapshot().WeatherRandomState &&
	             Weather.GetSnapshot().Pollution == Restored.GetSnapshot().Pollution &&
	             Weather.GetSnapshot().WindDirection == Restored.GetSnapshot().WindDirection);
	FCityAtmosphereMotion Motion;
	FCityEnvironmentSnapshot Target;
	Target.WindDirection = ECityWindDirection::North;
	Target.WindStrength = 2;
	Motion.Advance(.1, Target);
	TestTrue(TEXT("cosmetic direction progresses gradually"), Motion.WindYaw > 0 && Motion.WindYaw <= .9);
	TestTrue(TEXT("cosmetic strength eases"), Motion.WindStrength > 1 && Motion.WindStrength < 2);
	for (int32 I = 0; I < 200; ++I)
		Motion.Advance(.1, Target);
	TestTrue(TEXT("cosmetic wind settles without overshoot"),
	         FMath::IsNearlyEqual(Motion.WindYaw, 90.0) && Motion.WindStrength < 2);
	Motion.WindYaw = 359;
	Target.WindDirection = ECityWindDirection::East;
	Motion.Advance(.1, Target);
	TestTrue(TEXT("yaw wrap follows short direction"), Motion.WindYaw > 359 && Motion.WindYaw < 360);
	Target.Season = ECitySeason::Winter;
	Target.Temperature = ECityTemperatureBand::Cold;
	Target.bPrecipitation = true;
	Target.WindStrength = 2;
	TestTrue(TEXT("winter storm is named snowstorm"), Target.DescribeWeather().Contains(TEXT("SNOWSTORM")));
	Motion.Reset(Target);
	Target.Season = ECitySeason::Spring;
	Target.bPrecipitation = false;
	Motion.Advance(10, Target);
	TestTrue(TEXT("snow cover melts gradually"), Motion.SnowCover > 0 && Motion.SnowCover < 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityAtmospherePresentationTest,
                                 "CitySim.Presentation.AtmosphereSceneryAndLifecycle",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter |
                                     EAutomationTestFlags::NonNullRHI)
bool FCityAtmospherePresentationTest::RunTest(const FString& Parameters)
{
	const auto Values = UWorld::InitializationValues()
	                        .AllowAudioPlayback(false)
	                        .CreatePhysicsScene(false)
	                        .CreateNavigation(false)
	                        .CreateAISystem(false)
	                        .ShouldSimulatePhysics(false)
	                        .CreateFXSystem(true);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("CityAtmosphereTest"), nullptr, true,
	                                  ERHIFeatureLevel::Num, &Values);
	auto* Sim = World->GetSubsystem<UCitySimulationSubsystem>();
	auto* Board = World->SpawnActor<ACityBoardActor>();
	auto* Atmosphere = Board->Atmosphere.Get();
	Atmosphere->BeginPlay();
	Sim->SetSimulationPaused(false);
	const auto Heights = Sim->GetTerrain().GetQuantizedHeightsCm();
	const auto Revision = Sim->GetPlacementModel().GetRevision();
	Atmosphere->Update(Sim, 0);
	TestTrue(TEXT("bounded seeded trees appear"),
	         Atmosphere->GetTreeCount() > 0 && Atmosphere->GetTreeCount() <= Atmosphere->MaxTrees);
	TestEqual(TEXT("only static scenery uses bounded species/climate mesh pools"), Atmosphere->Pools.Num(),
	          UCityAtmosphereComponent::SceneryPoolCount);
	for (const auto& Pool : Atmosphere->Pools)
	{
		TestNotNull(TEXT("pooled environment mesh is imported"), Pool->GetStaticMesh().Get());
		TestTrue(TEXT("scenery and weather have no placement collision"),
		         Pool->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
	}
	TestTrue(TEXT("presentation cannot alter terrain or authority"),
	         Heights == Sim->GetTerrain().GetQuantizedHeightsCm() &&
	             Revision == Sim->GetPlacementModel().GetRevision());
	TSet<FCityCellCoord> OriginalScenery;
	for (const int32 Type : {0, 1, 3, 4})
		for (int32 I = 0; I < Atmosphere->Pools[Type]->GetInstanceCount(); ++I)
		{
			FTransform T;
			Atmosphere->Pools[Type]->GetInstanceTransform(I, T);
			const auto P = T.GetLocation();
			OriginalScenery.Add({FMath::FloorToInt32(P.X / CityBoardPicking::LogicalCellSizeCm),
			                     FMath::FloorToInt32(P.Y / CityBoardPicking::LogicalCellSizeCm)});
		}
	FTransform Tree;
	Atmosphere->Pools[0]->GetInstanceTransform(0, Tree);
	const auto Position = Tree.GetLocation();
	const FCityCellCoord Cell(FMath::FloorToInt32(Position.X / CityBoardPicking::LogicalCellSizeCm),
	                          FMath::FloorToInt32(Position.Y / CityBoardPicking::LogicalCellSizeCm));
	auto& City = Sim->GetPlacementModel();
	auto Road = City.BeginRoadPreview(Cell);
	TestTrue(TEXT("tree never rejects road placement"), City.CommitRoad(Road).IsSuccess());
	Atmosphere->Update(Sim, 0);
	for (const int32 Type : {0, 1, 3, 4})
		for (int32 I = 0; I < Atmosphere->Pools[Type]->GetInstanceCount(); ++I)
		{
			FTransform T;
			Atmosphere->Pools[Type]->GetInstanceTransform(I, T);
			const auto P = T.GetLocation();
			TestTrue(TEXT("scenery clears the road and cell margin"),
			         FMath::Abs(P.X / CityBoardPicking::LogicalCellSizeCm - Cell.X - .5) > 1.1 ||
			             FMath::Abs(P.Y / CityBoardPicking::LogicalCellSizeCm - Cell.Y - .5) > 1.1);
			TestTrue(
			    TEXT("clearing scenery does not regrow replacements elsewhere"),
			    OriginalScenery.Contains({FMath::FloorToInt32(P.X / CityBoardPicking::LogicalCellSizeCm),
			                              FMath::FloorToInt32(P.Y / CityBoardPicking::LogicalCellSizeCm)}));
		}
	FString Error;
	auto Weather = Sim->Environment.GetSnapshot();
	Weather.bPrecipitation = true;
	Weather.WindStrength = 2;
	Weather.Season = ECitySeason::Summer;
	Weather.Temperature = ECityTemperatureBand::Hot;
	Sim->Environment.RestoreState(Weather, 0, Error);
	++Sim->CitySessionGeneration;
	Atmosphere->Update(Sim, 0);
	TestEqual(TEXT("two reusable native weather systems"), Atmosphere->WeatherSystems.Num(), 2);
	TestEqual(TEXT("Rain crosses the layer in the requested half-time"),
	          Atmosphere->WeatherSystems[0]->WeatherVelocity.Z, -1900.f / .35f);
	for (const auto& System : Atmosphere->WeatherSystems)
	{
		if (!TestNotNull(TEXT("weather particle asset exists"), System->Template.Get()))
		{
			World->DestroyWorld(false);
			return false;
		}
		TestTrue(TEXT("weather uses bounded fixed culling bounds and no warmup"),
		         System->Template->bUseFixedRelativeBoundingBox && System->Template->WarmupTime == 0);
		for (const auto& Emitter : System->Template->Emitters)
		{
			const auto* LOD = Emitter->LODLevels[0].Get();
			const auto* Native = Cast<UCityWeatherParticleModule>(LOD->Modules[0]);
			const auto* Rate = Cast<UDistributionFloatConstant>(LOD->SpawnModule->Rate.Distribution);
			if (TestNotNull(TEXT("Native weather module exists"), Native) &&
			    TestNotNull(TEXT("Authored weather spawn rate exists"), Rate))
			{
				const float Lifetime = Native->Kind == 0 ? .35f : Native->Kind == 1 ? 5.f : .4f;
				const int32 Cap = Native->Kind == 2 ? 48 : 320;
				TestEqual(TEXT("Cooked spawn rate matches lifetime without raising the live particle budget"),
				          Rate->Constant * Lifetime, static_cast<float>(Cap));
			}
			TestNull(TEXT("weather sprite emitter has no mesh type data"), LOD->TypeDataModule.Get());
			TestTrue(TEXT("weather sprites follow board movement/scale"),
			         LOD->RequiredModule->bUseLocalSpace);
			TestTrue(TEXT("native emitter has a rendering cap"), LOD->RequiredModule->bUseMaxDrawCount);
#if WITH_EDITOR
			const auto* Material = Cast<UMaterial>(LOD->RequiredModule->Material.Get());
			if (TestNotNull(TEXT("sprite material exists"), Material))
			{
				const auto* Opacity =
				    Cast<UMaterialExpressionMultiply>(Material->GetEditorOnlyData()->Opacity.Expression);
				if (TestNotNull(TEXT("weather material has an opacity input"), Opacity))
					TestTrue(TEXT("sprite opacity receives particle alpha and its mask (disconnected alpha "
					              "makes every particle invisible)"),
					         Opacity->A.Expression &&
					             Opacity->A.Expression->IsA<UMaterialExpressionParticleColor>() &&
					             Opacity->A.MaskA && Opacity->B.Expression);
			}
#endif
		}
	}
	auto StepParticles = [](UCityWeatherParticleComponent* System, float Seconds) {
		System->TickComponent(Seconds, LEVELTICK_All, nullptr);
		// This isolated world is not driven by the engine's world particle manager.
		// Exercise its normal prepare/simulate/finalize phases explicitly.
		if (System->IsTickManaged())
		{
			System->MarshalParamsForAsyncTick();
			System->ComputeTickComponent_Concurrent();
			System->FinalizeTickComponent();
		}
	};
	auto TickWeather = [&](int32 Frames) {
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			Atmosphere->Update(Sim, 1.0f / 60);
			for (const auto& System : Atmosphere->WeatherSystems)
				if (System->IsComponentTickEnabled())
					StepParticles(System, 1.0f / 60);
			TestTrue(TEXT("live precipitation remains bounded throughout spawning"),
			         Atmosphere->GetPrecipitationCount() <= Atmosphere->MaxPrecipitation);
		}
	};
	TickWeather(180);
	AddInfo(FString::Printf(TEXT("Native rain: %d live drops, %d total, %d emitters"),
	                        Atmosphere->GetPrecipitationCount(),
	                        Atmosphere->WeatherSystems[0]->GetNumActiveParticles(),
	                        Atmosphere->WeatherSystems[0]->EmitterInstances.Num()));
	TestTrue(TEXT("rainstorm spawns real engine particles"), Atmosphere->GetPrecipitationCount() > 100);
	TestFalse(TEXT("summer rain has no active snow system"), Atmosphere->WeatherSystems[1]->IsActive());
	TestTrue(TEXT("rain has bounded native splashes"),
	         Atmosphere->WeatherSystems[0]->GetNumActiveParticles() - Atmosphere->GetPrecipitationCount() <=
	             Atmosphere->MaxSplashes);
	StepParticles(Atmosphere->WeatherSystems[0], .75f);
	TestTrue(TEXT("a long frame cannot exceed the hard spawn cap"),
	         Atmosphere->GetPrecipitationCount() <= Atmosphere->MaxPrecipitation);
	Weather.Season = ECitySeason::Winter;
	Weather.Temperature = ECityTemperatureBand::Cold;
	Sim->Environment.RestoreState(Weather, 0, Error);
	++Sim->CitySessionGeneration;
	Atmosphere->Update(Sim, 0);
	TickWeather(420);
	AddInfo(FString::Printf(TEXT("Native snow: %d live flakes, %d emitters"),
	                        Atmosphere->GetPrecipitationCount(),
	                        Atmosphere->WeatherSystems[1]->EmitterInstances.Num()));
	TestTrue(TEXT("winter storm spawns real native snow particles"),
	         Atmosphere->GetPrecipitationCount() > 100);
	TestFalse(TEXT("snow disables the rain/splash system"), Atmosphere->WeatherSystems[0]->IsActive());
	TestEqual(TEXT("season switch kills every old rain/splash particle"),
	          Atmosphere->WeatherSystems[0]->GetNumActiveParticles(), 0);
	Sim->SetSimulationPaused(true);
	const double Phase = Atmosphere->Motion.PhaseSeconds;
	Atmosphere->Update(Sim, 2);
	TestEqual(TEXT("pause freezes cosmetic weather"), Atmosphere->Motion.PhaseSeconds, Phase);
	const int32 FrozenCount = Atmosphere->GetPrecipitationCount();
	TickWeather(30);
	TestEqual(TEXT("pause stops native spawning and simulation"), Atmosphere->GetPrecipitationCount(),
	          FrozenCount);
	TestFalse(TEXT("pause disables particle component tick"),
	          Atmosphere->WeatherSystems[1]->IsComponentTickEnabled());
	Sim->SetEntryMenuSuspended(true);
	Atmosphere->Update(Sim, 1);
	TestFalse(TEXT("menu hides precipitation"), Atmosphere->WeatherSystems[1]->IsVisible());
	TestFalse(TEXT("menu stops particle tick"), Atmosphere->WeatherSystems[1]->IsComponentTickEnabled());
	Sim->SetEntryMenuSuspended(false);
	Sim->SetSimulationPaused(false);
	TickWeather(1);
	TestTrue(TEXT("returning to city resumes particles"),
	         Atmosphere->WeatherSystems[1]->IsVisible() &&
	             Atmosphere->WeatherSystems[1]->IsComponentTickEnabled());
	Weather.bPrecipitation = false;
	Sim->Environment.RestoreState(Weather, 0, Error);
	++Sim->CitySessionGeneration;
	Atmosphere->Update(Sim, 0);
	TestEqual(TEXT("loading a clear city clears old storm instances"), Atmosphere->GetPrecipitationCount(),
	          0);
	for (const auto& System : Atmosphere->WeatherSystems)
		TestFalse(TEXT("clear weather does not tick a particle system"), System->IsComponentTickEnabled());
	Atmosphere->EndPlay(EEndPlayReason::Destroyed);
	TestTrue(TEXT("end play destroys weather and scenery components"),
	         Atmosphere->WeatherSystems.IsEmpty() && Atmosphere->Pools.IsEmpty());
	World->DestroyWorld(false);
	return true;
}
#endif
