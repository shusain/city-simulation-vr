#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Persistence/CityPersistence.h"
#include "Simulation/CitySimulationSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCitySavedGrowthProbeTest, "CitySim.Progression.SavedGrowthProbe",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCitySavedGrowthProbeTest::RunTest(const FString& Parameters)
{
	FString Path;
	if (!FParse::Value(FCommandLine::Get(), TEXT("CityGrowthProbe="), Path))
	{
		AddInfo(TEXT("Pass -CityGrowthProbe=<copied city.json> to investigate a save without writing to it."));
		return true;
	}
	FString Json, Error;
	FCitySaveDocument Saved;
	if (!TestTrue(TEXT("copied save reads"), FCityPersistence::ReadFile(Path, Json, Error)) ||
	    !TestTrue(TEXT("copied save parses"), FCityPersistence::Deserialize(Json, Saved, Error)))
	{
		AddError(Error);
		return false;
	}
	const auto Values = UWorld::InitializationValues()
	                        .AllowAudioPlayback(false)
	                        .CreatePhysicsScene(false)
	                        .CreateNavigation(false)
	                        .CreateAISystem(false)
	                        .ShouldSimulatePhysics(false);
	UWorld* World =
	    UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	auto* Sim = World->GetSubsystem<UCitySimulationSubsystem>();
	auto* Catalog = LoadObject<UCityBuildingCatalog>(
	    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
	if (!TestNotNull(TEXT("catalog loads"), Catalog))
	{
		World->DestroyWorld(false);
		return false;
	}
	Sim->RegisterBuildingCatalog(Catalog);
	if (!TestTrue(TEXT("copied city loads"), Sim->LoadCityFromPath(Path, Error, false)))
	{
		AddError(Error);
		World->DestroyWorld(false);
		return false;
	}
	const auto& Needs = Sim->GetNeedsSnapshot();
	const auto DescribeMarket = [](const FCityNeedsSnapshot& State)
	{
		FString Text = FString::Printf(
		    TEXT(
		        "Housing occupied/operational/built=%d/%d/%d Jobs filled/operational/built=%d/%d/%d Goods wanted/produced/retail=%d/%d/%d\n"),
		    State.OccupiedResidents, State.HousingCapacity, State.BuiltHousingCapacity, State.EmployedJobs,
		    State.JobCapacity, State.BuiltJobCapacity, State.HouseholdGoodsDemand, State.GoodsProduced,
		    State.RetailCapacity);
		for (const auto Type : {ECityDevelopmentDemandType::Residential, ECityDevelopmentDemandType::Commercial,
		                        ECityDevelopmentDemandType::Industrial})
		{
			const auto& Market = State.DensityFor(Type);
			Text += FString::Printf(TEXT("Market %d desired=%d/%d/%d reserved=%d/%d/%d new-low=%d additional=%d\n"),
			                        static_cast<int32>(Type), Market.Desired[0], Market.Desired[1], Market.Desired[2],
			                        Market.Reserved[0], Market.Reserved[1], Market.Reserved[2], Market.NewLowDemand(),
			                        Market.AdditionalCapacity());
		}
		return Text;
	};
	FString Report = FString::Printf(
	    TEXT(
	        "City=%s Day=%lld NextGrowth=%lld Funds=%lld\nDemand R=%d C=%d I=%d\nBuildable R=%d C=%d I=%d\nConstraints R=%s C=%s I=%s\n"),
	    *Saved.Identity.Name, Saved.Economy.FixedStepId, Saved.NextGrowthStep, Sim->GetPlacementModel().GetFunds(),
	    Needs.ResidentialDemand, Needs.CommercialDemand, Needs.IndustrialDemand, Needs.ResidentialAffordableFootprints,
	    Needs.CommercialAffordableFootprints, Needs.IndustrialAffordableFootprints,
	    *FCityNeedsEvaluator::ConstraintToString(Needs.ResidentialConstraint),
	    *FCityNeedsEvaluator::ConstraintToString(Needs.CommercialConstraint),
	    *FCityNeedsEvaluator::ConstraintToString(Needs.IndustrialConstraint));
	Report += DescribeMarket(Needs);
	TArray<uint64> Ids;
	Sim->GetServiceSnapshot().EntityFailures.GetKeys(Ids);
	Ids.Sort();
	for (const uint64 Id : Ids)
	{
		const auto Failure = Sim->GetServiceSnapshot().GetFailure(Id);
		if (Failure == ECityOperationFailure::None)
			continue;
		const auto& Entity = Sim->GetPlacementModel().GetEntities().FindChecked(Id);
		Report += FString::Printf(TEXT("Disabled #%llu %s (%d,%d): %s\n"), Id, *Entity.DefinitionId.ToString(),
		                          Entity.Anchor.X, Entity.Anchor.Y, *FCityServiceEvaluator::FailureToString(Failure));
	}
	FCityGrowthSimulation Growth;
	TestTrue(TEXT("saved schedule restores"),
	         Growth.RestoreState(Saved.GrowthRandomState, Saved.NextGrowthStep, Error));
	const auto Result = Growth.AdvanceToStep(
	    FMath::Max(Saved.NextGrowthStep, Saved.Economy.FixedStepId), Sim->GetPlacementModel(),
	    Sim->GetServiceSnapshot(), Needs, Sim->GetEconomyTuning(), Catalog->FindDefinition(TEXT("Home")),
	    Catalog->FindDefinition(TEXT("Shop")), Saved.Economy.bGrowthBlockedByShortfall,
	    Catalog->FindDefinition(TEXT("Factory")), &Sim->GetEnvironmentSnapshot());
	Report += TEXT("Next check: ") + FCityGrowthSimulation::DescribeResult(Result) + TEXT("\n");
	if (FParse::Param(FCommandLine::Get(), TEXT("CityGrowthExpectSuccess")))
	{
		TestTrue(TEXT("ready saved zone grows despite local outages"), Result.IsSuccess());
		TestEqual(TEXT("private save growth is free to treasury"), Sim->GetPlacementModel().GetFunds(),
		          Saved.Placement.Funds);
		const auto* Added = Sim->GetPlacementModel().GetEntities().Find(Result.EntityId);
		if (TestNotNull(TEXT("growth produces a real entity"), Added))
			TestEqual(TEXT("private entity records zero treasury cost"), Added->RecordedPlacementCost, int64(0));
	}
	int32 AdvanceDays = 0;
	FParse::Value(FCommandLine::Get(), TEXT("CityGrowthAdvanceDays="), AdvanceDays);
	if (AdvanceDays > 0)
	{
		for (const int32 Speed : {1, 3, 5})
		{
			if (!TestTrue(TEXT("fresh copy restores for real clock replay"), Sim->LoadCityFromPath(Path, Error, false)))
				break;
			const int32 InitialCount = Sim->GetPlacementModel().GetEntities().Num();
			TMap<FName, int32> InitialFamilies;
			for (const auto& Pair : Sim->GetPlacementModel().GetEntities())
				++InitialFamilies.FindOrAdd(Pair.Value.DefinitionId);
			Sim->SetSimulationPaused(false);
			Sim->SetSimulationSpeed(Speed);
			const int64 EndDay = Saved.Economy.FixedStepId + FMath::Clamp(AdvanceDays, 1, 1000);
			int64 FirstGrowth = -1;
			while (Sim->GetEconomySnapshot().FixedStepId < EndDay)
			{
				Sim->Tick(0.12f);
				if (FirstGrowth < 0 && Sim->GetPlacementModel().GetEntities().Num() > InitialCount)
					FirstGrowth = Sim->GetEconomySnapshot().FixedStepId;
			}
			TestTrue(TEXT("saved city progresses on the actual clock"), FirstGrowth >= Saved.NextGrowthStep);
			Report += FString::Printf(TEXT("Replay %dx: first growth=%lld buildings=%d residents=%d funds=%lld\n"),
			                          Speed, FirstGrowth, Sim->GetPlacementModel().GetEntities().Num(),
			                          Sim->GetEconomySnapshot().Residents, Sim->GetPlacementModel().GetFunds());
			Report += DescribeMarket(Sim->GetNeedsSnapshot());
			if (FParse::Param(FCommandLine::Get(), TEXT("CityGrowthExpectFamilies")))
			{
				TMap<FName, int32> FinalFamilies;
				for (const auto& Pair : Sim->GetPlacementModel().GetEntities())
				{
					++FinalFamilies.FindOrAdd(Pair.Value.DefinitionId);
					if (Pair.Key >= Saved.Placement.NextEntityId)
						TestEqual(TEXT("every new private lot preserves zero city cost"),
						          Pair.Value.RecordedPlacementCost, int64(0));
				}
				for (const FName Family : {FName(TEXT("Home")), FName(TEXT("Shop")), FName(TEXT("Factory"))})
					TestTrue(*FString::Printf(TEXT("saved city adds %s lots on the real clock"), *Family.ToString()),
					         FinalFamilies.FindRef(Family) > InitialFamilies.FindRef(Family));
			}
			for (const auto& Pair : Sim->GetDevelopmentSnapshot().States)
			{
				if (Pair.Value.Stage != ECityDevelopmentStage::Construction)
					continue;
				Report += FString::Printf(
				    TEXT("Pending #%llu deadline=%lld failure=%s\n"), Pair.Key, Pair.Value.ConstructionCompleteStep,
				    *FCityServiceEvaluator::FailureToString(Sim->GetServiceSnapshot().GetFailure(Pair.Key)));
				for (const auto& Trip : Sim->GetPopulationSnapshot().TransitTrips)
					if (Trip.DestinationEntityId == Pair.Key && Trip.Purpose == ECityTripPurpose::MoveInArrival)
					{
						Report += FString::Printf(TEXT(" Delivery #%llu start=%lld complete=%d\n"), Trip.Id,
						                          Trip.StartStep, Trip.bCompleted);
						for (const auto& Vehicle : Sim->GetTrafficSnapshot().Vehicles)
							if (Vehicle.bTransitTrip && Vehicle.SourceId == Trip.Id)
								Report +=
								    FString::Printf(TEXT(" Vehicle leg=%d/%d position=%d speed=%d complete=%d\n"),
								                    Vehicle.RouteLegIndex, Vehicle.Route.Num(), Vehicle.DistanceUnits,
								                    Vehicle.SpeedUnitsPerStep, Vehicle.bCompleted);
					}
			}
			if (FParse::Param(FCommandLine::Get(), TEXT("CityGrowthExpectPopulation")))
				TestTrue(TEXT("saved city attracts households after employers expand"),
				         Sim->GetEconomySnapshot().Residents > Saved.Economy.Residents);
		}
	}
	FString After;
	TestTrue(TEXT("source copy remains readable"), FCityPersistence::ReadFile(Path, After, Error));
	TestEqual(TEXT("investigation never changes the input save"), After, Json);
	AddInfo(Report);
	FFileHelper::SaveStringToFile(Report,
	                              *(FPaths::ProjectDir() / TEXT("../Artifacts/GrowthSaveInvestigation/probe.txt")));
	World->DestroyWorld(false);
	return true;
}

#endif
