#if WITH_DEV_AUTOMATION_TESTS
#include "Simulation/CityEducationSimulation.h"
#include "Simulation/CityPopulationSimulation.h"
#include "Simulation/CityGrowthSimulation.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Presentation/CityDiagnosticOverlay.h"
#include "Interaction/CityPaletteModel.h"
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
uint64 EducationPlace(FCityPlacementModel& City, const TCHAR* Id, int32 X)
{
	FCityBuildingDefinition D;
	D.DefinitionId = Id;
	D.DisplayName = Id;
	D.Footprint = {1, 1};
	D.Cost = 1;
	auto P = City.BeginPreview(D, {X, 9});
	return City.CommitPlacement(P, D).EntityId;
}
void EducationRoad(FCityPlacementModel& City)
{
	auto P = City.BeginRoadPreview(FCityPlacementModel::EntranceCell);
	City.UpdateRoadPreviewEnd(P, {63, 8});
	City.CommitRoad(P);
}
} // namespace
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCitySchoolAccessTest, "CitySim.Education.CapacityOverlapOutagesAndProgress",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCitySchoolAccessTest::RunTest(const FString& Parameters)
{
	FCityPlacementModel City(100000);
	EducationRoad(City);
	const uint64 Home = EducationPlace(City, TEXT("Home"), 6), A = EducationPlace(City, TEXT("School"), 2),
	             B = EducationPlace(City, TEXT("School"), 3);
	FCityEconomyTuning T;
	T.SchoolCapacity = 3;
	T.SchoolMonthsPerLevel = 2;
	FCityPopulationSimulation People;
	FCityEconomySnapshot E;
	E.Residents = 10;
	E.FixedStepId = 1;
	FCityServiceSnapshot S;
	S.EntityResidents.Add(Home, 10);
	People.Reconcile(City, E, T, &S);
	S.PublicStaffPercent.Add(A, 100);
	S.PublicStaffPercent.Add(B, 100);
	FCityEducationEvaluator::Build(City, S, T, &People.GetSnapshot(), nullptr);
	TestEqual(TEXT("Overlapping schools allocate only unique residents"), S.EducationCitizenIds.Num(), 6);
	TestEqual(TEXT("Capacity use reconciles"), S.EducationServed,
	          S.SchoolUsedCapacity.FindRef(A) + S.SchoolUsedCapacity.FindRef(B));
	TestEqual(TEXT("Home coverage is partial not additive reward"),
	          S.EducationCoverage[9 * FCityGrid::Width + 6], 60);
	People.AdvanceEducation(1, S, T);
	People.AdvanceEducation(1, S, T);
	TestEqual(TEXT("Repeated month never double counts progress"),
	          People.GetSnapshot().Citizens[0].EducationMonths, 1);
	People.AdvanceEducation(2, S, T);
	TestEqual(TEXT("Served resident graduates to Secondary"), People.GetSnapshot().Citizens[0].Education,
	          ECityEducationLevel::Secondary);
	TestEqual(TEXT("Capacity-blocked resident remains Basic"), People.GetSnapshot().Citizens[9].Education,
	          ECityEducationLevel::Basic);
	FCityServiceSnapshot Outage;
	Outage.EntityResidents.Add(Home, 10);
	Outage.PublicStaffPercent = S.PublicStaffPercent;
	Outage.EntityFailures.Add(A, ECityOperationFailure::NoPower);
	Outage.EntityFailures.Add(B, ECityOperationFailure::NoService);
	FCityEducationEvaluator::Build(City, Outage, T, &People.GetSnapshot(), nullptr);
	TestEqual(TEXT("Utility outage removes education capacity"), Outage.EducationCapacity, 0);
	People.AdvanceEducation(3, Outage, T);
	TestEqual(TEXT("Outage gives no learning credit"), People.GetSnapshot().Citizens[9].EducationMonths, 0);
	FCityServiceSnapshot Partial;
	Partial.EntityResidents.Add(Home, 10);
	Partial.PublicStaffPercent.Add(A, 50);
	Partial.EntityFailures.Add(B, ECityOperationFailure::NoStaff);
	FCityEducationEvaluator::Build(City, Partial, T, &People.GetSnapshot(), nullptr);
	TestEqual(TEXT("Partial staffing scales capacity"), Partial.EducationCapacity, 1);
	City.RemoveRoad({4, 8});
	FCityServiceSnapshot Gap;
	Gap.EntityResidents.Add(Home, 10);
	Gap.PublicStaffPercent = S.PublicStaffPercent;
	FCityEducationEvaluator::Build(City, Gap, T, &People.GetSnapshot(), nullptr);
	TestEqual(TEXT("Disconnected road blocks resident access"), Gap.EducationServed, 0);
	// Multiple providers still grant only one bounded desirability benefit.
	EducationRoad(City);
	T.SchoolCapacity = 48;
	FCityServiceSnapshot Benefit;
	Benefit.EntityResidents.Add(Home, 10);
	Benefit.PublicStaffPercent = S.PublicStaffPercent;
	FCityEducationEvaluator::Build(City, Benefit, T, &People.GetSnapshot(), nullptr);
	FCityEnvironmentSnapshot Weather;
	const auto Desirability = FCityEnvironmentSimulation::EvaluateResidentialDesirability(
	    City, Benefit, Weather, {6, 9}, {1, 1}, T);
	TestEqual(TEXT("Overlapping school benefits are capped"), Desirability.EducationBonus, 8);
	FCityPopulationSimulation Loaded;
	FString Error;
	TestTrue(TEXT("Education records validate after outage"),
	         Loaded.RestoreState(People.GetSnapshot(), Error));
	auto Invalid = People.GetSnapshot();
	Invalid.Citizens[0].EducationMonths = 121;
	TestFalse(TEXT("Invalid progress rejects restore"), Loaded.RestoreState(Invalid, Error));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCitySkilledPayrollTest,
                                 "CitySim.Education.QualifiedRolesAndCashLimitedPayroll",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCitySkilledPayrollTest::RunTest(const FString& Parameters)
{
	FCityPlacementModel City(100000);
	EducationRoad(City);
	const uint64 Home = EducationPlace(City, TEXT("Home"), 6), Shop = EducationPlace(City, TEXT("Shop"), 10),
	             School = EducationPlace(City, TEXT("School"), 2);
	FCityEconomyTuning T;
	T.SchoolMonthsPerLevel = 1;
	FCityEconomySnapshot E;
	E.Residents = 10;
	E.Jobs = 12;
	E.RetailCapacity = 8;
	E.FixedStepId = 1;
	FCityServiceSnapshot S;
	S.EntityResidents.Add(Home, 10);
	S.EntityJobs.Add(Shop, 8);
	S.EntityOutput.Add(Shop, 8);
	S.EntitySkilledJobs.Add(Shop, 2);
	S.PublicJobs.Add(School, 4);
	S.PublicStaffPercent.Add(School, 100);
	FCityPopulationSimulation P;
	P.Reconcile(City, E, T, &S);
	int32 Skilled = 0;
	for (const auto& C : P.GetSnapshot().Citizens)
		Skilled += C.bSkilledJob ? 1 : 0;
	TestEqual(TEXT("Basic workers cannot fill skilled roles"), Skilled, 0);
	TestEqual(TEXT("Starter assistants bootstrap school staffing"), P.GetSnapshot().Workers(School), 4);
	FCityEducationEvaluator::Build(City, S, T, &P.GetSnapshot(), nullptr);
	P.AdvanceEducation(1, S, T);
	P.Reconcile(City, E, T, &S);
	Skilled = 0;
	for (const auto& C : P.GetSnapshot().Citizens)
		Skilled += C.bSkilledJob ? 1 : 0;
	TestEqual(TEXT("Graduates fill exactly the skilled role capacity"), Skilled, 2);
	auto State = P.GetSnapshot();
	State.Businesses[0].Balance = 1000;
	FString Error;
	P.RestoreState(State, Error);
	E.LastBilledMonthId = 1;
	E.LastPublicPayrollPaid = 20;
	P.AdvanceToMonth(City, E, T, &S, true, false);
	TestEqual(TEXT("Two skilled and four entry workers owe40 wages"),
	          P.GetSnapshot().Businesses[0].PayrollDue, int64(40));
	for (const auto& C : P.GetSnapshot().Citizens)
		if (C.WorkplaceEntityId == Shop)
			TestEqual(TEXT("Role and education determine exact funded wage"), C.AvailableMoney,
			          int64(C.bSkilledJob ? 10 : 5));
	State = P.GetSnapshot();
	State.Businesses[0].Balance = 13;
	P.RestoreState(State, Error);
	int64 Before = 0;
	for (const auto& C : State.Citizens)
		if (C.WorkplaceEntityId == Shop)
			Before += C.AvailableMoney;
	E.LastBilledMonthId = 2;
	P.AdvanceToMonth(City, E, T, &S, true, false);
	int64 After = 0;
	for (const auto& C : P.GetSnapshot().Citizens)
		if (C.WorkplaceEntityId == Shop)
			After += C.AvailableMoney;
	TestEqual(TEXT("Cash-limited differential wages conserve exact13 dollars"), After - Before, int64(13));
	TestEqual(TEXT("Employer cannot spend below available cash"), P.GetSnapshot().Businesses[0].Balance,
	          int64(0));
	FCitySaveDocument Doc, Read;
	Doc.Placement = City.ExportState();
	Doc.Economy = E;
    Doc.Economy.FixedStepId = E.LastBilledMonthId * T.FixedStepsPerMonth;
    Doc.Economy.MonthId = E.LastBilledMonthId;
    Doc.Disasters.LastFixedStepId = Doc.Economy.FixedStepId;
	Doc.Population = P.GetSnapshot();
	FString Json;
	if (!TestTrue(TEXT("Education progress and skilled roles serialize"),
                  FCityPersistence::Serialize(Doc, Json, Error)))
    { AddError(Error); return false; }
	if (!TestTrue(TEXT("Education progress and skilled roles deserialize"),
                  FCityPersistence::Deserialize(Json, Read, Error)))
    { AddError(Error); return false; }
	TestEqual(TEXT("Skilled capacity survives save"), Read.Population.Businesses[0].SkilledJobCapacity, 2);
	TestTrue(TEXT("Skilled citizen role survives save"),
	         Read.Population.Citizens.ContainsByPredicate([](const auto& C) { return C.bSkilledJob; }));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCitySchoolIntegrationTest,
                                 "CitySim.Education.CitySchoolSaveMigrationAndSpeed",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCitySchoolIntegrationTest::RunTest(const FString& Parameters)
{
	const auto Values = UWorld::InitializationValues()
	                        .AllowAudioPlayback(false)
	                        .CreatePhysicsScene(false)
	                        .CreateNavigation(false)
	                        .CreateAISystem(false)
	                        .ShouldSimulatePhysics(false);
	UWorld* W = UWorld::CreateWorld(EWorldType::Game, false, TEXT("SchoolSaveTest"), nullptr, true,
	                                ERHIFeatureLevel::Num, &Values);
	auto* Sim = W->GetSubsystem<UCitySimulationSubsystem>();
	auto* Catalog = LoadObject<UCityBuildingCatalog>(
	    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
	if (!TestNotNull(TEXT("Authored School definition exists"),
	                 Catalog ? Catalog->FindDefinition(TEXT("School")) : nullptr))
	{
		W->DestroyWorld(false);
		return false;
	}
	Sim->RegisterBuildingCatalog(Catalog);
	auto& City = Sim->GetPlacementModel();
	City.SetTerrain(nullptr);
	EducationRoad(City);
	uint64 School = 0;
	const TCHAR* Ids[] = {TEXT("Power"), TEXT("Service"), TEXT("Home"), TEXT("Shop"), TEXT("School")};
	const int32 Xs[] = {1, 3, 6, 10, 14};
	for (int32 I = 0; I < 5; ++I)
	{
		const auto* D = Catalog->FindDefinition(Ids[I]);
		auto Preview = City.BeginPreview(*D, {Xs[I], 9});
		auto Result = City.CommitPlacement(Preview, *D);
		TestTrue(TEXT("School fixture placement succeeds"), Result.IsSuccess());
		if (I == 4)
			School = Result.EntityId;
	}
	Sim->Development.InitializeExistingAsOperational(City, Catalog, 0);
	Sim->EconomyTuning.FixedStepSeconds = .125;
	Sim->EconomyTuning.FixedStepsPerMonth = 4;
	Sim->EconomyTuning.SchoolMonthsPerLevel = 2;
	Sim->EconomyTuning.CrimePopulationThreshold = 100000;
	Sim->EconomyTuning.FirePopulationThreshold = 100000;
	Sim->StartScenario();
	for (int32 I = 0; I < 3; ++I)
		Sim->Tick(.125f);
	TestTrue(TEXT("School has real public employees and places"),
	         Sim->ServiceSnapshot.OperationalSchools == 1 && Sim->ServiceSnapshot.EducationServed > 0 &&
	             Sim->Population.GetSnapshot().Workers(School) > 0);
	FString Error;
	const FString Path = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/Education-School.json"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	TestTrue(TEXT("School city saves"), Sim->SaveCityToPath(Path, Error));
	for (int32 I = 0; I < 15; ++I)
		Sim->Tick(.125f);
	FString Fine;
	FCityPersistence::Serialize(Sim->BuildSaveDocument(), Fine, Error);
	TestTrue(TEXT("Residents learn through actual operating school"),
	         Sim->ServiceSnapshot.SecondaryResidents > 0);
	const auto TrainedStaff = Sim->GetNeedsSnapshot().WorkforceEducation;
	TestTrue(TEXT("Live summary shows school-trained Secondary staff"), TrainedStaff.Secondary > 0);
	TestEqual(TEXT("Live staff summary reconciles with assigned citizens"), TrainedStaff.Total(),
	          Sim->Population.BuildAggregate(Sim->EconomyTuning).EmployedResidents);
	for (int32 Speed : {3, 5})
	{
		TestTrue(TEXT("School city reloads"), Sim->LoadCityFromPath(Path, Error, false));
		Sim->SetSimulationSpeed(Speed);
		Sim->Tick(1.875f / Speed);
		Sim->SetSimulationSpeed(1);
		FString Coarse;
		FCityPersistence::Serialize(Sim->BuildSaveDocument(), Coarse, Error);
		TestEqual(TEXT("School/education/payroll fine and fast catch-up identical"), Coarse, Fine);
		TestEqual(TEXT("Staff education summary matches after reload and fast catch-up"),
		          Sim->GetNeedsSnapshot().WorkforceEducation.Secondary, TrainedStaff.Secondary);
	}
	FCityDiagnosticOverlayResult Overlay;
	FCityDiagnosticOverlayEvaluator::Build(
	    Sim->GetPlacementModel(), Sim->ServiceSnapshot, Sim->NeedsSnapshot, Sim->EconomyTuning, nullptr,
	    nullptr, nullptr, Sim->Environment.GetSnapshot(), ECityDiagnosticOverlay::Education, 0, Overlay);
	TestTrue(TEXT("Education overlay names exact resident units"),
	         Overlay.Legend.Contains(TEXT("Resident places")));
	auto Bad = Sim->BuildSaveDocument();
	Bad.Population.Citizens[0].LastEducationMonth = Bad.Economy.LastBilledMonthId + 1;
	FString BadJson;
	FCityPersistence::Serialize(Bad, BadJson, Error);
	FFileHelper::SaveStringToFile(BadJson, *Path);
	TestFalse(TEXT("Future education clock rejects city load"), Sim->LoadCityFromPath(Path, Error, false));
	FString After;
	FCityPersistence::Serialize(Sim->BuildSaveDocument(), After, Error);
	TestEqual(TEXT("Rejected education load leaves whole city intact"), After, Fine);
	const auto Entries = FCityPaletteModel::Entries(ECityPalettePage::Build);
	TestTrue(TEXT("Education category is visible"), Entries.ContainsByPredicate([](const auto& E) {
		return E.Action == ECityPaletteAction::OpenEducationFacilities;
	}));
	IFileManager::Get().Delete(*Path, false, true, true);
	IFileManager::Get().Delete(*(Path + TEXT(".bak")), false, true, true);
	W->DestroyWorld(false);
	return true;
}
#endif
