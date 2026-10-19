#include "Presentation/CityDesktopHud.h"
#include "Presentation/CityDesktopIcon.h"
#include "Presentation/CityBoardActor.h"
#include "Presentation/CityConditionIndicators.h"
#include "Interaction/CityVRToolController.h"
#include "Interaction/CityBoardPicking.h"
#include "Sandbox/CitySandboxDesktopPawn.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"

void UCityDesktopHud::PresentConditions()
{
	if (!Session.IsValid() || !WidgetTree)
		return;
	auto* UI = Session.Get();
	auto* Sim = GetWorld()->GetSubsystem<UCitySimulationSubsystem>();
	auto* Player = GetWorld()->GetFirstPlayerController();
	auto* Canvas = Cast<UCanvasPanel>(WidgetTree->FindWidget(TEXT("ConditionCanvas")));
	if (!Canvas || !Sim || !Player || !UI->Board)
		return;
	if (ConditionSession != Sim->GetCitySessionGeneration())
	{
		ConditionSession = Sim->GetCitySessionGeneration();
		LastConditionTarget = 0;
	}
	ConditionTargets.Reset();
	AllConditionTargets.Reset();
	const auto* Pawn = Cast<ACitySandboxDesktopPawn>(Player->GetPawn());
	const bool Show = UI->bConditionIndicatorsVisible && !UI->bEntrySceneActive &&
	                  UI->Board->HasCurrentCityModel() &&
	                  (!UI->bPaletteOpen || UI->IsDesktopZoningPaletteActive()) &&
	                  !UI->bManipulationSession && UI->LifecycleInputGate.CanAcceptInput();
	const bool Interactive = Show && UI->bDesktopFocused && !UI->Board->HasActiveToolAction() &&
	                         !(Pawn && Pawn->IsCameraGestureActive());
	// Pixels returned by the active camera become logical HUD units exactly once.
	const double Dpi = UWidgetLayoutLibrary::GetViewportScale(this);
	const FVector2D View = UWidgetLayoutLibrary::GetViewportSize(this) / FMath::Max(.01, Dpi);
	TArray<FBox2D> Occluded;
	const auto CanvasGeometry = Canvas->GetCachedGeometry();
	for (const TCHAR* Name :
	     {TEXT("TopPanel"), TEXT("ToolbarPanel"), TEXT("ToolContextPanel"), TEXT("InspectorPanel"),
	      TEXT("InfoPanel"), TEXT("ObjectivesPanel"), TEXT("MenuPanel")})
		if (const auto* Panel = WidgetTree->FindWidget(FName(Name)); Panel && Panel->IsVisible())
		{
			const auto Geometry = Panel->GetCachedGeometry();
			const FVector2D Min = CanvasGeometry.AbsoluteToLocal(Geometry.GetAbsolutePosition());
			Occluded.Emplace(Min, Min + Geometry.GetLocalSize());
		}
	TArray<FCityConditionIndicator> Rows;
	if (Show)
		Rows = FCityConditionIndicators::Build(
		    Sim->GetPlacementModel(), Sim->GetFireSnapshot(), Sim->GetCrimeSnapshot(),
		    Sim->GetEconomyTuning().FireRepairSteps, &Sim->GetHealthSnapshot(), &Sim->GetPopulationSnapshot(),
		    0, &Sim->GetWasteSnapshot(), &Sim->GetServiceSnapshot(), MAX_int32, 0);
	for (const auto& Row : Rows)
		AllConditionTargets.Append(Row.EntityIds);
	const auto* Selected =
	    Rows.FindByPredicate([&](const auto& R) { return R.EntityId == UI->InspectedEntityId; });
	const auto Symbols = Selected ? Selected->Symbols() : TArray<FName>();
	FString RelatedLabel;
	const uint64 Related =
	    Selected ? FCityConditionIndicators::RelatedLot(*Sim, UI->InspectedEntityId, RelatedLabel) : 0;
	if (auto* Button = Cast<UCityDesktopActionButton>(WidgetTree->FindWidget(TEXT("ConditionProvider"))))
	{
		Button->Configure(UI, -61);
		Button->SetVisibility(Related ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		Button->SetIsEnabled(Interactive && UI->Board->GetToolMode() == ECityToolMode::Inspect);
		Button->SetPresentation(FLinearColor(.3, .75, .9), false);
	}
	if (auto* Label = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("ConditionProviderLabel"))))
		Label->SetText(FText::FromString(RelatedLabel));
	for (int32 I = 0; I < 10; ++I)
		if (auto* Icon = Cast<UCityDesktopIcon>(
		        WidgetTree->FindWidget(FName(FString::Printf(TEXT("ConditionDetail%d"), I)))))
		{
			Icon->SetVisibility(Symbols.IsValidIndex(I) ? ESlateVisibility::Visible
			                                            : ESlateVisibility::Collapsed);
			if (Symbols.IsValidIndex(I))
			{
				Icon->SetConditionIcon(Symbols[I], FLinearColor(1, .84, .48));
				Icon->SetToolTipText(FText::FromName(Symbols[I]));
			}
		}
	struct FProjected
	{
		FCityConditionIndicator Row;
		FVector2D Position;
	};
	TArray<FProjected> Visible;
	for (const auto& Row : Rows)
	{
		if (Row.EntityId == UI->InspectedEntityId)
			continue;
		FVector Roof;
		FVector2D Pixel;
		if (!UI->Board->GetBuildingMarkerAnchor(Row.EntityId, Roof) ||
		    !Player->ProjectWorldLocationToScreen(Roof, Pixel, true))
			continue;
		const FVector2D Position = Pixel / Dpi - FVector2D(36, 50);
		const FBox2D Bounds(Position, Position + FVector2D(72, 42));
		if (Bounds.Min.X < 8 || Bounds.Min.Y < 8 || Bounds.Max.X > View.X - 8 || Bounds.Max.Y > View.Y - 8 ||
		    Occluded.ContainsByPredicate([&](const auto& Box) { return Bounds.Intersect(Box); }))
			continue;
		// Close views separate lots naturally. At distance combine compatible overlapping
		// symbols, retaining every member; no screen-space chain across neighborhoods.
		auto* Cluster = Visible.FindByPredicate([&](const auto& Other) {
			return Other.Row.CauseMask() == Row.CauseMask() &&
			       Other.Row.OperationFailure == Row.OperationFailure &&
			       Other.Row.SettledConcerns == Row.SettledConcerns &&
			       FVector2D::Distance(Other.Position, Position) < 76 &&
			       (Other.Row.RegionBounds + Row.RegionBounds).GetSize().Size() <=
			           FCityConditionIndicators::MaximumClusterDiameterCells;
		});
		if (Cluster)
		{
			Cluster->Row.EntityIds.Append(Row.EntityIds);
			Cluster->Row.RegionBounds += Row.RegionBounds;
		}
		else if (!Visible.ContainsByPredicate([&](const auto& Other) {
			         return Bounds.Intersect(FBox2D(Other.Position, Other.Position + FVector2D(72, 42)));
		         }))
			Visible.Add({Row, Position});
	}
	int32 DisplayedLots = 0;
	for (int32 I = 0; I < FCityConditionIndicators::MaximumBadges; ++I)
	{
		auto* Button = Cast<UCityDesktopActionButton>(
		    WidgetTree->FindWidget(FName(FString::Printf(TEXT("Condition%d"), I))));
		if (!Button)
			continue;
		if (!Visible.IsValidIndex(I))
		{
			Button->SetVisibility(ESlateVisibility::Collapsed);
			continue;
		}
		const auto& Item = Visible[I];
		const auto& Row = Item.Row;
		ConditionTargets.Add(Row.EntityId);
		DisplayedLots += Row.EntityIds.Num();
		Button->Configure(UI, -100 - I);
		Button->SetVisibility(Interactive && UI->Board->GetToolMode() == ECityToolMode::Inspect
		                          ? ESlateVisibility::Visible
		                          : ESlateVisibility::HitTestInvisible);
		Button->SetPresentation(Row.Priority() < 2 ? FLinearColor(1, .3, .2) : FLinearColor(1, .75, .25),
		                        false);
		auto Style = Button->GetStyle();
		if (Style.NormalPadding != FMargin(3, 6))
		{
			Style.NormalPadding = Style.PressedPadding = FMargin(3, 6);
			Button->SetStyle(Style);
		}
		Cast<UCanvasPanelSlot>(Button->Slot)->SetPosition(Item.Position);
		Button->SetToolTipText(FText::FromString(
		    Row.Title(Sim->GetRepairCost(Row.EntityId)) + TEXT("\n") + Row.Detail() +
		    (UI->Board->GetToolMode() == ECityToolMode::Inspect ? TEXT("\nSelect to inspect")
		                                                        : TEXT("\nUse Inspect or Next issue"))));
		if (auto* Icon = Cast<UCityDesktopIcon>(
		        WidgetTree->FindWidget(FName(FString::Printf(TEXT("Condition%dIcon"), I)))))
			Icon->SetConditionIcon(Row.Symbol(), FLinearColor(1, .84, .48));
		if (auto* Count =
		        Cast<UTextBlock>(WidgetTree->FindWidget(FName(FString::Printf(TEXT("Condition%dLabel"), I)))))
			Count->SetText(FText::FromString(
			    Row.EntityIds.Num() > 1             ? FString::Printf(TEXT("%d lots"), Row.EntityIds.Num())
			    : Row.Symbol() == TEXT("Repairing") ? FString::Printf(TEXT("%d%%"), Row.RepairPercent)
			    : Row.CauseCount() > 1              ? FString::Printf(TEXT("+%d"), Row.CauseCount() - 1)
			                                        : TEXT("!")));
	}
	if (auto* Next = Cast<UCityDesktopActionButton>(WidgetTree->FindWidget(TEXT("ConditionNext"))))
	{
		Next->Configure(UI, -60);
		Next->SetVisibility(Show && !Rows.IsEmpty() ? ESlateVisibility::Visible
		                                            : ESlateVisibility::Collapsed);
		Next->SetIsEnabled(Interactive);
		Next->SetPresentation(FLinearColor(1, .75, .25), false);
		Next->SetToolTipText(FText::FromString(TEXT("Focus the next affected lot, including issues outside "
		                                            "this view. Opens Inspect; no remedy is executed.")));
	}
	if (auto* Label = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("ConditionNextLabel"))))
		Label->SetText(FText::FromString(FString::Printf(TEXT("%d lots | %d shown  > Next issue"),
		                                                 AllConditionTargets.Num(), DisplayedLots)));
}

void UCityDesktopHud::SelectCondition(const int32 Index)
{
	if (!Session.IsValid())
		return;
	auto* UI = Session.Get();
	auto* Sim = GetWorld()->GetSubsystem<UCitySimulationSubsystem>();
	auto* Player = GetWorld()->GetFirstPlayerController();
	const auto* Pawn = Player ? Cast<ACitySandboxDesktopPawn>(Player->GetPawn()) : nullptr;
	if (!Sim || ConditionSession != Sim->GetCitySessionGeneration() || !UI->Board || UI->bEntrySceneActive ||
	    (UI->bPaletteOpen && !UI->IsDesktopZoningPaletteActive()) || !UI->bDesktopFocused ||
	    !UI->LifecycleInputGate.CanAcceptInput() || !UI->bConditionIndicatorsVisible ||
	    UI->Board->HasActiveToolAction() || (Pawn && Pawn->IsCameraGestureActive()))
		return;
	uint64 Id = 0;
	if (Index == INDEX_NONE && !AllConditionTargets.IsEmpty())
		Id = AllConditionTargets[(AllConditionTargets.IndexOfByKey(LastConditionTarget) + 1) %
		                         AllConditionTargets.Num()];
	else if (ConditionTargets.IsValidIndex(Index) && UI->Board->GetToolMode() == ECityToolMode::Inspect)
		Id = ConditionTargets[Index];
	FVector Roof;
	if (!Id || !UI->Board->GetBuildingMarkerAnchor(Id, Roof))
		return;
	// Revalidate the condition as well as identity after a repair/demolition/settlement.
	const auto Current = FCityConditionIndicators::Build(
	    Sim->GetPlacementModel(), Sim->GetFireSnapshot(), Sim->GetCrimeSnapshot(),
	    Sim->GetEconomyTuning().FireRepairSteps, &Sim->GetHealthSnapshot(), &Sim->GetPopulationSnapshot(), 0,
	    &Sim->GetWasteSnapshot(), &Sim->GetServiceSnapshot(), MAX_int32, 0);
	if (!Current.ContainsByPredicate([&](const auto& R) { return R.EntityId == Id; }))
		return;
	if (UI->IsDesktopZoningPaletteActive())
		UI->SetPaletteOpen(false);
	UI->SelectInspect();
	UI->InspectBuilding(static_cast<int64>(Id));
	LastConditionTarget = Id;
	if (Index == INDEX_NONE)
		if (auto* Camera = Player ? Cast<ACitySandboxDesktopPawn>(Player->GetPawn()) : nullptr)
			Camera->FocusCondition(Roof);
}
