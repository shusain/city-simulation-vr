#pragma once
#include "CoreMinimal.h"
#include "Simulation/CityGrid.h"
struct FCityPopulationSnapshot;

enum class ECityHousingEventKind : uint8
{
	Searching,
	Waiting,
	Recovered,
	Relocated,
	Departed
};
struct CITYSIMVR_API FCityHousingEvent
{
	uint64 Id = 0, SourceHome = 0, DestinationHome = 0;
	int64 Month = 0, Money = 0;
	ECityHousingEventKind Kind = ECityHousingEventKind::Searching;
	uint8 HousingStatus = 0;
	FCityCellCoord SourceCell{-1, -1}, DestinationCell{-1, -1};
	int32 Residents = 0, Happiness = 0, Outlook = 0, DestinationOutlook = 0, Concerns = 0;
	TArray<uint64> CitizenIds;
};
class CITYSIMVR_API FCityHousingHistory
{
public:
	static constexpr int32 MaximumEvents = 32;
	static constexpr int32 MaximumCitizenIds = 128;
	static constexpr int32 EventsPerPage = 2;
	static void Record(FCityPopulationSnapshot& Population, FCityHousingEvent Event);
	static bool Validate(const FCityPopulationSnapshot& Population, FString& Error,
	                     const FCityGridDimensions* Dimensions = nullptr, uint64 NextHomeId = MAX_uint64);
	static FString Describe(const FCityHousingEvent& Event);
	static FString Recent(const FCityPopulationSnapshot& Population, int32 Page = 0, int32 PerPage = EventsPerPage);
	static FString Warnings(const FCityPopulationSnapshot& Population);
	static bool IsSearching(uint8 Status);
};
