#pragma once

#include "CoreMinimal.h"
#include "Simulation/CityGrid.h"

class FCityPlacementModel;
struct FCityEconomyTuning;
struct FCityServiceSnapshot;

enum class ECitySeason : uint8
{
	Spring,
	Summer,
	Autumn,
	Winter
};
enum class ECityTemperatureBand : uint8
{
	Cold,
	Mild,
	Hot
};
enum class ECityWindDirection : uint8
{
	North,
	East,
	South,
	West
};

struct CITYSIMVR_API FCityEnvironmentSnapshot
{
	FCityEnvironmentSnapshot();
	explicit FCityEnvironmentSnapshot(FCityGridDimensions InDimensions);
	FCityGridDimensions Dimensions;

	int64 LastFixedStepId = 0;
	TArray<int32> Pollution;
	int32 TotalPollution = 0;
	int32 PeakPollution = 0;
	// Persisted deterministic weather state.  It changes once per simulated month.
	uint32 WeatherRandomState = 0x43545925u;
	int64 NextWeatherChangeStep = 25;
	ECitySeason Season = ECitySeason::Spring;
	ECityTemperatureBand Temperature = ECityTemperatureBand::Mild;
	ECityWindDirection WindDirection = ECityWindDirection::East;
	int32 WindStrength = 1;
	bool bPrecipitation = false;

	int32 GetPollution(const FCityCellCoord& Cell) const;
	int32 GetWaterDemandModifierPercent() const;
	int32 GetDesirabilityModifier() const;
	FString DescribeWeather() const;
	bool IsValid(FString* OutError = nullptr) const;
};

struct CITYSIMVR_API FCityDesirabilityResult
{
	int32 Score = 0;
	int32 PollutionExposure = 0;
	int32 RoadBonus = 0;
	int32 UtilityBonus = 0;
	int32 ParkBonus = 0;
	int32 EducationBonus = 0;
	int32 WeatherModifier = 0;
	int32 PollutionPenalty = 0;
	int32 CrimePenalty = 0;

	bool IsResidentialEligible(int32 MinimumScore) const
	{
		return Score >= MinimumScore;
	}
};

class CITYSIMVR_API FCityEnvironmentSimulation
{
public:
	FCityEnvironmentSimulation();
	explicit FCityEnvironmentSimulation(FCityGridDimensions Dimensions);

	void AdvanceToStep(int64 FixedStepId, const FCityPlacementModel& City, const FCityServiceSnapshot& Services,
	                   const FCityEconomyTuning& Tuning);
	bool RestoreState(const FCityEnvironmentSnapshot& InSnapshot, int64 MaximumFixedStepId, FString& OutError);
	void Reset(int64 FixedStepId = 0);
	const FCityEnvironmentSnapshot& GetSnapshot() const
	{
		return Snapshot;
	}

	static FCityDesirabilityResult EvaluateResidentialDesirability(const FCityPlacementModel& City,
	                                                               const FCityServiceSnapshot& Services,
	                                                               const FCityEnvironmentSnapshot& Environment,
	                                                               const FCityCellCoord& Anchor,
	                                                               const FIntPoint& FootprintSize,
	                                                               const FCityEconomyTuning& Tuning);

private:
	void Step(const FCityPlacementModel& City, const FCityServiceSnapshot& Services, const FCityEconomyTuning& Tuning);
	void RefreshAggregates();
	void AdvanceWeather(const FCityEconomyTuning& Tuning);
	uint32 NextWeatherRandom();

	FCityEnvironmentSnapshot Snapshot;
};
