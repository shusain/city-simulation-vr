#include "Presentation/CityBeachScenery.h"
#include "Commands/CityPlacementModel.h"
#include "Interaction/CityBoardPicking.h"
#include "Presentation/CityScenery.h"

double FCityBeachScenery::SampleGround(const FCityTerrainHeightfield& Terrain, double X, double Y, FVector& Normal)
{
	constexpr double Samples = FCityTerrainHeightfield::SamplesPerCell;
	constexpr double Spacing = CityBoardPicking::LogicalCellSizeCm / Samples;
	const double SX = FMath::Clamp(X * Samples, 0., static_cast<double>(Terrain.GetSampleWidth() - 1));
	const double SY = FMath::Clamp(Y * Samples, 0., static_cast<double>(Terrain.GetSampleHeight() - 1));
	const int32 IX = FMath::Min(FMath::FloorToInt32(SX), Terrain.GetSampleWidth() - 2);
	const int32 IY = FMath::Min(FMath::FloorToInt32(SY), Terrain.GetSampleHeight() - 2);
	const double TX = SX - IX, TY = SY - IY;
	const double A = Terrain.GetSampleHeightCm(IX, IY), B = Terrain.GetSampleHeightCm(IX + 1, IY);
	const double C = Terrain.GetSampleHeightCm(IX, IY + 1), D = Terrain.GetSampleHeightCm(IX + 1, IY + 1);
	if (TX + TY <= 1.)
	{
		Normal = FVector(A - B, A - C, Spacing).GetSafeNormal();
		return A + TX * (B - A) + TY * (C - A);
	}
	Normal = FVector(C - D, B - D, Spacing).GetSafeNormal();
	return D + (1. - TX) * (C - D) + (1. - TY) * (B - D);
}

TArray<FCityBeachCandidate> FCityBeachScenery::SelectCandidates(const FCityTerrainHeightfield& Terrain)
{
	struct FPatch
	{
		uint32 Hash;
		TArray<FCityBeachCandidate> Props;
	};
	TArray<FPatch> Patches;
	const auto Dimensions = Terrain.GetDimensions();
	const uint32 Seed = static_cast<uint32>(Terrain.GetParameters().Seed) ^ 0xbeac412du;
	constexpr double CellSize = CityBoardPicking::LogicalCellSizeCm;
	for (int32 Y = 1; Y < Dimensions.GetHeight() - 1; ++Y)
		for (int32 X = 1; X < Dimensions.GetWidth() - 1; ++X)
		{
			const uint32 PatchHash = FCitySceneryField::Hash(X, Y, Seed);
			if (PatchHash % 4 != 0)
				continue;
			// Include dry portions of water-edge tiles and the next inland tile.
			// Use the nearest actual lake/river surface, never a sea-level guess.
			int32 Water = 0, Nearest = MAX_int32;
			FIntPoint WaterOffset(0, 0);
			for (int32 DY = -2; DY <= 2; ++DY)
				for (int32 DX = -2; DX <= 2; ++DX)
				{
					const int32 Distance = DX * DX + DY * DY;
					const FCityCellCoord Wet(X + DX, Y + DY);
					if (Distance < Nearest && Terrain.IsWaterCell(Wet))
					{
						Nearest = Distance;
						Water = Terrain.GetWaterSurfaceHeightCm(Wet);
						WaterOffset = FIntPoint(DX, DY);
					}
				}
			if (Nearest > 4)
				continue;
			FPatch Patch{PatchHash, {}};
			// Jitter separate strata across the full 8m tile rather than placing
			// a miniature repeated five-object arrangement at one anchor.
			for (int32 I = 0; I < 16; ++I)
			{
				const uint32 Key = FCitySceneryField::Hash(PatchHash, I, Seed);
				const double JX = .2 + .6 * (Key & 255) / 255.;
				const double JY = .2 + .6 * ((Key >> 8) & 255) / 255.;
				const FVector2D Point(X + (I % 4 + JX) / 4., Y + (I / 4 + JY) / 4.);
				FVector Normal;
				const double Height = SampleGround(Terrain, Point.X, Point.Y, Normal);
				const double AboveWater = Height - Water;
				if (AboveWater < 3 || AboveWater > 300 || Normal.Z < .94)
					continue;
				const uint32 Choice = (Key >> 16) % 100;
				ECityBeachKind Kind;
				if (AboveWater <= 70)
					Kind = Choice < 35   ? ECityBeachKind::Starfish
					       : Choice < 70 ? ECityBeachKind::FanShell
					                     : ECityBeachKind::SpiralShell;
				else if (AboveWater < 140)
					Kind = Choice < 25   ? ECityBeachKind::Driftwood
					       : Choice < 65 ? ECityBeachKind::FanShell
					                     : ECityBeachKind::SpiralShell;
				else
					Kind = Choice < 25 && AboveWater < 260 ? ECityBeachKind::Driftwood : ECityBeachKind::DuneGrass;
				const double Radius = Kind == ECityBeachKind::Driftwood ? 65 : 25;
				const FVector Position(Point.X * CellSize, Point.Y * CellSize, Height + .5);
				if (Patch.Props.ContainsByPredicate(
				        [&](const FCityBeachCandidate& Other)
				        {
					        const double OtherRadius = Other.Kind == ECityBeachKind::Driftwood ? 65 : 25;
					        return FVector::DistSquared2D(Position, Other.Transform.GetLocation()) <
					               FMath::Square(Radius + OtherRadius);
				        }))
					continue;
				const double Yaw = Kind == ECityBeachKind::Driftwood
				                       ? FMath::RadiansToDegrees(FMath::Atan2(static_cast<double>(WaterOffset.X),
				                                                              static_cast<double>(-WaterOffset.Y))) +
				                             static_cast<int32>(Key % 51) - 25
				                       : Key % 360;
				const FQuat Rotation = FRotationMatrix::MakeFromZ(Normal).ToQuat() *
				                       FQuat(FVector::UpVector, FMath::DegreesToRadians(Yaw));
				Patch.Props.Add({{X, Y}, Kind, FTransform(Rotation, Position), PatchHash, Water});
			}
			if (!Patch.Props.IsEmpty())
			{
				// Shuffle before capping; a tile's first row must not get priority.
				Patch.Props.Sort(
				    [&](const FCityBeachCandidate& A, const FCityBeachCandidate& B)
				    {
					    const auto PA = A.Transform.GetLocation(), PB = B.Transform.GetLocation();
					    return FCitySceneryField::Hash(FMath::RoundToInt(PA.X), FMath::RoundToInt(PA.Y), Seed) <
					           FCitySceneryField::Hash(FMath::RoundToInt(PB.X), FMath::RoundToInt(PB.Y), Seed);
				    });
				Patches.Add(MoveTemp(Patch));
			}
		}
	Patches.Sort([](const FPatch& A, const FPatch& B) { return A.Hash < B.Hash; });
	TArray<FCityBeachCandidate> Result;
	Result.Reserve(MaxProps);
	// Round-robin the selected tiles so one dense tile cannot consume the
	// unchanged city-wide budget. Development only removes these fixed poses.
	for (int32 Slot = 0; Slot < 8; ++Slot)
		for (int32 Index = 0; Index < FMath::Min(MaxClusters, Patches.Num()); ++Index)
			if (Patches[Index].Props.IsValidIndex(Slot) && Result.Num() < MaxProps)
				Result.Add(Patches[Index].Props[Slot]);
	return Result;
}

bool FCityBeachScenery::IsClear(const FCityPlacementModel& City, const FCityCellCoord& Cell)
{
	for (int32 Dy = -1; Dy <= 1; ++Dy)
		for (int32 Dx = -1; Dx <= 1; ++Dx)
		{
			const FCityCellCoord Neighbor(Cell.X + Dx, Cell.Y + Dy);
			if (City.GetGrid().GetOccupant(Neighbor) != 0 || City.IsRoad(Neighbor) ||
			    City.IsRoad({Neighbor.X, Neighbor.Y, 1}) || City.GetZone(Neighbor) != ECityZoneType::None)
				return false;
		}
	return true;
}
