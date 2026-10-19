#include "Interaction/CityVRToolController.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"
#include "Simulation/CitySimulationSubsystem.h"

void ACityVRToolController::ExecuteObjectiveFooter(const int32 Index)
{
	const UCitySimulationSubsystem* Simulation = GetWorld()->GetSubsystem<UCitySimulationSubsystem>();
	if (!Simulation || Simulation->GetGuidedProgress().CompletedIds.IsEmpty())
		return;
	if (Index == 0)
	{
		bObjectiveReview = !bObjectiveReview;
		ObjectiveReviewIndex = 0;
	}
	else if (Index == 1 && bObjectiveReview)
		ObjectiveReviewIndex = (ObjectiveReviewIndex + 1) % Simulation->GetGuidedProgress().CompletedIds.Num();
	UpdateObjectiveCard();
}

void ACityVRToolController::SetObjectivesVisible(const bool bVisible)
{
	bObjectivesOpen = bVisible;
	UpdateObjectiveCard();
}

void ACityVRToolController::UpdateObjectiveCard()
{
	const bool bAvailable =
	    HeadsetCamera && !bPaletteOpen && !bManipulationSession && LifecycleInputGate.CanAcceptInput();
	const bool bShow = bAvailable && bObjectivesOpen;
	ObjectiveToggle->SetVisibility(bAvailable);
	ObjectiveHeader->SetVisibility(bShow);
	ObjectiveToggleIcon->SetVisibility(bAvailable);
	ObjectiveToggleIcon->SetRelativeScale3D(FVector(bObjectiveToggleFocused ? 1.4 : bObjectivesOpen ? 1.3 : 1.2));
	if (!ObjectiveIconMaterial)
		ObjectiveIconMaterial = ObjectiveToggleIcon->CreateDynamicMaterialInstance(0);
	if (ObjectiveIconMaterial)
		ObjectiveIconMaterial->SetVectorParameterValue(TEXT("BarColor"), bObjectiveToggleFocused || bObjectivesOpen
		                                                                     ? FLinearColor(0.1f, 0.75f, 1.f)
		                                                                     : FLinearColor::White);
	ObjectiveToggleHelp->SetText(FText::FromString(TEXT("Show objectives")));
	ObjectiveToggleHelp->SetVisibility(bAvailable && bObjectiveToggleFocused && !bObjectivesOpen);
	ObjectiveBackground->SetVisibility(bShow);
	ObjectiveText->SetVisibility(bShow);
	ObjectiveRail->SetVisibility(bShow);
	ObjectiveFill->SetVisibility(false);
	for (int32 Index = 0; Index < ObjectiveFooterButtons.Num(); ++Index)
	{
		ObjectiveFooterButtons[Index]->SetVisibility(false);
		ObjectiveFooterLabels[Index]->SetVisibility(false);
	}
	if (const UCitySimulationSubsystem* Simulation = GetWorld()->GetSubsystem<UCitySimulationSubsystem>())
	{
		const FCityGuidedProgress& Progress = Simulation->GetGuidedProgress();
		if (Progress.CompletedIds.IsEmpty())
			bObjectiveReview = false;
		ObjectiveReviewIndex = FMath::Clamp(ObjectiveReviewIndex, 0, FMath::Max(0, Progress.CompletedIds.Num() - 1));
		const FCityGuidedObjective Card = GetObjectiveCard();
		RefreshObjectiveText(Card);
		const bool bHistoryAvailable = bShow && !Progress.CompletedIds.IsEmpty();
		ObjectiveFooterButtons[0]->SetVisibility(bHistoryAvailable);
		ObjectiveFooterLabels[0]->SetVisibility(bHistoryAvailable);
		ObjectiveFooterLabels[0]->SetText(FText::FromString(bObjectiveReview ? TEXT("LIVE") : TEXT("HISTORY")));
		ObjectiveFooterLabels[0]->SetTextRenderColor(HoveredObjectiveFooter == 0 ? FColor::Cyan : FColor::White);
		ObjectiveFooterButtons[1]->SetVisibility(bHistoryAvailable && bObjectiveReview &&
		                                         Progress.CompletedIds.Num() > 1);
		ObjectiveFooterLabels[1]->SetVisibility(bHistoryAvailable);
		ObjectiveFooterLabels[1]->SetText(
		    FText::FromString(bObjectiveReview ? TEXT("NEXT >")
		                                       : FString::Printf(TEXT("%d / %d DONE"), Progress.CompletedIds.Num(),
		                                                         FCityFirstPlayableGuide::MilestoneIds().Num())));
		ObjectiveFooterLabels[1]->SetTextRenderColor(HoveredObjectiveFooter == 1 ? FColor::Cyan : FColor::White);
	}
	ObjectiveHeader->SetText(FText::FromString(bObjectiveReview ? TEXT("COMPLETED") : TEXT("OBJECTIVES")));
}

void ACityVRToolController::RefreshObjectiveText(const FCityGuidedObjective& Card)
{
	// TextRender uses unscaled font height for empty lines. Keep every line
	// nonempty so small headset text stays inside its centimeter-sized card.
	ObjectiveText->SetText(FText::FromString(CityUiContent::Objective(Card)));
	const float Fraction = FMath::Clamp(Card.Fraction, 0.0f, 1.0f);
	ObjectiveFill->SetRelativeLocation(FVector(.9, 13.5 * (1 - Fraction), -15.5));
	ObjectiveFill->SetRelativeScale3D(FVector(.006, .27 * Fraction, .006));
	ObjectiveFill->SetVisibility(ObjectiveText->IsVisible() && Fraction > 0);
}

FCityGuidedObjective ACityVRToolController::GetObjectiveCard() const
{
	const auto* Sim = GetWorld()->GetSubsystem<UCitySimulationSubsystem>();
	if (!Sim)
		return FCityGuidedObjective();
	const auto& Progress = Sim->GetGuidedProgress();
	return bObjectiveReview && Progress.CompletedIds.IsValidIndex(ObjectiveReviewIndex)
	           ? FCityFirstPlayableGuide::BuildCompletedObjective(Progress.CompletedIds[ObjectiveReviewIndex],
	                                                              ObjectiveReviewIndex, Progress.CompletedIds.Num())
	           : Sim->GetGuidedObjective();
}
