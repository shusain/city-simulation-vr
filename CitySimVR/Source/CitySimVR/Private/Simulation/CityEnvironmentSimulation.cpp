#include "Simulation/CityEnvironmentSimulation.h"
#include "Simulation/CityCalendar.h"
#include "Simulation/CityParkSimulation.h"

#include "Commands/CityPlacementModel.h"
#include "Simulation/CityEconomySimulation.h"
#include "Simulation/CityGrowthSimulation.h"

namespace
{
	int32 EnvironmentCellIndex(const FCityCellCoord& Cell, const FCityGridDimensions& Dimensions)
	{
		return Cell.Y * Dimensions.GetWidth() + Cell.X;
	}

	void AddBounded(TArray<int32>& Values, const FCityGridDimensions& Dimensions, const FCityCellCoord& Cell,
	                const int32 Amount, const int32 Maximum)
	{
		if (Cell.X < 0 || Cell.Y < 0 || Cell.X >= Dimensions.GetWidth() || Cell.Y >= Dimensions.GetHeight())
		{
			return;
		}
		int32& Value = Values[EnvironmentCellIndex(Cell, Dimensions)];
		Value = static_cast<int32>(FMath::Min<int64>(Maximum, static_cast<int64>(Value) + FMath::Max(0, Amount)));
	}

	FIntPoint WindOffset(const ECityWindDirection Direction)
	{
		switch (Direction)
		{
		case ECityWindDirection::North:
			return FIntPoint(0, 1);
		case ECityWindDirection::South:
			return FIntPoint(0, -1);
		case ECityWindDirection::West:
			return FIntPoint(-1, 0);
		default:
			return FIntPoint(1, 0);
		}
	}
} // namespace

FCityEnvironmentSimulation::FCityEnvironmentSimulation() : FCityEnvironmentSimulation(FCityGridDimensions()) {}

FCityEnvironmentSimulation::FCityEnvironmentSimulation(const FCityGridDimensions Dimensions) : Snapshot(Dimensions)
{
	Reset();
}

FCityEnvironmentSnapshot::FCityEnvironmentSnapshot() : FCityEnvironmentSnapshot(FCityGridDimensions()) {}

FCityEnvironmentSnapshot::FCityEnvironmentSnapshot(const FCityGridDimensions InDimensions) : Dimensions(InDimensions)
{
	checkf(Dimensions.IsValid(), TEXT("Validate pollution dimensions before allocation"));
	Pollution.Init(0, Dimensions.GetCellCount());
}

int32 FCityEnvironmentSnapshot::GetPollution(const FCityCellCoord& Cell) const
{
	const int32 Count = Dimensions.GetCellCount();
	return Count > 0 && Cell.X >= 0 && Cell.Y >= 0 && Cell.X < Dimensions.GetWidth() &&
	               Cell.Y < Dimensions.GetHeight() && Pollution.Num() == Count
	           ? Pollution[EnvironmentCellIndex(Cell, Dimensions)]
	           : 0;
}

int32 FCityEnvironmentSnapshot::GetWaterDemandModifierPercent() const
{
	// Heat increases use; cold and rain remain deliberately small, bounded effects.
	return FMath::Clamp(100 +
	                        (Temperature == ECityTemperatureBand::Hot    ? 10
	                         : Temperature == ECityTemperatureBand::Cold ? 5
	                                                                     : 0) -
	                        (bPrecipitation ? 5 : 0),
	                    90, 110);
}

int32 FCityEnvironmentSnapshot::GetDesirabilityModifier() const
{
	return FMath::Clamp((Temperature == ECityTemperatureBand::Mild  ? 3
	                     : Temperature == ECityTemperatureBand::Hot ? -2
	                                                                : -3) +
	                        (bPrecipitation ? -1 : 0),
	                    -5, 3);
}

FString FCityEnvironmentSnapshot::DescribeWeather() const
{
	static const TCHAR* Seasons[] = {TEXT("SPRING"), TEXT("SUMMER"), TEXT("AUTUMN"), TEXT("WINTER")};
	static const TCHAR* Temperatures[] = {TEXT("COLD"), TEXT("MILD"), TEXT("HOT")};
	static const TCHAR* Winds[] = {TEXT("N"), TEXT("E"), TEXT("S"), TEXT("W")};
	const TCHAR* Precipitation = !bPrecipitation ? TEXT("")
	                             : Season == ECitySeason::Winter && Temperature == ECityTemperatureBand::Cold
	                                 ? (WindStrength == 2 ? TEXT(" SNOWSTORM") : TEXT(" SNOW"))
	                             : WindStrength == 2 ? TEXT(" RAINSTORM")
	                                                 : TEXT(" RAIN");
	return FString::Printf(TEXT("%s  %s%s  WIND %s%d"), Seasons[static_cast<uint8>(Season)],
	                       Temperatures[static_cast<uint8>(Temperature)], Precipitation,
	                       Winds[static_cast<uint8>(WindDirection)], WindStrength);
}

bool FCityEnvironmentSnapshot::IsValid(FString* OutError) const
{
	bool bValid = Dimensions.IsValid() && LastFixedStepId >= 0 && Pollution.Num() == Dimensions.GetCellCount() &&
	              WeatherRandomState != 0 && NextWeatherChangeStep >= LastFixedStepId &&
	              static_cast<uint8>(Season) <= static_cast<uint8>(ECitySeason::Winter) &&
	              static_cast<uint8>(Temperature) <= static_cast<uint8>(ECityTemperatureBand::Hot) &&
	              static_cast<uint8>(WindDirection) <= static_cast<uint8>(ECityWindDirection::West) &&
	              WindStrength >= 0 && WindStrength <= 2;
	int64 Total = 0;
	int32 Peak = 0;
	for (const int32 Value : Pollution)
	{
		bValid &= Value >= 0 && Value <= 1000;
		Total += FMath::Max(0, Value);
		Peak = FMath::Max(Peak, Value);
	}
	bValid &= TotalPollution == FMath::Min<int64>(MAX_int32, Total) && PeakPollution == Peak;
	if (!bValid && OutError)
	{
		*OutError = TEXT("Invalid saved pollution field");
	}
	return bValid;
}

void FCityEnvironmentSimulation::Reset(const int64 FixedStepId)
{
	Snapshot = FCityEnvironmentSnapshot(Snapshot.Dimensions);
	Snapshot.LastFixedStepId = FMath::Max<int64>(0, FixedStepId);
	Snapshot.NextWeatherChangeStep = ((Snapshot.LastFixedStepId / 25) + 1) * 25;
}

void FCityEnvironmentSimulation::AdvanceToStep(const int64 FixedStepId, const FCityPlacementModel& City,
                                               const FCityServiceSnapshot& Services, const FCityEconomyTuning& Tuning)
{
	checkf(Snapshot.Dimensions == City.GetGrid().GetDimensions(), TEXT("Environment and city dimensions must match"));
	while (Snapshot.LastFixedStepId < FixedStepId)
	{
		if (Snapshot.LastFixedStepId >= Snapshot.NextWeatherChangeStep)
		{
			AdvanceWeather(Tuning);
		}
		Step(City, Services, Tuning);
		++Snapshot.LastFixedStepId;
	}
}

void FCityEnvironmentSimulation::Step(const FCityPlacementModel& City, const FCityServiceSnapshot& Services,
                                      const FCityEconomyTuning& Tuning)
{
	const auto& Dimensions = Snapshot.Dimensions;
	TArray<int32> Decayed;
	Decayed.SetNumUninitialized(Dimensions.GetCellCount());
	for (int32 Index = 0; Index < Snapshot.Pollution.Num(); ++Index)
	{
		Decayed[Index] =
		    FMath::Max(0, Snapshot.Pollution[Index] - Tuning.PollutionDecayPerStep - (Snapshot.bPrecipitation ? 1 : 0));
	}

	TArray<int32> Next;
	Next.Init(0, Dimensions.GetCellCount());
	static const FIntPoint NeighborOffsets[] = {FIntPoint(0, 1), FIntPoint(1, 0), FIntPoint(0, -1), FIntPoint(-1, 0)};
	for (int32 Y = 0; Y < Dimensions.GetHeight(); ++Y)
	{
		for (int32 X = 0; X < Dimensions.GetWidth(); ++X)
		{
			const FCityCellCoord Cell(X, Y);
			const int32 Value = Decayed[EnvironmentCellIndex(Cell, Dimensions)];
			const int32 Share = Value / FMath::Max(8, Tuning.PollutionSpreadDivisor);
			const FIntPoint Downwind = WindOffset(Snapshot.WindDirection);
			const int32 WindBonus = FMath::Min(Value, (Share * Snapshot.WindStrength) / 2);
			int32 ValidNeighbors = 0;
			for (const FIntPoint& Offset : NeighborOffsets)
			{
				const FCityCellCoord Neighbor(X + Offset.X, Y + Offset.Y);
				if (Neighbor.X >= 0 && Neighbor.Y >= 0 && Neighbor.X < Dimensions.GetWidth() &&
				    Neighbor.Y < Dimensions.GetHeight())
				{
					++ValidNeighbors;
					AddBounded(Next, Dimensions, Neighbor, Share, Tuning.MaximumPollutionPerCell);
					if (Offset == Downwind)
					{
						AddBounded(Next, Dimensions, Neighbor, WindBonus, Tuning.MaximumPollutionPerCell);
					}
				}
			}
			AddBounded(Next, Dimensions, Cell, FMath::Max(0, Value - Share * ValidNeighbors - WindBonus),
			           Tuning.MaximumPollutionPerCell);
		}
	}

	TArray<uint64> EntityIds;
	City.GetEntities().GetKeys(EntityIds);
	EntityIds.Sort();
	for (const uint64 EntityId : EntityIds)
	{
		const FCityPlacedEntity& Entity = City.GetEntities().FindChecked(EntityId);
		if (Entity.DefinitionId != Tuning.IndustrialDefinitionId ||
		    Services.GetFailure(EntityId) != ECityOperationFailure::None)
		{
			continue;
		}
		const int32 CellCount = FMath::Max(1, Entity.FootprintSize.X * Entity.FootprintSize.Y);
		const int32 Emission = Services.EntityPollutionEmissions.FindRef(EntityId);
		const int32 PerCellEmission = FMath::Max(1, Emission / CellCount);
		for (int32 Y = Entity.Anchor.Y; Y < Entity.Anchor.Y + Entity.FootprintSize.Y; ++Y)
		{
			for (int32 X = Entity.Anchor.X; X < Entity.Anchor.X + Entity.FootprintSize.X; ++X)
			{
				AddBounded(Next, Dimensions, FCityCellCoord(X, Y), PerCellEmission, Tuning.MaximumPollutionPerCell);
			}
		}
	}
	Snapshot.Pollution = MoveTemp(Next);
	RefreshAggregates();
}

uint32 FCityEnvironmentSimulation::NextWeatherRandom()
{
	Snapshot.WeatherRandomState = Snapshot.WeatherRandomState * 1664525u + 1013904223u;
	return Snapshot.WeatherRandomState;
}

void FCityEnvironmentSimulation::AdvanceWeather(const FCityEconomyTuning& Tuning)
{
	const int32 StepsPerMonth = FMath::Max(1, Tuning.FixedStepsPerMonth);
	const int64 MonthIndex = FCityCalendar::MonthIndex(Snapshot.LastFixedStepId, StepsPerMonth);
	Snapshot.Season = static_cast<ECitySeason>((MonthIndex / 3) % 4);
	const uint32 Random = NextWeatherRandom();
	const uint32 TemperatureRoll = (Random >> 4) % 10;
	Snapshot.Temperature = Snapshot.Season == ECitySeason::Winter
	                           ? (TemperatureRoll < 8 ? ECityTemperatureBand::Cold : ECityTemperatureBand::Mild)
	                       : Snapshot.Season == ECitySeason::Summer
	                           ? (TemperatureRoll < 7 ? ECityTemperatureBand::Hot : ECityTemperatureBand::Mild)
	                       : TemperatureRoll < 2 ? ECityTemperatureBand::Cold
	                       : TemperatureRoll > 8 ? ECityTemperatureBand::Hot
	                                             : ECityTemperatureBand::Mild;
	// Persistent pressure systems: hold or turn by one compass sector, never
	// reverse instantaneously. Existing saved states remain valid starting points.
	const uint32 WindRoll = (Random >> 9) % 10;
	const int32 Turn = WindRoll < 6 ? 0 : WindRoll < 8 ? -1 : 1;
	Snapshot.WindDirection =
	    static_cast<ECityWindDirection>((static_cast<int32>(Snapshot.WindDirection) + Turn + 4) % 4);
	const int32 StrengthTarget = static_cast<int32>((Random >> 14) % 3);
	Snapshot.WindStrength += FMath::Clamp(StrengthTarget - Snapshot.WindStrength, -1, 1);
	const int32 WetChance = Snapshot.Season == ECitySeason::Summer ? 3 : Snapshot.Season == ECitySeason::Spring ? 6 : 5;
	const int32 WetRoll = static_cast<int32>((Random >> 19) % 10);
	Snapshot.bPrecipitation = WetRoll < WetChance + (Snapshot.bPrecipitation ? 1 : 0);
	Snapshot.NextWeatherChangeStep = Snapshot.LastFixedStepId + StepsPerMonth;
}

void FCityEnvironmentSimulation::RefreshAggregates()
{
	int64 Total = 0;
	Snapshot.PeakPollution = 0;
	for (const int32 Value : Snapshot.Pollution)
	{
		Total += Value;
		Snapshot.PeakPollution = FMath::Max(Snapshot.PeakPollution, Value);
	}
	Snapshot.TotalPollution = static_cast<int32>(FMath::Min<int64>(MAX_int32, Total));
}

bool FCityEnvironmentSimulation::RestoreState(const FCityEnvironmentSnapshot& InSnapshot,
                                              const int64 MaximumFixedStepId, FString& OutError)
{
	if (InSnapshot.Dimensions != Snapshot.Dimensions)
	{
		OutError = TEXT("Pollution dimensions differ from this city");
		return false;
	}
	if (!InSnapshot.IsValid(&OutError) || InSnapshot.LastFixedStepId > MaximumFixedStepId)
	{
		if (OutError.IsEmpty())
			OutError = TEXT("Pollution state is ahead of the simulation clock");
		return false;
	}
	Snapshot = InSnapshot;
	return true;
}

FCityDesirabilityResult FCityEnvironmentSimulation::EvaluateResidentialDesirability(
    const FCityPlacementModel& City, const FCityServiceSnapshot& Services, const FCityEnvironmentSnapshot& Environment,
    const FCityCellCoord& Anchor, const FIntPoint& FootprintSize, const FCityEconomyTuning& Tuning)
{
	const auto& Dimensions = City.GetGrid().GetDimensions();
	checkf(Environment.Dimensions == Dimensions && Services.Dimensions == Dimensions,
	       TEXT("Desirability fields must belong to this city"));
	FCityDesirabilityResult Result;
	int64 PollutionTotal = 0;
	int64 CrimeTotal = 0;
	int32 Cells = 0;
	for (int32 Y = Anchor.Y; Y < Anchor.Y + FootprintSize.Y; ++Y)
	{
		for (int32 X = Anchor.X; X < Anchor.X + FootprintSize.X; ++X)
		{
			PollutionTotal += Environment.GetPollution(FCityCellCoord(X, Y));
			const int32 Index = Y * Dimensions.GetWidth() + X;
			if (Services.CrimeRisk.IsValidIndex(Index))
				CrimeTotal += Services.CrimeRisk[Index];
			++Cells;
		}
	}
	Result.PollutionExposure = Cells > 0 ? static_cast<int32>(PollutionTotal / Cells) : 0;
	Result.RoadBonus = City.HasConnectedRoadAccess(Anchor, FootprintSize) ? Tuning.DesirabilityRoadBonus : 0;
	Result.UtilityBonus =
	    FCityServiceEvaluator::IsFootprintReached(City, Services.ServiceReach, Anchor, FootprintSize) &&
	            Services.PowerCapacity > 0
	        ? Tuning.DesirabilityUtilityBonus
	        : 0;
	Result.ParkBonus = FCityParkEvaluator::Benefit(Services.ParkProviders, Anchor, FootprintSize);
	int32 EducationTotal = 0;
	for (int32 Y = Anchor.Y; Y < Anchor.Y + FootprintSize.Y; ++Y)
		for (int32 X = Anchor.X; X < Anchor.X + FootprintSize.X; ++X)
			if (Services.EducationCoverage.IsValidIndex(Y * Dimensions.GetWidth() + X))
				EducationTotal += Services.EducationCoverage[Y * Dimensions.GetWidth() + X];
	Result.EducationBonus =
	    Cells > 0 ? EducationTotal * FMath::Clamp(Tuning.SchoolDesirabilityBonus, 0, 20) / (100 * Cells) : 0;
	Result.PollutionPenalty = Result.PollutionExposure / FMath::Max(1, Tuning.PollutionPerDesirabilityPenalty);
	Result.CrimePenalty = Cells > 0 ? FMath::Min(20, static_cast<int32>(CrimeTotal / Cells) / 5) : 0;
	Result.Score =
	    FMath::Clamp(Tuning.BaseResidentialDesirability + Result.RoadBonus + Result.UtilityBonus + Result.ParkBonus +
	                     Result.EducationBonus - Result.PollutionPenalty - Result.CrimePenalty +
	                     (Result.WeatherModifier = Environment.GetDesirabilityModifier()),
	                 0, 100);
	return Result;
}
