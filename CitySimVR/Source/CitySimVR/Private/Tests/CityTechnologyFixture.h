#pragma once
#include "Simulation/CitySimulationSubsystem.h"
#include "Simulation/CityTechnology.h"
#include "Simulation/CityCalendar.h"
#include "Simulation/CityDevelopmentSimulation.h"
#include "Simulation/CityPopulationSimulation.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Presentation/CityBoardActor.h"
#include "UObject/StrongObjectPtr.h"

// Test-owned populated city. Actual player profiles are never accessed.
class FCityTechnologyFixture
{
  public:
	// Packaged latent cases share a world. Restore the authored calendar/catalog
	// and clear this synthetic city even when a case returns early with an error.
	class FScopedState
	{
	public:
		FScopedState(UCitySimulationSubsystem* InSim, ACityBoardActor* InBoard)
		    : Sim(InSim), Board(InBoard), Tuning(InSim->EconomyTuning),
		      TerrainParameters(InSim->Terrain.GetParameters()), Catalog(InSim->BuildingCatalog.Get()) {}
		~FScopedState()
		{
			if (auto* Target = Sim.Get())
			{
				Target->EconomyTuning = Tuning;
				Target->Terrain.Generate(TerrainParameters);
				Target->RegisterBuildingCatalog(Catalog.Get());
				Target->ResetCity(false);
				if (Board.IsValid()) Board->PrepareCityView();
			}
		}
	private:
		TWeakObjectPtr<UCitySimulationSubsystem> Sim;
		TWeakObjectPtr<ACityBoardActor> Board;
		FCityEconomyTuning Tuning;
		FCityTerrainGenerationParameters TerrainParameters;
		TStrongObjectPtr<UCityBuildingCatalog> Catalog;
	};
	static void Month(UCitySimulationSubsystem* Sim)
	{
		Sim->SetSimulationPaused(false);
		for (int32 I = 0; I < Sim->EconomyTuning.FixedStepsPerMonth; ++I)
			Sim->Tick(.125f);
		Sim->SetSimulationPaused(true);
	}
	static bool LoadWithAuthoredTuning(UCitySimulationSubsystem* Sim, const FString& Path, FString& Error)
	{
		const auto TestTuning = Sim->EconomyTuning;
		Sim->EconomyTuning = FCityEconomyTuning();
		const bool Loaded = Sim->LoadCityFromPath(Path, Error, false);
		Sim->EconomyTuning = TestTuning;
		return Loaded;
	}
	static bool Seed(UCitySimulationSubsystem* Sim, ACityBoardActor* Board = nullptr, bool AiWorkforce = false,
	                 bool AuthoredCalendar = false)
	{
		auto Parameters = Sim->Terrain.GetParameters();
		Parameters.GeneratorVersion = 0;
		Parameters.BroadElevationAmplitudeCm = Parameters.HillAmplitudeCm = Parameters.RidgeAmplitudeCm = 0;
		Sim->Terrain.Generate(Parameters);
		Sim->ResetCity(false);
		auto* Original = LoadObject<UCityBuildingCatalog>(
		    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
		if (!Original)
			return false;
		auto* Catalog = NewObject<UCityBuildingCatalog>();
		Catalog->Definitions = Original->Definitions;
		for (auto& D : Catalog->Definitions)
			if (D.DevelopmentDemandType != ECityDevelopmentDemandType::None && !D.DensityLevels.IsEmpty())
				D.DensityLevels.SetNum(1);
		Sim->RegisterBuildingCatalog(Catalog);
		auto& City = Sim->GetPlacementModel();
		City.ApplyMonthlyEconomy(100000, 0);
		auto Road = City.BeginRoadPreview(City.GetEntranceCell());
		City.UpdateRoadPreviewEnd(Road, {40, 8});
		if (!City.CommitRoad(Road).IsSuccess())
			return false;
		auto Place = [&](const FName Id, int32 X, int32 Y, int32 Rotation = 0) {
			const auto* D = Catalog->FindDefinition(Id);
			if (!D)
				return false;
			auto Preview = City.BeginPreview(*D, {X, Y}, Rotation);
			return City.CommitPlacement(Preview, *D).IsSuccess();
		};
		const auto* WaterDefinition = Catalog->FindDefinition(TEXT("Service"));
		if (!WaterDefinition)
			return false;
		if (!Place(TEXT("CoalPower"), 1, 9) || !Place(TEXT("Service"), 4, 9) ||
		    !Place(TEXT("Service"), 25, 8 - WaterDefinition->Footprint.Height, 2) ||
		    !Place(TEXT("College"), 8, 9) || !Place(TEXT("Factory"), 24, 9) ||
		    !Place(TEXT("Factory"), 27, 9) || !Place(TEXT("Shop"), 30, 9) || !Place(TEXT("Shop"), 33, 9) ||
		    !Place(TEXT("Shop"), 36, 9))
			return false;
		for (int32 X = 1; X <= 22; X += 3)
			if (!Place(TEXT("Home"), X, 6, 2))
				return false;
        if (AiWorkforce)
        {
            auto ExtraRoad = City.BeginRoadPreview({40,8});
            City.UpdateRoadPreviewEnd(ExtraRoad,{55,8});
            if (!City.CommitRoad(ExtraRoad).IsSuccess()) return false;
            for (int32 X=40; X<=49; X+=3)
                if (!Place(TEXT("Home"),X,6,2)) return false;
        }
		Sim->Development.InitializeExistingAsOperational(City, Catalog, 0);
		Sim->EconomyTuning.FixedStepSeconds = .125;
		Sim->EconomyTuning.FixedStepsPerMonth = AuthoredCalendar ? FCityCalendar::DefaultStepsPerDay : 4;
		Sim->EconomyTuning.MaxCatchUpSteps = 32;
		Sim->EconomyTuning.CrimePopulationThreshold = Sim->EconomyTuning.FirePopulationThreshold = 100000;
		Sim->StartScenario();
		Sim->SetSimulationSpeed(1);
		for (int32 I = 0; I < 3; ++I)
			Month(Sim);
		auto People = Sim->Population.GetSnapshot();
		for (auto& C : People.Citizens)
			C.Education = ECityEducationLevel::Higher;
		FString Error;
		if (!Sim->Population.RestoreState(People, Error))
        { UE_LOG(LogTemp, Error, TEXT("CITYSIM_AI_SEED_RESTORE %s"), *Error); return false; }
		Sim->Population.MarkExistingBusinessesOpenForScenario();
		Month(Sim);
		++Sim->PresentationGeneration;
		Sim->RefreshDerivedSnapshots();
		if (Board)
			Board->PrepareCityView();
		return !People.Citizens.IsEmpty() &&
		       Sim->GetServiceSnapshot().OperatingFacilities.FindRef(TEXT("College")) > 0;
	}
	static bool Place(UCitySimulationSubsystem* Sim, const FName Id, const int32 X)
	{
		const auto* D = Sim->BuildingCatalog->FindDefinition(Id);
		if (!D)
			return false;
		auto& City = Sim->GetPlacementModel();
		auto Preview = City.BeginPreview(*D, {X, 9});
		const auto Result = City.CommitPlacement(Preview, *D, ECityConstructionFunding::CityTreasury,
		                                         &Sim->GetServiceSnapshot());
		if (!Result.IsSuccess())
        { UE_LOG(LogTemp, Error, TEXT("CITYSIM_TECH_PLACE_FAILED %s failure%d"), *Id.ToString(),static_cast<int32>(Result.Failure)); return false; }
		Sim->Development.Reconcile(City, Sim->BuildingCatalog);
		// Run real construction/settlement/hiring steps. Qualified existing residents can take the new roles.
		for (int32 I = 0; I < 4; ++I)
			Month(Sim);
		const uint64 Entity = Find(Sim, Id);
		UE_LOG(LogTemp, Display,
		       TEXT("CITYSIM_TECHNOLOGY_FIXTURE Id=%s Failure=%s Staff=%d Power=%d/%d Water=%d/%d"),
		       *Id.ToString(),
		       *FCityServiceEvaluator::FailureToString(Sim->GetServiceSnapshot().GetFailure(Entity)),
		       Sim->GetServiceSnapshot().PublicStaffPercent.FindRef(Entity),
		       Sim->GetServiceSnapshot().PowerUsed, Sim->GetServiceSnapshot().PowerCapacity,
		       Sim->GetServiceSnapshot().Water.EntityDelivered.FindRef(Entity),
		       Sim->GetServiceSnapshot().Water.EntityDemand.FindRef(Entity));
		return Sim->GetServiceSnapshot().OperatingFacilities.FindRef(Id) > 0;
	}
    static bool FundAiScenario(UCitySimulationSubsystem* Sim, bool PayrollShortage = false)
    {
        auto People = Sim->Population.GetSnapshot();
        for (auto& B : People.Businesses)
        {
            if (!B.bIndustrial) B.Balance = PayrollShortage ? 0 : 5000;
            if (PayrollShortage && !B.bIndustrial) B.PayrollPaid = 0;
        }
        FString Error;
        if (!Sim->Population.RestoreState(People, Error)) return false;
        ++Sim->PresentationGeneration;
        Sim->RefreshDerivedSnapshots();
        return true;
    }
    static void RefreshAi(UCitySimulationSubsystem* Sim)
    { ++Sim->PresentationGeneration; Sim->RefreshDerivedSnapshots(); }
    static bool SeedAi(UCitySimulationSubsystem* Sim, ACityBoardActor* Board = nullptr, bool AuthoredCalendar = false)
    {
        if (!Seed(Sim, Board, true, AuthoredCalendar)) { UE_LOG(LogTemp, Error, TEXT("CITYSIM_AI_SEED Foundation failed College=%d"), Sim->GetServiceSnapshot().OperatingFacilities.FindRef(TEXT("College"))); return false; }
        RefreshAi(Sim);
        if (!Place(Sim, TEXT("DigitalExchange"),12) || !Place(Sim,TEXT("ComputingCenter"),16) ||
            !Place(Sim,TEXT("ResearchCampus"),20) || !Place(Sim,TEXT("AIInstitute"),42) ||
            !Place(Sim,TEXT("EnterpriseAICenter"),46)) return false;
        if (Board) Board->PrepareCityView();
        return true;
    }
	static uint64 Find(UCitySimulationSubsystem* Sim, const FName Id)
	{
		for (const auto& E : Sim->GetPlacementModel().GetEntities())
			if (E.Value.DefinitionId == Id)
				return E.Key;
		return 0;
	}
};
