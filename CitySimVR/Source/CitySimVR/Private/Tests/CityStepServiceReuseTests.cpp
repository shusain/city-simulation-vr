#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Persistence/CityPersistence.h"
#include "Simulation/CitySimulationSubsystem.h"

namespace
{
	template <typename T> bool SameServiceValue(const T& A, const T& B)
	{
		return A == B;
	}
	template <typename K, typename V> bool SameServiceValue(const TMap<K, V>& A, const TMap<K, V>& B)
	{
		return A.OrderIndependentCompareEqual(B);
	}
	template <typename T> bool SameServiceValue(const TSet<T>& A, const TSet<T>& B)
	{
		if (A.Num() != B.Num())
			return false;
		for (const auto& Value : A)
			if (!B.Contains(Value))
				return false;
		return true;
	}
	bool SameParks(const TArray<FCityParkProvider>& A, const TArray<FCityParkProvider>& B)
	{
		if (A.Num() != B.Num())
			return false;
		for (int32 I = 0; I < A.Num(); ++I)
			if (A[I].EntityId != B[I].EntityId || A[I].Anchor != B[I].Anchor || A[I].Footprint != B[I].Footprint ||
			    A[I].Radius != B[I].Radius || A[I].Bonus != B[I].Bonus)
				return false;
		return true;
	}
	bool SameSources(const TArray<FCityWaterSource>& A, const TArray<FCityWaterSource>& B)
	{
		if (A.Num() != B.Num())
			return false;
		for (int32 I = 0; I < A.Num(); ++I)
			if (A[I].EntityId != B[I].EntityId || A[I].Capacity != B[I].Capacity || A[I].Used != B[I].Used ||
			    A[I].YieldPercent != B[I].YieldPercent || A[I].StaffPercent != B[I].StaffPercent ||
			    A[I].SourcePressurePercent != B[I].SourcePressurePercent || A[I].bOperational != B[I].bOperational ||
			    !SameServiceValue(A[I].RoadDistances, B[I].RoadDistances))
				return false;
		return true;
	}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityStepServiceReuseTest, "CitySim.Performance.StepServiceReuseEquivalence",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityStepServiceReuseTest::RunTest(const FString& Parameters)
{
	const auto SameServices = [&](const FCityServiceSnapshot& A, const FCityServiceSnapshot& B)
	{
	// Every semantic field, including finite water allocations and shortage causes.
	// Keep this list aligned with FCityServiceSnapshot. Timing pointers are observers.
#define SERVICE_FIELD(Field) TestTrue(TEXT("Identical services: ") TEXT(#Field), SameServiceValue(A.Field, B.Field));
		SERVICE_FIELD(BuiltHomes)
		SERVICE_FIELD(BuiltShops)
		SERVICE_FIELD(BuiltIndustries)
		SERVICE_FIELD(BuiltParks)
		SERVICE_FIELD(OperationalParks)
		SERVICE_FIELD(OperationalLargeParks)
		SERVICE_FIELD(ParkCoverage)
		SERVICE_FIELD(BuiltSchools)
		SERVICE_FIELD(OperationalSchools)
		SERVICE_FIELD(EducationDemand)
		SERVICE_FIELD(EducationCapacity)
		SERVICE_FIELD(EducationServed)
		SERVICE_FIELD(SecondaryResidents)
		SERVICE_FIELD(EducationCitizenIds)
		SERVICE_FIELD(SchoolUsedCapacity)
		SERVICE_FIELD(SchoolCapacities)
		SERVICE_FIELD(HomeEducationPlaces)
		SERVICE_FIELD(HomeEducationDemand)
		SERVICE_FIELD(EntitySkilledJobs)
		SERVICE_FIELD(EducationCoverage)
		SERVICE_FIELD(OperationalHomes)
		SERVICE_FIELD(OperationalShops)
		SERVICE_FIELD(OperationalIndustries)
		SERVICE_FIELD(BuiltPowerBuildings)
		SERVICE_FIELD(BuiltServiceBuildings)
		SERVICE_FIELD(OperationalPowerBuildings)
		SERVICE_FIELD(OperationalServiceBuildings)
		SERVICE_FIELD(Residents)
		SERVICE_FIELD(HomelessResidents)
		SERVICE_FIELD(UnemployedResidents)
		SERVICE_FIELD(DetainedResidents)
		SERVICE_FIELD(BuiltHousingCapacity)
		SERVICE_FIELD(HousingCapacity)
		SERVICE_FIELD(ResidentialUpkeep)
		SERVICE_FIELD(CommercialUpkeep)
		SERVICE_FIELD(IndustrialUpkeep)
		SERVICE_FIELD(ParkUpkeep)
		SERVICE_FIELD(CivicUpkeep)
		SERVICE_FIELD(UtilityUpgradeUpkeep)
		SERVICE_FIELD(bUsesPublicStaffing)
		SERVICE_FIELD(PublicJobs)
		SERVICE_FIELD(PublicStaffPercent)
		SERVICE_FIELD(PublicMinimumEducation)
		SERVICE_FIELD(HospitalCapacities)
		SERVICE_FIELD(HospitalFleetCapacities)
		SERVICE_FIELD(LegacyPowerBuildings)
		SERVICE_FIELD(LegacyServiceBuildings)
		SERVICE_FIELD(HigherEducationCitizenIds)
		SERVICE_FIELD(StationRoadRanges)
		SERVICE_FIELD(StationProtectionCapacities)
		SERVICE_FIELD(StationFleetCapacities)
		SERVICE_FIELD(CrimeRisk)
		SERVICE_FIELD(FireRisk)
		SERVICE_FIELD(FireStatus)
		SERVICE_FIELD(BuiltJobs)
		SERVICE_FIELD(Jobs)
		SERVICE_FIELD(CommercialJobs)
		SERVICE_FIELD(CommercialOutput)
		SERVICE_FIELD(IndustrialJobs)
		SERVICE_FIELD(IndustrialOutput)
		SERVICE_FIELD(PowerCapacity)
		SERVICE_FIELD(PowerDemand)
		SERVICE_FIELD(PowerUsed)
		SERVICE_FIELD(ServiceCapacity)
		SERVICE_FIELD(ServiceDemand)
		SERVICE_FIELD(ServiceCoveredDemand)
		SERVICE_FIELD(ServiceUsed)
		SERVICE_FIELD(bHasDisabledEntities)
		SERVICE_FIELD(EntityFailures)
		SERVICE_FIELD(EntityResidents)
		SERVICE_FIELD(EntityJobs)
		SERVICE_FIELD(EntityOutput)
		SERVICE_FIELD(EntityPollutionEmissions)
		SERVICE_FIELD(OperationalServiceEntityIds)
		SERVICE_FIELD(ServiceReach.RoadHopDistances)
		SERVICE_FIELD(Water.RulesVersion)
		SERVICE_FIELD(Water.bPrepareQueries)
		SERVICE_FIELD(Water.RoadPressurePercent)
		SERVICE_FIELD(Water.ActiveNetworkRoads)
		SERVICE_FIELD(Water.PotentialNetworkRoads)
		SERVICE_FIELD(Water.EntityDemand)
		SERVICE_FIELD(Water.EntityDelivered)
		SERVICE_FIELD(Water.EntityPressurePercent)
		SERVICE_FIELD(Water.EntityCauses)
#undef SERVICE_FIELD
		TestTrue(TEXT("Ordered park providers"), SameParks(A.ParkProviders, B.ParkProviders));
		TestTrue(TEXT("Ordered finite water sources and used budgets"), SameSources(A.Water.Sources, B.Water.Sources));
	};
	const auto Values = UWorld::InitializationValues()
	                        .AllowAudioPlayback(false)
	                        .CreatePhysicsScene(false)
	                        .CreateNavigation(false)
	                        .CreateAISystem(false)
	                        .ShouldSimulatePhysics(false);
	const FString OriginalCommandLine = FCommandLine::Get();
	const FString FixtureArgs = TEXT(
	    " -CityBenchmarkLots=1000 -CityBenchmarkPopulated -CityBenchmarkLateGame -CityBenchmarkSpeed=1 -CityBenchmarkWeather=Clear -CityBenchmarkCitizenRoutes=128 -CityBenchmarkVisibleTraffic=512");
	auto* Catalog = LoadObject<UCityBuildingCatalog>(
	    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
	if (!TestNotNull(TEXT("Production catalog"), Catalog))
		return false;
	FCommandLine::Set(*(OriginalCommandLine + FixtureArgs));
	auto* ReusedWorld =
	    UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	auto* Reused = ReusedWorld->GetSubsystem<UCitySimulationSubsystem>();
	Reused->RegisterBuildingCatalog(Catalog);
	FCommandLine::Set(*(OriginalCommandLine + FixtureArgs +
	                    TEXT(" -CityBenchmarkReferenceServiceReuse -CityBenchmarkReferenceCare")));
	auto* ReferenceWorld =
	    UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	auto* Reference = ReferenceWorld->GetSubsystem<UCitySimulationSubsystem>();
	Reference->RegisterBuildingCatalog(Catalog);
	FCommandLine::Set(*OriginalCommandLine);
	TestTrue(TEXT("Both generated fixtures initialized"),
	         Reused->IsBenchmarkActive() && Reference->IsBenchmarkActive());
	Reused->ResetBenchmarkTimings();
	Reference->ResetBenchmarkTimings();
	FString Error;
	const auto Json = [&](const UCitySimulationSubsystem* Sim)
	{
		FString Result;
		TestTrue(TEXT("Full simulation serialization"),
		         FCityPersistence::Serialize(Sim->BuildSaveDocument(), Result, Error));
		return Result;
	};
	const auto EqualState = [&](bool bCheckSave = true)
	{
		const FString A = bCheckSave ? Json(Reused) : FString(), B = bCheckSave ? Json(Reference) : FString();
		if (bCheckSave && A != B)
		{
			int32 Offset = 0;
			while (Offset < FMath::Min(A.Len(), B.Len()) && A[Offset] == B[Offset])
				++Offset;
			AddError(FString::Printf(TEXT("Full state mismatch day=%lld offset=%d reused=%s reference=%s"),
			                         Reused->Economy.GetSnapshot().FixedStepId, Offset, *A.Mid(Offset, 160),
			                         *B.Mid(Offset, 160)));
		}
		SameServices(Reused->ServiceSnapshot, Reference->ServiceSnapshot);
	};
	EqualState();
	TMap<FName, FCityBuildingDefinition> Definitions;
	for (const auto& D : Catalog->Definitions)
		Definitions.Add(D.DefinitionId, D);
	const auto BoundaryCheck = [&]()
	{
		// Exercise the producer/consumer boundary independently, including monthly
		// treasury changes. Only the disposable clone can be billed by this check.
		auto City =
		    FCityPlacementModel::CreateValidatedFromState(Reused->PlacementModel->ExportState(), Definitions, Error);
		if (!TestNotNull(TEXT("Boundary model clone"), City.Get()))
			return;
		City->SetTerrain(&Reused->Terrain);
		auto Economy = Reused->Economy;
		Economy.SetPaused(false);
		Economy.SetSpeedMultiplier(1);
		FCityServiceSnapshot Forwarded;
		int32 Calls = 0;
		Economy.Advance(
		    Reused->EconomyTuning.FixedStepSeconds * 3, *City, Reused->EconomyTuning, Catalog,
		    &Reused->Development.GetSnapshot(), &Reused->Population.GetSnapshot(),
		    [&]()
		    {
			    ++Calls;
			    const auto Fresh = FCityServiceEvaluator::BuildSnapshot(
			        *City, Reused->EconomyTuning, Catalog, &Reused->Development.GetSnapshot(),
			        &Reused->Environment.GetSnapshot(), &Reused->Population.GetSnapshot(), &Reused->Fire.GetSnapshot());
			    SameServices(Forwarded, Fresh);
		    },
		    &Reused->Fire.GetSnapshot(), {}, &Reused->Environment.GetSnapshot(), nullptr, true, &Forwarded);
		TestEqual(TEXT("Every catch-up callback receives a fresh step result"), Calls, 3);
	};
	uint64 Power = 0, Home = 0;
	for (const auto& Pair : Reused->PlacementModel->GetEntities())
	{
		if (Pair.Value.DefinitionId == TEXT("Power") && !Power)
			Power = Pair.Key;
		if (Pair.Value.DefinitionId == TEXT("Home") &&
		    (!Home || Reused->ServiceSnapshot.EntityResidents.FindRef(Pair.Key) >
		                  Reused->ServiceSnapshot.EntityResidents.FindRef(Home)))
			Home = Pair.Key;
	}
	const auto PowerLot = Reused->PlacementModel->GetEntities().FindChecked(Power);
	const auto HomeLot = Reused->PlacementModel->GetEntities().FindChecked(Home);
	const FString SavePath = FPaths::ProjectSavedDir() / TEXT("Automation/StepServices-StepServices") /
	                         FGuid::NewGuid().ToString() / TEXT("Continuation.json");
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(SavePath), true);
	bool bCatchUp = false;
	for (int32 Frame = 0; Frame < 350; ++Frame)
	{
		const int64 PreviousMonth = Reused->Economy.GetSnapshot().LastBilledMonthId;
		for (auto* Sim : {Reused, Reference})
		{
			if (Frame == 8)
			{
				Sim->SetSimulationPaused(true);
				TestTrue(TEXT("Paused road removal"), Sim->PlacementModel->RemoveRoad({1, 2}));
				Sim->Tick(0);
				Sim->SetSimulationPaused(false);
			}
			if (Frame == 16)
			{
				auto P = Sim->PlacementModel->BeginRoadPreview({1, 2});
				TestTrue(TEXT("Road recovery"), Sim->PlacementModel->CommitRoad(P).IsSuccess());
			}
			if (Frame == 24)
				Sim->BenchmarkResult.Config.Weather = TEXT("Rain");
			if (Frame == 32)
				Sim->BenchmarkResult.Config.Weather = TEXT("Clear");
			if (Frame == 48)
			{
				const auto* Worker = Sim->Population.GetSnapshot().Citizens.FindByPredicate(
				    [&](const auto& C)
				    {
					    const auto* Employer = Sim->PlacementModel->GetEntities().Find(C.WorkplaceEntityId);
					    return Employer && Employer->DefinitionId == TEXT("Factory");
				    });
				if (TestNotNull(TEXT("Private industrial worker"), Worker))
					TestTrue(TEXT("Illness changes live availability"),
					         Sim->Health.StartIllness(Worker->Id, ECityIllness::Severe));
			}
			if (Frame == 80)
				TestTrue(TEXT("Utility outage"), Sim->PlacementModel->DemolishEntity(Power).IsSuccess());
			if (Frame == 100)
			{
				const auto* D = Catalog->FindDefinition(TEXT("Power"));
				auto P = Sim->PlacementModel->BeginPreview(*D, PowerLot.Anchor);
				TestTrue(TEXT("Utility rebuilding starts"), Sim->PlacementModel->CommitPlacement(P, *D).IsSuccess());
			}
			if (Frame == 112)
			{
				TestTrue(TEXT("Housing edit"), Sim->PlacementModel->DemolishEntity(Home).IsSuccess());
				TestTrue(TEXT("Natural development zone"),
				         Sim->PlacementModel->SetZoneArea(HomeLot.Anchor, HomeLot.FootprintSize,
				                                          ECityZoneType::Residential));
			}
			Sim->SetSimulationSpeed(Frame < 120 ? 1 : Frame < 240 ? 3 : 5);
			const float Delta = Frame == 125 ? 7.2f : Sim->EconomyTuning.FixedStepSeconds / Sim->GetSimulationSpeed();
			Sim->Tick(Delta);
		}
		// Exact service outputs every day; full state at edits, monthly settlements
		// and periodic checkpoints avoids repeatedly serializing a dense fixture.
		EqualState(Frame % 25 == 0 || Reused->Economy.GetSnapshot().LastBilledMonthId != PreviousMonth || Frame == 8 ||
		           Frame == 16 || Frame == 24 || Frame == 32 || Frame == 48 || Frame == 80 || Frame == 100 ||
		           Frame == 112 || Frame == 125 || Frame == 160 || Frame == 349);
		bCatchUp |= Reused->BenchmarkTiming->MaximumStepsPerTick > 1;
		if (Frame % 25 == 13)
			BoundaryCheck();
		// Reload before edits, at EmergencyRegression's former failing emergency checkpoint and
		// after further settlements. Both producer paths must remain identical.
		if (Frame == 4 || Frame == 160 || Frame == 325)
		{
			TestTrue(TEXT("Isolated continuation written"),
			         FCityPersistence::WriteAtomic(SavePath, Json(Reference), Error));
			for (auto* Sim : {Reused, Reference})
			{
				const auto Config = Sim->BenchmarkResult.Config;
				if (!TestTrue(TEXT("Continuation restores both paths"), Sim->LoadCityFromPath(SavePath, Error, false)))
					AddError(Error);
				Sim->BenchmarkResult.Config = Config;
				Sim->ResetBenchmarkTimings();
			}
			EqualState();
		}
	}
	TestTrue(TEXT("Reused path has no redundant initial evaluations"),
	         Reused->BenchmarkTiming->ReusedInitialServices > 0 &&
	             Reused->BenchmarkTiming->EvaluatedInitialServices == 0);
	TestTrue(TEXT("Reference path evaluates every initial phase"),
	         Reference->BenchmarkTiming->EvaluatedInitialServices > 0 &&
	             Reference->BenchmarkTiming->ReusedInitialServices == 0);
	TestTrue(TEXT("Real multi-step catch-up exercised"), bCatchUp);
	TestTrue(TEXT("Dense continuation crosses later settlements"),
	         Reused->Economy.GetSnapshot().LastBilledMonthId >= 16);
	ReusedWorld->DestroyWorld(false);
	ReferenceWorld->DestroyWorld(false);
	return true;
}
#endif
