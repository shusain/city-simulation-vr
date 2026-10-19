#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Simulation/CityPopulationSimulation.h"
#include "Simulation/CityGrowthSimulation.h"
#include "Simulation/CityNeedsSimulation.h"
#include "Simulation/CityEconomySimulation.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Presentation/CityNeedsPresentation.h"
#include "Persistence/CityPersistence.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityWorkforceEducationTest, "CitySim.Education.WorkforceSummary",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityWorkforceEducationTest::RunTest(const FString& Parameters)
{
	FCityPlacementModel City(100000);
	FCityServiceSnapshot Services;
	uint64 Home = 0, Shop = 0;
	const TCHAR* Definitions[] = {TEXT("Home"),    TEXT("Shop"),   TEXT("Factory"), TEXT("Power"),
	                              TEXT("Service"), TEXT("Police"), TEXT("Fire"),    TEXT("School")};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Definitions); ++Index)
	{
		FCityBuildingDefinition Definition;
		Definition.DefinitionId = Definitions[Index];
		Definition.DisplayName = Definitions[Index];
		Definition.Footprint = {1, 1};
		Definition.Cost = 0;
		auto Preview = City.BeginPreview(Definition, {Index * 2 + 1, 9});
		const auto Placement = City.CommitPlacement(Preview, Definition);
		if (!TestTrue(TEXT("Workforce fixture builds"), Placement.IsSuccess()))
			return false;
		if (Index == 0)
			Home = Placement.EntityId;
		else if (Index <= 2)
		{
			Services.EntityJobs.Add(Placement.EntityId, 8);
			Services.EntityOutput.Add(Placement.EntityId, 8);
			if (Index == 1)
				Shop = Placement.EntityId;
		}
		else
			Services.PublicJobs.Add(Placement.EntityId, 4);
	}
	Services.EntityResidents.Add(Home, 40);
	FCityEconomyTuning Tuning;
	Tuning.NewArrivalUneducatedPercent = 0; // Explicit qualification fixture for summary counts.
	FCityEconomySnapshot Economy;
	Economy.Residents = 40;
	Economy.Jobs = 36;
	Economy.RetailCapacity = 8;
	Economy.GoodsProduced = 8;
	Economy.FixedStepId = 1;
	FCityPopulationSimulation People;
	People.Reconcile(City, Economy, Tuning, &Services);
	auto State = People.GetSnapshot();
	int32 PrivateIndex = 0;
	for (auto& Citizen : State.Citizens)
	{
		if (Citizen.WorkplaceEntityId == Shop)
		{
			if (PrivateIndex < 4)
				Citizen.Education = static_cast<ECityEducationLevel>(PrivateIndex);
			++PrivateIndex;
		}
		else if (!Citizen.WorkplaceEntityId)
			Citizen.Education = ECityEducationLevel::Higher;
	}
	FString Error;
	if (!TestTrue(TEXT("Mixed worker qualifications restore"), People.RestoreState(State, Error)))
		return false;
	const auto Staff = People.GetSnapshot().BuildWorkforceEducation();
	TestEqual(TEXT("All private and five civic employers count exactly once"), Staff.Total(), 36);
	TestEqual(TEXT("Workforce total reconciles to filled jobs"), Staff.Total(),
	          People.BuildAggregate(Tuning).EmployedResidents);
	TestEqual(TEXT("None means uneducated"), Staff.Uneducated, 1);
	TestEqual(TEXT("Basic counts civic and entry workers"), Staff.Basic, 33);
	TestEqual(TEXT("Secondary qualification counts even in an entry role"), Staff.Secondary, 1);
	TestEqual(TEXT("Unemployed Higher citizens are excluded"), Staff.Higher, 1);
	TestEqual(TEXT("Educated includes Basic and above"), Staff.Educated(), 35);
	const auto Needs = FCityNeedsEvaluator::BuildSnapshot(
	    City, Services, Tuning, nullptr, nullptr, false, nullptr, nullptr, 1000, 1000, &People.GetSnapshot());
	TestEqual(TEXT("Needs uses the shared authoritative staff count"), Needs.WorkforceEducation.Total(),
	          Staff.Total());
	const FString Panel = FCityNeedsPresentation::BuildPanel(Needs, Economy);
	TestTrue(TEXT("Summary shows educated and uneducated staff"),
	         Panel.Contains(TEXT("STAFF: 35 educated | 1 uneducated")));
	TestTrue(TEXT("Summary shows exact education tiers"),
	         Panel.Contains(TEXT("Basic 33 | Secondary 1 | Higher 1")));
	FCitySaveDocument Document, Loaded;
	Document.Placement = City.ExportState();
	Document.Population = People.GetSnapshot();
	Document.Economy = Economy;
	FString Json;
	TestTrue(TEXT("Workforce city serializes without a schema change"),
	         FCityPersistence::Serialize(Document, Json, Error));
	TestTrue(TEXT("Workforce city deserializes"), FCityPersistence::Deserialize(Json, Loaded, Error));
	TestEqual(TEXT("Staff qualifications derive identically after save/load"),
	          Loaded.Population.BuildWorkforceEducation().Educated(), Staff.Educated());
	State.Citizens.RemoveAll([](const auto& Citizen) { return Citizen.WorkplaceEntityId == 0; });
	for (auto& Citizen : State.Citizens)
		Citizen.WorkplaceEntityId = 0;
	TestEqual(TEXT("Losing all job assignments clears the breakdown"),
	          State.BuildWorkforceEducation().Total(), 0);
	FCityPopulationSnapshot Empty;
	const auto EmptyNeeds = FCityNeedsEvaluator::BuildSnapshot(City, Services, Tuning, nullptr, nullptr,
	                                                           false, nullptr, nullptr, 1000, 1000, &Empty);
	TestTrue(TEXT("Empty city displays real zero staff"),
	         FCityNeedsPresentation::BuildPanel(EmptyNeeds, Economy)
	             .Contains(TEXT("STAFF: 0 educated | 0 uneducated")));
	TestFalse(TEXT("Unavailable population does not fabricate zero counts"),
	          FCityNeedsPresentation::BuildPanel(FCityNeedsSnapshot(), Economy).Contains(TEXT("STAFF:")));
	return true;
}
#endif
