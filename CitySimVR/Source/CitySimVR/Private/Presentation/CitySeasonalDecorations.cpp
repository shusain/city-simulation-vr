#include "Presentation/CitySeasonalDecorations.h"

#include "Commands/CityPlacementModel.h"
#include "Presentation/CityScenery.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Simulation/CityDevelopmentSimulation.h"
#include "Simulation/CityEnvironmentSimulation.h"
#include "Simulation/CityGrowthSimulation.h"
#include "Simulation/CityTerrain.h"

namespace
{
	constexpr double CellMeters = 8.0;
	struct FEligibleLot
	{
		uint64 EntityId;
		uint32 Hash;
		bool bPark;
		bool bHome;
	};
	bool IsReviewedPark(const FCityBuildingDefinition& D)
	{
		const FName Id = D.DefinitionId;
		const int32 Side = Id == TEXT("Park") ? 3
		                   : Id == TEXT("NeighborhoodPark") || Id == TEXT("BasketballPark") || Id == TEXT("SkatePark")
		                       ? 5
		                   : Id == TEXT("CityPark") || Id == TEXT("BaseballPark") || Id == TEXT("SoccerPark") ? 7
		                                                                                                      : 0;
		return Side > 0 && D.Footprint.Width == Side && D.Footprint.Height == Side;
	}

	bool IsCityLot(const FCityBuildingDefinition& Definition)
	{
		// Only meshes with reviewed spare front corners. Do not infer safe space
		// from a civic tag or the bounds of an unfamiliar building family.
		const FName Id = Definition.DefinitionId;
		return ((Id == TEXT("Home") || Id == TEXT("Shop")) && Definition.Footprint.Width == 2 &&
		        Definition.Footprint.Height == 2) ||
		       ((Id == TEXT("School") || Id == TEXT("Police") || Id == TEXT("Fire")) &&
		        Definition.Footprint.Width == 3 && Definition.Footprint.Height == 2) ||
		       (Id == TEXT("Hospital") && Definition.Footprint.Width == 4 && Definition.Footprint.Height == 3);
	}

	FVector2D RotateQuarterTurns(FVector2D Point, int32 Turns)
	{
		for (int32 I = 0; I < (Turns % 4 + 4) % 4; ++I)
			Point = FVector2D(-Point.Y, Point.X);
		return Point;
	}

	bool SafeSurface(const FCityPlacementModel& City, const FCityTerrainHeightfield& Terrain,
	                 const FCityPlacedEntity& Entity, const FVector2D& Position, double Radius)
	{
		if (Position.X - Radius < Entity.Anchor.X || Position.Y - Radius < Entity.Anchor.Y ||
		    Position.X + Radius > Entity.Anchor.X + Entity.FootprintSize.X ||
		    Position.Y + Radius > Entity.Anchor.Y + Entity.FootprintSize.Y)
			return false;
		for (const FVector2D& Offset : {FVector2D(-Radius, -Radius), FVector2D(-Radius, Radius),
		                                FVector2D(Radius, -Radius), FVector2D(Radius, Radius)})
		{
			const FCityCellCoord Cell(FMath::FloorToInt(Position.X + Offset.X),
			                          FMath::FloorToInt(Position.Y + Offset.Y));
			if (!City.GetGrid().IsValidCell(Cell) || City.GetGrid().GetOccupant(Cell) != Entity.EntityId ||
			    Terrain.GetWaterClass(Cell) != ECityWaterClass::Land ||
			    Terrain.GetCellSlope(Cell) == ECityTerrainSlope::Steep || City.IsRoad(Cell) ||
			    City.IsReservedRoadCorridor(Cell))
				return false;
		}
		return true;
	}
} // namespace

void FCitySeasonalDecorations::Build(const FCityPlacementModel& City, const FCityTerrainHeightfield& Terrain,
                                     const FCityServiceSnapshot& Services, const FCityDevelopmentSnapshot& Development,
                                     const UCityBuildingCatalog* Catalog, const FCityEnvironmentSnapshot& Weather,
                                     const int32 Month, TArray<FCitySeasonalDecorationSample>& OutSamples)
{
	OutSamples.Reset();
	if (!Catalog || Month < 1 || Month > 12)
		return;
	const bool bHarvest = Month >= 9 && Month <= 11, bFestive = Month == 12;
	const bool bAutumn = Weather.Season == ECitySeason::Autumn, bWinter = Weather.Season == ECitySeason::Winter;
	const bool bFlowers =
	    !bHarvest && !bFestive && (Weather.Season == ECitySeason::Spring || Weather.Season == ECitySeason::Summer);
	const bool bWinterCold = bWinter && Weather.Temperature == ECityTemperatureBand::Cold;
	if (!bHarvest && !bFestive && !bFlowers && !bAutumn && !bWinter)
		return;
	FCitySceneryField Scenery;
	if (bWinterCold)
		Scenery.Build(Terrain);
	TSet<uint64> ParkIds;
	for (const auto& Provider : Services.ParkProviders)
		ParkIds.Add(Provider.EntityId);
	TArray<FEligibleLot> Lots;
	const uint32 Seed = static_cast<uint32>(Terrain.GetParameters().Seed) ^
	                    (static_cast<uint32>(Weather.Season) << 24) ^ (Month * 0x9e3779b9u);
	for (const auto& Pair : City.GetEntities())
	{
		const auto& Entity = Pair.Value;
		const auto* Definition = Catalog->FindDefinition(Entity.DefinitionId);
		const auto* Failure = Services.EntityFailures.Find(Entity.EntityId);
		const auto* State = Development.Find(Entity.EntityId);
		if (!Definition || !Failure || *Failure != ECityOperationFailure::None ||
		    !Development.IsOperational(Entity.EntityId) || (State && State->HasCivicUpgrade()))
			continue;
		const bool bPark = IsReviewedPark(*Definition) && ParkIds.Contains(Entity.EntityId);
		const bool bHome = Entity.DefinitionId == TEXT("Home");
		if (!bPark && (!IsCityLot(*Definition) || (State && State->DensityLevel != ECityDensityLevel::Low)))
			continue;
		const uint32 Hash = FCitySceneryField::Hash(static_cast<uint32>(Entity.EntityId),
		                                            static_cast<uint32>(Entity.EntityId >> 32), Seed);
		if (!bPark && (bHome ? (!(bHarvest || bFestive || bAutumn || bWinter) || Hash % 2 != 0)
		                     : (!(bHarvest || bFestive) || Hash % 3 != 0)))
			continue;
		Lots.Add({Entity.EntityId, Hash, bPark, bHome});
	}
	Lots.Sort(
	    [](const FEligibleLot& A, const FEligibleLot& B)
	    {
		    if (A.bPark != B.bPark)
			    return A.bPark;
		    if (A.bHome != B.bHome)
			    return A.bHome;
		    return A.Hash != B.Hash ? A.Hash < B.Hash : A.EntityId < B.EntityId;
	    });
	int32 ParkCount = 0, CityCount = 0;
	for (const auto& Lot : Lots)
	{
		if (OutSamples.Num() >= MaximumSamples)
			break;
		if (Lot.bPark ? ParkCount >= MaximumParkSamples : CityCount >= MaximumCitySamples)
			continue;
		const auto& Entity = City.GetEntities().FindChecked(Lot.EntityId);
		const auto& Definition = *Catalog->FindDefinition(Entity.DefinitionId);
		const FVector2D Center(Entity.Anchor.X + Entity.FootprintSize.X * .5,
		                       Entity.Anchor.Y + Entity.FootprintSize.Y * .5);
		const double HalfX = Definition.Footprint.Width * CellMeters * .5,
		             HalfY = Definition.Footprint.Height * CellMeters * .5;
		const int32 Turns = Entity.QuarterTurnsClockwise + Definition.PresentationYawOffsetQuarterTurns;
		const bool bSnowman = Lot.bPark && bWinterCold && Scenery.GetSample(Entity.Anchor).WarmthPercent <= 65;
		const bool bLeaves = Lot.bPark && bAutumn;
		const double Sign = Lot.Hash & 1 ? 1.0 : -1.0;
		auto Add = [&](ECitySeasonalDecorationKind Kind, const FVector2D& AuthoredMeters, float Scale,
		               float SurfaceOffset, double RadiusMeters, bool bWall = false, bool bPlaza = false)
		{
			if (OutSamples.Num() >= MaximumSamples ||
			    (Lot.bPark ? ParkCount >= MaximumParkSamples : CityCount >= MaximumCitySamples))
				return;
			// Existing FBX import reflects authored Y; the building transform then
			// applies presentation yaw plus its saved quarter-turn orientation.
			const FVector2D Authored = AuthoredMeters / CellMeters;
			const FVector2D Position = Center + RotateQuarterTurns(FVector2D(Authored.X, -Authored.Y), Turns);
			const double Radius = RadiusMeters / CellMeters;
			if (!SafeSurface(City, Terrain, Entity, Position, Radius))
				return;
			const FVector2D GameplayLocal = RotateQuarterTurns(Position - Center, -Entity.QuarterTurnsClockwise);
			const int32 Entrances = Definition.EntranceSides;
			// Pedestal trees replace the central fountain; wreaths mount on the
			// existing door. Neither creates a collision or closes a street path.
			const double PathClearance = (1.5 + RadiusMeters) / CellMeters;
			if (!bWall && !bPlaza &&
			    (((Entrances & 5) && FMath::Abs(GameplayLocal.X) < PathClearance) ||
			     ((Entrances & 10) && FMath::Abs(GameplayLocal.Y) < PathClearance)))
				return;
			FCitySeasonalDecorationSample Sample;
			Sample.EntityId = Entity.EntityId;
			Sample.Kind = Kind;
			Sample.CellPosition = Position;
			Sample.YawDegrees = static_cast<float>((Turns % 4) * 90 + (bWall ? 0 : (Lot.Hash % 4) * 15));
			Sample.Scale =
			    bWall ? Scale
			          : Scale *
			                (.9f + static_cast<float>((Lot.Hash >> ((static_cast<int32>(Kind) % 4) * 4)) % 11) * .01f);
			Sample.bPark = Lot.bPark;
			Sample.bWallMounted = bWall;
			// Wall props and existing authored civic/park paving stay with their mesh.
			// Home yard props follow terrain after the artificial lawn slab is removed.
			Sample.bUseBuildingBase = !Lot.bHome || bWall;
			Sample.SurfaceOffsetCm = Sample.bUseBuildingBase ? SurfaceOffset : 2.f;
			OutSamples.Add(Sample);
			if (Lot.bPark)
				++ParkCount;
			else
				++CityCount;
		};
		if (Lot.bPark)
		{
			const FName Id = Entity.DefinitionId;
			const bool bSports = Id == TEXT("BasketballPark") || Id == TEXT("SkatePark") ||
			                     Id == TEXT("BaseballPark") || Id == TEXT("SoccerPark");
			const float LawnTop = Id == TEXT("Park") ? 37.f : 14.f;
			// Plain parks have3m-wide cross paths. Sports parks have perimeter
			// promenades, a central court/field and separate benches/trees.
			const FVector2D PathA(Sign * (bSports ? 3.6 : 3.0), bSports ? -HalfY + 3.2 : -5.0);
			const FVector2D PathB(-PathA.X, PathA.Y);
			if (bFestive)
			{
				const FVector2D TreeSite = bSports ? FVector2D(Sign * (HalfX - 3.1), 5.0) : FVector2D::ZeroVector;
				const float TreeTop = bSports ? LawnTop : Id == TEXT("Park") ? 67.f : 32.f;
				Add(ECitySeasonalDecorationKind::FestiveTree, TreeSite, bSports ? .65f : .9f, TreeTop, 1.3, false,
				    !bSports);
				Add(ECitySeasonalDecorationKind::Gift, PathA, 1.f, LawnTop, 1.3);
			}
			else if (bHarvest)
			{
				Add(ECitySeasonalDecorationKind::Pumpkin, PathA, 1.f, LawnTop, 1.3);
				Add(ECitySeasonalDecorationKind::Pumpkin, PathB, 1.f, LawnTop, 1.3);
			}
			else if (bFlowers)
			{
				Add(ECitySeasonalDecorationKind::FlowerBed, PathA, .65f, LawnTop, 1.3);
				Add(ECitySeasonalDecorationKind::FlowerBed, PathB, .65f, LawnTop, 1.3);
			}
			if (bLeaves)
			{
				// Place fallen leaves2.8m from a reviewed broadleaf trunk. They
				// extend past the canopy edge, without covering the trunk or paths.
				const double Trunk = Id == TEXT("Park") ? 7.0 : HalfX - (bSports ? 4.0 : 6.0);
				const double TreeSign = Lot.Hash & 2 ? 1.0 : -1.0;
				Add(ECitySeasonalDecorationKind::LeafPile, FVector2D(TreeSign * (Trunk - 2.7), TreeSign * (Trunk + .8)),
				    1.f, LawnTop, 1.3);
			}
			const FVector2D SnowSite = !bHarvest ? PathB
			                           : bSports ? FVector2D(Sign * (HalfX - 3.2), -5.0)
			                                     : FVector2D(Sign * 3.0, 5.0);
			if (bSnowman)
				Add(ECitySeasonalDecorationKind::Snowman, SnowSite, 1.f, LawnTop, 1.3);
		}
		else if (Lot.bHome)
		{
			// SeasonalBuildings door front is4.72m from the lot center. Half-size wreaths
			// sit6cm outside it, with their ring center at115cm on the185cm door.
			if (bAutumn || bWinter)
				Add(bWinter ? ECitySeasonalDecorationKind::WreathWinter : ECitySeasonalDecorationKind::WreathAutumn,
				    FVector2D(0, -4.78), .5f, 85.f, .325, true);
			if (bFestive || bHarvest || bAutumn)
			{
				const auto Kind = bFestive   ? ECitySeasonalDecorationKind::Gift
				                  : bHarvest ? ECitySeasonalDecorationKind::Pumpkin
				                             : ECitySeasonalDecorationKind::LeafPile;
				Add(Kind, FVector2D(Sign * 2.4, -7.1), .5f, 2.f, .65);
			}
		}
		else
		{
			const float Top = Entity.DefinitionId == TEXT("Shop")       ? -18.f
			                  : Entity.DefinitionId == TEXT("Hospital") ? 24.f
			                                                            : 16.f;
			Add(bFestive ? ECitySeasonalDecorationKind::Gift : ECitySeasonalDecorationKind::Pumpkin,
			    FVector2D(Sign * (HalfX - .45), -HalfY + .45), .35f, Top, .4);
		}
	}
	check(OutSamples.Num() <= MaximumSamples && ParkCount <= MaximumParkSamples && CityCount <= MaximumCitySamples);
}
