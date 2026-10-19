#include "Presentation/CityDesktopHud.h"
#include "Presentation/CityDesktopChart.h"
#include "Presentation/CityDesktopNewspaper.h"
#include "Presentation/CityDesktopIcon.h"
#include "Presentation/CityDesktopModelPreview.h"
#include "Presentation/CityDesktopTerrainPreview.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/WrapBox.h"
#include "Components/ScrollBox.h"
#include "Framework/Application/SlateApplication.h"
#include "Interaction/CityVRToolController.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "ProfilingDebugging/ScopedTimers.h"
#include "Brushes/SlateRoundedBoxBrush.h"

UCityDesktopActionButton::UCityDesktopActionButton()
{
	InitIsFocusable(false);
}

void UCityDesktopActionButton::SetPresentation(const FLinearColor Accent, const bool bSelected)
{
	if (bPresentationSet && PresentationAccent == Accent && bIsSelected == bSelected)
		return;
	bPresentationSet = true;
	PresentationAccent = Accent;
	bIsSelected = bSelected;
	const FLinearColor Base(.018f, .034f, .052f);
	auto Style = GetStyle();
	Style.Normal = FSlateRoundedBoxBrush(bSelected ? Base + Accent * .16f : Base, 8.f,
	                                     bSelected ? Accent : FLinearColor(.06f, .09f, .13f), bSelected ? 2.f : 1.f);
	Style.Hovered = FSlateRoundedBoxBrush(Base + Accent * (bSelected ? .22f : .08f), 8.f,
	                                      Accent * .85f + FLinearColor(.08f, .08f, .08f, 0), 2.f);
	Style.Pressed = FSlateRoundedBoxBrush(Base + Accent * .28f, 8.f, Accent, 2.f);
	Style.Disabled =
	    FSlateRoundedBoxBrush(FLinearColor(.016f, .024f, .032f), 8.f, FLinearColor(.03f, .045f, .06f), 1.f);
	// Match padding in every state so hover/press never move the icon or label.
	Style.NormalPadding = FMargin(10, 7);
	Style.PressedPadding = FMargin(10, 7);
	SetBackgroundColor(FLinearColor::White);
	SetStyle(Style);
}

void UCityDesktopActionButton::Configure(ACityVRToolController* InSession, const int32 InAction)
{
	if (Session == InSession && Action == InAction)
		return;
	Session = InSession;
	Action = InAction;
	OnClicked.AddUniqueDynamic(this, &UCityDesktopActionButton::Activate);
	OnPressed.AddUniqueDynamic(this, &UCityDesktopActionButton::RememberConditionTarget);
}
void UCityDesktopActionButton::RememberConditionTarget()
{
	PressedCondition = 0;
	PressedConditionSession = MAX_uint64;
	if (auto* Hud = GetTypedOuter<UCityDesktopHud>(); Hud && Action <= -100 && Action > -116)
		if (Hud->ConditionTargets.IsValidIndex(-100 - Action))
		{
			PressedCondition = Hud->ConditionTargets[-100 - Action];
			PressedConditionSession = Hud->ConditionSession;
		}
}
void UCityDesktopActionButton::Activate()
{
	if (auto* Hud = GetTypedOuter<UCityDesktopHud>())
	{
		if (Action == -60 || (Action <= -100 && Action > -116))
		{
			if (PressedCondition && (PressedConditionSession != Hud->ConditionSession ||
			                         !Hud->ConditionTargets.IsValidIndex(-100 - Action) ||
			                         Hud->ConditionTargets[-100 - Action] != PressedCondition))
			{
				PressedCondition = 0;
				return;
			}
			PressedCondition = 0;
			Hud->SelectCondition(Action == -60 ? INDEX_NONE : -100 - Action);
			return;
		}
		if (Action <= -40 && Action >= -45)
		{
			Hud->ToggleInspectionSection(-40 - Action);
			return;
		}
		if (Action == -31)
		{
			Hud->ToggleInspectionDetails();
			return;
		}
		if (Action == -32)
		{
			Hud->ToggleCityDetails();
			return;
		}
	}
	if (Session.IsValid())
		Session->ExecuteDesktopUiAction(Action);
}
void UCityDesktopHud::SetSession(ACityVRToolController* InSession)
{
	Session = InSession;
	Present();
}
void UCityDesktopHud::NativeDestruct()
{
	if (WidgetTree)
		if (auto* Preview = Cast<UCityDesktopModelPreview>(WidgetTree->FindWidget(TEXT("BuildingPreview"))))
			Preview->ClearPreview();
	Session.Reset();
	Super::NativeDestruct();
}
bool UCityDesktopHud::IsPointerOverUi() const
{
	if (!WidgetTree || !FSlateApplication::IsInitialized())
		return false;
	const FVector2D Mouse = FSlateApplication::Get().GetCursorPos();
	for (int32 Index = -1; Index < 16; ++Index)
	{
		const FName Name =
		    Index < 0 ? FName(TEXT("ConditionNext")) : FName(FString::Printf(TEXT("Condition%d"), Index));
		const auto* Button = Cast<UButton>(WidgetTree->FindWidget(Name));
		if (Button && Button->GetVisibility() == ESlateVisibility::Visible &&
		    Button->GetCachedGeometry().IsUnderLocation(Mouse))
			return true;
	}
	for (const TCHAR* Name : {TEXT("TopPanel"), TEXT("ToolbarPanel"), TEXT("ToolContextPanel"), TEXT("MenuPanel"),
	                          TEXT("TerrainPanel"), TEXT("InfoPanel"), TEXT("InspectorPanel"), TEXT("ObjectivesPanel")})
	{
		if (const UWidget* Panel = WidgetTree->FindWidget(FName(Name));
		    Panel && Panel->IsVisible() && Panel->GetCachedGeometry().IsUnderLocation(Mouse))
			return true;
	}
	return false;
}
void UCityDesktopHud::NativeTick(const FGeometry& Geometry, const float DeltaSeconds)
{
	Super::NativeTick(Geometry, DeltaSeconds);
	Present();
	PresentConditions();
}
void UCityDesktopHud::Present()
{
	if (!Session.IsValid() || !WidgetTree)
		return;
	FScopedDurationTimer PresentationTimer(PresentationSeconds);
	++PresentationCount;
	FCityUiSnapshot Data = Session->GetUiSnapshot();
	if (const auto* Sim = Session->GetWorld()->GetSubsystem<UCitySimulationSubsystem>())
	{
		InspectionTrends.Observe(Sim->GetPlacementModel(), Sim->GetPopulationSnapshot(),
		    Sim->GetEconomySnapshot().FixedStepId, Sim->GetEconomySnapshot().MonthId, Sim->GetCitySessionGeneration());
		InspectionTrends.Present(Session->InspectedEntityId, Data);
	}
	auto Text = [this](const TCHAR* Name, const FText& Value)
	{
		if (auto* Label = Cast<UTextBlock>(WidgetTree->FindWidget(FName(Name))))
			if (!Label->GetText().EqualTo(Value))
				Label->SetText(Value);
	};
	auto SetVisible = [this](const TCHAR* Name, const bool bVisible)
	{
		if (auto* Widget = WidgetTree->FindWidget(FName(Name)))
			Widget->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	};
	auto Button = [this, &Text, &Data](const FString& Name, const int32 Action, const FText& Label, const bool bEnabled,
	                                   const FText& Help, const bool bSelected)
	{
		if (auto* Widget = Cast<UCityDesktopActionButton>(WidgetTree->FindWidget(FName(Name))))
		{
			Widget->Configure(Session.Get(), Action);
			const FName Symbol = UCityDesktopIcon::ForAction(Action);
			const FLinearColor Accent = Action == -20 || Action == -3  ? FLinearColor(1.f, .7f, .22f)
			                            : Action == -4 || Action == -8 ? FLinearColor(.24f, .82f, .4f)
			                                                           : UCityDesktopIcon::ColorForSymbol(Symbol);
			Widget->SetPresentation(Accent, bSelected);
			Widget->SetIsEnabled(bEnabled);
			if (!Widget->GetToolTipText().EqualTo(Help))
				Widget->SetToolTipText(Help);
			FString ShortLabel = Name.StartsWith(TEXT("InspectionSection")) ? Label.ToString() : Label.ToString().ToLower();
			if (!ShortLabel.IsEmpty())
				ShortLabel[0] = FChar::ToUpper(ShortLabel[0]);
			if (Name.StartsWith(TEXT("Toolbar")))
				ShortLabel = ShortLabel.Replace(TEXT(" [z]"), TEXT(""));
			Text(*(Name + TEXT("Label")), FText::FromString(ShortLabel));
			if (auto* Icon = Cast<UCityDesktopIcon>(WidgetTree->FindWidget(FName(Name + TEXT("Icon")))))
			{
				const bool bGraphic = !Symbol.IsNone();
				Icon->SetIcon(Symbol, bSelected ? Accent * .8f + FLinearColor(.2f, .2f, .2f, 0) : Accent);
				Icon->SetVisibility(bGraphic ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
			}
		}
	};
	FString Calendar, IgnoredWeather;
	Data.Calendar.ToString().Split(TEXT("\n"), &Calendar, &IgnoredWeather);
	Text(TEXT("Calendar"), FText::FromString(Calendar.IsEmpty() ? Data.Calendar.ToString() : Calendar));
	Text(TEXT("Funds"), Data.Funds);
	Text(TEXT("Population"), Data.Population);
	Text(TEXT("Weather"), Data.Weather);
	Text(TEXT("Totals"), Data.Totals);
	Text(TEXT("Trends"), Data.Trends);
	Text(TEXT("Budget"), Data.Budget);
	Text(TEXT("LastNet"), Data.LastNet);
	Text(TEXT("Tool"), Data.Tool);
	Text(TEXT("Construction"), Data.Construction);
	if (auto* Label = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Construction"))))
		Label->SetColorAndOpacity(Data.bUnaffordable ? FLinearColor(1, .3, .2)
		                          : Data.Construction.ToString().StartsWith(TEXT("HOME SEARCH"))
		                              ? FLinearColor(1, .8, .25)
		                              : FLinearColor::White);
	Text(TEXT("MenuTitle"), Data.MenuTitle);
	Text(TEXT("MenuBody"), Data.MenuBody);
	SetVisible(TEXT("Newspaper"), Data.bNewspaper);
	SetVisible(TEXT("NewsNavigation"), Data.bNewspaper);
	SetVisible(TEXT("MenuButtons"), !Data.bNewspaper);
	if (auto* Paper=Cast<UCityDesktopNewspaper>(WidgetTree->FindWidget(TEXT("Newspaper"))))
		if (Data.bNewspaper && Paper->SetEdition(Data.Newspaper))
			if (auto* Content=Paper->GetParent())
				if (auto* Scroll=Cast<UScrollBox>(Content->GetParent())) Scroll->ScrollToStart();
	if (auto* Panel=Cast<UBorder>(WidgetTree->FindWidget(TEXT("MenuPanel"))))
	{
		if (!OriginalMenuColor) OriginalMenuColor=Panel->GetBrushColor();
		Panel->SetBrushColor(Data.bNewspaper ? FLinearColor(.93f,.90f,.82f) : OriginalMenuColor.GetValue());
	}
	if (auto* Title=Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("MenuTitle"))))
		Title->SetColorAndOpacity(Data.bNewspaper ? FLinearColor(.12f,.10f,.08f) : FLinearColor(.9f,.94f,1));
	const auto* TerrainParameters = Session->GetDesktopTerrainPreview();
	SetVisible(TEXT("TerrainPanel"), TerrainParameters != nullptr);
	if (auto* Preview = Cast<UCityDesktopTerrainPreview>(WidgetTree->FindWidget(TEXT("TerrainPreview"))))
		Preview->Present(TerrainParameters);
	SetVisible(TEXT("TaxRows"), Data.bTaxMenu);
	const ECityPaletteAction Decrease[] = {ECityPaletteAction::TaxResidentialDown,
	                                       ECityPaletteAction::TaxCommercialDown,
	                                       ECityPaletteAction::TaxIndustrialDown};
	const ECityPaletteAction Increase[] = {ECityPaletteAction::TaxResidentialUp, ECityPaletteAction::TaxCommercialUp,
	                                       ECityPaletteAction::TaxIndustrialUp};
	for (int32 Index = 0; Data.bTaxMenu && Index < Data.TaxRates.Num(); ++Index)
	{
		const auto& Rate = Data.TaxRates[Index];
		Text(*FString::Printf(TEXT("TaxRate%d"), Index),
		     FText::FromString(Rate.Label.ToString() + TEXT("   ") + Rate.Value.ToString()));
		Button(FString::Printf(TEXT("TaxDown%d"), Index), static_cast<int32>(Decrease[Index]),
		       FText::FromString(TEXT("- 1%")), !Session->IsPaletteActionDisabled(Decrease[Index]),
		       FText::FromString(TEXT("Decrease ") + Rate.Label.ToString() + TEXT(" tax by 1%")), false);
		Button(FString::Printf(TEXT("TaxUp%d"), Index), static_cast<int32>(Increase[Index]),
		       FText::FromString(TEXT("+ 1%")), !Session->IsPaletteActionDisabled(Increase[Index]),
		       FText::FromString(TEXT("Increase ") + Rate.Label.ToString() + TEXT(" tax by 1%")), false);
	}
	if (auto* Chart = Cast<UCityDesktopChart>(WidgetTree->FindWidget(TEXT("ReportChart"))))
	{
		Chart->SetReport(Data.ReportChart);
		SetVisible(TEXT("ReportChart"), Data.bMenuOpen && !Data.ReportChart.Bars.IsEmpty());
	}
	auto Gauges = [this, &SetVisible](const TCHAR* Name, const TArray<FCityUiGauge>& Rows, const TCHAR* Title,
	                                  const TCHAR* Legend)
	{
		if (auto* Chart = Cast<UCityDesktopChart>(WidgetTree->FindWidget(Name)))
		{
			Chart->SetGauges(Rows, Title, Legend);
			SetVisible(Name, !Rows.IsEmpty());
		}
	};
	Gauges(TEXT("DemandChart"), Data.DemandGauges, TEXT("Demand"), TEXT("Center = balanced; right = needed"));
	TArray<FCityUiGauge> Utilities;
	for (int32 Index = 2; Index <= 3 && Index < Data.CapacityGauges.Num(); ++Index)
		Utilities.Add(Data.CapacityGauges[Index]);
	Gauges(TEXT("CapacityChart"), bCityDetails ? Data.CapacityGauges : Utilities, TEXT("Capacity / use"),
	       TEXT("Operational supply"));
	Gauges(TEXT("DensityChart"), Data.DensityGauges, TEXT("DENSITY PREFERENCES"),
	       TEXT("Left: spare | right: gap (places/jobs)"));
	Text(TEXT("ObjectiveTitle"), Data.ObjectiveTitle);
	Text(TEXT("Objectives"), Data.bObjectivesOpen ? Data.ObjectiveProgress : FText::GetEmpty());
	if (auto* Objective = WidgetTree->FindWidget(TEXT("ObjectivesPanel")))
		Objective->SetToolTipText(Data.Objectives);
	Text(TEXT("Inspection"), Data.Inspection);
	Text(TEXT("InspectionTitle"), Data.InspectionTitle);
	Text(TEXT("InspectionSummary"), Data.InspectionSummary);
	Gauges(TEXT("InspectionChart"), Data.InspectionGauges, TEXT(""), TEXT(""));
	Text(TEXT("Growth"), Data.bCityInfoOpen ? Data.Growth : FText::GetEmpty());
	SetVisible(TEXT("MenuPanel"), Data.bMenuOpen);
	// Temporarily cover objectives with the left menu, retaining the toggle preference.
	SetVisible(TEXT("ObjectivesPanel"), !Data.bEntryScene && !Data.bMenuOpen && Data.bObjectivesOpen);
	const bool bInspection = !Data.bEntryScene && !Data.bMenuOpen && !Data.Inspection.IsEmpty();
	SetVisible(TEXT("InfoPanel"), !Data.bEntryScene && Data.bCityInfoOpen);
	SetVisible(TEXT("InspectorPanel"), bInspection);
	// Selection owns accordion state even when a model preview is unavailable.
	if (!bInspection || LastPreviewEntity != Session->InspectedEntityId)
		OpenInspectionSections = 0;
	LastPreviewEntity = bInspection ? Session->InspectedEntityId : 0;
	if (auto* Preview = Cast<UCityDesktopModelPreview>(WidgetTree->FindWidget(TEXT("BuildingPreview"))))
	{
		FCityDesktopModelSource Source;
		const bool bSource = bInspection && Session->GetDesktopModelSource(Source);
		Preview->Present(bSource ? &Source : nullptr);
		SetVisible(TEXT("PreviewSize"), bSource);
	}
	SetVisible(TEXT("ToolbarPanel"), !Data.bEntryScene);
	SetVisible(TEXT("ToolContextPanel"), !Data.bEntryScene && !Data.bNewspaper);
	SetVisible(TEXT("TopPanel"), !Data.bEntryScene);
	// Keep the summary compact; full financial details remain in City Info and
	// on the net-trend tooltip, using the same shared snapshot.
	SetVisible(TEXT("Budget"), false);
	SetVisible(TEXT("LastNet"), false);
	SetVisible(TEXT("Totals"), false);
	SetVisible(TEXT("Trends"), false);
	SetVisible(TEXT("Weather"), false);
	if (auto* Funds = WidgetTree->FindWidget(TEXT("Funds")))
		Funds->SetToolTipText(FText::FromString(Data.Totals.ToString() + TEXT("\n") + Data.Trends.ToString() +
		                                        TEXT("\n") + Data.Budget.ToString() + TEXT("\n") +
		                                        Data.LastNet.ToString()));
	if (auto* CalendarLabel = WidgetTree->FindWidget(TEXT("Calendar")))
		CalendarLabel->SetToolTipText(Data.Weather);
	if (auto* Trends = WidgetTree->FindWidget(TEXT("Trends")))
	{
		const FText Help = FText::FromString(Data.Budget.ToString() + TEXT("\n") + Data.LastNet.ToString());
		if (!Trends->GetToolTipText().EqualTo(Help))
			Trends->SetToolTipText(Help);
	}
	SetVisible(TEXT("Rotate"), Data.bPlacementTool && !Data.bRoadTool && !Data.bMenuOpen);
	SetVisible(TEXT("RoadDown"), Data.bRoadTool && !Data.bMenuOpen);
	SetVisible(TEXT("RoadUp"), Data.bRoadTool && !Data.bMenuOpen);
	SetVisible(TEXT("Cancel"), !Data.bEntryScene && Data.bCanCancelTool);
	SetVisible(TEXT("Construction"), !Data.Construction.IsEmpty());
	SetVisible(TEXT("Inspection"), false);
	SetVisible(TEXT("InspectionChart"), false);
	SetVisible(TEXT("InspectionDetails"), false);
	for (int32 I = 0; I < 3; ++I)
	{
		const FString Name = FString::Printf(TEXT("Overview%d"), I);
		const bool bValue = Data.InspectionOverview.IsValidIndex(I) && !Data.InspectionOverview[I].IsEmpty();
		SetVisible(*(Name + TEXT("Row")), bInspection && bValue);
		if (bValue)
			Text(*Name, Data.InspectionOverview[I]);
		if (auto* Row = WidgetTree->FindWidget(FName(Name + TEXT("Row"))))
			Row->SetToolTipText(Data.InspectionOverviewHelp.IsValidIndex(I) ? Data.InspectionOverviewHelp[I] : FText::GetEmpty());
		if (auto* Icon = Cast<UCityDesktopIcon>(WidgetTree->FindWidget(FName(Name + TEXT("Icon")))))
		{
			const FName Symbol = Data.InspectionOverviewIcons.IsValidIndex(I) ? Data.InspectionOverviewIcons[I] : TEXT("TrendUnknown");
			Icon->SetIcon(Symbol, Symbol == TEXT("TrendUp") ? FLinearColor(.4f, .85f, .65f)
			    : Symbol == TEXT("TrendDown") || Symbol == TEXT("Leaving") ? FLinearColor(1.f, .55f, .35f)
			    : FLinearColor(.5f, .75f, .95f));
		}
	}
	for (int32 I = 0; I < 6; ++I)
	{
		const FString Name = FString::Printf(TEXT("InspectionSection%d"), I);
		const bool bContent = Data.InspectionSections.IsValidIndex(I) &&
		    (!Data.InspectionSections[I].Detail.IsEmpty() || Data.InspectionSections[I].Gauges.Num() || Data.InspectionSections[I].Rows.Num());
		const bool bOpen = bContent && IsInspectionSectionOpen(I);
		SetVisible(*Name, bInspection && bContent);
		SetVisible(*(Name + TEXT("Body")), bInspection && bOpen);
		if (bContent)
		{
			const auto& Section = Data.InspectionSections[I];
			Button(Name, -40 - I, FText::FromString((bOpen ? TEXT("-  ") : TEXT("+  ")) + Section.Title.ToString()),
			       true, Section.Help, bOpen);
			Text(*(Name + TEXT("Detail")), Section.Detail);
			SetVisible(*(Name + TEXT("Detail")), bOpen && !Section.Detail.IsEmpty());
			Gauges(*(Name + TEXT("Chart")), Section.Gauges, TEXT(""), TEXT(""));
			SetVisible(*(Name + TEXT("Chart")), bOpen && Section.Gauges.Num() > 0);
			if (auto* Table = Cast<UCityDesktopChart>(WidgetTree->FindWidget(FName(Name + TEXT("Table")))))
			{
				Table->SetTable(Section.Rows);
				Table->SetToolTipText(Section.Help);
			}
			SetVisible(*(Name + TEXT("Table")), bOpen && Section.Rows.Num() > 0);
		}
	}
	SetVisible(TEXT("Growth"), Data.bCityInfoOpen && bCityDetails);
	SetVisible(TEXT("DensityChart"), Data.bCityInfoOpen && bCityDetails);
	Text(TEXT("InfoTitle"), FText::FromString(TEXT("City")));
	SetVisible(TEXT("MenuBody"), !Data.bZoneMenu && !Data.bNewspaper);
	// Bound panel widths/heights in Slate units, keeping a clear map area at
	// both laptop and larger window sizes. Long content scrolls within its panel.
	const FVector2D View = GetCachedGeometry().GetLocalSize();
	if (View.X > 0 && View.Y > 0)
	{
		// Message length and selected/hovered labels never determine tool-button geometry.
		constexpr float ToolbarHeight = 82.f;
		const auto* ContextText = WidgetTree->FindWidget(TEXT("Construction"));
		const float ContextHeight = !Data.bMenuOpen && !Data.Construction.IsEmpty() && ContextText
		        ? FMath::Max(72.f, 64.f + static_cast<float>(ContextText->GetDesiredSize().Y))
		                                : 72.f;
		const float FooterHeight = ToolbarHeight + (Data.bNewspaper ? 0.f : ContextHeight) + 24.f;
		constexpr float PanelTop = 96.f;
		const float AvailableHeight = FMath::Max(100.f, static_cast<float>(View.Y) - PanelTop - FooterHeight - 12.f);
		auto Bounds = [this](const TCHAR* Name, const FMargin& Offsets)
		{
			if (auto* Widget = WidgetTree->FindWidget(FName(Name)))
				if (auto* Slot = Cast<UCanvasPanelSlot>(Widget->Slot); Slot && Slot->GetOffsets() != Offsets)
					Slot->SetOffsets(Offsets);
		};
		const float DockWidth = FMath::Min(860.f, static_cast<float>(View.X) - 40.f);
		const float MenuWidth =
		    Data.bNewspaper ? FMath::Min(1040.f,static_cast<float>(View.X)-64.f)
		    : TerrainParameters || Data.bTaxMenu ? FMath::Min(460.f, static_cast<float>(View.X) * .52f)
		    : Data.bZoneMenu                   ? FMath::Min(550.f, DockWidth)
		                     : FMath::Min(Data.bDetailedMenu ? 720.f : 460.f, static_cast<float>(View.X) * .52f);
		if (auto* Choices = Cast<UWrapBox>(WidgetTree->FindWidget(TEXT("MenuButtons"))))
			Choices->SetWrapSize(MenuWidth - 48.f);
		if (auto* Choices=Cast<UWrapBox>(WidgetTree->FindWidget(TEXT("NewsNavigation")))) Choices->SetWrapSize(MenuWidth-48.f);
		if (auto* Paper=Cast<UCityDesktopNewspaper>(WidgetTree->FindWidget(TEXT("Newspaper")))) Paper->SetPaperWidth(MenuWidth-56.f);
		float MenuHeight =
		    Data.bDetailedMenu
		        ? AvailableHeight
		        : FMath::Min(AvailableHeight, 140.f + FMath::CeilToFloat(Data.MenuEntries.Num() / 2.f) * 48.f);
		float ContentHeight = 44.f + (Data.bTaxMenu ? 24.f : 0.f);
		for (const TCHAR* Name :
		     {TEXT("MenuHeader"),TEXT("NewsNavigation"),TEXT("Newspaper"), TEXT("MenuBody"), TEXT("ReportChart"), TEXT("TaxRows"), TEXT("MenuButtons")})
			if (const auto* Widget = WidgetTree->FindWidget(FName(Name)); Widget && Widget->IsVisible())
				ContentHeight += Widget->GetDesiredSize().Y;
		if (ContentHeight > 64.f)
			MenuHeight = FMath::Clamp(ContentHeight, 120.f, AvailableHeight);
		Bounds(TEXT("ToolbarPanel"), FMargin(-DockWidth * .5f, -ToolbarHeight - 16, DockWidth, ToolbarHeight));
		Bounds(TEXT("ToolContextPanel"), FMargin(-DockWidth * .5f, -FooterHeight, DockWidth, ContextHeight));
		const float ToolWidth = (DockWidth - 72.f) / FCityPaletteModel::BarEntryCount;
		for (int32 Index = 0; Index < FCityPaletteModel::BarEntryCount; ++Index)
			if (auto* Box =
			        Cast<USizeBox>(WidgetTree->FindWidget(FName(FString::Printf(TEXT("ToolbarSize%d"), Index)))))
				Box->SetWidthOverride(FMath::Max(68.f, ToolWidth));
		Bounds(TEXT("TopPanel"), FMargin(20, 16, 20, 64));
		const float TerrainWidth = FMath::Min(340.f, static_cast<float>(View.X) - MenuWidth - 56.f);
		const float EntryLeft = (View.X - MenuWidth - (TerrainParameters ? TerrainWidth + 16.f : 0.f)) * .5f;
		const float EntryTop = FMath::Max(PanelTop, (static_cast<float>(View.Y) - MenuHeight) * .5f);
		Bounds(TEXT("TerrainPanel"), FMargin(EntryLeft + MenuWidth + 16.f, EntryTop, TerrainWidth,
		                                     FMath::Min(AvailableHeight, TerrainWidth + 148.f)));
		Bounds(TEXT("MenuPanel"), FMargin(Data.bEntryScene ? EntryLeft
		                                  : Data.bZoneMenu || Data.bNewspaper ? (View.X - MenuWidth) * .5f
		                                                   : 20.f,
		                                  Data.bZoneMenu     ? View.Y - FooterHeight - 144.f
		                                  : Data.bEntryScene ? EntryTop
		                                                     : PanelTop,
		                                  MenuWidth, Data.bZoneMenu ? 136.f : MenuHeight));
		Bounds(TEXT("ObjectivesPanel"), FMargin(20, PanelTop, FMath::Min(320.f, static_cast<float>(View.X) * .29f),
		                                        FMath::Min(168.f, AvailableHeight)));
		const float InfoWidth = FMath::Min(360.f, static_cast<float>(View.X) * .32f);
		float InspectorContentHeight = 90.f;
		if (const auto* Detail = WidgetTree->FindWidget(TEXT("Inspection")))
			if (const auto* Content = Detail->GetParent())
				InspectorContentHeight += Content->GetDesiredSize().Y;
		const float InspectorHeight = FMath::Min(
		    InspectorContentHeight, bInspection && Data.bCityInfoOpen ? AvailableHeight * .64f - 6.f : AvailableHeight);
		Bounds(TEXT("InspectorPanel"), FMargin(-InfoWidth - 20, PanelTop, InfoWidth, InspectorHeight));
		Bounds(TEXT("InfoPanel"),
		       FMargin(-InfoWidth - 20, bInspection ? PanelTop + InspectorHeight + 12.f : PanelTop, InfoWidth,
		               bInspection    ? AvailableHeight - InspectorHeight - 12.f
		               : bCityDetails ? AvailableHeight
		                              : FMath::Min(410.f, AvailableHeight)));
		if (auto* PreviewSize = Cast<USizeBox>(WidgetTree->FindWidget(TEXT("PreviewSize"))))
			PreviewSize->SetHeightOverride(120.f);
	}
	SetVisible(TEXT("History"), Data.bObjectivesOpen);
	SetVisible(TEXT("NextObjective"), Data.bObjectivesOpen);
	for (int32 Index = 0; Index < 19; ++Index)
	{
		const FString Name = FString::Printf(TEXT("Menu%d"), Index);
		const bool bAvailable = Data.MenuEntries.IsValidIndex(Index);
		SetVisible(*Name, bAvailable);
		if (bAvailable)
		{
			const auto& Entry = Data.MenuEntries[Index];
			Button(Name, Entry.ActionId, Entry.Label, Entry.bEnabled, Entry.Help, Entry.bSelected);
		}
	}
	for (int32 Index = 0; Index < Data.ToolbarEntries.Num(); ++Index)
	{
		const auto& Entry = Data.ToolbarEntries[Index];
		Button(FString::Printf(TEXT("Toolbar%d"), Index), Entry.ActionId, Entry.Label, Entry.bEnabled, Entry.Help,
		       Entry.bSelected);
	}
	for (int32 Index=0; Index<8; ++Index)
	{
		const FString Name=FString::Printf(TEXT("NewsAction%d"),Index);
		const bool Available=Data.bNewspaper && Data.MenuEntries.IsValidIndex(Index);
		SetVisible(*Name,Available);
		if (!Available) continue;
		const auto& Entry=Data.MenuEntries[Index];
		Button(Name,Entry.ActionId,Entry.Label,Entry.bEnabled,Entry.Help,Entry.bSelected);
		if (auto* Control=Cast<UCityDesktopActionButton>(WidgetTree->FindWidget(FName(Name))))
		{
			auto Style=Control->GetStyle();
			Style.Normal=FSlateRoundedBoxBrush(FLinearColor(.86f,.83f,.75f),2.f,FLinearColor(.22f,.19f,.14f),1.f);
			Style.Hovered=FSlateRoundedBoxBrush(FLinearColor(.98f,.94f,.85f),2.f,FLinearColor(.22f,.19f,.14f),1.f);
			Style.Pressed=Style.Hovered;
			Style.Disabled=FSlateRoundedBoxBrush(FLinearColor(.88f,.85f,.77f),2.f);
			Control->SetStyle(Style);
		}
		if (auto* Label=Cast<UTextBlock>(WidgetTree->FindWidget(FName(Name+TEXT("Label")))))
			Label->SetColorAndOpacity(Entry.bEnabled ? FLinearColor(.12f,.10f,.08f) : FLinearColor(.48f,.44f,.37f));
	}
	for (int32 Index = 0; Index < 8; ++Index)
	{
		const FString Name = FString::Printf(TEXT("CityCard%d"), Index);
		const bool bShow = Data.bCityInfoOpen && bCityDetails && Data.CityCards.IsValidIndex(Index);
		SetVisible(*Name, bShow);
		if (bShow)
		{
			const auto& Card = Data.CityCards[Index];
			Text(*(Name + TEXT("Title")), Card.Title);
			Text(*(Name + TEXT("Labels")), Card.Labels);
			Text(*(Name + TEXT("Values")), Card.Values);
		}
	}
	struct FControl
	{
		const TCHAR* Name;
		int32 Action;
		const TCHAR* Label;
	};
	static const FControl Controls[] = {{TEXT("MenuToggle"), -1, TEXT("Menu")},
	                                    {TEXT("CityToggle"), -2, TEXT("City info")},
	                                    {TEXT("ObjectivesToggle"), -3, TEXT("OBJECTIVES")},
	                                    {TEXT("Save"), -4, TEXT("Save")},
	                                    {TEXT("Load"), -5, TEXT("Load")},
	                                    {TEXT("Rotate"), -6, TEXT("ROTATE [R]")},
	                                    {TEXT("Cancel"), -7, TEXT("CANCEL [Esc]")},
	                                    {TEXT("Upgrade"), -8, TEXT("UPGRADE")},
	                                    {TEXT("History"), -9, TEXT("HISTORY / LIVE")},
	                                    {TEXT("NextObjective"), -10, TEXT("NEXT")},
	                                    {TEXT("RoadDown"), -11, TEXT("ROAD -")},
	                                    {TEXT("RoadUp"), -12, TEXT("ROAD +")},
	                                    {TEXT("MenuClose"), -13, TEXT("CLOSE")},
	                                    {TEXT("InfoClose"), -33, TEXT("Close")},
	                                    {TEXT("ObjectivesClose"), -3, TEXT("HIDE")},
	                                    {TEXT("InspectorClose"), -30, TEXT("Close")},
	                                    {TEXT("InspectionDetails"), -31, TEXT("Details")},
	                                    {TEXT("CityDetails"), -32, TEXT("Details")},
	                                    {TEXT("Pause"), -20, TEXT("Pause")},
	                                    {TEXT("Speed1"), -21, TEXT("1x")},
	                                    {TEXT("Speed3"), -22, TEXT("3x")},
	                                    {TEXT("Speed5"), -23, TEXT("5x")}};
	for (const auto& Control : Controls)
	{
		const bool bSideToggle =
		    Control.Action == -2 ||
		    (Control.Action == -3 && FCString::Strcmp(Control.Name, TEXT("ObjectivesToggle")) == 0);
		const bool bSelected = Control.Action == -2    ? Data.bCityInfoOpen
		                       : Control.Action == -3  ? Data.bObjectivesOpen
		                       : Control.Action == -20 ? Data.bPaused
		                       : Control.Action == -21 ? !Data.bPaused && Data.SpeedMultiplier == 1
		                       : Control.Action == -22 ? !Data.bPaused && Data.SpeedMultiplier == 3
		                       : Control.Action == -23 ? !Data.bPaused && Data.SpeedMultiplier == 5
		                                               : false;
		const FString Label = Control.Action == -31   ? (OpenInspectionSections ? TEXT("Less detail") : TEXT("Details"))
		                      : Control.Action == -32 ? (bCityDetails ? TEXT("Summary") : TEXT("Details"))
		                                              : Control.Label;
		Button(Control.Name, Control.Action, FText::FromString(Label),
		       !Data.bEntryScene || Control.Action == -1 || Control.Action == -5,
		       bSideToggle ? FText::FromString(TEXT("Show or hide this panel")) : FText::GetEmpty(), bSelected);
	}
	Button(TEXT("Upgrade"), -8, Data.Upgrade, Data.bUpgradeAvailable, FText::GetEmpty(), false);
	SetVisible(TEXT("Upgrade"), !Data.Inspection.IsEmpty() && !Data.Upgrade.IsEmpty());
	Text(TEXT("MenuCloseLabel"), FText::FromString(Data.bEntryScene ? TEXT("BACK") : TEXT("CLOSE")));
	SetVisible(TEXT("MenuClose"), !Data.bEntryScene || Data.bDetailedMenu);
}
