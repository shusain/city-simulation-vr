#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Simulation/CitySimulationSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityMonthlyReconciliationTest, "CitySim.Performance.MonthlyReconciliation",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityMonthlyReconciliationTest::RunTest(const FString& Parameters)
{
	const FString OriginalCommandLine = FCommandLine::Get();
	FCommandLine::Set(*(
	    OriginalCommandLine +
	    TEXT(
	        " -CityBenchmarkLots=1000 -CityBenchmarkPopulated -CityBenchmarkLateGame -CityBenchmarkSpeed=1 -CityBenchmarkWeather=Clear -CityBenchmarkCitizenRoutes=128 -CityBenchmarkVisibleTraffic=512")));
	const auto Values = UWorld::InitializationValues()
	                        .AllowAudioPlayback(false)
	                        .CreatePhysicsScene(false)
	                        .CreateNavigation(false)
	                        .CreateAISystem(false)
	                        .ShouldSimulatePhysics(false);
	auto* World =
	    UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	auto* Sim = World->GetSubsystem<UCitySimulationSubsystem>();
	auto* Catalog = LoadObject<UCityBuildingCatalog>(
	    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
	Sim->RegisterBuildingCatalog(Catalog);
	FCommandLine::Set(*OriginalCommandLine);
	TestTrue(TEXT("Dense late-game fixture initialized"), Sim->IsBenchmarkActive());
	TestEqual(TEXT("Fixture starts with 5960 actual residents"), Sim->Population.GetSnapshot().Citizens.Num(), 5960);
	for (int32 Day = 0; Day < 350; ++Day)
	{
		const int64 PreviousMonth = Sim->Economy.GetSnapshot().LastBilledMonthId;
		Sim->Tick(Sim->EconomyTuning.FixedStepSeconds);
		if (Sim->Economy.GetSnapshot().LastBilledMonthId == PreviousMonth)
			continue;
		const auto Actual = Sim->Population.BuildAggregate(Sim->EconomyTuning);
		const auto& Economy = Sim->Economy.GetSnapshot();
		AddInfo(FString::Printf(
		    TEXT("Context: detained=%d medical=%d healthIntroduction=%lld healthCheck=%lld arrests=%lld"),
		    Sim->Population.GetSnapshot()
		        .Citizens.FilterByPredicate([](const auto& C) { return C.IsDetained(); })
		        .Num(),
		    Sim->Population.GetSnapshot()
		        .Citizens.FilterByPredicate([](const auto& C) { return C.bOnMedicalLeave; })
		        .Num(),
		    Sim->Health.GetSnapshot().IntroductionStep, Sim->Health.GetSnapshot().LastCheckStep,
		    static_cast<int64>(Sim->Crime.GetSnapshot().TotalArrests)));
		AddInfo(FString::Printf(
		    TEXT(
		        "Month=%lld actual/current residents=%d/%d jobs=%d/%d employed=%d/%d demand=%d/%d production=%d/%d retail=%d/%d sales=%d/%d income=%lld/%lld spending=%lld/%lld"),
		    Economy.LastBilledMonthId, Actual.Residents, Economy.Residents, Actual.Jobs, Economy.Jobs,
		    Actual.EmployedResidents, Economy.EmployedResidents, Actual.HouseholdGoodsDemand,
		    Economy.HouseholdGoodsDemand, Actual.GoodsProduced, Economy.GoodsProduced, Actual.RetailCapacity,
		    Economy.RetailCapacity, Actual.GoodsSold, Economy.GoodsSold, Actual.HouseholdIncome,
		    Economy.HouseholdIncome, Actual.HouseholdSpending, Economy.HouseholdSpending));
	}
	TestTrue(TEXT("Dense fixture crosses sixteen settlements including later illness"),
	         Sim->Economy.GetSnapshot().LastBilledMonthId >= 16);
	World->DestroyWorld(false);
	return true;
}
#endif
