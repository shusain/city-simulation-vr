#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Presentation/CityConditionIndicators.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Simulation/CityHappinessSimulation.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityConditionCauseTest,
                                 "CitySim.Indicators.PurchaseCausesUseCurrentEvidence",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityConditionCauseTest::RunTest(const FString& Parameters)
{
	const auto Init = UWorld::InitializationValues()
	                      .AllowAudioPlayback(false)
	                      .CreatePhysicsScene(false)
	                      .CreateNavigation(false)
	                      .CreateAISystem(false)
	                      .ShouldSimulatePhysics(false);
	auto* W = UWorld::CreateWorld(EWorldType::Game, false, TEXT("ConditionCauses"), nullptr, true,
	                              ERHIFeatureLevel::Num, &Init);
	auto* Sim = W->GetSubsystem<UCitySimulationSubsystem>();
	auto* Catalog = LoadObject<UCityBuildingCatalog>(
	    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
	Sim->RegisterBuildingCatalog(Catalog);
	auto& City = Sim->GetPlacementModel();
	City.SetTerrain(nullptr);
	City.ApplyMonthlyEconomy(10000, 0);
	auto Road = City.BeginRoadPreview(City.GetEntranceCell());
	City.UpdateRoadPreviewEnd(Road, {15, 8});
	City.CommitRoad(Road);
	auto Place = [&](FName Name, FCityCellCoord Cell) {
		const auto* D = Catalog->FindDefinition(Name);
		auto P = City.BeginPreview(*D, Cell);
		return City.CommitPlacement(P, *D).EntityId;
	};
	const uint64 Home = Place(TEXT("Home"), {2, 9}), Shop = Place(TEXT("Shop"), {9, 9});
	if (!TestTrue(TEXT("Cause fixture has actual lots"), Home && Shop))
	{
		W->DestroyWorld(false);
		return false;
	}
	FCityRoadNetworkBuilder::Build(City, FCityRoadDefinition::Local(), Sim->RoadNetworkSnapshot);
	Sim->ServiceSnapshot.EntityFailures.Reset();
	FCityPopulationSnapshot People;
	People.NextCitizenId = 2;
	People.LastSettledMonthId = 1;
	FCityHouseholdRecord H;
	H.Id = H.HomeEntityId = Home;
	H.ConsumptionOrders = 1;
	H.Balance = 4;
	People.Households.Add(H);
	FCityCitizenRecord C;
	C.Id = 1;
	C.HouseholdId = C.HomeEntityId = C.CurrentLocationEntityId = Home;
	C.AvailableMoney = 4;
	C.ConsumptionNeed = 1;
	C.UnemployedSinceStep = 0;
	C.LastHappinessMonth = 1;
	C.HappinessConcerns = static_cast<int32>(ECityHappinessConcern::Purchases);
	People.Citizens.Add(C);
	FCityBusinessLedgerRecord B;
	B.EntityId = Shop;
	B.JobCapacity = 1;
	B.PendingOrders = 1;
	People.Businesses.Add(B);
	FString Error;
	auto Restore = [&]() {
		Error.Reset();
		const bool OK = Sim->Population.RestoreState(People, Error);
		TestTrue(TEXT("Cause fixture validates: ") + Error, OK);
		return OK;
	};
	if (!Restore())
	{
		W->DestroyWorld(false);
		return false;
	}
	const int64 Cash = City.GetFunds();
	const uint64 Revision = City.GetRevision();
	TestTrue(TEXT("Known current cash limit is distinguished"),
	         FCityConditionIndicators::Explain(*Sim, Home).Contains(TEXT("unreserved cash")));
	TestEqual(TEXT("Supply accordion gives a concise cash blocker"), FCityConditionIndicators::ShoppingBlocker(*Sim, Home), FString(TEXT("Insufficient available cash")));
	People.Citizens[0].AvailableMoney = People.Households[0].Balance = 50;
	if (!Restore())
	{
		W->DestroyWorld(false);
		return false;
	}
	TestTrue(TEXT("Known stock limit is distinguished"),
	         FCityConditionIndicators::Explain(*Sim, Home).Contains(TEXT("no unreserved stock")));
	TestEqual(TEXT("Supply accordion gives a concise stock blocker"), FCityConditionIndicators::ShoppingBlocker(*Sim, Home), FString(TEXT("No available shop stock")));
	People.Businesses[0].Inventory = 1;
	People.Businesses[0].PendingOrders = 0;
	if (!Restore())
	{
		W->DestroyWorld(false);
		return false;
	}
	TestTrue(TEXT("Known retail slot limit is distinguished"),
	         FCityConditionIndicators::Explain(*Sim, Home).Contains(TEXT("no free purchase slots")));
	TestEqual(TEXT("Supply accordion gives a concise capacity blocker"), FCityConditionIndicators::ShoppingBlocker(*Sim, Home), FString(TEXT("No available shop capacity")));
	People.Businesses[0].PendingOrders = 1;
	if (!Restore())
	{
		W->DestroyWorld(false);
		return false;
	}
	TestTrue(
	    TEXT("Available current supply cannot diagnose a historical failure"),
	    FCityConditionIndicators::Explain(*Sim, Home).Contains(TEXT("Yesterday's shortfall is recorded separately")));
	People.bUsesPurchaseReservations = true;
	People.Citizens[0].PendingShopEntityId = Shop;
	FCityPurchaseReservation Order;
	Order.BuyerId = 1;
	Order.HouseholdId = Home;
	Order.ShopId = Shop;
	Order.Price = 5;
	Order.Contributions.Add({1, 5});
	People.PurchaseReservations.Add(Order);
	if (!Restore())
	{
		W->DestroyWorld(false);
		return false;
	}
	TestTrue(TEXT("Actual traffic route supports the reserved purchase"),
	         FCityConditionIndicators::Explain(*Sim, Home).Contains(TEXT("walk route available")));
	People.Citizens[0].Schedule = ECityCitizenSchedule::Work;
	People.Citizens[0].Activity = ECityCitizenActivity::Work;
	People.Citizens[0].WorkplaceEntityId = Shop;
	People.Citizens[0].CurrentLocationEntityId = Shop;
	People.Citizens[0].NextActionStep = 90;
	if (!Restore()) { W->DestroyWorld(false); return false; }
	TestTrue(TEXT("Work dwell is not mislabeled as departure wait"),
	         FCityConditionIndicators::Explain(*Sim, Home).Contains(TEXT("Buyer at work until")));
	TestEqual(TEXT("Supply accordion distinguishes work"), FCityConditionIndicators::ShoppingBlocker(*Sim, Home), FString(TEXT("Buyer at work")));
	People.Citizens[0].Schedule = ECityCitizenSchedule::ToWork;
	People.Citizens[0].CurrentLocationEntityId = Home;
	People.Citizens[0].TripPurpose = ECityTripPurpose::Commute;
	People.Citizens[0].TripDestinationEntityId = Shop;
	if (!Restore()) { W->DestroyWorld(false); return false; }
	TestTrue(TEXT("Commute before shopping is explicit"),
	         FCityConditionIndicators::Explain(*Sim, Home).Contains(TEXT("Buyer commuting to work")));
	TestEqual(TEXT("Supply accordion distinguishes commute"), FCityConditionIndicators::ShoppingBlocker(*Sim, Home), FString(TEXT("Buyer commuting")));
	People.Citizens[0].Schedule = ECityCitizenSchedule::Home;
	People.Citizens[0].Activity = ECityCitizenActivity::Home;
	People.Citizens[0].TripPurpose = ECityTripPurpose::None;
	People.Citizens[0].TripDestinationEntityId = 0;
	if (!Restore()) { W->DestroyWorld(false); return false; }
	FString Label;
	TestEqual(TEXT("Provider shortcut identifies actual reserved shop"),
	          FCityConditionIndicators::RelatedLot(*Sim, Home, Label), Shop);
	TestTrue(TEXT("Route break removes an actual road cell"), City.RemoveRoad({5, 8}));
	FCityRoadNetworkBuilder::Build(City, FCityRoadDefinition::Local(), Sim->RoadNetworkSnapshot);
	TestTrue(TEXT("Broken route uses actual traffic eligibility"),
	         FCityConditionIndicators::Explain(*Sim, Home).Contains(TEXT("no valid walk route")));
	TestEqual(TEXT("Supply accordion distinguishes an actual route break"), FCityConditionIndicators::ShoppingBlocker(*Sim, Home), FString(TEXT("No shopping route")));
	TestEqual(TEXT("Only explicit road removal changed authority"), City.GetRevision(), Revision + 1);
	TestTrue(TEXT("Explanations never charge funds"), City.GetFunds() >= Cash);
	People.Citizens[0].HappinessConcerns = 0;
	if (!Restore())
	{
		W->DestroyWorld(false);
		return false;
	}
	TestTrue(TEXT("Settled purchase recovery clears concern"),
	         FCityConditionIndicators::Explain(*Sim, Home).IsEmpty());
	// Exercise generic operating-prerequisite navigation with a transient catalog fixture.
	auto* Extended = NewObject<UCityBuildingCatalog>();
	Extended->Definitions = Catalog->Definitions;
	FCityBuildingDefinition Dependency;
	Dependency.DefinitionId = TEXT("ConditionIconsDependencyProbe");
	Dependency.DisplayName = TEXT("Dependency probe");
	Dependency.Cost = 0;
	Dependency.Footprint = {1, 1};
	Dependency.RequiredOperatingFacilities.Add(TEXT("Shop"));
	Extended->Definitions.Add(Dependency);
	Sim->RegisterBuildingCatalog(Extended);
	FCityServiceSnapshot Ready;
	Ready.PlacementRevision = City.GetRevision();
	Ready.OperatingFacilities.Add(TEXT("Shop"), 1);
	auto Preview = City.BeginPreview(Dependency, {20, 20});
	const auto Placed =
	    City.CommitPlacement(Preview, Dependency, ECityConstructionFunding::CityTreasury, &Ready);
	if (TestTrue(TEXT("Prerequisite probe uses the actual construction gate"), Placed.IsSuccess()))
	{
		Sim->ServiceSnapshot.EntityFailures.Add(Placed.EntityId,
		                                        ECityOperationFailure::TechnologyPrerequisite);
		Sim->ServiceSnapshot.OperatingFacilities.Remove(TEXT("Shop"));
		TestEqual(TEXT("Failed prerequisite points to an actual built provider"),
		          FCityConditionIndicators::RelatedLot(*Sim, Placed.EntityId, Label), Shop);
		City.DemolishEntity(Shop);
		TestEqual(TEXT("Missing prerequisite never invents a provider"),
		          FCityConditionIndicators::RelatedLot(*Sim, Placed.EntityId, Label), uint64(0));
	}
	W->DestroyWorld(false);
	return true;
}
#endif
