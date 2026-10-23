#include "Presentation/CityUiContent.h"

#include "Presentation/CityBoardActor.h"
#include "Presentation/CityNeedsPresentation.h"
#include "Interaction/CityHudLayout.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Simulation/CityHappinessSimulation.h"
#include "Simulation/CityHousingHistory.h"

void CityUiContent::InspectionOverview(const UCitySimulationSubsystem& Simulation, uint64 EntityId,
                                       FCityUiSnapshot& Out)
{
	const auto& Population = Simulation.GetPopulationSnapshot();
	const auto Mood = FCityHappinessEvaluator::Summarize(Population, EntityId);
	Out.InspectionOverview.Reset();
	Out.InspectionOverviewIcons.Reset();
	Out.InspectionOverviewHelp.Reset();
	const auto* Entity = Simulation.GetPlacementModel().GetEntities().Find(EntityId);
	if (!Mood.Residents && (!Entity || Entity->DefinitionId != TEXT("Home")))
		return;
	Out.InspectionOverview.SetNum(3);
	Out.InspectionOverview[0] = FText::FromString(TEXT("Population: collecting trend"));
	if (Mood.Residents) Out.InspectionOverview[1] = FText::FromString(TEXT("Happiness: collecting trend"));
	int32 Seeking = 0;
	for (const auto& C : Population.Citizens)
		Seeking += C.HomeEntityId == EntityId && FCityHousingHistory::IsSearching(static_cast<uint8>(C.HousingStatus));
	if (Seeking)
		Out.InspectionOverview[2] = FText::FromString(FString::Printf(TEXT("Seeking another home: %d"), Seeking));
}

FString CityUiContent::Trend(const int32 Index, const int64 Change, const bool bKnown)
{
	return Index == 0 ? FString::Printf(TEXT("Daily net now %s$%lld"),
	                                    Change > 0   ? TEXT("+")
	                                    : Change < 0 ? TEXT("-")
	                                                 : TEXT(""),
	                                    FMath::Abs(Change))
	       : bKnown   ? FString::Printf(TEXT("Last day %s%lld people"), Change > 0 ? TEXT("+") : TEXT(""), Change)
	                  : TEXT("No population history");
}

FString CityUiContent::Budget(const UCitySimulationSubsystem& Simulation)
{
	const auto Preview = Simulation.GetMonthSettlementPreview();
	return FString::Printf(TEXT("Income now $%lld  |  Full daily bills $%lld"), Preview.Income, Preview.Expenses);
}

FString CityUiContent::LastNet(const UCitySimulationSubsystem& Simulation)
{
	const auto& Economy = Simulation.GetEconomySnapshot();
	const int64 Net = Economy.LastIncome - Economy.LastUpkeep - Economy.Loan.LastPrincipalPaid - Economy.Loan.LastInterestPaid - Economy.Policies.Paid();
	return Economy.LastBilledMonthId > 0
	           ? FString::Printf(TEXT("Last settled net %s$%lld  |  Borrowing/purchases separate"),
	                             Net >= 0 ? TEXT("+") : TEXT("-"), FMath::Abs(Net))
	           : TEXT("Completed sales + residential tax, less full bills");
}

FString CityUiContent::Objective(const FCityGuidedObjective& Card)
{
	return FString::Printf(TEXT("%s\n%s\n%s\n%s"), *Card.Title, *Card.Reason, *Card.Hint, *Card.Progress);
}

FString CityUiContent::Growth(const ACityBoardActor& Board)
{
	FString Guidance = Board.DescribeNeedsPanel();
	int32 HeadingEnd = INDEX_NONE;
	if (Guidance.FindChar(TEXT('\n'), HeadingEnd))
		Guidance = Guidance.Mid(HeadingEnd + 1);
	return Guidance;
}

FString CityUiContent::Construction(const ACityBoardActor& Board)
{
	FString Status = Board.GetConstructionStatus();
	if (Board.IsConstructionPriceBlocked() && !Status.Contains(TEXT("INSUFFICIENT FUNDS")))
		Status += TEXT("\nINSUFFICIENT FUNDS - cannot build");
	return Status;
}

void CityUiContent::NeedsGauges(const FCityNeedsSnapshot& Needs, FCityUiSnapshot& Out)
{
	Out.DemandGauges.Reset();
	Out.CapacityGauges.Reset();
	Out.DensityGauges.Reset();
	const FLinearColor Colors[] = {{.24f, .82f, .4f}, {.2f, .65f, 1.f}, {1.f, .7f, .22f}};
	const TCHAR* Families[] = {TEXT("Homes"), TEXT("Shops"), TEXT("Industry")};
	const int32 Demand[] = {Needs.ResidentialDemandBalance, Needs.CommercialDemandBalance,
	                        Needs.IndustrialDemandBalance};
	for (int32 Family = 0; Family < 3; ++Family)
	{
		FCityUiGauge Gauge;
		Gauge.Label = FText::FromString(Families[Family]);
		Gauge.Value = FText::AsNumber(Demand[Family]);
		Gauge.Fill = FCityNeedsPresentation::NormalizeSignedValue(Demand[Family], CityHudLayout::DemandVisualBound);
		Gauge.bSigned = true;
		Gauge.Color = Colors[Family];
		Out.DemandGauges.Add(Gauge);
		if (!Needs.bHasDensityDemand)
			continue;
		const auto& Market = Needs.DensityFor(static_cast<ECityDevelopmentDemandType>(Family + 1));
		int64 Bound = 1;
		for (int32 Tier = 0; Tier < 3; ++Tier)
			Bound =
			    FMath::Max<int64>(Bound, FMath::Abs(static_cast<int64>(Market.Desired[Tier]) - Market.Reserved[Tier]));
		for (int32 Tier = 0; Tier < 3; ++Tier)
		{
			const int64 Balance = static_cast<int64>(Market.Desired[Tier]) - Market.Reserved[Tier];
			Gauge.Label = FText::FromString(FString::Printf(TEXT("%s %s"), Families[Family],
			                                                Tier == 0   ? TEXT("Low")
			                                                : Tier == 1 ? TEXT("Medium")
			                                                            : TEXT("High")));
			Gauge.Value = FText::FromString(FString::Printf(TEXT("%+lld"), Balance));
			Gauge.Fill = FCityNeedsPresentation::NormalizeSignedValue(Balance, Bound);
			Out.DensityGauges.Add(Gauge);
		}
	}
	const int32 Values[] = {Needs.OccupiedResidents, Needs.EmployedJobs, Needs.PowerUsage, Needs.ServiceUsage,
	                        Needs.bUsesStoredSupplies ? FMath::Max(0, Needs.HouseholdGoodsDemand - Needs.UnmetGoodsDemand) : Needs.GoodsSold};
	const int32 Limits[] = {Needs.HousingCapacity, Needs.JobCapacity, Needs.PowerCapacity, Needs.ServiceCapacity,
	                        Needs.HouseholdGoodsDemand};
	const TCHAR* Names[] = {TEXT("Housing"), TEXT("Jobs"), TEXT("Power"), TEXT("Water"), Needs.bUsesStoredSupplies ? TEXT("Essential daily cover") : TEXT("Goods purchased")};
	for (int32 Row = 0; Row < 5; ++Row)
	{
		FCityUiGauge Gauge;
		Gauge.Label = FText::FromString(Names[Row]);
		Gauge.Value = FText::FromString(FString::Printf(TEXT("%d / %d"), Values[Row], Limits[Row]));
		Gauge.Fill = FCityNeedsPresentation::NormalizeRatioValue(Values[Row], Limits[Row]);
		Gauge.Color = Colors[Row % 3];
		Out.CapacityGauges.Add(Gauge);
	}
}

FCityUiReportChart CityUiContent::ReportChart(const TArray<FCityReportSample>& Samples, const ECityReportMetric Metric,
                                              const int32 Selected, const ECityReportPeriod Period)
{
	FCityUiReportChart Chart;
	if (!Samples.IsValidIndex(Selected))
		return Chart;
	const int32 End = FMath::Min(Samples.Num(), FMath::Max(Selected + 1, 24));
	const int32 Start = FMath::Max(0, End - 24);
	int64 Minimum = 0, Maximum = 0;
	for (int32 Index = Start; Index < End; ++Index)
	{
		Minimum = FMath::Min(Minimum, Samples[Index].Get(Metric));
		Maximum = FMath::Max(Maximum, Samples[Index].Get(Metric));
	}
	const double Range = FMath::Max(1.0, static_cast<double>(Maximum) - static_cast<double>(Minimum));
	Chart.Zero = -static_cast<double>(Minimum) / Range;
	Chart.Selected = Selected - Start;
	for (int32 Index = Start; Index < End; ++Index)
	{
		Chart.Values.Add(Samples[Index].Get(Metric));
		Chart.Periods.Add(Samples[Index].MonthId);
		Chart.Bars.Add((static_cast<double>(Samples[Index].Get(Metric)) - Minimum) / Range);
	}
	const FString PeriodLabel = FString(FCityReportHistory::PeriodName(Period)) + TEXT("s");
	Chart.FirstPeriod = FText::FromString(FString::Printf(TEXT("%s %lld"), *PeriodLabel, Samples[Start].MonthId));
	Chart.LastPeriod = FText::AsNumber(Samples[End - 1].MonthId);
	Chart.Range =
	    FText::FromString(FString::Printf(TEXT("%s %lld-%lld  |  scale %lld to %lld  |  gold = selected"), *PeriodLabel,
	                                      Samples[Start].MonthId, Samples[End - 1].MonthId, Minimum, Maximum));
	return Chart;
}
