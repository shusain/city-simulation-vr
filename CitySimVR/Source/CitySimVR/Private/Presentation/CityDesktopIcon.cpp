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
		if (Name == TEXT("Inspect"))
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
	case ECityPaletteAction::SelectPower:
	case ECityPaletteAction::SelectCoalPower:
	case ECityPaletteAction::SelectOilPower:
	case ECityPaletteAction::SelectWindPower:
	case ECityPaletteAction::SelectSolarPower:
	case ECityPaletteAction::SelectNuclearPower:
		return TEXT("Power");
	case ECityPaletteAction::OpenWaterFacilities:
	case ECityPaletteAction::SelectWaterTower:
	case ECityPaletteAction::SelectService:
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
	case ECityPaletteAction::SelectAIInstitute:
	case ECityPaletteAction::SelectEnterpriseAICenter:
	case ECityPaletteAction::SelectResearchCampus:
	case ECityPaletteAction::ReviewResearchCampus:
		return TEXT("Research");
	case ECityPaletteAction::OpenEducationFacilities:
	case ECityPaletteAction::SelectPrimarySchool:
	case ECityPaletteAction::SelectSecondarySchool:
	case ECityPaletteAction::SelectCollege:
		return TEXT("School");
	case ECityPaletteAction::OpenWasteFacilities:
	case ECityPaletteAction::SelectWasteDepot:
	case ECityPaletteAction::SelectRecycling:
		return TEXT("Waste");
	case ECityPaletteAction::OpenParks:
	case ECityPaletteAction::SelectPark:
	case ECityPaletteAction::SelectNeighborhoodPark:
	case ECityPaletteAction::SelectCityPark:
	case ECityPaletteAction::SelectBasketballPark:
	case ECityPaletteAction::SelectSkatePark:
	case ECityPaletteAction::SelectBaseballPark:
	case ECityPaletteAction::SelectSoccerPark:
		return TEXT("Park");
	case ECityPaletteAction::SelectDirtRoad:
	case ECityPaletteAction::SelectLocalRoad:
	case ECityPaletteAction::SelectAvenue:
	case ECityPaletteAction::SelectHighway:
		return TEXT("Road");
	default:
		return NAME_None;
	}
}

FLinearColor UCityDesktopIcon::ColorForSymbol(const FName InSymbol)
{
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
