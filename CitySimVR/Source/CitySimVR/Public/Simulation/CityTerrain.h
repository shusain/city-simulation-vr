#pragma once

#include "CoreMinimal.h"
#include "Simulation/CityGrid.h"

enum class ECityTerrainSlope : uint8
{
	Flat,
	Gentle,
	Steep
};

enum class ECityWaterClass : uint8
{
	Land,
	Shoreline,
	ShallowWater,
	DeepWater
};

struct CITYSIMVR_API FCityTerrainGenerationParameters
{
	FCityGridDimensions Dimensions;
	// Frozen revisions: 0 original, 1 rounded variety, 2 organic lakes/rugged clusters.
	int32 GeneratorVersion = 0;
	// Independent rules preserve existing geometry and established-city services.
	int32 WaterRulesVersion = 0;
	int32 SceneryVersion = 0;
	int32 Seed = 7429;
	int32 BroadElevationAmplitudeCm = 500;
	int32 HillAmplitudeCm = 650;
	int32 RidgeAmplitudeCm = 450;
	int32 SmoothingPasses = 4;
	int32 QuantizationCm = 10;
	int32 SeaLevelCm = -800;
	int32 StarterHalfWidthCells = 6;
	int32 StarterDepthCells = 10;

	bool IsValid(FString* OutError = nullptr) const;
	bool operator==(const FCityTerrainGenerationParameters& Other) const = default;
	FCityCellCoord GetEntranceCell() const;
	int32 GetMapRotation() const;
	FString DescribeLandscape() const;
};

// Deterministic terrain authority owned by one city's dimensions.
class CITYSIMVR_API FCityTerrainHeightfield
{
public:
	// Terrain presentation/query samples are finer than the authoritative city
	// grid. Static sizes remain the default for unmigrated presentation callers.
	static constexpr int32 SamplesPerCell = 2;
	static constexpr int32 SampleWidth = FCityGrid::Width * SamplesPerCell + 1;
	static constexpr int32 SampleHeight = FCityGrid::Height * SamplesPerCell + 1;
	static constexpr int32 SampleCount = SampleWidth * SampleHeight;
	const FCityGridDimensions& GetDimensions() const
	{
		return Parameters.Dimensions;
	}
	int32 GetSampleWidth() const
	{
		return GetDimensions().GetWidth() * SamplesPerCell + 1;
	}
	int32 GetSampleHeight() const
	{
		return GetDimensions().GetHeight() * SamplesPerCell + 1;
	}
	int32 GetSampleCount() const
	{
		return GetSampleWidth() * GetSampleHeight();
	}

	bool Generate(const FCityTerrainGenerationParameters& InParameters, FString* OutError = nullptr);
	const FCityTerrainGenerationParameters& GetParameters() const
	{
		return Parameters;
	}
	int32 GetVertexHeightCm(int32 X, int32 Y) const;
	int32 GetSampleHeightCm(int32 SampleX, int32 SampleY) const;
	int32 GetCellCornerHeightCm(const FCityCellCoord& Cell, int32 CornerX, int32 CornerY) const;
	double SampleHeightCm(double CellX, double CellY) const;
	FVector GetSurfaceNormal(double CellX, double CellY, double RadiusCells = 0.5) const;
	double GetCellCenterHeightCm(const FCityCellCoord& Cell) const;
	int32 GetCellMinHeightCm(const FCityCellCoord& Cell) const;
	int32 GetCellMaxHeightCm(const FCityCellCoord& Cell) const;
	ECityTerrainSlope GetCellSlope(const FCityCellCoord& Cell) const;
	ECityWaterClass GetWaterClass(const FCityCellCoord& Cell) const;
	bool IsWaterCell(const FCityCellCoord& Cell) const;
	bool IsCellBuildable(const FCityCellCoord& Cell) const;
	int32 GetWaterSurfaceHeightCm(const FCityCellCoord& Cell) const;
	int32 GetSeaLevelCm() const
	{
		return Parameters.SeaLevelCm;
	}
	int32 GetGroundwaterYieldPercent(const FCityCellCoord& Cell) const;
	const TArray<ECityWaterClass>& GetWaterClasses() const
	{
		return WaterClasses;
	}
	const TArray<FCityCellCoord>& GetRiverCells() const
	{
		return RiverCells;
	}
	const TArray<int32>& GetQuantizedHeightsCm() const
	{
		return HeightsCm;
	}

private:
	int32 Index(int32 X, int32 Y) const
	{
		return Y * GetSampleWidth() + X;
	}
	int32 CellIndex(const FCityCellCoord& Cell) const
	{
		return Cell.Y * GetDimensions().GetWidth() + Cell.X;
	}
	bool IsValidCell(const FCityCellCoord& Cell) const
	{
		return Cell.X >= 0 && Cell.Y >= 0 && Cell.X < GetDimensions().GetWidth() &&
		       Cell.Y < GetDimensions().GetHeight();
	}
	void GenerateHydrology();
	void GenerateVariedLandscape();
	void AddMountainDetail();
	FCityTerrainGenerationParameters Parameters;
	TArray<int32> HeightsCm;
	TArray<ECityWaterClass> WaterClasses;
	TArray<int32> WaterSurfaceHeightsCm;
	TArray<FCityCellCoord> RiverCells;
};
