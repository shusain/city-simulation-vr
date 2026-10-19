#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
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
	struct FDecorationFixture
	{
		FCityTerrainHeightfield Terrain;
		FCityPlacementModel City{100000};
		UCityBuildingCatalog* Catalog = NewObject<UCityBuildingCatalog>();
		FCityServiceSnapshot Services;
		FCityDevelopmentSnapshot Development;
		FCityEnvironmentSnapshot Weather;
		explicit FDecorationFixture(int32 Seed = 1)
		{
			FCityTerrainGenerationParameters P;
			P.Seed = Seed;
			P.BroadElevationAmplitudeCm = P.HillAmplitudeCm = P.RidgeAmplitudeCm = 0;
			Terrain.Generate(P);
			City.SetTerrain(&Terrain);
			for (const FName& Id : {FName(TEXT("Park")), FName(TEXT("NeighborhoodPark")), FName(TEXT("CityPark")),
			                        FName(TEXT("BasketballPark")), FName(TEXT("SkatePark")), FName(TEXT("SoccerPark")),
			                        FName(TEXT("BaseballPark")), FName(TEXT("Home")), FName(TEXT("Shop")),
			                        FName(TEXT("School")), FName(TEXT("Industry"))})
			{
				FCityBuildingDefinition D;
				D.DefinitionId = Id;
				D.DisplayName = Id.ToString();
				D.Cost = 1;
				const bool bPark = Id.ToString().EndsWith(TEXT("Park"));
				const int32 Side = Id == TEXT("Park") ? 3
				                   : Id == TEXT("CityPark") || Id == TEXT("SoccerPark") || Id == TEXT("BaseballPark")
				                       ? 7
				                       : 5;
				D.Footprint = bPark                  ? FCityCellFootprint(Side, Side)
				              : Id == TEXT("School") ? FCityCellFootprint(3, 2)
				                                     : FCityCellFootprint(2, 2);
				D.PresentationYawOffsetQuarterTurns = 2;
				// Legacy pocket-park operation uses tuning, with zero catalog bonus.
				if (bPark)
				{
					D.EntranceSides = 15;
					D.ParkDesirabilityBonus = Id == TEXT("Park") ? 0 : 10;
				}
				Catalog->Definitions.Add(D);
			}
		}
		uint64 Add(FName Id, int32 Turns = 0)
		{
			const auto& D = *Catalog->FindDefinition(Id);
			for (int32 Y = 1; Y < FCityGrid::Height - 4; ++Y)
				for (int32 X = 1; X < FCityGrid::Width - 4; ++X)
				{
					auto Preview = City.BeginPreview(D, {X, Y}, Turns);
					const auto Result = City.CommitPlacement(Preview, D);
					if (!Result.IsSuccess())
						continue;
					const auto& E = City.GetEntities().FindChecked(Result.EntityId);
					Services.EntityFailures.Add(E.EntityId, ECityOperationFailure::None);
					FCityDevelopmentState State;
					State.EntityId = E.EntityId;
					Development.States.Add(E.EntityId, State);
					if (Id.ToString().EndsWith(TEXT("Park")))
						Services.ParkProviders.Add({E.EntityId, E.Anchor, E.FootprintSize, 5, 10});
					return E.EntityId;
				}
			return 0;
		}
		TArray<FCitySeasonalDecorationSample> Build(int32 Month) const
		{
			TArray<FCitySeasonalDecorationSample> Result;
			FCitySeasonalDecorations::Build(City, Terrain, Services, Development, Catalog, Weather, Month, Result);
			return Result;
		}
	};
	int32 ClimateSeed(int32 Warmth)
	{
		for (int32 Seed = 1; Seed < 100; ++Seed)
			if (FCitySceneryField::ClimateWarmthPercent(Seed) == Warmth)
				return Seed;
		return 1;
	}
	bool HasKind(const TArray<FCitySeasonalDecorationSample>& Samples, ECitySeasonalDecorationKind Kind)
	{
		return Samples.ContainsByPredicate([Kind](const auto& S) { return S.Kind == Kind; });
	}
	FVector2D AuthoredMeters(const FCitySeasonalDecorationSample& Sample, const FCityPlacedEntity& Entity,
	                         const FCityBuildingDefinition& Definition)
	{
		FVector2D Point = (Sample.CellPosition - FVector2D(Entity.Anchor.X + Entity.FootprintSize.X * .5,
		                                                   Entity.Anchor.Y + Entity.FootprintSize.Y * .5)) *
		                  8;
		const int32 Turns = (Entity.QuarterTurnsClockwise + Definition.PresentationYawOffsetQuarterTurns) % 4;
		for (int32 I = 0; I < (4 - Turns) % 4; ++I)
			Point = FVector2D(-Point.Y, Point.X);
		return FVector2D(Point.X, -Point.Y);
	}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCitySeasonalDecorationCalendarTest,
                                 "CitySim.Presentation.SeasonalDecorationsCalendarAndClimate",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCitySeasonalDecorationCalendarTest::RunTest(const FString& Parameters)
{
	FDecorationFixture Cool(ClimateSeed(30)), Warm(ClimateSeed(80));
	TestTrue(TEXT("Cool and warm park fixtures place"), Cool.Add(TEXT("Park")) != 0 && Warm.Add(TEXT("Park")) != 0);
	for (auto* Fixture : {&Cool, &Warm})
	{
		Fixture->Weather.Season = ECitySeason::Spring;
		TestTrue(TEXT("Spring parks have flowers"), HasKind(Fixture->Build(2), ECitySeasonalDecorationKind::FlowerBed));
		Fixture->Weather.Season = ECitySeason::Summer;
		TestTrue(TEXT("Summer parks have flowers"), HasKind(Fixture->Build(5), ECitySeasonalDecorationKind::FlowerBed));
		Fixture->Weather.Season = ECitySeason::Autumn;
		TestFalse(TEXT("Harvest has explicit September start"),
		          HasKind(Fixture->Build(8), ECitySeasonalDecorationKind::Pumpkin));
		TestTrue(TEXT("Autumn leaves begin before the September harvest window"),
		         HasKind(Fixture->Build(7), ECitySeasonalDecorationKind::LeafPile));
		for (int32 Month : {9, 10, 11})
		{
			const auto Samples = Fixture->Build(Month);
			TestTrue(TEXT("September through November parks have harvest pumpkins"),
			         HasKind(Samples, ECitySeasonalDecorationKind::Pumpkin));
			TestFalse(TEXT("Harvest has no premature festive trees"),
			          HasKind(Samples, ECitySeasonalDecorationKind::FestiveTree));
		}
		Fixture->Weather.Season = ECitySeason::Winter;
		Fixture->Weather.Temperature = ECityTemperatureBand::Cold;
		const auto December = Fixture->Build(12);
		TestTrue(TEXT("December dressing works in every climate"),
		         HasKind(December, ECitySeasonalDecorationKind::FestiveTree) &&
		             HasKind(December, ECitySeasonalDecorationKind::Gift));
		TestFalse(TEXT("December ends the harvest window"), HasKind(December, ECitySeasonalDecorationKind::Pumpkin));
		TestTrue(TEXT("Invalid calendar input clears prior output"),
		         Fixture->Build(0).IsEmpty() && Fixture->Build(13).IsEmpty());
	}
	TestTrue(TEXT("Cold suitable winter park adds snowman"),
	         HasKind(Cool.Build(12), ECitySeasonalDecorationKind::Snowman));
	TestFalse(TEXT("Warm lowland holiday park has no snowman"),
	          HasKind(Warm.Build(12), ECitySeasonalDecorationKind::Snowman));
	Cool.Weather.Temperature = ECityTemperatureBand::Mild;
	TestFalse(TEXT("Mild winter removes snow props while retaining holiday dressing"),
	          HasKind(Cool.Build(12), ECitySeasonalDecorationKind::Snowman));
	Cool.Weather.Season = ECitySeason::Spring;
	TestFalse(TEXT("Year rollover clears December dressing"),
	          HasKind(Cool.Build(1), ECitySeasonalDecorationKind::FestiveTree));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCitySeasonalDecorationBoundedTest,
                                 "CitySim.Presentation.SeasonalDecorationsBoundedDeterministicReadOnly",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCitySeasonalDecorationBoundedTest::RunTest(const FString& Parameters)
{
	FDecorationFixture F;
	for (int32 I = 0; I < 20; ++I)
		TestTrue(TEXT("Budget fixture parks place"), F.Add(TEXT("Park")) != 0);
	for (int32 I = 0; I < 300; ++I)
		TestTrue(TEXT("Budget fixture city lots place"), F.Add(I % 2 ? TEXT("Home") : TEXT("Shop")) != 0);
	F.Weather.Season = ECitySeason::Winter;
	F.Weather.Temperature = ECityTemperatureBand::Mild;
	const auto Before = F.City.ExportState();
	const auto Heights = F.Terrain.GetQuantizedHeightsCm();
	const auto RandomState = F.Weather.WeatherRandomState;
	const auto First = F.Build(12), Second = F.Build(12);
	TestTrue(TEXT("Repeated selection is exact with no frame RNG"), First == Second);
	int32 Parks = 0, Lots = 0;
	bool bReachedCityLots = false;
	for (const auto& Sample : First)
	{
		Parks += Sample.bPark;
		Lots += !Sample.bPark;
		if (!Sample.bPark)
			bReachedCityLots = true;
		TestFalse(TEXT("Park decoration budget has first priority"), bReachedCityLots && Sample.bPark);
		const auto& E = F.City.GetEntities().FindChecked(Sample.EntityId);
		TestTrue(TEXT("Decoration remains inside owning lot with complete prop clearance"),
		         Sample.CellPosition.X > E.Anchor.X && Sample.CellPosition.Y > E.Anchor.Y &&
		             Sample.CellPosition.X < E.Anchor.X + E.FootprintSize.X &&
		             Sample.CellPosition.Y < E.Anchor.Y + E.FootprintSize.Y);
		const FCityCellCoord Cell(FMath::FloorToInt(Sample.CellPosition.X), FMath::FloorToInt(Sample.CellPosition.Y));
		TestEqual(TEXT("Decoration footprint remains owned by same building"), F.City.GetGrid().GetOccupant(Cell),
		          E.EntityId);
		TestTrue(TEXT("Decorations avoid road, water and unsafe slopes"),
		         !F.City.IsRoad(Cell) && !F.Terrain.IsWaterCell(Cell) &&
		             F.Terrain.GetCellSlope(Cell) != ECityTerrainSlope::Steep);
		TestTrue(TEXT("Ground city props stay within reviewed porch margins"),
		         Sample.bPark || Sample.bWallMounted || Sample.Scale <= .5f);
		if (!Sample.bPark)
		{
			if (Sample.bWallMounted)
			{
				TestEqual(TEXT("Wreath scale remains authored and exact"), Sample.Scale, .5f);
				TestEqual(TEXT("Wreath ground pivot puts ring center at the approved door height"),
				          Sample.SurfaceOffsetCm, 85.f);
			}
			else
			{
				TestTrue(TEXT("Front Home props clear the shrunk body with conservative radius"),
				         Sample.CellPosition.Y + .08125 < E.Anchor.Y + .23);
				TestTrue(TEXT("Front Home props remain far from the centered entrance path"),
				         FMath::Abs(Sample.CellPosition.X - (E.Anchor.X + 1)) - .08125 > .1875);
				TestTrue(TEXT("Ground props stand two centimeters above terrain"),
				         !Sample.bUseBuildingBase && Sample.SurfaceOffsetCm == 2.f);
			}
		}
	}
	TestEqual(TEXT("Total decoration cap is enforced"), First.Num(), FCitySeasonalDecorations::MaximumSamples);
	TestEqual(TEXT("Park cap is enforced independently"), Parks, FCitySeasonalDecorations::MaximumParkSamples);
	TestEqual(TEXT("City cap is enforced independently"), Lots, FCitySeasonalDecorations::MaximumCitySamples);
	FString Error;
	auto Reversed = Before;
	Reversed.Entities.Sort([](const auto& A, const auto& B) { return A.EntityId > B.EntityId; });
	TMap<FName, FCityBuildingDefinition> Definitions;
	for (const auto& D : F.Catalog->Definitions)
		Definitions.Add(D.DefinitionId, D);
	auto Reloaded = FCityPlacementModel::CreateValidatedFromState(Reversed, Definitions, Error);
	if (TestTrue(TEXT("Saved lot fixture validates after reordered records"), Reloaded.IsValid()))
	{
		Reloaded->SetTerrain(&F.Terrain);
		TArray<FCitySeasonalDecorationSample> Restored;
		FCitySeasonalDecorations::Build(*Reloaded, F.Terrain, F.Services, F.Development, F.Catalog, F.Weather, 12,
		                                Restored);
		TestTrue(TEXT("Reload and unordered entity records reproduce exact decorations"), First == Restored);
	}
	TestEqual(TEXT("Cosmetics preserve money"), F.City.GetFunds(), Before.Funds);
	TestEqual(TEXT("Cosmetics preserve command revision"), F.City.GetRevision(), Before.Revision);
	TestEqual(TEXT("Cosmetics preserve entity allocation"), F.City.GetNextEntityId(), Before.NextEntityId);
	TestTrue(TEXT("Cosmetics preserve terrain"), Heights == F.Terrain.GetQuantizedHeightsCm());
	TestEqual(TEXT("Cosmetics preserve authoritative weather random stream"), F.Weather.WeatherRandomState,
	          RandomState);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCitySeasonalDecorationEligibilityTest,
                                 "CitySim.Presentation.SeasonalDecorationsRotationAndLifecycle",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCitySeasonalDecorationEligibilityTest::RunTest(const FString& Parameters)
{
	TArray<FVector2D> OriginalOffsets;
	for (int32 Turns = 0; Turns < 4; ++Turns)
	{
		FDecorationFixture F;
		const uint64 Id = F.Add(TEXT("Park"), Turns);
		F.Weather.Season = ECitySeason::Autumn;
		const auto Samples = F.Build(9);
		TestEqual(TEXT("Every park orientation retains harvest accents and a nearby leaf pile"), Samples.Num(), 3);
		if (!Id || Samples.Num() != 3)
			continue;
		const auto& E = F.City.GetEntities().FindChecked(Id);
		const FVector2D Center(E.Anchor.X + E.FootprintSize.X * .5, E.Anchor.Y + E.FootprintSize.Y * .5);
		for (int32 I = 0; I < Samples.Num(); ++I)
		{
			const FVector2D Offset = Samples[I].CellPosition - Center;
			if (Turns == 0)
				OriginalOffsets.Add(Offset);
			FVector2D Expected = OriginalOffsets[I];
			for (int32 T = 0; T < Turns; ++T)
				Expected = FVector2D(-Expected.Y, Expected.X);
			TestTrue(TEXT("Imported authored corners follow saved quarter-turn rotation"),
			         Offset.Equals(Expected, .000001));
			TestTrue(TEXT("Four entrance paths remain clear"), FMath::Abs(Offset.X) > .3 && FMath::Abs(Offset.Y) > .3);
			TestTrue(TEXT("Pocket lawn top includes clearance"),
			         Samples[I].bUseBuildingBase && Samples[I].SurfaceOffsetCm == 37.f);
		}
		F.Services.EntityFailures[Id] = ECityOperationFailure::FireDamage;
		TestTrue(TEXT("Damaged lots immediately lose dressing"), F.Build(9).IsEmpty());
		F.Services.EntityFailures[Id] = ECityOperationFailure::None;
		F.Development.States[Id].Stage = ECityDevelopmentStage::Construction;
		TestTrue(TEXT("Construction silhouettes remain clear"), F.Build(9).IsEmpty());
		F.Development.States[Id].Stage = ECityDevelopmentStage::Operational;
		F.Development.States[Id].PendingCivicLevel = 1;
		TestTrue(TEXT("Active upgrade construction remains clear"), F.Build(9).IsEmpty());
		F.Development.States[Id].PendingCivicLevel = -1;
		F.Services.ParkProviders.Reset();
		TestTrue(TEXT("Park operation outage removes accents"), F.Build(9).IsEmpty());
		F.Services.ParkProviders.Add({Id, E.Anchor, E.FootprintSize, 5, 10});
		TestTrue(TEXT("Repaired park restores the same arrangement"), F.Build(9) == Samples);
		F.City.DemolishEntity(Id);
		TestTrue(TEXT("Stale provider records cannot preserve demolished props"), F.Build(9).IsEmpty());
	}
	FDecorationFixture F;
	for (int32 I = 0; I < 30; ++I)
		F.Add(TEXT("Home"));
	F.Weather.Season = ECitySeason::Autumn;
	const auto Homes = F.Build(9);
	TSet<uint64> DressedHomes;
	for (const auto& S : Homes)
		DressedHomes.Add(S.EntityId);
	TestTrue(TEXT("Stable seeded subset dresses city lots"),
	         !Homes.IsEmpty() && DressedHomes.Num() < F.City.GetEntities().Num());
	for (auto& Pair : F.Development.States)
		Pair.Value.DensityLevel = ECityDensityLevel::High;
	TestTrue(TEXT("Unreviewed high-density margins are conservatively excluded"), F.Build(9).IsEmpty());
	for (auto& Pair : F.Development.States)
		Pair.Value.DensityLevel = ECityDensityLevel::Low;
	F.Services.EntityFailures.Reset();
	TestTrue(TEXT("Missing operational records cannot invent operating lots"), F.Build(9).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCitySeasonalDecorationParkLayoutTest,
                                 "CitySim.Presentation.SeasonalDecorationsReviewedParkLayouts",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCitySeasonalDecorationParkLayoutTest::RunTest(const FString& Parameters)
{
	for (const FName& DefinitionId :
	     {FName(TEXT("Park")), FName(TEXT("NeighborhoodPark")), FName(TEXT("CityPark")), FName(TEXT("BasketballPark")),
	      FName(TEXT("SkatePark")), FName(TEXT("SoccerPark")), FName(TEXT("BaseballPark"))})
		for (int32 Turns = 0; Turns < 4; ++Turns)
		{
			FDecorationFixture F(ClimateSeed(30));
			const uint64 Id = F.Add(DefinitionId, Turns);
			if (!TestTrue(TEXT("Reviewed park fixture places"), Id != 0))
				continue;
			const auto& E = F.City.GetEntities().FindChecked(Id);
			const auto& D = *F.Catalog->FindDefinition(DefinitionId);
			const bool bSports = DefinitionId == TEXT("BasketballPark") || DefinitionId == TEXT("SkatePark") ||
			                     DefinitionId == TEXT("SoccerPark") || DefinitionId == TEXT("BaseballPark");
			const double Half = D.Footprint.Width * 4.;
			for (int32 Month : {7, 9, 10, 12})
			{
				F.Weather.Season = Month <= 9 ? ECitySeason::Autumn : ECitySeason::Winter;
				F.Weather.Temperature = ECityTemperatureBand::Cold;
				const auto Samples = F.Build(Month);
				TestTrue(TEXT("Every reviewed park has seasonal dressing"), !Samples.IsEmpty());
				for (int32 I = 0; I < Samples.Num(); ++I)
				{
					const auto& S = Samples[I];
					const FVector2D P = AuthoredMeters(S, E, D);
					TestTrue(TEXT("Complete conservative prop support fits its lawn"),
					         FMath::Abs(P.X) + 1.3 < Half && FMath::Abs(P.Y) + 1.3 < Half);
					for (int32 J = I + 1; J < Samples.Num(); ++J)
						TestTrue(TEXT("Park props have distinct sites with conservative horizontal clearance"),
						         FVector2D::Distance(S.CellPosition, Samples[J].CellPosition) * 8. >= 2.6 - .00001);
					if (S.Kind == ECitySeasonalDecorationKind::FestiveTree && !bSports)
					{
						TestTrue(TEXT("Plain park festive tree is centered on the unobstructed December pedestal"),
						         P.IsNearlyZero(.000001));
						TestEqual(TEXT("Tree support matches pocket pedestal or larger plaza"), S.SurfaceOffsetCm,
						          DefinitionId == TEXT("Park") ? 67.f : 32.f);
						TestTrue(
						    TEXT(
						        "Centered festive tree fits reviewed two-meter plaza and stays visible at full park size"),
						    S.Scale >= .8099f && S.Scale <= .9001f);
					}
					else if (S.Kind == ECitySeasonalDecorationKind::LeafPile)
					{
						const double Trunk = DefinitionId == TEXT("Park") ? 7. : Half - (bSports ? 4. : 6.);
						const FVector2D Tree(FMath::Sign(P.X) * Trunk, FMath::Sign(P.Y) * Trunk);
						TestTrue(TEXT("Autumn leaves sit near an authored tree without covering its trunk"),
						         FMath::IsNearlyEqual(FVector2D::Distance(P, Tree), FMath::Sqrt(2.7 * 2.7 + .8 * .8),
						                              .000001));
					}
					else if (!bSports)
					{
						TestTrue(TEXT("Plain park accents sit beside cross paths with twenty-centimeter safety margin"),
						         FMath::IsNearlyEqual(FMath::Abs(P.X), 3., .000001) &&
						             FMath::IsNearlyEqual(FMath::Abs(P.Y), 5., .000001));
						TestTrue(TEXT("Full prop radius leaves three-meter cross paths open"),
						         FMath::Abs(P.X) - 1.3 > 1.5 && FMath::Abs(P.Y) - 1.3 > 1.5);
					}
					if (bSports)
					{
						if (S.Kind == ECitySeasonalDecorationKind::FestiveTree ||
						    (S.Kind == ECitySeasonalDecorationKind::Snowman && Month == 10))
						{
							const double CourtHalfX = DefinitionId == TEXT("SoccerPark")     ? 22.
							                          : DefinitionId == TEXT("BaseballPark") ? 22.
							                                                                 : 14.;
							TestTrue(TEXT("Sports side props clear courts, ramps and playing fields"),
							         FMath::Abs(P.X) - 1.3 > CourtHalfX);
							TestTrue(TEXT("Side props clear central soccer goals"), FMath::Abs(P.Y) - 1.3 > 3.11);
						}
						else if (S.Kind != ECitySeasonalDecorationKind::LeafPile)
						{
							TestTrue(TEXT("Sports path accents stay outside courts and below front benches"),
							         P.Y + 1.3 < -Half + 4.5 + .000001);
							TestTrue(TEXT("Sports path accents clear the entrance and perimeter promenade"),
							         FMath::Abs(P.X) - 1.3 > 1.5 && FMath::Abs(P.Y) + 1.3 < Half - 1.7);
						}
					}
				}
			}
		}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCitySeasonalDecorationHomePorchTest,
                                 "CitySim.Presentation.SeasonalDecorationsHomePorchAndDoorWreaths",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCitySeasonalDecorationHomePorchTest::RunTest(const FString& Parameters)
{
	for (int32 Month : {7, 9, 10, 12})
		for (int32 Turns = 0; Turns < 4; ++Turns)
		{
			const ECitySeason Season = Month <= 9 ? ECitySeason::Autumn : ECitySeason::Winter;
			int32 Seed = 1;
			while (FCitySceneryField::Hash(
			           1, 0, static_cast<uint32>(Seed) ^ (static_cast<uint32>(Season) << 24) ^ (Month * 0x9e3779b9u)) %
			       2)
				++Seed;
			FDecorationFixture F(Seed);
			const uint64 Id = F.Add(TEXT("Home"), Turns);
			if (!TestTrue(TEXT("Reviewed low Home fixture places"), Id != 0))
				continue;
			F.Weather.Season = Season;
			const auto Samples = F.Build(Month);
			if (!TestEqual(TEXT("Selected Home prioritizes a wreath and one porch accent"), Samples.Num(), 2))
				continue;
			const auto& E = F.City.GetEntities().FindChecked(Id);
			const auto& D = *F.Catalog->FindDefinition(TEXT("Home"));
			const auto& Wreath = Samples[0];
			const auto& Ground = Samples[1];
			TestTrue(TEXT("Door wreath is selected before ground props under city budget"),
			         Wreath.bWallMounted && !Ground.bWallMounted);
			TestTrue(TEXT("Autumn and winter wreath follow authoritative season, including October harvest"),
			         Wreath.Kind == (Season == ECitySeason::Autumn ? ECitySeasonalDecorationKind::WreathAutumn
			                                                       : ECitySeasonalDecorationKind::WreathWinter));
			TestTrue(TEXT("Wreath mounts six centimeters outside the authored door plane"),
			         AuthoredMeters(Wreath, E, D).Equals(FVector2D(0, -4.78), .000001));
			TestEqual(TEXT("Wall mesh stays parallel to rotated door with no cosmetic random yaw"), Wreath.YawDegrees,
			          static_cast<float>((Turns + 2) % 4 * 90));
			TestTrue(TEXT("Authored wreath scale, support height and ring center clear the porch canopy"),
			         Wreath.Scale == .5f && Wreath.bUseBuildingBase && Wreath.SurfaceOffsetCm == 85.f &&
			             Wreath.SurfaceOffsetCm + 60.f < 185.f);
			const FVector2D P = AuthoredMeters(Ground, E, D);
			TestTrue(TEXT("Ground accent occupies approved front porch strip at every orientation"),
			         FMath::IsNearlyEqual(FMath::Abs(P.X), 2.4, .000001) && FMath::IsNearlyEqual(P.Y, -7.1, .000001));
			TestTrue(TEXT("Conservative prop radius clears body, lot boundary and centered entrance"),
			         P.Y + .65 < -4.62 && P.Y - .65 > -8. && FMath::Abs(P.X) - .65 > 1.5);
			TestTrue(TEXT("Home ground accent is half scale and follows terrain"),
			         Ground.Scale <= .5001f && Ground.Scale >= .4499f && !Ground.bUseBuildingBase &&
			             Ground.SurfaceOffsetCm == 2.f);
			const auto Kind = Month == 12  ? ECitySeasonalDecorationKind::Gift
			                  : Month >= 9 ? ECitySeasonalDecorationKind::Pumpkin
			                               : ECitySeasonalDecorationKind::LeafPile;
			TestTrue(TEXT("Home ground accent follows harvest, festive and earlier autumn windows"),
			         Ground.Kind == Kind);
			F.Weather.Season = ECitySeason::Spring;
			TestTrue(TEXT("Spring clears autumn and winter Home dressing"), F.Build(2).IsEmpty());
			F.Weather.Season = Season;
			F.Services.EntityFailures[Id] = ECityOperationFailure::FireDamage;
			TestTrue(TEXT("Damage removes door and porch dressing together"), F.Build(Month).IsEmpty());
		}
	return true;
}
#endif
