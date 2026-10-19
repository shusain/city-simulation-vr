#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Persistence/CitySaveLibrary.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Simulation/CityRoadNetwork.h"
#include "Engine/World.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
FCitySaveDocument DimensionSave(int32 Size)
{
	FCitySaveDocument D(FCityGridDimensions(Size, Size));
	D.Identity.Id = FGuid::NewGuid().ToString(EGuidFormats::Digits).ToLower();
	D.Placement = FCityPlacementModel(123456, D.GetDimensions()).ExportState();
	D.Terrain.Seed = 20261011;
	return D;
}
TSharedPtr<FJsonObject> DimensionRoot(const FString& Json)
{
	TSharedPtr<FJsonObject> Root;
	FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root);
	return Root;
}
FString DimensionJson(const TSharedPtr<FJsonObject>& Root)
{
	FString Json;
	FJsonSerializer::Serialize(Root.ToSharedRef(), TJsonWriterFactory<>::Create(&Json));
	return Json;
}
FString DimensionDirectory()
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/SaveDimensions"),
	                       FGuid::NewGuid().ToString(EGuidFormats::Digits));
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCitySaveDimensionsRoundtrip, "CitySim.Dimensions.SaveRoundtrip",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCitySaveDimensionsRoundtrip::RunTest(const FString& Parameters)
{
	for (const int32 Size : {64, 80})
	{
		auto D = DimensionSave(Size);
		FCityPlacementModel City(123456, D.GetDimensions());
		auto Road = City.BeginRoadPreview({0, 8});
		City.UpdateRoadPreviewEnd(Road, {Size - 1, 8});
		TestTrue(TEXT("edge road built"), City.CommitRoad(Road).IsSuccess());
		TestTrue(TEXT("corner zone painted"),
		         City.SetZoneArea({Size - 2, Size - 2}, {2, 2}, ECityZoneType::Industrial));
		FCityBuildingDefinition Home;
		Home.DefinitionId = TEXT("DimensionHome");
		Home.DisplayName = TEXT("Home");
		Home.Footprint = {2, 2};
		Home.Cost = 10;
		auto Preview = City.BeginPreview(Home, {Size - 3, 9});
		const auto Placed = City.CommitPlacement(Preview, Home);
		TestTrue(TEXT("edge home built"), Placed.IsSuccess());
		D.Placement = City.ExportState();
		D.Environment.Pollution.Last() = 7;
		D.Environment.TotalPollution = D.Environment.PeakPollution = 7;
		// Structural damage is bounded by this city's cells, not the legacy4096 limit.
		for (int32 I = 1; I <= (Size == 80 ? 4097 : 1); ++I)
			D.Fire.DamagedBuildings.Add(I, {1, -1, 1});
		FString Json, Error;
		if (!TestTrue(TEXT("whole-city dimensions serialize"), FCityPersistence::Serialize(D, Json, Error)))
			return false;
		FCitySaveDocument Loaded;
		if (!TestTrue(TEXT("whole-city dimensions deserialize"),
		              FCityPersistence::Deserialize(Json, Loaded, Error, ECitySavePlatform::Desktop)))
		{
			AddError(Error);
			return false;
		}
		TestTrue(TEXT("all dimensioned fields agree"), Loaded.ValidateDimensions(Error));
		TestEqual(TEXT("current schema retained"), Loaded.SchemaVersion,
		          FCitySaveDocument::CurrentSchemaVersion);
		TestEqual(TEXT("edge pollution preserved"), Loaded.Environment.Pollution.Last(), 7);
		TestEqual(TEXT("structural damage count preserved"), Loaded.Fire.DamagedBuildings.Num(),
		          D.Fire.DamagedBuildings.Num());
		TestTrue(TEXT("all zones preserved"), Loaded.Placement.Zones == D.Placement.Zones);
		TMap<FName, FCityBuildingDefinition> Definitions;
		Definitions.Add(Home.DefinitionId, Home);
		auto Restored = FCityPlacementModel::CreateValidatedFromState(Loaded.Placement, Definitions, Error);
		if (!TestTrue(TEXT("native occupancy restores"), Restored.IsValid()))
			return false;
		TestEqual(TEXT("entity ID preserved"), Restored->GetGrid().GetOccupant({Size - 2, 10}),
		          Placed.EntityId);
		TestEqual(TEXT("treasury preserved"), Restored->GetFunds(), City.GetFunds());
		FCityRoadNetworkSnapshot Before, After;
		FCityRoadNetworkBuilder::Build(City, FCityRoadDefinition::Local(), Before);
		FCityRoadNetworkBuilder::Build(*Restored, FCityRoadDefinition::Local(), After);
		TArray<FCityCellCoord> A, B;
		TestTrue(TEXT("original edge route"),
		         Before.FindRoute({0, 8}, {Size - 1, 8}, ECityRoadAccess::Car, A));
		TestTrue(TEXT("restored edge route"),
		         After.FindRoute({0, 8}, {Size - 1, 8}, ECityRoadAccess::Car, B));
		TestTrue(TEXT("exact route cells preserved"), A == B);
		FCityTerrainHeightfield OriginalTerrain, LoadedTerrain;
		TestTrue(TEXT("original terrain generated"), OriginalTerrain.Generate(D.Terrain, &Error));
		TestTrue(TEXT("restored terrain generated"), LoadedTerrain.Generate(Loaded.Terrain, &Error));
		TestTrue(TEXT("exact terrain preserved"),
		         OriginalTerrain.GetQuantizedHeightsCm() == LoadedTerrain.GetQuantizedHeightsCm() &&
		             OriginalTerrain.GetWaterClasses() == LoadedTerrain.GetWaterClasses());
		FCityEnvironmentSimulation Environment(Loaded.GetDimensions());
		TestTrue(TEXT("native environment restores"), Environment.RestoreState(Loaded.Environment, 0, Error));
		FCityFireSimulation Fire;
		TestTrue(TEXT("native structural damage restores"), Fire.RestoreState(Loaded.Fire, 0, Error));
		for (const auto Platform : {ECitySavePlatform::Quest, ECitySavePlatform::PCVR})
		{
			ECitySaveReadFailure Failure;
			FCitySaveDocument Sentinel;
			Sentinel.Identity.Name = TEXT("untouched");
			TestEqual(TEXT("XR only accepts64"),
			          FCityPersistence::Deserialize(Json, Sentinel, Error, Platform, &Failure), Size == 64);
			if (Size == 80)
			{
				TestEqual(TEXT("profile rejection classified"), Failure,
				          ECitySaveReadFailure::UnsupportedPlatform);
				TestEqual(TEXT("rejection leaves output intact"), Sentinel.Identity.Name,
				          FString(TEXT("untouched")));
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCitySaveDimensionsMetadata, "CitySim.Dimensions.SaveMetadata",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCitySaveDimensionsMetadata::RunTest(const FString& Parameters)
{
	FString Json, Error;
	if (!TestTrue(TEXT("fixture serializes"), FCityPersistence::Serialize(DimensionSave(64), Json, Error)))
		return false;
	for (int32 Case = 0; Case < 11; ++Case)
	{
		auto Root = DimensionRoot(Json);
		switch (Case)
		{
		case 0:
			Root->SetNumberField(TEXT("gridWidth"), MAX_int32);
			break;
		case 1:
			Root->SetNumberField(TEXT("gridWidth"), 0);
			break;
		case 2:
			Root->SetNumberField(TEXT("gridWidth"), -64);
			break;
		case 3:
			Root->SetNumberField(TEXT("gridWidth"), 64.5);
			break;
		case 4:
			Root->SetNumberField(TEXT("gridWidth"), 80);
			break;
		case 5:
			Root->RemoveField(TEXT("gridRulesVersion"));
			break;
		case 6:
			Root->SetNumberField(TEXT("gridRulesVersion"), 2);
			break;
		case 7:
			Root->SetNumberField(TEXT("schemaVersion"), FCitySaveDocument::CurrentSchemaVersion + 1);
			break;
		case 8:
			Root->SetNumberField(TEXT("gridWidth"), 96);
			Root->SetNumberField(TEXT("gridHeight"), 96);
			break;
		case 9:
			Root->SetArrayField(TEXT("zones"), {});
			break;
		case 10:
			Root->GetObjectField(TEXT("environment"))->SetArrayField(TEXT("pollution"), {});
			break;
		}
		FCitySaveDocument Sentinel;
		Sentinel.Identity.Name = TEXT("untouched");
		ECitySaveReadFailure Failure;
		TestFalse(TEXT("malformed or unsupported metadata rejected"),
		          FCityPersistence::Deserialize(DimensionJson(Root), Sentinel, Error, ECitySavePlatform::Any,
		                                        &Failure));
		TestEqual(TEXT("failed decode is transactional"), Sentinel.Identity.Name, FString(TEXT("untouched")));
		if (Case == 7)
			TestEqual(TEXT("future schema classified"), Failure, ECitySaveReadFailure::UnsupportedSchema);
		if (Case == 6 || Case == 8)
			TestEqual(TEXT("unknown grid encoding classified"), Failure,
			          ECitySaveReadFailure::UnsupportedGrid);
	}
	FCityPersistence::Serialize(DimensionSave(80), Json, Error);
	auto Root = DimensionRoot(Json);
	Root->SetNumberField(TEXT("schemaVersion"), 59);
	FCitySaveDocument Loaded;
	TestFalse(TEXT("legacy schema cannot encode80"),
	          FCityPersistence::Deserialize(DimensionJson(Root), Loaded, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCitySaveDimensionsLibrary, "CitySim.Dimensions.MixedSizeLibrary",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCitySaveDimensionsLibrary::RunTest(const FString& Parameters)
{
	const auto Dir = DimensionDirectory();
	FString Error;
	TMap<FString, FString> Bytes;
	for (int32 Size : {64, 80})
	{
		auto D = DimensionSave(Size);
		FString Json;
		TestTrue(TEXT("fixture serializes"), FCityPersistence::Serialize(D, Json, Error));
		TestTrue(TEXT("fixture committed"),
		         FCityPersistence::WriteAtomic(FCitySaveLibrary::PathFor(Dir, D.Identity.Id), Json, Error));
		Bytes.Add(D.Identity.Id, Json);
	}
	const auto Slots = FCitySaveLibrary::List(Dir);
	if (!TestEqual(TEXT("both city sizes discovered"), Slots.Num(), 2))
		return false;
	FString Index;
	FFileHelper::LoadFileToString(Index, *FCitySaveLibrary::IndexPath(Dir));
	const auto Root = DimensionRoot(Index);
	TestEqual(TEXT("index upgraded independently"), Root->GetIntegerField(TEXT("version")), 3);
	TestTrue(TEXT("index carries expanded dimensions"),
	         Index.Contains(TEXT("80")) && Index.Contains(TEXT("gridRulesVersion")));
	for (const auto& Slot : Slots)
	{
		TestTrue(TEXT("slot dimensions match authoritative terrain"),
		         Slot.Dimensions == Slot.Terrain.Dimensions);
		TestTrue(TEXT("archive preserves unsupported-size data"),
		         FCitySaveLibrary::SetArchived(Dir, Slot, true, Error));
	}
	for (const auto& Slot : FCitySaveLibrary::List(Dir, FString(), true))
	{
		FString Json;
		FFileHelper::LoadFileToString(Json, *Slot.Path);
		TestEqual(TEXT("archive never crops or rewrites"), Json, Bytes.FindChecked(Slot.Identity.Id));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCitySaveDimensionsLiveRejection, "CitySim.Dimensions.TransactionalLoad",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCitySaveDimensionsLiveRejection::RunTest(const FString& Parameters)
{
	const auto Init = UWorld::InitializationValues()
	                      .AllowAudioPlayback(false)
	                      .CreatePhysicsScene(false)
	                      .CreateNavigation(false)
	                      .CreateAISystem(false)
	                      .ShouldSimulatePhysics(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("DimensionLoad"), nullptr, true,
	                                  ERHIFeatureLevel::Num, &Init);
	auto* Sim = World->GetSubsystem<UCitySimulationSubsystem>();
	Sim->RegisterBuildingCatalog(LoadObject<UCityBuildingCatalog>(
	    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog")));
	Sim->SetSimulationPaused(true);
	const auto Dir = DimensionDirectory(), BaselinePath = FPaths::Combine(Dir, TEXT("baseline.json")),
	           Path = FPaths::Combine(Dir, TEXT("candidate.json"));
	FString Error, Baseline;
	FCityIdentity InitialIdentity;
	InitialIdentity.Id = FGuid::NewGuid().ToString(EGuidFormats::Digits).ToLower();
	if (!TestTrue(TEXT("fresh gameplay city initializes"),
	              Sim->CreateCity(InitialIdentity, FCityTerrainGenerationParameters(), Dir, Error)))
	{
		AddError(Error);
		World->DestroyWorld(false);
		return false;
	}
	TestTrue(TEXT("live city saved in isolated directory"), Sim->SaveCityToPath(BaselinePath, Error));
	FFileHelper::LoadFileToString(Baseline, *BaselinePath);
	const auto Generation = Sim->GetCitySessionGeneration();
	for (int32 Case = FParse::Param(FCommandLine::Get(), TEXT("vr")) ? 0 : 1; Case < 5; ++Case)
	{
		FString Json;
		FCityPersistence::Serialize(DimensionSave(80), Json, Error);
		if (Case > 0)
		{
			auto Root = DimensionRoot(Baseline);
			if (Case == 1)
				Root->SetNumberField(TEXT("schemaVersion"), FCitySaveDocument::CurrentSchemaVersion + 1);
			if (Case == 2)
				Root->SetNumberField(TEXT("gridRulesVersion"), 2);
			if (Case == 3)
			{
				Root->SetNumberField(TEXT("gridWidth"), 96);
				Root->SetNumberField(TEXT("gridHeight"), 96);
			}
			if (Case == 4)
				Root->SetArrayField(TEXT("zones"), {});
			Json = DimensionJson(Root);
		}
		FFileHelper::SaveStringToFile(*Json, *Path);
		FFileHelper::SaveStringToFile(*Baseline, *(Path + TEXT(".bak")));
		TestFalse(TEXT("failed load preserves current city"), Sim->LoadCityFromPath(Path, Error, Case != 4));
		TestEqual(TEXT("failed load leaves session generation"), Sim->GetCitySessionGeneration(), Generation);
		TestTrue(TEXT("failed load leaves live city saveable"), Sim->SaveCityToPath(BaselinePath, Error));
		FString After;
		FFileHelper::LoadFileToString(After, *BaselinePath);
		TestEqual(TEXT("every live authority preserved"), After, Baseline);
		FFileHelper::LoadFileToString(After, *Path);
		TestEqual(TEXT("primary untouched"), After, Json);
		FFileHelper::LoadFileToString(After, *(Path + TEXT(".bak")));
		TestEqual(TEXT("backup untouched"), After, Baseline);
	}
	FCityTerrainGenerationParameters Terrain;
	Terrain.Dimensions = FCityGridDimensions(96, 96);
	FCityIdentity Identity;
	Identity.Id = FGuid::NewGuid().ToString(EGuidFormats::Digits).ToLower();
	TestFalse(TEXT("unsupported size cannot be created"), Sim->CreateCity(Identity, Terrain, Dir, Error));
	TestFalse(TEXT("rejected creation writes no city"),
	          FPaths::FileExists(FCitySaveLibrary::PathFor(Dir, Identity.Id)));
	TestEqual(TEXT("rejected creation preserves generation"), Sim->GetCitySessionGeneration(), Generation);
	FFileHelper::SaveStringToFile(TEXT("{broken"), *Path);
	if (!TestTrue(TEXT("corrupt primary still recovers valid backup"),
	              Sim->LoadCityFromPath(Path, Error, true)))
		AddError(Error);
	World->DestroyWorld(false);
	return true;
}
#endif
