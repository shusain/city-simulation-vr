#include "Presentation/CityDesktopIcon.h"

#include "Interaction/CityPaletteModel.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SLeafWidget.h"
#include "Engine/Texture2D.h"

class SCityDesktopIcon : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SCityDesktopIcon) {}
	SLATE_END_ARGS()
	void Construct(const FArguments&, UCityDesktopIcon* InOwner)
	{
		Owner = InOwner;
	}
	virtual FVector2D ComputeDesiredSize(float) const override
	{
		return FVector2D(28, 28);
	}
	virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&,
	                      FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style,
	                      bool bEnabled) const override
	{
		const auto* Data = Owner.Get();
		if (!Data)
			return Layer;
		const FLinearColor Modulation = Style.GetColorAndOpacityTint() * (bEnabled ? 1.f : .45f);
		FLinearColor Color = Data->Tint * Modulation;
		if (const auto* Brush = Data->GetConditionBrush())
		{
			FSlateDrawElement::MakeBox(Elements, Layer, Geometry.ToPaintGeometry(), Brush,
			                           ESlateDrawEffect::None, Color);
			return Layer;
		}
		auto Ink = [&](const FLinearColor Value) { Color = Value * Modulation; };
		const float Scale = FMath::Min(Geometry.GetLocalSize().X, Geometry.GetLocalSize().Y) / 32.f;
		const FVector2f Offset((Geometry.GetLocalSize().X - Scale * 32) * .5f,
		                       (Geometry.GetLocalSize().Y - Scale * 32) * .5f);
		auto Lines = [&](std::initializer_list<FVector2f> Input)
		{
			TArray<FVector2f> Points;
			for (const auto& P : Input)
				Points.Add(Offset + P * Scale);
			FSlateDrawElement::MakeLines(Elements, Layer, Geometry.ToPaintGeometry(), Points, ESlateDrawEffect::None,
			                             Color, true, FMath::Max(1.f, Scale * 2.5f));
		};
		auto Box = [&](float X, float Y, float W, float H)
		{
			FSlateDrawElement::MakeBox(
			    Elements, Layer,
			    Geometry.ToPaintGeometry(FVector2f(W * Scale, H * Scale),
			                             FSlateLayoutTransform(Offset + FVector2f(X, Y) * Scale)),
			    FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")), ESlateDrawEffect::None, Color);
		};
		auto Ring = [&](float X, float Y, float Radius)
		{
			TArray<FVector2f> Points;
			for (int32 I = 0; I <= 24; ++I)
			{
				const float A = I * 2.f * PI / 24.f;
				Points.Add(Offset + FVector2f(X + FMath::Cos(A) * Radius, Y + FMath::Sin(A) * Radius) * Scale);
			}
			FSlateDrawElement::MakeLines(Elements, Layer, Geometry.ToPaintGeometry(), Points, ESlateDrawEffect::None,
			                             Color, true, FMath::Max(1.f, Scale * 2.5f));
		};
		const FName Name = Data->Symbol;
		if (Name == TEXT("DirtRoad"))
		{
			Lines({{10, 3}, {8, 11}, {9, 19}, {6, 29}});
			Lines({{22, 3}, {24, 11}, {23, 19}, {26, 29}});
			Box(14, 8, 3, 2); Box(18, 19, 3, 2); Box(12, 25, 2, 2);
		}
		else if (Name == TEXT("LocalRoad"))
		{
			Lines({{8, 3}, {8, 29}}); Lines({{24, 3}, {24, 29}});
			Box(15, 4, 2, 5); Box(15, 14, 2, 5); Box(15, 24, 2, 5);
		}
		else if (Name == TEXT("Avenue"))
		{
			Lines({{3, 3}, {3, 29}}); Lines({{29, 3}, {29, 29}});
			Box(14, 3, 4, 26);
			for (float Y : {4.f, 15.f, 26.f}) { Box(8, Y, 1, 4); Box(23, Y, 1, 4); }
		}
		else if (Name == TEXT("Highway"))
		{
			Lines({{4, 3}, {4, 29}}); Lines({{12, 3}, {12, 29}});
			Lines({{20, 3}, {20, 29}}); Lines({{28, 3}, {28, 16}, {31, 25}});
			Lines({{1, 7}, {4, 16}}); Lines({{15, 7}, {16, 5}, {17, 7}});
			Lines({{16, 5}, {16, 12}});
		}
		else if (Name == TEXT("BasicPower"))
		{
			Lines({{3, 28}, {3, 11}, {11, 11}, {11, 6}, {22, 6}, {22, 14}, {29, 14}, {29, 28}, {3, 28}});
			Lines({{19, 11}, {12, 20}, {19, 20}, {15, 27}});
		}
		else if (Name == TEXT("CoalPower"))
		{
			Box(5, 5, 5, 23); Box(15, 11, 5, 17);
			Lines({{3, 29}, {29, 29}, {29, 20}, {23, 16}, {23, 29}});
			Ring(8, 2, 1); Lines({{10, 2}, {14, 3}, {18, 2}});
		}
		else if (Name == TEXT("OilPower"))
		{
			Lines({{4, 12}, {4, 27}, {28, 27}, {28, 12}, {4, 12}, {8, 7}, {24, 7}, {28, 12}});
			Lines({{16, 14}, {12, 21}, {13, 24}, {19, 24}, {20, 21}, {16, 14}});
		}
		else if (Name == TEXT("WindPower"))
		{
			Lines({{16, 13}, {16, 29}, {9, 29}, {23, 29}});
			Ring(16, 12, 2); Lines({{16, 10}, {16, 2}});
			Lines({{14, 13}, {4, 20}}); Lines({{18, 13}, {28, 20}});
		}
		else if (Name == TEXT("SolarPower"))
		{
			Lines({{7, 12}, {27, 12}, {24, 26}, {3, 26}, {7, 12}});
			Lines({{5, 19}, {25, 19}}); Lines({{13, 12}, {10, 26}}); Lines({{20, 12}, {17, 26}});
			Ring(7, 5, 3); Lines({{14, 5}, {18, 5}}); Lines({{7, 29}, {23, 29}});
		}
		else if (Name == TEXT("NuclearPower"))
		{
			Lines({{2, 29}, {7, 18}, {7, 9}, {15, 9}, {15, 18}, {20, 29}, {2, 29}});
			Lines({{20, 10}, {26, 10}, {26, 19}, {30, 29}, {22, 29}});
			Ring(22, 4, 2); Lines({{9, 3}, {12, 5}, {15, 3}});
		}
		else if (Name == TEXT("WaterTower"))
		{
			Lines({{6, 6}, {10, 3}, {23, 3}, {27, 6}, {27, 14}, {23, 18}, {10, 18}, {6, 14}, {6, 6}});
			Lines({{11, 18}, {7, 29}}); Lines({{23, 18}, {27, 29}}); Lines({{9, 25}, {25, 25}});
		}
		else if (Name == TEXT("WaterTreatment"))
		{
			Lines({{3, 13}, {14, 13}, {14, 28}, {3, 28}, {3, 13}});
			Lines({{18, 13}, {29, 13}, {29, 28}, {18, 28}, {18, 13}});
			Lines({{5, 19}, {9, 21}, {12, 19}}); Lines({{20, 19}, {24, 21}, {27, 19}});
			Lines({{8, 13}, {8, 6}, {24, 6}, {24, 13}});
		}
		else if (Name == TEXT("PrimarySchool"))
		{
			Lines({{3, 7}, {12, 7}, {16, 10}, {20, 7}, {29, 7}, {29, 25}, {20, 25}, {16, 28}, {12, 25}, {3, 25}, {3, 7}});
			Lines({{16, 10}, {16, 27}}); Lines({{7, 13}, {12, 13}}); Lines({{20, 13}, {25, 13}});
		}
		else if (Name == TEXT("SecondarySchool"))
		{
			Lines({{3, 29}, {3, 14}, {16, 4}, {29, 14}, {29, 29}, {3, 29}});
			Ring(16, 13, 3); Lines({{16, 11}, {16, 13}, {18, 14}});
			Box(7, 19, 3, 5); Box(22, 19, 3, 5); Box(14, 22, 4, 7);
		}
		else if (Name == TEXT("College"))
		{
			Lines({{2, 10}, {16, 4}, {30, 10}, {16, 16}, {2, 10}});
			Lines({{8, 14}, {8, 22}, {16, 26}, {24, 22}, {24, 14}});
			Lines({{30, 10}, {30, 24}});
		}
		else if (Name == TEXT("PocketPark"))
		{
			Ring(10, 10, 7); Lines({{10, 17}, {10, 29}});
			Lines({{18, 21}, {29, 21}}); Lines({{19, 16}, {28, 16}});
			Lines({{20, 21}, {20, 28}}); Lines({{27, 21}, {27, 28}});
		}
		else if (Name == TEXT("NeighborhoodPark"))
		{
			Ring(7, 9, 5); Ring(25, 13, 5);
			Lines({{7, 14}, {7, 24}}); Lines({{25, 18}, {25, 27}});
			Lines({{10, 29}, {16, 22}, {13, 16}, {18, 8}});
		}
		else if (Name == TEXT("CityPark"))
		{
			Lines({{3, 27}, {7, 22}, {15, 22}, {20, 26}, {29, 26}});
			Ring(8, 9, 6); Lines({{8, 15}, {8, 20}});
			Lines({{18, 11}, {24, 3}, {30, 11}, {18, 11}}); Lines({{24, 11}, {24, 20}});
		}
		else if (Name == TEXT("BasketballPark"))
		{
			Lines({{10, 3}, {28, 3}, {28, 15}, {10, 15}, {10, 3}});
			Lines({{19, 15}, {19, 29}, {27, 29}}); Ring(16, 16, 4);
			Lines({{13, 19}, {15, 24}, {19, 24}, {21, 19}});
		}
		else if (Name == TEXT("SkatePark"))
		{
			Lines({{3, 7}, {3, 27}, {29, 27}, {29, 7}});
			Lines({{3, 9}, {6, 19}, {10, 23}, {22, 23}, {26, 19}, {29, 9}});
			Lines({{11, 10}, {22, 10}}); Ring(13, 14, 1); Ring(20, 14, 1);
		}
		else if (Name == TEXT("BaseballPark"))
		{
			Lines({{16, 3}, {29, 16}, {16, 29}, {3, 16}, {16, 3}});
			Box(14, 4, 4, 4); Box(24, 14, 4, 4); Box(4, 14, 4, 4);
			Lines({{16, 25}, {16, 16}}); Ring(16, 16, 2);
		}
		else if (Name == TEXT("SoccerPark"))
		{
			Lines({{3, 5}, {29, 5}, {29, 27}, {3, 27}, {3, 5}});
			Lines({{16, 5}, {16, 27}}); Ring(16, 16, 5);
			Lines({{3, 11}, {8, 11}, {8, 21}, {3, 21}}); Lines({{29, 11}, {24, 11}, {24, 21}, {29, 21}});
		}
		else if (Name == TEXT("WasteDepot"))
		{
			Lines({{4, 10}, {7, 28}, {25, 28}, {28, 10}}); Lines({{2, 9}, {30, 9}});
			Lines({{11, 4}, {21, 4}, {21, 9}, {11, 9}, {11, 4}});
			Lines({{12, 14}, {12, 23}}); Lines({{20, 14}, {20, 23}});
		}
		else if (Name == TEXT("Recycling"))
		{
			Lines({{11, 10}, {16, 3}, {23, 14}, {17, 12}, {23, 14}, {25, 8}});
			Lines({{25, 18}, {29, 26}, {16, 26}, {20, 21}, {16, 26}, {20, 30}});
			Lines({{12, 26}, {3, 26}, {8, 15}, {10, 21}, {8, 15}, {2, 16}});
		}
		else if (Name == TEXT("AIInstitute"))
		{
			Ring(16, 5, 3); Ring(5, 16, 3); Ring(27, 16, 3); Ring(16, 27, 3);
			Lines({{14, 7}, {7, 14}}); Lines({{18, 7}, {25, 14}});
			Lines({{7, 18}, {14, 25}}); Lines({{25, 18}, {18, 25}}); Ring(16, 16, 4);
		}
		else if (Name == TEXT("EnterpriseAI"))
		{
			Lines({{5, 29}, {5, 8}, {18, 3}, {18, 29}, {5, 29}});
			Box(9, 12, 3, 3); Box(9, 20, 3, 3);
			Lines({{21, 13}, {29, 13}, {29, 25}, {21, 25}, {21, 13}});
			Lines({{23, 9}, {23, 13}}); Lines({{27, 25}, {27, 29}});
		}
		else if (Name == TEXT("Inspect"))
		{
			Ring(13, 13, 8);
			Lines({{19, 19}, {28, 28}});
		}
		else if (Name == TEXT("TrendUp") || Name == TEXT("TrendDown") || Name == TEXT("TrendSteady"))
		{
			if (Name == TEXT("TrendUp")) Lines({{6, 26}, {26, 6}, {14, 6}, {26, 6}, {26, 18}});
			else if (Name == TEXT("TrendDown")) Lines({{6, 6}, {26, 26}, {14, 26}, {26, 26}, {26, 14}});
			else Lines({{4, 16}, {28, 16}, {21, 9}, {28, 16}, {21, 23}});
		}
		else if (Name == TEXT("TrendUnknown"))
		{
			Ring(16, 16, 12); Lines({{12, 10}, {15, 7}, {20, 10}, {20, 14}, {16, 18}, {16, 21}}); Box(15, 24, 2, 2);
		}
		else if (Name == TEXT("Happiness"))
		{
			Ring(16, 16, 12); Box(11, 10, 2, 3); Box(20, 10, 2, 3);
			Lines({{9, 19}, {12, 23}, {20, 23}, {24, 19}});
		}
		else if (Name == TEXT("Prosperity"))
		{
			Ring(16, 16, 12); Lines({{21, 10}, {13, 10}, {10, 13}, {22, 19}, {19, 23}, {11, 23}});
			Lines({{16, 6}, {16, 27}});
		}
		else if (Name == TEXT("Growth") || Name == TEXT("Leaving"))
		{
			if (Name == TEXT("Growth"))
				Lines({{6, 26}, {14, 18}, {19, 21}, {27, 7}, {17, 8}, {27, 7}, {28, 17}});
			else
			{
				Lines({{13, 5}, {5, 5}, {5, 27}, {13, 27}});
				Lines({{11, 16}, {28, 16}, {21, 9}, {28, 16}, {21, 23}});
			}
		}
		else if (Name == TEXT("Repair"))
		{
			Lines({{7, 4}, {7, 11}, {12, 13}, {17, 10}, {17, 4}});
			Lines({{8, 13}, {23, 28}, {28, 23}, {14, 9}});
		}
		else if (Name == TEXT("Home"))
		{
			Box(7, 14, 18, 13);
			Ink(FLinearColor(.8f, .3f, .16f));
			Lines({{3, 15}, {16, 4}, {29, 15}});
			Ink(FLinearColor(.9f, .94f, .84f));
			Box(13, 19, 6, 9);
			Box(8, 17, 3, 4);
		}
		else if (Name == TEXT("Shop"))
		{
			Box(6, 13, 20, 14);
			Ink(FLinearColor(.85f, .9f, .94f));
			Lines({{4, 12}, {7, 5}, {25, 5}, {28, 12}, {4, 12}});
			Box(9, 19, 6, 9);
			Box(19, 18, 4, 4);
			Lines({{11, 5}, {10, 12}});
			Lines({{21, 5}, {22, 12}});
		}
		else if (Name == TEXT("Industry"))
		{
			Lines({{4, 27}, {4, 15}, {12, 10}, {12, 16}, {20, 11}, {20, 16}, {28, 16}, {28, 27}, {4, 27}});
			Box(23, 3, 4, 13);
			Ink(FLinearColor(.9f, .94f, .85f));
			Box(8, 21, 3, 3);
			Box(15, 21, 3, 3);
			Box(22, 21, 3, 3);
		}
		else if (Name == TEXT("Zone") || Name == TEXT("Overlays"))
		{
			Lines({{3, 12}, {16, 5}, {29, 12}, {16, 19}, {3, 12}});
			Lines({{3, 18}, {16, 25}, {29, 18}});
			if (Name == TEXT("Overlays"))
				Lines({{3, 23}, {16, 30}, {29, 23}});
			else
				Lines({{16, 5}, {16, 19}});
		}
		else if (Name == TEXT("Technology"))
		{
			Lines({{8, 8}, {24, 8}, {24, 24}, {8, 24}, {8, 8}});
			for (float P : {12.f, 20.f})
			{
				Lines({{P, 3}, {P, 8}});
				Lines({{P, 24}, {P, 29}});
				Lines({{3, P}, {8, P}});
				Lines({{24, P}, {29, P}});
			}
			Box(12, 12, 8, 8);
		}
		else if (Name == TEXT("Connectivity"))
		{
			Lines({{16, 9}, {16, 18}, {6, 24}, {26, 24}, {16, 18}});
			Ring(16, 6, 3);
			Ring(5, 26, 3);
			Ring(27, 26, 3);
		}
		else if (Name == TEXT("Computing"))
		{
			Lines({{7, 4}, {25, 4}, {25, 28}, {7, 28}, {7, 4}});
			Lines({{7, 12}, {25, 12}});
			Lines({{7, 20}, {25, 20}});
			Box(10, 7, 2, 2);
			Box(10, 15, 2, 2);
			Box(10, 23, 2, 2);
		}
		else if (Name == TEXT("Research"))
		{
			Lines({{11, 4}, {21, 4}, {19, 4}, {19, 13}, {28, 26}, {26, 29}, {6, 29}, {4, 26}, {13, 13}, {13, 4}});
			Lines({{8, 22}, {24, 22}});
			Ring(16, 19, 2);
		}
		else if (Name == TEXT("City"))
		{
			Box(5, 18, 5, 10);
			Ink(FLinearColor(.26f, .75f, .55f));
			Box(14, 10, 5, 18);
			Ink(FLinearColor(.88f, .64f, .22f));
			Box(23, 4, 5, 24);
		}
		else if (Name == TEXT("Settings"))
		{
			Ring(16, 16, 8);
			Ring(16, 16, 3);
			for (int32 I = 0; I < 8; ++I)
			{
				const float A = I * PI / 4;
				Lines({{16 + FMath::Cos(A) * 8, 16 + FMath::Sin(A) * 8},
				       {16 + FMath::Cos(A) * 13, 16 + FMath::Sin(A) * 13}});
			}
		}
		else if (Name == TEXT("Demolish"))
		{
			Lines({{3, 25}, {29, 25}});
			Box(5, 18, 18, 5);
			Lines({{10, 18}, {10, 9}, {21, 9}, {23, 18}});
			Lines({{26, 14}, {29, 14}, {29, 24}, {26, 24}});
		}
		else if (Name == TEXT("Power"))
		{
			Lines({{19, 3}, {7, 18}, {15, 18}, {12, 29}, {26, 13}, {18, 13}, {19, 3}});
		}
		else if (Name == TEXT("Water"))
		{
			Lines({{16, 3}, {6, 18}, {7, 25}, {12, 29}, {20, 29}, {25, 25}, {26, 18}, {16, 3}});
		}
		else if (Name == TEXT("Road"))
		{
			Lines({{10, 3}, {5, 29}});
			Lines({{22, 3}, {27, 29}});
			Ink(FLinearColor(.9f, .82f, .48f));
			Box(15, 4, 2, 5);
			Box(15, 14, 2, 5);
			Box(15, 24, 2, 5);
		}
		else if (Name == TEXT("Park"))
		{
			Box(8, 19, 4, 10);
			Box(22, 19, 3, 10);
			Ring(10, 12, 8);
			Ink(FLinearColor(.12f, .55f, .25f));
			Ring(23, 16, 6);
			Ink(FLinearColor(.6f, .36f, .14f));
			Box(9, 20, 2, 9);
			Box(23, 21, 2, 8);
		}
		else if (Name == TEXT("Police"))
		{
			Lines({{16, 3}, {28, 8}, {26, 21}, {16, 29}, {6, 21}, {4, 8}, {16, 3}});
			Ink(FLinearColor(.92f, .76f, .28f));
			Lines({{16, 9},
			       {18, 14},
			       {23, 15},
			       {19, 19},
			       {20, 24},
			       {16, 21},
			       {12, 24},
			       {13, 19},
			       {9, 15},
			       {14, 14},
			       {16, 9}});
		}
		else if (Name == TEXT("Fire"))
		{
			Lines({{17, 3},
			       {23, 12},
			       {25, 9},
			       {28, 20},
			       {26, 26},
			       {20, 29},
			       {11, 29},
			       {5, 24},
			       {5, 17},
			       {10, 11},
			       {10, 17},
			       {17, 3}});
			Ink(FLinearColor(.95f, .68f, .18f));
			Lines({{17, 16}, {22, 24}, {18, 28}, {13, 26}, {12, 23}, {17, 16}});
		}
		else if (Name == TEXT("Health"))
		{
			Box(12, 4, 8, 24);
			Box(4, 12, 24, 8);
			Ink(FLinearColor(.95f, .85f, .82f));
			Box(14, 9, 4, 14);
			Box(9, 14, 14, 4);
		}
		else if (Name == TEXT("School"))
		{
			Lines(
			    {{3, 6}, {12, 6}, {16, 9}, {20, 6}, {29, 6}, {29, 26}, {20, 26}, {16, 29}, {12, 26}, {3, 26}, {3, 6}});
			Ink(FLinearColor(.92f, .73f, .25f));
			Lines({{16, 9}, {16, 27}});
			Lines({{7, 12}, {12, 12}});
			Lines({{20, 12}, {25, 12}});
			Lines({{7, 18}, {12, 18}});
			Lines({{20, 18}, {25, 18}});
		}
		else if (Name == TEXT("Waste"))
		{
			Lines({{6, 8}, {9, 28}, {24, 28}, {27, 8}});
			Lines({{4, 7}, {29, 7}});
			Lines({{11, 3}, {22, 3}});
			Ink(FLinearColor(.9f, .94f, .85f));
			Lines({{14, 12}, {14, 23}});
			Lines({{20, 12}, {20, 23}});
		}
		else if (Name == TEXT("People"))
		{
			Ring(16, 9, 5);
			Lines({{7, 28}, {7, 23}, {10, 18}, {22, 18}, {25, 23}, {25, 28}});
		}
		else if (Name == TEXT("Money"))
		{
			Ring(16, 16, 12);
			Lines({{21, 9}, {12, 9}, {9, 13}, {12, 16}, {20, 16}, {23, 20}, {20, 24}, {11, 24}});
			Lines({{16, 5}, {16, 28}});
		}
		else if (Name == TEXT("Calendar"))
		{
			Lines({{5, 7}, {27, 7}, {27, 28}, {5, 28}, {5, 7}});
			Lines({{5, 13}, {27, 13}});
			Lines({{10, 3}, {10, 10}});
			Lines({{22, 3}, {22, 10}});
			Box(10, 18, 4, 4);
			Box(18, 18, 4, 4);
		}
		else if (Name == TEXT("Objective"))
		{
			Ring(16, 16, 11);
			Ring(16, 16, 6);
			Lines({{16, 16}, {28, 4}});
		}
		else if (Name == TEXT("Menu"))
		{
			Lines({{5, 8}, {27, 8}});
			Lines({{5, 16}, {27, 16}});
			Lines({{5, 24}, {27, 24}});
		}
		else if (Name == TEXT("Build"))
		{
			Lines({{6, 28}, {22, 9}});
			Ink(FLinearColor(.85f, .9f, .94f));
			Lines({{14, 5}, {20, 3}, {29, 11}, {24, 17}, {14, 5}});
		}
		else
		{
			Ring(16, 16, 10);
			Lines({{12, 11}, {21, 16}, {12, 21}});
		}
		return Layer;
	}

private:
	TWeakObjectPtr<UCityDesktopIcon> Owner;
};

void UCityDesktopIcon::SetIcon(const FName InSymbol, const FLinearColor InTint)
{
	ConditionTexture = nullptr;
	if (Symbol == InSymbol && Tint == InTint)
		return;
	Symbol = InSymbol;
	Tint = InTint;
	if (Icon)
		Icon->Invalidate(EInvalidateWidgetReason::Paint);
}

void UCityDesktopIcon::SetConditionIcon(const FName InSymbol, const FLinearColor InTint)
{
	if (ConditionTexture && Symbol == InSymbol && Tint == InTint)
		return;
	SetIcon(InSymbol, InTint);
	static const TMap<FName, FString> Names = {{TEXT("Fire"), TEXT("local_fire_department")},
	                                           {TEXT("Health"), TEXT("medical_services")},
	                                           {TEXT("Waste"), TEXT("delete")},
	                                           {TEXT("Power"), TEXT("bolt")},
	                                           {TEXT("Water"), TEXT("water_drop")},
	                                           {TEXT("Shop"), TEXT("shopping_basket")},
	                                           {TEXT("Happiness"), TEXT("sentiment_dissatisfied")},
	                                           {TEXT("Leaving"), TEXT("package_2")},
	                                           {TEXT("Home"), TEXT("home")},
	                                           {TEXT("People"), TEXT("groups")},
	                                           {TEXT("School"), TEXT("school")},
	                                           {TEXT("Money"), TEXT("account_balance_wallet")},
	                                           {TEXT("Inspect"), TEXT("pause_circle")},
	                                           {TEXT("Repair"), TEXT("build")},
	                                           {TEXT("Repairing"), TEXT("construction")},
	                                           {TEXT("Police"), TEXT("local_police")}};
	if (const auto* Name = Names.Find(InSymbol))
	{
		ConditionTexture = LoadObject<UTexture2D>(
		    nullptr, *FString::Printf(TEXT("/Game/CitySim/UI/Icons/T_Icon_%s.T_Icon_%s"), **Name, **Name));
		ConditionBrush.SetResourceObject(ConditionTexture);
		ConditionBrush.ImageSize = FVector2D(256, 256);
		ConditionBrush.DrawAs = ESlateBrushDrawType::Image;
	}
	if (Icon)
		Icon->Invalidate(EInvalidateWidgetReason::Paint);
}

FName UCityDesktopIcon::ForAction(const int32 Action)
{
	switch (static_cast<ECityPaletteAction>(Action))
	{
	case ECityPaletteAction::SelectInspect:
		return TEXT("Inspect");
	case ECityPaletteAction::SelectRepair:
		return TEXT("Repair");
	case ECityPaletteAction::OpenBuild:
		return TEXT("Build");
	case ECityPaletteAction::OpenZone:
		return TEXT("Zone");
	case ECityPaletteAction::SelectDemolish:
		return TEXT("Demolish");
	case ECityPaletteAction::OpenOverlays:
		return TEXT("Overlays");
	case ECityPaletteAction::OpenCity:
		return TEXT("City");
	case ECityPaletteAction::OpenSettings:
		return TEXT("Settings");
	case ECityPaletteAction::SelectResidentialZone:
		return TEXT("Home");
	case ECityPaletteAction::SelectCommercialZone:
		return TEXT("Shop");
	case ECityPaletteAction::SelectIndustrialZone:
		return TEXT("Industry");
	case ECityPaletteAction::OpenRoads:
		return TEXT("Road");
	case ECityPaletteAction::OpenPowerFacilities:
		return TEXT("Power");
	case ECityPaletteAction::OpenWaterFacilities:
		return TEXT("Water");
	case ECityPaletteAction::SelectPolice:
		return TEXT("Police");
	case ECityPaletteAction::SelectFire:
		return TEXT("Fire");
	case ECityPaletteAction::SelectHospital:
		return TEXT("Health");
	case ECityPaletteAction::OverlayCommercialAi:
	case ECityPaletteAction::OpenCommercialAi:
	case ECityPaletteAction::ReviewAIInstitute:
	case ECityPaletteAction::ReviewEnterpriseAICenter:
	case ECityPaletteAction::OpenTechnology:
	case ECityPaletteAction::OpenTechnologyFacilities:
		return TEXT("Technology");
	case ECityPaletteAction::SelectDigitalExchange:
	case ECityPaletteAction::ReviewDigitalExchange:
		return TEXT("Connectivity");
	case ECityPaletteAction::SelectComputingCenter:
	case ECityPaletteAction::ReviewComputingCenter:
		return TEXT("Computing");
	case ECityPaletteAction::SelectResearchCampus:
	case ECityPaletteAction::ReviewResearchCampus:
		return TEXT("Research");
	case ECityPaletteAction::OpenEducationFacilities:
		return TEXT("School");
	case ECityPaletteAction::OpenWasteFacilities:
		return TEXT("Waste");
	case ECityPaletteAction::OpenParks:
		return TEXT("Park");
	case ECityPaletteAction::SelectDirtRoad:
		return TEXT("DirtRoad");
	case ECityPaletteAction::SelectLocalRoad:
		return TEXT("LocalRoad");
	case ECityPaletteAction::SelectAvenue:
		return TEXT("Avenue");
	case ECityPaletteAction::SelectHighway:
		return TEXT("Highway");
	case ECityPaletteAction::SelectPower:
		return TEXT("BasicPower");
	case ECityPaletteAction::SelectCoalPower:
		return TEXT("CoalPower");
	case ECityPaletteAction::SelectOilPower:
		return TEXT("OilPower");
	case ECityPaletteAction::SelectWindPower:
		return TEXT("WindPower");
	case ECityPaletteAction::SelectSolarPower:
		return TEXT("SolarPower");
	case ECityPaletteAction::SelectNuclearPower:
		return TEXT("NuclearPower");
	case ECityPaletteAction::SelectWaterTower:
		return TEXT("WaterTower");
	case ECityPaletteAction::SelectService:
		return TEXT("WaterTreatment");
	case ECityPaletteAction::SelectPrimarySchool:
		return TEXT("PrimarySchool");
	case ECityPaletteAction::SelectSecondarySchool:
		return TEXT("SecondarySchool");
	case ECityPaletteAction::SelectCollege:
		return TEXT("College");
	case ECityPaletteAction::SelectPark:
		return TEXT("PocketPark");
	case ECityPaletteAction::SelectNeighborhoodPark:
		return TEXT("NeighborhoodPark");
	case ECityPaletteAction::SelectCityPark:
		return TEXT("CityPark");
	case ECityPaletteAction::SelectBasketballPark:
		return TEXT("BasketballPark");
	case ECityPaletteAction::SelectSkatePark:
		return TEXT("SkatePark");
	case ECityPaletteAction::SelectBaseballPark:
		return TEXT("BaseballPark");
	case ECityPaletteAction::SelectSoccerPark:
		return TEXT("SoccerPark");
	case ECityPaletteAction::SelectWasteDepot:
		return TEXT("WasteDepot");
	case ECityPaletteAction::SelectRecycling:
		return TEXT("Recycling");
	case ECityPaletteAction::SelectAIInstitute:
		return TEXT("AIInstitute");
	case ECityPaletteAction::SelectEnterpriseAICenter:
		return TEXT("EnterpriseAI");
	default:
		return NAME_None;
	}
}

FLinearColor UCityDesktopIcon::ColorForSymbol(const FName InSymbol)
{
	const FString Name = InSymbol.ToString();
	if (Name.EndsWith(TEXT("Power"))) return FLinearColor(1.f, .7f, .22f);
	if (Name.StartsWith(TEXT("Water"))) return FLinearColor(.16f, .5f, .92f);
	if (Name.EndsWith(TEXT("School")) || InSymbol == TEXT("College")) return FLinearColor(.12f, .7f, .73f);
	if (Name.EndsWith(TEXT("Park")) || InSymbol == TEXT("WasteDepot") || InSymbol == TEXT("Recycling"))
		return FLinearColor(.2f, .7f, .38f);
	if (InSymbol == TEXT("DirtRoad")) return FLinearColor(.75f, .55f, .32f);
	if (InSymbol == TEXT("LocalRoad") || InSymbol == TEXT("Avenue") || InSymbol == TEXT("Highway"))
		return FLinearColor(.65f, .78f, .9f);
	if (InSymbol == TEXT("AIInstitute") || InSymbol == TEXT("EnterpriseAI")) return FLinearColor(.55f, .65f, 1.f);
	if (InSymbol == TEXT("Home") || InSymbol == TEXT("Zone"))
		return FLinearColor(.24f, .82f, .4f);
	if (InSymbol == TEXT("Shop") || InSymbol == TEXT("Inspect"))
		return FLinearColor(.2f, .65f, 1.f);
	if (InSymbol == TEXT("Industry") || InSymbol == TEXT("Build") || InSymbol == TEXT("Power"))
		return FLinearColor(1.f, .7f, .22f);
	if (InSymbol == TEXT("Demolish") || InSymbol == TEXT("Fire") || InSymbol == TEXT("Health"))
		return FLinearColor(.95f, .27f, .19f);
	if (InSymbol == TEXT("Overlays"))
		return FLinearColor(.62f, .4f, .92f);
	if (InSymbol == TEXT("Park") || InSymbol == TEXT("Waste") || InSymbol == TEXT("Money"))
		return FLinearColor(.2f, .7f, .38f);
	if (InSymbol == TEXT("City") || InSymbol == TEXT("School"))
		return FLinearColor(.12f, .7f, .73f);
	if (InSymbol == TEXT("Water") || InSymbol == TEXT("Police") || InSymbol == TEXT("People"))
		return FLinearColor(.16f, .5f, .92f);
	if (InSymbol == TEXT("Technology") || InSymbol == TEXT("Connectivity") || InSymbol == TEXT("Computing") ||
	    InSymbol == TEXT("Research"))
		return FLinearColor(.55f, .65f, 1.f);
	if (InSymbol == TEXT("Calendar"))
		return FLinearColor(.65f, .48f, .82f);
	if (InSymbol == TEXT("Road") || InSymbol == TEXT("Settings"))
		return FLinearColor(.5f, .65f, .8f);
	return FLinearColor(.3f, .6f, .8f);
}

TSharedRef<SWidget> UCityDesktopIcon::RebuildWidget()
{
	Icon = SNew(SCityDesktopIcon, this);
	return Icon.ToSharedRef();
}
void UCityDesktopIcon::ReleaseSlateResources(const bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	Icon.Reset();
}
