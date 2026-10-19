#include "Presentation/CityDesktopTerrainPreview.h"

#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SLeafWidget.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"

namespace
{
	class SCityDesktopTerrainPreview : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SCityDesktopTerrainPreview) {}
		SLATE_END_ARGS()
		void Construct(const FArguments&, UCityDesktopTerrainPreview* InOwner)
		{
			Owner = InOwner;
		}
		virtual FVector2D ComputeDesiredSize(float) const override
		{
			return FVector2D(300, 380);
		}
		virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&,
		                      FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style,
		                      bool) const override
		{
			const auto* Data = Owner.Get();
			if (!Data || !Data->GetParameters().IsSet())
				return Layer;
			const float Width = Geometry.GetLocalSize().X;
			const float Side = FMath::Max(1.f, FMath::Min(Width - 20, Geometry.GetLocalSize().Y - 148));
			const float Left = (Width - Side) * .5f, Top = 32;
			const auto* Brush = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
			const FLinearColor Tint = Style.GetColorAndOpacityTint();
			auto Box = [&](float X, float Y, float W, float H, FLinearColor Color)
			{
				FSlateDrawElement::MakeBox(
				    Elements, Layer, Geometry.ToPaintGeometry(FVector2f(W, H), FSlateLayoutTransform(FVector2f(X, Y))),
				    Brush, ESlateDrawEffect::None, Color * Tint);
			};
			auto Text = [&](float Y, const FString& Value, int32 Size)
			{
				const auto Font = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), Size);
				const auto Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
				TArray<FString> SourceLines;
				Value.ParseIntoArrayLines(SourceLines);
				int32 Row = 0;
				auto DrawLine = [&](const FString& Line)
				{
					FSlateDrawElement::MakeText(
					    Elements, Layer + 1,
					    Geometry.ToPaintGeometry(FVector2f(Width - 20, Size + 4),
					                             FSlateLayoutTransform(FVector2f(10, Y + Row++ * (Size + 3)))),
					    Line, Font, ESlateDrawEffect::None, FLinearColor(.9f, .94f, 1) * Tint);
				};
				for (const auto& SourceLine : SourceLines)
				{
					TArray<FString> Words;
					SourceLine.ParseIntoArray(Words, TEXT(" "), true);
					FString Line;
					for (const auto& Word : Words)
					{
						const FString Candidate = Line.IsEmpty() ? Word : Line + TEXT(" ") + Word;
						if (!Line.IsEmpty() && Measure->Measure(Candidate, Font).X > Width - 20)
						{
							DrawLine(Line);
							Line = Word;
						}
						else
							Line = Candidate;
					}
					DrawLine(Line);
				}
			};
			Text(4, TEXT("Map preview"), 16);
			const auto& Parameters = Data->GetParameters().GetValue();
			const int32 Columns = Parameters.Dimensions.GetWidth(), Rows = Parameters.Dimensions.GetHeight();
			const float StepX = Side / Columns, StepY = Side / Rows;
			for (int32 Y = 0; Y < Rows; ++Y)
				for (int32 X = 0; X < Columns; ++X)
					if (Data->GetCells().IsValidIndex(Y * Columns + X))
						// Match VR orientation: X increases right, Y increases up.
						Box(Left + X * StepX, Top + (Rows - 1 - Y) * StepY, StepX + .25f, StepY + .25f,
						    Data->GetCells()[Y * Columns + X]);
			if (!Data->GetCells().IsEmpty())
			{
				const auto Entry = Parameters.GetEntranceCell();
				const float X = Left + (Entry.X + .5f) * StepX, Y = Top + (Rows - Entry.Y - .5f) * StepY;
				Box(X - 4, Y - 4, 8, 8, FLinearColor::Black);
				Box(X - 3, Y - 3, 6, 6, FLinearColor::White);
			}
			Text(Top + Side + 8, Data->GetLegend(), 11);
			return Layer + 1;
		}

	private:
		TWeakObjectPtr<UCityDesktopTerrainPreview> Owner;
	};
} // namespace

void UCityDesktopTerrainPreview::Present(const FCityTerrainGenerationParameters* Parameters)
{
	if (!Parameters || (CachedParameters.IsSet() && CachedParameters.GetValue() == *Parameters))
		return;
	CachedParameters = *Parameters;
	Cells.Reset();
	FCityTerrainHeightfield Terrain;
	if (!Terrain.Generate(*Parameters))
		Legend = TEXT("Terrain unavailable");
	else
	{
		double Minimum = MAX_int32, Maximum = MIN_int32;
		int32 Water = 0, Steep = 0;
		const auto Dimensions = Terrain.GetDimensions();
		for (int32 Y = 0; Y < Dimensions.GetHeight(); ++Y)
			for (int32 X = 0; X < Dimensions.GetWidth(); ++X)
			{
				const double Height = Terrain.GetCellCenterHeightCm({X, Y});
				Minimum = FMath::Min(Minimum, Height);
				Maximum = FMath::Max(Maximum, Height);
			}
		for (int32 Y = 0; Y < Dimensions.GetHeight(); ++Y)
			for (int32 X = 0; X < Dimensions.GetWidth(); ++X)
			{
				const FCityCellCoord Cell(X, Y);
				const bool bWater = Terrain.IsWaterCell(Cell),
				           bSteep = !bWater && Terrain.GetCellSlope(Cell) == ECityTerrainSlope::Steep;
				Water += bWater;
				Steep += bSteep;
				const float Height =
				    (Terrain.GetCellCenterHeightCm(Cell) - Minimum) / FMath::Max(1.0, Maximum - Minimum);
				Cells.Add(bWater   ? FLinearColor(.12f, .46f, .92f)
				          : bSteep ? FLinearColor(.95f, .78f, .08f)
				                   : FLinearColor(.06f + .14f * Height, .22f + .40f * Height, .07f + .13f * Height));
			}
		Legend = FString::Printf(
		    TEXT("%s\nBlue water %d%% | Gold steep %d%%\nGreen height: dark = low\nWhite entry | Terrain only"),
		    *Parameters->DescribeLandscape(), Water * 100 / Dimensions.GetCellCount(),
		    Steep * 100 / Dimensions.GetCellCount());
	}
	if (PreviewWidget)
		PreviewWidget->Invalidate(EInvalidateWidgetReason::Paint);
}

TSharedRef<SWidget> UCityDesktopTerrainPreview::RebuildWidget()
{
	PreviewWidget = SNew(SCityDesktopTerrainPreview, this);
	return PreviewWidget.ToSharedRef();
}

void UCityDesktopTerrainPreview::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	PreviewWidget.Reset();
}
