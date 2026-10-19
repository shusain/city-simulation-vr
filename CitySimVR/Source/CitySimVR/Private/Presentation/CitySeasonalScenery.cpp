#include "Presentation/CitySeasonalScenery.h"

ECitySeasonalTreeVariant FCitySeasonalScenery::BroadleafVariant(const int32, const ECitySeason Season)
{
	return Season == ECitySeason::Autumn   ? ECitySeasonalTreeVariant::Autumn
	       : Season == ECitySeason::Winter ? ECitySeasonalTreeVariant::Bare
	                                       : ECitySeasonalTreeVariant::Leafy;
}

bool FCitySeasonalScenery::ConiferSnowMesh(const FCityEnvironmentSnapshot& Weather, const double SnowCover,
                                           const bool bPreviouslySnowy)
{
	// Hysteresis prevents repeated static-mesh uploads near an accumulation boundary.
	// White caps require actual cold winter coverage, rather than the date alone.
	return Weather.Season == ECitySeason::Winter && Weather.Temperature == ECityTemperatureBand::Cold &&
	       FMath::IsFinite(SnowCover) && SnowCover >= (bPreviouslySnowy ? .06 : .12);
}

FLinearColor FCitySeasonalScenery::BroadleafColor(const ECitySeasonalTreeVariant Variant)
{
	return Variant == ECitySeasonalTreeVariant::Autumn ? FLinearColor(.64, .25, .035) : FLinearColor(.12, .32, .07);
}
