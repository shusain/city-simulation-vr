#include "Presentation/CityScenery.h"

uint32 FCitySceneryField::Hash(const uint32 X, const uint32 Y, const uint32 Seed)
{
	uint32 Value = X * 0x8da6b343u ^ Y * 0xd8163841u ^ Seed;
	Value ^= Value >> 13;
	Value *= 0x85ebca6bu;
	Value ^= Value >> 16;
	return Value;
}

int32 FCitySceneryField::ClimateWarmthPercent(const int32 Seed)
{
	return 30 + static_cast<int32>(Hash(0, 0, static_cast<uint32>(Seed) ^ 0x4c11ab1eu) % 3) * 25;
}

FString FCitySceneryField::ClimateDescription(const int32 Seed)
{
	const int32 Warmth = ClimateWarmthPercent(Seed);
	return Warmth == 30 ? TEXT("cool climate") : Warmth == 55 ? TEXT("temperate climate") : TEXT("warm climate");
}

void FCitySceneryField::Build(const FCityTerrainHeightfield& Terrain)
{
	Dimensions = Terrain.GetDimensions();
	Samples.SetNum(Dimensions.GetCellCount());
	TArray<int32> MountainDistance, ShoreDistance;
	MountainDistance.Init(Dimensions.GetWidth() + Dimensions.GetHeight(), Dimensions.GetCellCount());
	ShoreDistance = MountainDistance;
	TArray<FCityCellCoord> MountainFront, ShoreFront;
	for (int32 Y = 0; Y < Dimensions.GetHeight(); ++Y)
		for (int32 X = 0; X < Dimensions.GetWidth(); ++X)
		{
			const FCityCellCoord Cell(X, Y);
			const int32 Index = Y * Dimensions.GetWidth() + X;
			auto& Sample = Samples[Index];
			Sample.ElevationCm = FMath::RoundToInt32(Terrain.GetCellCenterHeightCm(Cell));
			Sample.Slope = Terrain.GetCellSlope(Cell);
			if (Sample.ElevationCm >= 1300 && Terrain.GetWaterClass(Cell) == ECityWaterClass::Land)
			{
				MountainDistance[Index] = 0;
				MountainFront.Add(Cell);
			}
			if (Terrain.GetWaterClass(Cell) != ECityWaterClass::Land)
			{
				ShoreDistance[Index] = 0;
				ShoreFront.Add(Cell);
			}
		}
	auto Flood = [this](TArray<FCityCellCoord>& Front, TArray<int32>& Distances)
	{
		for (int32 Index = 0; Index < Front.Num(); ++Index)
		{
			const auto Cell = Front[Index];
			const int32 Distance = Distances[Cell.Y * Dimensions.GetWidth() + Cell.X] + 1;
			for (const FCityCellCoord Offset :
			     {FCityCellCoord(1, 0), FCityCellCoord(-1, 0), FCityCellCoord(0, 1), FCityCellCoord(0, -1)})
			{
				const FCityCellCoord Neighbor(Cell.X + Offset.X, Cell.Y + Offset.Y);
				if (Neighbor.X < 0 || Neighbor.Y < 0 || Neighbor.X >= Dimensions.GetWidth() ||
				    Neighbor.Y >= Dimensions.GetHeight())
					continue;
				auto& Existing = Distances[Neighbor.Y * Dimensions.GetWidth() + Neighbor.X];
				if (Existing > Distance)
				{
					Existing = Distance;
					Front.Add(Neighbor);
				}
			}
		}
	};
	Flood(MountainFront, MountainDistance);
	Flood(ShoreFront, ShoreDistance);
	const uint32 Seed = static_cast<uint32>(Terrain.GetParameters().Seed);
	for (int32 Y = 0; Y < Dimensions.GetHeight(); ++Y)
		for (int32 X = 0; X < Dimensions.GetWidth(); ++X)
		{
			const int32 Index = Y * Dimensions.GetWidth() + X;
			auto& Sample = Samples[Index];
			// Bilinear regional variation gives coherent climate patches, rather than
			// adjacent random temperatures. Altitude cools even a warm coastal city.
			auto Noise = [Seed](int32 Nx, int32 Ny)
			{ return static_cast<double>(Hash(Nx, Ny, Seed ^ 0x937ace11u) % 25) - 12; };
			const double Fx = (X % 16) / 16.0, Fy = (Y % 16) / 16.0;
			const double Variation =
			    FMath::Lerp(FMath::Lerp(Noise(X / 16, Y / 16), Noise(X / 16 + 1, Y / 16), Fx),
			                FMath::Lerp(Noise(X / 16, Y / 16 + 1), Noise(X / 16 + 1, Y / 16 + 1), Fx), Fy);
			Sample.WarmthPercent =
			    FMath::Clamp(ClimateWarmthPercent(Terrain.GetParameters().Seed) + FMath::RoundToInt32(Variation) -
			                     FMath::Max(0, Sample.ElevationCm) / 100,
			                 0, 100);
			Sample.MountainAffinityPercent = FMath::Clamp(100 - MountainDistance[Index] * 16, 0, 100);
			Sample.ShoreDistanceCells = ShoreDistance[Index];
		}
}

const FCityScenerySample& FCitySceneryField::GetSample(const FCityCellCoord& Cell) const
{
	check(Cell.X >= 0 && Cell.Y >= 0 && Cell.X < Dimensions.GetWidth() && Cell.Y < Dimensions.GetHeight());
	return Samples[Cell.Y * Dimensions.GetWidth() + Cell.X];
}

ECitySceneryKind FCitySceneryField::SelectKind(const FCityScenerySample& Sample, const uint32 Choice)
{
	if (Sample.Slope == ECityTerrainSlope::Steep || Sample.ElevationCm > 1300)
		return ECitySceneryKind::Rock;
	const int32 PineWeight = 5 + Sample.MountainAffinityPercent * 2 + FMath::Max(0, 45 - Sample.WarmthPercent);
	const int32 BroadleafWeight = FMath::Max(10, 100 - Sample.MountainAffinityPercent);
	const int32 PalmWeight =
	    Sample.WarmthPercent >= 65 && Sample.ShoreDistanceCells <= 3 && Sample.ElevationCm <= 1000 ? 180 : 0;
	const int32 Pick = static_cast<int32>(Choice % static_cast<uint32>(PineWeight + BroadleafWeight + PalmWeight));
	return Pick < PineWeight                     ? ECitySceneryKind::Conifer
	       : Pick < PineWeight + BroadleafWeight ? ECitySceneryKind::Broadleaf
	                                             : ECitySceneryKind::Palm;
}

TArray<FCitySceneryCandidate> FCitySceneryField::SelectCandidates(const FCityTerrainHeightfield& Terrain,
                                                                  const FCityCellCoord& Entrance, const int32 MaxTrees,
                                                                  const int32 MaxRocks)
{
	FCitySceneryField Field;
	const auto Dimensions = Terrain.GetDimensions();
	const bool bLegacy = Terrain.GetParameters().SceneryVersion == 0;
	if (!bLegacy)
		Field.Build(Terrain);
	const uint32 Seed = static_cast<uint32>(Terrain.GetParameters().Seed);
	TArray<FCitySceneryCandidate> Candidates;
	for (int32 Y = 1; Y < Dimensions.GetHeight() - 1; ++Y)
		for (int32 X = 1; X < Dimensions.GetWidth() - 1; ++X)
		{
			const FCityCellCoord Cell(X, Y);
			if (Terrain.GetWaterClass(Cell) != ECityWaterClass::Land ||
			    FMath::Abs(X - Entrance.X) + FMath::Abs(Y - Entrance.Y) < 14)
				continue;
			const uint32 Key = Hash(X, Y, Seed), Patch = Hash(X / 7, Y / 7, Seed ^ 0x71aa42u);
			const bool bRock =
			    Terrain.GetCellSlope(Cell) == ECityTerrainSlope::Steep || Terrain.GetCellCenterHeightCm(Cell) > 1300;
			if (bRock ? Key % 9 == 0 : Key % 100 < (Patch % 3 == 0 ? 26u : 5u))
				Candidates.Add({Cell, Key,
				                bRock     ? ECitySceneryKind::Rock
				                : bLegacy ? (Key % 3 == 0 ? ECitySceneryKind::Conifer : ECitySceneryKind::Broadleaf)
				                          : SelectKind(Field.GetSample(Cell), Hash(X, Y, Seed ^ 0x834df47au))});
		}
	Candidates.Sort(
	    [](const auto& A, const auto& B)
	    {
		    return A.Hash != B.Hash       ? A.Hash < B.Hash
		           : A.Cell.Y != B.Cell.Y ? A.Cell.Y < B.Cell.Y
		                                  : A.Cell.X < B.Cell.X;
	    });
	int32 Trees = 0, Rocks = 0;
	Candidates.RemoveAll(
	    [&](const auto& Candidate)
	    { return Candidate.Kind == ECitySceneryKind::Rock ? ++Rocks > MaxRocks : ++Trees > MaxTrees; });
	return Candidates;
}
