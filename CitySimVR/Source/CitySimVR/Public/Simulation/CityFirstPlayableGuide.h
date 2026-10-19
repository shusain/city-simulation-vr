#pragma once

#include "CoreMinimal.h"
#include "Simulation/CityScenarioSimulation.h"

struct CITYSIMVR_API FCityFirstPlayableGuideState
{
	ECityScenarioPhase ScenarioPhase = ECityScenarioPhase::NotStarted;
	int32 Residents = 0;
	int32 Jobs = 0;
	int32 ProfitableMonthStreak = 0;
	bool bHasConnectedEntranceRoad = false;
	bool bHasResidentialZone = false;
	bool bHasCommercialZone = false;
	int32 BuiltPowerBuildings = 0;
	int32 OperationalPowerBuildings = 0;
	int32 BuiltServiceBuildings = 0;
	int32 OperationalServiceBuildings = 0;
	int32 CrimePopulationThreshold = 100;
	int32 CrimeGraceMonths = 2;
	int32 FirePopulationThreshold = 150;
	int32 FireGraceMonths = 2;
	bool bCrimeIntroduced = false;
	int32 OperationalPolice = 0;
	int32 BuiltPolice = 0;
	int64 PoliceCost = 0;
	bool bFireIntroduced = false;
	int32 OperationalFire = 0;
	int32 BuiltFire = 0;
	int32 BurningBuildings = 0;
	int32 DamagedBuildings = 0;
	int64 FireCost = 0;
	int32 OperationalSchools = 0;
	int32 EducationServed = 0;
	int32 SecondaryResidents = 0;
	int32 OperationalLargeParks = 0;
	int32 HospitalCapacity = 0, IllResidents = 0;
	int64 RecoveredResidents = 0;
	int32 WasteFleet = 0;
	int64 WasteCollected = 0, WasteRecycled = 0;
	bool bHasIndustrialZone = false;
	int64 CompletedPurchases = 0;
	int32 PowerShortage = 0;
	int32 WaterShortage = 0;
	int64 UpkeepShortfall = 0;
	int64 Funds = 0;
	int64 PowerCost = 0;
	int64 WaterCost = 0;
	int64 IndustryCost = 0;
};

// Saved teaching history; live recovery hints never erase learned milestones.
struct CITYSIMVR_API FCityGuidedProgress
{
	TArray<FName> CompletedIds;
	FName CurrentId = TEXT("entrance_road");
};
struct CITYSIMVR_API FCityGuidedObjective
{
	FName Id;
	FString Title;
	FString Reason;
	FString Hint;
	FString Progress;
	float Fraction = 0.0f;
};

class CITYSIMVR_API FCityFirstPlayableGuide
{
public:
	static const TArray<FName>& MilestoneIds();
	static void UpdateProgress(const FCityFirstPlayableGuideState& State, FCityGuidedProgress& Progress);
	static bool ValidateProgress(const FCityGuidedProgress& Progress, FString& OutError);
	static FCityGuidedObjective BuildObjective(const FCityFirstPlayableGuideState& State);
	static FCityGuidedObjective BuildObjective(const FCityFirstPlayableGuideState& State,
	                                           const FCityGuidedProgress& Progress);
	static FCityGuidedObjective BuildCompletedObjective(FName Id, int32 Index, int32 Total);
	static FCityGuidedObjective BuildMilestone(FName Id, const FCityFirstPlayableGuideState& State);
	static FString DescribeNextStep(const FCityFirstPlayableGuideState& State);
};
