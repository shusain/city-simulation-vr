#include "Interaction/CityVRToolController.h"
#include "Presentation/CityTutorialSettings.h"
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
	if (bTutorialResetSummary && !bObjectiveReview && CityTutorialAttention::HasCompletedOpening(Progress.CompletedIds))
		return {TEXT("tutorial_complete"), TEXT("Opening tutorial complete"),
		        TEXT("This city already has its opening milestones."),
		        TEXT("History reviews these steps.\nStart a new city from Main Menu\nto practise the opening again."),
		        TEXT("Your city and progress are preserved."), 1.f};
	return bObjectiveReview && Progress.CompletedIds.IsValidIndex(ObjectiveReviewIndex)
	           ? FCityFirstPlayableGuide::BuildCompletedObjective(Progress.CompletedIds[ObjectiveReviewIndex],
	                                                              ObjectiveReviewIndex, Progress.CompletedIds.Num())
	           : Sim->GetGuidedObjective();
}

void ACityVRToolController::SetTutorialEnabled(bool bEnabled, bool bReset)
{
    GetMutableDefault<UCityTutorialSettings>()->SetEnabled(bEnabled);
    bTutorialResetSummary = bReset;
    bObjectiveReview = false;
    if (bEnabled) bObjectivesOpen = true;
    UpdateObjectiveCard();
    RefreshPaletteText();
}

bool ACityVRToolController::IsTutorialTarget(ECityPaletteAction Action) const
{
    if (!GetDefault<UCityTutorialSettings>()->bEnabled || bEntrySceneActive || bObjectiveReview) return false;
    const auto* Sim = GetWorld()->GetSubsystem<UCitySimulationSubsystem>();
    if (!Sim) return false;
    if (bTutorialResetSummary && CityTutorialAttention::HasCompletedOpening(Sim->GetGuidedProgress().CompletedIds)) return false;
    const auto Id = Sim->GetGuidedObjective().Id;
    if (!CityTutorialAttention::IsOpeningStep(Id)) return false;
    const auto State = Sim->GetGuideState();
    using A = ECityPaletteAction;
    A Target = A::SelectInspect;
    const bool Wait = Id == TEXT("first_residents") || (Id == TEXT("first_jobs") && State.bHasCommercialZone);
    if (Wait) Target = Sim->IsSimulationPaused() ? A::Pause : A::SelectInspect;
    else if ((Id == TEXT("power") && State.BuiltPowerBuildings > 0) ||
             (Id == TEXT("water") && State.BuiltServiceBuildings > 0)) Target = A::SelectInspect;
    else if (Id == TEXT("homes_zone") || Id == TEXT("first_jobs"))
        Target = PalettePage == ECityPalettePage::Zone ?
            (Id == TEXT("homes_zone") ? A::SelectResidentialZone : A::SelectCommercialZone) : A::OpenZone;
    else if (Id == TEXT("entrance_road"))
        Target = PalettePage == ECityPalettePage::Roads ? A::SelectLocalRoad :
            PalettePage == ECityPalettePage::Build ? A::OpenRoads : A::OpenBuild;
    else if (Id == TEXT("power"))
        Target = PalettePage == ECityPalettePage::PowerFacilities ? A::SelectPower :
            PalettePage == ECityPalettePage::Build ? A::OpenPowerFacilities : A::OpenBuild;
    else if (Id == TEXT("water"))
        Target = PalettePage == ECityPalettePage::WaterFacilities ? A::SelectService :
            PalettePage == ECityPalettePage::Build ? A::OpenWaterFacilities : A::OpenBuild;
    return Action == Target;
}
