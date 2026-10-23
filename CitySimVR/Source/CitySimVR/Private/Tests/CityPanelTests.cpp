#if WITH_DEV_AUTOMATION_TESTS

#include "Interaction/CityVRToolController.h"
#include "Presentation/CityBoardActor.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Presentation/CityNeedsPresentation.h"
#include "Presentation/CityDesktopModelPreview.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityPanelsTest, "CitySim.Input.ContextPanels",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityPanelsTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues Values = UWorld::InitializationValues()
	                                                .AllowAudioPlayback(false)
	                                                .CreatePhysicsScene(false)
	                                                .CreateNavigation(false)
	                                                .CreateAISystem(false)
	                                                .ShouldSimulatePhysics(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("CityPanelsTest"), nullptr, true,
	                                    ERHIFeatureLevel::Num, &Values);
	ACityVRToolController* Controller = World->SpawnActor<ACityVRToolController>();
	ACityBoardActor* Board = World->SpawnActor<ACityBoardActor>();
	Controller->Board = Board;
	Board->BuildingCatalog = LoadObject<UCityBuildingCatalog>(
	    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
	Board->DispatchBeginPlay();
	Board->SetCommandInputBound(true);
	Board->SetActorTransform(FTransform(FRotator(0, 32, 0), FVector(-80, -40, 80), FVector(0.004)));
	AActor* CameraOwner = World->SpawnActor<AActor>();
	UCameraComponent* Camera = NewObject<UCameraComponent>(CameraOwner);
	CameraOwner->SetRootComponent(Camera);
	Camera->RegisterComponent();
	Camera->SetWorldLocation(FVector(0, 0, 160));
	Controller->HeadsetCamera = Camera;
	Controller->UpdateHeadsetStatusAttachment();
	for (int32 Index = 0; Index < Controller->TimeControlButtons.Num(); ++Index)
	{
		const FVector Normal = Controller->HeadsetStatusRoot->GetForwardVector();
		const FVector Origin = Controller->TimeControlButtons[Index]->GetComponentLocation() + Normal * 40.0;
		TestEqual(TEXT("Each visible time button resolves its own index"), Controller->FindTimeControl(Origin, -Normal),
		          Index);
	}
	Controller->RefreshNeedsVisibility();
	Controller->UpdateNeedsStatusBars();
	TestFalse(TEXT("Needs starts hidden"), Controller->NeedsText->IsVisible());
	TestFalse(TEXT("Summary starts hidden"), Controller->SummaryText->IsVisible());
	TestFalse(TEXT("Hidden panel rails cannot reappear"), Controller->NeedsCapacityRails[0]->IsVisible());
	const FVector ToggleNormal = Controller->NeedsToggleRoot->GetForwardVector();
	const FVector ToggleOrigin = Controller->NeedsToggleRoot->GetComponentLocation() + ToggleNormal * 40.0;
	TestTrue(TEXT("Chart icon is selectable while dashboard hidden"),
	         Controller->IsNeedsToggleHit(ToggleOrigin, -ToggleNormal));
	Controller->bNeedsOpen = true;
	Controller->UpdateNeedsStatusBars();
	Controller->RefreshNeedsVisibility();
	TestTrue(TEXT("Showing dashboard reveals text and rails"),
	         Controller->SummaryText->IsVisible() && Controller->NeedsCapacityRails[0]->IsVisible());
	FCityNeedsSnapshot RecoveryNeeds;
	RecoveryNeeds.ResidentialConstraint = ECityDemandConstraint::OperatingShortfall;
	RecoveryNeeds.bHasWorkforceEducation = true;
	RecoveryNeeds.WorkforceEducation.Basic = 28;
	RecoveryNeeds.WorkforceEducation.Secondary = 8;
	RecoveryNeeds.WorkforceEducation.Uneducated = 4;
	FCityEconomySnapshot RecoveryEconomy;
	RecoveryEconomy.bGrowthBlockedByShortfall = true;
	RecoveryEconomy.UpkeepShortfall = 258;
	FCityBudgetProjection RecoveryBudget;
	RecoveryBudget.RoadUpkeep = 240;
	RecoveryBudget.UtilityUpkeep = 160;
	RecoveryBudget.BuildingUpkeep = 85;
	RecoveryBudget.PublicPayroll = 40;
	Controller->NeedsText->SetText(
	    FText::FromString(FCityNeedsPresentation::BuildPanel(RecoveryNeeds, RecoveryEconomy, 0, 0, &RecoveryBudget)));
	Controller->UpdateNeedsStatusBars();
	Controller->RefreshNeedsVisibility();
	const auto RecoveryTextBounds =
	    Controller->NeedsText->CalcBounds(Controller->NeedsText->GetRelativeTransform()).GetBox();
	TestTrue(TEXT("Budget recovery text stays above utilization rails"),
	         Controller->NeedsCapacityRails[0]->GetRelativeLocation().Z < RecoveryTextBounds.Min.Z - 1.0);
	const auto RecoveryBackgroundBounds =
	    Controller->NeedsBackground->CalcBounds(Controller->NeedsBackground->GetRelativeTransform()).GetBox();
	TestTrue(TEXT("Recovery dashboard background contains expanded text and final bar"),
	         RecoveryBackgroundBounds.Min.Z < Controller->NeedsDemandRails[2]->GetRelativeLocation().Z &&
	             RecoveryBackgroundBounds.Min.Y <= RecoveryTextBounds.Min.Y &&
	             RecoveryBackgroundBounds.Max.Y >= RecoveryTextBounds.Max.Y);
	Controller->NeedsText->SetText(FText::FromString(Board->DescribeNeedsPanel()));
	Controller->UpdateNeedsStatusBars();
	Controller->RefreshNeedsVisibility();
	const FVector PanelNormal = Controller->NeedsBackground->GetForwardVector();
	const FVector PanelOrigin = Controller->NeedsBackground->GetComponentLocation() + PanelNormal * 40.0;
	TestTrue(TEXT("Visible dashboard consumes panel hits"), Controller->IsNeedsPanelHit(PanelOrigin, -PanelNormal));
	Controller->bNeedsOpen = false;
	Controller->UpdateNeedsStatusBars();
	Controller->RefreshNeedsVisibility();
	TestFalse(TEXT("Hidden dashboard has no hit surface"), Controller->IsNeedsPanelHit(PanelOrigin, -PanelNormal));
	Controller->SetCitySummaryVisible(true);
	Controller->SetPaletteOpen(true);
	Controller->UpdateNeedsStatusBars();
	Controller->RefreshNeedsVisibility();
	TestFalse(TEXT("Open drawer hides summary text"), Controller->SummaryText->IsVisible());
	TestFalse(TEXT("Open drawer hides summary rails"), Controller->NeedsCapacityRails[0]->IsVisible());
	TestFalse(TEXT("Hidden summary cannot intercept drawer input"),
	          Controller->IsNeedsPanelHit(PanelOrigin, -PanelNormal));
	Controller->SetPaletteOpen(false);
	TestTrue(TEXT("Closing drawer restores requested summary and rails"),
	         Controller->bNeedsOpen && Controller->SummaryText->IsVisible() &&
	             Controller->NeedsCapacityRails[0]->IsVisible());
	Controller->SetCitySummaryVisible(false);
	Controller->SetPaletteOpen(true);
	Controller->SetPaletteOpen(false);
	TestFalse(TEXT("Opening tools does not reopen needs"), Controller->NeedsText->IsVisible());

	Controller->UpdateObjectiveCard();
	TestTrue(TEXT("Objective card starts open outside stats"), Controller->ObjectiveText->IsVisible());
	const FVector ObjectiveNormal = Controller->ObjectiveRoot->GetForwardVector();
	const FVector ObjectiveOrigin = Controller->ObjectiveToggle->GetComponentLocation() + ObjectiveNormal * 40;
	const FVector ObjectiveBodyOrigin = Controller->ObjectiveBackground->GetComponentLocation() + ObjectiveNormal * 40;
	TestTrue(TEXT("Objective icon is selectable"), Controller->IsObjectiveToggleHit(ObjectiveOrigin, -ObjectiveNormal));
	TestTrue(TEXT("Open objective body consumes hits"),
	         Controller->IsObjectivePanelHit(ObjectiveBodyOrigin, -ObjectiveNormal));
	Controller->bObjectivesOpen = false;
	Controller->UpdateObjectiveCard();
	TestFalse(TEXT("Collapsed objective body passes rays through"),
	          Controller->IsObjectivePanelHit(ObjectiveBodyOrigin, -ObjectiveNormal));
	TestTrue(TEXT("Collapsed icon remains selectable"),
	         Controller->IsObjectiveToggleHit(ObjectiveOrigin, -ObjectiveNormal));
	TestFalse(TEXT("Collapsed objectives leave no text banner or body"),
	          Controller->ObjectiveHeader->IsVisible() || Controller->ObjectiveText->IsVisible());
	TestTrue(TEXT("Collapsed objectives retain a noncolliding checklist icon"),
	         Controller->ObjectiveToggleIcon->IsVisible() &&
	             Controller->ObjectiveToggleIcon->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
	TestFalse(TEXT("Icon hit target excludes nearby HUD space"),
	          Controller->IsObjectiveToggleHit(
	              ObjectiveOrigin + Controller->ObjectiveToggleRoot->GetRightVector() * 3.5, -ObjectiveNormal));
	Controller->bObjectiveToggleFocused = true;
	Controller->UpdateObjectiveCard();
	TestTrue(TEXT("Collapsed icon exposes its label on hover"), Controller->ObjectiveToggleHelp->IsVisible());
	Controller->SetObjectivesVisible(true);
	TestTrue(TEXT("Opening objectives restores title and body without a hover label"),
	         Controller->ObjectiveHeader->IsVisible() && Controller->ObjectiveText->IsVisible() &&
	             !Controller->ObjectiveToggleHelp->IsVisible());
	Controller->SetObjectivesVisible(false);
	TestFalse(TEXT("Collapsing objectives hides the title and body"),
	          Controller->ObjectiveHeader->IsVisible() || Controller->ObjectiveText->IsVisible());
	Controller->bObjectiveToggleFocused = false;
	Controller->bObjectivesOpen = true;
	Controller->SetPaletteOpen(true);
	Controller->UpdateObjectiveCard();
	TestFalse(TEXT("Drawer hides objective card"), Controller->ObjectiveText->IsVisible());
	TestFalse(TEXT("Hidden objective header cannot capture input"),
	          Controller->IsObjectiveToggleHit(ObjectiveOrigin, -ObjectiveNormal));
	Controller->SetPaletteOpen(false);
	Controller->UpdateObjectiveCard();
	TestTrue(TEXT("Closing drawer restores requested objective visibility"), Controller->ObjectiveText->IsVisible());

	// Check actual font bounds: empty TextRender lines previously added an
	// unscaled font height and scattered the hints well below the card.
	TArray<FCityFirstPlayableGuideState> GuideStates;
	FCityFirstPlayableGuideState Guide;
	GuideStates.Add(Guide);
	Guide.ScenarioPhase = ECityScenarioPhase::Running;
	GuideStates.Add(Guide);
	Guide.bHasConnectedEntranceRoad = true;
	GuideStates.Add(Guide);
	Guide.BuiltPowerBuildings = 1;
	GuideStates.Add(Guide);
	Guide.OperationalPowerBuildings = 1;
	GuideStates.Add(Guide);
	Guide.BuiltServiceBuildings = 1;
	GuideStates.Add(Guide);
	Guide.OperationalServiceBuildings = 1;
	GuideStates.Add(Guide);
	Guide.bHasResidentialZone = true;
	GuideStates.Add(Guide);
	Guide.Residents = 1;
	GuideStates.Add(Guide);
	Guide.bHasCommercialZone = true;
	GuideStates.Add(Guide);
	Guide.Jobs = 1;
	Guide.Residents = 57;
	GuideStates.Add(Guide);
	Guide.CompletedPurchases = 1;
	GuideStates.Add(Guide);
	Guide.PowerShortage = 5;
	GuideStates.Add(Guide);
	Guide.PowerShortage = 0;
	Guide.Residents = 100;
	Guide.ProfitableMonthStreak = 2;
	GuideStates.Add(Guide);
	Guide.ScenarioPhase = ECityScenarioPhase::Succeeded;
	GuideStates.Add(Guide);
	TArray<FCityGuidedObjective> ExtraCards;
	FCityGuidedProgress Completed;
	FCityFirstPlayableGuide::UpdateProgress(Guide, Completed);
	Guide.bHasConnectedEntranceRoad = false;
	ExtraCards.Add(FCityFirstPlayableGuide::BuildObjective(Guide, Completed));
	Guide.bHasConnectedEntranceRoad = true;
	Guide.OperationalPowerBuildings = 0;
	ExtraCards.Add(FCityFirstPlayableGuide::BuildObjective(Guide, Completed));
	Guide.OperationalPowerBuildings = 1;
	Guide.OperationalServiceBuildings = 0;
	ExtraCards.Add(FCityFirstPlayableGuide::BuildObjective(Guide, Completed));
	Guide.OperationalServiceBuildings = 1;
	Guide.WaterShortage = 250;
	Guide.UpkeepShortfall = 9000;
	ExtraCards.Add(FCityFirstPlayableGuide::BuildObjective(Guide, Completed));
	Guide = {};
	Guide.ScenarioPhase = ECityScenarioPhase::Running;
	Guide.bHasConnectedEntranceRoad = true;
	Guide.PowerCost = 600;
	Guide.Funds = 100;
	ExtraCards.Add(FCityFirstPlayableGuide::BuildObjective(Guide));
	Guide.OperationalPowerBuildings = Guide.OperationalServiceBuildings = 1;
	Guide.bCrimeIntroduced = true;
	Guide.PoliceCost = 800;
	ExtraCards.Add(FCityFirstPlayableGuide::BuildObjective(Guide));
	Guide.Funds = 1000;
	ExtraCards.Add(FCityFirstPlayableGuide::BuildObjective(Guide));
	Guide.BuiltPolice = 1;
	ExtraCards.Add(FCityFirstPlayableGuide::BuildObjective(Guide));
	for (bool Fire : {false, true})
		for (int32 Threshold : {75, 225})
		{
			FCityFirstPlayableGuideState Ready;
			Ready.Residents = Threshold - 10;
			Ready.BuiltPolice = Ready.BuiltFire = Ready.OperationalPolice = Ready.OperationalFire = 1;
			Ready.CrimePopulationThreshold = Ready.FirePopulationThreshold = Threshold;
			Ready.CrimeGraceMonths = Ready.FireGraceMonths = 3;
			ExtraCards.Add(FCityFirstPlayableGuide::BuildMilestone(Fire ? TEXT("fire") : TEXT("police"), Ready));
		}
	const FBox CardBounds =
	    Controller->ObjectiveBackground->CalcBounds(Controller->ObjectiveBackground->GetRelativeTransform()).GetBox();
	for (const FCityFirstPlayableGuideState& GuideState : GuideStates)
	{
		const FCityGuidedObjective Card = FCityFirstPlayableGuide::BuildObjective(GuideState);
		Controller->RefreshObjectiveText(Card);
		const FBox TextBounds =
		    Controller->ObjectiveText->CalcBounds(Controller->ObjectiveText->GetRelativeTransform()).GetBox();
		TestTrue(*FString::Printf(TEXT("%s: all text stays inside the background"), *Card.Id.ToString()),
		         TextBounds.Min.Y >= CardBounds.Min.Y + 1 && TextBounds.Max.Y <= CardBounds.Max.Y - 1 &&
		             TextBounds.Min.Z >= Controller->ObjectiveRail->GetRelativeLocation().Z + 1 &&
		             TextBounds.Max.Z <= CardBounds.Max.Z - .5);
	}
	for (const FName Id : FCityFirstPlayableGuide::MilestoneIds())
	{
		Controller->RefreshObjectiveText(FCityFirstPlayableGuide::BuildCompletedObjective(Id, 0, 10));
		const FBox TextBounds =
		    Controller->ObjectiveText->CalcBounds(Controller->ObjectiveText->GetRelativeTransform()).GetBox();
		TestTrue(TEXT("Completed step text stays within card"),
		         TextBounds.Min.Y >= CardBounds.Min.Y + 1 && TextBounds.Max.Y <= CardBounds.Max.Y - 1 &&
		             TextBounds.Min.Z >= Controller->ObjectiveRail->GetRelativeLocation().Z + 1);
	}
	for (const FCityGuidedObjective& Card : ExtraCards)
	{
		Controller->RefreshObjectiveText(Card);
		const FBox TextBounds =
		    Controller->ObjectiveText->CalcBounds(Controller->ObjectiveText->GetRelativeTransform()).GetBox();
		TestTrue(*FString::Printf(TEXT("%s: recovery/funds text fits card"), *Card.Title),
		         TextBounds.Min.Y >= CardBounds.Min.Y + 1 && TextBounds.Max.Y <= CardBounds.Max.Y - 1 &&
		             TextBounds.Min.Z >= Controller->ObjectiveRail->GetRelativeLocation().Z + 1 &&
		             TextBounds.Max.Z <= CardBounds.Max.Z - .5);
	}
	Controller->UpdateObjectiveCard();

	TestFalse(TEXT("Debug starts hidden"), Controller->bDebugInfoVisible);
	const bool PreviousDebugDisplay = GEngine->bEnableOnScreenDebugMessagesDisplay;
	const bool PreviousDebugGeneration = GEngine->bEnableOnScreenDebugMessages;
	Controller->SetDebugInfoVisible(false);
	TestFalse(TEXT("Hidden debug disables its on-screen rendering"),
	          bool(GEngine->bEnableOnScreenDebugMessagesDisplay));
	Controller->SetPalettePage(ECityPalettePage::Settings);
	int32 DebugIndex = INDEX_NONE;
	for (int32 Index = 0; Index < Controller->VisiblePaletteEntries.Num(); ++Index)
		if (Controller->VisiblePaletteEntries[Index].Action == ECityPaletteAction::ToggleDebugInfo)
			DebugIndex = Index;
	if (TestTrue(TEXT("Game Settings exposes debug toggle"), DebugIndex != INDEX_NONE))
	{
		Controller->ExecutePaletteAction(DebugIndex);
		TestTrue(TEXT("Settings action shows debug text"),
		         Controller->bDebugInfoVisible && GEngine->bEnableOnScreenDebugMessagesDisplay);
		TestTrue(TEXT("Toggle label exposes ON state"),
		         Controller->GetPaletteEntryLabel(Controller->VisiblePaletteEntries[DebugIndex]).Contains(TEXT("ON")));
		Controller->ExecutePaletteAction(DebugIndex);
		TestFalse(TEXT("Second action hides debug text"), bool(GEngine->bEnableOnScreenDebugMessagesDisplay));
		TestTrue(TEXT("Toggle label exposes OFF state"),
		         Controller->GetPaletteEntryLabel(Controller->VisiblePaletteEntries[DebugIndex]).Contains(TEXT("OFF")));
	}
	auto HasAction = [Controller](ECityPaletteAction Action)
	{
		return Controller->VisiblePaletteEntries.ContainsByPredicate([Action](const FCityPaletteEntry& Entry)
		                                                             { return Entry.Action == Action; });
	};
	TestFalse(TEXT("Debug off hides test navigation"), HasAction(ECityPaletteAction::OpenSafetyTests));
	TestTrue(TEXT("Debug off blocks stale test actions"),
	         Controller->IsPaletteActionDisabled(ECityPaletteAction::SelectTestFire) &&
	             Controller->IsPaletteActionDisabled(ECityPaletteAction::PreviewAlarm));
	Controller->SetDebugInfoVisible(true);
	TestTrue(TEXT("Debug on exposes test navigation"), HasAction(ECityPaletteAction::OpenSafetyTests));
	Controller->SetPalettePage(ECityPalettePage::SafetyTests);
	TestTrue(TEXT("All test controls share the debug drawer"), HasAction(ECityPaletteAction::PreviewAlarm) &&
	                                                               HasAction(ECityPaletteAction::SelectTestArson) &&
	                                                               HasAction(ECityPaletteAction::TestTornado));
	Board->SetToolMode(ECityToolMode::TestFire);
	Controller->SetDebugInfoVisible(false);
	TestEqual(TEXT("Turning debug off dismisses test drawer"), Controller->PalettePage, ECityPalettePage::Settings);
	TestEqual(TEXT("Turning debug off disarms test tool"), Board->GetToolMode(), ECityToolMode::Inspect);
	Controller->SetPalettePage(ECityPalettePage::Audio);
	TestFalse(TEXT("Normal audio settings exclude preview"), HasAction(ECityPaletteAction::PreviewAlarm));
	TestTrue(TEXT("Normal audio settings retain volume"), HasAction(ECityPaletteAction::Alarm75));
	Controller->SetPalettePage(ECityPalettePage::Root);
	Controller->SetPaletteOpen(true);
	FLinearColor SelectedColor = FLinearColor::Black, OtherColor = FLinearColor::Black;
	for (int32 I = 0; I < Controller->VisiblePaletteEntries.Num(); ++I)
	{
		if (Controller->VisiblePaletteEntries[I].Action == ECityPaletteAction::SelectInspect)
			Controller->PaletteButtonMaterials[I]->GetVectorParameterValue(FMaterialParameterInfo(TEXT("BarColor")),
			                                                               SelectedColor);
		if (Controller->VisiblePaletteEntries[I].Action == ECityPaletteAction::SelectDemolish)
			Controller->PaletteButtonMaterials[I]->GetVectorParameterValue(FMaterialParameterInfo(TEXT("BarColor")),
			                                                               OtherColor);
	}
	TestTrue(TEXT("Selected tool has a distinct blue button background"), SelectedColor.B > OtherColor.B + .3f);
	Board->SetToolMode(ECityToolMode::Demolish);
	Controller->RefreshPaletteText();
	for (int32 I = 0; I < Controller->VisiblePaletteEntries.Num(); ++I)
		if (Controller->VisiblePaletteEntries[I].Action == ECityPaletteAction::SelectDemolish)
		{
			Controller->PaletteButtonMaterials[I]->GetVectorParameterValue(FMaterialParameterInfo(TEXT("BarColor")),
			                                                               SelectedColor);
			TestTrue(TEXT("Selected demolition has orange fill"), SelectedColor.R > SelectedColor.B + .4f);
			TestEqual(TEXT("Selected demolition text keeps contrast"), Controller->PaletteLabels[I]->TextRenderColor,
			          FColor::White);
		}
	Board->SetToolMode(ECityToolMode::Inspect);
	TestTrue(TEXT("City panel moved inward"),
	         FMath::IsNearlyEqual(Controller->NeedsStatusRoot->GetRelativeLocation().Y, -15.5));
	Controller->UpdateCityTotals();
	const double TopBottom = Controller->CityTotalsBackground->Bounds.GetBox().Min.Z;
	TestTrue(TEXT("Objective and city-info icon bottoms align with top panel"),
	         FMath::IsNearlyEqual(Controller->ObjectiveToggle->Bounds.GetBox().Min.Z, TopBottom, .01) &&
	             FMath::IsNearlyEqual(Controller->NeedsToggleBackground->Bounds.GetBox().Min.Z, TopBottom, .01));
	const FBox TotalsBounds =
	    Controller->CityTotalsText->CalcBounds(Controller->CityTotalsText->GetRelativeTransform()).GetBox();
	const FBox TotalsBackgroundBounds =
	    Controller->CityTotalsBackground->CalcBounds(Controller->CityTotalsBackground->GetRelativeTransform()).GetBox();
	TestTrue(TEXT("City totals fit their top background"), TotalsBounds.Min.Y > TotalsBackgroundBounds.Min.Y &&
	                                                           TotalsBounds.Max.Y < TotalsBackgroundBounds.Max.Y &&
	                                                           TotalsBounds.Min.Z > TotalsBackgroundBounds.Min.Z &&
	                                                           TotalsBounds.Max.Z < TotalsBackgroundBounds.Max.Z);
	TestTrue(TEXT("Top info exposes treasury and population"),
	         Board->DescribeCityTotals().Contains(TEXT("TREASURY")) &&
	             Board->DescribeCityTotals().Contains(TEXT("POPULATION")));
	Controller->SetPaletteOpen(false);
	Controller->RestoreDebugInfoDisplay();
	TestEqual(TEXT("Closing the controller restores engine display state"),
	          bool(GEngine->bEnableOnScreenDebugMessagesDisplay), PreviousDebugDisplay);
	TestEqual(TEXT("Display toggle retains debug generation"), bool(GEngine->bEnableOnScreenDebugMessages),
	          PreviousDebugGeneration);

	TArray<UTextRenderComponent*> Texts;
	Controller->GetComponents(Texts);
	for (UTextRenderComponent* Text : Texts)
	{
		TestTrue(TEXT("Every UI text component is unlit"),
		         Text->GetMaterial(0) &&
		             Text->GetMaterial(0)->GetMaterial()->GetShadingModels().HasShadingModel(MSM_Unlit));
	}
	for (UStaticMeshComponent* Panel :
	     {Controller->PaletteBackground.Get(), Controller->PaletteButtons[0].Get(),
	      Controller->ObjectiveBackground.Get(), Controller->ObjectiveFill.Get(), Controller->InspectorBackground.Get(),
	      Controller->NeedsBackground.Get(), Controller->NeedsToggleIcon.Get()})
	{
		TestTrue(TEXT("Panel chrome is unlit"),
		         Panel->GetMaterial(0) &&
		             Panel->GetMaterial(0)->GetMaterial()->GetShadingModels().HasShadingModel(MSM_Unlit));
	}

	UCitySimulationSubsystem* Simulation = World->GetSubsystem<UCitySimulationSubsystem>();
	FCityPlacementModel& Model = Simulation->GetPlacementModel();
	// Keep placement eligibility independent of procedural terrain in this presentation fixture.
	Model.SetTerrain(nullptr);
	const FCityBuildingDefinition* Definition = Board->BuildingCatalog->FindDefinition(TEXT("Home"));
	if (!TestNotNull(TEXT("Home definition exists"), Definition))
	{
		World->DestroyWorld(false);
		return false;
	}
	FCityPlacementPreview Preview = Model.BeginPreview(*Definition, FCityCellCoord(20, 20));
	const auto Placed = Model.CommitPlacement(Preview, *Definition);
	if (!TestTrue(TEXT("Fixture home placed"), Placed.IsSuccess()))
	{
		World->DestroyWorld(false);
		return false;
	}
	const int64 FundsBeforeStart = Model.GetFunds();
	Simulation->StartScenario();
	Simulation->Guidance.CompletedIds = {TEXT("entrance_road"), TEXT("power")};
	Simulation->Guidance.CurrentId = TEXT("water");
	Controller->UpdateObjectiveCard();
	const FVector HistoryOrigin = Controller->ObjectiveFooterButtons[0]->GetComponentLocation() + ObjectiveNormal * 40;
	TestEqual(TEXT("History footer has its own hit target"),
	          Controller->FindObjectiveFooter(HistoryOrigin, -ObjectiveNormal), 0);
	const FName StageBeforeReview = Simulation->GetGuidedProgress().CurrentId;
	Controller->ExecuteObjectiveFooter(0);
	TestTrue(TEXT("History opens completed steps"), Controller->bObjectiveReview);
	Controller->ExecuteObjectiveFooter(1);
	TestEqual(TEXT("Next cycles completed steps"), Controller->ObjectiveReviewIndex, 1);
	Controller->ExecuteObjectiveFooter(1);
	TestEqual(TEXT("History cycles at end"), Controller->ObjectiveReviewIndex, 0);
	TestEqual(TEXT("History is presentation only"), Simulation->GetGuidedProgress().CurrentId, StageBeforeReview);
	Controller->bObjectivesOpen = false;
	Controller->UpdateObjectiveCard();
	TestEqual(TEXT("Collapsed history cannot capture rays"),
	          Controller->FindObjectiveFooter(HistoryOrigin, -ObjectiveNormal), INDEX_NONE);
	Controller->bObjectivesOpen = true;
	Controller->ExecuteObjectiveFooter(0);
	TestFalse(TEXT("Live returns to next action"), Controller->bObjectiveReview);
	Simulation->Guidance = {};

	TestTrue(TEXT("Starting guidance preserves an existing city"), Model.GetEntities().Contains(Placed.EntityId));
	TestEqual(TEXT("Starting guidance preserves funds"), Model.GetFunds(), FundsBeforeStart);
	Board->RenderPlacement(Placed.EntityId, Model.GetEntities().FindChecked(Placed.EntityId), *Definition);
	FVector Roof;
	FString Detail;
	TestTrue(TEXT("Stable entity resolves rendered roof"), Board->GetInspectionBuilding(Placed.EntityId, Roof, Detail));
	const FVector Origin = Roof + FVector(0, 0, 50);
	uint64 HitId = 0;
	FVector Hit;
	TestTrue(TEXT("Ray hits transformed building"),
	         Board->PickInspectionBuilding(Origin, -FVector::UpVector, HitId, Hit));
	TestEqual(TEXT("Rendered hit resolves authoritative identity"), HitId, Placed.EntityId);
	Controller->UpdateInspectionTooltip();
	TestEqual(TEXT("Hover/query alone does not select"), Controller->InspectedEntityId, uint64(0));
	Controller->ToggleInspection(Origin, -FVector::UpVector);
	TestEqual(TEXT("Trigger selects building"), Controller->InspectedEntityId, Placed.EntityId);
	TestTrue(TEXT("Selected tooltip is shown"), Controller->InspectorText->IsVisible());
	FCityPopulationSnapshot Fixture;
	FCityHouseholdRecord Household;
	Household.Id = Household.HomeEntityId = Placed.EntityId;
	Household.Balance = 12;
	Household.CompletedPurchases = 2;
	Household.ConsumptionOrders = 4;
	Fixture.Households.Add(Household);
	FString RestoreError;
	TestTrue(TEXT("inspection fixture restores"), Simulation->Population.RestoreState(Fixture, RestoreError));
	Controller->UpdateInspectionTooltip();
	TestTrue(TEXT("summary exposes savings"),
	         Controller->InspectorText->Text.ToString().Contains(TEXT("Shared savings $12")));
	TestEqual(TEXT("three bounded metric rows"), Controller->InspectorMetricRails.Num(), 3);
	// Utilities precede work/finance metrics; locate the semantic row instead of
	// assuming that purchase/staff metrics always occupy a fixed rail index.
	const auto GaugeIndex = [&](const TCHAR* Label) {
		return Controller->SharedInspectionGauges.IndexOfByPredicate(
		    [&](const auto& Gauge) { return Gauge.Label.ToString() == Label; });
	};
	const int32 PurchaseIndex = GaugeIndex(TEXT("Purchases last day"));
	TestTrue(TEXT("purchase gauge exposes exact values"), PurchaseIndex != INDEX_NONE &&
	         Controller->SharedInspectionGauges[PurchaseIndex].Value.ToString() == TEXT("2 / 4"));
	if (TestTrue(TEXT("purchase metric has a rendered rail"), Controller->InspectorMetricRails.IsValidIndex(PurchaseIndex)))
	{
		TestTrue(TEXT("purchase fill visible"), Controller->InspectorMetricFills[PurchaseIndex]->IsVisible());
		TestTrue(TEXT("half purchases fill half rail"),
		         FMath::IsNearlyEqual(Controller->InspectorMetricFills[PurchaseIndex]->GetRelativeScale3D().Y * 2,
		                              Controller->InspectorMetricRails[PurchaseIndex]->GetRelativeScale3D().Y));
	}
	const int32 AtHomeIndex = GaugeIndex(TEXT("At home now"));
	if (TestTrue(TEXT("occupancy metric has a rendered rail"), Controller->InspectorMetricRails.IsValidIndex(AtHomeIndex)))
		TestFalse(TEXT("zero-capacity occupancy hides fill"), Controller->InspectorMetricFills[AtHomeIndex]->IsVisible());
	FCityPopulationSnapshot FactoryFixture = Fixture;
	FCityBusinessLedgerRecord Business;
	Business.EntityId = Placed.EntityId;
	Business.bIndustrial = true;
	Business.JobCapacity = 5;
	Business.PendingOrders = 7;
	Business.ProducedGoods = 2;
	Business.PayrollDue = 10;
	Business.PayrollPaid = 5;
	FactoryFixture.Businesses.Add(Business);
	for (uint64 Id = 1; Id <= 2; ++Id)
	{
		FCityCitizenRecord Citizen;
		Citizen.Id = Id;
		Citizen.HomeEntityId = Citizen.HouseholdId = Citizen.WorkplaceEntityId = Placed.EntityId;
		FactoryFixture.Citizens.Add(Citizen);
	}
	FactoryFixture.NextCitizenId = 3;
	TestTrue(TEXT("factory metric fixture restores"),
	         Simulation->Population.RestoreState(FactoryFixture, RestoreError));
	Controller->UpdateInspectionTooltip();
	const auto CheckGauge = [&](const TCHAR* Label, const TCHAR* Expected) {
		const int32 Index = GaugeIndex(Label);
		TestTrue(Label, Index != INDEX_NONE && Controller->SharedInspectionGauges[Index].Value.ToString() == Expected);
	};
	CheckGauge(TEXT("Staff assigned"), TEXT("2 / 5"));
	CheckGauge(TEXT("Output next day"), TEXT("2 / 7"));
	CheckGauge(TEXT("Payroll last day $"), TEXT("5 / 10"));
	TestTrue(TEXT("home fixture restores"), Simulation->Population.RestoreState(Fixture, RestoreError));
	Controller->UpdateInspectionTooltip();
	const FVector FirstPosition = Controller->InspectorStatusRoot->GetComponentLocation();
	Board->AddActorWorldOffset(FVector(20, 0, 0));
	Controller->UpdateInspectionTooltip();
	TestFalse(TEXT("Tooltip follows moved board"),
	          Controller->InspectorStatusRoot->GetComponentLocation().Equals(FirstPosition));
	const FVector TowardHead =
	    (Camera->GetComponentLocation() - Controller->InspectorStatusRoot->GetComponentLocation()).GetSafeNormal();
	TestTrue(TEXT("Tooltip faces headset"),
	         FVector::DotProduct(TowardHead, Controller->InspectorStatusRoot->GetForwardVector()) > 0.999);
	Board->GetInspectionBuilding(Placed.EntityId, Roof, Detail);
	Controller->ToggleInspection(Roof + FVector(0, 0, 50), -FVector::UpVector);
	TestEqual(TEXT("Inspecting same entity preserves selection"), Controller->InspectedEntityId, Placed.EntityId);
	Controller->ToggleInspection(Roof + FVector(0, 0, 50), -FVector::UpVector);
	Controller->SetPaletteOpen(true);
	Controller->UpdateInspectionTooltip();
	TestFalse(TEXT("Drawer suppresses tooltip visually"), Controller->InspectorText->IsVisible());
	TestFalse(TEXT("Drawer suppresses metric fills"), Controller->InspectorMetricFills[1]->IsVisible());
	Controller->SetPaletteOpen(false);
	Controller->UpdateInspectionTooltip();
	TestTrue(TEXT("Selection survives drawer close"), Controller->InspectorText->IsVisible());
	Controller->SetCitySummaryVisible(true);
	++Simulation->PresentationGeneration;
	Board->PrepareCityView();
	Controller->UpdateInspectionTooltip();
	TestEqual(TEXT("City refresh preserves inspected entity"), Controller->InspectedEntityId, Placed.EntityId);
	Controller->bDesktopUi = true;
	FCityDesktopModelSource PreviewSource;
	TestTrue(TEXT("City refresh retains actual inspector mesh source"),
	         Controller->GetDesktopModelSource(PreviewSource) && PreviewSource.EntityId == Placed.EntityId && PreviewSource.Mesh);
	Controller->bDesktopUi = false;
	TestTrue(TEXT("Inspection suppresses summary without changing preference"),
	         Controller->bNeedsOpen && !Controller->SummaryText->IsVisible());
	Controller->ClearInspection();
	TestTrue(TEXT("Explicit dismissal restores enabled summary"), Controller->SummaryText->IsVisible());
	Controller->SetCitySummaryVisible(false);
	Controller->InspectBuilding(Placed.EntityId);
	Controller->ClearInspection();
	TestFalse(TEXT("Dismissal preserves disabled summary preference"), Controller->SummaryText->IsVisible());
	Controller->InspectBuilding(Placed.EntityId);
	Model.DemolishEntity(Placed.EntityId);
	Controller->UpdateInspectionTooltip();
	TestEqual(TEXT("Demolition clears selected identity"), Controller->InspectedEntityId, uint64(0));
	TestFalse(TEXT("Demolition hides tooltip"), Controller->InspectorText->IsVisible());
	// A load/reset can reuse the same entity ID; generation must invalidate it.
	Controller->InspectedEntityId = Placed.EntityId;
	Controller->InspectionGeneration = Simulation->GetCitySessionGeneration() + 1;
	Controller->bDesktopUi = true;
	TestFalse(TEXT("City replacement rejects stale model preview identity"), Controller->GetDesktopModelSource(PreviewSource));
	Controller->bDesktopUi = false;
	Controller->UpdateInspectionTooltip();
	TestEqual(TEXT("City replacement invalidates selection"), Controller->InspectedEntityId, uint64(0));
	Board->SetToolMode(ECityToolMode::Road);
	const FCityCellCoord Start = Model.GetEntranceCell();
	const FCityCellCoord End(Start.X + 2, Start.Y);
	Board->UpdateRoadTool(nullptr, Start, true, true, false);
	TestTrue(TEXT("Road start becomes active"), Board->RoadPreview.bActive);
	++Simulation->PresentationGeneration;
	Board->PrepareCityView();
	// An unrelated authoritative edit also changes the optimistic command revision.
	auto Unrelated = Model.BeginPreview(*Definition, {24, 24});
	TestTrue(TEXT("Unrelated building changes model"), Model.CommitPlacement(Unrelated, *Definition).IsSuccess());
	Board->UpdateRoadTool(nullptr, End, true, false, false);
	TestTrue(TEXT("Refresh retains start and rebuilds preview"),
	         Board->RoadPreview.bActive && Board->RoadPreview.Start == Start &&
	         Board->RoadPreviewInstances->GetInstanceCount() > 0);
	TestEqual(TEXT("Preview revalidates current model revision"),
	          Board->RoadPreview.ExpectedRevision, Model.GetRevision());
	Board->UpdateRoadTool(nullptr, End, true, true, false);
	TestTrue(TEXT("Road completes after city refresh"), Model.IsRoad(End));
	Board->UpdateRoadTool(nullptr, Start, true, true, false);
	++Simulation->CitySessionGeneration;
	++Simulation->PresentationGeneration;
	Board->PrepareCityView();
	TestFalse(TEXT("City replacement clears old road start"), Board->RoadPreview.bActive);
	TestTrue(TEXT("Primary hover cost includes full public payroll"),
	         Board->DescribeConstructionCost(TEXT("PrimarySchool")).Contains(TEXT("Build $600 | Daily $40")));
	TestTrue(TEXT("Wind hover cost includes full public payroll"),
	         Board->DescribeConstructionCost(TEXT("WindPower")).Contains(TEXT("Build $600 | Daily $25")));
	TestTrue(TEXT("Road costs use tile units"),
	         Board->DescribeConstructionCost(TEXT("LocalRoad")).Contains(TEXT("Build $10/tile | Daily $1/tile")));
	for (int32 AnchorMode : {0, 2})
	{
		Controller->PaletteAnchorMode = AnchorMode;
		for (auto Page :
		     {ECityPalettePage::Build, ECityPalettePage::Parks, ECityPalettePage::EducationFacilities,
		      ECityPalettePage::PowerFacilities, ECityPalettePage::WaterFacilities, ECityPalettePage::Roads})
		{
			Controller->SetPalettePage(Page);
			for (int32 I = 0; I < Controller->VisiblePaletteEntries.Num(); ++I)
			{
				const FName Id = FCityPaletteModel::ConstructionDefinition(Controller->VisiblePaletteEntries[I].Action);
				if (Id.IsNone())
					continue;
				Controller->HoveredPaletteIndex = I;
				Controller->RefreshPaletteText();
				TestTrue(TEXT("Constructible hover shows build and daily costs"),
				         Controller->PaletteHelp->Text.ToString().Contains(TEXT("Build $")) &&
				             Controller->PaletteHelp->Text.ToString().Contains(TEXT("Daily $")));
				const auto HelpBounds =
				    Controller->PaletteHelp->CalcBounds(Controller->PaletteHelp->GetRelativeTransform()).GetBox();
				const auto BackBounds =
				    Controller->PaletteBackground->CalcBounds(Controller->PaletteBackground->GetRelativeTransform())
				        .GetBox();
				TestTrue(TEXT("Cost description stays inside drawer background"),
				         BackBounds.Min.Z <= HelpBounds.Min.Z && BackBounds.Min.Y <= HelpBounds.Min.Y &&
				             BackBounds.Max.Y >= HelpBounds.Max.Y);
			}
		}
	}
	Controller->PaletteAnchorMode = 0;
	Controller->SetPaletteOpen(false);
	Board->SetToolMode(ECityToolMode::Power);
	Model.TrySpendFunds(Model.GetFunds());
	const auto* Power = Board->BuildingCatalog->FindDefinition(TEXT("Power"));
	Board->PlacementPreview = Model.BeginPreview(*Power, {1, 1});
	FCityPlacementResult TooDear;
	TooDear.Failure = ECityPlacementFailure::InsufficientFunds;
	Board->UpdateGhost(*Power, TooDear);
	Board->UpdatePlacementStatus(*Power, TooDear);
	Controller->UpdateControllerRay();
	TestTrue(TEXT("Price rejection is explicit"),
	         Board->IsConstructionPriceBlocked() &&
	             Board->GetConstructionStatus().Contains(TEXT("INSUFFICIENT FUNDS")));
	TestTrue(TEXT("Crossed dollar is visible for unaffordable previews"),
	         Controller->PriceBlockedIcon->IsVisible() && Controller->PriceBlockedSlash->IsVisible());

	FVector CostAnchor;
	TestTrue(TEXT("Building preview supplies a world anchor"), Board->GetConstructionMarkerAnchor(CostAnchor));
	TestTrue(TEXT("Cost leader visible and non-colliding"),
	         Controller->ConstructionLeader->IsVisible() &&
	             Controller->ConstructionLeader->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
	const FVector LeaderStart =
	    Controller->ConstructionLeader->GetComponentLocation() -
	    Controller->ConstructionLeader->GetForwardVector() * Controller->ConstructionLeader->GetComponentScale().X * 50;
	TestTrue(TEXT("Cost pointer reaches the actual transformed preview"), LeaderStart.Equals(CostAnchor, .01));
	TestEqual(TEXT("Unaffordable placement callout is red"), Controller->RoadStatusText->TextRenderColor,
	          FColor(255, 100, 80));
	Controller->SetPaletteOpen(true);
	Controller->UpdateConstructionCallout();
	TestFalse(TEXT("Drawer suppresses cost callout and leader"),
	          Controller->RoadStatusText->IsVisible() || Controller->ConstructionLeader->IsVisible());
	Controller->SetPaletteOpen(false);
	Controller->UpdateConstructionCallout();
	Board->PlacementGhost->SetVisibility(false);
	Controller->UpdateControllerRay();
	TestFalse(TEXT("No preview clears the price indicator"), Controller->PriceBlockedIcon->IsVisible());

	TestFalse(TEXT("Hidden preview also clears its world leader"), Controller->ConstructionLeader->IsVisible());
	Board->SetToolMode(ECityToolMode::Road);
	Board->UpdateRoadTool(nullptr, {5, 8}, true, true, false);
	Board->UpdateRoadTool(nullptr, {12, 8}, true, false, false);
	Controller->UpdateConstructionCallout();
	TestTrue(TEXT("Road span cost is attached to its actual endpoint"),
	         Board->GetConstructionMarkerAnchor(CostAnchor) && Controller->ConstructionLeader->IsVisible());
	TestTrue(TEXT("Unaffordable road cost warns explicitly"),
	         Controller->RoadStatusText->Text.ToString().Contains(TEXT("INSUFFICIENT FUNDS")) &&
	             Controller->RoadStatusText->TextRenderColor == FColor(255, 100, 80));
	Model.ApplyMonthlyEconomy(100000, 0);
	Board->UpdateRoadTool(nullptr, {12, 8}, true, false, false);
	Controller->UpdateConstructionCallout();
	TestEqual(TEXT("Affordable road callout returns to normal"), Controller->RoadStatusText->TextRenderColor,
	          FColor::White);
	Board->UpdateRoadTool(nullptr, {12, 8}, false, false, true);
	Controller->UpdateConstructionCallout();
	TestFalse(TEXT("Cancel away from board clears span callout, leader and funds icon"),
	          Controller->RoadStatusText->IsVisible() || Controller->ConstructionLeader->IsVisible() ||
	              Controller->PriceBlockedIcon->IsVisible());
	Board->SetToolMode(ECityToolMode::Inspect);
	Controller->UpdateConstructionCallout();
	TestFalse(TEXT("Tool switch clears cost status"), Controller->RoadStatusText->IsVisible());
	Controller->SetCitySummaryVisible(true);
	Controller->UpdateCityInfoCards();
	Controller->RefreshNeedsVisibility();
	TestEqual(TEXT("Six grouped exact-value city cards"), Controller->CityInfoTitles.Num(), 6);
	TestTrue(TEXT("City cards separate headings from numeric values"),
	         Controller->CityInfoTitles[0]->Text.ToString() == TEXT("PEOPLE") &&
	             !Controller->CityInfoValues[0]->Text.ToString().Contains(TEXT("Homeless")));
	TestTrue(TEXT("Objectives are brought inward"), Controller->ObjectiveRoot->GetRelativeLocation().Y > -43);
	TestTrue(TEXT("Growth explanations have a matching container and distinct heading"),
	         Controller->GrowthNeedsBackground->IsVisible() &&
	             Controller->GrowthNeedsTitle->Text.ToString() == TEXT("NEEDS FOR GROWTH") &&
	             !Controller->NeedsText->Text.ToString().StartsWith(TEXT("GROWTH LIMITS")));
	const auto GuidanceTextBounds =
	    Controller->NeedsText->CalcBounds(Controller->NeedsText->GetRelativeTransform()).GetBox();
	const auto GuidancePanelBounds =
	    Controller->GrowthNeedsBackground->CalcBounds(Controller->GrowthNeedsBackground->GetRelativeTransform())
	        .GetBox();
	TestTrue(TEXT("Growth card includes full explanations and leaves text in front"),
	         GuidancePanelBounds.Min.Z < GuidanceTextBounds.Min.Z &&
	             GuidancePanelBounds.Max.Z > GuidanceTextBounds.Max.Z &&
	             GuidanceTextBounds.Min.X > GuidancePanelBounds.Max.X);
	const auto TopPanelBounds =
	    Controller->CityTotalsBackground->CalcBounds(Controller->CityTotalsBackground->GetRelativeTransform()).GetBox();
	TestTrue(TEXT("Calendar has a real depth gap ahead of the panel front"),
	         Controller->CalendarText->GetRelativeLocation().X > TopPanelBounds.Max.X + .4);
	Controller->SetCitySummaryVisible(false);
	TestFalse(TEXT("Closing overview hides numeric cards"),
	          Controller->CityInfoTitles[0]->IsVisible() || Controller->CityInfoBackgrounds[0]->IsVisible());
	TestFalse(TEXT("Closing overview hides growth card and heading"),
	          Controller->GrowthNeedsBackground->IsVisible() || Controller->GrowthNeedsTitle->IsVisible());
	World->DestroyWorld(false);
	return true;
}

#endif
