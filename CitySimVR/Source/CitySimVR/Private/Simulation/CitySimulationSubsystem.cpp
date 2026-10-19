#include "Simulation/CitySimulationSubsystem.h"
#include "Simulation/CityParkSimulation.h"
#include "Simulation/CityHousingMigration.h"

#include "Commands/CityPlacementModel.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/CoreDelegates.h"
#include "Persistence/CityPersistence.h"
#include "Persistence/CitySaveLibrary.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Simulation/CityFirstPlayableGuide.h"
#include "Simulation/CityNeedsSimulation.h"
#include "Engine/Engine.h"
#include "IXRTrackingSystem.h"

namespace
{
FCityEconomyTuning SavedTuning(const FCityEconomyTuning& Base, const FCitySaveDocument& Document)
{
	auto Tuning = Base;
	Tuning.NewArrivalUneducatedPercent = Document.DifficultyRules.NewArrivalUneducatedPercent;
	Tuning.CrimePopulationThreshold = Document.DifficultyRules.CrimePopulationThreshold;
	Tuning.FirePopulationThreshold = Document.DifficultyRules.FirePopulationThreshold;
	Tuning.CrimeGraceMonths = Document.DifficultyRules.CrimeGraceMonths;
	Tuning.FireGraceMonths = Document.DifficultyRules.FireGraceMonths;
	Tuning.CrimeCheckIntervalSteps = Document.DifficultyRules.CrimeCheckIntervalSteps;
	Tuning.FireCheckIntervalSteps = Document.DifficultyRules.FireCheckIntervalSteps;
	Tuning.FireRepairCostPercent = Document.DifficultyRules.FireRepairCostPercent;
	Tuning.FireRepairSteps = Document.DifficultyRules.FireRepairSteps;
	return Tuning;
}
ECitySavePlatform ActiveSavePlatform(UWorld* World)
{
	const bool bIsXR = FParse::Param(FCommandLine::Get(), TEXT("vr")) ||
	                   (World && GEngine && GEngine->XRSystem.IsValid() &&
	                    GEngine->XRSystem->IsHeadTrackingAllowedForWorld(*World));
	return FCityPersistence::NativePlatform(bIsXR);
}
bool ValidateRuntimeGrid(const FCityGridDimensions Dimensions, UWorld* World, FString& Error)
{
	return FCityPersistence::ValidatePlatform(Dimensions, ActiveSavePlatform(World), Error);
}
} // namespace

void UCitySimulationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
#if CITYSIM_CAPACITY_GRID_SIZE != 64
	FString CapacityUser;
	FParse::Value(FCommandLine::Get(), TEXT("UserDir="), CapacityUser);
	CapacityUser.ReplaceInline(TEXT("\\"), TEXT("/"));
	if (!FParse::Param(FCommandLine::Get(), TEXT("CityDesktop")) ||
	    !FParse::Param(FCommandLine::Get(), TEXT("CityBenchmarkPopulated")) ||
	    !FParse::Param(FCommandLine::Get(), TEXT("CityBenchmarkExit")) ||
	    !CapacityUser.Contains(TEXT("/Artifacts/Benchmarks/")))
		UE_LOG(LogTemp, Fatal,
		       TEXT("Experimental capacity grid requires isolated desktop benchmark arguments"));
#endif
	CityIdentity.Id = TEXT("00000000000000000000000000000002");
	CityIdentity.Name = TEXT("Untitled City");
	ensure(Terrain.Generate(FCityTerrainGenerationParameters()));
	PlacementModel = MakeUnique<FCityPlacementModel>(10000);
	PlacementModel->SetTerrain(&Terrain);
	Growth = FCityGrowthSimulation(static_cast<uint32>(EconomyTuning.GrowthSeed));
	Environment.Reset();
	Development.Reset();
	Population.Reset();
	Reports = FCityReportHistory();
	Traffic.Reset();
	PreviousTraffic = FCityTrafficSnapshot();
	Economy.SetPaused(true);
	BackgroundDelegateHandle = FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddUObject(
	    this, &UCitySimulationSubsystem::HandleApplicationBackgrounded);
	ForegroundDelegateHandle = FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddUObject(
	    this, &UCitySimulationSubsystem::HandleApplicationForegrounded);
}

void UCitySimulationSubsystem::Deinitialize()
{
	FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Remove(BackgroundDelegateHandle);
	FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Remove(ForegroundDelegateHandle);
	PlacementModel.Reset();
	Super::Deinitialize();
}

void UCitySimulationSubsystem::Tick(const float DeltaTime)
{
	if (!PlacementModel || bEntryMenuSuspended)
	{
		return;
	}
	const double ProfileStart = BenchmarkTiming ? FPlatformTime::Seconds() : 0;
	const int64 ProfileMonth = Economy.GetSnapshot().LastBilledMonthId;
	const int64 ProfileStep = Economy.GetSnapshot().FixedStepId;

	const int32 PreviousDevelopmentCount = Development.GetSnapshot().States.Num();
	Development.Reconcile(*PlacementModel, BuildingCatalog, GetStationHalfDayClock(), EconomyTuning.FixedStepsPerMonth);
	if (Development.GetSnapshot().States.Num() != PreviousDevelopmentCount)
		++PresentationGeneration;
	const auto CompleteStations = [this](const int64 HalfDay) {
		if (!Development.AdvanceStationsToHalfDay(HalfDay, ServiceSnapshot).ChangedEntityIds.IsEmpty())
			++PresentationGeneration;
	};
	const bool bReuseServices = UseInitialServiceReuse();
	FCityServiceSnapshot StepServices;
	Economy.Advance(
	    DeltaTime, *PlacementModel, EconomyTuning, BuildingCatalog, &Development.GetSnapshot(),
	    &Population.GetSnapshot(),
	    [this, &StepServices, bReuseServices]() {
		    AdvanceSimulationStep(bReuseServices ? &StepServices : nullptr);
	    },
	    &Fire.GetSnapshot(), [this, &CompleteStations](const int64 NextStep) { CompleteStations(NextStep * 2 / FMath::Max(1, EconomyTuning.FixedStepsPerMonth)); },
	    &Environment.GetSnapshot(), BenchmarkTiming ? &BenchmarkTiming->EconomyEvaluation : nullptr,
	    UsePreparedPopulationQueries(), bReuseServices ? &StepServices : nullptr);
	if (!Economy.IsPaused())
		CompleteStations(GetStationHalfDayClock());
	if (NeedsDerivedRefresh())
		RefreshDerivedSnapshots();
	if (BenchmarkTiming)
	{
		const double ElapsedMs = (FPlatformTime::Seconds() - ProfileStart) * 1000;
		BenchmarkTiming->Ticks.Add(ElapsedMs);
		const uint64 Steps = Economy.GetSnapshot().FixedStepId - ProfileStep;
		BenchmarkTiming->MaximumStepsPerTick = FMath::Max(BenchmarkTiming->MaximumStepsPerTick, Steps);
		BenchmarkTiming->CatchUpTicks += Steps > 1 ? 1 : 0;
		if (Economy.GetSnapshot().LastBilledMonthId > ProfileMonth)
			BenchmarkTiming->MonthlyTicks.Add(ElapsedMs);
	}
}

void UCitySimulationSubsystem::AdvanceSimulationStep(FCityServiceSnapshot* StepServices)
{
	const double ProfileFixedStart = BenchmarkTiming ? FPlatformTime::Seconds() : 0;
	const auto TimedPhase = [](FCityFrameTiming* Timing, auto&& Action) {
		const double Start = Timing ? FPlatformTime::Seconds() : 0;
		Action();
		if (Timing)
			Timing->Add((FPlatformTime::Seconds() - Start) * 1000);
	};
	const int64 PreviousBilledMonth = Population.GetSnapshot().LastSettledMonthId;
	const int64 PreviousFixedStep = Traffic.GetSnapshot().FixedStepId;
	const FCityEconomySnapshot& Snapshot = Economy.GetSnapshot();

	if (StepServices)
	{
		ServiceSnapshot = MoveTemp(*StepServices);
		ServiceSnapshot.Water.EvaluationTiming =
		    BenchmarkTiming ? &BenchmarkTiming->ServiceEvaluation : nullptr;
		if (BenchmarkTiming)
			++BenchmarkTiming->ReusedInitialServices;
	}
	else
	{
		ServiceSnapshot = FCityServiceEvaluator::BuildSnapshot(
		    *PlacementModel, EconomyTuning, BuildingCatalog, &Development.GetSnapshot(),
		    &Environment.GetSnapshot(), &Population.GetSnapshot(), &Fire.GetSnapshot(), nullptr, -1,
		    BenchmarkTiming ? &BenchmarkTiming->ServiceEvaluation : nullptr, UsePreparedServiceQueries());
		if (BenchmarkTiming)
			++BenchmarkTiming->EvaluatedInitialServices;
	}
	if (BenchmarkTiming)
		BenchmarkTiming->InitialServices.Add((FPlatformTime::Seconds() - ProfileFixedStart) * 1000);
	double ProfilePhase = BenchmarkTiming ? FPlatformTime::Seconds() : 0;
	// Reconcile at the economy's fixed cadence, before development can change
	// capacities. A render-only frame must not mix new lots with stale totals.
	if (Snapshot.FixedStepId != PreviousFixedStep)
	{
		RefreshRoadNetwork();
		TimedPhase(BenchmarkTiming ? &BenchmarkTiming->ScheduleHospital : nullptr, [&]() {
			Population.UpdateHospitalCare(
			    Health, *PlacementModel, BuildingCatalog, RoadNetworkSnapshot, ServiceSnapshot,
			    Snapshot.FixedStepId, EconomyTuning.FixedStepsPerMonth,
			    static_cast<uint32>(EconomyTuning.GrowthSeed), Snapshot.HasOperatingUpkeepShortfall(), true,
			    &Environment.GetSnapshot(), &Waste.GetSnapshot(), UsePreparedCareQueries());
		});
		Population.AdvanceToMonth(*PlacementModel, Snapshot, EconomyTuning, &ServiceSnapshot, true, true,
		                          UsePreparedPopulationQueries(),
		                          BenchmarkTiming ? &BenchmarkTiming->LivePopulationEvaluation : nullptr);
		Population.EnableHouseholdSupplies(Snapshot.LastBilledMonthId);
		if (Snapshot.LastBilledMonthId != PreviousBilledMonth)
		{
			// Check settled finance and finite production before further care/traffic
			// changes workers. The earlier production forecast has a different phase.
			TimedPhase(BenchmarkTiming ? &BenchmarkTiming->MonthlyPopulationCheck : nullptr, [&]() {
				ensureMsgf(Population.MatchesSettledEconomy(Snapshot, EconomyTuning),
				           TEXT("CITYSIM_POPULATION_RECONCILIATION_FAILED Month=%lld"),
				           Snapshot.LastBilledMonthId);
			});
		}
		if (BenchmarkTiming)
			BenchmarkTiming->PopulationReconcile.Add((FPlatformTime::Seconds() - ProfilePhase) * 1000);
		ProfilePhase = BenchmarkTiming ? FPlatformTime::Seconds() : 0;
		for (int64 TrafficStep = PreviousFixedStep + 1; TrafficStep <= Snapshot.FixedStepId; ++TrafficStep)
		{
			TimedPhase(BenchmarkTiming ? &BenchmarkTiming->SchedulePrune : nullptr,
			           [&]() { Population.PruneTransitTrips(*PlacementModel); });
			TimedPhase(BenchmarkTiming ? &BenchmarkTiming->ScheduleHospital : nullptr, [&]() {
				Population.UpdateHospitalCare(
				    Health, *PlacementModel, BuildingCatalog, RoadNetworkSnapshot, ServiceSnapshot,
				    TrafficStep, EconomyTuning.FixedStepsPerMonth,
				    static_cast<uint32>(EconomyTuning.GrowthSeed), Snapshot.HasOperatingUpkeepShortfall(),
				    false, &Environment.GetSnapshot(), &Waste.GetSnapshot(), UsePreparedCareQueries());
			});
			TimedPhase(BenchmarkTiming ? &BenchmarkTiming->ScheduleWaste : nullptr, [&]() {
				Population.UpdateWasteService(Waste, TrafficStep, EconomyTuning.FixedStepsPerMonth,
				                              *PlacementModel, BuildingCatalog, RoadNetworkSnapshot,
				                              ServiceSnapshot, Development.GetSnapshot(),
				                              Snapshot.HasOperatingUpkeepShortfall());
			});
			TimedPhase(BenchmarkTiming ? &BenchmarkTiming->ScheduleEmergency : nullptr, [&]() {
				Population.UpdateEmergencyResponses(*PlacementModel, BuildingCatalog, RoadNetworkSnapshot,
				                                    Fire.GetSnapshot(), Crime.GetSnapshot(), ServiceSnapshot,
				                                    TrafficStep, Snapshot.HasOperatingUpkeepShortfall());
			});
			TimedPhase(BenchmarkTiming ? &BenchmarkTiming->ScheduleOpenings : nullptr,
		           [&]() { Population.QueueBusinessOpenings(Development.GetSnapshot(), TrafficStep, EconomyTuning.FixedStepsPerMonth); });
			TimedPhase(BenchmarkTiming ? &BenchmarkTiming->ScheduleFreight : nullptr,
			           [&]() { Population.QueueFreight(TrafficStep); });
			Population.RetryPurchaseReservations(*PlacementModel, BuildingCatalog, RoadNetworkSnapshot, TrafficStep, EconomyTuning, &Development.GetSnapshot());
			TimedPhase(BenchmarkTiming ? &BenchmarkTiming->ScheduleCitizens : nullptr,
			           [&]() { Population.AdvanceCitizenSchedules(*PlacementModel, TrafficStep, EconomyTuning.FixedStepsPerMonth); });
			Traffic.UpdateTrips(*PlacementModel, BuildingCatalog, Population.GetSnapshot(),
			                    RoadNetworkSnapshot, TrafficStep);
			TimedPhase(BenchmarkTiming ? &BenchmarkTiming->TrafficSnapshotCopy : nullptr,
			           [&]() { PreviousTraffic = Traffic.GetSnapshot(); });
			Traffic.AdvanceOneStep(RoadNetworkSnapshot, TrafficStep);
			TimedPhase(BenchmarkTiming ? &BenchmarkTiming->TrafficCompletions : nullptr, [&]() {
				for (const FCityTrafficVehicleState& Vehicle : Traffic.GetSnapshot().Vehicles)
					if (Vehicle.bCompleted)
					{
						if (Vehicle.bTransitTrip)
							Population.CompleteTransitTrip(Vehicle.SourceId, TrafficStep);
						else
							Population.CompleteCitizenTrip(Vehicle.SourceId, Vehicle.OriginEntityId,
							                               Vehicle.DestinationEntityId, TrafficStep, EconomyTuning.FixedStepsPerMonth);
					}
			});
			TimedPhase(BenchmarkTiming ? &BenchmarkTiming->ScheduleHospital : nullptr, [&]() {
				Population.UpdateHospitalCare(
				    Health, *PlacementModel, BuildingCatalog, RoadNetworkSnapshot, ServiceSnapshot,
				    TrafficStep, EconomyTuning.FixedStepsPerMonth,
				    static_cast<uint32>(EconomyTuning.GrowthSeed), Snapshot.HasOperatingUpkeepShortfall(),
				    false, &Environment.GetSnapshot(), &Waste.GetSnapshot(), UsePreparedCareQueries());
			});
			TimedPhase(BenchmarkTiming ? &BenchmarkTiming->ScheduleWaste : nullptr, [&]() {
				Population.UpdateWasteService(Waste, TrafficStep, EconomyTuning.FixedStepsPerMonth,
				                              *PlacementModel, BuildingCatalog, RoadNetworkSnapshot,
				                              ServiceSnapshot, Development.GetSnapshot(),
				                              Snapshot.HasOperatingUpkeepShortfall());
			});
			Traffic.UpdateTrips(*PlacementModel, BuildingCatalog, Population.GetSnapshot(),
			                    RoadNetworkSnapshot, TrafficStep);
			TimedPhase(BenchmarkTiming ? &BenchmarkTiming->DeliveryHolds : nullptr, [&]() {
				for (const FCityTransitTripRecord& Trip : Population.GetSnapshot().TransitTrips)
				{
					if (Trip.Purpose != ECityTripPurpose::MoveInArrival)
						continue;
					// Completed trips survive until the next step, giving construction
					// its full duration measured from the actual arrival step.
					const FCityPlacedEntity* Entity =
					    PlacementModel->GetEntities().Find(Trip.DestinationEntityId);
					const FCityBuildingDefinition* Definition =
					    Entity && BuildingCatalog ? BuildingCatalog->FindDefinition(Entity->DefinitionId)
					                              : nullptr;
					if (Definition)
						Development.HoldForDelivery(Trip.DestinationEntityId, TrafficStep,
						                            Definition->ConstructionSteps);
				}
			});
		}
	}
	const auto PreviousWind = Environment.GetSnapshot().WindDirection;
	if (BenchmarkTiming)
		BenchmarkTiming->SchedulingTraffic.Add((FPlatformTime::Seconds() - ProfilePhase) * 1000);
	ProfilePhase = BenchmarkTiming ? FPlatformTime::Seconds() : 0;
	for (const auto& S : Waste.GetSnapshot().Sites)
		ServiceSnapshot.EntityPollutionEmissions.FindOrAdd(S.EntityId) +=
		    Waste.GetSnapshot().Pressure(S.EntityId, EconomyTuning.FixedStepsPerMonth);
	Environment.AdvanceToStep(Snapshot.FixedStepId, *PlacementModel, ServiceSnapshot, EconomyTuning);
	if (PreviousWind != Environment.GetSnapshot().WindDirection)
		++PresentationGeneration;
	if (BenchmarkTiming)
		BenchmarkTiming->EnvironmentAdvance.Add((FPlatformTime::Seconds() - ProfilePhase) * 1000);
	// The derived refresh evaluates services with the new weather before development.
	RefreshDerivedSnapshots();
	ProfilePhase = BenchmarkTiming ? FPlatformTime::Seconds() : 0;
	const FCityDevelopmentResult DevelopmentResult =
	    Development.AdvanceToStep(Snapshot.FixedStepId, *PlacementModel, BuildingCatalog, ServiceSnapshot,
	                              NeedsSnapshot, Environment.GetSnapshot(), EconomyTuning);
	if (!DevelopmentResult.ChangedEntityIds.IsEmpty())
	{
		++PresentationGeneration;
		RefreshDerivedSnapshots();
	}
	if (BuildingCatalog)
	{
		const FCityGrowthResult GrowthResult = Growth.AdvanceToStep(
		    Snapshot.FixedStepId, *PlacementModel, ServiceSnapshot, NeedsSnapshot, EconomyTuning,
		    BuildingCatalog->FindDefinition(EconomyTuning.HomeDefinitionId),
		    BuildingCatalog->FindDefinition(EconomyTuning.ShopDefinitionId),
		    Snapshot.bGrowthBlockedByShortfall,
		    BuildingCatalog->FindDefinition(EconomyTuning.IndustrialDefinitionId),
		    &Environment.GetSnapshot());
		if (GrowthResult.Failure != ECityGrowthFailure::NotDue)
		{
			LastGrowthResult = GrowthResult;
		}
		if (GrowthResult.IsSuccess())
		{
			Development.Reconcile(*PlacementModel, BuildingCatalog);
			Population.QueueConstructionArrival(GrowthResult.EntityId, Snapshot.FixedStepId);
			// Construction remains held until its traffic delivery reports arrival.
			PendingSpawnedEntityIds.Add(GrowthResult.EntityId);
			UE_LOG(LogTemp, Display,
			       TEXT("CITYSIM_GROWTH_PLACED Entity=%llu Zone=%d Cell=(%d,%d) Funds=%lld"),
			       GrowthResult.EntityId, static_cast<int32>(GrowthResult.Zone), GrowthResult.Anchor.X,
			       GrowthResult.Anchor.Y, PlacementModel->GetFunds());
		}
		else if (GrowthResult.Failure != ECityGrowthFailure::NotDue)
		{
			UE_LOG(LogTemp, Display, TEXT("CITYSIM_GROWTH_BLOCKED Reason=%s"),
			       *FCityGrowthSimulation::DescribeResult(GrowthResult));
		}
	}
	if (BenchmarkTiming)
		BenchmarkTiming->DevelopmentGrowth.Add((FPlatformTime::Seconds() - ProfilePhase) * 1000);
	ProfilePhase = BenchmarkTiming ? FPlatformTime::Seconds() : 0;
	Crime.AdvanceToStep(Snapshot.FixedStepId, *PlacementModel, ServiceSnapshot, Development.GetSnapshot(),
	                    EconomyTuning, Snapshot.HasOperatingUpkeepShortfall(), &Population.GetSnapshot());
	ServiceSnapshot.CrimeRisk = Crime.GetSnapshot().CellRisk;
	if (Snapshot.LastBilledMonthId != PreviousBilledMonth)
		Economy.SettlePolicies(*PlacementModel, Population, ServiceSnapshot, EconomyTuning);
    // Public and private payroll have settled and derived providers have refreshed.
    // Never sell software from yesterday's operating chain on a newly unpaid day.
    if (Snapshot.LastBilledMonthId != PreviousBilledMonth && Population.GetSnapshot().bCommercialAiPilot)
    {
        Population.SettleCommercialAi(ServiceSnapshot);
        ++PresentationGeneration;
        RefreshDerivedSnapshots();
    }
	Scenario.Advance(Snapshot);
	if (Snapshot.LastBilledMonthId != PreviousBilledMonth && !Snapshot.HasOperatingUpkeepShortfall())
		Population.AdvanceEducation(Snapshot.LastBilledMonthId, ServiceSnapshot, EconomyTuning);
	Fire.AdvanceToStep(Snapshot.FixedStepId, *PlacementModel, ServiceSnapshot, Development.GetSnapshot(),
	                   Environment.GetSnapshot(), EconomyTuning, Snapshot.HasOperatingUpkeepShortfall(),
	                   &Population.GetSnapshot());
	Crime.AdvanceArson(*PlacementModel, Development.GetSnapshot(), EconomyTuning, ServiceSnapshot,
	                   Snapshot.HasOperatingUpkeepShortfall(), Population, Fire);
	Disasters.AdvanceToStep(Snapshot.FixedStepId, *PlacementModel, Development.GetSnapshot(), Fire,
	                        CityIdentity, ServiceSnapshot.Residents, EconomyTuning.FixedStepsPerMonth,
	                        static_cast<int32>(Environment.GetSnapshot().WindDirection));
	const auto* Disaster = Disasters.GetSnapshot().Current();
	if (Disaster && Disaster->StartStep == Snapshot.FixedStepId)
		Economy.SetSpeedMultiplier(1);
	if (BenchmarkTiming)
		BenchmarkTiming->Incidents.Add((FPlatformTime::Seconds() - ProfilePhase) * 1000);
	RefreshDerivedSnapshots();
	if (Snapshot.LastBilledMonthId != PreviousBilledMonth)
	{
		Population.AdvanceHappiness(Snapshot.LastBilledMonthId, *PlacementModel, ServiceSnapshot,
		                            Environment.GetSnapshot(), Snapshot, EconomyTuning, Health.GetSnapshot(),
		                            Waste.GetSnapshot());
		const bool Migrated = Population.AdvanceHousingMigration(
		    Snapshot.LastBilledMonthId, *PlacementModel, ServiceSnapshot, Environment.GetSnapshot(), Snapshot,
		    EconomyTuning, Health.GetSnapshot(), Waste.GetSnapshot(), Crime.GetSnapshot());
		if (Migrated)
		{
			Population.Reconcile(*PlacementModel, Snapshot, EconomyTuning, &ServiceSnapshot,
			                     UsePreparedPopulationQueries());
			Traffic.UpdateTrips(*PlacementModel, BuildingCatalog, Population.GetSnapshot(),
			                    RoadNetworkSnapshot, Snapshot.FixedStepId);
			PreviousTraffic = Traffic.GetSnapshot();
			++PresentationGeneration;
			RefreshDerivedSnapshots();
		}
		Economy.UpdateResidentCensus(Population.GetSnapshot());
		// Sample after monthly education and derived services reconcile. Ledger flows
		// still describe the settled month; headcounts/capacities are closing snapshots.
		Reports.Capture(Snapshot, Population.GetSnapshot(), ServiceSnapshot, Environment.GetSnapshot(),
		                PlacementModel->GetFunds(), &Crime.GetSnapshot(), &Health.GetSnapshot(),
		                &Waste.GetSnapshot());
		UE_LOG(LogTemp, Display,
		       TEXT("CITYSIM_MONTH_BILLED Month=%lld Residents=%d Jobs=%d Income=%lld ResidentialTax=%lld "
		            "CommercialTax=%lld IndustrialTax=%lld Upkeep=%lld Shortfall=%lld Funds=%lld"),
		       Snapshot.LastBilledMonthId, Snapshot.Residents, Snapshot.Jobs, Snapshot.LastIncome,
		       Snapshot.LastResidentialTaxRevenue, Snapshot.LastCommercialTaxRevenue,
		       Snapshot.LastIndustrialTaxRevenue, Snapshot.LastUpkeep, Snapshot.UpkeepShortfall,
		       PlacementModel->GetFunds());
	}
	if (BenchmarkTiming)
		BenchmarkTiming->FixedSteps.Add((FPlatformTime::Seconds() - ProfileFixedStart) * 1000);
	Economy.UpdateResidentCensus(Population.GetSnapshot());
}

void UCitySimulationSubsystem::RegisterBuildingCatalog(UCityBuildingCatalog* Catalog)
{
	BuildingCatalog = Catalog;
	Development.Reconcile(*PlacementModel, BuildingCatalog);
	if (const FCityBuildingDefinition* Industry =
	        BuildingCatalog ? BuildingCatalog->FindDefinition(EconomyTuning.IndustrialDefinitionId) : nullptr)
	{
		EconomyTuning.JobsPerOperationalIndustry = Industry->JobsProvided;
		EconomyTuning.OutputPerOperationalIndustry = Industry->EconomicOutput;
		EconomyTuning.IndustrialPowerDemand = Industry->PowerDemand;
		EconomyTuning.IndustrialWaterDemand = Industry->WaterDemand;
		EconomyTuning.PollutionEmissionPerIndustrialBuilding = Industry->PollutionEmission;
		EconomyTuning.IndustrialUpkeep = Industry->MonthlyUpkeep;
	}
	if (const FCityBuildingDefinition* Park =
	        BuildingCatalog ? BuildingCatalog->FindDefinition(EconomyTuning.ParkDefinitionId) : nullptr)
	{
		EconomyTuning.ParkUpkeep = Park->MonthlyUpkeep;
	}
	RefreshDerivedSnapshots();
	TryInitializeBenchmark();
	RunStorageProbe();
}

FCityBudgetProjection UCitySimulationSubsystem::GetMonthSettlementPreview() const
{
	if (!PlacementModel)
		return FCityBudgetProjection();
	auto Budget = Economy.GetProjectedBudget(*PlacementModel, EconomyTuning, -1, -1, -1,
	                                         Population.GetSnapshot().SalesThisMonth);
	Budget.PolicyCost =
	    Economy.GetSnapshot().Policies.Forecast(Population.GetSnapshot(), ServiceSnapshot, EconomyTuning,
	                                            !Economy.GetSnapshot().HasOperatingUpkeepShortfall());
	Budget.Expenses += FMath::Min(Budget.PolicyCost, MAX_int64 - Budget.Expenses);
	return Budget;
}

FCityBudgetProjection UCitySimulationSubsystem::GetProjectedBudget(const int32 ResidentialBasisPoints,
                                                                   const int32 CommercialBasisPoints,
                                                                   const int32 IndustrialBasisPoints) const
{
	if (!PlacementModel)
		return FCityBudgetProjection();
	auto Budget = Economy.GetProjectedBudget(*PlacementModel, EconomyTuning, ResidentialBasisPoints,
	                                         CommercialBasisPoints, IndustrialBasisPoints);
	Budget.PolicyCost =
	    Economy.GetSnapshot().Policies.Forecast(Population.GetSnapshot(), ServiceSnapshot, EconomyTuning,
	                                            !Economy.GetSnapshot().HasOperatingUpkeepShortfall());
	Budget.Expenses += FMath::Min(Budget.PolicyCost, MAX_int64 - Budget.Expenses);
	return Budget;
}

bool UCitySimulationSubsystem::SetCommercialAiPilot(bool Enabled)
{
	if (Population.GetSnapshot().bCommercialAiPilot == Enabled)
		return false;
	Population.SetCommercialAiPilot(Enabled);
	++PresentationGeneration;
	RefreshDerivedSnapshots();
	return true;
}

bool UCitySimulationSubsystem::NeedsDerivedRefresh() const
{
	// Explicit public state changes still refresh immediately. This checkpoint
	// only skips redundant render-only work, never a fixed simulation step.
	return PlacementModel &&
	       (DerivedOwner != PlacementModel.Get() || DerivedRevision != PlacementModel->GetRevision() ||
	        DerivedSession != CitySessionGeneration || DerivedPresentation != PresentationGeneration ||
	        DerivedFunds != PlacementModel->GetFunds() || DerivedStep != Economy.GetSnapshot().FixedStepId);
}

bool UCitySimulationSubsystem::UsePreparedServiceQueries() const
{
	return !(IsBenchmarkActive() && BenchmarkResult.Config.bPopulated &&
	         FParse::Param(FCommandLine::Get(), TEXT("CityBenchmarkReferenceServices")));
}

bool UCitySimulationSubsystem::UsePreparedPopulationQueries() const
{
	return !(IsBenchmarkActive() && BenchmarkResult.Config.bPopulated &&
	         FParse::Param(FCommandLine::Get(), TEXT("CityBenchmarkReferencePopulation")));
}

bool UCitySimulationSubsystem::UseInitialServiceReuse() const
{
	// The older service-query control must retain its original initial evaluation.
	return UsePreparedServiceQueries() && !(IsBenchmarkActive() && BenchmarkResult.Config.bPopulated &&
	                                        BenchmarkResult.Config.bReferenceServiceReuse);
}

bool UCitySimulationSubsystem::UsePreparedCareQueries() const
{
	return !(IsBenchmarkActive() && BenchmarkResult.Config.bPopulated &&
	         BenchmarkResult.Config.bReferenceCare);
}

void UCitySimulationSubsystem::RefreshRoadNetwork()
{
	if (RoadNetworkOwner != PlacementModel.Get() ||
	    RoadNetworkSnapshot.SourceRevision != PlacementModel->GetRevision())
	{
		FCityRoadNetworkBuilder::Build(*PlacementModel, FCityRoadDefinition::Local(), RoadNetworkSnapshot);
		Traffic.ReconcileNetwork(RoadNetworkSnapshot);
		RoadNetworkOwner = PlacementModel.Get();
	}
	RoadNetworkSnapshot.RoutingTiming = BenchmarkTiming ? &BenchmarkTiming->Routing : nullptr;
	// The graph can predate benchmark activation; capture mode is independent
	// of geometry invalidation, and must also bind when no rebuild is needed.
	RoadNetworkSnapshot.bUsePreparedRouting =
	    !(IsBenchmarkActive() && BenchmarkResult.Config.bPopulated &&
	      FParse::Param(FCommandLine::Get(), TEXT("CityBenchmarkUncachedRouting")));
}

void UCitySimulationSubsystem::RefreshDerivedSnapshots()
{
	const double ProfileDerivedStart = BenchmarkTiming ? FPlatformTime::Seconds() : 0;
	if (!PlacementModel)
	{
		ServiceSnapshot = FCityServiceSnapshot();
		NeedsSnapshot = FCityNeedsSnapshot();
		RoadNetworkSnapshot = FCityRoadNetworkSnapshot();
		return;
	}
	Fire.RestoreFireproofStations(*PlacementModel);
	Crime.RefreshCustody(*PlacementModel, EconomyTuning, Population, Fire);
	double ProfilePart = BenchmarkTiming ? FPlatformTime::Seconds() : 0;
	RefreshRoadNetwork();
	if (BenchmarkTiming)
	{
		BenchmarkTiming->RoadNetwork.Add((FPlatformTime::Seconds() - ProfilePart) * 1000);
		ProfilePart = FPlatformTime::Seconds();
	}
	ServiceSnapshot = FCityServiceEvaluator::BuildSnapshot(
	    *PlacementModel, EconomyTuning, BuildingCatalog, &Development.GetSnapshot(),
	    &Environment.GetSnapshot(), &Population.GetSnapshot(), &Fire.GetSnapshot(), nullptr, -1,
	    BenchmarkTiming ? &BenchmarkTiming->ServiceEvaluation : nullptr, UsePreparedServiceQueries());
	if (BenchmarkTiming)
		BenchmarkTiming->Services.Add((FPlatformTime::Seconds() - ProfilePart) * 1000);
	if (Waste.NeedsReconcile(PlacementModel->GetRevision()))
		Population.UpdateWasteService(Waste, Economy.GetSnapshot().FixedStepId,
		                              EconomyTuning.FixedStepsPerMonth, *PlacementModel, BuildingCatalog,
		                              RoadNetworkSnapshot, ServiceSnapshot, Development.GetSnapshot(),
		                              Economy.GetSnapshot().HasOperatingUpkeepShortfall());
	Crime.Refresh(*PlacementModel, ServiceSnapshot, Development.GetSnapshot(), EconomyTuning,
	              Economy.GetSnapshot().HasOperatingUpkeepShortfall());
	ServiceSnapshot.CrimeRisk = Crime.GetSnapshot().CellRisk;
	Fire.Refresh(*PlacementModel, ServiceSnapshot, Development.GetSnapshot(), Environment.GetSnapshot(),
	             EconomyTuning, Economy.GetSnapshot().HasOperatingUpkeepShortfall(),
	             &Population.GetSnapshot());
	ServiceSnapshot.FireRisk = Fire.GetSnapshot().CellRisk;
	ServiceSnapshot.FireStatus = Fire.GetSnapshot().CellStatus;
	ProfilePart = BenchmarkTiming ? FPlatformTime::Seconds() : 0;
	FCityParkEvaluator::Build(*PlacementModel, ServiceSnapshot, EconomyTuning, BuildingCatalog,
	                          &Development.GetSnapshot(),
	                          Economy.GetSnapshot().HasOperatingUpkeepShortfall());
	if (BenchmarkTiming)
	{
		BenchmarkTiming->Parks.Add((FPlatformTime::Seconds() - ProfilePart) * 1000);
		ProfilePart = FPlatformTime::Seconds();
	}
	const FCityBuildingDefinition* HomeDefinition =
	    BuildingCatalog ? BuildingCatalog->FindDefinition(EconomyTuning.HomeDefinitionId) : nullptr;
	const FCityBuildingDefinition* ShopDefinition =
	    BuildingCatalog ? BuildingCatalog->FindDefinition(EconomyTuning.ShopDefinitionId) : nullptr;
	const FCityBuildingDefinition* IndustryDefinition =
	    BuildingCatalog ? BuildingCatalog->FindDefinition(EconomyTuning.IndustrialDefinitionId) : nullptr;
	NeedsSnapshot = FCityNeedsEvaluator::BuildSnapshot(
	    *PlacementModel, ServiceSnapshot, EconomyTuning, HomeDefinition, ShopDefinition,
	    Economy.GetSnapshot().bGrowthBlockedByShortfall, IndustryDefinition, &Environment.GetSnapshot(),
	    Economy.GetSnapshot().ResidentialTaxBasisPoints, Economy.GetSnapshot().CommercialTaxBasisPoints,
	    &Population.GetSnapshot(), Economy.GetSnapshot().IndustrialTaxBasisPoints, &Development.GetSnapshot(),
	    BenchmarkTiming ? &BenchmarkTiming->NeedsEvaluation : nullptr, UsePreparedPopulationQueries());
	FCityFirstPlayableGuide::UpdateProgress(GetGuideState(), Guidance);
	DerivedOwner = PlacementModel.Get();
	DerivedRevision = PlacementModel->GetRevision();
	DerivedSession = CitySessionGeneration;
	DerivedPresentation = PresentationGeneration;
	DerivedFunds = PlacementModel->GetFunds();
	DerivedStep = Economy.GetSnapshot().FixedStepId;
	if (BenchmarkTiming)
	{
		BenchmarkTiming->Needs.Add((FPlatformTime::Seconds() - ProfilePart) * 1000);
		BenchmarkTiming->Derived.Add((FPlatformTime::Seconds() - ProfileDerivedStart) * 1000);
	}
}

void UCitySimulationSubsystem::TryInitializeBenchmark()
{
	if (bBenchmarkInitializationAttempted || !BuildingCatalog)
	{
		return;
	}
	bBenchmarkInitializationAttempted = true;
	FCityBenchmarkConfig Config;
	FString Error;
	if (!FCityBenchmark::ParseCommandLine(FCommandLine::Get(), Config, Error))
	{
		ControlStatus = TEXT("Benchmark disabled: ") + Error;
		UE_LOG(LogTemp, Error, TEXT("CITYSIM_BENCHMARK_INVALID Reason=%s"), *Error);
		return;
	}
	if (!Config.IsEnabled())
	{
		return;
	}
	if (!Config.SavedCityPath.IsEmpty())
	{
		if (!InitializeSavedBenchmark(Config, Error))
			UE_LOG(LogTemp, Error, TEXT("CITYSIM_BENCHMARK_LOAD_FAILED Reason=%s"), *Error);
		return;
	}

	FCityPlacementModelState State;
	FCityBenchmarkResult Result;
	if (!FCityBenchmark::BuildScene(Config, *BuildingCatalog, State, Result, Error))
	{
		ControlStatus = TEXT("Benchmark disabled: ") + Error;
		UE_LOG(LogTemp, Error, TEXT("CITYSIM_BENCHMARK_BUILD_FAILED Reason=%s"), *Error);
		return;
	}
	TMap<FName, FCityBuildingDefinition> Definitions;
	for (const FCityBuildingDefinition& Definition : BuildingCatalog->Definitions)
	{
		if (Definition.IsValid())
		{
			Definitions.Add(Definition.DefinitionId, Definition);
		}
	}
	TUniquePtr<FCityPlacementModel> BenchmarkModel =
	    FCityPlacementModel::CreateValidatedFromState(State, Definitions, Error);
	if (!BenchmarkModel)
	{
		ControlStatus = TEXT("Benchmark disabled: ") + Error;
		UE_LOG(LogTemp, Error, TEXT("CITYSIM_BENCHMARK_VALIDATE_FAILED Reason=%s"), *Error);
		return;
	}

	auto BenchmarkTerrain = FCityBenchmark::CapacityTerrainParameters();
	if (Config.bLateGame)
		BenchmarkTerrain.WaterRulesVersion = 1;
	if (Config.bPopulated && !Terrain.Generate(BenchmarkTerrain, &Error))
	{
		UE_LOG(LogTemp, Error, TEXT("CITYSIM_BENCHMARK_BUILD_FAILED Terrain=%s"), *Error);
		return;
	}
	PlacementModel = MoveTemp(BenchmarkModel);
	RoadNetworkOwner = DerivedOwner = nullptr;
	PlacementModel->SetTerrain(&Terrain);
	if (FParse::Param(FCommandLine::Get(), TEXT("CityBenchmarkUncachedSpatial")))
		PlacementModel->SetSpatialCacheEnabled(false);
	Economy = FCityEconomySimulation();
	Economy.SetPaused(true);
	Growth = FCityGrowthSimulation(static_cast<uint32>(EconomyTuning.GrowthSeed));
	Development.InitializeExistingAsOperational(*PlacementModel, BuildingCatalog, 0);
	if (Config.bLateGame)
	{
		auto MatureDevelopment = Development.GetSnapshot();
		for (auto& Pair : MatureDevelopment.States)
		{
			const auto& Entity = PlacementModel->GetEntities().FindChecked(Pair.Key);
			const auto* Definition = BuildingCatalog->FindDefinition(Entity.DefinitionId);
			const bool bUtility = FCityBuildingDefinition::IsPowerFacility(Entity.DefinitionId) ||
			                      FCityBuildingDefinition::IsWaterFacility(Entity.DefinitionId);
			const auto TargetLevel =
			    bUtility || Pair.Key % 5 == 0 ? ECityDensityLevel::High : ECityDensityLevel::Medium;
			if (Definition && Definition->FindDensityLevel(TargetLevel))
				Pair.Value.DensityLevel = TargetLevel;
		}
		if (!Development.RestoreState(MatureDevelopment, *PlacementModel, BuildingCatalog, 0, Error))
			UE_LOG(LogTemp, Fatal, TEXT("CITYSIM_BENCHMARK_BUILD_FAILED MatureDevelopment=%s"), *Error);
	}
	Population.Reset();
	Reports = FCityReportHistory();
	Traffic.Reset();
	PreviousTraffic = FCityTrafficSnapshot();
	RefreshDerivedSnapshots();
	LastGrowthResult = FCityGrowthResult();
	PendingSpawnedEntityIds.Reset();
	Scenario = FCityScenarioSimulation();
	Guidance = FCityGuidedProgress();
	Crime.Reset();
	Fire.Reset();
	Health = FCityHealthSimulation();
	Waste.Reset();
	Disasters.Reset(0, static_cast<uint32>(Terrain.GetParameters().Seed) ^ 0x44495341u);
	++CitySessionGeneration;
	BenchmarkResult = Result;
	Traffic.SetBenchmarkCitizenRouteBudget(Config.CitizenRouteBudget);
	if (Config.bPopulated)
	{
		CityIdentity.DisasterFrequency = ECityDisasterFrequency::Off;
		Economy.SetPaused(false);
		// Real authority bootstraps residents/jobs/trips before timed warmup. No
		// fabricated citizens, teleporting traffic or modified player save.
		for (int32 Step = 0; Step < 8; ++Step)
			Tick(EconomyTuning.FixedStepSeconds);
		if (Config.bLateGame)
		{
			// Mature-city qualifications/opening deliveries are declared fixture
			// inputs; residents and jobs still come from real simulation authority.
			auto MaturePopulation = Population.GetSnapshot();
			for (auto& Citizen : MaturePopulation.Citizens)
				Citizen.Education = Citizen.Id % 10 == 0  ? ECityEducationLevel::Higher
				                    : Citizen.Id % 10 < 4 ? ECityEducationLevel::Secondary
				                                          : ECityEducationLevel::Basic;
			if (!Population.RestoreState(MaturePopulation, Error))
				UE_LOG(LogTemp, Fatal, TEXT("CITYSIM_BENCHMARK_BUILD_FAILED MaturePopulation=%s"), *Error);
			Population.MarkExistingBusinessesOpenForScenario();
			for (int32 Step = 0; Step < 52; ++Step)
				Tick(EconomyTuning.FixedStepSeconds);
		}
		SetSimulationSpeed(FMath::Max(1, Config.Speed));
		SetSimulationPaused(Config.Speed == 0);
		if (Config.Weather != TEXT("Natural"))
		{
			auto Weather = Environment.GetSnapshot();
			Weather.Temperature = ECityTemperatureBand::Mild;
			Weather.Season = ECitySeason::Spring;
			Weather.bPrecipitation = Config.Weather == TEXT("Rain");
			Weather.NextWeatherChangeStep = MAX_int64 / 2;
			if (!Environment.RestoreState(Weather, Economy.GetSnapshot().FixedStepId, Error))
				UE_LOG(LogTemp, Error, TEXT("CITYSIM_BENCHMARK_BUILD_FAILED Weather=%s"), *Error);
		}
	}
	ControlStatus = GetBenchmarkStatus();
	++PresentationGeneration;
	UE_LOG(LogTemp, Display,
	       TEXT("CITYSIM_BENCHMARK_SCENE RequestedLots=%d PlacedLots=%d RequestedRoads=%d PlacedRoads=%d "
	            "RequestedVehicles=%d PlacedVehicles=%d"),
	       BenchmarkResult.Config.RequestedLots, BenchmarkResult.PlacedLots,
	       BenchmarkResult.Config.RequestedRoadCells, BenchmarkResult.PlacedRoadCells,
	       BenchmarkResult.Config.RequestedVehicles, BenchmarkResult.PlacedVehicles);
}

bool UCitySimulationSubsystem::InitializeSavedBenchmark(const FCityBenchmarkConfig& Config, FString& OutError)
{
	if (Config.SavedCityPath.IsEmpty() || !LoadCityFromPath(Config.SavedCityPath, OutError, false))
		return false;
	BenchmarkResult = FCityBenchmarkResult();
	BenchmarkResult.Config = Config;
	Traffic.SetBenchmarkCitizenRouteBudget(Config.CitizenRouteBudget);
	BenchmarkResult.PlacedLots = PlacementModel->GetEntities().Num();
	BenchmarkResult.PlacedRoadCells = PlacementModel->ExportState().RoadCells.Num();
	// Saved cities use their real traffic; never add synthetic benchmark vehicles.
	SetSimulationSpeed(FMath::Max(1, Config.Speed));
	SetSimulationPaused(Config.Speed == 0);
	if (Config.Weather != TEXT("Natural"))
	{
		auto Weather = Environment.GetSnapshot();
		Weather.Temperature = ECityTemperatureBand::Mild;
		Weather.Season = ECitySeason::Spring;
		Weather.WindDirection = ECityWindDirection::East;
		Weather.WindStrength = 1;
		Weather.bPrecipitation = Config.Weather == TEXT("Rain");
		// A declared, in-memory controlled-weather experiment. Authority remains
		// on the isolated city copy and no original save is rewritten.
		Weather.NextWeatherChangeStep = MAX_int64 / 2;
		if (!Environment.RestoreState(Weather, Economy.GetSnapshot().FixedStepId, OutError))
			return false;
	}
	RefreshDerivedSnapshots();
	ControlStatus = GetBenchmarkStatus();
	UE_LOG(LogTemp, Display,
	       TEXT("CITYSIM_BENCHMARK_SAVED_CITY Lots=%d Roads=%d Citizens=%d Speed=%d Weather=%s"),
	       BenchmarkResult.PlacedLots, BenchmarkResult.PlacedRoadCells,
	       Population.GetSnapshot().Citizens.Num(), Config.Speed, *Config.Weather);
	return true;
}

FString UCitySimulationSubsystem::GetBenchmarkStatus() const
{
	if (!BenchmarkResult.Config.SavedCityPath.IsEmpty())
		return FString::Printf(TEXT("SAVED CITY %d lots | %d citizens | %dx | %s"),
		                       PlacementModel->GetEntities().Num(), Population.GetSnapshot().Citizens.Num(),
		                       BenchmarkResult.Config.Speed, *BenchmarkResult.Config.Weather);
	return BenchmarkResult.Config.IsEnabled()
	           ? FString::Printf(TEXT("BENCH %d/%d lots | %d roads | %d vehicles"),
	                             BenchmarkResult.PlacedLots, BenchmarkResult.Config.RequestedLots,
	                             BenchmarkResult.PlacedRoadCells, BenchmarkResult.PlacedVehicles)
	           : FString();
}

void UCitySimulationSubsystem::ConsumeSpawnedEntityIds(TArray<uint64>& OutEntityIds)
{
	OutEntityIds = MoveTemp(PendingSpawnedEntityIds);
	PendingSpawnedEntityIds.Reset();
}

TStatId UCitySimulationSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UCitySimulationSubsystem, STATGROUP_Tickables);
}

bool UCitySimulationSubsystem::IsTickable() const
{
	return PlacementModel.IsValid() && GetWorld() && GetWorld()->IsGameWorld();
}

FCityPlacementModel& UCitySimulationSubsystem::GetPlacementModel() const
{
	check(PlacementModel);
	return *PlacementModel;
}

void UCitySimulationSubsystem::SetSimulationPaused(const bool bPaused)
{
	Economy.SetPaused(bPaused);
	ControlStatus = bPaused ? TEXT("Simulation paused") : TEXT("Simulation running");
}

void UCitySimulationSubsystem::SetSimulationSpeed(const int32 SpeedMultiplier)
{
	Economy.SetSpeedMultiplier(SpeedMultiplier);
	ControlStatus = FString::Printf(TEXT("Simulation speed %dx"), Economy.GetSpeedMultiplier());
}

bool UCitySimulationSubsystem::SetTaxRates(const int32 ResidentialBasisPoints,
                                           const int32 CommercialBasisPoints,
                                           const int32 IndustrialBasisPoints)
{
	if (!Economy.SetTaxRates(ResidentialBasisPoints, CommercialBasisPoints, IndustrialBasisPoints))
	{
		ControlStatus = TEXT("Tax rates must be between 0% and 25%");
		return false;
	}
	ControlStatus =
	    FString::Printf(TEXT("Taxes applied: residential %.2f%%, commercial %.2f%%, industrial %.2f%%"),
	                    ResidentialBasisPoints / 100.0, CommercialBasisPoints / 100.0,
	                    Economy.GetSnapshot().IndustrialTaxBasisPoints / 100.0);
	RefreshDerivedSnapshots();
	return true;
}

void UCitySimulationSubsystem::ToggleSimulationPaused()
{
	SetSimulationPaused(!Economy.IsPaused());
}

void UCitySimulationSubsystem::StartScenario()
{
	if (Scenario.GetSnapshot().Phase != ECityScenarioPhase::NotStarted)
	{
		ControlStatus = TEXT("Scenario already started; use RESET for a new city");
		return;
	}
	Scenario.Start(Economy.GetSnapshot().LastBilledMonthId);
	RefreshDerivedSnapshots();
	Economy.SetPaused(false);
	ControlStatus = TEXT("Scenario started; current city retained");
}

void UCitySimulationSubsystem::RequestResetScenario()
{
	if (!bResetConfirmationPending)
	{
		bResetConfirmationPending = true;
		ControlStatus = TEXT("RESET armed: select RESET again to discard city");
		return;
	}
	ResetCity(true);
	ControlStatus = TEXT("City reset; scenario restarted");
}

void UCitySimulationSubsystem::ResetCity(const bool bStartRunning)
{
	ensure(Terrain.Generate(Terrain.GetParameters()));
	const auto Dimensions = Terrain.GetDimensions();
	PlacementModel =
	    MakeUnique<FCityPlacementModel>(FCityIdentity::StartingFunds(CityIdentity.Difficulty), Dimensions);
	RoadNetworkOwner = DerivedOwner = nullptr;
	PlacementModel->SetTerrain(&Terrain);
	Economy = FCityEconomySimulation();
	Economy.SetPaused(!bStartRunning);
	Economy.SetSpeedMultiplier(1);
	Growth = FCityGrowthSimulation(static_cast<uint32>(EconomyTuning.GrowthSeed));
	Environment = FCityEnvironmentSimulation(Dimensions);
	Environment.Reset();
	Development.Reset();
	Population.Reset();
	Reports = FCityReportHistory();
	Traffic.Reset();
	PreviousTraffic = FCityTrafficSnapshot();
	ServiceSnapshot = FCityServiceSnapshot();
	NeedsSnapshot = FCityNeedsSnapshot();
	LastGrowthResult = FCityGrowthResult();
	PendingSpawnedEntityIds.Reset();
	Scenario = FCityScenarioSimulation();
	Guidance = FCityGuidedProgress();
	Crime.Reset();
	Fire.Reset();
	Health = FCityHealthSimulation();
	Waste.Reset();
	Disasters.Reset(0, static_cast<uint32>(Terrain.GetParameters().Seed) ^ 0x44495341u, Dimensions);
	Disasters.SetFrequency(CityIdentity.DisasterFrequency, EconomyTuning.FixedStepsPerMonth);
	RefreshDerivedSnapshots();
	if (bStartRunning)
	{
		Scenario.Start();
	}
	bResetConfirmationPending = false;
	++CitySessionGeneration;
	++PresentationGeneration;
}

FString UCitySimulationSubsystem::GetScenarioStatus() const
{
	return Scenario.Describe(Economy.GetSnapshot().Residents);
}

FString UCitySimulationSubsystem::GetTutorialStatus() const
{
	return FCityFirstPlayableGuide::DescribeNextStep(GetGuideState());
}

FCityGuidedObjective UCitySimulationSubsystem::GetGuidedObjective() const
{
	return FCityFirstPlayableGuide::BuildObjective(GetGuideState(), Guidance);
}

FCityFirstPlayableGuideState UCitySimulationSubsystem::GetGuideState() const
{
	if (!PlacementModel)
	{
		return {};
	}

	const FCityEconomySnapshot& EconomySnapshot = Economy.GetSnapshot();
	const FCityScenarioSnapshot& ScenarioSnapshot = Scenario.GetSnapshot();
	FCityFirstPlayableGuideState State;
	State.ScenarioPhase = ScenarioSnapshot.Phase;
	State.Residents = EconomySnapshot.Residents;
	State.Jobs = EconomySnapshot.Jobs;
	State.ProfitableMonthStreak = ScenarioSnapshot.ProfitableMonthStreak;
	State.bHasConnectedEntranceRoad = PlacementModel->IsRoadConnected(PlacementModel->GetEntranceCell());
	State.bHasResidentialZone = PlacementModel->HasZone(ECityZoneType::Residential);
	State.bHasCommercialZone = PlacementModel->HasZone(ECityZoneType::Commercial);
	State.BuiltPowerBuildings = ServiceSnapshot.BuiltPowerBuildings;
	State.OperationalPowerBuildings = ServiceSnapshot.OperationalPowerBuildings;
	State.BuiltServiceBuildings = ServiceSnapshot.BuiltServiceBuildings;
	State.OperationalServiceBuildings = ServiceSnapshot.OperationalServiceBuildings;
	State.CrimePopulationThreshold = EconomyTuning.CrimePopulationThreshold;
	State.CrimeGraceMonths = EconomyTuning.CrimeGraceMonths;
	State.FirePopulationThreshold = EconomyTuning.FirePopulationThreshold;
	State.FireGraceMonths = EconomyTuning.FireGraceMonths;
	State.bCrimeIntroduced = Crime.GetSnapshot().IsIntroduced();
	State.OperationalPolice = Crime.GetSnapshot().OperationalStations;
	for (const auto& Pair : PlacementModel->GetEntities())
		if (Pair.Value.DefinitionId == TEXT("Police"))
			++State.BuiltPolice;
	State.bFireIntroduced = Fire.GetSnapshot().IsIntroduced();
	State.OperationalSchools = ServiceSnapshot.OperationalSchools;
	State.EducationServed = ServiceSnapshot.EducationServed;
	State.SecondaryResidents = ServiceSnapshot.SecondaryResidents;
	State.OperationalLargeParks = ServiceSnapshot.OperationalLargeParks;
	for (const auto& H : ServiceSnapshot.HospitalCapacities)
		State.HospitalCapacity += H.Value;
	for (const auto& D : Waste.GetSnapshot().Depots)
		State.WasteFleet += GetWastePolicy(D.EntityId).Fleet;
	State.WasteCollected = Waste.GetSnapshot().Collected;
	State.WasteRecycled = Waste.GetSnapshot().Recycled;
	State.IllResidents = Health.GetSnapshot().Patients.Num();
	State.RecoveredResidents = Health.GetSnapshot().TotalRecovered;
	State.OperationalFire = Fire.GetSnapshot().OperationalStations;
	State.BurningBuildings = Fire.GetSnapshot().ActiveIncidents;
	State.DamagedBuildings = Fire.GetSnapshot().DamagedBuildings.Num();
	for (const auto& Pair : PlacementModel->GetEntities())
		if (Pair.Value.DefinitionId == TEXT("Fire"))
			++State.BuiltFire;
	State.bHasIndustrialZone = PlacementModel->HasZone(ECityZoneType::Industrial);
	State.CompletedPurchases = Population.GetSnapshot().TotalArrivalPurchases;
	State.PowerShortage = NeedsSnapshot.PowerShortage;
	State.WaterShortage = NeedsSnapshot.UnservedServiceDemand;
	State.UpkeepShortfall = EconomySnapshot.UpkeepShortfall;
	State.Funds = PlacementModel->GetFunds();
	State.IndustryCost = NeedsSnapshot.IndustrialGrowthCost;
	if (BuildingCatalog)
	{
		if (const FCityBuildingDefinition* FireStation = BuildingCatalog->FindDefinition(TEXT("Fire")))
			State.FireCost = FireStation->Cost;
		if (const FCityBuildingDefinition* Police = BuildingCatalog->FindDefinition(TEXT("Police")))
			State.PoliceCost = Police->Cost;
		if (const FCityBuildingDefinition* Power = BuildingCatalog->FindDefinition(TEXT("Power")))
			State.PowerCost = Power->Cost;
		if (const FCityBuildingDefinition* Water = BuildingCatalog->FindDefinition(TEXT("Service")))
			State.WaterCost = Water->Cost;
	}
	return State;
}

FString UCitySimulationSubsystem::GetTimeStatus() const
{
	return Economy.IsPaused() ? TEXT("Time: PAUSED")
	                          : FString::Printf(TEXT("Time: %dx"), Economy.GetSpeedMultiplier());
}

FCitySaveDocument UCitySimulationSubsystem::BuildSaveDocument() const
{
	FCitySaveDocument Document(PlacementModel->GetGrid().GetDimensions());
	Document.Identity = CityIdentity;
	Document.DifficultyRules = EconomyTuning;
	Document.Placement = PlacementModel->ExportState();
	Document.Economy = Economy.GetSnapshot();
	Document.Reports = Reports;
	Document.EconomyAccumulatorSeconds = Economy.GetAccumulatorSeconds();
	Document.bSimulationPaused = Economy.IsPaused();
	Document.SimulationSpeed = Economy.GetSpeedMultiplier();
	Document.GrowthRandomState = Growth.GetRandomState();
	Document.NextGrowthStep = Growth.GetNextGrowthStep();
	const FCityScenarioSnapshot& ScenarioSnapshot = Scenario.GetSnapshot();
	Document.Scenario.State = static_cast<int32>(ScenarioSnapshot.Phase);
	Document.Scenario.ProfitableMonthStreak = ScenarioSnapshot.ProfitableMonthStreak;
	Document.Scenario.bComplete = ScenarioSnapshot.Phase == ECityScenarioPhase::Succeeded;
	Document.Scenario.LastObservedBilledMonthId = ScenarioSnapshot.LastObservedBilledMonthId;
	Document.Guidance = Guidance;
	Document.Crime = Crime.GetSnapshot();

	Document.Fire = Fire.GetSnapshot();
	Document.Health = Health.GetSnapshot();
	Document.Waste = Waste.GetSnapshot();

	Document.Disasters = Disasters.GetSnapshot();

	Document.Terrain = Terrain.GetParameters();
	Document.Environment = Environment.GetSnapshot();
	Document.Development = Development.GetSnapshot();

	Document.Population = Population.GetSnapshot();

	FCityTrafficSimulation SaveTraffic = Traffic;
	FCityRoadNetworkSnapshot SaveNetwork;
	FCityRoadNetworkBuilder::Build(*PlacementModel, FCityRoadDefinition::Local(), SaveNetwork);
	SaveTraffic.ReconcileNetwork(SaveNetwork);
	Document.Traffic = SaveTraffic.GetSnapshot();

	return Document;
}

bool UCitySimulationSubsystem::ValidateSaveDocument(
    const FCitySaveDocument& Document, TUniquePtr<FCityPlacementModel>& OutModel,
    FCityEconomySimulation& OutEconomy, FCityGrowthSimulation& OutGrowth,
    FCityEnvironmentSimulation& OutEnvironment, FCityDevelopmentSimulation& OutDevelopment,
    FCityPopulationSimulation& OutPopulation, FCityTrafficSimulation& OutTraffic,
    FCityTerrainHeightfield& OutTerrain, FString& OutError) const
{
	if (!Document.ValidateDimensions(OutError) || !Document.Identity.Validate(OutError) ||
	    !Document.Reports.Validate(Document.Economy.LastBilledMonthId, OutError))
		return false;
	const auto CandidateTuning = SavedTuning(EconomyTuning, Document);
	if (!BuildingCatalog)
	{
		OutError = TEXT("Building catalog is unavailable");
		return false;
	}
	FCityScenarioSimulation CandidateScenario;
	FCityScenarioSnapshot ScenarioSnapshot;
	ScenarioSnapshot.Phase = static_cast<ECityScenarioPhase>(Document.Scenario.State);
	ScenarioSnapshot.ProfitableMonthStreak = Document.Scenario.ProfitableMonthStreak;
	ScenarioSnapshot.LastObservedBilledMonthId = Document.Scenario.LastObservedBilledMonthId;
	if (Document.Scenario.bComplete != (ScenarioSnapshot.Phase == ECityScenarioPhase::Succeeded) ||
	    ScenarioSnapshot.LastObservedBilledMonthId > Document.Economy.LastBilledMonthId ||
	    !CandidateScenario.Restore(ScenarioSnapshot, OutError))
	{
		if (OutError.IsEmpty())
		{
			OutError = TEXT("Scenario progress is ahead of the economy clock");
		}
		return false;
	}
	if (!FCityFirstPlayableGuide::ValidateProgress(Document.Guidance, OutError))
		return false;
	if ((Document.Crime.LastFixedStepId != Document.Economy.FixedStepId ||
	     !FCityCrimeSimulation::ValidateState(Document.Crime, Document.Economy.FixedStepId, OutError)))
	{
		if (OutError.IsEmpty())
			OutError = TEXT("Crime and economy clocks do not match");
		return false;
	}
	TMap<FName, FCityBuildingDefinition> Definitions;
	for (const FCityBuildingDefinition& Definition : BuildingCatalog->Definitions)
	{
		if (Definition.IsValid())
		{
			Definitions.Add(Definition.DefinitionId, Definition);
		}
	}
	OutModel = FCityPlacementModel::CreateValidatedFromState(Document.Placement, Definitions, OutError);
	OutEnvironment = FCityEnvironmentSimulation(Document.GetDimensions());
	if (!OutModel ||
	    !OutEconomy.RestoreState(Document.Economy, Document.EconomyAccumulatorSeconds,
	                             Document.bSimulationPaused, Document.SimulationSpeed, CandidateTuning,
	                             OutError) ||
	    !OutGrowth.RestoreState(Document.GrowthRandomState, Document.NextGrowthStep, OutError) ||
	    !OutEnvironment.RestoreState(Document.Environment, Document.Economy.FixedStepId, OutError) ||
	    !OutTerrain.Generate(Document.Terrain, &OutError))
	{
		return false;
	}
	if (!OutModel->SetTerrain(&OutTerrain, &OutError) || !OutModel->ValidateRoadStructures(OutError))
		return false;
	if ((Document.Disasters.LastFixedStepId != Document.Economy.FixedStepId ||
	     !FCityDisasterSimulation::ValidateState(Document.Disasters, Document.Economy.FixedStepId, OutError)))
		return false;
	{
		if (Document.Fire.LastFixedStepId != Document.Economy.FixedStepId ||
		    !FCityFireSimulation::ValidateState(Document.Fire, Document.Economy.FixedStepId, OutError))
		{
			if (OutError.IsEmpty())
				OutError = TEXT("Fire clock mismatch");
			return false;
		}
		for (const auto& Pair : Document.Fire.DamagedBuildings)
		{
			const auto* Entity = OutModel->GetEntities().Find(Pair.Key);
			if (!Entity || (!FCityFireSimulation::IsFireTarget(Entity->DefinitionId, CandidateTuning) &&
			                Entity->DefinitionId != TEXT("Fire")))
			{
				OutError = TEXT("Fire damage references a missing or incompatible building");
				return false;
			}
		}
	}
	{
		const int64 SavedHalfDay =
		    FCityCalendar::HalfDayIndex(Document.Economy.FixedStepId,
		    Document.EconomyAccumulatorSeconds / CandidateTuning.FixedStepSeconds, CandidateTuning.FixedStepsPerMonth);
		if (!OutDevelopment.RestoreState(Document.Development, *OutModel, BuildingCatalog,
		                                 Document.Economy.FixedStepId, OutError, SavedHalfDay, CandidateTuning.FixedStepsPerMonth))
		{
			return false;
		}
	}
	{
		if (!OutPopulation.RestoreState(Document.Population, OutError))
			return false;
		OutPopulation.SyncHealthAvailability(Document.Health);
	}
	if (!FCityWasteSimulation::ValidateState(Document.Waste, Document.Economy.FixedStepId, OutError) ||
	    !FCityWasteSimulation::ValidateReferences(Document.Waste, *OutModel, OutPopulation.GetSnapshot(),
	                                              OutError))
		return false;
	if (!FCityHousingMigration::Validate(OutPopulation.GetSnapshot(), OutError, OutModel.Get()))
		return false;
	TMap<uint64, int32> JailOccupancy;
	for (const auto& C : OutPopulation.GetSnapshot().Citizens)
	{
		if (C.IsDetained())
		{
			const auto* Station = OutModel->GetEntities().Find(C.JailStationId);
			if (!Station || Station->DefinitionId != TEXT("Police") ||
			    ++JailOccupancy.FindOrAdd(C.JailStationId) >
			        FCityCrimeSimulation::JailCapacity(OutDevelopment.GetSnapshot(), C.JailStationId))
			{
				OutError = TEXT("Invalid detention station or jail capacity");
				return false;
			}
		}
		if (C.LastEducationMonth > Document.Economy.LastBilledMonthId ||
		    C.LastHappinessMonth > Document.Economy.LastBilledMonthId)
		{
			OutError = TEXT("Future education or happiness clock");
			return false;
		}
		if (C.bSkilledJob)
		{
			const auto* B = OutPopulation.GetSnapshot().Businesses.FindByPredicate(
			    [&](const auto& V) { return V.EntityId == C.WorkplaceEntityId; });
			if (!B || B->SkilledJobCapacity <= 0)
			{
				OutError = TEXT("Invalid skilled role employer");
				return false;
			}
		}
	}
	for (const auto& E : OutPopulation.GetSnapshot().PublicEmployers)
	{
		const auto* Entity = OutModel->GetEntities().Find(E.EntityId);
		if (!Entity || !FCityPopulationSimulation::PublicJobs(Entity->DefinitionId, CandidateTuning))
		{
			OutError = TEXT("Invalid public employer reference");
			return false;
		}
	}
	for (const auto& Trip : OutPopulation.GetSnapshot().TransitTrips)
	{
		if (Trip.IsHealthJourney())
		{
			const auto* P = Document.Health.Find(Trip.PatientCitizenId);
			const auto* H = OutModel->GetEntities().Find(Trip.StationEntityId);
			if (!P || P->HospitalId != Trip.StationEntityId || !H || H->DefinitionId != TEXT("Hospital") ||
			    (Trip.OriginEntityId && !OutModel->GetEntities().Contains(Trip.OriginEntityId)) ||
			    (Trip.DestinationEntityId && !OutModel->GetEntities().Contains(Trip.DestinationEntityId)) ||
			    (Trip.Purpose == ECityTripPurpose::HealthWalkIn ? P->Care != ECityHealthCare::WalkIn
			     : Trip.Purpose == ECityTripPurpose::HealthDriveIn ? P->Care != ECityHealthCare::DriveIn
			     : Trip.Purpose == ECityTripPurpose::HealthReturnHome ? P->Care != ECityHealthCare::ReturningHome
			     : P->Care != ECityHealthCare::AwaitingAmbulance))
			{
				OutError = TEXT("Hospital journey patient/reservation mismatch");
				return false;
			}
			continue;
		}
		if (!Trip.IsEmergency())
			continue;
		const auto* Station = OutModel->GetEntities().Find(Trip.StationEntityId);
		if (!Station || !OutModel->GetEntities().Contains(Trip.OriginEntityId) ||
		    !OutModel->GetEntities().Contains(Trip.DestinationEntityId) ||
		    Station->DefinitionId !=
		        (Trip.VehicleType == ECityTripVehicleType::FireEngine ? TEXT("Fire") : TEXT("Police")))
		{
			OutError = TEXT("Invalid emergency station/target reference");
			return false;
		}
		if (Trip.Purpose == ECityTripPurpose::FireResponse && Trip.OriginEntityId != Trip.StationEntityId &&
		    !FCityFireSimulation::IsFireTarget(
		        OutModel->GetEntities().FindChecked(Trip.OriginEntityId).DefinitionId, CandidateTuning))
		{
			OutError = TEXT("Invalid fire response scene origin");
			return false;
		}
		const bool bValidIncident = Trip.VehicleType == ECityTripVehicleType::FireEngine
		                                ? Document.Fire.RecentIncidents.ContainsByPredicate(
		                                      [&](const auto& I) { return I.Id == Trip.IncidentId; })
		                                : Document.Crime.RecentIncidents.ContainsByPredicate(
		                                      [&](const auto& I) { return I.Id == Trip.IncidentId; });
		if (Trip.Purpose == ECityTripPurpose::FireResponse &&
		    Document.Fire.RecentIncidents.ContainsByPredicate([&](const auto& I) {
			    return I.Id == Trip.IncidentId && I.EntityId != Trip.DestinationEntityId;
		    }))
		{
			OutError = TEXT("Fire response incident/target mismatch");
			return false;
		}
		if (!bValidIncident && Trip.Purpose != ECityTripPurpose::EmergencyReturn &&
		    Trip.StartStep >=
		        (Trip.VehicleType == ECityTripVehicleType::FireEngine
		             ? Document.Fire.LastFixedStepId - FCityFireSimulation::MaximumBurnSteps
		             : Document.Crime.LastFixedStepId - FCityCrimeSimulation::IncidentDurationSteps))
		{
			OutError = TEXT("Emergency trip references missing incident");
			return false;
		}
	}
	FCityRoadNetworkSnapshot TrafficNetwork;
	FCityRoadNetworkBuilder::Build(*OutModel, FCityRoadDefinition::Local(), TrafficNetwork);
	FCityTrafficSnapshot TrafficState = Document.Traffic;

	if (TrafficState.FixedStepId != Document.Economy.FixedStepId)
	{
		OutError = TEXT("Traffic and economy clocks do not match");
		return false;
	}
	for (const auto* Records : {&TrafficState.Vehicles, &TrafficState.WaitingDepartures})
	for (const auto& Vehicle : *Records)
	{
		if (!Vehicle.SourceId)
			continue;
		const auto* Trip = Vehicle.bTransitTrip ? Document.Population.TransitTrips.FindByPredicate(
		                                              [&](const auto& T) { return T.Id == Vehicle.SourceId; })
		                                        : nullptr;
		const bool bExpectedResponse = Trip && (Trip->Purpose == ECityTripPurpose::FireResponse ||
		                                        Trip->Purpose == ECityTripPurpose::PoliceResponse ||
		                                        Trip->Purpose == ECityTripPurpose::HealthPickup ||
		                                        Trip->Purpose == ECityTripPurpose::HealthAdmission);
		if (Vehicle.bEmergencyResponding != bExpectedResponse)
		{
			OutError = TEXT("Traffic response priority mismatches its source trip");
			return false;
		}
	}
	if (!OutTraffic.RestoreState(TrafficState, TrafficNetwork, Document.Economy.FixedStepId, OutError))
		return false;
	return true;
}

bool UCitySimulationSubsystem::SaveCityToPath(const FString& SavePath, FString& OutError,
                                              const bool bSimulateInterruptionBeforeReplace)
{
#if CITYSIM_CAPACITY_GRID_SIZE != 64
	OutError = TEXT("Isolated capacity builds cannot write city saves");
	return false;
#else
	if (!PlacementModel)
	{
		OutError = TEXT("City simulation is unavailable");
		return false;
	}
	FString Json;
	if (!ValidateRuntimeGrid(PlacementModel->GetGrid().GetDimensions(), GetWorld(), OutError) ||
	    !FCityPersistence::Serialize(BuildSaveDocument(), Json, OutError) ||
	    !FCityPersistence::WriteAtomic(SavePath, Json, OutError, bSimulateInterruptionBeforeReplace))
	{
		PersistenceStatus = TEXT("Save failed: ") + OutError;
		return false;
	}
	PersistenceStatus = TEXT("City saved locally");
	UE_LOG(LogTemp, Display, TEXT("CITYSIM_SAVE_OK Path=%s"), *SavePath);
	return true;
#endif
}

bool UCitySimulationSubsystem::LoadCityFromPath(const FString& SavePath, FString& OutError,
                                                const bool bAllowBackup)
{
	bool bIncompatible = false;
	auto TryLoad = [this, &bIncompatible](const FString& CandidatePath, FString& Error) -> bool {
		FString Json;
		FCitySaveDocument Document;
		TUniquePtr<FCityPlacementModel> CandidateModel;
		FCityEconomySimulation CandidateEconomy;
		FCityGrowthSimulation CandidateGrowth;
		FCityEnvironmentSimulation CandidateEnvironment;
		FCityDevelopmentSimulation CandidateDevelopment;
		FCityPopulationSimulation CandidatePopulation;
		FCityTrafficSimulation CandidateTraffic;
		FCityTerrainHeightfield CandidateTerrain;
		ECitySaveReadFailure Failure = ECitySaveReadFailure::None;
		if (!FCityPersistence::ReadFile(CandidatePath, Json, Error))
			return false;
		if (!FCityPersistence::Deserialize(Json, Document, Error, ActiveSavePlatform(GetWorld()), &Failure))
		{
			bIncompatible = Failure != ECitySaveReadFailure::InvalidData;
			return false;
		}
		if (!ValidateRuntimeGrid(Document.GetDimensions(), GetWorld(), Error))
		{
			bIncompatible = true;
			return false;
		}
		if (!ValidateSaveDocument(Document, CandidateModel, CandidateEconomy, CandidateGrowth,
		                          CandidateEnvironment, CandidateDevelopment, CandidatePopulation,
		                          CandidateTraffic, CandidateTerrain, Error))
		{
			return false;
		}
		// Restore every remaining authority into local candidates before touching
		// identity, clocks, the live model or session generations.
		FCityWasteSimulation CandidateWaste;
		const auto CandidateTuning = SavedTuning(EconomyTuning, Document);
		FCityFireSimulation CandidateFire;
		FCityHealthSimulation CandidateHealth;
		FCityDisasterSimulation CandidateDisasters;
		FCityCrimeSimulation CandidateCrime;
		FCityScenarioSimulation CandidateScenario;
		FCityScenarioSnapshot ScenarioSnapshot;
		ScenarioSnapshot.Phase = static_cast<ECityScenarioPhase>(Document.Scenario.State);
		ScenarioSnapshot.ProfitableMonthStreak = Document.Scenario.ProfitableMonthStreak;
		ScenarioSnapshot.LastObservedBilledMonthId = Document.Scenario.LastObservedBilledMonthId;
		if (!CandidateWaste.RestoreState(Document.Waste, Error) ||
		    !CandidateHealth.RestoreState(Document.Health, Error) ||
		    !CandidateScenario.Restore(ScenarioSnapshot, Error))
			return false;
		{
			if (!CandidateFire.RestoreState(Document.Fire, Document.Economy.FixedStepId, Error))
				return false;
		}
		{
			if (!CandidateDisasters.RestoreState(Document.Disasters, Document.Economy.FixedStepId, Error))
				return false;
		}
		{
			if (!CandidateCrime.RestoreState(Document.Crime, Document.Economy.FixedStepId, Error))
				return false;
		}
		CandidatePopulation.SyncHealthAvailability(CandidateHealth.GetSnapshot());
		CityIdentity = Document.Identity;
		EconomyTuning = CandidateTuning;
		PlacementModel = MoveTemp(CandidateModel);
		RoadNetworkOwner = DerivedOwner = nullptr;
		Economy = CandidateEconomy;
		Growth = CandidateGrowth;
		Environment = CandidateEnvironment;
		Development = CandidateDevelopment;
		Population = CandidatePopulation;
		Population.EnableHouseholdSupplies(Document.Economy.LastBilledMonthId);
		Reports = Document.Reports;
		Traffic = CandidateTraffic;
		Waste = MoveTemp(CandidateWaste);
		PreviousTraffic = Traffic.GetSnapshot();
		Terrain = MoveTemp(CandidateTerrain);
		PlacementModel->SetTerrain(&Terrain);
		Scenario = MoveTemp(CandidateScenario);
		Guidance = Document.Guidance;
		Fire = MoveTemp(CandidateFire);
		Health = MoveTemp(CandidateHealth);
		Disasters = MoveTemp(CandidateDisasters);
		Crime = MoveTemp(CandidateCrime);
		RefreshDerivedSnapshots();
		LastGrowthResult = FCityGrowthResult();
		PendingSpawnedEntityIds.Reset();
		bResetConfirmationPending = false;
		++CitySessionGeneration;
		++PresentationGeneration;
		return true;
	};

	FString PrimaryError;
	if (TryLoad(SavePath, PrimaryError))
	{
		PersistenceStatus = TEXT("City loaded");
		UE_LOG(LogTemp, Display, TEXT("CITYSIM_LOAD_OK Path=%s"), *SavePath);
		return true;
	}
	if (bAllowBackup && !bIncompatible)
	{
		FString BackupError;
		if (TryLoad(SavePath + TEXT(".bak"), BackupError))
		{
			PersistenceStatus = TEXT("Loaded backup; primary failed: ") + PrimaryError;
			UE_LOG(LogTemp, Warning, TEXT("CITYSIM_LOAD_BACKUP PrimaryError=%s"), *PrimaryError);
			return true;
		}
	}
	OutError = PrimaryError;
	PersistenceStatus = TEXT("Load failed: ") + OutError;
	UE_LOG(LogTemp, Warning, TEXT("CITYSIM_LOAD_FAILED Reason=%s"), *OutError);
	return false;
}

bool UCitySimulationSubsystem::SaveCity()
{
	FString Error;
	const FString PriorId = CityIdentity.Id;
	if (CityIdentity.Id == TEXT("00000000000000000000000000000002"))
		CityIdentity.Id = FGuid::NewGuid().ToString(EGuidFormats::Digits).ToLower();
	const FString Directory = FCitySaveLibrary::Directory();
	const auto Slots = FCitySaveLibrary::List(Directory);
	if (FCitySaveLibrary::Count(Directory) >= FCitySaveLibrary::MaxCities &&
	    !Slots.ContainsByPredicate([&](const auto& Slot) { return Slot.Identity.Id == CityIdentity.Id; }))
	{
		CityIdentity.Id = PriorId;
		PersistenceStatus = TEXT("Save failed: the 32-city library is full");
		return false;
	}
	const auto Archived = FCitySaveLibrary::List(Directory, FString(), true);
	if (Archived.ContainsByPredicate([&](const auto& Slot) { return Slot.Identity.Id == CityIdentity.Id; }))
	{
		CityIdentity.Id = PriorId;
		PersistenceStatus = TEXT("Restore this city from the archive before saving");
		return false;
	}
	const bool bSaved = SaveCityToPath(FCitySaveLibrary::PathFor(Directory, CityIdentity.Id), Error);
	if (bSaved)
	{
		FString CacheError;
		FCitySaveLibrary::RefreshIndex(Directory, CacheError);
	}
	if (!bSaved)
		CityIdentity.Id = PriorId;
	return bSaved;
}

void UCitySimulationSubsystem::HandleApplicationBackgrounded()
{
	bResumeAfterForeground = !Economy.IsPaused();
	Economy.SetPaused(true);
	ControlStatus = TEXT("Paused while headset/app is inactive");
	UE_LOG(LogTemp, Display, TEXT("CITYSIM_LIFECYCLE_BACKGROUND Paused=1"));
}

void UCitySimulationSubsystem::HandleApplicationForegrounded()
{
	if (bResumeAfterForeground)
	{
		Economy.SetPaused(false);
	}
	bResumeAfterForeground = false;
	ControlStatus = Economy.IsPaused() ? TEXT("Returned paused") : TEXT("Simulation resumed");
	UE_LOG(LogTemp, Display, TEXT("CITYSIM_LIFECYCLE_FOREGROUND Paused=%d"), Economy.IsPaused());
}

bool UCitySimulationSubsystem::LoadCity()
{
	FString Error;
	return LoadCityFromPath(FCityPersistence::GetDefaultSavePath(), Error);
}

void UCitySimulationSubsystem::RunStorageProbe()
{
	if (bStorageProbeRun || !BuildingCatalog || !PlacementModel)
	{
		return;
	}
	bStorageProbeRun = true;
	const FString ProbePath =
	    FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"), TEXT("CityStorageProbe.json"));
	FString Error;
	FString Json;
	FCitySaveDocument RoundTrip;
	TUniquePtr<FCityPlacementModel> ProbeModel;
	FCityEconomySimulation ProbeEconomy;
	FCityGrowthSimulation ProbeGrowth;
	FCityEnvironmentSimulation ProbeEnvironment;
	FCityDevelopmentSimulation ProbeDevelopment;
	FCityPopulationSimulation ProbePopulation;
	FCityTrafficSimulation ProbeTraffic;
	FCityTerrainHeightfield ProbeTerrain;
	const bool bPassed =
	    FCityPersistence::Serialize(BuildSaveDocument(), Json, Error) &&
	    FCityPersistence::WriteAtomic(ProbePath, Json, Error) &&
	    FCityPersistence::ReadFile(ProbePath, Json, Error) &&
	    FCityPersistence::Deserialize(Json, RoundTrip, Error) &&
	    ValidateSaveDocument(RoundTrip, ProbeModel, ProbeEconomy, ProbeGrowth, ProbeEnvironment,
	                         ProbeDevelopment, ProbePopulation, ProbeTraffic, ProbeTerrain, Error);
	IFileManager::Get().Delete(*ProbePath, false, true, true);
	IFileManager::Get().Delete(*(ProbePath + TEXT(".tmp")), false, true, true);
	IFileManager::Get().Delete(*(ProbePath + TEXT(".bak")), false, true, true);
	if (bPassed)
	{
		UE_LOG(LogTemp, Display, TEXT("CITYSIM_STORAGE_PROBE_OK Path=%s"), *ProbePath);
	}
	else
	{
		PersistenceStatus = TEXT("Storage unavailable: ") + Error;
		UE_LOG(LogTemp, Error, TEXT("CITYSIM_STORAGE_PROBE_FAILED Reason=%s"), *Error);
	}
}

int64 UCitySimulationSubsystem::GetRepairCost(const uint64 EntityId) const
{
	const auto* Entity = PlacementModel ? PlacementModel->GetEntities().Find(EntityId) : nullptr;
	const auto* Definition =
	    Entity && BuildingCatalog ? BuildingCatalog->FindDefinition(Entity->DefinitionId) : nullptr;
	return Definition ? Fire.RepairCost(EntityId, *Definition, EconomyTuning) : 0;
}

bool UCitySimulationSubsystem::StartHealthTest(const uint64 EntityId, const bool bSevere)
{
	if (UE_BUILD_SHIPPING || !PlacementModel || !PlacementModel->GetEntities().Contains(EntityId))
		return false;
	for (const auto& C : Population.GetSnapshot().Citizens)
		if ((C.HomeEntityId == EntityId || C.WorkplaceEntityId == EntityId) && !C.IsDetained() &&
		    !Health.GetSnapshot().Find(C.Id))
		{
			const bool Started =
			    Health.StartIllness(C.Id, bSevere ? ECityIllness::Severe : ECityIllness::Mild);
			Population.SyncHealthAvailability(Health.GetSnapshot());
			if (Started)
				++PresentationGeneration;
			ControlStatus =
			    Started ? TEXT("Citizen ill: run time for hospital care") : TEXT("Sickness test unavailable");
			return Started;
		}
	ControlStatus = TEXT("Point at a home or workplace with a healthy citizen");
	return false;
}

bool UCitySimulationSubsystem::StartSafetyTest(const uint64 EntityId, const bool bFire, const bool bCrime)
{
	const auto* E = PlacementModel ? PlacementModel->GetEntities().Find(EntityId) : nullptr;
	if (!E || (!bFire && !bCrime))
	{
		ControlStatus = TEXT("Point the test tool at a building and pull trigger");
		return false;
	}
	if (E->DefinitionId == TEXT("Fire") && bFire)
	{
		ControlStatus = TEXT("Fire stations are fireproof");
		return false;
	}
	if (!Development.GetSnapshot().IsOperational(EntityId) ||
	    (bFire && !FCityFireSimulation::IsFireTarget(E->DefinitionId, EconomyTuning)) ||
	    (bCrime && E->DefinitionId != EconomyTuning.HomeDefinitionId &&
	     E->DefinitionId != EconomyTuning.ShopDefinitionId &&
	     E->DefinitionId != EconomyTuning.IndustrialDefinitionId))
	{
		ControlStatus = TEXT(
		    "Choose a completed home, shop or industry for crime/arson; fire also supports utilities/police");
		return false;
	}
	// Validate both halves on copies so an arson rejection cannot create half an event.
	auto CandidateFire = Fire;
	auto CandidateCrime = Crime;
	if ((bFire && !CandidateFire.StartTest(EntityId, ControlStatus)) ||
	    (bCrime && !CandidateCrime.StartTest(EntityId, ControlStatus)))
		return false;
	if (bFire && bCrime)
		CandidateCrime.LinkArson(EntityId, Population.GetSnapshot(), CandidateFire);
	Fire = MoveTemp(CandidateFire);
	Crime = MoveTemp(CandidateCrime);
	ControlStatus = bFire && bCrime ? TEXT("Test arson started: fire and crime; real damage/response")
	                : bFire         ? TEXT("Test fire started: real spread/damage/repair")
	                                : TEXT("Test crime started: normal police response");
	if (Economy.IsPaused())
		ControlStatus += TEXT("; resume time to dispatch");
	RefreshDerivedSnapshots();
	++PresentationGeneration;
	return true;
}

bool UCitySimulationSubsystem::RepairBuilding(const uint64 EntityId)
{
	const auto* Entity = PlacementModel ? PlacementModel->GetEntities().Find(EntityId) : nullptr;
	const auto* Definition =
	    Entity && BuildingCatalog ? BuildingCatalog->FindDefinition(Entity->DefinitionId) : nullptr;
	if (!Definition)
	{
		ControlStatus = TEXT("Point REPAIR at a damaged building");
		return false;
	}
	const bool Success = Fire.Repair(EntityId, *PlacementModel, *Definition, EconomyTuning, ControlStatus);
	RefreshDerivedSnapshots();
	return Success;
}

bool UCitySimulationSubsystem::CreateCity(const FCityIdentity& Identity,
                                          const FCityTerrainGenerationParameters& Parameters,
                                          const FString& Directory, FString& Error)
{
	if (!ValidateRuntimeGrid(Parameters.Dimensions, GetWorld(), Error))
		return false;
	if (!Identity.Validate(Error) || !Parameters.IsValid())
	{
		if (Error.IsEmpty())
			Error = TEXT("Invalid new terrain parameters");
		return false;
	}
	const FString Path = FCitySaveLibrary::PathFor(Directory, Identity.Id);
	const FString ArchivedPath =
	    FCitySaveLibrary::PathFor(FCitySaveLibrary::ArchiveDirectory(Directory), Identity.Id);
	if (IFileManager::Get().FileExists(*ArchivedPath) ||
	    IFileManager::Get().FileExists(*(ArchivedPath + TEXT(".bak"))) || Directory.IsEmpty() ||
	    IFileManager::Get().FileExists(*Path) || IFileManager::Get().FileExists(*(Path + TEXT(".bak"))) ||
	    FCitySaveLibrary::Count(Directory) >= FCitySaveLibrary::MaxCities)
	{
		Error = TEXT("City slot already exists or the 32-city library is full");
		return false;
	}
	FCitySaveDocument Document(Parameters.Dimensions);
	Document.Identity = Identity;
	Document.Identity.CreatedGameRevision = FCityGameRevision::Current();
	Document.Terrain = Parameters;
	const FCityEconomyTuning Tuning = FCityIdentity::Apply(Identity.Difficulty, EconomyTuning);
	Document.DifficultyRules = Tuning;
	FCityPlacementModel FreshModel(FCityIdentity::StartingFunds(Identity.Difficulty), Parameters.Dimensions);
	Document.Placement = FreshModel.ExportState();
	Document.bSimulationPaused = true;
	FCityGrowthSimulation FreshGrowth(static_cast<uint32>(Tuning.GrowthSeed));
	Document.GrowthRandomState = FreshGrowth.GetRandomState();
	Document.NextGrowthStep = FreshGrowth.GetNextGrowthStep();
	FCityEnvironmentSimulation FreshEnvironment(Parameters.Dimensions);
	FreshEnvironment.Reset();
	Document.Environment = FreshEnvironment.GetSnapshot();
	FCityDisasterSimulation FreshDisasters;
	FreshDisasters.Reset(0, static_cast<uint32>(Parameters.Seed) ^ 0x44495341u, Parameters.Dimensions);
	FreshDisasters.SetFrequency(Identity.DisasterFrequency, Tuning.FixedStepsPerMonth);
	Document.Disasters = FreshDisasters.GetSnapshot();
	FCityScenarioSimulation FreshScenario;
	FreshScenario.Start();
	Document.Scenario.State = static_cast<int32>(FreshScenario.GetSnapshot().Phase);
	TUniquePtr<FCityPlacementModel> Model;
	FCityEconomySimulation CandidateEconomy;
	FCityGrowthSimulation CandidateGrowth;
	FCityEnvironmentSimulation CandidateEnvironment;
	FCityDevelopmentSimulation CandidateDevelopment;
	FCityPopulationSimulation CandidatePopulation;
	FCityTrafficSimulation CandidateTraffic;
	FCityTerrainHeightfield CandidateTerrain;
	FString Json;
	if (!ValidateSaveDocument(Document, Model, CandidateEconomy, CandidateGrowth, CandidateEnvironment,
	                          CandidateDevelopment, CandidatePopulation, CandidateTraffic, CandidateTerrain,
	                          Error) ||
	    !FCityPersistence::Serialize(Document, Json, Error))
		return false;
	FCitySaveDocument RoundTrip;
	if (!FCityPersistence::Deserialize(Json, RoundTrip, Error) ||
	    !ValidateSaveDocument(RoundTrip, Model, CandidateEconomy, CandidateGrowth, CandidateEnvironment,
	                          CandidateDevelopment, CandidatePopulation, CandidateTraffic, CandidateTerrain,
	                          Error) ||
	    !FCityPersistence::WriteAtomic(Path, Json, Error))
		return false;
	// The exact validated file is the commit point; failure keeps the old city in memory.
	const bool bLoaded = LoadCityFromPath(Path, Error, false);
	if (bLoaded)
	{
		FString CacheError;
		FCitySaveLibrary::RefreshIndex(Directory, CacheError);
	}
	return bLoaded;
}

bool UCitySimulationSubsystem::SetDisasterFrequency(const ECityDisasterFrequency Frequency)
{
	if (static_cast<uint8>(Frequency) > 3)
		return false;
	if (CityIdentity.DisasterFrequency == Frequency)
		return true;
	CityIdentity.DisasterFrequency = Frequency;
	Disasters.SetFrequency(Frequency, EconomyTuning.FixedStepsPerMonth);
	ControlStatus = FString::Printf(
	    TEXT("Natural disasters: %s. Building fires use normal safety rules. Save to retain this setting."),
	    FCityIdentity::DisasterFrequencyName(Frequency));
	return true;
}

bool UCitySimulationSubsystem::StartDisasterTest(const ECityNaturalDisaster Kind)
{
	TArray<uint64> Ids;
	PlacementModel->GetEntities().GetKeys(Ids);
	Ids.Sort();
	for (const uint64 Id : Ids)
	{
		const auto& E = PlacementModel->GetEntities().FindChecked(Id);
		if ((E.DefinitionId == TEXT("Home") || E.DefinitionId == TEXT("Shop") ||
		     E.DefinitionId == TEXT("Factory")) &&
		    Development.GetSnapshot().IsOperational(Id) && !Fire.GetSnapshot().IsDisabled(Id))
		{
			const bool Started = Disasters.StartTest(
			    Kind, E.Anchor, static_cast<int32>(Environment.GetSnapshot().WindDirection));
			ControlStatus = Started ? TEXT("Disaster test started near the first completed "
			                               "home/shop/factory; real damage and repair")
			                        : TEXT("A disaster is already active");
			return Started;
		}
	}
	ControlStatus = TEXT("Build an undamaged completed home, shop or factory first");
	return false;
}

const FCityWasteFacilityPolicy UCitySimulationSubsystem::GetWastePolicy(uint64 Id) const
{
	return FCityWasteSimulation::Policy(Id, *PlacementModel, BuildingCatalog, Development.GetSnapshot(),
	                                    ServiceSnapshot, Economy.GetSnapshot().HasOperatingUpkeepShortfall());
}
bool UCitySimulationSubsystem::StartWasteTest(uint64 Id)
{
	RefreshDerivedSnapshots();
	const bool Success = Waste.AddTestPile(Id);
	ControlStatus = Success ? TEXT("Test waste: lot filled; collection needs a staffed depot")
	                        : TEXT("Choose a home, shop or factory");
	if (Success)
		++PresentationGeneration;
	return Success;
}
