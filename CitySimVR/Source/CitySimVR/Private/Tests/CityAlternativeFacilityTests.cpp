#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Simulation/CityUtilitySimulation.h"
#include "Simulation/CityGrowthSimulation.h"
#include "Simulation/CityDevelopmentSimulation.h"
#include "Simulation/CityEducationSimulation.h"
#include "Simulation/CityFireSimulation.h"
#include "Persistence/CityPersistence.h"
#include "Interaction/CityPaletteModel.h"
#include "Engine/StaticMesh.h"
#include "Misc/Paths.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Presentation/CityBoardActor.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
namespace
{
uint64 FacilityPlace(FCityPlacementModel& City, const UCityBuildingCatalog* Catalog, FName Id, int32 X)
{
	const auto* D = Catalog->FindDefinition(Id);
	auto P = City.BeginPreview(*D, {X, 9});
	return City.CommitPlacement(P, *D).EntityId;
}
void FacilityRoad(FCityPlacementModel& City)
{
	auto P = City.BeginRoadPreview(FCityPlacementModel::EntranceCell);
	City.UpdateRoadPreviewEnd(P, {63, 8});
	City.CommitRoad(P);
}
} // namespace
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityAlternativeSupplyTest,
                                 "CitySim.Utilities.AlternativeSupplyQualificationsBudgetAndDamage",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityAlternativeSupplyTest::RunTest(const FString& Parameters)
{
	auto* Catalog = LoadObject<UCityBuildingCatalog>(
	    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
	if (!TestNotNull(TEXT("Catalog"), Catalog))
		return false;
	for (FName Name : {FName(TEXT("WaterTower")), FName(TEXT("CoalPower")), FName(TEXT("OilPower")),
	                   FName(TEXT("WindPower")), FName(TEXT("SolarPower")), FName(TEXT("NuclearPower"))})
	{
		const auto* Definition = Catalog->FindDefinition(Name);
		if (!TestTrue(TEXT("Valid distinct utility"),
		              Definition && Definition->IsValid() && Definition->UtilityCapacity > 0))
			return false;
		FCityPlacementModel City(100000);
		FacilityRoad(City);
		const uint64 Id = FacilityPlace(City, Catalog, Name, 8);
		if (!TestTrue(TEXT("Facility places"), Id > 0))
			return false;
		FCityDevelopmentSimulation Dev;
		Dev.InitializeExistingAsOperational(City, Catalog, 0);
		FCityPopulationSnapshot Population;
		FCityFireSnapshot Fire;
		FCityEconomyTuning Tuning;
		const auto Build = [&]() {
			return FCityServiceEvaluator::BuildSnapshot(City, Tuning, Catalog, &Dev.GetSnapshot(), nullptr,
			                                            &Population, &Fire);
		};
		const auto Capacity = [&](const FCityServiceSnapshot& S) {
			return Name == TEXT("WaterTower") ? S.ServiceCapacity : S.PowerCapacity;
		};
		TestEqual(TEXT("Empty plant only provides its explicit floor"), Capacity(Build()),
		          Definition->UtilityCapacity * Definition->UtilityMinimumOutputPercent / 100);
		Population.PublicEmployers.Add({Id, Definition->JobsProvided, 100, 100});
		for (int32 I = 0; I < Definition->JobsProvided; ++I)
		{
			FCityCitizenRecord C;
			C.Id = I + 1;
			C.WorkplaceEntityId = Id;
			C.Education =
			    static_cast<ECityEducationLevel>(FMath::Max(0, Definition->MinimumStaffEducation - 1));
			Population.Citizens.Add(C);
		}
		if (Definition->MinimumStaffEducation > 0)
			TestEqual(TEXT("Unqualified staff cannot power specialized plant"), Capacity(Build()),
			          Definition->UtilityCapacity * Definition->UtilityMinimumOutputPercent / 100);
		for (auto& C : Population.Citizens)
			C.Education = static_cast<ECityEducationLevel>(Definition->MinimumStaffEducation);
		auto S = Build();
		TestEqual(TEXT("Qualified paid staff deliver nominal output"), Capacity(S),
		          Definition->UtilityCapacity);
		TestEqual(TEXT("Staff vacancies follow catalog"), S.PublicJobs.FindRef(Id), Definition->JobsProvided);
		Population.PublicEmployers[0].PayrollPaid = 50;
		TestEqual(TEXT("Half payroll reduces output above the explicit floor"), Capacity(Build()),
		          Definition->UtilityCapacity *
		              (Definition->UtilityMinimumOutputPercent +
		               (100 - Definition->UtilityMinimumOutputPercent) / 2) /
		              100);
		Population.PublicEmployers[0].PayrollPaid = 100;
		FCityEconomySimulation Economy;
		Economy.Advance(Tuning.FixedStepSeconds, City, Tuning, Catalog, &Dev.GetSnapshot(), &Population, {},
		                &Fire);
		TestEqual(TEXT("Alternative utility upkeep billed once"),
		          Economy.GetProjectedBudget(City, Tuning).UtilityUpkeep, Definition->MonthlyUpkeep);
		TestEqual(TEXT("Pollution follows facility"), S.EntityPollutionEmissions.FindRef(Id),
		          Definition->PollutionEmission);
		Fire.DamagedBuildings.Add(Id, {100, -1, 2});
		TestEqual(TEXT("Natural damage removes output including floors"), Capacity(Build()), 0);
		Fire.DamagedBuildings.Reset();
		TestEqual(TEXT("Recovered plant restores nominal output"), Capacity(Build()),
		          Definition->UtilityCapacity);
		for (int32 X = 8; X < 8 + Definition->Footprint.Width; ++X)
			City.RemoveRoad({X, 8});
		TestEqual(TEXT("Disconnected plant provides no supply"), Capacity(Build()), 0);
		auto* Mesh = Definition->PresentationMesh.LoadSynchronous();
		if (TestNotNull(TEXT("Facility mesh"), Mesh))
			TestTrue(TEXT("Imported facility keeps geometry; masonry/asphalt add shared sections"),
			         Mesh->GetNumTriangles(0) <= 800 && Mesh->GetNumSections(0) <= 4);
		const FString Package =
		    Definition->PresentationMesh.ToSoftObjectPath().GetLongPackageName() + TEXT("_Damaged");
		TestNotNull(
		    TEXT("Damaged variant"),
		    LoadObject<UStaticMesh>(nullptr, *(Package + TEXT(".") + FPaths::GetBaseFilename(Package))));
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityCollegeProgressTest,
                                 "CitySim.Education.CollegePrerequisitesHigherGraduationAndHiring",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityCollegeProgressTest::RunTest(const FString& Parameters)
{
	auto* Catalog = LoadObject<UCityBuildingCatalog>(
	    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
	if (!TestNotNull(TEXT("Catalog"), Catalog))
		return false;
	FCityPlacementModel City(100000);
	FacilityRoad(City);
	const uint64 Home = FacilityPlace(City, Catalog, TEXT("Home"), 8),
	             School = FacilityPlace(City, Catalog, TEXT("School"), 12),
	             College = FacilityPlace(City, Catalog, TEXT("College"), 16),
	             Nuclear = FacilityPlace(City, Catalog, TEXT("NuclearPower"), 20);
	FCityPopulationSimulation People;
	FCityEconomySnapshot Economy;
	Economy.Residents = 10;
	Economy.Jobs = 8;
	Economy.FixedStepId = 1;
	FCityEconomyTuning Tuning;
	Tuning.SchoolMonthsPerLevel = 2;
	FCityServiceSnapshot Services;
	Services.EntityResidents.Add(Home, 10);
	Services.PublicJobs.Add(Nuclear, 8);
	Services.PublicMinimumEducation.Add(Nuclear, 3);
	People.Reconcile(City, Economy, Tuning, &Services);
	TestEqual(TEXT("Basic citizens cannot fill nuclear roles"), People.GetSnapshot().Workers(Nuclear), 0);
	const auto Education = [&](bool bCollege) {
		FCityServiceSnapshot S;
		S.EntityResidents.Add(Home, 10);
		S.PublicStaffPercent.Add(School, 100);
		S.PublicStaffPercent.Add(College, bCollege ? 100 : 0);
		FCityEducationEvaluator::Build(City, S, Tuning, &People.GetSnapshot(), nullptr);
		return S;
	};
	auto Learning = Education(false);
	People.AdvanceEducation(1, Learning, Tuning);
	People.AdvanceEducation(2, Learning, Tuning);
	TestEqual(TEXT("Existing school teaches Secondary"), People.GetSnapshot().Citizens[0].Education,
	          ECityEducationLevel::Secondary);
	Learning = Education(false);
	People.AdvanceEducation(3, Learning, Tuning);
	People.AdvanceEducation(4, Learning, Tuning);
	TestEqual(TEXT("School cannot award Higher"), People.GetSnapshot().Citizens[0].Education,
	          ECityEducationLevel::Secondary);
	Learning = Education(true);
	TestEqual(TEXT("College offers unique Higher places"), Learning.HigherEducationCitizenIds.Num(), 10);
	People.AdvanceEducation(5, Learning, Tuning);
	People.AdvanceEducation(5, Learning, Tuning);
	TestEqual(TEXT("Repeated month grants one credit"), People.GetSnapshot().Citizens[0].EducationMonths, 1);
	FCityPopulationSimulation Resume;
	FString Error;
	if (!TestTrue(TEXT("Partial Higher study saves"), Resume.RestoreState(People.GetSnapshot(), Error)))
		return false;
	Resume.AdvanceEducation(6, Learning, Tuning);
	TestEqual(TEXT("College graduates reach Higher"), Resume.GetSnapshot().Citizens[0].Education,
	          ECityEducationLevel::Higher);
	Resume.Reconcile(City, Economy, Tuning, &Services);
	TestEqual(TEXT("Graduates fill actual nuclear vacancies"), Resume.GetSnapshot().Workers(Nuclear), 8);
	TestEqual(TEXT("Higher wages have higher ceiling"),
	          FCityPopulationSimulation::MonthlySalaryLimit(ECityEducationLevel::Higher, Tuning),
	          int64(Tuning.HouseholdIncomePerEmployedResident * 4));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityAlternativeSaveTest, "CitySim.Persistence.AlternativeFacilitiesAndMenu",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityAlternativeSaveTest::RunTest(const FString& Parameters)
{
	auto* Catalog = LoadObject<UCityBuildingCatalog>(
	    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
	if (!TestNotNull(TEXT("Catalog"), Catalog))
		return false;
	FCityPlacementModel City(100000);
	FacilityRoad(City);
	int32 X = 4;
	for (FName Name : {FName(TEXT("WaterTower")), FName(TEXT("CoalPower")), FName(TEXT("OilPower")),
	                   FName(TEXT("WindPower")), FName(TEXT("SolarPower")), FName(TEXT("NuclearPower")),
	                   FName(TEXT("College"))})
	{
		TestTrue(TEXT("Save fixture places"), FacilityPlace(City, Catalog, Name, X) != 0);
		X += 5;
	}
	FCityDevelopmentSimulation Dev;
	Dev.InitializeExistingAsOperational(City, Catalog, 0);
	FCitySaveDocument Document;
	Document.Placement = City.ExportState();
	Document.Development = Dev.GetSnapshot();
	FCitySaveDocument Loaded;
	FString Error, Json;
	if (!TestTrue(TEXT("Alternative city serializes"), FCityPersistence::Serialize(Document, Json, Error)))
		return false;
	TestTrue(TEXT("Alternative city reloads"), FCityPersistence::Deserialize(Json, Loaded, Error));
	TestEqual(TEXT("All stable IDs survive"), Loaded.Placement.Entities.Num(), 7);
	Document.SchemaVersion = 46;
	TestFalse(TEXT("Older versions cannot silently reinterpret facilities"),
	          FCityPersistence::Serialize(Document, Json, Error));
	TestEqual(TEXT("Power selection offers six choices and back"),
	          FCityPaletteModel::Entries(ECityPalettePage::PowerFacilities).Num(), 7);
	TestEqual(TEXT("Water selection offers tower, treatment and back"),
	          FCityPaletteModel::Entries(ECityPalettePage::WaterFacilities).Num(), 3);
	TestTrue(TEXT("Education category visible in Build"),
	         FCityPaletteModel::Entries(ECityPalettePage::Build).ContainsByPredicate([](const auto& E) {
		         return E.Action == ECityPaletteAction::OpenEducationFacilities;
	         }));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityAlternativeIntegrationTest,
                                 "CitySim.Utilities.CollegeToNuclearNativeSaveAndToolSelection",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityAlternativeIntegrationTest::RunTest(const FString& Parameters)
{
	const auto Values = UWorld::InitializationValues()
	                        .AllowAudioPlayback(false)
	                        .CreatePhysicsScene(false)
	                        .CreateNavigation(false)
	                        .CreateAISystem(false)
	                        .ShouldSimulatePhysics(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("AlternativeFacilityCity"), nullptr, true,
	                                  ERHIFeatureLevel::Num, &Values);
	auto* Sim = World->GetSubsystem<UCitySimulationSubsystem>();
	auto* Catalog = LoadObject<UCityBuildingCatalog>(
	    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
	Sim->RegisterBuildingCatalog(Catalog);
	auto& City = Sim->GetPlacementModel();
	City.SetTerrain(nullptr);
	City.ApplyMonthlyEconomy(100000, 0);
	FacilityRoad(City);
	// Reserve the reviewed maximum upgrade sites while retaining the original
	// relative school/college/home distances used by the education progression.
	TestTrue(TEXT("Power4x4 places"),FacilityPlace(City,Catalog,TEXT("Power"),1)!=0);
	TestTrue(TEXT("Water6x4 places"),FacilityPlace(City,Catalog,TEXT("Service"),6)!=0);
	TestTrue(TEXT("School places"),FacilityPlace(City,Catalog,TEXT("School"),13)!=0);
	TestTrue(TEXT("College places"),FacilityPlace(City,Catalog,TEXT("College"),17)!=0);
	const uint64 Nuclear=FacilityPlace(City,Catalog,TEXT("NuclearPower"),22);
	TestTrue(TEXT("Nuclear4x4 places"),Nuclear!=0);
	for(int32 X:{29,33,37})
		TestTrue(TEXT("Home places without overlap"),FacilityPlace(City,Catalog,TEXT("Home"),X)!=0);
	Sim->Development.InitializeExistingAsOperational(City, Catalog, 0);
	Sim->EconomyTuning.FixedStepSeconds = .125;
	Sim->EconomyTuning.FixedStepsPerMonth = 4;
	Sim->EconomyTuning.SchoolMonthsPerLevel = 2;
	Sim->EconomyTuning.FirePopulationThreshold = Sim->EconomyTuning.CrimePopulationThreshold = 100000;
	Sim->SetDisasterFrequency(ECityDisasterFrequency::Off);
	Sim->StartScenario();
	Sim->Tick(.125f);
	TestEqual(TEXT("New city has no qualified nuclear operators"),
	          Sim->GetPopulationSnapshot().Workers(Nuclear), 0);
	FString Error;
	const FString Path =
	    FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/FacilityUpgrades-AlternativeCity.json"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	if (!TestTrue(TEXT("Native alternative save"), Sim->SaveCityToPath(Path, Error)))
	{
		AddError(Error);
		World->DestroyWorld(false);
		return false;
	}
	for (int32 Speed : {1, 3, 5})
	{
		if (!TestTrue(TEXT("Native alternative city reloads"), Sim->LoadCityFromPath(Path, Error, false)))
		{
			AddError(Error);
			break;
		}
		Sim->SetSimulationSpeed(Speed);
		Sim->SetSimulationPaused(false);
		for (int32 Step = 0; Step < 40; ++Step)
			Sim->Tick(.125f / Speed);
		TestTrue(TEXT("Actual college produces Higher graduates at each speed"),
		         Sim->GetPopulationSnapshot().Citizens.ContainsByPredicate(
		             [](const auto& C) { return C.Education == ECityEducationLevel::Higher; }));
		TestTrue(TEXT("Graduates are hired by nuclear plant"),
		         Sim->GetPopulationSnapshot().Workers(Nuclear) > 0);
		TestTrue(TEXT("Nuclear output activates after education"),
		         Sim->GetServiceSnapshot().PowerCapacity > 200);
		TestTrue(TEXT("Economy and citizen jobs reconcile"),
		         Sim->Population.MatchesEconomy(Sim->GetEconomySnapshot(), Sim->EconomyTuning));
	}
	const FString Completed = Path + TEXT(".completed");
	TestTrue(TEXT("Graduated nuclear city saves and reloads"),
	         Sim->SaveCityToPath(Completed, Error) && Sim->LoadCityFromPath(Completed, Error, false));
	auto* Board = World->SpawnActor<ACityBoardActor>();
	Board->BuildingCatalog = Catalog;
	Board->DispatchBeginPlay();
	for (FName Name :
	     {FName(TEXT("WindPower")), FName(TEXT("SolarPower")), FName(TEXT("Power")),
	      FName(TEXT("WaterTower")), FName(TEXT("Service")), FName(TEXT("College")), FName(TEXT("School"))})
	{
		Board->SetSelectedFacility(Name);
		TestEqual(TEXT("Facility selector resolves exact definition"), Board->GetSelectedFacilityId(), Name);
	}
	FString Fixture;
	FCityPersistence::Serialize(Sim->BuildSaveDocument(), Fixture, Error);
	FFileHelper::SaveStringToFile(
	    Fixture,
	    *FPaths::Combine(FPaths::ProjectDir(), TEXT("../Artifacts/Preview/FacilityUpgrades-Alternative-City.json")));
	for (const FString& File : {Path, Path + TEXT(".bak"), Completed, Completed + TEXT(".bak")})
		IFileManager::Get().Delete(*File, false, true, true);
	World->DestroyWorld(false);
	return true;
}
#endif
