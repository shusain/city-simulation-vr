#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Presentation/CityDiagnosticOverlay.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Simulation/CityGrowthSimulation.h"
#include "Simulation/CityNeedsSimulation.h"
#include "Simulation/CityEnvironmentSimulation.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityWaterDiagnosticContractTest,
                                 "CitySim.Water.ExactDeliveryColorsAndRaisedPipeDiagnostics",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityWaterDiagnosticContractTest::RunTest(const FString& Parameters)
{
	FCityPlacementModel City(100000);
	auto Road = City.BeginRoadPreview({0, 10});
	City.UpdateRoadPreviewEnd(Road, {25, 10});
	City.CommitRoad(Road);
	auto Place = [&](FName Name, int32 X)
	{
		FCityBuildingDefinition D;
		D.DefinitionId = Name;
		D.DisplayName = Name.ToString();
		D.Footprint = FCityCellFootprint(1, 1);
		D.LogicalHeightCm = 100;
		auto Preview = City.BeginPreview(D, {X, 9});
		return City.CommitPlacement(Preview, D).EntityId;
	};
	const uint64 Home = Place(TEXT("Home"), 5), Hospital = Place(TEXT("Hospital"), 10), Shop = Place(TEXT("Shop"), 15);
	if (!TestTrue(TEXT("Diagnostic parcels place"), Home && Hospital && Shop))
		return false;
	FCityServiceSnapshot S;
	S.Water.RulesVersion = 1;
	S.ServiceCapacity = 100;
	S.ServiceUsed = 15;
	S.Water.RoadPressurePercent.Add({5, 10}, 80);
	S.Water.RoadPressurePercent.Add({5, 10, 1}, 120);
	S.Water.RoadPressurePercent.Add({20, 10, 1}, 60);
	S.Water.EntityDemand.Add(Home, 10);
	S.Water.EntityDelivered.Add(Home, 10);
	S.Water.EntityPressurePercent.Add(Home, 80);
	S.Water.EntityDemand.Add(Hospital, 10);
	S.Water.EntityDelivered.Add(Hospital, 5);
	S.Water.EntityPressurePercent.Add(Hospital, 30);
	S.Water.EntityCauses.Add(Hospital, ECityWaterFailure::LowPressure);
	S.Water.EntityDemand.Add(Shop, 10);
	S.Water.EntityDelivered.Add(Shop, 0);
	S.Water.EntityCauses.Add(Shop, ECityWaterFailure::SourceOutage);
	FCityNeedsSnapshot Needs;
	FCityEconomyTuning T;
	FCityEnvironmentSnapshot Environment;
	FCityDiagnosticOverlayResult R;
	FCityDiagnosticOverlayEvaluator::Build(City, S, Needs, T, nullptr, nullptr, nullptr, Environment,
	                                       ECityDiagnosticOverlay::Service, 0, R);
	const int32 HomeIndex = 9 * FCityGrid::Width + 5, HospitalIndex = 9 * FCityGrid::Width + 10,
	            ShopIndex = 9 * FCityGrid::Width + 15;
	TestTrue(TEXT("Full parcel is green"), R.ColorAt(HomeIndex).G > .6f && R.ColorAt(HomeIndex).R < .1f);
	TestTrue(TEXT("Partially served civic building is yellow"),
	         R.ColorAt(HospitalIndex).R > .9f && R.ColorAt(HospitalIndex).G > .7f);
	TestTrue(TEXT("No usable supply is red"), R.ColorAt(ShopIndex).R > .9f && R.ColorAt(ShopIndex).G < .1f);
	TestTrue(TEXT("Supplied road pipe is blue"), R.ColorAt(10 * FCityGrid::Width + 5).B > .9f);
	TestEqual(TEXT("Exact parcel delivery percent"), R.Values[HospitalIndex], 50);
	TestTrue(TEXT("Exact pressure and cause accompany color"),
	         R.DescribeCell({10, 9}).Contains(TEXT("5/10")) && R.DescribeCell({10, 9}).Contains(TEXT("30%")) &&
	             R.DescribeCell({10, 9}).Contains(TEXT("Low water pressure")));
	TestTrue(TEXT("Raised and ground pipes retain distinct pressure"),
	         R.DescribeCell({5, 10, 1}).Contains(TEXT("raised pipe: pressure 120%")) &&
	             R.DescribeCell({5, 10}).Contains(TEXT("ground pipe: pressure 80%")));
	TestEqual(TEXT("Raised-only pipe never colors an unconnected ground underpass"),
	          R.Status[10 * FCityGrid::Width + 20], uint8(0));
	TestTrue(TEXT("Raised-only pipe retains separate diagnostic"),
	         R.DescribeCell({20, 10, 1}).Contains(TEXT("pressure 60%")));
	TestTrue(TEXT("Water source outage explained"), R.DescribeCell({15, 9}).Contains(TEXT("offline")));
	TestEqual(TEXT("Low pressure links to water diagnostics"),
	          FCityDiagnosticOverlayEvaluator::OverlayForOperationFailure(ECityOperationFailure::LowWaterPressure),
	          ECityDiagnosticOverlay::Service);
	FCityDiagnosticOverlayEvaluator::Build(City, S, Needs, T, nullptr, nullptr, nullptr, Environment,
	                                       ECityDiagnosticOverlay::None, 0, R);
	TestTrue(TEXT("Changing views clears exact water details"), R.CellDetails.IsEmpty() && R.Status.IsEmpty());
	return true;
}
#endif
