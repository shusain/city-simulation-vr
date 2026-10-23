#include "Presentation/CityBeachScenery.h"
#include "Commands/CityPlacementModel.h"
#include "Interaction/CityBoardPicking.h"
#include "Presentation/CityScenery.h"

double FCityBeachScenery::ClearanceRadiusCm(ECityBeachKind Kind)
{
	const double Radii[] = {25, 25, 25, 65, 25, 50, 8, 12, 16, 111, 102, 48, 98, 35, 27, 50};
	return Radii[static_cast<int32>(Kind)];
}

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
	// Positions round-trip through centimetres in saved/rendered transforms.
	// Give exact diagonal-edge samples a stable facet despite floating error.
	if (TX + TY <= 1. + 1.e-9)
	{
		Normal = FVector(A - B, A - C, Spacing).GetSafeNormal();
		return A + TX * (B - A) + TY * (C - A);
	}
	Normal = FVector(C - D, B - D, Spacing).GetSafeNormal();
	return D + (1. - TX) * (C - D) + (1. - TY) * (B - D);
}

TArray<FCityBeachCandidate> FCityBeachScenery::SelectCandidates(const FCityTerrainHeightfield& Terrain,
                                                                TConstArrayView<FCitySceneryCandidate> Scenery)
{
	struct FPatch
	{
		uint32 Hash;
		TArray<FCityBeachCandidate> Props;
		FCityCellCoord Cell;
		int32 Water;
		FIntPoint WaterOffset;
	};
	TArray<FPatch> Patches;
	const auto Dimensions = Terrain.GetDimensions();
	const uint32 Seed = static_cast<uint32>(Terrain.GetParameters().Seed) ^ 0xbeac412du;
	constexpr double CellSize = CityBoardPicking::LogicalCellSizeCm;
	for (int32 Y = 1; Y < Dimensions.GetHeight() - 1; ++Y)
		for (int32 X = 1; X < Dimensions.GetWidth() - 1; ++X)
		{
			const uint32 PatchHash = FCitySceneryField::Hash(X, Y, Seed);
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
			FPatch Patch{PatchHash, {}, {X, Y}, Water, WaterOffset};
			const bool bWooded = Scenery.ContainsByPredicate(
			    [&](const FCitySceneryCandidate& Tree)
			    {
				    return Tree.Kind != ECitySceneryKind::Rock && FMath::Abs(Tree.Cell.X - X) <= 1 &&
				           FMath::Abs(Tree.Cell.Y - Y) <= 1;
			    });
			const bool bWrackPatch = FCitySceneryField::Hash(X / 4, Y / 4, Seed ^ 0x59ca17u) % 4 == 0;
			const int32 TargetCount =
			    4 + static_cast<int32>((PatchHash >> 16) % 5) + (bWooded ? 2 : 0) + (bWrackPatch ? 3 : 0);
			// Jitter separate strata across the full 8m tile rather than placing
			// a miniature repeated five-object arrangement at one anchor.
			for (int32 I = 0; I < 16; ++I)
			{
				const uint32 Key = FCitySceneryField::Hash(PatchHash, I, Seed);
				const double JX = .2 + .6 * (Key & 255) / 255.;
				const double JY = .2 + .6 * ((Key >> 8) & 255) / 255.;
				const FVector2D Point(X + FMath::Clamp((I % 4 + JX) / 4., .09, .91),
				                      Y + FMath::Clamp((I / 4 + JY) / 4., .09, .91));
				FVector Normal;
				const double Height = SampleGround(Terrain, Point.X, Point.Y, Normal);
				const double AboveWater = Height - Water;
				if (AboveWater < 3 || AboveWater > 300 || Normal.Z < .94)
					continue;
				const uint32 Choice = (Key >> 16) % 100;
				ECityBeachKind Kind;
				if (AboveWater <= 70)
					Kind = Choice < 12   ? ECityBeachKind::Seaweed
					       : Choice < 33 ? ECityBeachKind::Starfish
					       : Choice < 55 ? ECityBeachKind::FanShell
					       : Choice < 71 ? ECityBeachKind::SpiralShell
					       : Choice < 81 ? ECityBeachKind::PebbleSmall
					       : Choice < 91 ? ECityBeachKind::PebbleMedium
					                     : ECityBeachKind::PebbleLarge;
				else if (AboveWater < 140)
					Kind = Choice < 25   ? ECityBeachKind::Seaweed
					       : Choice < 29 ? ECityBeachKind::Driftwood
					       : Choice < 44 ? ECityBeachKind::FanShell
					       : Choice < 56 ? ECityBeachKind::SpiralShell
					       : Choice < 76 ? ECityBeachKind::PebbleSmall
					       : Choice < 90 ? ECityBeachKind::PebbleMedium
					                     : ECityBeachKind::PebbleLarge;
				else
					Kind = Choice < 4 && AboveWater < 260 ? ECityBeachKind::Driftwood
					       : Choice < 48                  ? ECityBeachKind::DuneGrass
					       : Choice < 70                  ? ECityBeachKind::PebbleSmall
					       : Choice < 85                  ? ECityBeachKind::PebbleMedium
					                                      : ECityBeachKind::PebbleLarge;
				// Coherent wrack patches and nearby frozen tree candidates supply
				// local character without leaving the rest of the shoreline empty.
				const uint32 HabitatChoice = (Key >> 8) % 100;
				if (bWrackPatch && AboveWater < 140 && HabitatChoice < 45)
					Kind = ECityBeachKind::Seaweed;
				else if (bWooded && AboveWater > 70 && AboveWater < 260 && HabitatChoice < 10)
					Kind = ECityBeachKind::Driftwood;
				// Alternate the approved source on existing kelp sites; retain spacing and density.
				if (Kind == ECityBeachKind::Seaweed && ((Key >> 28) & 1u))
					Kind = ECityBeachKind::CurledKelp;
				const double Scale = IsPebble(Kind) ? .9 + .2 * ((Key >> 24) & 255) / 255. : 1.;
				const double Radius = ClearanceRadiusCm(Kind) * Scale;
				const FVector Position(Point.X * CellSize, Point.Y * CellSize, Height + .5);
				if (Patch.Props.ContainsByPredicate(
				        [&](const FCityBeachCandidate& Other)
				        {
					        const double OtherRadius = ClearanceRadiusCm(Other.Kind) * Other.Transform.GetScale3D().X;
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
				Patch.Props.Add({{X, Y}, Kind, FTransform(Rotation, Position, FVector(Scale)), PatchHash, Water});
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
				Patch.Props.SetNum(FMath::Min(TargetCount, Patch.Props.Num()));
				Patches.Add(MoveTemp(Patch));
			}
		}
	Patches.Sort([](const FPatch& A, const FPatch& B) { return A.Hash < B.Hash; });
	for (int32 Index = 0; Index < Patches.Num(); ++Index)
	{
		auto& Patch = Patches[Index];
		// A local sparse density, not a city-wide number of allowed groups.
		// Longer suitable shores can support more independent arrangements.
		if (Patch.Hash % 12 != 0)
			continue;
		// One modest household-scale arrangement. These static props carry no
		// resident, occupancy, purchasing or happiness state.
		const FVector2D Offsets[] = {{-130, 90}, {100, 80}, {-120, -105}, {90, -150}, {-30, -75}, {-40, -225}};
		const double Yaw = FMath::RadiansToDegrees(
		    FMath::Atan2(-static_cast<double>(Patch.WaterOffset.X), static_cast<double>(Patch.WaterOffset.Y)));
		const FQuat Facing(FVector::UpVector, FMath::DegreesToRadians(Yaw));
		TArray<FCityBeachCandidate> Group;
		for (int32 Item = 0; Item < 6; ++Item)
		{
			const auto Kind = static_cast<ECityBeachKind>(static_cast<int32>(ECityBeachKind::Umbrella) + Item);
			const FVector Offset = Facing.RotateVector(FVector(Offsets[Item], 0));
			const double X = Patch.Cell.X + .5 + Offset.X / CellSize;
			const double Y = Patch.Cell.Y + .5 + Offset.Y / CellSize;
			FVector Normal;
			const double Ground = SampleGround(Terrain, X, Y, Normal);
			bool bSupported = Ground - Patch.Water >= 100 && Ground - Patch.Water <= 300 && Normal.Z >= .995;
			for (int32 Edge = 0; Edge < 8 && bSupported; ++Edge)
			{
				const double Angle = Edge * PI / 4.;
				const double DX = FMath::Cos(Angle) * ClearanceRadiusCm(Kind);
				const double DY = FMath::Sin(Angle) * ClearanceRadiusCm(Kind);
				FVector EdgeNormal;
				const double Height = SampleGround(Terrain, X + DX / CellSize, Y + DY / CellSize, EdgeNormal);
				const double Plane = Ground - (Normal.X * DX + Normal.Y * DY) / Normal.Z;
				bSupported = Height - Patch.Water >= 50 && FMath::Abs(Height - Plane) <= 2.;
			}
			if (!bSupported)
				break;
			const FQuat Rotation = FRotationMatrix::MakeFromZ(Normal).ToQuat() * Facing;
			Group.Add({Patch.Cell, Kind, FTransform(Rotation, FVector(X * CellSize, Y * CellSize, Ground + .5)),
			           Patch.Hash, Patch.Water});
		}
		if (Group.Num() != 6)
			continue;
		Patch.Props.RemoveAll(
		    [&](const FCityBeachCandidate& Natural)
		    {
			    return Group.ContainsByPredicate(
			        [&](const FCityBeachCandidate& Furniture)
			        {
				        const double Radius = ClearanceRadiusCm(Furniture.Kind) +
				                              ClearanceRadiusCm(Natural.Kind) * Natural.Transform.GetScale3D().X;
				        return FVector::DistSquared2D(Furniture.Transform.GetLocation(),
				                                      Natural.Transform.GetLocation()) < FMath::Square(Radius);
			        });
		    });
		Group.Append(MoveTemp(Patch.Props));
		Patch.Props = MoveTemp(Group);
	}
	TArray<FCityBeachCandidate> Result;
	// Every suitable tile keeps its locally generated props. Presentation
	// limits must not decide which parts of a city are allowed to exist.
	for (auto& Patch : Patches)
		Result.Append(MoveTemp(Patch.Props));
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
