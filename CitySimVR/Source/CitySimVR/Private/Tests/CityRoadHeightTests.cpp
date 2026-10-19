#if WITH_DEV_AUTOMATION_TESTS
#include "Interaction/CityRoadHeightControl.h"
#include "Commands/CityPlacementModel.h"
#include "Simulation/CityTerrain.h"
#include "Persistence/CityPersistence.h"
#include "Misc/AutomationTest.h"
#include "Presentation/CityBoardActor.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityRoadHeightTest, "CitySim.Roads.HeightStepsAndSupports",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityRoadHeightTest::RunTest(const FString& Parameters)
{
	FCityRoadHeightControl Control;
	TestEqual(TEXT("Stick held on entry does not change height"), Control.Update(1, true), 0);
	Control.Update(0, true);
	TestEqual(TEXT("Up flick produces one step"), Control.Update(.8, true), 1);
	TestEqual(TEXT("Holding does not repeat"), Control.Update(1, true), 0);
	TestEqual(TEXT("Opposite without neutral does not repeat"), Control.Update(-1, true), 0);
	Control.Update(.2, true);
	TestEqual(TEXT("Down after neutral produces one step"), Control.Update(-.8, true), -1);
	Control.Update(0, false);
	TestEqual(TEXT("Held input on resume is ignored"), Control.Update(1, true), 0);
	Control.Update(0, true);
	TestEqual(TEXT("Small deflection ignored"), Control.Update(.4, true), 0);
	TestEqual(TEXT("Crossing threshold changes once"), Control.Update(.71, true), 1);
	FCityTerrainGenerationParameters Config;
	Config.BroadElevationAmplitudeCm = 0;
	Config.HillAmplitudeCm = 0;
	Config.RidgeAmplitudeCm = 0;
	Config.StarterHalfWidthCells = 16;
	Config.StarterDepthCells = 24;
	FCityTerrainHeightfield Terrain;
	Terrain.Generate(Config);
	FString Error;
	TMap<FName, FCityBuildingDefinition> Definitions;
	for (int32 Step = 0; Step <= FCityPlacementModel::MaximumRoadElevationStep; ++Step)
	{
		FCityPlacementModel City(100000);
		City.SetTerrain(&Terrain);
		auto P = City.BeginRoadPreview({5, 1});
		P.End = {5, 23};
		P.Profile = ECityRoadProfile::Elevated;
		P.ElevationStep = Step;
		auto R = City.ValidateRoad(P);
		TestTrue(TEXT("Long dry span fits every height step"), R.IsSuccess());
		if (!R.IsSuccess())
		{
			AddError(R.Detail);
			continue;
		}
		TestEqual(TEXT("Height rises by exact two-metre steps"), R.Structure.DeckHeightCm, 600 + Step * 200);
		FCityRoadStructure Legacy;
		TestTrue(TEXT("Legacy terrain-relative geometry can be reconstructed"),
		         City.MakeRoadStructure(P, Legacy, Error, false));
		TestEqual(TEXT("Snapping never reduces clearance"), R.Structure.DeckHeightCm - Legacy.DeckHeightCm,
		          120);
		TestEqual(TEXT("Deck is an absolute even metre"), R.Structure.DeckHeightCm % 200, 0);
		TestEqual(TEXT("Extra height charges a bounded per-cell surcharge"), R.Structure.RecordedCost,
		          static_cast<int64>(21 * (50 + Step * 20)));
		TestTrue(TEXT("Height preview commits"), City.CommitRoad(P).IsSuccess());
		for (int32 I = 1; I < 22; ++I)
			TestEqual(TEXT("Supports alternate; intervening cell has no column"),
			          City.HasRoadSupport({5, 1 + I, 1}), I % 2 == 1);
		FCitySaveDocument D;
		D.Placement = City.ExportState();
		D.Terrain = Config;
		FString Json;
		FCityPersistence::Serialize(D, Json, Error);
		FCitySaveDocument Loaded;
		TestTrue(TEXT("Chosen height parses"), FCityPersistence::Deserialize(Json, Loaded, Error));
		auto Restored = FCityPlacementModel::CreateValidatedFromState(Loaded.Placement, Definitions, Error);
		TestTrue(TEXT("Chosen height restores"), Restored.IsValid());
		if (Restored)
		{
			Restored->SetTerrain(&Terrain);
			TestTrue(TEXT("Chosen height revalidates against terrain"),
			         Restored->ValidateRoadStructures(Error));
			TestEqual(TEXT("Height step survives save"), Restored->GetRoadStructures()[0].ElevationStep,
			          Step);
		}
		auto LegacyState = City.ExportState();
		LegacyState.RoadStructures[0] = Legacy;
		auto LegacyCity = FCityPlacementModel::CreateValidatedFromState(LegacyState, Definitions, Error);
		if (TestTrue(TEXT("Existing fractional deck restores"), LegacyCity.IsValid()))
		{
			LegacyCity->SetTerrain(&Terrain);
			TestTrue(TEXT("Legacy geometry remains valid without moving it"),
			         LegacyCity->ValidateRoadStructures(Error));
			TestEqual(TEXT("Legacy deck is preserved"), LegacyCity->GetRoadStructures()[0].DeckHeightCm,
			          480 + Step * 200);
		}
		P = City.BeginRoadPreview({6, 1});
		P.End = {6, 23};
		P.Profile = ECityRoadProfile::Elevated;
		P.ElevationStep = 5;
		TestFalse(TEXT("Out-of-range height cannot commit"), City.CommitRoad(P).IsSuccess());
	}
	const auto Values = UWorld::InitializationValues()
	                        .AllowAudioPlayback(false)
	                        .CreatePhysicsScene(false)
	                        .CreateNavigation(false)
	                        .CreateAISystem(false)
	                        .ShouldSimulatePhysics(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("CityRoadHeightPreview"), nullptr, true,
	                                    ERHIFeatureLevel::Num, &Values);
	auto* Board = World->SpawnActor<ACityBoardActor>();
	FCityPlacementModel City(100000);
	City.SetTerrain(&Terrain);
	Board->PlacementModel = &City;
	Board->SetSelectedRoadProfile(ECityRoadProfile::Elevated);
	Board->RoadPreview = City.BeginRoadPreview({5, 1});
	Board->RoadPreview.End = {5, 23};
	Board->RoadPreview.Profile = ECityRoadProfile::Elevated;
	Board->AdjustRoadElevation(1);
	TestTrue(TEXT("Height adjustment preserves active preview"), Board->RoadPreview.bActive);
	TestTrue(TEXT("Height adjustment preserves start and end"),
	         Board->RoadPreview.Start == FCityCellCoord(5, 1) &&
	             Board->RoadPreview.End == FCityCellCoord(5, 23));
	TestEqual(TEXT("Pending command uses selected height"), Board->RoadPreview.ElevationStep, 1);
	for (int32 I = 0; I < 10; ++I)
		Board->AdjustRoadElevation(1);
	TestEqual(TEXT("Height selection is bounded"), Board->GetSelectedRoadElevationStep(), 4);
	for (int32 I = 0; I < 10; ++I)
		Board->AdjustRoadElevation(-1);
	TestEqual(TEXT("Cannot lower below safe automatic height"), Board->GetSelectedRoadElevationStep(), 0);
	TestEqual(TEXT("Changing preview never spends treasury"), City.GetFunds(), static_cast<int64>(100000));
	Board->SetSelectedRoadProfile(ECityRoadProfile::Ground);
	Board->RoadPreview = City.BeginRoadPreview({5, 1});
	Board->RoadPreview.End = {5, 23};
	Board->RefreshRoadDrawingProfile();
	TestTrue(TEXT("Dry drawing starts on ground"), Board->RoadPreview.Profile == ECityRoadProfile::Ground);
	Board->AdjustRoadElevation(1);
	TestTrue(TEXT("First up flick raises a dry road with safe automatic ramps"),
	         Board->RoadPreview.Profile == ECityRoadProfile::Elevated &&
	             Board->RoadPreview.ElevationStep == 0);
	Board->AdjustRoadElevation(-1);
	TestTrue(TEXT("Down from automatic elevated returns dry road to ground"),
	         Board->RoadPreview.Profile == ECityRoadProfile::Ground);
	TestTrue(TEXT("Changing treatment retains endpoints"),
	         Board->RoadPreview.Start == FCityCellCoord(5, 1) &&
	             Board->RoadPreview.End == FCityCellCoord(5, 23));
	Board->RoadPreview.Start = {58, 10};
	Board->RoadPreview.End = {58, 62};
	Board->RefreshRoadDrawingProfile();
	TestTrue(TEXT("Regular road crossing water becomes bridge"),
	         Board->RoadPreview.Profile == ECityRoadProfile::Bridge);
	const auto BridgeCheck = City.ValidateRoad(Board->RoadPreview);
	TestTrue(TEXT("Automatically selected bridge obeys normal span validation"), BridgeCheck.IsSuccess());
	Board->AdjustRoadElevation(1);
	TestEqual(TEXT("First up on an automatic water bridge adds height"), Board->RoadPreview.ElevationStep, 1);
	Board->AdjustRoadElevation(-1);
	Board->AdjustRoadElevation(-1);
	TestTrue(TEXT("Lowering over water preserves safe bridge treatment"),
	         Board->RoadPreview.Profile == ECityRoadProfile::Bridge && Board->RoadPreview.ElevationStep == 0);
	const auto BridgeCommit = City.CommitRoad(Board->RoadPreview);
	TestTrue(TEXT("Regular water drawing commits a persisted explicit span"), BridgeCommit.IsSuccess());
	TestEqual(TEXT("Automatic bridge commit uses exact preview cost"), BridgeCommit.Cost, BridgeCheck.Cost);
	auto Bent = City.BeginRoadPreview({58, 10});
	Bent.End = {57, 62};
	Bent.Profile = City.ResolveRoadDrawingProfile(Bent.Start, Bent.End, false);
	TestTrue(TEXT("Bent water drawing still resolves as bridge"), Bent.Profile == ECityRoadProfile::Bridge);
	TestFalse(TEXT("Bent water crossing cannot bypass straight-span rules"),
	          City.ValidateRoad(Bent).IsSuccess());
	Board->RoadPreview = City.BeginRoadPreview({5, 1});
	Board->RoadPreview.End = {5, 23};
	Board->RefreshRoadDrawingProfile();
	TestTrue(TEXT("Moving preview from water to dry terrain restores ground"),
	         Board->RoadPreview.Profile == ECityRoadProfile::Ground);
	Board->SetSelectedRoadDefinition(TEXT("Avenue"));
	TestTrue(TEXT("Choosing a road class starts on ground and clears old preview"),
	         Board->GetSelectedRoadProfile() == ECityRoadProfile::Ground && !Board->RoadPreview.bActive &&
	             Board->GetSelectedRoadElevationStep() == 0);
	World->DestroyWorld(false);
	return true;
}
#endif
