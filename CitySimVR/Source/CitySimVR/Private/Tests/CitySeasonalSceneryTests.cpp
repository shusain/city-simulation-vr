#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Presentation/CitySeasonalScenery.h"
#include "Presentation/CityAtmosphereComponent.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Interaction/CityBoardPicking.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCitySeasonalSceneryPolicyTest,
                                 "CitySim.Presentation.SeasonalScenerySpeciesAndClimate",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCitySeasonalSceneryPolicyTest::RunTest(const FString& Parameters)
{
	for (const int32 Warmth : {0, 30, 55, 64, 65, 80, 100})
	{
		TestTrue(TEXT("ordinary broadleaf turns in autumn across cool and warm climates"),
		         FCitySeasonalScenery::BroadleafVariant(Warmth, ECitySeason::Autumn) ==
		             ECitySeasonalTreeVariant::Autumn);
		TestTrue(TEXT("ordinary broadleaf loses leaves in winter across cool and warm climates"),
		         FCitySeasonalScenery::BroadleafVariant(Warmth, ECitySeason::Winter) == ECitySeasonalTreeVariant::Bare);
		TestTrue(
		    TEXT("spring and summer retain leafy identity"),
		    FCitySeasonalScenery::BroadleafVariant(Warmth, ECitySeason::Spring) == ECitySeasonalTreeVariant::Leafy &&
		        FCitySeasonalScenery::BroadleafVariant(Warmth, ECitySeason::Summer) == ECitySeasonalTreeVariant::Leafy);
	}
	FCityEnvironmentSnapshot Weather;
	Weather.Season = ECitySeason::Winter;
	Weather.Temperature = ECityTemperatureBand::Cold;
	TestFalse(TEXT("winter date alone cannot put snow on conifers"),
	          FCitySeasonalScenery::ConiferSnowMesh(Weather, 0, false));
	TestFalse(TEXT("early accumulation remains the original conifer"),
	          FCitySeasonalScenery::ConiferSnowMesh(Weather, .119, false));
	TestTrue(TEXT("accumulation crosses into snow mesh"), FCitySeasonalScenery::ConiferSnowMesh(Weather, .12, false));
	TestTrue(TEXT("small decline stays snowy rather than repeatedly swapping"),
	         FCitySeasonalScenery::ConiferSnowMesh(Weather, .09, true));
	TestFalse(TEXT("settled melt restores the original evergreen"),
	          FCitySeasonalScenery::ConiferSnowMesh(Weather, .059, true));
	Weather.Temperature = ECityTemperatureBand::Mild;
	TestFalse(TEXT("mild winter stays evergreen without white caps"),
	          FCitySeasonalScenery::ConiferSnowMesh(Weather, 1, true));
	Weather.Temperature = ECityTemperatureBand::Cold;
	Weather.Season = ECitySeason::Summer;
	TestFalse(TEXT("summer cannot retain a winter snow mesh"), FCitySeasonalScenery::ConiferSnowMesh(Weather, 1, true));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCitySeasonalSceneryPresentationTest,
                                 "CitySim.Presentation.SeasonalSceneryFrozenTransformsAndLifecycle",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter |
                                     EAutomationTestFlags::NonNullRHI)
bool FCitySeasonalSceneryPresentationTest::RunTest(const FString& Parameters)
{
	const auto Init = UWorld::InitializationValues()
	                      .AllowAudioPlayback(false)
	                      .CreatePhysicsScene(false)
	                      .CreateNavigation(false)
	                      .CreateAISystem(false)
	                      .ShouldSimulatePhysics(false)
	                      .CreateFXSystem(true);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("SeasonalSceneryTest"), nullptr, true,
	                                  ERHIFeatureLevel::Num, &Init);
	auto* Simulation = World->GetSubsystem<UCitySimulationSubsystem>();
	auto* Owner = World->SpawnActor<AActor>();
	auto* Root = NewObject<USceneComponent>(Owner);
	Owner->SetRootComponent(Root);
	Owner->AddInstanceComponent(Root);
	Root->RegisterComponent();
	auto* Scenery = NewObject<UCityAtmosphereComponent>(Owner);
	Owner->AddInstanceComponent(Scenery);
	Scenery->RegisterComponent();
	Scenery->BeginPlay();
	const FString FixturePath = FPaths::ProjectSavedDir() / TEXT("Automation/SeasonalScenery.json");
	auto* Catalog = LoadObject<UCityBuildingCatalog>(
	    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
	Simulation->RegisterBuildingCatalog(Catalog);
	FCitySaveDocument Document;
	Document.Placement = FCityPlacementModel(10000).ExportState();
	Document.bSimulationPaused = true;
	Document.Terrain.GeneratorVersion = 2;
	Document.Terrain.SceneryVersion = 1;
	bool bMixedClimate = false;
	for (int32 Seed = 1; Seed <= 128; ++Seed)
	{
		Document.Terrain.Seed = Seed;
		FCityTerrainHeightfield Terrain;
		Terrain.Generate(Document.Terrain);
		FCitySceneryField Climate;
		Climate.Build(Terrain);
		const auto Candidates =
		    FCitySceneryField::SelectCandidates(Terrain, Document.Terrain.GetEntranceCell(), 420, 100);
		int32 CoolBroadleaf = 0, WarmBroadleaf = 0, Palms = 0;
		for (const auto& Candidate : Candidates)
		{
			CoolBroadleaf +=
			    Candidate.Kind == ECitySceneryKind::Broadleaf &&
			    Climate.GetSample(Candidate.Cell).WarmthPercent < FCitySeasonalScenery::WarmBroadleafPoolThreshold;
			WarmBroadleaf +=
			    Candidate.Kind == ECitySceneryKind::Broadleaf &&
			    Climate.GetSample(Candidate.Cell).WarmthPercent >= FCitySeasonalScenery::WarmBroadleafPoolThreshold;
			Palms += Candidate.Kind == ECitySceneryKind::Palm;
		}
		if (CoolBroadleaf > 0 && WarmBroadleaf > 0 && Palms > 0)
		{
			bMixedClimate = true;
			break;
		}
	}
	FString Json, Error;
	auto LoadFixture = [&]()
	{
		return FCityPersistence::Serialize(Document, Json, Error) &&
		       FCityPersistence::WriteAtomic(FixturePath, Json, Error) &&
		       Simulation->LoadCityFromPath(FixturePath, Error, false);
	};
	do
	{
		if (!TestNotNull(TEXT("native building catalog supplies isolated city loads"), Catalog))
			break;
		if (!TestTrue(TEXT("fixture contains cool broadleaf, warm broadleaf and palms together"), bMixedClimate))
			break;
		if (!TestTrue(TEXT("isolated mixed-climate city loads"), LoadFixture()))
		{
			AddError(Error);
			break;
		}
		Scenery->Update(Simulation, 0);
		TestEqual(TEXT("existing climate grouping retains the same five bounded pools"), Scenery->Pools.Num(), 5);
		TestTrue(TEXT("real visible fixture exercises both cool and warm ordinary trees"),
		         Scenery->Pools[0]->GetInstanceCount() > 0 && Scenery->Pools[4]->GetInstanceCount() > 0);
		TestTrue(TEXT("combined species retain the 420 tree and 100 rock caps"),
		         Scenery->GetTreeCount() > 0 && Scenery->GetTreeCount() <= 420 &&
		             Scenery->Pools[2]->GetInstanceCount() <= 100);
		for (const auto& Mesh : Scenery->SeasonalMeshes)
			TestNotNull(TEXT("seasonal mesh is imported"), Mesh.Get());
		if (Scenery->SeasonalMeshes.Contains(nullptr))
			break;
		TestEqual(TEXT("bare winter tree has only bark material"),
		          Scenery->SeasonalMeshes[1]->GetStaticMaterials().Num(), 1);
		TestEqual(TEXT("snow pine separates bark, evergreen canopy and snow"),
		          Scenery->SeasonalMeshes[2]->GetStaticMaterials().Num(), 3);
		const auto Candidates = Scenery->SceneryCandidates;
		const auto Warmth = Scenery->SceneryWarmth;
		const auto Heights = Simulation->GetTerrain().GetQuantizedHeightsCm();
		const uint64 Revision = Simulation->GetPlacementModel().GetRevision();
		TArray<TArray<FTransform>> Frozen;
		for (const auto& Pool : Scenery->Pools)
		{
			TArray<FTransform> Transforms;
			for (int32 Index = 0; Index < Pool->GetInstanceCount(); ++Index)
			{
				FTransform Transform;
				Pool->GetInstanceTransform(Index, Transform);
				Transforms.Add(Transform);
			}
			Frozen.Add(MoveTemp(Transforms));
		}
		// Match every visible instance to the exact pre-SeasonalScenery hash/terrain transform.
		for (int32 PoolIndex = 0; PoolIndex < Frozen.Num(); ++PoolIndex)
			for (const auto& Transform : Frozen[PoolIndex])
			{
				const FVector Position = Transform.GetLocation();
				const FCityCellCoord Cell(FMath::FloorToInt32(Position.X / CityBoardPicking::LogicalCellSizeCm),
				                          FMath::FloorToInt32(Position.Y / CityBoardPicking::LogicalCellSizeCm));
				const auto* Candidate = Candidates.FindByPredicate([&](const auto& Item) { return Item.Cell == Cell; });
				if (!TestNotNull(TEXT("visible tree/rock retains a frozen candidate"), Candidate))
					continue;
				const uint32 Hash = Candidate->Hash;
				const double X = Cell.X + .3 + ((Hash >> 8) % 40) / 100.0,
				             Y = Cell.Y + .3 + ((Hash >> 16) % 40) / 100.0;
				const double Scale = .65 + (Hash >> 24) / 255.0 * .65;
				const FTransform Expected(FRotator(0, Hash % 360, 0),
				                          FVector(X * CityBoardPicking::LogicalCellSizeCm,
				                                  Y * CityBoardPicking::LogicalCellSizeCm,
				                                  Simulation->GetTerrain().SampleHeightCm(X, Y)),
				                          FVector(Scale));
				TestTrue(TEXT("geometry variants preserve exact original position, yaw and scale"),
				         Transform.Equals(Expected, .00001));
				TestTrue(TEXT("pool splitting preserves species"),
				         static_cast<int32>(Candidate->Kind) == (PoolIndex == 4 ? 0 : PoolIndex));
			}
		auto CheckFrozen = [&]()
		{
			for (int32 PoolIndex = 0; PoolIndex < Frozen.Num(); ++PoolIndex)
			{
				TestEqual(TEXT("season changes cannot change per-climate instance counts"),
				          Scenery->Pools[PoolIndex]->GetInstanceCount(), Frozen[PoolIndex].Num());
				for (int32 Index = 0; Index < Frozen[PoolIndex].Num(); ++Index)
				{
					FTransform Transform;
					Scenery->Pools[PoolIndex]->GetInstanceTransform(Index, Transform);
					TestTrue(TEXT("season changes cannot move or reroll an instance"),
					         Transform.Equals(Frozen[PoolIndex][Index], .00001));
				}
			}
			TestTrue(TEXT("candidate species and cached initial climate are frozen"),
			         Scenery->SceneryCandidates == Candidates && Scenery->SceneryWarmth == Warmth);
		};
		for (const auto Season : {ECitySeason::Autumn, ECitySeason::Winter, ECitySeason::Spring, ECitySeason::Summer})
		{
			FCityEnvironmentSnapshot Weather;
			Weather.Season = Season;
			Weather.Temperature =
			    Season == ECitySeason::Winter ? ECityTemperatureBand::Cold : ECityTemperatureBand::Mild;
			Scenery->Motion.SnowCover = Season == ECitySeason::Winter ? .8 : 0;
			Scenery->UpdateSeasonalScenery(Weather);
			UStaticMesh* Expected = Season == ECitySeason::Autumn   ? Scenery->SeasonalMeshes[0].Get()
			                        : Season == ECitySeason::Winter ? Scenery->SeasonalMeshes[1].Get()
			                                                        : Scenery->Meshes[0].Get();
			for (const int32 PoolIndex : {0, 4})
			{
				TestTrue(TEXT("ordinary geometry follows autumn/bare/regrowth in cool and warm pools"),
				         Scenery->Pools[PoolIndex]->GetStaticMesh() == Expected);
				TestTrue(TEXT("bark material is never replaced with canopy tint"),
				         Scenery->Pools[PoolIndex]->GetMaterial(0) == Expected->GetMaterial(0));
				TestTrue(TEXT("both broadleaf pools receive the seasonal canopy color"),
				         Scenery->Materials[PoolIndex]
				             ->K2_GetVectorParameterValue(TEXT("CityColor"))
				             .Equals(FCitySeasonalScenery::BroadleafColor(
				                 FCitySeasonalScenery::BroadleafVariant(0, Season))));
				TestTrue(TEXT("bare trees have no canopy override; regrowth rebinds shared foliage"),
				         Season == ECitySeason::Winter
				             ? Scenery->Pools[PoolIndex]->GetMaterial(1) == nullptr
				             : Scenery->Pools[PoolIndex]->GetMaterial(1) == Scenery->Materials[PoolIndex]);
			}
			TestTrue(TEXT("palms keep their original leafy mesh in every season"),
			         Scenery->Pools[3]->GetStaticMesh() == Scenery->Meshes[3]);
			TestTrue(TEXT("snow caps retain a separate authored snow material"),
			         Season != ECitySeason::Winter ||
			             Scenery->Pools[1]->GetMaterial(2) == Scenery->SeasonalMeshes[2]->GetMaterial(2));
			TestTrue(TEXT("conifer evergreen canopy material remains stable"),
			         Scenery->Pools[1]->GetMaterial(1) == Scenery->Materials[1]);
			CheckFrozen();
		}
		Scenery->Update(Simulation, 0);
		TestTrue(TEXT("spring regrowth restores green ordinary foliage and palms remain green"),
		         Scenery->Materials[0]->K2_GetVectorParameterValue(TEXT("CityColor")).G >
		                 Scenery->Materials[0]->K2_GetVectorParameterValue(TEXT("CityColor")).R &&
		             Scenery->Materials[4]->K2_GetVectorParameterValue(TEXT("CityColor")).G >
		                 Scenery->Materials[4]->K2_GetVectorParameterValue(TEXT("CityColor")).R &&
		             Scenery->Materials[3]->K2_GetVectorParameterValue(TEXT("CityColor")).G >
		                 Scenery->Materials[3]->K2_GetVectorParameterValue(TEXT("CityColor")).R);
		TestTrue(TEXT("visual swaps never alter terrain or command revision"),
		         Heights == Simulation->GetTerrain().GetQuantizedHeightsCm() &&
		             Revision == Simulation->GetPlacementModel().GetRevision());
		Simulation->SetEntryMenuSuspended(true);
		Scenery->Update(Simulation, 0);
		for (const auto& Pool : Scenery->Pools)
			TestFalse(TEXT("menu hides every seasonal pool"), Pool->IsVisible());
		Simulation->SetEntryMenuSuspended(false);
		Scenery->Update(Simulation, 0);
		for (const auto& Pool : Scenery->Pools)
			TestTrue(TEXT("city return restores all scenery pools"), Pool->IsVisible());
		CheckFrozen();
		Document.Environment.Season = ECitySeason::Winter;
		Document.Environment.Temperature = ECityTemperatureBand::Cold;
		Document.Environment.bPrecipitation = true;
		if (!TestTrue(TEXT("loading snowy city succeeds"), LoadFixture()))
			break;
		Scenery->Update(Simulation, 0);
		CheckFrozen();
		TestTrue(TEXT("loaded winter selects bare broadleaf across climates and snow pine immediately"),
		         Scenery->Pools[0]->GetStaticMesh() == Scenery->SeasonalMeshes[1] &&
		             Scenery->Pools[4]->GetStaticMesh() == Scenery->SeasonalMeshes[1] &&
		             Scenery->Pools[1]->GetStaticMesh() == Scenery->SeasonalMeshes[2]);
		Simulation->SetEntryMenuSuspended(true);
		Scenery->Update(Simulation, 0);
		Simulation->SetEntryMenuSuspended(false);
		Scenery->Update(Simulation, 0);
		CheckFrozen();
		TestTrue(TEXT("winter menu return cannot restore warm green trees or stale foliage"),
		         Scenery->Pools[0]->GetStaticMesh() == Scenery->SeasonalMeshes[1] &&
		             Scenery->Pools[4]->GetStaticMesh() == Scenery->SeasonalMeshes[1] &&
		             Scenery->Pools[0]->GetMaterial(1) == nullptr && Scenery->Pools[4]->GetMaterial(1) == nullptr);
		Document.Environment.Season = ECitySeason::Spring;
		Document.Environment.Temperature = ECityTemperatureBand::Mild;
		Document.Environment.bPrecipitation = false;
		if (!TestTrue(TEXT("reloading spring regrowth succeeds"), LoadFixture()))
			break;
		Scenery->Update(Simulation, 0);
		CheckFrozen();
		TestTrue(TEXT("spring reload regrows both ordinary tree groups with foliage bound"),
		         Scenery->Pools[0]->GetStaticMesh() == Scenery->Meshes[0] &&
		             Scenery->Pools[4]->GetStaticMesh() == Scenery->Meshes[0] &&
		             Scenery->Pools[0]->GetMaterial(1) == Scenery->Materials[0] &&
		             Scenery->Pools[4]->GetMaterial(1) == Scenery->Materials[4]);
		Document.Terrain.SceneryVersion = 0;
		if (!TestTrue(TEXT("legacy city remains loadable"), LoadFixture()))
			break;
		Scenery->Update(Simulation, 0);
		const auto Legacy = FCitySceneryField::SelectCandidates(
		    Simulation->GetTerrain(), Simulation->GetPlacementModel().GetEntranceCell(), 420, 100);
		TestTrue(TEXT("legacy candidate species and locations remain exact"), Scenery->SceneryCandidates == Legacy);
		TestEqual(TEXT("legacy scenery does not gain palms"), Scenery->Pools[3]->GetInstanceCount(), 0);
		Scenery->SeasonalMeshes[0] = nullptr;
		Scenery->SeasonalMeshes[1] = nullptr;
		Scenery->SeasonalMeshes[2] = nullptr;
		FCityEnvironmentSnapshot MissingVariantWinter;
		MissingVariantWinter.Season = ECitySeason::Winter;
		MissingVariantWinter.Temperature = ECityTemperatureBand::Cold;
		Scenery->Motion.SnowCover = .8;
		Scenery->UpdateSeasonalScenery(MissingVariantWinter);
		TestTrue(TEXT("missing seasonal assets fall back to the original usable trees in both climates"),
		         Scenery->Pools[0]->GetStaticMesh() == Scenery->Meshes[0] &&
		             Scenery->Pools[4]->GetStaticMesh() == Scenery->Meshes[0] &&
		             Scenery->Pools[1]->GetStaticMesh() == Scenery->Meshes[1]);
	} while (false);
	Scenery->EndPlay(EEndPlayReason::Destroyed);
	TestTrue(TEXT("end play clears all pools and frozen climate cache"),
	         Scenery->Pools.IsEmpty() && Scenery->SceneryWarmth.IsEmpty() && Scenery->SceneryCandidates.IsEmpty());
	IFileManager::Get().Delete(*FixturePath);
	IFileManager::Get().Delete(*(FixturePath + TEXT(".bak")));
	World->DestroyWorld(false);
	return true;
}
#endif
