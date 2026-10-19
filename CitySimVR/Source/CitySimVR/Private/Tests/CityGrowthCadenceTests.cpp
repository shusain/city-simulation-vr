#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Persistence/CityPersistence.h"
#include "Presentation/CityNeedsPresentation.h"
#include "Simulation/CitySimulationSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityGrowthCadenceTest,
                                 "CitySim.Progression.ResponsiveGrowthAndSavedDelivery",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityGrowthCadenceTest::RunTest(const FString& Parameters)
{
	FString Report = TEXT("seed,speed,first_growth_day,delivery_day,completion_day,growth_wall_seconds,"
	                      "completion_wall_seconds,day30_buildings,day30_residents,treasury\n");
	const auto Values = UWorld::InitializationValues()
	                        .AllowAudioPlayback(false)
	                        .CreatePhysicsScene(false)
	                        .CreateNavigation(false)
	                        .CreateAISystem(false)
	                        .ShouldSimulatePhysics(false);
	for (const int32 Seed : {7429, 1337, 2026})
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
		                                    ERHIFeatureLevel::Num, &Values);
		auto* Sim = World->GetSubsystem<UCitySimulationSubsystem>();
		auto* Catalog = LoadObject<UCityBuildingCatalog>(
		    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
		if (!TestNotNull(TEXT("authored catalog loads"), Catalog))
		{
			World->DestroyWorld(false);
			return false;
		}
		Sim->RegisterBuildingCatalog(Catalog);
		FCityIdentity Identity;
		Identity.Id = FGuid::NewGuid().ToString(EGuidFormats::Digits).ToLower();
		Identity.Name = TEXT("Growth cadence regression");
		Identity.DisasterFrequency = ECityDisasterFrequency::Off;
		FCityTerrainGenerationParameters Terrain;
		Terrain.Seed = Seed;
		const FString Directory =
		    FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/GrowthCadence"), Identity.Id);
		FString Error;
		if (!TestTrue(TEXT("ordinary Normal city creates"),
		              Sim->CreateCity(Identity, Terrain, Directory, Error)))
		{
			AddError(Error);
			World->DestroyWorld(false);
			return false;
		}
		auto& City = Sim->GetPlacementModel();
		TestEqual(TEXT("ordinary starting budget"), City.GetFunds(), int64(10000));
		auto Road = City.BeginRoadPreview(FCityPlacementModel::EntranceCell);
		Road.End = {10, 8};
		TestTrue(TEXT("starter road purchased"), City.CommitRoad(Road).IsSuccess());
		for (int32 Index = 0; Index < 2; ++Index)
		{
			const auto* Definition = Catalog->FindDefinition(Index == 0 ? TEXT("Power") : TEXT("Service"));
			if (!TestNotNull(TEXT("starter utility"), Definition))
			{
				World->DestroyWorld(false);
				return false;
			}
			auto Preview = City.BeginRoadFacingPreview(*Definition, {Index * 2, 9});
			TestTrue(TEXT("starter utility purchased"),
			         City.CommitPlacement(Preview, *Definition).IsSuccess());
		}
		TestTrue(TEXT("homes zoned"), City.SetZoneArea({4, 9}, {6, 2}, ECityZoneType::Residential));
		TestTrue(TEXT("shops zoned"), City.SetZoneArea({0, 6}, {4, 2}, ECityZoneType::Commercial));
		TestTrue(TEXT("industry zoned"), City.SetZoneArea({4, 6}, {6, 2}, ECityZoneType::Industrial));
		const FString Initial = Directory / TEXT("initial.json");
		const FString Pending = Directory / TEXT("pending.json");
		TestTrue(TEXT("initial starter saves"), Sim->SaveCityToPath(Initial, Error));
		const auto Tuning = Sim->GetEconomyTuning();
		TestEqual(TEXT("calendar step duration unchanged"), Tuning.FixedStepSeconds, 2.4);
		TestEqual(TEXT("monthly billing cadence unchanged"), Tuning.FixedStepsPerMonth, 25);
		TestEqual(TEXT("private growth checks every three game days"), Tuning.GrowthIntervalSteps, 3);
		int64 BaselineFunds = -1, BaselineGrowth = -1, BaselineCompletion = -1;
		int32 BaselineBuildings = -1, BaselineResidents = -1;
		for (const int32 Speed : {1, 3, 5})
		{
			if (!TestTrue(TEXT("initial city restores"), Sim->LoadCityFromPath(Initial, Error, false)))
			{
				World->DestroyWorld(false);
				return false;
			}
			Sim->SetSimulationSpeed(Speed);
			Sim->SetSimulationPaused(false);
			int64 FirstGrowth = -1, Arrival = -1, Completion = -1;
			uint64 HomeId = 0;
			double WallSeconds = 0, GrowthWall = 0, CompletionWall = 0;
			bool bSavedPending = false;
			for (int32 Frame = 0; Frame < 1000 && Sim->GetEconomySnapshot().FixedStepId < 30; ++Frame)
			{
				Sim->Tick(0.12f);
				WallSeconds += 0.12;
				const int64 Day = Sim->GetEconomySnapshot().FixedStepId;
				if (Day < 25)
					TestEqual(TEXT("private growth cannot trigger early monthly billing"),
					          Sim->GetEconomySnapshot().LastBilledMonthId, int64(0));
				if (!HomeId)
					for (const auto& Pair : Sim->GetPlacementModel().GetEntities())
						if (Pair.Value.DefinitionId == TEXT("Home"))
						{
							HomeId = Pair.Key;
							FirstGrowth = Day;
							GrowthWall = WallSeconds;
							break;
						}
				if (!HomeId)
					continue;
				const auto* State = Sim->GetDevelopmentSnapshot().Find(HomeId);
				if (!TestNotNull(TEXT("first home's construction state exists"), State))
					break;
				for (const auto& Trip : Sim->GetPopulationSnapshot().TransitTrips)
					if (Trip.DestinationEntityId == HomeId && Trip.Purpose == ECityTripPurpose::MoveInArrival)
					{
						if (Trip.bCompleted)
							Arrival = Trip.ArrivalStep;
						else
							TestEqual(TEXT("construction cannot finish before its real truck arrives"),
							          State->Stage, ECityDevelopmentStage::Construction);
					}
				if (!bSavedPending && Day == 4)
				{
					const int64 Deadline = State->ConstructionCompleteStep;
					const int32 Trips = Sim->GetPopulationSnapshot().TransitTrips.Num();
					Sim->SetSimulationPaused(true);
					Sim->Tick(60);
					TestEqual(TEXT("pause freezes the day clock"), Sim->GetEconomySnapshot().FixedStepId,
					          Day);
					TestEqual(TEXT("pause freezes the construction deadline"),
					          Sim->GetDevelopmentSnapshot().Find(HomeId)->ConstructionCompleteStep, Deadline);
					TestTrue(TEXT("pending construction/delivery saves"),
					         Sim->SaveCityToPath(Pending, Error));
					if (!TestTrue(TEXT("pending construction/delivery reloads"),
					              Sim->LoadCityFromPath(Pending, Error, false)))
						break;
					TestTrue(TEXT("saved pause restored"), Sim->IsSimulationPaused());
					TestEqual(TEXT("saved deadline is preserved"),
					          Sim->GetDevelopmentSnapshot().Find(HomeId)->ConstructionCompleteStep, Deadline);
					TestEqual(TEXT("saved delivery count is preserved"),
					          Sim->GetPopulationSnapshot().TransitTrips.Num(), Trips);
					Sim->SetSimulationPaused(false);
					bSavedPending = true;
					continue;
				}
				if (Completion < 0 && State->Stage == ECityDevelopmentStage::Operational)
				{
					Completion = Day;
					CompletionWall = WallSeconds;
				}
			}
			TestEqual(TEXT("first growth starts on day three"), FirstGrowth, int64(3));
			TestTrue(TEXT("real delivery arrival observed"), Arrival > FirstGrowth);
			TestEqual(TEXT("completion follows actual delivery by three days"), Completion, Arrival + 3);
			TestTrue(TEXT("starter construction completes before first month boundary"),
			         Completion > 0 && Completion < 25);
			TestTrue(TEXT("pending checkpoint exercised"), bSavedPending);
			TestEqual(TEXT("month billed once at day thirty"), Sim->GetEconomySnapshot().LastBilledMonthId,
			          int64(1));
			TestTrue(TEXT("first-growth wall pacing matches speed"),
			         FMath::Abs(GrowthWall - 3 * 2.4 / Speed) <= 0.13);
			const auto Funds = Sim->GetPlacementModel().GetFunds();
			const int32 Buildings = Sim->GetPlacementModel().GetEntities().Num();
			const int32 Residents = Sim->GetPopulationSnapshot().Citizens.Num();
			if (Speed == 1)
			{
				BaselineFunds = Funds;
				BaselineBuildings = Buildings;
				BaselineResidents = Residents;
				BaselineGrowth = FirstGrowth;
				BaselineCompletion = Completion;
			}
			else
			{
				TestEqual(TEXT("speed does not alter treasury outcome"), Funds, BaselineFunds);
				TestEqual(TEXT("speed does not duplicate growth"), Buildings, BaselineBuildings);
				TestEqual(TEXT("speed does not alter population"), Residents, BaselineResidents);
				TestEqual(TEXT("speed preserves growth game day"), FirstGrowth, BaselineGrowth);
				TestEqual(TEXT("speed preserves completion game day"), Completion, BaselineCompletion);
			}
			Report +=
			    FString::Printf(TEXT("%d,%d,%lld,%lld,%lld,%.3f,%.3f,%d,%d,%lld\n"), Seed, Speed, FirstGrowth,
			                    Arrival, Completion, GrowthWall, CompletionWall, Buildings, Residents, Funds);
		}
		World->DestroyWorld(false);
	}
	const auto ReportPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("../Artifacts/GrowthCadence-Growth-Pacing.csv"));
	TestTrue(TEXT("pacing evidence writes"), FFileHelper::SaveStringToFile(Report, *ReportPath));
	FCityNeedsSnapshot Needs;
	Needs.ResidentialConstraint = ECityDemandConstraint::None;
	const auto Guidance = FCityNeedsPresentation::BuildGuidance(Needs, FCityEconomySnapshot());
	TestTrue(TEXT("shared guidance explains game days and real deliveries"),
	         Guidance.Contains(TEXT("every 3 game days")) && Guidance.Contains(TEXT("Deliveries")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityGrowthDeadlineCompatibilityTest,
                                 "CitySim.Progression.ExistingDevelopmentDeadlines",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityGrowthDeadlineCompatibilityTest::RunTest(const FString& Parameters)
{
	auto* Catalog = LoadObject<UCityBuildingCatalog>(
	    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
	if (!TestNotNull(TEXT("authored catalog"), Catalog))
		return false;
	const auto* Home = Catalog->FindDefinition(TEXT("Home"));
	if (!TestNotNull(TEXT("new three-day home definition"), Home))
		return false;
	FCityPlacementModel City(10000);
	auto Preview = City.BeginPreview(*Home, {10, 10});
	const uint64 Id = City.CommitPlacement(Preview, *Home).EntityId;
	FCitySaveDocument Save;
	Save.Placement = City.ExportState();
	Save.Economy.FixedStepId = 4;
	Save.NextGrowthStep = 25;
	Save.Development.LastFixedStepId = 4;
	FCityDevelopmentState State;
	State.EntityId = Id;
	State.Stage = ECityDevelopmentStage::Construction;
	State.ConstructionCompleteStep = 30; // An older 25-day construction period already in progress.
	Save.Development.States.Add(Id, State);
	FCitySaveDocument Loaded;
	FString Json, Error;
	if (!TestTrue(TEXT("old pending deadlines save"), FCityPersistence::Serialize(Save, Json, Error)) ||
	    !TestTrue(TEXT("old pending deadlines load"), FCityPersistence::Deserialize(Json, Loaded, Error)))
		return false;
	FCityDevelopmentSimulation Development;
	if (!TestTrue(TEXT("old pending development restores"),
	              Development.RestoreState(Loaded.Development, City, Catalog, 4, Error)))
		return false;
	TestEqual(TEXT("new catalog cannot shorten an already saved deadline"),
	          Development.GetSnapshot().Find(Id)->ConstructionCompleteStep, int64(30));
	FCityEconomyTuning Tuning;
	FCityServiceSnapshot Services;
	FCityNeedsSnapshot Needs;
	FCityEnvironmentSnapshot Environment;
	Development.AdvanceToStep(29, City, Catalog, Services, Needs, Environment, Tuning);
	TestEqual(TEXT("old construction does not finish early"), Development.GetSnapshot().Find(Id)->Stage,
	          ECityDevelopmentStage::Construction);
	Development.AdvanceToStep(30, City, Catalog, Services, Needs, Environment, Tuning);
	TestEqual(TEXT("old construction finishes at saved deadline"), Development.GetSnapshot().Find(Id)->Stage,
	          ECityDevelopmentStage::Operational);
	FCityGrowthSimulation Growth;
	TestTrue(TEXT("old pending growth schedule restores"),
	         Growth.RestoreState(Loaded.GrowthRandomState, Loaded.NextGrowthStep, Error));
	TestEqual(TEXT("old pending growth check not run early"),
	          Growth.AdvanceToStep(24, City, Services, Needs, Tuning, Home, nullptr, false).Failure,
	          ECityGrowthFailure::NotDue);
	Growth.AdvanceToStep(25, City, Services, Needs, Tuning, Home, nullptr, false);
	TestEqual(TEXT("subsequent checks use new three-day cadence"), Growth.GetNextGrowthStep(), int64(28));
	return true;
}

#endif
