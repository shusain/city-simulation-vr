#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Presentation/CityDiagnosticOverlay.h"
#include "Presentation/CityNeedsPresentation.h"
#include "Simulation/CityEconomySimulation.h"
#include "Simulation/CityEnvironmentSimulation.h"
#include "Simulation/CityGrowthSimulation.h"
#include "Simulation/CityNeedsSimulation.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityNeedsPresentationTest, "CitySim.Presentation.NeedsPanelAndOverlays",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityNeedsPresentationTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("ratio bar explicitly labels zero capacity"),
	         FCityNeedsPresentation::BuildRatioBar(12, 0).Contains(TEXT("NO CAP")));
	TestEqual(TEXT("ratio bar visually clamps values over capacity"),
	          FCityNeedsPresentation::BuildRatioBar(250, 100, 4), FString(TEXT("[####]")));
	TestEqual(TEXT("positive signed bar fills only right of zero"), FCityNeedsPresentation::BuildSignedBar(50, 100, 4),
	          FString(TEXT("[....|>>..]")));
	TestEqual(TEXT("negative signed bar fills only left of zero"), FCityNeedsPresentation::BuildSignedBar(-50, 100, 4),
	          FString(TEXT("[..<<|....]")));
	TestEqual(TEXT("visual signed fill preserves positive direction"),
	          FCityNeedsPresentation::NormalizeSignedValue(20, 40), 0.5);
	TestEqual(TEXT("visual signed fill preserves negative direction"),
	          FCityNeedsPresentation::NormalizeSignedValue(-20, 40), -0.5);
	TestEqual(TEXT("visual signed fill clamps beyond its bound"), FCityNeedsPresentation::NormalizeSignedValue(80, 40),
	          1.0);
	TestEqual(TEXT("visual capacity fill preserves utilization"), FCityNeedsPresentation::NormalizeRatioValue(75, 100),
	          0.75);
	TestEqual(TEXT("visual capacity fill clamps over capacity"), FCityNeedsPresentation::NormalizeRatioValue(120, 100),
	          1.0);
	TestEqual(TEXT("visual capacity fill stays empty without capacity"),
	          FCityNeedsPresentation::NormalizeRatioValue(12, 0), 0.0);

	FCityNeedsSnapshot Needs;
	Needs.OccupiedResidents = 120;
	Needs.HousingCapacity = 100;
	Needs.HousingShortage = 20;
	Needs.PowerDemand = 30;
	Needs.PowerUsage = 20;
	Needs.PowerCapacity = 20;
	Needs.PowerShortage = 10;
	Needs.ResidentialDemand = 40;
	Needs.ResidentialDemandBalance = -20;
	Needs.CommercialDemandBalance = 20;
	Needs.ResidentialConstraint = ECityDemandConstraint::PowerCapacity;
	Needs.AvailableConstructionFunds = 75;
	Needs.ResidentialGrowthCost = 100;
	Needs.CommercialGrowthCost = 150;
	Needs.IndustrialGrowthCost = 200;
	FCityEconomySnapshot Economy;
	Economy.LastIncome = 25;
	Economy.LastUpkeep = 75;
	const FString Panel = FCityNeedsPresentation::BuildPanel(Needs, Economy);
	TestTrue(TEXT("panel names each active demand constraint"),
	         Panel.Contains(TEXT("GROW HOME:POWER FULL SHOP:LOW DEMAND IND:LOW DEMAND")));
	TestTrue(TEXT("panel identifies private development"), Panel.Contains(TEXT("PRIVATE DEVELOPMENT")));
	TestFalse(TEXT("panel does not imply treasury funds zoned growth"), Panel.Contains(TEXT("CASH $75")));
	TestTrue(TEXT("panel retains only actionable shortage text"), Panel.Contains(TEXT("SHORT HOME:20 PWR:10")));
	TestFalse(TEXT("panel no longer duplicates capacity bar values"), Panel.Contains(TEXT("HOU 120/100")));
	TestFalse(TEXT("panel no longer duplicates demand bar values"), Panel.Contains(TEXT("R DEM")));
	TestTrue(TEXT("panel shows signed monthly loss"), Panel.Contains(TEXT("NET -$50")));
	Needs.ResidentialConstraint = ECityDemandConstraint::InsufficientFunds;
	const FString FundsPanel = FCityNeedsPresentation::BuildPanel(Needs, Economy);
	TestTrue(TEXT("funds blocker names the required construction amount"), FundsPanel.Contains(TEXT("HOME:NEED $100")));
	Needs.ResidentialConstraint = ECityDemandConstraint::InvalidTerrain;
	const FString TerrainPanel = FCityNeedsPresentation::BuildPanel(Needs, Economy);
	TestTrue(TEXT("terrain blocker is never mislabeled as construction cash"),
	         TerrainPanel.Contains(TEXT("HOME:BAD TERRAIN")));
	Needs.ResidentialConstraint = ECityDemandConstraint::OperatingShortfall;
	Economy.UpkeepShortfall = 258;
	Economy.bGrowthBlockedByShortfall = true;
	FCityBudgetProjection Budget;
	Budget.RoadUpkeep = 240;
	Budget.UtilityUpkeep = 160;
	Budget.BuildingUpkeep = 85;
	Budget.PublicPayroll = 40;
	Budget.Income = 267;
	Budget.Expenses = 525;
	const FString RecoveryPanel = FCityNeedsPresentation::BuildPanel(Needs, Economy, 0, 0, &Budget);
	TestTrue(TEXT("Upkeep blocker identifies unpaid bills and exact amount"),
	         RecoveryPanel.Contains(TEXT("HOME:UNPAID BILLS")) &&
	             RecoveryPanel.Contains(TEXT("Unpaid last day $258")));
	TestTrue(TEXT("Recovery names controllable costs"), RecoveryPanel.Contains(TEXT("roads $240 | utilities $160")) &&
	                                                        RecoveryPanel.Contains(TEXT("public wages $40")));
	TestTrue(TEXT("Recovery gives actual policy navigation and billing timing"),
	         RecoveryPanel.Contains(TEXT("City > TAXES")) && RecoveryPanel.Contains(TEXT("then Apply")) &&
	             RecoveryPanel.Contains(TEXT("day is fully paid")));
	TestTrue(TEXT("Current forecast is visibly separate from last month"),
	         RecoveryPanel.Contains(TEXT("Forecast -$258/day")) && RecoveryPanel.Contains(TEXT("LAST DAY")));

	TestEqual(TEXT("service blocker selects service overlay"),
	          FCityDiagnosticOverlayEvaluator::OverlayForOperationFailure(ECityOperationFailure::ServiceCapacity),
	          ECityDiagnosticOverlay::Service);
	TestEqual(TEXT("power blocker selects power overlay"),
	          FCityDiagnosticOverlayEvaluator::OverlayForOperationFailure(ECityOperationFailure::NoPower),
	          ECityDiagnosticOverlay::Power);

	FCityPlacementModel City(1000);
	FCityEconomyTuning Tuning;
	const FCityServiceSnapshot Services = FCityServiceEvaluator::BuildSnapshot(City, Tuning);
	FCityDiagnosticOverlayResult Result;
	Result.PositiveCells.Add(FCityCellCoord(1, 1));
	Result.WarningCells.Add(FCityCellCoord(2, 2));
	FCityDiagnosticOverlayEvaluator::Build(City, Services, Needs, Tuning, nullptr, nullptr, nullptr,
	                                       FCityEnvironmentSnapshot(), ECityDiagnosticOverlay::None,
	                                       FCityGrid::EmptyEntityId, Result);
	TestTrue(TEXT("switching overlay off clears stale positive instances"), Result.PositiveCells.IsEmpty());
	TestTrue(TEXT("switching overlay off clears stale warning instances"), Result.WarningCells.IsEmpty());
	TestEqual(TEXT("cleared overlay reports off"), Result.Description, FString(TEXT("Off")));

	FCityEnvironmentSnapshot Environment;
	Environment.Pollution[1 * FCityGrid::Width + 1] = 10;
	Environment.Pollution[2 * FCityGrid::Width + 2] = 250;
	FCityDiagnosticOverlayEvaluator::Build(City, Services, Needs, Tuning, nullptr, nullptr, nullptr, Environment,
	                                       ECityDiagnosticOverlay::Pollution, FCityGrid::EmptyEntityId, Result);
	TestTrue(TEXT("low pollution is visible as an informational cell"),
	         Result.PositiveCells.Contains(FCityCellCoord(1, 1)));
	TestTrue(TEXT("residential pressure threshold is visible as a warning cell"),
	         Result.WarningCells.Contains(FCityCellCoord(2, 2)));
	TestEqual(TEXT("pollution overlay has an explicit label"), Result.Description, FString(TEXT("Pollution exposure")));
	return true;
}

#endif
