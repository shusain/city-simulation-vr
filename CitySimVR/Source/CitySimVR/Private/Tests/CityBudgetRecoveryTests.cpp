#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Simulation/CityEconomySimulation.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Presentation/CityNeedsPresentation.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityBudgetRecoveryTest, "CitySim.Economy.BudgetForecastAndRecovery",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityBudgetRecoveryTest::RunTest(const FString& Parameters)
{
	FCityPlacementModel City(100000);
	auto Road = City.BeginRoadPreview(FCityPlacementModel::EntranceCell);
	City.UpdateRoadPreviewEnd(Road, {12, 8});
	City.CommitRoad(Road);
	for (const TCHAR* Id : {TEXT("Power"), TEXT("Service"), TEXT("Home"), TEXT("Shop")})
	{
		FCityBuildingDefinition Def;
		Def.DefinitionId = Id;
		Def.DisplayName = Id;
		Def.Footprint = {1, 1};
		Def.Cost = 0;
		const int32 X = City.GetEntities().Num() * 2 + 1;
		auto P = City.BeginPreview(Def, {X, 9});
		TestTrue(TEXT("Budget fixture placement"), City.CommitPlacement(P, Def).IsSuccess());
	}
	FCityEconomyTuning T;
	T.FixedStepSeconds = .125;
	T.FixedStepsPerMonth = 4;
	City.TrySpendFunds(City.GetFunds());
	FCityEconomySimulation Economy;
	Economy.Advance(.5, City, T);
	TestTrue(TEXT("Actual unpaid bills block growth"), Economy.GetSnapshot().bGrowthBlockedByShortfall);
	const auto Before = Economy.GetSnapshot();
	const auto Baseline = Economy.GetProjectedBudget(City, T);
	TestEqual(TEXT("Named costs reconcile to the exact monthly debit"), Baseline.Expenses, Before.LastUpkeep);
	TestEqual(TEXT("Cost categories reconcile"), Baseline.Expenses,
	          Baseline.RoadUpkeep + Baseline.UtilityUpkeep + Baseline.BuildingUpkeep + Baseline.PublicPayroll);
	const auto Preview = Economy.GetProjectedBudget(City, T, 2500, 1000);
	TestTrue(TEXT("Tax preview exposes an affordable recovery without mutating policy"),
	         Preview.Net() > 0 && Economy.GetSnapshot().ResidentialTaxBasisPoints == 1000);
	TestEqual(TEXT("Preview cannot spend treasury"), City.GetFunds(), int64(0));
	Economy.SetTaxRates(2500, 1000);
	TestTrue(TEXT("Applying tax does not retroactively pay the prior month"),
	         Economy.GetSnapshot().bGrowthBlockedByShortfall);
	Economy.Advance(.5, City, T);
	TestFalse(TEXT("Next fully paid month clears the growth guard"), Economy.GetSnapshot().bGrowthBlockedByShortfall);
	TestEqual(TEXT("Recovered monthly cash matches shared forecast"), City.GetFunds(), Preview.Net());
	TestEqual(TEXT("Preview and billing share exact income"), Economy.GetSnapshot().LastIncome, Preview.Income);

	// The HUD must not present capacity-based goods forecasts as completed income.
	FCityEconomySimulation PreviewEconomy;
	FCityEconomySnapshot Ledger = Economy.GetSnapshot();
	Ledger.GoodsSold = 30;
	Ledger.PublicPayrollDue = 35;
	Ledger.CivicUpkeep = 20;
	Ledger.UtilityUpgradeUpkeep = 15;
	FString PreviewError;
	TestTrue(TEXT("Restore optimistic sales fixture"),
	         PreviewEconomy.RestoreState(Ledger, 0, true, 1, T, PreviewError));
	const auto Optimistic = PreviewEconomy.GetProjectedBudget(City, T);
	for (const int32 Completed : {0, 2, 30})
	{
		const auto Earned = PreviewEconomy.GetProjectedBudget(City, T, -1, -1, -1, Completed);
		TestEqual(TEXT("HUD deducts full bills, including payroll and upgrades, exactly once"), Earned.Expenses,
		          Optimistic.Expenses);
		TestEqual(TEXT("Residential tax policy is preserved"), Earned.ResidentialTax, Optimistic.ResidentialTax);
		TestEqual(TEXT("Commercial revenue follows completed sales"), Earned.CommercialTax,
		          FCityEconomySimulation::CalculateTaxRevenue(Completed, T.IncomePerOperationalShop,
		                                                      Ledger.CommercialTaxBasisPoints) /
		              FMath::Max(1, T.JobsPerOperationalShop));
		TestEqual(TEXT("Industrial revenue follows completed sales"), Earned.IndustrialTax,
		          FCityEconomySimulation::CalculateTaxRevenue(Completed, T.IncomePerOperationalIndustry,
		                                                      Ledger.IndustrialTaxBasisPoints) /
		              FMath::Max(1, T.OutputPerOperationalIndustry));
		TestEqual(TEXT("Net is realized income minus all bills"), Earned.Net(), Earned.Income - Earned.Expenses);
		if (Completed < 30)
			TestTrue(TEXT("Incomplete sales do not promise the full forecast gain"), Earned.Net() < Optimistic.Net());
	}
	TestEqual(TEXT("Read-only HUD preview does not mutate forecast or treasury"),
	          PreviewEconomy.GetSnapshot().GoodsSold, 30);

	// Optional real-world reproduction: load a read-only copy, never the headset save.
	FString SavedCityPath;
	if (FParse::Value(FCommandLine::Get(), TEXT("CitySimBudgetSave="), SavedCityPath))
	{
		FString Original;
		if (!TestTrue(TEXT("Read supplied reproduction save"), FFileHelper::LoadFileToString(Original, *SavedCityPath)))
			return false;
		const auto Values = UWorld::InitializationValues()
		                        .AllowAudioPlayback(false)
		                        .CreatePhysicsScene(false)
		                        .CreateNavigation(false)
		                        .CreateAISystem(false)
		                        .ShouldSimulatePhysics(false);
		UWorld* W = UWorld::CreateWorld(EWorldType::Game, false, TEXT("BudgetSaveRecovery"), nullptr, true,
		                                ERHIFeatureLevel::Num, &Values);
		auto* Sim = W->GetSubsystem<UCitySimulationSubsystem>();
		Sim->RegisterBuildingCatalog(LoadObject<UCityBuildingCatalog>(
		    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog")));
		FString Error;
		if (!TestTrue(*FString::Printf(TEXT("Saved scenario loads: %s"), *Error),
		              Sim->LoadCityFromPath(SavedCityPath, Error, false)))
		{
			AddError(Error);
			W->DestroyWorld(false);
			return false;
		}
		Sim->SetSimulationPaused(true);
		Sim->SetSimulationSpeed(1);
		const auto Initial = Sim->GetEconomySnapshot();
		const auto InitialBudget = Sim->GetProjectedBudget();
		const int32 InitialBuildings = Sim->GetPlacementModel().GetEntities().Num();
		AddInfo(FString::Printf(
		    TEXT(
		        "Saved city: cash=%lld income=%lld bills=%lld unpaid=%lld roads=%lld utilities=%lld other=%lld wages=%lld residential demand=%d power spare=%d water spare=%d"),
		    Sim->GetPlacementModel().GetFunds(), Initial.LastIncome, Initial.LastUpkeep, Initial.UpkeepShortfall,
		    InitialBudget.RoadUpkeep, InitialBudget.UtilityUpkeep, InitialBudget.BuildingUpkeep,
		    InitialBudget.PublicPayroll, Sim->GetNeedsSnapshot().ResidentialDemand,
		    Sim->GetServiceSnapshot().GetAvailablePower(), Sim->GetServiceSnapshot().GetAvailableService()));
		TestTrue(TEXT("Reproduction confirms unpaid bills despite growth demand and utility headroom"),
		         Initial.bGrowthBlockedByShortfall && Sim->GetNeedsSnapshot().ResidentialDemand > 0 &&
		             Sim->GetServiceSnapshot().GetAvailablePower() > 0 &&
		             Sim->GetServiceSnapshot().GetAvailableService() > 0);
		const auto Recovery = Sim->GetProjectedBudget(2500, 1000);
		TestTrue(TEXT("Real saved city has an available tax recovery route"), Recovery.Net() > 0);
		TestTrue(TEXT("Player tax command accepted"), Sim->SetTaxRates(2500, 1000));
		Sim->SetSimulationPaused(false);
		bool PaidMonth = false, Grew = false;
		for (int32 I = 0; I < Sim->GetEconomyTuning().FixedStepsPerMonth * 12; ++I)
		{
			Sim->Tick(static_cast<float>(Sim->GetEconomyTuning().FixedStepSeconds));
			PaidMonth |= !Sim->GetEconomySnapshot().bGrowthBlockedByShortfall;
			Grew |= Sim->GetPlacementModel().GetEntities().Num() > InitialBuildings;
			if (PaidMonth && Grew)
				break;
		}
		TestTrue(TEXT("Saved city's bills become paid after tax adjustment"), PaidMonth);
		TestTrue(TEXT("Saved city resumes actual zoned building growth"), Grew);
		AddInfo(FString::Printf(TEXT("Recovery at month=%lld cash=%lld buildings=%d -> %d last income=%lld bills=%lld"),
		                        Sim->GetEconomySnapshot().MonthId, Sim->GetPlacementModel().GetFunds(),
		                        InitialBuildings, Sim->GetPlacementModel().GetEntities().Num(),
		                        Sim->GetEconomySnapshot().LastIncome, Sim->GetEconomySnapshot().LastUpkeep));
		FString After;
		FFileHelper::LoadFileToString(After, *SavedCityPath);
		TestEqual(TEXT("Reproduction leaves original copy unchanged"), After, Original);
		W->DestroyWorld(false);
	}
	return true;
}
#endif
