#include "Interaction/CityVRToolController.h"

#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"
#include "Presentation/CityBoardActor.h"
#include "Simulation/CitySimulationSubsystem.h"

namespace
{
	FString WrapCallout(const FString& Text)
	{
		TArray<FString> Lines;
		Text.ParseIntoArrayLines(Lines, false);
		TArray<FString> Wrapped;
		for (FString Line : Lines)
		{
			while (Line.Len() > 48)
			{
				int32 Split = 48;
				while (Split > 0 && Line[Split] != TEXT(' '))
					--Split;
				if (Split == 0)
					break;
				Wrapped.Add(Line.Left(Split));
				Line = Line.Mid(Split + 1);
			}
			Wrapped.Add(Line);
		}
		return FString::Join(Wrapped, TEXT("\n"));
	}
} // namespace

void ACityVRToolController::UpdateCityTotals()
{
	if (!Board)
		return;
	CityTotalsText->SetText(FText::FromString(Board->DescribeCityTotals()));
	int64 MonthlyMoney = 0, PopulationChange = 0;
	bool bPopulationHistory = false;
	Board->GetCityHudTrends(MonthlyMoney, PopulationChange, bPopulationHistory);
	FBox Bounds = CalendarText->CalcBounds(CalendarText->GetRelativeTransform()).GetBox();
	Bounds += CityTotalsText->CalcBounds(CityTotalsText->GetRelativeTransform()).GetBox();
	for (int32 Index = 0; Index < 2; ++Index)
	{
		const int64 Change = Index == 0 ? MonthlyMoney : PopulationChange;
		const bool bKnown = Index == 0 || bPopulationHistory;
		const FColor Color = !bKnown || Change == 0 ? FColor(190, 200, 210)
		                     : Change > 0           ? FColor(80, 240, 120)
		                                            : FColor(255, 100, 80);
		const FString Label = CityUiContent::Trend(Index, Change, bKnown);
		CityTrendTexts[Index]->SetText(FText::FromString(Label));
		CityTrendTexts[Index]->SetTextRenderColor(Color);
		const FBox TextBounds =
		    CityTrendTexts[Index]->CalcBounds(CityTrendTexts[Index]->GetRelativeTransform()).GetBox();
		Bounds += TextBounds;
		auto* Arrow = CityTrendArrows[Index].Get();
		Arrow->SetVisibility(bKnown && Change != 0);
		Arrow->SetRelativeLocation(FVector(.9f, TextBounds.Max.Y + 1.1f, 12.8f));
		Arrow->SetRelativeRotation(FRotator(0, 0, Change < 0 ? 180.f : 0.f));
		if (CityTrendMaterials.Num() <= Index)
			CityTrendMaterials.Add(Arrow->CreateDynamicMaterialInstance(0));
		if (CityTrendMaterials[Index])
			CityTrendMaterials[Index]->SetVectorParameterValue(TEXT("BarColor"), FLinearColor::FromSRGBColor(Color));
		if (bKnown && Change != 0)
			Bounds += Arrow->CalcBounds(Arrow->GetRelativeTransform()).GetBox();
	}
	const auto* Sim = GetWorld() ? GetWorld()->GetSubsystem<UCitySimulationSubsystem>() : nullptr;
	if (Sim)
	{
		CityBudgetDetail->SetText(FText::FromString(CityUiContent::Budget(*Sim)));
		CityLastNetText->SetText(FText::FromString(CityUiContent::LastNet(*Sim)));
	}
	Bounds += CityBudgetDetail->CalcBounds(CityBudgetDetail->GetRelativeTransform()).GetBox();
	Bounds += CityLastNetText->CalcBounds(CityLastNetText->GetRelativeTransform()).GetBox();
	if (ConditionSummaryText && ConditionSummaryText->IsVisible())
		Bounds += ConditionSummaryText->CalcBounds(ConditionSummaryText->GetRelativeTransform()).GetBox();
	for (UTextRenderComponent* Label : TimeControlLabels)
		Bounds += Label->CalcBounds(Label->GetRelativeTransform()).GetBox();
	// TextRender's multiline center alignment draws the first calendar line above its reported box.
	Bounds.Max.Z = FMath::Max(Bounds.Max.Z, CalendarText->GetRelativeLocation().Z + 3.5);
	CityTotalsBackground->SetRelativeLocation(FVector(-.3f, Bounds.GetCenter().Y, Bounds.GetCenter().Z));
	CityTotalsBackground->SetRelativeScale3D(
	    FVector(.006f, (Bounds.GetSize().Y + 4.f) / 100.f, (Bounds.GetSize().Z + 2.f) / 100.f));
	// Both icon buttons share the information panel's lower edge, even as its text changes.
	const double BottomZ = HeadsetStatusRoot->GetRelativeLocation().Z + Bounds.Min.Z - 1.0;
	for (USceneComponent* ToggleRoot : {NeedsToggleRoot.Get(), ObjectiveToggleRoot.Get()})
	{
		FVector Position = ToggleRoot->GetRelativeLocation();
		Position.Z = BottomZ + 3.0;
		ToggleRoot->SetRelativeLocation(Position);
	}
	FVector PanelPosition = ObjectiveRoot->GetRelativeLocation();
	PanelPosition.Z = BottomZ - 2.5;
	ObjectiveRoot->SetRelativeLocation(PanelPosition);
}

void ACityVRToolController::UpdateCityInfoCards()
{
	if (!Board || !bNeedsOpen || bPaletteOpen || bEntrySceneActive)
		return;
	const auto Cards = Board->GetCityInfoCards();
	for (int32 Index = 0; Index < Cards.Num() && Index < CityInfoTitles.Num(); ++Index)
	{
		CityInfoTitles[Index]->SetText(FText::FromString(Cards[Index].Title));
		CityInfoLabels[Index]->SetText(FText::FromString(Cards[Index].Labels));
		CityInfoValues[Index]->SetText(FText::FromString(Cards[Index].Values));
	}
	// The heading has its own blue title; retain all authoritative explanations below.
	NeedsText->SetText(FText::FromString(CityUiContent::Growth(*Board)));
}

void ACityVRToolController::UpdateConstructionCallout()
{
	FVector Anchor;
	const bool bAllowed = Board && HeadsetCamera && !bEntrySceneActive && !bPaletteOpen && !bMainMenuPinned &&
	                      !bManipulationSession && LifecycleInputGate.CanAcceptInput();
	const bool bAnchored = bAllowed && Board->GetConstructionMarkerAnchor(Anchor);
	const bool bShow = bAnchored;
	const bool bPriceBlocked = bAnchored && Board->IsConstructionPriceBlocked();
	RoadStatusText->SetVisibility(bShow);
	RoadStatusBackground->SetVisibility(bShow);
	PriceBlockedIcon->SetVisibility(bPriceBlocked);
	PriceBlockedSlash->SetVisibility(bPriceBlocked);
	ConstructionLeader->SetVisibility(bAnchored);
	if (!bShow)
		return;
	const FString Status = CityUiContent::Construction(*Board);
	RoadStatusText->SetText(FText::FromString(WrapCallout(Status)));
	RoadStatusText->SetTextRenderColor(bPriceBlocked ? FColor(255, 100, 80) : FColor::White);
	FBox Bounds = RoadStatusText->CalcBounds(RoadStatusText->GetRelativeTransform()).GetBox();
	if (bPriceBlocked)
	{
		const double IconY = Bounds.Max.Y + 2.5;
		PriceBlockedIcon->SetRelativeLocation(FVector(.8, IconY, Bounds.GetCenter().Z));
		PriceBlockedSlash->SetRelativeLocation(FVector(1, IconY, Bounds.GetCenter().Z));
		Bounds += PriceBlockedIcon->CalcBounds(PriceBlockedIcon->GetRelativeTransform()).GetBox();
	}
	RoadStatusBackground->SetRelativeLocation(FVector(.2, Bounds.GetCenter().Y, Bounds.GetCenter().Z));
	RoadStatusBackground->SetRelativeScale3D(
	    FVector(.006, (Bounds.GetSize().Y + 3) / 100.0, (Bounds.GetSize().Z + 2) / 100.0));
	double Scale = 1;
	FVector Location;
	if (bAnchored)
	{
		Scale = FMath::Clamp(FVector::Distance(Anchor, HeadsetCamera->GetComponentLocation()) / 110.0, .35, 4.0);
		Location = Anchor + HeadsetCamera->GetUpVector() * (Bounds.GetSize().Z + 3.0) * Scale +
		           HeadsetCamera->GetRightVector() * 7.0 * Scale;
	}
	else
		Location = HeadsetStatusRoot->GetComponentTransform().TransformPosition(FVector(0, 0, 4));
	FRotator Facing = (HeadsetCamera->GetComponentLocation() - Location).Rotation();
	Facing.Roll = 0;
	ConstructionStatusRoot->SetWorldLocationAndRotation(Location, Facing);
	ConstructionStatusRoot->SetWorldScale3D(FVector(Scale));
	if (bAnchored)
	{
		const FVector End =
		    ConstructionStatusRoot->GetComponentTransform().TransformPosition(FVector(0, 0, Bounds.Min.Z - 1));
		ConstructionLeader->SetWorldLocationAndRotation((Anchor + End) * .5, (End - Anchor).Rotation());
		ConstructionLeader->SetWorldScale3D(
		    FVector(FVector::Distance(Anchor, End) / 100.0, .0012 * Scale, .0012 * Scale));
	}
}
