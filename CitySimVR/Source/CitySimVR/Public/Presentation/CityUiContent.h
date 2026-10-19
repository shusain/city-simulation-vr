#pragma once

#include "CoreMinimal.h"
#include "CityUiContent.generated.h"

class ACityBoardActor;
class UCitySimulationSubsystem;
struct FCityGuidedObjective;
struct FCityNeedsSnapshot;
struct FCityReportSample;
enum class ECityReportMetric : uint8;

USTRUCT(BlueprintType)
struct CITYSIMVR_API FCityUiNewsArticle
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FText Section;
	UPROPERTY(BlueprintReadOnly) FText Headline;
	UPROPERTY(BlueprintReadOnly) FText Body;
};
USTRUCT(BlueprintType)
struct CITYSIMVR_API FCityUiNewspaper
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FText Masthead;
	UPROPERTY(BlueprintReadOnly) FText Dateline;
	UPROPERTY(BlueprintReadOnly) FText Headline;
	UPROPERTY(BlueprintReadOnly) FText Lead;
	UPROPERTY(BlueprintReadOnly) FText Footer;
	UPROPERTY(BlueprintReadOnly) TArray<FCityUiNewsArticle> Articles;
};

USTRUCT(BlueprintType)
struct CITYSIMVR_API FCityUiGauge
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly)
	FText Label;
	UPROPERTY(BlueprintReadOnly)
	FText Value;
	UPROPERTY(BlueprintReadOnly)
	double Fill = 0;
	UPROPERTY(BlueprintReadOnly)
	bool bSigned = false;
	UPROPERTY(BlueprintReadOnly)
	FLinearColor Color = FLinearColor::White;
};

// The selected report window, scale and normalized bars are identical in VR/desktop.
USTRUCT(BlueprintType)
struct CITYSIMVR_API FCityUiReportChart
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly)
	TArray<double> Bars;
	UPROPERTY(BlueprintReadOnly)
	TArray<int64> Values;
	UPROPERTY(BlueprintReadOnly)
	TArray<int64> Periods;
	UPROPERTY(BlueprintReadOnly)
	int32 Selected = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly)
	double Zero = 0;
	UPROPERTY(BlueprintReadOnly)
	FText Range;
	UPROPERTY(BlueprintReadOnly)
	FText FirstPeriod;
	UPROPERTY(BlueprintReadOnly)
	FText LastPeriod;
};

// Semantic content is shared; each presenter owns its layout, fonts and hit targets.
USTRUCT(BlueprintType)
struct CITYSIMVR_API FCityUiActionView
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly)
	int32 Index = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly)
	int32 ActionId = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly)
	FText Label;
	UPROPERTY(BlueprintReadOnly)
	FText Help;
	UPROPERTY(BlueprintReadOnly)
	bool bEnabled = true;
	UPROPERTY(BlueprintReadOnly)
	bool bSelected = false;
};

USTRUCT(BlueprintType)
struct CITYSIMVR_API FCityUiInfoCard
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly)
	FText Title;
	UPROPERTY(BlueprintReadOnly)
	FText Labels;
	UPROPERTY(BlueprintReadOnly)
	FText Values;
};

USTRUCT(BlueprintType)
struct CITYSIMVR_API FCityUiInspectionSection
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly)
	FText Title;
	UPROPERTY(BlueprintReadOnly)
	FText Detail;
	UPROPERTY(BlueprintReadOnly)
	FText Help;
	UPROPERTY(BlueprintReadOnly)
	TArray<FCityUiGauge> Gauges;
	UPROPERTY(BlueprintReadOnly)
	TArray<FCityUiGauge> Rows;
};

USTRUCT(BlueprintType)
struct CITYSIMVR_API FCityUiSnapshot
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly)
	bool bEntryScene = false;
	UPROPERTY(BlueprintReadOnly)
	bool bMenuOpen = false;
	UPROPERTY(BlueprintReadOnly)
	bool bCityInfoOpen = false;
	UPROPERTY(BlueprintReadOnly)
	bool bObjectivesOpen = false;
	UPROPERTY(BlueprintReadOnly)
	bool bUnaffordable = false;
	UPROPERTY(BlueprintReadOnly)
	bool bUpgradeAvailable = false;
	UPROPERTY(BlueprintReadOnly)
	bool bDetailedMenu = false;
	UPROPERTY(BlueprintReadOnly)
	bool bRoadTool = false;
	UPROPERTY(BlueprintReadOnly)
	bool bPlacementTool = false;
	UPROPERTY(BlueprintReadOnly)
	bool bCanCancelTool = false;
	UPROPERTY(BlueprintReadOnly)
	bool bPaused = true;
	UPROPERTY(BlueprintReadOnly)
	int32 SpeedMultiplier = 1;
	UPROPERTY(BlueprintReadOnly)
	bool bZoneMenu = false;
	UPROPERTY(BlueprintReadOnly)
	bool bTaxMenu = false;
	UPROPERTY(BlueprintReadOnly)
	bool bNewspaper = false;
	UPROPERTY(BlueprintReadOnly)
	FCityUiNewspaper Newspaper;
	UPROPERTY(BlueprintReadOnly)
	TArray<FCityUiGauge> TaxRates;
	UPROPERTY(BlueprintReadOnly)
	FText Funds;
	UPROPERTY(BlueprintReadOnly)
	FText Population;
	UPROPERTY(BlueprintReadOnly)
	FText Weather;
	UPROPERTY(BlueprintReadOnly)
	FText ObjectiveTitle;
	UPROPERTY(BlueprintReadOnly)
	FText ObjectiveProgress;
	UPROPERTY(BlueprintReadOnly)
	FText InspectionTitle;
	UPROPERTY(BlueprintReadOnly)
	FText InspectionSummary;
	UPROPERTY(BlueprintReadOnly)
	TArray<FCityUiInspectionSection> InspectionSections;
	UPROPERTY(BlueprintReadOnly)
	TArray<FText> InspectionOverview;
	UPROPERTY(BlueprintReadOnly)
	TArray<FName> InspectionOverviewIcons;
	UPROPERTY(BlueprintReadOnly)
	TArray<FText> InspectionOverviewHelp;
	UPROPERTY(BlueprintReadOnly)
	TArray<FCityUiGauge> InspectionGauges;
	UPROPERTY(BlueprintReadOnly)
	FText MenuTitle;
	UPROPERTY(BlueprintReadOnly)
	FText MenuBody;
	UPROPERTY(BlueprintReadOnly)
	FText Calendar;
	UPROPERTY(BlueprintReadOnly)
	FText Totals;
	UPROPERTY(BlueprintReadOnly)
	FText Trends;
	UPROPERTY(BlueprintReadOnly)
	FText Budget;
	UPROPERTY(BlueprintReadOnly)
	FText LastNet;
	UPROPERTY(BlueprintReadOnly)
	FText Tool;
	UPROPERTY(BlueprintReadOnly)
	FText Construction;
	UPROPERTY(BlueprintReadOnly)
	FText Growth;
	UPROPERTY(BlueprintReadOnly)
	FText Objectives;
	UPROPERTY(BlueprintReadOnly)
	FText Inspection;
	UPROPERTY(BlueprintReadOnly)
	FText Upgrade;
	UPROPERTY(BlueprintReadOnly)
	TArray<FCityUiActionView> MenuEntries;
	UPROPERTY(BlueprintReadOnly)
	TArray<FCityUiActionView> ToolbarEntries;
	UPROPERTY(BlueprintReadOnly)
	TArray<FCityUiInfoCard> CityCards;
	UPROPERTY(BlueprintReadOnly)
	TArray<FCityUiGauge> DemandGauges;
	UPROPERTY(BlueprintReadOnly)
	TArray<FCityUiGauge> CapacityGauges;
	UPROPERTY(BlueprintReadOnly)
	TArray<FCityUiGauge> DensityGauges;
	UPROPERTY(BlueprintReadOnly)
	FCityUiReportChart ReportChart;
};

namespace CityUiContent
{
	CITYSIMVR_API void InspectionOverview(const UCitySimulationSubsystem& Simulation, uint64 EntityId,
	                                     FCityUiSnapshot& Out);
	CITYSIMVR_API void InspectionDetails(const UCitySimulationSubsystem& Simulation, uint64 EntityId,
	                                    FCityUiSnapshot& Out);
	CITYSIMVR_API FString Budget(const UCitySimulationSubsystem& Simulation);
	CITYSIMVR_API FString LastNet(const UCitySimulationSubsystem& Simulation);
	CITYSIMVR_API FString Objective(const FCityGuidedObjective& Card);
	CITYSIMVR_API FString Growth(const ACityBoardActor& Board);
	CITYSIMVR_API FString Construction(const ACityBoardActor& Board);
	CITYSIMVR_API FString Trend(int32 Index, int64 Change, bool bKnown);
	CITYSIMVR_API void NeedsGauges(const FCityNeedsSnapshot& Needs, FCityUiSnapshot& Out);
	CITYSIMVR_API FCityUiReportChart ReportChart(const TArray<FCityReportSample>& Samples, ECityReportMetric Metric,
	                                             int32 Selected, bool bAnnual);
} // namespace CityUiContent
