#include "Presentation/CityDesktopChart.h"

#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SLeafWidget.h"

namespace
{
	class SCityDesktopChart : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SCityDesktopChart) {}
		SLATE_END_ARGS()
		void Construct(const FArguments&, UCityDesktopChart* InOwner)
		{
			Owner = InOwner;
		}
		virtual FVector2D ComputeDesiredSize(float) const override
		{
			const auto* Data = Owner.Get();
			return FVector2D(280, Data && Data->IsTable() ? 8 + Data->GetRows().Num() * 26 : Data && !Data->IsReport()
			                          ? (Data->GetTitle().IsEmpty() ? 8 : 34) + Data->GetRows().Num() * 36 +
			                                (Data->GetLegend().IsEmpty() ? 8 : 30)
			                          : 230);
		}
		virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&,
		                      FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style,
		                      bool) const override
		{
			const auto* Data = Owner.Get();
			if (!Data)
				return Layer;
			const float Width = Geometry.GetLocalSize().X;
			const float Height = Geometry.GetLocalSize().Y;
			const auto* Brush = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
			const FLinearColor Tint = Style.GetColorAndOpacityTint();
			auto Box = [&](float X, float Y, float W, float H, FLinearColor Color)
			{
				if (W > 0 && H > 0)
					FSlateDrawElement::MakeBox(
					    Elements, Layer,
					    Geometry.ToPaintGeometry(FVector2f(W, H), FSlateLayoutTransform(FVector2f(X, Y))), Brush,
					    ESlateDrawEffect::None, Color * Tint);
			};
			auto Text = [&](float X, float Y, const FString& Value, int32 Size = 12)
			{
				FSlateDrawElement::MakeText(
				    Elements, Layer + 1,
				    Geometry.ToPaintGeometry(FVector2f(Width - X, 20), FSlateLayoutTransform(FVector2f(X, Y))), Value,
				    FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), Size), ESlateDrawEffect::None,
				    FLinearColor(.9f, .94f, 1) * Tint);
			};
			Box(0, 0, Width, Height, FLinearColor(.016f, .032f, .048f));
			if (Data->IsTable())
			{
				for (int32 I = 0; I < Data->GetRows().Num(); ++I)
				{
					const float Y = 4 + I * 26;
					if ((I % 2) == 0)
						Box(0, Y, Width, 26, FLinearColor(.035f, .06f, .08f));
					Text(8, Y + 4, Data->GetRows()[I].Label.ToString());
					Text(Width * .48f, Y + 4, Data->GetRows()[I].Value.ToString());
				}
			}
			else if (!Data->IsReport())
			{
				if (!Data->GetTitle().IsEmpty())
					Text(10, 8, Data->GetTitle(), 14);
				for (int32 Index = 0; Index < Data->GetRows().Num(); ++Index)
				{
					const auto& Row = Data->GetRows()[Index];
					const float Y = (Data->GetTitle().IsEmpty() ? 6 : 34) + Index * 36;
					Text(10, Y, Row.Label.ToString());
					Text(Width * .61f, Y, Row.Value.ToString());
					const float Rail = Width - 20;
					Box(10, Y + 21, Rail, 7, FLinearColor(.13f, .19f, .25f));
					const float Zero = Row.bSigned ? Rail * .5f : 0;
					const float End = Zero + Row.Fill * Rail * (Row.bSigned ? .5f : 1.f);
					Box(10 + FMath::Min(Zero, End), Y + 21, FMath::Abs(End - Zero), 7, Row.Color);
					if (Row.bSigned)
						Box(10 + Zero, Y + 19, 1, 11, FLinearColor(.75f, .8f, .85f));
				}
				if (!Data->GetLegend().IsEmpty())
					Text(10, Height - 25, Data->GetLegend(), 11);
			}
			else
			{
				const auto& Report = Data->GetReport();
				const float PlotX = 12, PlotY = 14, PlotWidth = Width - 24, PlotHeight = Height - 60;
				const float ZeroY = PlotY + (1 - Report.Zero) * PlotHeight;
				for (int32 Index = 0; Index < Report.Bars.Num(); ++Index)
				{
					const float Step = PlotWidth / Report.Bars.Num();
					const float ValueY = PlotY + (1 - Report.Bars[Index]) * PlotHeight;
					const bool Selected = Index == Report.Selected;
					Box(PlotX + Index * Step + Step * .12f, FMath::Min(ZeroY, ValueY), Step * .76f,
					    FMath::Max(1.f, FMath::Abs(ValueY - ZeroY)),
					    Selected ? FLinearColor(1, .7f, .22f) : FLinearColor(.2f, .65f, 1));
				}
				Box(PlotX, ZeroY, PlotWidth, 1, FLinearColor(.6f, .68f, .75f));
				Text(12, Height - 42, Report.FirstPeriod.ToString());
				Text(Width - 60, Height - 42, Report.LastPeriod.ToString());
				Text(12, Height - 23, TEXT("Gold: selected period | zero line shown"), 11);
			}
			return Layer + 1;
		}

	private:
		TWeakObjectPtr<UCityDesktopChart> Owner;
	};
} // namespace

void UCityDesktopChart::SetGauges(const TArray<FCityUiGauge>& InRows, const FString& InTitle, const FString& InLegend)
{
	bool Changed = bReport || bTable || Title != InTitle || Legend != InLegend || Rows.Num() != InRows.Num();
	for (int32 Index = 0; !Changed && Index < Rows.Num(); ++Index)
		Changed = !Rows[Index].Label.EqualTo(InRows[Index].Label) || !Rows[Index].Value.EqualTo(InRows[Index].Value) ||
		          Rows[Index].Fill != InRows[Index].Fill || Rows[Index].bSigned != InRows[Index].bSigned ||
		          Rows[Index].Color != InRows[Index].Color;
	if (!Changed)
		return;
	bReport = false;
	bTable = false;
	Rows = InRows;
	Title = InTitle;
	Legend = InLegend;
	if (ChartWidget)
		ChartWidget->Invalidate(EInvalidateWidgetReason::Layout | EInvalidateWidgetReason::Paint);
}

void UCityDesktopChart::SetTable(const TArray<FCityUiGauge>& InRows)
{
	const bool bWasTable = bTable;
	bTable = false;
	SetGauges(InRows, TEXT(""), TEXT(""));
	bTable = true;
	if (ChartWidget && !bWasTable)
		ChartWidget->Invalidate(EInvalidateWidgetReason::Layout | EInvalidateWidgetReason::Paint);
}

void UCityDesktopChart::SetReport(const FCityUiReportChart& InReport)
{
	if (bReport && Report.Bars == InReport.Bars && Report.Selected == InReport.Selected &&
	    Report.Zero == InReport.Zero && Report.Range.EqualTo(InReport.Range))
		return;
	bReport = true;
	bTable = false;
	Report = InReport;
	if (ChartWidget)
		ChartWidget->Invalidate(EInvalidateWidgetReason::Layout | EInvalidateWidgetReason::Paint);
}

TSharedRef<SWidget> UCityDesktopChart::RebuildWidget()
{
	ChartWidget = SNew(SCityDesktopChart, this);
	return ChartWidget.ToSharedRef();
}

void UCityDesktopChart::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	ChartWidget.Reset();
}
