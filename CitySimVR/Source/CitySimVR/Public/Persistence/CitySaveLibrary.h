#pragma once
#include "Persistence/CityPersistence.h"

struct CITYSIMVR_API FCitySaveSlot
{
	FCityGridDimensions Dimensions;
	int32 GridRulesVersion = FCitySaveDocument::CurrentGridRulesVersion;
	FCityIdentity Identity;
	FString Path;
	FDateTime LastSaved;
	bool bBackupOnly = false;
	bool bArchived = false;
	bool bOriginalSingleFile = false;
	bool bSupported = true;
	int32 SchemaVersion = FCitySaveDocument::CurrentSchemaVersion;
	FString LastSavedGameRevision = FCityGameRevision::Current();
	FString UnsupportedReason;
	FCityTerrainGenerationParameters Terrain;
	int64 Funds = 0;
	int64 MonthId = 0;
	int32 Residents = 0;
	int32 ReportMonths = 0;
};
class CITYSIMVR_API FCitySaveLibrary
{
  public:
	static constexpr int32 MaxCities = 32;
	static FString Directory();
	static FString ArchiveDirectory(const FString& Directory);
	static FString IndexPath(const FString& Directory);
	static FString PathFor(const FString& Directory, const FString& Id);
	// Documents are authoritative. The bounded index is an atomically rebuilt catalog cache.
	static TArray<FCitySaveSlot> List(const FString& Directory, const FString& OriginalPath = FString(),
	                                  bool bArchived = false);
	static int32 Count(const FString& Directory);
	static bool RefreshIndex(const FString& Directory, FString& Error, bool bSimulateInterruption = false);
	static bool SetArchived(const FString& Directory, const FCitySaveSlot& Slot, bool bArchived,
	                        FString& Error);
};
