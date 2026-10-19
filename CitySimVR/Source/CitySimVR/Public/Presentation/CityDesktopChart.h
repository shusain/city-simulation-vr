#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "Presentation/CityUiContent.h"
#include "CityDesktopChart.generated.h"

// Blueprint places this widget; the shared snapshot owns its values and scale.
UCLASS()
class CITYSIMVR_API UCityDesktopChart : public UWidget
{
	GENERATED_BODY()
public:
	void SetGauges(const TArray<FCityUiGauge>& InRows, const FString& InTitle, const FString& InLegend);
	void SetTable(const TArray<FCityUiGauge>& InRows);
	bool IsTable() const { return bTable; }
	void SetReport(const FCityUiReportChart& InReport);
	const TArray<FCityUiGauge>& GetRows() const
	{
		return Rows;
	}
	const FCityUiReportChart& GetReport() const
	{
		return Report;
	}
	const FString& GetTitle() const
	{
		return Title;
	}
	const FString& GetLegend() const
	{
		return Legend;
	}
	bool IsReport() const
	{
		return bReport;
	}
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	TArray<FCityUiGauge> Rows;
	FCityUiReportChart Report;
	FString Title, Legend;
	bool bReport = false;
	bool bTable = false;
	TSharedPtr<SWidget> ChartWidget;
};
