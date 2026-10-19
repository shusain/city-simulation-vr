#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Commands/CityPlacementModel.h"
#include "Simulation/CityUtilitySimulation.h"
#include "Simulation/CityEducationSimulation.h"
#include "Simulation/CityGrowthSimulation.h"
#include "Simulation/CityDevelopmentSimulation.h"
#include "Persistence/CityPersistence.h"
#include "Interaction/CityPaletteModel.h"
#include "Engine/StaticMesh.h"
namespace
{
uint64 WeatherPlace(FCityPlacementModel& City, const UCityBuildingCatalog* Catalog, FName Id, int32 X)
{
	const auto* D = Catalog->FindDefinition(Id);
	if (!D)
		return 0;
	auto Preview = City.BeginPreview(*D, {X, 9});
	return City.CommitPlacement(Preview, *D).EntityId;
}
void WeatherRoad(FCityPlacementModel& City)
{
	auto Preview = City.BeginRoadPreview(FCityPlacementModel::EntranceCell);
	City.UpdateRoadPreviewEnd(Preview, {63, 8});
	City.CommitRoad(Preview);
}
} // namespace
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityRenewableWeatherTest, "CitySim.Utilities.WeatherLimitedGeneration",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityRenewableWeatherTest::RunTest(const FString& Parameters)
{
	auto* Catalog = LoadObject<UCityBuildingCatalog>(
	    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
	if (!TestNotNull(TEXT("Catalog"), Catalog))
		return false;
	for (FName Name : {FName(TEXT("WindPower")), FName(TEXT("SolarPower"))})
	{
		FCityPlacementModel City(100000);
		WeatherRoad(City);
		const uint64 Id = WeatherPlace(City, Catalog, Name, 8);
		if (!TestTrue(TEXT("Facility placement"), Id > 0))
			return false;
		if (!TestTrue(TEXT("Weather outage consumer and water supply place"),
		              WeatherPlace(City, Catalog, TEXT("Home"), 14) > 0 &&
		                  WeatherPlace(City, Catalog, TEXT("Service"), 18) > 0))
			return false;
		FCityDevelopmentSimulation Dev;
		Dev.InitializeExistingAsOperational(City, Catalog, 0);
		FCityEconomyTuning T;
		FCityEnvironmentSnapshot Weather;
		const int32 Nominal = Catalog->FindDefinition(Name)->UtilityCapacity;
		for (int32 Strength = 0; Strength <= 2; ++Strength)
			for (bool Rain : {false, true})
				for (int32 Direction = 0; Direction < 4; ++Direction)
				{
					Weather.WindStrength = Strength;
					Weather.bPrecipitation = Rain;
					Weather.WindDirection = static_cast<ECityWindDirection>(Direction);
					const int32 Expected = Name == TEXT("WindPower") ? (Strength == 0   ? 0
					                                                    : Strength == 1 ? 75
					                                                                    : 100)
					                       : Rain                    ? (Strength == 2 ? 35 : 65)
					                                                 : 100;
					const auto Services =
					    FCityServiceEvaluator::BuildSnapshot(City, T, Catalog, &Dev.GetSnapshot(), &Weather);
					TestEqual(
					    TEXT("Weather drives authoritative power supply; yaw direction preserves efficiency"),
					    Services.PowerCapacity, Nominal * Expected / 100);
					FCityEconomySimulation Economy;
					Economy.Advance(T.FixedStepSeconds, City, T, Catalog, &Dev.GetSnapshot(), nullptr, {},
					                nullptr, {}, &Weather);
					TestEqual(TEXT("Economy uses weather-limited supply when evaluating connected homes"),
					          Economy.GetSnapshot().OccupiedHomes,
					          Name == TEXT("WindPower") && Strength == 0 ? 0 : 1);
					FCityPopulationSnapshot Empty;
					TestEqual(TEXT("Weather cannot create power without qualified staff"),
					          FCityServiceEvaluator::BuildSnapshot(City, T, Catalog, &Dev.GetSnapshot(),
					                                               &Weather, &Empty)
					              .PowerCapacity,
					          0);
				}
	}
	FCityEnvironmentSnapshot Rain;
	Rain.bPrecipitation = true;
	Rain.WindStrength = 2;
	TestEqual(TEXT("Coal independent of rain"), CityUtilityWeatherPercent(TEXT("CoalPower"), &Rain), 100);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityEducationStagesTest,
                                 "CitySim.Education.SeparateStagesAndLegacyCompatibility",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityEducationStagesTest::RunTest(const FString& Parameters)
{
	auto* Catalog = LoadObject<UCityBuildingCatalog>(
	    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
	if (!TestNotNull(TEXT("Catalog"), Catalog))
		return false;
	FCityPlacementModel City(100000);
	WeatherRoad(City);
	const uint64 Home = WeatherPlace(City, Catalog, TEXT("Home"), 2);
	const uint64 Primary = WeatherPlace(City, Catalog, TEXT("PrimarySchool"), 6),
	             Secondary = WeatherPlace(City, Catalog, TEXT("SecondarySchool"), 10),
	             College = WeatherPlace(City, Catalog, TEXT("College"), 15);
	if (!TestTrue(TEXT("All education stages place"), Home && Primary && Secondary && College))
		return false;
	FCityPopulationSimulation People;
	FCityEconomySnapshot Economy;
	Economy.Residents = 3;
	Economy.FixedStepId = 1;
	FCityEconomyTuning T;
	T.SchoolMonthsPerLevel = 1;
	FCityServiceSnapshot Homes;
	Homes.EntityResidents.Add(Home, 3);
	People.Reconcile(City, Economy, T, &Homes);
	auto Saved = People.GetSnapshot();
	for (int32 I = 0; I < 3; ++I)
		Saved.Citizens[I].Education = static_cast<ECityEducationLevel>(I);
	FString Error;
	if (!TestTrue(TEXT("Mixed education fixture valid"), People.RestoreState(Saved, Error)))
		return false;
	for (uint64 Provider : {Primary, Secondary, College})
	{
		FCityServiceSnapshot S;
		S.EntityResidents.Add(Home, 3);
		S.PublicStaffPercent.Add(Provider, 100);
		FCityEducationEvaluator::Build(City, S, T, &People.GetSnapshot(), nullptr);
		TestEqual(TEXT("Provider serves only its matching entry level"), S.EducationServed, 1);
		TestEqual(TEXT("No duplicate lower education allocation"), S.EducationCitizenIds.Num(),
		          Provider == College ? 0 : 1);
		TestEqual(TEXT("College only allocates higher education"), S.HigherEducationCitizenIds.Num(),
		          Provider == College ? 1 : 0);
	}
	FCityServiceSnapshot All;
	All.EntityResidents.Add(Home, 3);
	for (uint64 Provider : {Primary, Secondary, College})
		All.PublicStaffPercent.Add(Provider, 100);
	FCityEducationEvaluator::Build(City, All, T, &People.GetSnapshot(), nullptr);
	People.AdvanceEducation(1, All, T);
	for (int32 I = 0; I < 3; ++I)
		TestEqual(TEXT("Each stage awards exactly one level"),
		          static_cast<int32>(People.GetSnapshot().Citizens[I].Education), I + 1);
	TestTrue(TEXT("Legacy combined school continues teaching None and Basic"),
	         FCityBuildingDefinition::CanTeachEducation(TEXT("School"), 0) &&
	             FCityBuildingDefinition::CanTeachEducation(TEXT("School"), 1));
	FCitySaveDocument Doc, Read;
	Doc.Placement = City.ExportState();
	FString Json;
	TestTrue(TEXT("Separate identities save"), FCityPersistence::Serialize(Doc, Json, Error));
	TestTrue(TEXT("Separate identities load"), FCityPersistence::Deserialize(Json, Read, Error));
	Doc.SchemaVersion = 47;
	TestFalse(TEXT("Older schema cannot silently accept new stages"),
	          FCityPersistence::Serialize(Doc, Json, Error));
	const auto Menu = FCityPaletteModel::Entries(ECityPalettePage::EducationFacilities);
	TestEqual(TEXT("Three education choices and Back"), Menu.Num(), 4);
	TestTrue(TEXT("Education returns to Build"),
	         FCityPaletteModel::ParentPage(ECityPalettePage::EducationFacilities) == ECityPalettePage::Build);
	for (FName Name : {FName(TEXT("PrimarySchool")), FName(TEXT("SecondarySchool")), FName(TEXT("College"))})
	{
		auto* Mesh = Catalog->FindDefinition(Name)->PresentationMesh.LoadSynchronous();
		if (TestNotNull(TEXT("Education mesh"), Mesh))
			TestTrue(TEXT("Reviewed opaque windows and civic surface budget"),
			         Mesh->GetNumTriangles(0) <= 800 && Mesh->GetNumSections(0) == 5);
	}
	return true;
}
#endif
