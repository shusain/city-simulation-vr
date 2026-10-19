#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Persistence/CityPersistence.h"
#include "Presentation/CityNeedsPresentation.h"
#include "Simulation/CitySimulationSubsystem.h"

namespace
{
UCityBuildingCatalog* DensityCatalog()
{
	return LoadObject<UCityBuildingCatalog>(
	    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
}

void CountDensities(const FCityPlacementModel& City, const FCityDevelopmentSnapshot& Development,
                    const FName Id, int32 (&Counts)[3])
{
	for (const auto& Pair : City.GetEntities())
		if (Pair.Value.DefinitionId == Id)
			if (const auto* State = Development.Find(Pair.Key))
				++Counts[static_cast<int32>(State->DensityLevel)];
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityDensityPreferenceTest,
                                 "CitySim.Progression.DensityPreferencesAndReservations",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityDensityPreferenceTest::RunTest(const FString& Parameters)
{
	auto* Catalog = DensityCatalog();
	if (!TestNotNull(TEXT("authored catalog"), Catalog))
		return false;
	const auto* Home = Catalog->FindDefinition(TEXT("Home"));
	const auto* Shop = Catalog->FindDefinition(TEXT("Shop"));
	const auto* Industry = Catalog->FindDefinition(TEXT("Factory"));
	FCityPlacementModel City(0);
	FCityDevelopmentSnapshot Development;
	FCityServiceSnapshot Services;
	FCityEconomyTuning Tuning;
	FCityNeedsSnapshot Needs;
	FCityNeedsEvaluator::BuildDensityDemand(Needs, City, Services, Development, Tuning, *Home, Shop, Industry,
	                                        nullptr);
	TestEqual(TEXT("no-job/no-house city bootstraps one low home"), Needs.ResidentialDensity.NewLowDemand(),
	          10);
	TestTrue(TEXT("starter explanation is explicit"), Needs.bStarterHousingDemand);
	TestEqual(TEXT("no residents means no artificial shop demand"), Needs.CommercialDensity.NewLowDemand(),
	          0);
	TestEqual(TEXT("no residents means no artificial industrial demand"),
	          Needs.IndustrialDensity.NewLowDemand(), 0);
	auto Preview = City.BeginPreview(*Home, {10, 10});
	const uint64 Id =
	    City.CommitPlacement(Preview, *Home, ECityConstructionFunding::PrivateDeveloper).EntityId;
	TestTrue(TEXT("starter private lot exists"), Id != 0);
	FCityNeedsEvaluator::BuildDensityDemand(Needs, City, Services, Development, Tuning, *Home, Shop, Industry,
	                                        nullptr);
	TestEqual(TEXT("pending construction reserves the starter allowance"),
	          Needs.ResidentialDensity.NewLowDemand(), 0);
	TestFalse(TEXT("reserved starter is not permanent artificial demand"), Needs.bStarterHousingDemand);
	Services.Residents = 10;
	Needs.HouseholdGoodsDemand = 5;
	FCityNeedsEvaluator::BuildDensityDemand(Needs, City, Services, Development, Tuning, *Home, Shop, Industry,
	                                        nullptr);
	TestTrue(TEXT("first unemployed households create shop demand"),
	         Needs.CommercialDensity.NewLowDemand() > 0);
	TestTrue(TEXT("first unemployed households create industrial demand"),
	         Needs.IndustrialDensity.NewLowDemand() > 0);
	Services.Residents = 1000;
	Needs.JobCapacity = 1000;
	Needs.HouseholdGoodsDemand = 500;
	Needs.EmployedJobs = 1000;
	Needs.WorkforceEducation.Secondary = 750;
	Needs.WorkforceEducation.Higher = 250;
	FCityNeedsEvaluator::BuildDensityDemand(Needs, City, Services, Development, Tuning, *Home, Shop, Industry,
	                                        nullptr);
	const auto Healthy = Needs;
	TestTrue(TEXT("mature business market leaves bounded employer expansion room"),
	         Healthy.CommercialDensity.Desired[0] + Healthy.CommercialDensity.Desired[1] +
	                 Healthy.CommercialDensity.Desired[2] >=
	             625);
	TestTrue(TEXT("industry has matching workforce expansion room"),
	         Healthy.IndustrialDensity.Desired[0] + Healthy.IndustrialDensity.Desired[1] +
	                 Healthy.IndustrialDensity.Desired[2] >=
	             625);
	for (const auto Type : {ECityDevelopmentDemandType::Residential, ECityDevelopmentDemandType::Commercial,
	                        ECityDevelopmentDemandType::Industrial})
	{
		const auto& Market = Needs.DensityFor(Type);
		TestTrue(TEXT("mature working market retains substantial low preference"),
		         Market.Desired[0] >= (Market.Desired[0] + Market.Desired[1] + Market.Desired[2]) / 2);
		TestTrue(TEXT("mature market supports medium and high"),
		         Market.Desired[1] > 0 && Market.Desired[2] > 0);
		TestEqual(TEXT("medium preference is whole buildings"),
		          Market.Desired[1] % Market.BuildingCapacity[1], 0);
		TestEqual(TEXT("high preference is whole buildings"), Market.Desired[2] % Market.BuildingCapacity[2],
		          0);
	}
	Needs.EmployedJobs = 0;
	FCityNeedsEvaluator::BuildDensityDemand(Needs, City, Services, Development, Tuning, *Home, Shop, Industry,
	                                        nullptr);
	TestEqual(TEXT("unemployment removes high-density pressure"), Needs.ResidentialDensity.Desired[2], 0);
	TestTrue(TEXT("working skilled households prefer more medium capacity"),
	         Healthy.ResidentialDensity.Desired[1] > Needs.ResidentialDensity.Desired[1]);
	FCityDevelopmentState State;
	State.EntityId = Id;
	State.DensityLevel = ECityDensityLevel::High;
	Development.States.Add(Id, State);
	Services.Residents = 0;
	Needs.JobCapacity = 0;
	FCityNeedsEvaluator::BuildDensityDemand(Needs, City, Services, Development, Tuning, *Home, Shop, Industry,
	                                        nullptr);
	TestEqual(TEXT("legacy high capacity suppresses compensating low overbuild"),
	          Needs.ResidentialDensity.NewLowDemand(), 0);
	TestEqual(TEXT("preferences do not downgrade existing lots"), Development.Find(Id)->DensityLevel,
	          ECityDensityLevel::High);
	TestTrue(TEXT("shared guidance explains density and starter policy"),
	         FCityNeedsPresentation::BuildGuidance(Healthy, {}).Contains(TEXT("Low / Medium / High")));
	// A fully reserved market suppresses further construction even when every
	// reserved lot is still waiting for delivery. Demand is not an arrival stream.
	FCityDensityDemand ReservedMarket = Healthy.CommercialDensity;
	for (int32 Tier = 0; Tier < 3; ++Tier)
		ReservedMarket.Reserved[Tier] = ReservedMarket.Desired[Tier];
	TestEqual(TEXT("reserved expansion capacity stops duplicate lots"), ReservedMarket.NewLowDemand(), 0);
	Tuning.PrivateJobTargetOccupancyPercent = 100;
	Services.Residents = 1000;
	FCityNeedsEvaluator::BuildDensityDemand(Needs, City, Services, Development, Tuning, *Home, Shop, Industry,
	                                        nullptr);
	TestTrue(TEXT("full occupancy tuning reproduces closed workforce demand"),
	         Needs.CommercialDensity.Desired[0] + Needs.CommercialDensity.Desired[1] +
	                 Needs.CommercialDensity.Desired[2] <=
	             510);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityDensityMixTest, "CitySim.Progression.DensityMixSavedContinuation",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityDensityMixTest::RunTest(const FString& Parameters)
{
	auto* Catalog = DensityCatalog();
	if (!TestNotNull(TEXT("authored catalog"), Catalog))
		return false;
	FCityPlacementModel City(0);
	for (int32 Family = 0; Family < 3; ++Family)
	{
		const auto* Definition = Catalog->FindDefinition(Family == 0   ? TEXT("Home")
		                                                 : Family == 1 ? TEXT("Shop")
		                                                               : TEXT("Factory"));
		for (int32 Index = 0; Index < 40; ++Index)
		{
			auto Preview = City.BeginPreview(*Definition, {Index % 20 * 3, 9 + Family * 9 + Index / 20 * 3});
			TestTrue(TEXT("private controlled-market fixture commits"),
			         City.CommitPlacement(Preview, *Definition, ECityConstructionFunding::PrivateDeveloper)
			             .IsSuccess());
		}
	}
	FCityNeedsSnapshot Needs;
	Needs.bHasDensityDemand = true;
	for (const auto Type : {ECityDevelopmentDemandType::Residential, ECityDevelopmentDemandType::Commercial,
	                        ECityDevelopmentDemandType::Industrial})
	{
		auto& Market = Type == ECityDevelopmentDemandType::Residential  ? Needs.ResidentialDensity
		               : Type == ECityDevelopmentDemandType::Commercial ? Needs.CommercialDensity
		                                                                : Needs.IndustrialDensity;
		const auto* Definition =
		    Catalog->FindDefinition(Type == ECityDevelopmentDemandType::Residential  ? TEXT("Home")
		                            : Type == ECityDevelopmentDemandType::Commercial ? TEXT("Shop")
		                                                                             : TEXT("Factory"));
		for (int32 Index = 0; Index < 3; ++Index)
		{
			Market.BuildingCapacity[Index] = FCityDensityDemand::Capacity(
			    *Definition->FindDensityLevel(static_cast<ECityDensityLevel>(Index)), Type);
			const int32 TargetLots = Type == ECityDevelopmentDemandType::Industrial ? (Index == 0   ? 29
			                                                                           : Index == 1 ? 10
			                                                                                        : 1)
			                                                                        : (Index == 0   ? 33
			                                                                           : Index == 1 ? 6
			                                                                                        : 1);
			Market.Desired[Index] = TargetLots * Market.BuildingCapacity[Index];
		}
	}
	FCityServiceSnapshot Services;
	Services.PowerCapacity = Services.ServiceCapacity = 50000;
	FCityEconomyTuning Tuning;
	Tuning.BaseResidentialDesirability = 100;
	FCityEnvironmentSnapshot Environment;
	FCityDevelopmentSimulation Development;
	Development.AdvanceToStep(80, City, Catalog, Services, Needs, Environment, Tuning);
	FCitySaveDocument Saved;
	Saved.Placement = City.ExportState();
	Saved.Economy.FixedStepId = 80;
	Saved.Economy.MonthId = Saved.Economy.LastBilledMonthId = 3;
	Saved.Development = Development.GetSnapshot();
	Saved.bSimulationPaused = true;
	FString Json, Error;
	FCitySaveDocument Loaded;
	TestTrue(TEXT("partial density mix saves"), FCityPersistence::Serialize(Saved, Json, Error));
	if (!TestTrue(TEXT("partial density mix parses"), FCityPersistence::Deserialize(Json, Loaded, Error)))
		return false;
	TMap<FName, FCityBuildingDefinition> Definitions;
	for (const auto& Definition : Catalog->Definitions)
		Definitions.Add(Definition.DefinitionId, Definition);
	auto Restored = FCityPlacementModel::CreateValidatedFromState(Loaded.Placement, Definitions, Error);
	if (!TestTrue(TEXT("saved model restores"), Restored.IsValid()))
		return false;
	FCityDevelopmentSimulation Continued;
	if (!TestTrue(TEXT("saved levels and ages restore"),
	              Continued.RestoreState(Loaded.Development, *Restored, Catalog, 80, Error)))
		return false;
	Continued.AdvanceToStep(80, *Restored, Catalog, Services, Needs, Environment, Tuning);
	TestTrue(TEXT("pause is preserved"), Loaded.bSimulationPaused);
	Development.AdvanceToStep(400, City, Catalog, Services, Needs, Environment, Tuning);
	for (int64 Step = 81; Step <= 400; ++Step)
		Continued.AdvanceToStep(Step, *Restored, Catalog, Services, Needs, Environment, Tuning);
	for (const auto Name : {FName(TEXT("Home")), FName(TEXT("Shop")), FName(TEXT("Factory"))})
	{
		int32 Counts[3] = {0}, ReloadCounts[3] = {0};
		CountDensities(City, Development.GetSnapshot(), Name, Counts);
		CountDensities(*Restored, Continued.GetSnapshot(), Name, ReloadCounts);
		TestEqual(TEXT("low-density stock survives many months"), Counts[0],
		          Name == TEXT("Factory") ? 29 : 33);
		TestEqual(TEXT("warranted medium mix completes"), Counts[1], Name == TEXT("Factory") ? 10 : 6);
		TestEqual(TEXT("warranted high mix completes"), Counts[2], 1);
		for (int32 Index = 0; Index < 3; ++Index)
			TestEqual(TEXT("fine/reloaded and batched continuation agree"), ReloadCounts[Index],
			          Counts[Index]);
	}
	TestEqual(TEXT("private redevelopment never spends treasury"), City.GetFunds(), int64(0));
	TestEqual(TEXT("density preference cannot spawn extra lots"), City.GetEntities().Num(), 120);
	for (const auto& Pair : City.GetEntities())
		TestEqual(TEXT("identity and original cost survive"),
		          Restored->GetEntities().FindChecked(Pair.Key).RecordedPlacementCost, int64(0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityDensityNormalCityTest, "CitySim.Progression.NormalDensityGrowthReplay",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityDensityNormalCityTest::RunTest(const FString& Parameters)
{
	const auto Values = UWorld::InitializationValues()
	                        .AllowAudioPlayback(false)
	                        .CreatePhysicsScene(false)
	                        .CreateNavigation(false)
	                        .CreateAISystem(false)
	                        .ShouldSimulatePhysics(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
	                                    ERHIFeatureLevel::Num, &Values);
	auto* Sim = World->GetSubsystem<UCitySimulationSubsystem>();
	auto* Catalog = DensityCatalog();
	if (!TestNotNull(TEXT("authored catalog loads"), Catalog))
	{
		World->DestroyWorld(false);
		return false;
	}
	Sim->RegisterBuildingCatalog(Catalog);
	FCityIdentity Identity;
	Identity.Id = FGuid::NewGuid().ToString(EGuidFormats::Digits).ToLower();
	Identity.Name = TEXT("Normal density progression");
	Identity.DisasterFrequency = ECityDisasterFrequency::Off;
	FCityTerrainGenerationParameters Terrain;
	Terrain.Seed = 7429;
	const FString Directory = FPaths::ProjectSavedDir() / TEXT("Automation/DensityDemand") / Identity.Id;
	FString Error;
	if (!TestTrue(TEXT("Normal city creates"), Sim->CreateCity(Identity, Terrain, Directory, Error)))
	{
		AddError(Error);
		World->DestroyWorld(false);
		return false;
	}
	auto& City = Sim->GetPlacementModel();
	TestEqual(TEXT("ordinary starting funds"), City.GetFunds(), int64(10000));
	auto Road = City.BeginRoadPreview(FCityPlacementModel::EntranceCell);
	Road.End = {52, 8};
	TestTrue(TEXT("growth frontier road purchased"), City.CommitRoad(Road).IsSuccess());
	for (int32 Index = 0; Index < 6; ++Index)
	{
		const auto* Definition = Catalog->FindDefinition(Index % 2 == 0 ? TEXT("Power") : TEXT("Service"));
		auto Preview = City.BeginRoadFacingPreview(*Definition, {Index * 2, 9});
		TestTrue(TEXT("operational headroom purchased from real budget"),
		         City.CommitPlacement(Preview, *Definition).IsSuccess());
	}
	uint64 Tower = 0;
	for (int32 X = 24; X <= 48 && !Tower; ++X)
	{
		const auto* Definition = Catalog->FindDefinition(TEXT("WaterTower"));
		auto Preview = City.BeginRoadFacingPreview(*Definition, {X, 9});
		if (City.ValidatePlacement(Preview, *Definition).IsSuccess())
			Tower = City.CommitPlacement(Preview, *Definition).EntityId;
	}
	TestTrue(TEXT("local frontier water purchased on real terrain"), Tower != 0);
	TestTrue(TEXT("residential frontier zoned"),
	         City.SetZoneArea({12, 9}, {40, 2}, ECityZoneType::Residential));
	TestTrue(TEXT("retail frontier zoned"), City.SetZoneArea({0, 6}, {16, 2}, ECityZoneType::Commercial));
	TestTrue(TEXT("industrial frontier zoned"),
	         City.SetZoneArea({16, 6}, {16, 2}, ECityZoneType::Industrial));
	const FString Initial = Directory / TEXT("initial.json");
	const FString Pending = Directory / TEXT("pending.json");
	TestTrue(TEXT("starter saves"), Sim->SaveCityToPath(Initial, Error));
	FString Report = TEXT("speed,day,buildings,residents,jobs,funds,home_low,home_medium,home_high,shop_low,"
	                      "shop_medium,shop_high,industry_low,industry_medium,industry_high\n");
	FString Baseline;
	for (const int32 Speed : {1, 3, 5})
	{
		if (!TestTrue(TEXT("same starter restores"), Sim->LoadCityFromPath(Initial, Error, false)))
			break;
		Sim->SetSimulationPaused(false);
		Sim->SetSimulationSpeed(Speed);
		bool SavedPending = false;
		while (Sim->GetEconomySnapshot().FixedStepId < 200)
		{
			Sim->Tick(0.12f);
			if (!SavedPending && Sim->GetEconomySnapshot().FixedStepId == 80)
			{
				Sim->SetSimulationPaused(true);
				Sim->Tick(60);
				TestEqual(TEXT("pause freezes density and city clock"), Sim->GetEconomySnapshot().FixedStepId,
				          int64(80));
				TestTrue(TEXT("city with partial density progression saves"),
				         Sim->SaveCityToPath(Pending, Error));
				if (!TestTrue(TEXT("partial city resumes"), Sim->LoadCityFromPath(Pending, Error, false)))
					break;
				TestTrue(TEXT("saved pause restores"), Sim->IsSimulationPaused());
				Sim->SetSimulationPaused(false);
				SavedPending = true;
			}
		}
		int32 Homes[3] = {0}, Shops[3] = {0}, Industry[3] = {0};
		CountDensities(Sim->GetPlacementModel(), Sim->GetDevelopmentSnapshot(), TEXT("Home"), Homes);
		CountDensities(Sim->GetPlacementModel(), Sim->GetDevelopmentSnapshot(), TEXT("Shop"), Shops);
		CountDensities(Sim->GetPlacementModel(), Sim->GetDevelopmentSnapshot(), TEXT("Factory"), Industry);
		TestTrue(TEXT("actual early households arrive without pre-existing jobs"),
		         Sim->GetPopulationSnapshot().Citizens.Num() >= 10);
		TestTrue(TEXT("ordinary supplied growth retains low homes"), Homes[0] > 0);
		TestTrue(TEXT("ordinary growth warrants some medium development"),
		         Homes[1] + Shops[1] + Industry[1] > 0);
		TestTrue(TEXT("saved continuation exercised"), SavedPending);
		for (const auto& Pair : Sim->GetPlacementModel().GetEntities())
			if (Pair.Value.DefinitionId == TEXT("Home") || Pair.Value.DefinitionId == TEXT("Shop") ||
			    Pair.Value.DefinitionId == TEXT("Factory"))
				TestEqual(TEXT("ordinary zoned lots remain developer-funded"),
				          Pair.Value.RecordedPlacementCost, int64(0));
		const auto& Economy = Sim->GetEconomySnapshot();
		const FString Outcome =
		    FString::Printf(TEXT("%lld,%d,%d,%d,%lld,%d,%d,%d,%d,%d,%d,%d,%d,%d"), Economy.FixedStepId,
		                    Sim->GetPlacementModel().GetEntities().Num(), Economy.Residents, Economy.Jobs,
		                    Sim->GetPlacementModel().GetFunds(), Homes[0], Homes[1], Homes[2], Shops[0],
		                    Shops[1], Shops[2], Industry[0], Industry[1], Industry[2]);
		if (Speed == 1)
			Baseline = Outcome;
		else
			TestEqual(TEXT("speed and saved continuation preserve growth, jobs, mix and funds"), Outcome,
			          Baseline);
		Report += FString::FromInt(Speed) + TEXT(",") + Outcome + TEXT("\n");
	}
	TestTrue(TEXT("ordinary pacing report writes"),
	         FFileHelper::SaveStringToFile(
	             Report, *(FPaths::ProjectDir() / TEXT("../Artifacts/DensityDemand/NormalGrowth.csv"))));
	World->DestroyWorld(false);
	return true;
}

#endif
