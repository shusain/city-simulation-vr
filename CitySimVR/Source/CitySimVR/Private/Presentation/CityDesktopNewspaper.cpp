#include "Presentation/CityDesktopNewspaper.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SLeafWidget.h"

namespace
{
struct FPaperText
{
	FString Text;
	FVector2D Position;
	int32 Size = 18;
	bool Bold = false;
};
struct FPaperLayout
{
	TArray<FPaperText> Text;
	TArray<FVector4> Rules;
	float Height = 0;
	float IllustrationTop = 0;
	float IllustrationHeight = 0;
};
class SCityNewspaper : public SLeafWidget
{
  public:
	SLATE_BEGIN_ARGS(SCityNewspaper)
	{
	}
	SLATE_END_ARGS()
		void Construct(const FArguments&, TSharedRef<FCityUiNewspaper> InContent, UFont* InFont, float InWidth, float InFontScale)
	{
		Content = InContent;
		Font.Reset(InFont);
		const TCHAR* Names[] = {TEXT("Housing"), TEXT("Commerce"), TEXT("Automation")};
		for (int32 Index = 0; Index < 3; ++Index)
		{
			const FString Path = FString::Printf(TEXT("/Game/CitySim/UI/Newspaper/T_%s.T_%s"), Names[Index], Names[Index]);
			Illustrations[Index].Reset(LoadObject<UTexture2D>(nullptr, *Path));
			if (auto* Texture = Illustrations[Index].Get())
			{
				IllustrationBrushes[Index].SetResourceObject(Texture);
				IllustrationBrushes[Index].ImageType = ESlateBrushImageType::FullColor;
				IllustrationBrushes[Index].ImageSize = FVector2D(Texture->GetSizeX(), Texture->GetSizeY());
			}
		}
		PaperWidth = InWidth;
			FontScale = InFontScale;
	}
	void SetWidth(float Width)
	{
		if (!FMath::IsNearlyEqual(PaperWidth, Width))
		{
			PaperWidth = Width;
			Invalidate(EInvalidateWidgetReason::Layout);
		}
	}
	FSlateFontInfo Style(int32 Size, bool Bold) const
	{
			Size=FMath::Max(9,FMath::RoundToInt(Size*FontScale));
		return Font.IsValid() ? FSlateFontInfo(Font.Get(), Size, Bold ? TEXT("Bold") : TEXT("Regular"))
		                      : FCoreStyle::GetDefaultFontStyle(Bold ? TEXT("Bold") : TEXT("Regular"), Size);
	}
	FPaperLayout Layout(float Width) const
	{
		FPaperLayout Page;
		const auto Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		auto Paragraph = [&](const FString& Text, float X, float Y, float W, int32 Size, bool Bold = false,
		                     bool Center = false) {
			const auto Typeface = Style(Size, Bold);
			TArray<FString> Words;
			Text.ParseIntoArrayWS(Words);
			TArray<FString> Lines;
			FString Line;
			for (const auto& Word : Words)
			{
				const FString Candidate = Line.IsEmpty() ? Word : Line + TEXT(" ") + Word;
				if (!Line.IsEmpty() && Measure->Measure(Candidate, Typeface).X > W)
				{
					Lines.Add(Line);
					Line = Word;
				}
				else
					Line = Candidate;
			}
			if (!Line.IsEmpty())
				Lines.Add(Line);
			const float LineHeight = Measure->GetMaxCharacterHeight(Typeface) * 1.12f;
			for (const auto& Value : Lines)
			{
				Page.Text.Add(
				    {Value, FVector2D(X + (Center ? (W - Measure->Measure(Value, Typeface).X) * .5f : 0), Y),
				     Size, Bold});
				Y += LineHeight;
			}
			return Y;
		};
			// Leave a small gutter for scroll clipping and hinted serif overhangs.
			const float Margin = 24, Inner = Width - Margin * 2 - 24;
		float Y = 14;
		Y = Paragraph(TEXT("CITY AFFAIRS  /  HOUSING  /  THE PUBLIC PURSE"), Margin, Y, Inner, 11, false,
		              true) +
		    4;
		Y = Paragraph(Content->Masthead.ToString(), Margin, Y, Inner, 42, true, true) + 6;
		Page.Rules.Add(FVector4(Margin, Y, Inner, 3));
		Y += 6;
		Page.Rules.Add(FVector4(Margin, Y, Inner, 1));
		Y += 8;
		Y = Paragraph(Content->Dateline.ToString(), Margin, Y, Inner, 12, false, true) + 8;
		Page.Rules.Add(FVector4(Margin, Y, Inner, 1));
		Y += 14;
		Y = Paragraph(Content->Headline.ToString(), Margin, Y, Inner, 32, true) + 8;
		Y = Paragraph(Content->Lead.ToString(), Margin, Y, Inner, 18) + 16;
		// Generic editorial atmosphere; it never represents an actual city event.
		Page.IllustrationTop = Y;
		Page.IllustrationHeight = FontScale < .9f ? 52.f : 100.f;
		Y += Page.IllustrationHeight + 8;
		Page.Rules.Add(FVector4(Margin, Y, Inner, 2));
		Y += 16;
		const float Gap = 32, Col = (Inner - Gap) * .5f;
		const float ArticlesTop = Y;
		for (int32 I = 0; I < Content->Articles.Num(); I += 2)
		{
			float Bottom = Y;
			for (int32 C = 0; C < 2 && I + C < Content->Articles.Num(); ++C)
			{
				const auto& Article = Content->Articles[I + C];
				const float X = Margin + C * (Col + Gap);
				float Top = Paragraph(Article.Section.ToString(), X, Y, Col, 11) + 5;
				Top = Paragraph(Article.Headline.ToString(), X, Top, Col, 22, true) + 8;
				Top = Paragraph(Article.Body.ToString(), X, Top, Col, 17) + 16;
				Bottom = FMath::Max(Bottom, Top);
			}
			Y = Bottom;
			if (I + 2 < Content->Articles.Num())
			{
				Page.Rules.Add(FVector4(Margin, Y, Inner, 1));
				Y += 14;
			}
		}
		if (!Content->Articles.IsEmpty())
			Page.Rules.Add(FVector4(Width * .5f, ArticlesTop, 1, Y - ArticlesTop));
		Page.Rules.Add(FVector4(Margin, Y, Inner, 2));
		Y += 10;
		Y = Paragraph(Content->Footer.ToString(), Margin, Y, Inner, 12) + 16;
		Page.Height = Y;
		return Page;
	}
	virtual FVector2D ComputeDesiredSize(float) const override
	{
		return FVector2D(PaperWidth, Layout(PaperWidth).Height);
	}
	virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&,
	                      FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& WidgetStyle,
	                      bool) const override
	{
		const float W = Geometry.GetLocalSize().X;
		const auto Page = Layout(W);
		const FLinearColor Ink(.105f, .09f, .07f), Paper(.93f, .90f, .82f);
		const auto* Brush = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
		auto Box = [&](float X, float Y, float Width, float Height, FLinearColor Color) {
			FSlateDrawElement::MakeBox(
			    Elements, Layer,
			    Geometry.ToPaintGeometry(FVector2f(Width, Height), FSlateLayoutTransform(FVector2f(X, Y))),
			    Brush, ESlateDrawEffect::None, Color * WidgetStyle.GetColorAndOpacityTint());
		};
		Box(0, 0, W, FMath::Max(Page.Height, static_cast<float>(Geometry.GetLocalSize().Y)), Paper);
		const float ArtWidth = (W - 72.f - 32.f) / 3.f;
		for (int32 Index = 0; Index < 3; ++Index)
		{
			const auto* Texture = Illustrations[Index].Get();
			if (!Texture)
				continue;
			const float Scale = FMath::Min(ArtWidth / Texture->GetSizeX(), Page.IllustrationHeight / Texture->GetSizeY());
			const FVector2f Size(Texture->GetSizeX() * Scale, Texture->GetSizeY() * Scale);
			const FVector2f Position(24.f + Index * (ArtWidth + 16.f) + (ArtWidth - Size.X) * .5f,
			                         Page.IllustrationTop + (Page.IllustrationHeight - Size.Y) * .5f);
			FSlateDrawElement::MakeBox(Elements, Layer + 1,
			    Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(Position)), &IllustrationBrushes[Index],
			    ESlateDrawEffect::None, Ink * WidgetStyle.GetColorAndOpacityTint());
		}
		for (const auto& R : Page.Rules)
			Box(R.X, R.Y, R.Z, R.W, Ink * .8f);
		for (const auto& Text : Page.Text)
			FSlateDrawElement::MakeText(
			    Elements, Layer + 1,
			    Geometry.ToPaintGeometry(FVector2f(W, 40), FSlateLayoutTransform(FVector2f(Text.Position))),
			    Text.Text, Style(Text.Size, Text.Bold), ESlateDrawEffect::None,
			    Ink * WidgetStyle.GetColorAndOpacityTint());
		return Layer + 1;
	}

  private:
	TSharedRef<FCityUiNewspaper> Content = MakeShared<FCityUiNewspaper>();
	TStrongObjectPtr<UFont> Font;
	TStrongObjectPtr<UTexture2D> Illustrations[3];
	FSlateBrush IllustrationBrushes[3];
	float PaperWidth = 920;
		float FontScale = 1;
};
bool Same(const FCityUiNewspaper& A, const FCityUiNewspaper& B)
{
	if (!A.Dateline.EqualTo(B.Dateline) || !A.Headline.EqualTo(B.Headline) || !A.Lead.EqualTo(B.Lead) ||
	    !A.Footer.EqualTo(B.Footer) || A.Articles.Num() != B.Articles.Num())
		return false;
	for (int32 I = 0; I < A.Articles.Num(); ++I)
		if (!A.Articles[I].Headline.EqualTo(B.Articles[I].Headline) ||
		    !A.Articles[I].Body.EqualTo(B.Articles[I].Body))
			return false;
	return A.Masthead.EqualTo(B.Masthead);
}
} // namespace
TSharedRef<SWidget> CityNewspaper::MakeSheet(TSharedRef<FCityUiNewspaper> Content, UFont* Font, float Width, float FontScale)
{
	return SNew(SCityNewspaper, Content, Font, Width, FontScale);
}
bool UCityDesktopNewspaper::SetEdition(const FCityUiNewspaper& InEdition)
{
	if (Same(*Content, InEdition))
		return false;
	*Content = InEdition;
	if (Sheet)
		Sheet->Invalidate(EInvalidateWidgetReason::Layout | EInvalidateWidgetReason::Paint);
	return true;
}
void UCityDesktopNewspaper::SetPaperWidth(float Width)
{
	PaperWidth = Width;
	if (Sheet)
		StaticCastSharedPtr<SCityNewspaper>(Sheet)->SetWidth(Width);
}
TSharedRef<SWidget> UCityDesktopNewspaper::RebuildWidget()
{
	Sheet = CityNewspaper::MakeSheet(Content, Typeface, PaperWidth);
	return Sheet.ToSharedRef();
}
void UCityDesktopNewspaper::ReleaseSlateResources(bool Children)
{
	Super::ReleaseSlateResources(Children);
	Sheet.Reset();
}
