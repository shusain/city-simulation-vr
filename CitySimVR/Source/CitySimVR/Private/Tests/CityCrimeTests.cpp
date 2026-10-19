#if WITH_DEV_AUTOMATION_TESTS
#include "Simulation/CityCrimeSimulation.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Simulation/CityGrowthSimulation.h"
#include "Simulation/CityEnvironmentSimulation.h"
#include "Persistence/CityPersistence.h"
#include "Presentation/CityDiagnosticOverlay.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
FCityBuildingDefinition CrimeDefinition(const TCHAR* Id)
{
	FCityBuildingDefinition Definition;
	Definition.DefinitionId = Id;
	Definition.DisplayName = Id;
	Definition.Footprint = {1, 1};
	Definition.Cost = 1;
	if (Definition.DefinitionId == TEXT("Police"))
	{
		Definition.PowerDemand = 10;
		Definition.WaterDemand = 5;
		Definition.MonthlyUpkeep = 25;
	}
	return Definition;
}
uint64 CrimePlace(FCityPlacementModel& City, const TCHAR* Id, const FCityCellCoord Cell)
{
	const auto Definition = CrimeDefinition(Id);
	auto Preview = City.BeginPreview(Definition, Cell);
	return City.CommitPlacement(Preview, Definition).EntityId;
}
void CrimeRoad(FCityPlacementModel& City)
{
	auto Preview = City.BeginRoadPreview(FCityPlacementModel::EntranceCell);
	City.UpdateRoadPreviewEnd(Preview, {30, 8});
	City.CommitRoad(Preview);
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityCrimePressureTest, "CitySim.Safety.CrimePressureAndCoverage",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityCrimePressureTest::RunTest(const FString& Parameters)
{
	FCityPlacementModel City(100000);
	CrimeRoad(City);
	CrimePlace(City, TEXT("Power"), {1, 9});
	CrimePlace(City, TEXT("Service"), {2, 9});
	const uint64 Near = CrimePlace(City, TEXT("Home"), {4, 9});
	const uint64 Far = CrimePlace(City, TEXT("Home"), {20, 9});
	const uint64 Station = CrimePlace(City, TEXT("Police"), {6, 9});
	auto* Catalog = NewObject<UCityBuildingCatalog>();
	Catalog->Definitions = {CrimeDefinition(TEXT("Home")), CrimeDefinition(TEXT("Police"))};
	FCityEconomyTuning Tuning;
	Tuning.CrimePopulationThreshold = 20;
	Tuning.PoliceRoadRangeHops = 4;
	FCityDevelopmentSnapshot Development;
	auto Services = FCityServiceEvaluator::BuildSnapshot(City, Tuning, Catalog);
	TestEqual(TEXT("Police has real utility demand"), Services.PowerDemand, 30);
	TestEqual(TEXT("Police upkeep is named separately"), Services.CivicUpkeep, int64(25));
	FCityCrimeSimulation Crime;
	Crime.Refresh(City, Services, Development, Tuning, false);
	TestEqual(TEXT("No crime before establishment threshold"), Crime.GetSnapshot().BaseRisk, 0);
	Crime.AdvanceToStep(1, City, Services, Development, Tuning, false);
	TestTrue(TEXT("Threshold introduces warning/grace"),
	         Crime.GetSnapshot().IsIntroduced() && Crime.GetSnapshot().IsInGrace());
	TestEqual(TEXT("Two game months to respond"), Crime.GetSnapshot().GraceEndStep, int64(51));
	TestEqual(TEXT("Full jobs shortage increases pressure"), Crime.GetSnapshot().BaseRisk, 65);
	TestTrue(TEXT("Reachable station lowers near risk"),
	         Crime.GetSnapshot().EntityRisk.FindRef(Near) < Crime.GetSnapshot().EntityRisk.FindRef(Far));
	TestEqual(TEXT("Beyond road budget receives no protection"),
	          Crime.GetSnapshot().EntityProtection.FindRef(Far), 0);
	Services.Jobs = 20;
	Crime.Refresh(City, Services, Development, Tuning, false);
	TestEqual(TEXT("Available jobs reduce pressure"), Crime.GetSnapshot().BaseRisk, 25);
	Crime.AdvanceToStep(50, City, Services, Development, Tuning, false);
	TestEqual(TEXT("Grace has no crime incidents"), Crime.GetSnapshot().TotalIncidents, int64(0));
	City.DemolishEntity(Far);
	Services = FCityServiceEvaluator::BuildSnapshot(City, Tuning, Catalog);
	Services.Jobs = 20;
	Services.Residents = 20;
	Services.HomelessResidents = 10;
	Crime.AdvanceToStep(51, City, Services, Development, Tuning, false);
	TestEqual(TEXT("Crime consumes citywide citizen homelessness"), Crime.GetSnapshot().HomelessResidents,
	          10);
	TestEqual(TEXT("Homelessness independently adds pressure"), Crime.GetSnapshot().BaseRisk, 55);
	CrimePlace(City, TEXT("Home"), {20, 9});
	Services = FCityServiceEvaluator::BuildSnapshot(City, Tuning, Catalog);
	Services.Jobs = 20;
	Crime.AdvanceToStep(52, City, Services, Development, Tuning, false);
	TestEqual(TEXT("Citizen recovery clears citywide homelessness"), Crime.GetSnapshot().HomelessResidents,
	          0);
	Tuning.PoliceProtectionCapacity = 3;
	Crime.Refresh(City, Services, Development, Tuning, false);
	TestEqual(TEXT("Police capacity allocation is bounded"), Crime.GetSnapshot().ProtectedDemand, 3);
	Crime.Refresh(City, Services, Development, Tuning, true);
	TestEqual(TEXT("Unpaid upkeep stops protection"), Crime.GetSnapshot().ProtectedDemand, 0);
	TestEqual(TEXT("Upkeep recovery does not invent staff or coverage"),
	          Crime.GetSnapshot().OperationalStations, 0);
	City.RemoveRoad({6, 8});
	Services = FCityServiceEvaluator::BuildSnapshot(City, Tuning, Catalog);
	TestEqual(TEXT("Station road access is authoritative"), Services.GetFailure(Station),
	          ECityOperationFailure::NoRoad);
	Crime.Refresh(City, Services, Development, Tuning, false);
	TestEqual(TEXT("Disconnected station cannot protect"), Crime.GetSnapshot().ProtectedDemand, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityCrimeDeterminismTest, "CitySim.Safety.CrimeIncidentsAndResume",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityCrimeDeterminismTest::RunTest(const FString& Parameters)
{
	FCityPlacementModel City(100000);
	CrimeRoad(City);
	CrimePlace(City, TEXT("Power"), {1, 9});
	CrimePlace(City, TEXT("Service"), {2, 9});
	CrimePlace(City, TEXT("Home"), {4, 9});
	CrimePlace(City, TEXT("Home"), {10, 9});
	FCityEconomyTuning Tuning;
	Tuning.CrimePopulationThreshold = 1;
	Tuning.CrimeGraceMonths = 0;
	Tuning.CrimeBaseRisk = 100;
	Tuning.CrimeCheckIntervalSteps = 1;
	FCityDevelopmentSnapshot Development;
	auto Services = FCityServiceEvaluator::BuildSnapshot(City, Tuning);
	FCityCrimeSimulation Fine, Coarse;
	for (int64 Step = 1; Step <= 300; ++Step)
	{
		Fine.AdvanceToStep(Step, City, Services, Development, Tuning, false);
		TestTrue(TEXT("Active incidents stay within cap"),
		         Fine.GetSnapshot().ActiveIncidents <= FCityCrimeSimulation::MaximumActiveIncidents);
	}
	Coarse.AdvanceToStep(300, City, Services, Development, Tuning, false);
	TestEqual(TEXT("Seeded catch-up uses same random state"), Coarse.GetSnapshot().RandomState,
	          Fine.GetSnapshot().RandomState);
	TestEqual(TEXT("Seeded catch-up uses same incidents"), Coarse.GetSnapshot().TotalIncidents,
	          Fine.GetSnapshot().TotalIncidents);
	TestTrue(TEXT("Incidents occur after grace"), Fine.GetSnapshot().TotalIncidents > 0);
	TestTrue(TEXT("History is bounded"),
	         Fine.GetSnapshot().RecentIncidents.Num() <= FCityCrimeSimulation::MaximumRecentIncidents);
	FString Error;
	FCityCrimeSimulation Restored;
	TestTrue(TEXT("Active incidents restore"), Restored.RestoreState(Fine.GetSnapshot(), 300, Error));
	Fine.AdvanceToStep(400, City, Services, Development, Tuning, false);
	Restored.AdvanceToStep(400, City, Services, Development, Tuning, false);
	TestEqual(TEXT("Resume preserves next random sample"), Restored.GetSnapshot().RandomState,
	          Fine.GetSnapshot().RandomState);
	TestEqual(TEXT("Resume preserves incident cadence"), Restored.GetSnapshot().TotalIncidents,
	          Fine.GetSnapshot().TotalIncidents);
	const auto BeforePause = Fine.GetSnapshot();
	Fine.Refresh(City, Services, Development, Tuning, false);
	Fine.AdvanceToStep(400, City, Services, Development, Tuning, false);
	TestEqual(TEXT("Render refresh/pause cannot advance incidents"), Fine.GetSnapshot().TotalIncidents,
	          BeforePause.TotalIncidents);
	CrimePlace(City, TEXT("Police"), {6, 9});
	auto* Catalog = NewObject<UCityBuildingCatalog>();
	Catalog->Definitions.Add(CrimeDefinition(TEXT("Police")));
	Services = FCityServiceEvaluator::BuildSnapshot(City, Tuning, Catalog);
	Tuning.PoliceRiskReduction = 100;
	FCityCrimeSimulation Protected;
	Protected.AdvanceToStep(40, City, Services, Development, Tuning, false);
	TestEqual(TEXT("Full prevention suppresses incidents"), Protected.GetSnapshot().TotalIncidents, int64(0));
	TestTrue(TEXT("Prevented attempts are counted"), Protected.GetSnapshot().PreventedIncidents > 0);
	Services.CrimeRisk = Fine.GetSnapshot().CellRisk;
	const auto Penalty = FCityEnvironmentSimulation::EvaluateResidentialDesirability(City, Services, {},
	                                                                                 {4, 9}, {1, 1}, Tuning);
	TestEqual(TEXT("Crime consequence is bounded desirability penalty"), Penalty.CrimePenalty, 20);
	FCityDiagnosticOverlayResult View;
	FCityNeedsSnapshot Needs;
	FCityDiagnosticOverlayEvaluator::Build(City, Services, Needs, Tuning, nullptr, nullptr, nullptr, {},
	                                       ECityDiagnosticOverlay::Crime, 0, View);
	TestEqual(TEXT("Crime heatmap uses exact authority"), View.Values[9 * FCityGrid::Width + 4],
	          Fine.GetSnapshot().GetRisk({4, 9}));
	auto Invalid = Fine.GetSnapshot();
	Invalid.HomelessResidents = -1;
	TestFalse(TEXT("Negative displacement is rejected"),
	          FCityCrimeSimulation::ValidateState(Invalid, 400, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityCrimeIntegrationTest, "CitySim.Persistence.CrimeMigrationAndRecovery",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityCrimeIntegrationTest::RunTest(const FString& Parameters)
{
	const auto Values = UWorld::InitializationValues()
	                        .AllowAudioPlayback(false)
	                        .CreatePhysicsScene(false)
	                        .CreateNavigation(false)
	                        .CreateAISystem(false)
	                        .ShouldSimulatePhysics(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("CrimeSaveTest"), nullptr, true,
	                                    ERHIFeatureLevel::Num, &Values);
	auto* Sim = World->GetSubsystem<UCitySimulationSubsystem>();
	auto* Catalog = LoadObject<UCityBuildingCatalog>(
	    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
	if (!TestNotNull(TEXT("Police is in the real catalog"),
	                 Catalog ? Catalog->FindDefinition(TEXT("Police")) : nullptr))
	{
		World->DestroyWorld(false);
		return false;
	}
	Sim->RegisterBuildingCatalog(Catalog);
	auto& City = Sim->GetPlacementModel();
	City.SetTerrain(nullptr);
	CrimeRoad(City);
	const TCHAR* DefinitionIds[] = {TEXT("Power"), TEXT("Service"), TEXT("Home"), TEXT("Police")};
	const FCityCellCoord Locations[] = {{1, 9}, {3, 9}, {6, 9}, {16, 9}};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(DefinitionIds); ++Index)
	{
		const auto* Definition = Catalog->FindDefinition(DefinitionIds[Index]);
		auto Preview = City.BeginPreview(*Definition, Locations[Index]);
		TestTrue(TEXT("Crime fixture placement commits"),
		         City.CommitPlacement(Preview, *Definition).IsSuccess());
	}
	Sim->Development.InitializeExistingAsOperational(City, Catalog, 0);
	Sim->EconomyTuning.FixedStepSeconds = .125;
	Sim->EconomyTuning.FixedStepsPerMonth = 4;
	Sim->EconomyTuning.CrimePopulationThreshold = 5;
	Sim->EconomyTuning.CrimeGraceMonths = 1;
	Sim->StartScenario();
	for (int32 Step = 0; Step < 8; ++Step)
		Sim->Tick(.125f);
	TestTrue(TEXT("Established city introduces police guidance"), Sim->Crime.GetSnapshot().IsIntroduced());
	TestEqual(TEXT("Police upkeep enters monthly ledger"), Sim->GetEconomySnapshot().CivicUpkeep, int64(25));
	FString Json, Error;
	TestTrue(TEXT("Crime state serializes"),
	         FCityPersistence::Serialize(Sim->BuildSaveDocument(), Json, Error));
	const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/Crime"));
	IFileManager::Get().MakeDirectory(*Directory, true);
	const FString Path = FPaths::Combine(Directory, TEXT("City.json"));
	FFileHelper::SaveStringToFile(Json, *Path);
	const auto State = Sim->GetCrimeSnapshot();
	TestTrue(TEXT("Crime state loads"), Sim->LoadCityFromPath(Path, Error, false));
	TestEqual(TEXT("Grace survives load"), Sim->GetCrimeSnapshot().GraceEndStep, State.GraceEndStep);
	TestEqual(TEXT("Random state survives load"), Sim->GetCrimeSnapshot().RandomState, State.RandomState);
	TSharedPtr<FJsonObject> Root;
	FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root);
	Root->SetNumberField(TEXT("schemaVersion"), 22);
	Json.Reset();
	FJsonSerializer::Serialize(Root.ToSharedRef(), TJsonWriterFactory<>::Create(&Json));
	FFileHelper::SaveStringToFile(Json, *Path);
	TestFalse(TEXT("Historical format rejected"), Sim->LoadCityFromPath(Path, Error, false));
	FString Before, After;
	FCityPersistence::Serialize(Sim->BuildSaveDocument(), Before, Error);
	FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Before), Root);
	Root->GetObjectField(TEXT("crime"))->SetNumberField(TEXT("homelessResidents"), -1);
	Json.Reset();
	FJsonSerializer::Serialize(Root.ToSharedRef(), TJsonWriterFactory<>::Create(&Json));
	FFileHelper::SaveStringToFile(Json, *Path);
	TestFalse(TEXT("Bad crime load is rejected"), Sim->LoadCityFromPath(Path, Error, false));
	FCityPersistence::Serialize(Sim->BuildSaveDocument(), After, Error);
	TestEqual(TEXT("Rejection leaves the full city unchanged"), After, Before);
	Sim->RequestResetScenario();
	Sim->RequestResetScenario();
	TestFalse(TEXT("Reset clears crime activation"), Sim->GetCrimeSnapshot().IsIntroduced());
	TestEqual(TEXT("Reset clears displacement"), Sim->GetCrimeSnapshot().HomelessResidents, 0);
	World->DestroyWorld(false);
	return true;
}
#endif
