#pragma once
#include "CoreMinimal.h"
#include "Simulation/CityEnvironmentSimulation.h"

// One cosmetic weather clock, independent of simulation speed and authority.
struct FCityAtmosphereMotion
{
	double WindYaw = 0.0;
	double WindStrength = 1.0;
	double Wetness = 0.0;
	double SnowCover = 0.0;
	double Precipitation = 0.0;
	double PhaseSeconds = 0.0;
	static double TargetYaw(ECityWindDirection Direction)
	{
		return Direction == ECityWindDirection::North   ? 90.0
		       : Direction == ECityWindDirection::East  ? 0.0
		       : Direction == ECityWindDirection::South ? 270.0
		                                                : 180.0;
	}
	static bool IsSnow(const FCityEnvironmentSnapshot& Weather)
	{
		return Weather.Season == ECitySeason::Winter && Weather.Temperature == ECityTemperatureBand::Cold;
	}
	void Reset(const FCityEnvironmentSnapshot& Weather)
	{
		*this = FCityAtmosphereMotion();
		WindYaw = TargetYaw(Weather.WindDirection);
		WindStrength = Weather.WindStrength;
		Precipitation = Weather.bPrecipitation ? 1.0 : 0.0;
		Wetness = Weather.bPrecipitation && !IsSnow(Weather) ? 1.0 : 0.0;
		SnowCover = Weather.bPrecipitation && IsSnow(Weather) ? 1.0 : 0.0;
	}
	void Advance(double Seconds, const FCityEnvironmentSnapshot& Weather)
	{
		if (!FMath::IsFinite(Seconds) || Seconds <= 0.0)
			return;
		const double Delta = FMath::FindDeltaAngleDegrees(WindYaw, TargetYaw(Weather.WindDirection));
		WindYaw = FRotator::ClampAxis(WindYaw + FMath::Clamp(Delta, -9.0 * Seconds, 9.0 * Seconds));
		const double Ease = 1.0 - FMath::Exp(-Seconds / 3.0);
		WindStrength = FMath::Lerp(WindStrength, static_cast<double>(Weather.WindStrength), Ease);
		Precipitation = FMath::Lerp(Precipitation, Weather.bPrecipitation ? 1.0 : 0.0, Ease);
		Wetness = FMath::Lerp(Wetness, Weather.bPrecipitation && !IsSnow(Weather) ? 1.0 : 0.0,
		                      1.0 - FMath::Exp(-Seconds / 12.0));
		SnowCover = FMath::Lerp(SnowCover, Weather.bPrecipitation && IsSnow(Weather) ? 1.0 : 0.0,
		                        1.0 - FMath::Exp(-Seconds / (IsSnow(Weather) ? 24.0 : 10.0)));
		PhaseSeconds = FMath::Fmod(PhaseSeconds + Seconds, 3600.0);
	}
};
