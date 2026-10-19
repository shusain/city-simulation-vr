#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Commands/CityPlacementModel.h"
#include "Simulation/CityTerrain.h"
#include "Simulation/CityRoadNetwork.h"
#include "Simulation/CityTrafficSimulation.h"
#include "Persistence/CityPersistence.h"
namespace
{
FCityRoadPreview CrossingPreview(FCityPlacementModel& City, FCityCellCoord A, FCityCellCoord B,
                                 const FCityRoadDefinition& Def, int32 Height = 0)
{
	auto P = City.BeginRoadPreview(A, Def.Id, Def.PlacementCostPerCell);
	P.End = B;
	P.Profile = ECityRoadProfile::Elevated;
	P.ElevationStep = Height;
	return P;
}
} // namespace
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityRaisedCrossingTest, "CitySim.Roads.SameHeightRaisedCrossings",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityRaisedCrossingTest::RunTest(const FString& Parameters)
{
	FCityTerrainGenerationParameters Config;
	Config.BroadElevationAmplitudeCm = 0;
	Config.HillAmplitudeCm = 0;
	Config.RidgeAmplitudeCm = 0;
	Config.StarterHalfWidthCells = 16;
	Config.StarterDepthCells = 24;
	FCityTerrainHeightfield Terrain;
	Terrain.Generate(Config);
	TMap<FName, FCityBuildingDefinition> Definitions;
	FString Error;
	for (const auto& Def : {FCityRoadDefinition::Dirt(), FCityRoadDefinition::Local(),
	                        FCityRoadDefinition::Avenue(), FCityRoadDefinition::Highway()})
		for (bool VerticalFirst : {false, true})
			for (bool Reverse : {false, true})
			{
				FCityPlacementModel City(100000);
				City.SetTerrain(&Terrain);
				FCityCellCoord A(2, 8), B(16, 8), C(8, 2), D(8, 16);
				if (Reverse)
				{
					Swap(A, B);
					Swap(C, D);
				}
				auto First = CrossingPreview(City, VerticalFirst ? C : A, VerticalFirst ? D : B, Def);
				if (!TestTrue(TEXT("First complete span commits"), City.CommitRoad(First).IsSuccess()))
					continue;
				auto Second = CrossingPreview(City, VerticalFirst ? A : C, VerticalFirst ? B : D, Def);
				const auto Validation = City.ValidateRoad(Second);
				const int64 Funds = City.GetFunds();
				const auto Built = City.CommitRoad(Second);
				if (!TestTrue(TEXT("Perpendicular same-height deck crossing commits"), Built.IsSuccess()))
				{
					AddError(Built.Detail);
					continue;
				}
				TestEqual(TEXT("Cost exactly matches review including junction reinforcement"),
				          Funds - City.GetFunds(), Validation.Cost);
				const FCityCellCoord Junction(8, 8, 1);
				TestTrue(TEXT("One raised address shared by two spans"),
				         City.IsRoad(Junction) && City.GetRoadStructures().Num() == 2);
				TestFalse(TEXT("No ground road invented under crossing"), City.IsRoad({8, 8}));
				TestEqual(TEXT("Four deck approaches connect"), City.GetRoadNeighborMask(Junction),
				          static_cast<uint8>(15));
				TestTrue(TEXT("Shared ownership validates"), City.ValidateRoadStructures(Error));
				FCityRoadNetworkSnapshot Graph;
				FCityRoadNetworkBuilder::Build(City, Def, Graph);
				TestTrue(TEXT("Raised graph validates"), Graph.IsValid(&Error));
				TArray<FCityCellCoord> Route;
				TestTrue(TEXT("Vehicles can turn onto the other span"),
				         Graph.FindRoute(A, D, ECityRoadAccess::Car, Route));
				TestTrue(TEXT("Turning route uses shared raised junction"),
				         Route.Contains(Junction) && !Route.Contains(FCityCellCoord(8, 8)));
				FCityTrafficVehicleState Vehicle;
				Vehicle.Id = 1;
				TestTrue(TEXT("Turning vehicle route builds"),
				         FCityTrafficSimulation::BuildRoute(Graph, A, D, Vehicle));
				FCityTrafficSimulation Traffic;
				TestTrue(TEXT("Vehicle accepted"), Traffic.AddVehicle(Vehicle, Graph, &Error));
				TestTrue(TEXT("Turning traffic advances"), Traffic.AdvanceToStep(Graph, 8, Error));
				FCitySaveDocument Save, Loaded;
				Save.Placement = City.ExportState();
				Save.Terrain = Config;
				Save.Traffic = Traffic.GetSnapshot();
				Save.Economy.FixedStepId = 8;
				FString Json;
				TestTrue(TEXT("Crossing city saves"), FCityPersistence::Serialize(Save, Json, Error));
				TestTrue(TEXT("Crossing city loads"), FCityPersistence::Deserialize(Json, Loaded, Error));
				auto Restored =
				    FCityPlacementModel::CreateValidatedFromState(Loaded.Placement, Definitions, Error);
				if (!TestTrue(TEXT("Shared spans restore"), Restored.IsValid()))
					continue;
				Restored->SetTerrain(&Terrain);
				TestTrue(TEXT("Terrain validates shared decks"), Restored->ValidateRoadStructures(Error));
				FCityRoadNetworkSnapshot RestoredGraph;
				FCityRoadNetworkBuilder::Build(*Restored, Def, RestoredGraph);
				FCityTrafficSimulation Resumed;
				TestTrue(TEXT("Turning traffic resumes"),
				         Resumed.RestoreState(Loaded.Traffic, RestoredGraph, 8, Error));
				TestTrue(TEXT("Fine traffic continues"), Traffic.AdvanceToStep(Graph, 16, Error));
				TestTrue(TEXT("Resumed traffic continues"), Resumed.AdvanceToStep(RestoredGraph, 16, Error));
				if (Traffic.GetSnapshot().Vehicles.Num() && Resumed.GetSnapshot().Vehicles.Num())
					TestEqual(TEXT("Traffic progress survives save/catch-up"),
					          Traffic.GetSnapshot().Vehicles[0].DistanceUnits,
					          Resumed.GetSnapshot().Vehicles[0].DistanceUnits);
				FString RelabeledJson = Json;
				RelabeledJson.ReplaceInline(
				    *FString::Printf(TEXT("\"schemaVersion\": %d"), FCitySaveDocument::CurrentSchemaVersion),
				    TEXT("\"schemaVersion\": 48"));
				TestFalse(TEXT("Relabeled old-format crossing rejects load"),
				          FCityPersistence::Deserialize(RelabeledJson, Loaded, Error));
				Save.SchemaVersion = 48;
				TestFalse(TEXT("Old format cannot encode shared intersection ownership"),
				          FCityPersistence::Serialize(Save, Json, Error));
				auto Corrupt = Loaded.Placement;
				Corrupt.RoadStructures[1].DeckHeightCm += 200;
				TestFalse(
				    TEXT("Mismatched saved decks reject restore"),
				    FCityPlacementModel::CreateValidatedFromState(Corrupt, Definitions, Error).IsValid());
				const auto Removed = Restored->GetRoadStructures()[0];
				const FCityCellCoord Away(Removed.Start.X + (Removed.End.X > Removed.Start.X   ? 4
				                                             : Removed.End.X < Removed.Start.X ? -4
				                                                                               : 0),
				                          Removed.Start.Y + (Removed.End.Y > Removed.Start.Y   ? 4
				                                             : Removed.End.Y < Removed.Start.Y ? -4
				                                                                               : 0),
				                          1);
				TestTrue(TEXT("Demolish one complete span"), Restored->RemoveRoad(Away));
				TestTrue(TEXT("Other span retains its junction tile"),
				         Restored->IsRoad(Junction) && Restored->GetRoadStructures().Num() == 1);
				TestTrue(TEXT("Remaining span validates after demolition"),
				         Restored->ValidateRoadStructures(Error));
				TestEqual(TEXT("Junction becomes straight after demolition"),
				          Restored->GetRoadNeighborMask(Junction),
				          static_cast<uint8>(VerticalFirst ? 10 : 5));
				TestTrue(TEXT("Last span removes shared tile"), Restored->RemoveRoad(Junction));
				TestFalse(TEXT("No orphan shared deck remains"), Restored->IsRoad(Junction));
			}
	const auto Def = FCityRoadDefinition::Local();
	FCityPlacementModel City(100000);
	City.SetTerrain(&Terrain);
	auto First = CrossingPreview(City, {2, 8}, {16, 8}, Def);
	City.CommitRoad(First);
	for (const auto& P :
	     {CrossingPreview(City, {8, 2}, {8, 16}, Def, 1), CrossingPreview(City, {4, 2}, {4, 16}, Def),
	      CrossingPreview(City, {2, 8}, {16, 8}, Def)})
	{
		auto Preview = P;
		const int64 Funds = City.GetFunds();
		TestFalse(TEXT("Height/ramp/parallel/class conflicts cannot commit"),
		          City.CommitRoad(Preview).IsSuccess());
		TestEqual(TEXT("Rejected crossing does not charge"), City.GetFunds(), Funds);
		TestEqual(TEXT("Rejected crossing retains only original span"), City.GetRoadStructures().Num(), 1);
	}
	return true;
}
#endif
