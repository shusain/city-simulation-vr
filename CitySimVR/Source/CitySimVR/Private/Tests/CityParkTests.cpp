#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Simulation/CityParkSimulation.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Simulation/CityGrowthSimulation.h"
#include "Simulation/CityEnvironmentSimulation.h"
#include "Simulation/CityNeedsSimulation.h"
#include "Simulation/CityDevelopmentSimulation.h"
#include "Simulation/CityFireSimulation.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Presentation/CityBoardActor.h"
#include "Presentation/CityParkActivity.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Presentation/CityDiagnosticOverlay.h"
#include "Interaction/CityPaletteModel.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
FCityBuildingDefinition ParkDefinition(const TCHAR* Id, int32 Size, int64 Cost, int32 Radius = 0,
                                       int32 Bonus = 0, int64 Upkeep = 0, int32 Demand = 0)
{
	FCityBuildingDefinition D;
	D.DefinitionId = Id;
	D.DisplayName = Id;
	D.Footprint = {Size, Size};
	D.Cost = Cost;
	D.EntranceSides = 15;
	D.ParkRadiusCells = Radius;
	D.ParkDesirabilityBonus = Bonus;
	D.MonthlyUpkeep = Upkeep;
	D.PowerDemand = D.WaterDemand = Demand;
	return D;
}
uint64 ParkPlace(FCityPlacementModel& City, const FCityBuildingDefinition& D, int32 X)
{
	auto P = City.BeginPreview(D, {X, 9});
	return City.CommitPlacement(P, D).EntityId;
}
void ParkRoad(FCityPlacementModel& City)
{
	auto R = City.BeginRoadPreview(FCityPlacementModel::EntranceCell);
	City.UpdateRoadPreviewEnd(R, {60, 8});
	City.CommitRoad(R);
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityParkCoverageTest, "CitySim.Parks.OperationCoverageAndGrowthLimits",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityParkCoverageTest::RunTest(const FString& Parameters)
{
	FCityPlacementModel City(100000);
	ParkRoad(City);
	auto* Catalog = NewObject<UCityBuildingCatalog>();
	Catalog->Definitions = {ParkDefinition(TEXT("Power"), 1, 0),
	                        ParkDefinition(TEXT("Service"), 1, 0),
	                        ParkDefinition(TEXT("Park"), 3, 150),
	                        ParkDefinition(TEXT("NeighborhoodPark"), 5, 800, 12, 25, 30, 4),
	                        ParkDefinition(TEXT("CityPark"), 7, 1600, 18, 30, 50, 8),
	                        ParkDefinition(TEXT("Home"), 2, 100),
	                        ParkDefinition(TEXT("Shop"), 2, 100)};
	ParkPlace(City, Catalog->Definitions[0], 1);
	ParkPlace(City, Catalog->Definitions[1], 3);
	const int64 Before = City.GetFunds();
	const uint64 Pocket = ParkPlace(City, Catalog->Definitions[2], 6),
	             Neighborhood = ParkPlace(City, Catalog->Definitions[3], 13),
	             Large = ParkPlace(City, Catalog->Definitions[4], 24);
	TestTrue(TEXT("All park sizes place"), Pocket && Neighborhood && Large);
	TestEqual(TEXT("Park purchases charge exactly authored costs"), Before - City.GetFunds(), int64(2550));
	FCityEconomyTuning T;
	T.WaterRangeRoadHops = 100;
	auto S = FCityServiceEvaluator::BuildSnapshot(City, T, Catalog);
	TestEqual(TEXT("All park variants count toward upkeep"), S.ParkUpkeep, int64(95));
	TestEqual(TEXT("All completed connected parks operate"), S.OperationalParks, 3);
	TestEqual(TEXT("Larger parks are counted separately for objectives"), S.OperationalLargeParks, 2);
	TestEqual(TEXT("Parks invent no resident or job capacity"), S.HousingCapacity + S.Jobs, 0);
	TestEqual(TEXT("Only larger parks draw authored utilities"), S.PowerUsed, 12);
	TestEqual(TEXT("Overlapping benefits use strongest park"),
	          FCityParkEvaluator::Benefit(S.ParkProviders, {17, 9}, {2, 2}), 30);
	TestEqual(TEXT("Inclusive outer boundary retains city park benefit"),
	          S.ParkCoverage[9 * FCityGrid::Width + 48], 30);
	TestEqual(TEXT("Beyond city park boundary has no benefit"), S.ParkCoverage[9 * FCityGrid::Width + 49], 0);
	const auto D = FCityEnvironmentSimulation::EvaluateResidentialDesirability(
	    City, S, FCityEnvironmentSnapshot(), {47, 9}, {1, 1}, T);
	TestEqual(TEXT("Residential evaluator and exact overlay agree"), D.ParkBonus,
	          S.ParkCoverage[9 * FCityGrid::Width + 47]);
	FCityDiagnosticOverlayResult Overlay;
	FCityDiagnosticOverlayEvaluator::Build(City, S, FCityNeedsSnapshot(), T, nullptr, nullptr, nullptr,
	                                       FCityEnvironmentSnapshot(), ECityDiagnosticOverlay::Parks, 0,
	                                       Overlay);
	TestEqual(TEXT("Park view retains exact source values"), Overlay.Values[9 * FCityGrid::Width + 47], 30);
	TestTrue(TEXT("Zero benefit is explained rather than missing data"),
	         Overlay.DescribeCell({63, 63}).Contains(TEXT("+0/30")));
	TestTrue(TEXT("Park view names operation and overlap"),
	         Overlay.Legend.Contains(TEXT("Operating 3/3")) && Overlay.Legend.Contains(TEXT("Strongest")));
	FCityParkEvaluator::Build(City, S, T, Catalog, nullptr, true);
	TestEqual(TEXT("Unpaid bills pause appeal"), S.OperationalParks, 0);
	TestEqual(TEXT("Unpaid parks still incur upkeep"), S.ParkUpkeep, int64(95));
	FCityParkEvaluator::Build(City, S, T, Catalog, nullptr, false);
	TestEqual(TEXT("Paid bills restore exact benefits"), S.OperationalParks, 3);
	FCityDevelopmentSnapshot Construction;
	auto& State = Construction.States.FindOrAdd(Large);
	State.EntityId = Large;
	State.Stage = ECityDevelopmentStage::Construction;
	S = FCityServiceEvaluator::BuildSnapshot(City, T, Catalog, &Construction);
	TestEqual(TEXT("Construction gives no park benefit"), S.OperationalLargeParks, 1);
	FCityFireSnapshot Fire;
	Fire.DamagedBuildings.Add(Large, {40, -1});
	S = FCityServiceEvaluator::BuildSnapshot(City, T, Catalog, nullptr, nullptr, nullptr, &Fire);
	TestEqual(TEXT("Damage removes park benefit"), S.OperationalLargeParks, 1);
	for (int32 X = 24; X <= 30; ++X)
		City.RemoveRoad({X, 8});
	S = FCityServiceEvaluator::BuildSnapshot(City, T, Catalog);
	TestEqual(TEXT("Park needs entrance-connected road"), S.GetFailure(Large), ECityOperationFailure::NoRoad);
	TestEqual(TEXT("Road outage removes wide benefit"), S.ParkCoverage[9 * FCityGrid::Width + 47], 0);
	TestFalse(TEXT("Optional park road outage does not halt all growth"), S.bHasDisabledEntities);
	ParkRoad(City);
	const uint64 Home = ParkPlace(City, Catalog->Definitions[5], 52);
	T.PowerCapacityPerBuilding = T.UtilityDemandPerBuilding;
	T.WaterCapacityPerTreatment = T.ResidentsPerOccupiedHome * T.WaterDemandPerResident;
	S = FCityServiceEvaluator::BuildSnapshot(City, T, Catalog);
	TestEqual(TEXT("Existing homes retain scarce utilities ahead of parks"), S.GetFailure(Home),
	          ECityOperationFailure::None);
	TestEqual(TEXT("Larger parks pause under scarce utilities"), S.OperationalLargeParks, 0);
	TestEqual(
	    TEXT("Larger park preview explains exhausted water"),
	    FCityServiceEvaluator::EvaluateCandidate(City, S, T, TEXT("CityPark"), {40, 9}, {7, 7}, Catalog),
	    ECityOperationFailure::ServiceCapacity);
	TestEqual(TEXT("Pocket park preview needs no utility supply"),
	          FCityServiceEvaluator::EvaluateCandidate(City, S, T, TEXT("Park"), {40, 9}, {3, 3}, Catalog),
	          ECityOperationFailure::None);
	TestEqual(
	    TEXT("Larger park preview explains disconnected entrance"),
	    FCityServiceEvaluator::EvaluateCandidate(City, S, T, TEXT("CityPark"), {40, 30}, {7, 7}, Catalog),
	    ECityOperationFailure::NoRoad);
	TestFalse(TEXT("Optional utility outage cannot stop serviced homes growing"), S.bHasDisabledEntities);
	T.PowerCapacityPerBuilding = T.WaterCapacityPerTreatment = 500;
	S = FCityServiceEvaluator::BuildSnapshot(City, T, Catalog);
	for (int32 Y = 9; Y < 11; ++Y)
		for (int32 X = 44; X < 46; ++X)
			City.SetZone({X, Y}, ECityZoneType::Residential);
	ParkPlace(City, Catalog->Definitions[6], 58);
	S = FCityServiceEvaluator::BuildSnapshot(City, T, Catalog);
	City.TrySpendFunds(City.GetFunds() - 1);
	const FCityEnvironmentSnapshot Weather;
	FCityNeedsSnapshot Needs = FCityNeedsEvaluator::BuildSnapshot(
	    City, S, T, &Catalog->Definitions[5], &Catalog->Definitions[6], false, nullptr, &Weather);
	FCityGrowthSimulation Growth;
	TestEqual(TEXT("Serviced private growth with park appeal does not need city construction cash"),
	          Growth.AdvanceToStep(100, City, S, Needs, T, &Catalog->Definitions[5], nullptr, false).Failure,
	          ECityGrowthFailure::None);
	TestEqual(TEXT("Private growth preserves the remaining treasury"), City.GetFunds(), int64(1));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityParkIntegrationTest, "CitySim.Parks.CatalogToolsSaveAndSpeed",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityParkIntegrationTest::RunTest(const FString& Parameters)
{
	const auto Init = UWorld::InitializationValues()
	                      .AllowAudioPlayback(false)
	                      .CreatePhysicsScene(false)
	                      .CreateNavigation(false)
	                      .CreateAISystem(false)
	                      .ShouldSimulatePhysics(false);
	UWorld* W = UWorld::CreateWorld(EWorldType::Game, false, TEXT("ParkIntegration"), nullptr, true,
	                                ERHIFeatureLevel::Num, &Init);
	auto* Sim = W->GetSubsystem<UCitySimulationSubsystem>();
	auto* Catalog = LoadObject<UCityBuildingCatalog>(
	    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
	if (!TestNotNull(TEXT("Park catalog loads"), Catalog))
	{
		W->DestroyWorld(false);
		return false;
	}
	Sim->RegisterBuildingCatalog(Catalog);
	auto& City = Sim->GetPlacementModel();
	City.SetTerrain(nullptr);
	City.ApplyMonthlyEconomy(100000, 0);
	ParkRoad(City);
	auto Spine = City.BeginRoadPreview({0, 8});
	City.UpdateRoadPreviewEnd(Spine, {0, 24});
	City.CommitRoad(Spine);
	auto Second = City.BeginRoadPreview({0, 24});
	City.UpdateRoadPreviewEnd(Second, {60, 24});
	City.CommitRoad(Second);
	Sim->EconomyTuning.WaterRangeRoadHops = 100;
	Sim->EconomyTuning.FixedStepSeconds = .125;
	Sim->EconomyTuning.FixedStepsPerMonth = 4;
	Sim->EconomyTuning.CrimePopulationThreshold = Sim->EconomyTuning.FirePopulationThreshold = 100000;
	for (const auto& Pair : TArray<TPair<FName, int32>>{{TEXT("Power"), 1},
	                                                    {TEXT("Service"), 4},
	                                                    {TEXT("Service"), 16},
	                                                    {TEXT("Service"), 58},
	                                                    {TEXT("Service"), 12},
	                                                    {TEXT("Home"), 8},
	                                                    {TEXT("Shop"), 11},
	                                                    {TEXT("NeighborhoodPark"), 21},
	                                                    {TEXT("CityPark"), 30},
	                                                    {TEXT("BasketballPark"), 40},
	                                                    {TEXT("SkatePark"), 48},
	                                                    {TEXT("Park"), 55},
	                                                    {TEXT("BaseballPark"), 21},
	                                                    {TEXT("SoccerPark"), 30}})
	{
		const auto* Def = Catalog->FindDefinition(Pair.Key);
		if (!TestNotNull(TEXT("Park fixture definition exists"), Def))
		{
			W->DestroyWorld(false);
			return false;
		}
		auto Preview = City.BeginPreview(
		    *Def, {Pair.Value, (Pair.Key == TEXT("BaseballPark") || Pair.Key == TEXT("SoccerPark") ||
		                        (Pair.Key == TEXT("Service") && Pair.Value == 12))
		                           ? 25
		                           : 9});
		const uint64 Id = City.CommitPlacement(Preview, *Def).EntityId;
		TestTrue(TEXT("Authored footprint/cost placement works"), Id != 0);
		if (Def->ParkDesirabilityBonus > 0)
		{
			auto* Mesh = Def->PresentationMesh.LoadSynchronous();
			if (TestNotNull(TEXT("Authored park mesh loads"), Mesh))
			{
				TestTrue(TEXT("Park geometry and material budget bounded"),
				         Mesh->GetNumTriangles(0) <= 4248 && Mesh->GetStaticMaterials().Num() <= 4);
				TestEqual(TEXT("Four park entrances are explicit"), Def->EntranceSides, 15);
			}
			const FString Damaged =
			    Def->PresentationMesh.ToSoftObjectPath().GetLongPackageName() + TEXT("_Damaged.") +
			    Def->PresentationMesh.ToSoftObjectPath().GetAssetName() + TEXT("_Damaged");
			TestNotNull(TEXT("Matching damaged park mesh loads"), LoadObject<UStaticMesh>(nullptr, *Damaged));
		}
	}
	Sim->Development.InitializeExistingAsOperational(City, Catalog, 0);
	Sim->StartScenario();
	Sim->Tick(.125f);
	TestEqual(TEXT("Actual city operates all six larger parks"),
	          Sim->GetServiceSnapshot().OperationalLargeParks, 6);
	auto* Board = W->SpawnActor<ACityBoardActor>();
	Board->BuildingCatalog = Catalog;
	Board->SimulationSubsystem = Sim;
	Board->PlacementModel = &City;
	Board->SetToolMode(ECityToolMode::NeighborhoodPark);
	TestEqual(TEXT("Neighborhood tool selects authored definition"),
	          Board->GetSelectedDefinition()->DefinitionId, FName(TEXT("NeighborhoodPark")));
	Board->SetToolMode(ECityToolMode::CityPark);
	TestEqual(TEXT("City park tool selects authored definition"),
	          Board->GetSelectedDefinition()->DefinitionId, FName(TEXT("CityPark")));
	for (const auto& Pair :
	     TArray<TPair<ECityToolMode, FName>>{{ECityToolMode::BasketballPark, TEXT("BasketballPark")},
	                                         {ECityToolMode::SkatePark, TEXT("SkatePark")},
	                                         {ECityToolMode::BaseballPark, TEXT("BaseballPark")},
	                                         {ECityToolMode::SoccerPark, TEXT("SoccerPark")}})
	{
		Board->SetToolMode(Pair.Key);
		TestEqual(TEXT("Sports tool selects authored park definition"),
		          Board->GetSelectedDefinition()->DefinitionId, Pair.Value);
	}
	auto* Environment = W->SpawnActor<ACitySandboxEnvironment>();
	Environment->SetLightingPreset(ECityLightingPreset::Day);
	FString BeforeActivity, AfterActivity, ActivityError;
	FCityPersistence::Serialize(Sim->BuildSaveDocument(), BeforeActivity, ActivityError);
	IFileManager::Get().MakeDirectory(*FPaths::Combine(FPaths::ProjectDir(), TEXT("../Artifacts/Preview")),
	                                  true);
	FFileHelper::SaveStringToFile(
	    BeforeActivity,
	    *FPaths::Combine(FPaths::ProjectDir(), TEXT("../Artifacts/Preview/Parks-Sports-City.json")));
	Board->UpdateParkActivity(.5f);
	const auto VisitorCount = [&]() {
		int32 Count = Board->ParkVisitorInstances->GetInstanceCount() +
		              Board->ParkOfficeVisitorInstances->GetInstanceCount() +
		              Board->ParkWorkerVisitorInstances->GetInstanceCount();
		for (const auto& Pool : Board->ParkSkateVisitorPools)
			Count += Pool->GetInstanceCount();
		for (const auto& Component : Board->ParkAnimatedVisitors)
			Count += Component->IsVisible() ? 1 : 0;
		return Count;
	};
	TestEqual(TEXT("All seven park styles have daytime visitors"), VisitorCount(), 27);
	TestTrue(TEXT("Nearby visitors use bounded centrally driven skeletal components"),
	         Board->ParkAnimatedVisitors.Num() > 0 &&
	             Board->ParkAnimatedVisitors.Num() <= FCityParkMotion::DesktopAnimationCap);
	for (const auto& Component : Board->ParkAnimatedVisitors)
	{
		TestFalse(TEXT("No per-person component tick is enabled"),
		          Component->PrimaryComponentTick.bCanEverTick);
		TestEqual(TEXT("Animated visitor cannot intercept picking"), Component->GetCollisionEnabled(),
		          ECollisionEnabled::NoCollision);
	}
	TestEqual(TEXT("Basketball activity includes two orange balls"),
	          Board->ParkBasketballInstances->GetInstanceCount(), 2);
	TestEqual(TEXT("Soccer activities include matching balls"),
	          Board->ParkFieldBallInstances->GetInstanceCount(), 3);
	TestEqual(TEXT("Baseball activities include correctly sized balls"),
	          Board->ParkBaseballInstances->GetInstanceCount(), 2);
	TestEqual(TEXT("Skate activity includes three boards"),
	          Board->ParkSkateboardInstances->GetInstanceCount(), 3);
	TestEqual(TEXT("Baseball batter has a swinging bat"), Board->ParkBatInstances->GetInstanceCount(), 1);
	// Use the asymmetric imported mound as evidence of the FBX coordinate conversion.
	const auto* Baseball = Catalog->FindDefinition(TEXT("BaseballPark"));
	auto* BaseballMesh = Baseball->PresentationMesh.LoadSynchronous();
	FVector Mound = FVector::ZeroVector;
	int32 MoundVertices = 0;
	const auto& Positions = BaseballMesh->GetRenderData()->LODResources[0].VertexBuffers.PositionVertexBuffer;
	for (uint32 I = 0; I < Positions.GetNumVertices(); ++I)
	{
		const FVector P(Positions.VertexPosition(I));
		if (FMath::IsNearlyEqual(P.Z, 45.0, .05))
		{
			Mound += P;
			++MoundVertices;
		}
	}
	if (TestTrue(TEXT("Actual imported mound top vertices are found"), MoundVertices > 0))
	{
		Mound /= MoundVertices;
		AddInfo(
		    FString::Printf(TEXT("Imported mound center %.1f, %.1f, %.1f cm"), Mound.X, Mound.Y, Mound.Z));
		TArray<FCityParkActivitySample> Samples;
		FCityParkActivity::Build(City, Sim->GetServiceSnapshot(), Catalog, Board->ParkActivitySeconds, 1,
		                         Samples);
		const auto* Pitcher = Samples.FindByPredicate([&](const auto& P) {
			return P.Prop == ECityParkActivityProp::Visitor &&
			       City.GetEntities().FindChecked(P.EntityId).DefinitionId == TEXT("BaseballPark");
		});
		if (TestNotNull(TEXT("Pitcher activity exists"), Pitcher))
			for (int32 Turns = 0; Turns < 4; ++Turns)
			{
				const auto& E = City.GetEntities().FindChecked(Pitcher->EntityId);
				const auto Parent = Board->MakeBuildingTransform(E.Anchor, *Baseball, Turns);
				const FVector Expected = Parent.TransformPosition(Mound),
				              Actual =
				                  (FCityParkActivity::ToImportedMeshSpace(*Pitcher) * Parent).GetLocation();
				TestTrue(TEXT("Pitcher stands on imported mound for all four rotations"),
				         FVector2D(Expected.X, Expected.Y).Equals(FVector2D(Actual.X, Actual.Y), .5) &&
				             FMath::IsNearlyEqual(Actual.Z, Expected.Z + 1, .5));
			}
	}
	Board->ParkActivitySeconds = 119.9;
	Board->UpdateParkActivity(.2f);
	TestTrue(TEXT("Activity clock never wraps abruptly at two minutes"), Board->ParkActivitySeconds > 120);
	const double Clock = Board->ParkActivitySeconds;
	Sim->SetSimulationPaused(true);
	Board->UpdateParkActivity(5);
	TestEqual(TEXT("Pause holds park animation clock"), Board->ParkActivitySeconds, Clock);
	Sim->SetSimulationPaused(false);
	FCityPersistence::Serialize(Sim->BuildSaveDocument(), AfterActivity, ActivityError);
	TestEqual(TEXT("Park animation never changes saved authority"), AfterActivity, BeforeActivity);
	for (const auto Preset : {ECityLightingPreset::Dusk, ECityLightingPreset::Night})
	{
		Environment->SetLightingPreset(Preset);
		Board->UpdateParkActivity(0);
		TestEqual(TEXT("Closed daytime activity has no night/dusk visitors"), VisitorCount(), 0);
		TestEqual(TEXT("Skateboards hide with visitors"), Board->ParkSkateboardInstances->GetInstanceCount(),
		          0);
	}
	Environment->SetLightingPreset(ECityLightingPreset::Day);
	Board->SetDiagnosticOverlay(ECityDiagnosticOverlay::Parks);
	Board->UpdateParkActivity(0);
	TestEqual(TEXT("Diagnostic view suppresses park activity"), VisitorCount(), 0);
	Board->SetDiagnosticOverlay(ECityDiagnosticOverlay::None);
	Board->UpdateParkActivity(0);
	TestEqual(TEXT("Daytime activity returns after leaving view"), VisitorCount(), 27);
	for (int32 X = 40; X < 45; ++X)
		City.RemoveRoad({X, 8});
	Sim->Tick(0);
	Board->UpdateParkActivity(0);
	TestEqual(TEXT("Real disconnected basketball park hides both balls"),
	          Board->ParkBasketballInstances->GetInstanceCount(), 0);
	ParkRoad(City);
	Sim->Tick(0);
	Board->UpdateParkActivity(0);
	TestEqual(TEXT("Restored operation resumes basketball activity"),
	          Board->ParkBasketballInstances->GetInstanceCount(), 2);
	const auto Entries = FCityPaletteModel::Entries(ECityPalettePage::Parks);
	TestEqual(TEXT("Park submenu returns to Build"), FCityPaletteModel::ParentPage(ECityPalettePage::Parks),
	          ECityPalettePage::Build);
	TestTrue(TEXT("Park submenu exposes both larger choices"), Entries.ContainsByPredicate([](const auto& E) {
		return E.Action == ECityPaletteAction::SelectNeighborhoodPark;
	}) && Entries.ContainsByPredicate([](const auto& E) {
		return E.Action == ECityPaletteAction::SelectCityPark;
	}));
	const FString Path = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/Parks-Parks.json"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	FString Error;
	TestTrue(TEXT("City with larger parks saves"), Sim->SaveCityToPath(Path, Error));
	for (int32 I = 0; I < 15; ++I)
		Sim->Tick(.125f);
	FString Fine;
	FCityPersistence::Serialize(Sim->BuildSaveDocument(), Fine, Error);
	const auto Coverage = Sim->GetServiceSnapshot().ParkCoverage;
	for (int32 Speed : {3, 5})
	{
		TestTrue(TEXT("Larger parks reload"), Sim->LoadCityFromPath(Path, Error, false));
		Sim->SetSimulationSpeed(Speed);
		Sim->Tick(1.875f / Speed);
		Sim->SetSimulationSpeed(1);
		FString Coarse;
		FCityPersistence::Serialize(Sim->BuildSaveDocument(), Coarse, Error);
		TestEqual(TEXT("Park city settlement matches fine/fast stepping"), Coarse, Fine);
		TestTrue(TEXT("Coverage rebuilds identically after load/catch-up"),
		         Sim->GetServiceSnapshot().ParkCoverage == Coverage);
	}
	IFileManager::Get().Delete(*Path, false, true, true);
	IFileManager::Get().Delete(*(Path + TEXT(".bak")), false, true, true);
	W->DestroyWorld(false);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityParkActivityTest, "CitySim.Parks.ActivityBoundsAndSampling",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityParkActivityTest::RunTest(const FString& Parameters)
{
	auto* Catalog = LoadObject<UCityBuildingCatalog>(
	    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
	if (!TestNotNull(TEXT("Activity catalog loads"), Catalog))
		return false;
	FCityPlacementModel City(100000);
	FCityServiceSnapshot Services;
	for (int32 I = 0; I < 20; ++I)
	{
		const FName Name = I % 4 == 0   ? TEXT("BasketballPark")
		                   : I % 4 == 1 ? TEXT("SkatePark")
		                   : I % 4 == 2 ? TEXT("SoccerPark")
		                                : TEXT("BaseballPark");
		const auto* Def = Catalog->FindDefinition(Name);
		if (!TestNotNull(TEXT("Sport definition exists"), Def))
			return false;
		const FCityCellCoord Anchor(1 + (I % 6) * 10, 1 + (I / 6) * 10);
		auto P = City.BeginPreview(*Def, Anchor);
		const uint64 Id = City.CommitPlacement(P, *Def).EntityId;
		if (!TestTrue(TEXT("Bounded activity fixture places"), Id != 0))
			return false;
		Services.ParkProviders.Add(
		    {Id, Anchor, Def->Footprint.GetRotatedSize(0), Def->ParkRadiusCells, Def->ParkDesirabilityBonus});
	}
	const int64 Funds = City.GetFunds();
	const uint64 Revision = City.GetRevision();
	TArray<FCityParkActivitySample> A, B;
	for (const double Time : {0.0, .5, 2.0, 4.0, 6.0, 99.0})
	{
		FCityParkActivity::Build(City, Services, Catalog, Time, 1, A);
		TestTrue(TEXT("Shared sample cap holds"), A.Num() <= FCityParkActivity::MaximumSamples);
		TSet<uint64> Parks;
		for (const auto& Sample : A)
		{
			Parks.Add(Sample.EntityId);
			const auto& Entity = City.GetEntities().FindChecked(Sample.EntityId);
			const double Half = Entity.FootprintSize.X * 400.0;
			TestTrue(TEXT("Activity stays inside authored park footprint"),
			         FMath::Abs(Sample.Position.X) < Half && FMath::Abs(Sample.Position.Y) < Half &&
			             Sample.Position.Z >= 0 && !Sample.Position.ContainsNaN());
		}
		TestEqual(TEXT("Only sixteen parks receive cosmetic samples"), Parks.Num(),
		          FCityParkActivity::MaximumParks);
	}
	FCityParkActivity::Build(City, Services, Catalog, 2, 1, A);
	FCityParkActivity::Build(City, Services, Catalog, 2.5, 1, B);
	TestTrue(TEXT("Activity samples actually animate over time"),
	         !A[0].Position.Equals(B[0].Position) ||
	             !FMath::IsNearlyEqual(A[0].AnimationSeconds, B[0].AnimationSeconds));
	FCityParkActivity::Build(City, Services, Catalog, 119.99, 1, A);
	FCityParkActivity::Build(City, Services, Catalog, 120.01, 1, B);
	TestTrue(TEXT("Activity stays continuous across longer running time"),
	         A[0].Position.Equals(B[0].Position, 50));
	FCityParkActivity::Build(City, Services, Catalog, 2, 0, A);
	TestTrue(TEXT("Night clears every activity sample"), A.IsEmpty());
	FCityParkActivity::Build(City, FCityServiceSnapshot(), Catalog, 2, 1, A);
	TestTrue(TEXT("No operating providers means no activity"), A.IsEmpty());
	TestEqual(TEXT("Cosmetic sampling never spends city funds"), City.GetFunds(), Funds);
	TestEqual(TEXT("Cosmetic sampling never edits city"), City.GetRevision(), Revision);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityParkTerrainTest, "CitySim.Parks.TerrainClearance",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityParkTerrainTest::RunTest(const FString& Parameters)
{
	const auto Init = UWorld::InitializationValues()
	                      .AllowAudioPlayback(false)
	                      .CreatePhysicsScene(false)
	                      .CreateNavigation(false)
	                      .CreateAISystem(false)
	                      .ShouldSimulatePhysics(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("ParkTerrain"), nullptr, true,
	                                    ERHIFeatureLevel::Num, &Init);
	auto* Sim = World->GetSubsystem<UCitySimulationSubsystem>();
	auto* Board = World->SpawnActor<ACityBoardActor>();
	auto* Catalog = LoadObject<UCityBuildingCatalog>(
	    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
	if (!TestNotNull(TEXT("Terrain fixture catalog loads"), Catalog))
	{
		World->DestroyWorld(false);
		return false;
	}
	Board->SimulationSubsystem = Sim;
	Board->BuildingCatalog = Catalog;
	const auto& Terrain = Sim->GetTerrain();
	const auto Original = Terrain.GetQuantizedHeightsCm();
	uint64 RenderId = 1;
	for (const FName Id : {FName(TEXT("Park")), FName(TEXT("NeighborhoodPark")), FName(TEXT("CityPark")),
	                       FName(TEXT("BasketballPark")), FName(TEXT("SkatePark")),
	                       FName(TEXT("BaseballPark")), FName(TEXT("SoccerPark"))})
	{
		const auto* Authored = Catalog->FindDefinition(Id);
		if (!TestNotNull(TEXT("Authored park exists"), Authored))
		{
			World->DestroyWorld(false);
			return false;
		}
		FCityBuildingDefinition Def = *Authored;
		Def.ConstructionMesh.Reset();
		const FIntPoint Size = Def.Footprint.GetRotatedSize(0);
		FCityCellCoord Anchor;
		int32 Peak = 0;
		bool bFound = false;
		// Exercise the two failure modes together: a low road entrance and a
		// terrain peak at a sub-cell sample missed by integer-grid vertices.
		for (int32 Y = 1; Y < FCityGrid::Height - Size.Y - 1 && !bFound; ++Y)
			for (int32 X = 1; X < FCityGrid::Width - Size.X - 1 && !bFound; ++X)
			{
				int32 Samples = MIN_int32, Vertices = MIN_int32;
				for (int32 V = Y * 2; V <= (Y + Size.Y) * 2; ++V)
					for (int32 U = X * 2; U <= (X + Size.X) * 2; ++U)
					{
						const int32 H = Terrain.GetSampleHeightCm(U, V);
						Samples = FMath::Max(Samples, H);
						if (U % 2 == 0 && V % 2 == 0)
							Vertices = FMath::Max(Vertices, H);
					}
				if (Samples > Vertices && Samples > Terrain.SampleHeightCm(X + .5, Y) + 80)
				{
					Anchor = {X, Y};
					Peak = Samples;
					bFound = true;
				}
			}
		if (!TestTrue(TEXT("Fixture has downhill entrance and interior terrain peak"), bFound))
		{
			World->DestroyWorld(false);
			return false;
		}
		FCityPlacementModel City(100000);
		Board->PlacementModel = &City;
		const TArray<FCityCellCoord> Corners = {{Anchor.X - 1, Anchor.Y - 1},
		                                        {Anchor.X + Size.X, Anchor.Y - 1},
		                                        {Anchor.X + Size.X, Anchor.Y + Size.Y},
		                                        {Anchor.X - 1, Anchor.Y + Size.Y}};
		for (int32 Side = 0; Side < 4; ++Side)
		{
			auto Road = City.BeginRoadPreview(Corners[Side]);
			City.UpdateRoadPreviewEnd(Road, Corners[(Side + 1) % 4]);
			TestTrue(TEXT("Park entrance road places"), City.CommitRoad(Road).IsSuccess());
		}
		for (int32 Turns = 0; Turns < 4; ++Turns)
		{
			FVector Door, Road;
			TestTrue(TEXT("Rotated park has road entrance"),
			         Board->FindEntranceConnection(Anchor, Def, Turns, Door, Road));
			const FTransform Normal = Board->MakeBuildingTransform(Anchor, Def, Turns);
			TestEqual(TEXT("Park clears the highest rendered terrain sample by40cm"), Normal.GetLocation().Z,
			          Peak + 40.0);
			FCityBuildingDefinition Damaged = Def;
			Damaged.DefinitionId = FName(*(Id.ToString() + TEXT("_Damaged")));
			TestTrue(TEXT("Damaged park uses identical terrain clearance"),
			         Board->MakeBuildingTransform(Anchor, Damaged, Turns).Equals(Normal));
			FCityBuildingDefinition Construction = Def;
			Construction.DefinitionId = FName(*(Id.ToString() + TEXT("_Construction")));
			TestTrue(TEXT("Construction uses identical terrain clearance"),
			         Board->MakeBuildingTransform(Anchor, Construction, Turns).Equals(Normal));
			Board->PlacementPreview.Anchor = Anchor;
			Board->PlacementPreview.QuarterTurnsClockwise = Turns;
			Board->UpdateGhost(Def, FCityPlacementResult());
			TestTrue(TEXT("Placement ghost matches finished height"),
			         Board->PlacementGhost->GetRelativeTransform().Equals(Normal));
			FCityPlacedEntity Entity;
			Entity.Anchor = Anchor;
			Entity.QuarterTurnsClockwise = Turns;
			Board->RenderPlacement(RenderId++, Entity, Def);
			FTransform Foundation;
			Board->FoundationInstances->GetInstanceTransform(
			    Board->FoundationInstances->GetInstanceCount() - 1, Foundation, false);
			TestTrue(TEXT("Foundation reaches park base without clipping its lawn"),
			         FMath::IsNearlyEqual(Foundation.GetLocation().Z + Foundation.GetScale3D().Z * 50,
			                              Peak + 20.0, .01));
			FCityParkActivitySample Visitor;
			Visitor.Position = FVector(0, 0, 13);
			TestEqual(TEXT("Park activity rises with the same building transform"),
			          (FCityParkActivity::ToImportedMeshSpace(Visitor) * Normal).GetLocation().Z,
			          Peak + 53.0);
		}
		Board->PlacementModel = nullptr;
	}
	TestTrue(TEXT("Raising parks leaves authoritative terrain unchanged"),
	         Original == Terrain.GetQuantizedHeightsCm());
	World->DestroyWorld(false);
	return true;
}

#endif
