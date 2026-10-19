#pragma once

#include "Simulation/CityGrid.h"

// Derived, city-owned geometry only. Never serialized or shared between cities.
struct FCityFrontageKey
{
	FCityCellCoord Anchor;
	FIntPoint Size;
	bool bConnectedOnly = false;
	bool bBuildingFrontageOnly = false;
	bool operator==(const FCityFrontageKey& Other) const
	{
		return Anchor == Other.Anchor && Size == Other.Size && bConnectedOnly == Other.bConnectedOnly &&
		       bBuildingFrontageOnly == Other.bBuildingFrontageOnly;
	}
	friend uint32 GetTypeHash(const FCityFrontageKey& Key)
	{
		return HashCombine(HashCombine(GetTypeHash(Key.Anchor), GetTypeHash(Key.Size)),
		                   (Key.bConnectedOnly ? 1u : 0u) | (Key.bBuildingFrontageOnly ? 2u : 0u));
	}
};

struct FCityRoadReachKey
{
	TArray<FCityCellCoord> Starts;
	int32 HopBudget = 0;
	bool bConnectedOnly = false;
	bool operator==(const FCityRoadReachKey& Other) const
	{
		return Starts == Other.Starts && HopBudget == Other.HopBudget && bConnectedOnly == Other.bConnectedOnly;
	}
	friend uint32 GetTypeHash(const FCityRoadReachKey& Key)
	{
		uint32 Hash = HashCombine(GetTypeHash(Key.HopBudget), GetTypeHash(Key.bConnectedOnly));
		for (const auto& Cell : Key.Starts)
			Hash = HashCombine(Hash, GetTypeHash(Cell));
		return Hash;
	}
};

struct FCitySpatialCacheStats
{
	uint64 Invalidations = 0;
	uint64 FrontageBuilds = 0;
	uint64 FrontageHits = 0;
	uint64 ReachBuilds = 0;
	uint64 ReachHits = 0;
};

struct FCitySpatialCache
{
	TMap<FCityFrontageKey, TArray<FCityCellCoord>> Frontages;
	TMap<FCityRoadReachKey, TMap<FCityCellCoord, int32>> Reaches;
	int32 CachedDistances = 0;
	bool bEnabled = true;
	FCitySpatialCacheStats Stats;
	void Invalidate()
	{
		Frontages.Reset();
		Reaches.Reset();
		CachedDistances = 0;
		++Stats.Invalidations;
	}
};
