#pragma once

#include "CoreMinimal.h"
#include "Simulation/CityReportHistory.h"
#include "Simulation/CityHealthSimulation.h"
#include "Simulation/CityWasteSimulation.h"
#include "Simulation/CityIdentity.h"
#include "Commands/CityPlacementModel.h"
#include "Simulation/CityEconomySimulation.h"
#include "Simulation/CityEnvironmentSimulation.h"
#include "Simulation/CityDevelopmentSimulation.h"
#include "Simulation/CityPopulationSimulation.h"
#include "Simulation/CityTrafficSimulation.h"
#include "Simulation/CityScenarioSimulation.h"
#include "Simulation/CityFirstPlayableGuide.h"
#include "Simulation/CityCrimeSimulation.h"
#include "Simulation/CityFireSimulation.h"
#include "Simulation/CityDisasterSimulation.h"
#include "Simulation/CityTerrain.h"

struct CITYSIMVR_API FCityScenarioSaveState
{
	int32 State = 0;
	int32 ProfitableMonthStreak = 0;
	bool bComplete = false;
	int64 LastObservedBilledMonthId = 0;
};

struct CITYSIMVR_API FCitySaveDocument
{
	static constexpr int32 CurrentSchemaVersion = 72;
	// Previous maintained outpatient city; the unmerged Technology-only schema71 is distinct.
	static constexpr int32 PreAiSchemaVersion = 70;
	// Narrow preceding-city transitions preserve pantry cities; writers emit outpatient semantics.
	static constexpr int32 PreOutpatientSchemaVersion = 69;
	static constexpr int32 PrePantrySchemaVersion = 68;
	static constexpr int32 CurrentGridRulesVersion = 1;
	FCitySaveDocument() = default;
	explicit FCitySaveDocument(FCityGridDimensions Dimensions);
	FCityGridDimensions GetDimensions() const
	{
		return FCityGridDimensions(GridWidth, GridHeight);
	}
	bool ValidateDimensions(FString& OutError) const;

	int32 SchemaVersion = CurrentSchemaVersion;
	int32 GridRulesVersion = CurrentGridRulesVersion;
	FCityIdentity Identity;
	FString LastSavedGameRevision = FCityGameRevision::Current();
	FCityEconomyTuning DifficultyRules;
	int32 GridWidth = FCityGrid::Width;
	int32 GridHeight = FCityGrid::Height;
	FCityPlacementModelState Placement;
	FCityEconomySnapshot Economy;
	FCityReportHistory Reports;
	FCityHealthSnapshot Health;
	FCityWasteSnapshot Waste;
	double EconomyAccumulatorSeconds = 0.0;
	bool bSimulationPaused = false;
	int32 SimulationSpeed = 1;
	uint32 GrowthRandomState = 1;
	int64 NextGrowthStep = 0;
	FCityScenarioSaveState Scenario;
	FCityGuidedProgress Guidance;
	FCityCrimeSnapshot Crime;
	FCityFireSnapshot Fire;
	FCityDisasterSnapshot Disasters;
	FCityTerrainGenerationParameters Terrain;
	FCityEnvironmentSnapshot Environment;
	FCityDevelopmentSnapshot Development;
	FCityPopulationSnapshot Population;
	FCityTrafficSnapshot Traffic;
};

enum class ECitySavePlatform : uint8
{
	Any,
	Desktop,
	PCVR,
	Quest
};

enum class ECitySaveReadFailure : uint8
{
	None,
	InvalidData,
	UnsupportedSchema,
	UnsupportedGrid,
	UnsupportedPlatform
};

class CITYSIMVR_API FCityPersistence
{
  public:
	// Any is the bounded portable data format. Live callers must pass their
	// profile; PCVR/Quest stay64 until larger XR maps pass their own gates.
	static bool ValidatePlatform(FCityGridDimensions Dimensions, ECitySavePlatform Platform,
	                             FString& OutError);
	static ECitySavePlatform NativePlatform(bool bIsXR = false);
	static bool Serialize(const FCitySaveDocument& Document, FString& OutJson, FString& OutError);
	static bool Deserialize(const FString& Json, FCitySaveDocument& OutDocument, FString& OutError,
	                        ECitySavePlatform Platform = ECitySavePlatform::Any,
	                        ECitySaveReadFailure* OutFailure = nullptr);
	static bool WriteAtomic(const FString& SavePath, const FString& Json, FString& OutError,
	                        bool bSimulateInterruptionBeforeReplace = false);
	static bool ReadFile(const FString& SavePath, FString& OutJson, FString& OutError);
	static FString GetDefaultSavePath();
};
