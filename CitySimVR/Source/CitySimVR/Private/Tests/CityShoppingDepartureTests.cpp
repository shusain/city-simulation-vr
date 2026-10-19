#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Presentation/CityConditionIndicators.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityShoppingDepartureSaveTest, "CitySim.Traffic.ReportedShoppingCity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FCityShoppingDepartureSaveTest::RunTest(const FString& Parameters)
{
	FString Path, Output, Before, Error;
	if (!FParse::Value(FCommandLine::Get(), TEXT("ShoppingSave="), Path))
	{ AddInfo(TEXT("Supply -ShoppingSave and -ShoppingOutput for the preserved reported-city fixture")); return true; }
	if (!FParse::Value(FCommandLine::Get(), TEXT("ShoppingOutput="), Output) ||
		!FFileHelper::LoadFileToString(Before, *Path))
	{ AddError(TEXT("Requires readable Shopping input and isolated output directory")); return false; }
	const FString ReloadPath = Output / TEXT("ReloadCity.json");
	if (FPaths::IsSamePath(Path, ReloadPath))
	{ AddError(TEXT("Output must differ from input city")); return false; }
	IFileManager::Get().MakeDirectory(*Output, true);
	const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
		.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("ShoppingShoppingFix"), nullptr,
		true, ERHIFeatureLevel::Num, &Values);
	UWorld* ReloadWorld = UWorld::CreateWorld(EWorldType::Game, false, TEXT("ShoppingShoppingReload"), nullptr,
		true, ERHIFeatureLevel::Num, &Values);
	auto* Sim = World->GetSubsystem<UCitySimulationSubsystem>();
	auto* Reload = ReloadWorld->GetSubsystem<UCitySimulationSubsystem>();
	auto* Catalog = LoadObject<UCityBuildingCatalog>(nullptr,
		TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
	Sim->RegisterBuildingCatalog(Catalog); Reload->RegisterBuildingCatalog(Catalog);
	bool bLoaded = TestTrue(TEXT("reported format67 city loads"), Sim->LoadCityFromPath(Path, Error, false));
	if (!bLoaded) { AddError(Error); World->DestroyWorld(false); ReloadWorld->DestroyWorld(false); return false; }
	Sim->SetSimulationSpeed(1); Sim->SetSimulationPaused(false);
	const int64 InitialStep = Sim->GetEconomySnapshot().FixedStepId;
	const int64 InitialPurchases = Sim->GetPopulationSnapshot().TotalArrivalPurchases;
	int32 Departed = -1, Arrived = -1;
	FString Report;
	bool bReloaded = false, bSawBlocked = false, bSawTravelling = false;
	for (int32 Day = 1; Day <= 400; ++Day)
	{
		Sim->Tick(static_cast<float>(Sim->GetEconomyTuning().FixedStepSeconds));
		if (bReloaded) Reload->Tick(static_cast<float>(Reload->GetEconomyTuning().FixedStepSeconds));
		const auto& Traffic = Sim->GetTrafficSnapshot();
		const auto& People = Sim->GetPopulationSnapshot();
		const auto* V = Traffic.Vehicles.FindByPredicate([](const auto& Item) {
			return !Item.bTransitTrip && Item.SourceId == 23 && Item.OriginEntityId == 5 && Item.DestinationEntityId == 8;
		});
		if (V && Departed < 0) Departed = Day;
		const auto* C = People.Citizens.FindByPredicate([](const auto& Item) { return Item.Id == 23; });
		if (C && C->CurrentLocationEntityId == 8 && Arrived < 0)
		{
			Arrived = Day;
			TestFalse(TEXT("actual arrival consumes target purchase reservation"), People.PurchaseReservations.ContainsByPredicate(
				[](const auto& Order) { return Order.BuyerId == 23 && Order.ShopId == 8; }));
			TestTrue(TEXT("actual visits increase completed purchases"), People.TotalArrivalPurchases > InitialPurchases);
		}
		const FString Explanation = FCityConditionIndicators::Explain(*Sim, 6);
		if (!bSawBlocked && Explanation.Contains(TEXT("Departure blocked by traffic")))
		{
			bSawBlocked = true;
			Report += TEXT("BLOCKED: ") + Explanation + TEXT("\n");
		}
		if (!bSawTravelling && Explanation.Contains(TEXT("Shopper travelling")))
		{
			bSawTravelling = true;
			Report += TEXT("TRAVELLING: ") + Explanation + TEXT("\n");
		}
		FCityTrafficSimulation Safe;
		if (!TestTrue(TEXT("reported city retains safe traffic gaps"),
			Safe.RestoreState(Traffic, Sim->GetRoadNetworkSnapshot(), InitialStep + Day, Error)))
		{ AddError(Error); break; }
		if (bReloaded)
		{
			const auto& RT = Reload->GetTrafficSnapshot();
			TestEqual(TEXT("reload preserves actual purchases"), Reload->GetPopulationSnapshot().TotalArrivalPurchases,
				People.TotalArrivalPurchases);
			TestEqual(TEXT("reload preserves waiting queue size"), RT.WaitingDepartures.Num(), Traffic.WaitingDepartures.Num());
			if (!TestEqual(TEXT("reload preserves moving records"), RT.Vehicles.Num(), Traffic.Vehicles.Num())) break;
			for (int32 I = 0; I < Traffic.Vehicles.Num(); ++I)
			{
				TestEqual(TEXT("reload preserves identity"), RT.Vehicles[I].Id, Traffic.Vehicles[I].Id);
				TestEqual(TEXT("reload preserves movement"), RT.Vehicles[I].DistanceUnits, Traffic.Vehicles[I].DistanceUnits);
				TestEqual(TEXT("reload preserves route progress"), RT.Vehicles[I].RouteLegIndex, Traffic.Vehicles[I].RouteLegIndex);
			}
		}
		if (Day == 4)
		{
			Report += Explanation + TEXT("\n");
			TestTrue(TEXT("copied city saves with blocked departures"), Sim->SaveCityToPath(ReloadPath, Error));
			bReloaded = TestTrue(TEXT("copied city reloads with blocked departures"), Reload->LoadCityFromPath(ReloadPath, Error, false));
			Reload->SetSimulationSpeed(1); Reload->SetSimulationPaused(false);
		}
		if (Day % 25 == 0)
		{
			Report += FString::Printf(TEXT("day=%d lastMonthSales=%d totalPurchases=%lld waiting=%d\n"),
				Day, People.LastMonthSales, People.TotalArrivalPurchases, Traffic.WaitingDepartures.Num());
			if (V)
				Report += FString::Printf(TEXT("C23 leg=%d edge=%d distance=%d speed=%d completed=%d\n"),
					V->RouteLegIndex, V->Route[V->RouteLegIndex].EdgeId, V->DistanceUnits, V->SpeedUnitsPerStep, V->bCompleted);
		}
	}
	TestTrue(TEXT("old shopper eventually departs"), Departed > 0 && Departed <= 50);
	TestTrue(TEXT("old shopper actually arrives"), Arrived > Departed && Arrived <= 400);
	TestTrue(TEXT("inspection distinguishes traffic-blocked departure or prompt departure"), bSawBlocked || Departed <= 8);
	TestTrue(TEXT("inspection distinguishes actual travelling shopper"), bSawTravelling);
	Report += FString::Printf(TEXT("Shopping C23 departureDay=%d arrivalDay=%d reload=%d initialPurchases=%lld finalPurchases=%lld\n"),
		Departed, Arrived, bReloaded, InitialPurchases, Sim->GetPopulationSnapshot().TotalArrivalPurchases);
	AddInfo(Report);
	Sim->SaveCityToPath(Output / TEXT("AfterContinuation.json"), Error);
	FFileHelper::SaveStringToFile(Report, *(Output / TEXT("ShoppingFix.txt")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	FString After; FFileHelper::LoadFileToString(After, *Path);
	TestEqual(TEXT("reported city input untouched"), After, Before);
	World->DestroyWorld(false); ReloadWorld->DestroyWorld(false);
	return true;
}
#endif
