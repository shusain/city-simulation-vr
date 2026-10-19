#include "Interaction/CityVRToolController.h"
#include "Interaction/CityHudLayout.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	using CityHudLayout::DemandVisualBound;
	using CityHudLayout::DemandBarCenterY;
	using CityHudLayout::DemandBarHalfWidth;
	using CityHudLayout::CapacityBarTopZ;
	using CityHudLayout::CapacityBarRowSpacing;
	using CityHudLayout::DemandBarTopZ;
	using CityHudLayout::DemandBarRowSpacing;
	constexpr int32 PaletteEntryCount = FCityPaletteModel::MaximumEntryCount;
} // namespace

ACityVRToolController::ACityVRToolController()
{
	PrimaryActorTick.bCanEverTick = true;
	PaletteRoot = CreateDefaultSubobject<USceneComponent>(TEXT("PaletteRoot"));
	SetRootComponent(PaletteRoot);
	// Native hard references ensure these runtime-selected variants cook on Quest too.
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> HudText(TEXT("/Game/CitySim/UI/M_CityHudText"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> HudBar(TEXT("/Game/CitySim/UI/M_CityHudBar"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> HudIcon(TEXT("/Game/CitySim/UI/M_CityHudIcon"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> HudHeatMap(TEXT("/Game/CitySim/UI/M_CityHudHeatMap"));
	HudOverlayMasters = {HudText.Object, HudBar.Object, HudIcon.Object, HudHeatMap.Object};
	HeadsetStatusRoot = CreateDefaultSubobject<USceneComponent>(TEXT("HeadsetStatusRoot"));
	HeadsetStatusRoot->SetupAttachment(PaletteRoot);
	NeedsStatusRoot = CreateDefaultSubobject<USceneComponent>(TEXT("NeedsStatusRoot"));
	NeedsStatusRoot->SetupAttachment(HeadsetStatusRoot);
	InspectorStatusRoot = CreateDefaultSubobject<USceneComponent>(TEXT("InspectorStatusRoot"));
	InspectorStatusRoot->SetupAttachment(PaletteRoot);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> MenuSphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MenuSkyAsset(
	    TEXT("/Game/CitySim/Art/Materials/M_CitySky.M_CitySky"));
	EntrySky = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EntrySky"));
	EntrySky->SetupAttachment(PaletteRoot);
	EntrySky->SetAbsolute(true, true, true);
	EntrySky->SetStaticMesh(MenuSphere.Object);
	EntrySky->SetMaterial(0, MenuSkyAsset.Object);
	EntrySky->SetRelativeScale3D(FVector(20000));
	EntrySky->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	EntrySky->SetGenerateOverlapEvents(false);
	EntrySky->SetCanEverAffectNavigation(false);
	EntrySky->SetCastShadow(false);
	EntrySky->bVisibleInReflectionCaptures = false;
	EntrySky->bVisibleInRealTimeSkyCaptures = false;
	EntrySky->SetVisibility(false);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DownloadIcon(
	    TEXT("/Game/CitySim/UI/Icons/MI_Icon_download.MI_Icon_download"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> UploadIcon(
	    TEXT("/Game/CitySim/UI/Icons/MI_Icon_upload.MI_Icon_upload"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PlayIcon(
	    TEXT("/Game/CitySim/UI/Icons/MI_Icon_play_arrow.MI_Icon_play_arrow"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ResetIcon(
	    TEXT("/Game/CitySim/UI/Icons/MI_Icon_restart_alt.MI_Icon_restart_alt"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BackIcon(
	    TEXT("/Game/CitySim/UI/Icons/MI_Icon_arrow_back.MI_Icon_arrow_back"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SettingsIcon(
	    TEXT("/Game/CitySim/UI/Icons/MI_Icon_settings.MI_Icon_settings"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> RepairIcon(
	    TEXT("/Game/CitySim/UI/Icons/MI_Icon_build.MI_Icon_build"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> RepairingIcon(
	    TEXT("/Game/CitySim/UI/Icons/MI_Icon_construction.MI_Icon_construction"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> CrimeIcon(
	    TEXT("/Game/CitySim/UI/Icons/MI_Icon_local_police.MI_Icon_local_police"));
	ConditionIconMaterials = {RepairIcon.Object, RepairingIcon.Object, CrimeIcon.Object};
	{
		ConstructorHelpers::FObjectFinder<UMaterialInterface> Icon(
		    TEXT("/Game/CitySim/UI/Icons/MI_Icon_account_balance_wallet.MI_Icon_account_balance_wallet"));
		ConditionSymbols.Add(TEXT("Money"), Icon.Object);
	}
	{
		ConstructorHelpers::FObjectFinder<UMaterialInterface> Icon(
		    TEXT("/Game/CitySim/UI/Icons/MI_Icon_bolt.MI_Icon_bolt"));
		ConditionSymbols.Add(TEXT("Power"), Icon.Object);
	}
	{
		ConstructorHelpers::FObjectFinder<UMaterialInterface> Icon(
		    TEXT("/Game/CitySim/UI/Icons/MI_Icon_delete.MI_Icon_delete"));
		ConditionSymbols.Add(TEXT("Waste"), Icon.Object);
	}
	{
		ConstructorHelpers::FObjectFinder<UMaterialInterface> Icon(
		    TEXT("/Game/CitySim/UI/Icons/MI_Icon_groups.MI_Icon_groups"));
		ConditionSymbols.Add(TEXT("People"), Icon.Object);
	}
	{
		ConstructorHelpers::FObjectFinder<UMaterialInterface> Icon(
		    TEXT("/Game/CitySim/UI/Icons/MI_Icon_home.MI_Icon_home"));
		ConditionSymbols.Add(TEXT("Home"), Icon.Object);
	}
	{
		ConstructorHelpers::FObjectFinder<UMaterialInterface> Icon(
		    TEXT("/Game/CitySim/UI/Icons/MI_Icon_local_fire_department.MI_Icon_local_fire_department"));
		ConditionSymbols.Add(TEXT("Fire"), Icon.Object);
	}
	{
		ConstructorHelpers::FObjectFinder<UMaterialInterface> Icon(
		    TEXT("/Game/CitySim/UI/Icons/MI_Icon_medical_services.MI_Icon_medical_services"));
		ConditionSymbols.Add(TEXT("Health"), Icon.Object);
	}
	{
		ConstructorHelpers::FObjectFinder<UMaterialInterface> Icon(
		    TEXT("/Game/CitySim/UI/Icons/MI_Icon_package_2.MI_Icon_package_2"));
		ConditionSymbols.Add(TEXT("Leaving"), Icon.Object);
	}
	{
		ConstructorHelpers::FObjectFinder<UMaterialInterface> Icon(
		    TEXT("/Game/CitySim/UI/Icons/MI_Icon_pause_circle.MI_Icon_pause_circle"));
		ConditionSymbols.Add(TEXT("Inspect"), Icon.Object);
	}
	{
		ConstructorHelpers::FObjectFinder<UMaterialInterface> Icon(
		    TEXT("/Game/CitySim/UI/Icons/MI_Icon_school.MI_Icon_school"));
		ConditionSymbols.Add(TEXT("School"), Icon.Object);
	}
	{
		ConstructorHelpers::FObjectFinder<UMaterialInterface> Icon(
		    TEXT("/Game/CitySim/UI/Icons/MI_Icon_sentiment_dissatisfied.MI_Icon_sentiment_dissatisfied"));
		ConditionSymbols.Add(TEXT("Happiness"), Icon.Object);
	}
	{
		ConstructorHelpers::FObjectFinder<UMaterialInterface> Icon(
		    TEXT("/Game/CitySim/UI/Icons/MI_Icon_shopping_basket.MI_Icon_shopping_basket"));
		ConditionSymbols.Add(TEXT("Shop"), Icon.Object);
	}
	{
		ConstructorHelpers::FObjectFinder<UMaterialInterface> Icon(
		    TEXT("/Game/CitySim/UI/Icons/MI_Icon_water_drop.MI_Icon_water_drop"));
		ConditionSymbols.Add(TEXT("Water"), Icon.Object);
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ResidentialSlider(
	    TEXT("/Game/CitySim/Art/Materials/MI_CityResidential.MI_CityResidential"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> CommercialSlider(
	    TEXT("/Game/CitySim/Art/Materials/MI_CityCommercial.MI_CityCommercial"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DemandRail(
	    TEXT("/Game/CitySim/UI/MI_CityUiBarRail.MI_CityUiBarRail"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ResidentialDemand(
	    TEXT("/Game/CitySim/UI/MI_CityUiBarResidential.MI_CityUiBarResidential"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> CommercialDemand(
	    TEXT("/Game/CitySim/UI/MI_CityUiBarCommercial.MI_CityUiBarCommercial"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> IndustrialDemand(
	    TEXT("/Game/CitySim/UI/MI_CityUiBarIndustrial.MI_CityUiBarIndustrial"));
	ResidentialSliderMaterial = ResidentialSlider.Object;
	CommercialSliderMaterial = CommercialSlider.Object;
	DemandRailMaterial = DemandRail.Object;
	ResidentialDemandMaterial = ResidentialDemand.Object;
	CommercialDemandMaterial = CommercialDemand.Object;
	IndustrialDemandMaterial = IndustrialDemand.Object;
	PaletteIconMaterials = {DownloadIcon.Object, UploadIcon.Object, PlayIcon.Object,
	                        ResetIcon.Object,    BackIcon.Object,   SettingsIcon.Object};
	PaletteBackground = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PaletteBackground"));
	PaletteBackground->SetupAttachment(PaletteRoot);
	PaletteBackground->SetStaticMesh(CubeMesh.Object);
	PaletteBackground->SetRelativeLocation(FVector(0.0f, 0.0f, -0.5f));
	PaletteBackground->SetRelativeScale3D(FVector(0.01f, 0.34f, 0.42f));
	PaletteBackground->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	for (int32 Index = 0; Index < PaletteEntryCount; ++Index)
	{
		UStaticMeshComponent* Button =
		    CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("PaletteButton%d"), Index));
		Button->SetupAttachment(PaletteRoot);
		Button->SetStaticMesh(CubeMesh.Object);
		Button->SetRelativeLocation(GetPaletteButtonLocation(Index));
		Button->SetRelativeScale3D(FVector(0.008f, 0.135f, 0.014f));
		Button->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PaletteButtons.Add(Button);

		UTextRenderComponent* Label =
		    CreateDefaultSubobject<UTextRenderComponent>(*FString::Printf(TEXT("PaletteLabel%d"), Index));
		Label->SetupAttachment(PaletteRoot);
		const FVector ButtonLocation = GetPaletteButtonLocation(Index);
		Label->SetRelativeLocation(FVector(2.0f, ButtonLocation.Y, ButtonLocation.Z));
		Label->SetHorizontalAlignment(EHTA_Center);
		Label->SetVerticalAlignment(EVRTA_TextCenter);
		Label->SetWorldSize(1.8f);
		Label->SetTextRenderColor(FColor::White);
		PaletteLabels.Add(Label);

		UStaticMeshComponent* Icon =
		    CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("PaletteIcon%d"), Index));
		Icon->SetupAttachment(PaletteRoot);
		Icon->SetStaticMesh(PlaneMesh.Object);
		Icon->SetRelativeLocation(FVector(2.1f, ButtonLocation.Y, ButtonLocation.Z));
		// The palette is viewed along its local +X face, so viewer-right and
		// viewer-up are local -Y and -Z. Map the Plane mesh's image +X/+Y axes
		// onto those directions while keeping its +Z normal palette-forward.
		Icon->SetRelativeRotation(FRotator(0.0f, -90.0f, 90.0f));
		Icon->SetRelativeScale3D(FVector(0.026f));
		Icon->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PaletteIcons.Add(Icon);

		UStaticMeshComponent* SliderFill =
		    CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("PaletteSliderFill%d"), Index));
		SliderFill->SetupAttachment(PaletteRoot);
		SliderFill->SetStaticMesh(CubeMesh.Object);
		SliderFill->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SliderFill->SetVisibility(false);
		PaletteSliderFills.Add(SliderFill);

		UStaticMeshComponent* SliderThumb =
		    CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("PaletteSliderThumb%d"), Index));
		SliderThumb->SetupAttachment(PaletteRoot);
		SliderThumb->SetStaticMesh(CubeMesh.Object);
		SliderThumb->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SliderThumb->SetVisibility(false);
		PaletteSliderThumbs.Add(SliderThumb);
	}
	PaletteTitle = CreateDefaultSubobject<UTextRenderComponent>(TEXT("PaletteTitle"));
	PaletteTitle->SetupAttachment(PaletteRoot);
	PaletteTitle->SetRelativeLocation(FVector(2.0f, -15.0f, 17.0f));
	PaletteTitle->SetHorizontalAlignment(EHTA_Left);
	PaletteTitle->SetVerticalAlignment(EVRTA_TextCenter);
	PaletteTitle->SetWorldSize(1.7f);
	PaletteTitle->SetTextRenderColor(FColor::Cyan);

	PaletteHelp = CreateDefaultSubobject<UTextRenderComponent>(TEXT("PaletteHelp"));
	PaletteHelp->SetupAttachment(PaletteRoot);
	PaletteHelp->SetRelativeLocation(FVector(2.0f, -15.0f, -17.5f));
	PaletteHelp->SetHorizontalAlignment(EHTA_Left);
	PaletteHelp->SetVerticalAlignment(EVRTA_TextTop);
	PaletteHelp->SetWorldSize(1.35f);
	PaletteHelp->SetTextRenderColor(FColor::Yellow);

	InspectorText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("InspectorText"));
	InspectorText->SetupAttachment(InspectorStatusRoot);
	InspectorText->SetRelativeLocation(FVector::ZeroVector);
	InspectorText->SetHorizontalAlignment(EHTA_Left);
	InspectorText->SetVerticalAlignment(EVRTA_TextTop);
	InspectorText->SetWorldSize(1.05f);
	InspectorText->SetHorizontalAlignment(EHTA_Center);
	InspectorText->SetTextRenderColor(FColor::White);

	SummaryText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("SummaryText"));
	SummaryText->SetupAttachment(NeedsStatusRoot);
	SummaryText->SetRelativeLocation(FVector::ZeroVector);
	SummaryText->SetHorizontalAlignment(EHTA_Left);
	SummaryText->SetVerticalAlignment(EVRTA_TextTop);
	SummaryText->SetWorldSize(1.55f);
	SummaryText->SetTextRenderColor(FColor::White);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> HeadsetTextMaterial(
	    TEXT("/Game/CitySim/UI/M_CityTextUnlit.M_CityTextUnlit"));
	if (HeadsetTextMaterial.Succeeded())
	{
		SummaryText->SetTextMaterial(HeadsetTextMaterial.Object);
		InspectorText->SetTextMaterial(HeadsetTextMaterial.Object);
		PaletteTitle->SetTextMaterial(HeadsetTextMaterial.Object);
		PaletteHelp->SetTextMaterial(HeadsetTextMaterial.Object);
		for (UTextRenderComponent* Label : PaletteLabels)
			Label->SetTextMaterial(HeadsetTextMaterial.Object);
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PanelMaterial(
	    TEXT("/Game/CitySim/UI/MI_CityUiPanel.MI_CityUiPanel"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ChartIcon(
	    TEXT("/Game/CitySim/UI/Icons/MI_Icon_bar_chart.MI_Icon_bar_chart"));
	PaletteBackground->SetMaterial(0, PanelMaterial.Object);
	PaletteTerrainPreview = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("PaletteTerrainPreview"));
	PaletteTerrainPreview->SetupAttachment(PaletteRoot);
	PaletteTerrainPreview->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PaletteTerrainPreview->SetCastShadow(false);
	PaletteTerrainPreview->SetVisibility(false);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> TerrainPreviewMaterial(
	    TEXT("/Game/CitySim/UI/M_CityHeatMap.M_CityHeatMap"));
	for (int32 Section = 0; Section < 3; ++Section)
		PaletteTerrainPreview->SetMaterial(Section, TerrainPreviewMaterial.Object);
	PaletteTerrainLegend = CreateDefaultSubobject<UTextRenderComponent>(TEXT("PaletteTerrainLegend"));
	PaletteTerrainLegend->SetupAttachment(PaletteRoot);
	PaletteTerrainLegend->SetRelativeLocation(FVector(2, 18, 3));
	PaletteTerrainLegend->SetHorizontalAlignment(EHTA_Center);
	PaletteTerrainLegend->SetVerticalAlignment(EVRTA_TextCenter);
	PaletteTerrainLegend->SetWorldSize(.8f);
	PaletteTerrainLegend->SetTextRenderColor(FColor::White);
	PaletteTerrainLegend->SetTextMaterial(HeadsetTextMaterial.Object);
	PaletteTerrainLegend->SetVisibility(false);
	PaletteTerrainEntryMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PaletteTerrainEntryMarker"));
	PaletteTerrainEntryMarker->SetupAttachment(PaletteRoot);
	PaletteTerrainEntryMarker->SetStaticMesh(CubeMesh.Object);
	PaletteTerrainEntryMarker->SetMaterial(0, DemandRailMaterial.Get());
	PaletteTerrainEntryMarker->SetRelativeScale3D(FVector(.003, .005, .005));
	PaletteTerrainEntryMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PaletteTerrainEntryMarker->SetCastShadow(false);
	PaletteTerrainEntryMarker->SetVisibility(false);
	ReportText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("ReportText"));
	ReportText->SetupAttachment(PaletteRoot);
	ReportText->SetRelativeLocation(FVector(2, 0, 48));
	ReportText->SetHorizontalAlignment(EHTA_Center);
	ReportText->SetVerticalAlignment(EVRTA_TextCenter);
	ReportText->SetWorldSize(1.65f);
	ReportText->SetTextRenderColor(FColor::White);
	ReportText->SetTextMaterial(HeadsetTextMaterial.Object);
	ReportText->SetVisibility(false);
	ReportRangeText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("ReportRangeText"));
	ReportRangeText->SetupAttachment(PaletteRoot);
	ReportRangeText->SetRelativeLocation(FVector(2, 0, 25.5));
	ReportRangeText->SetHorizontalAlignment(EHTA_Center);
	ReportRangeText->SetVerticalAlignment(EVRTA_TextCenter);
	ReportRangeText->SetWorldSize(1.25f);
	ReportRangeText->SetTextRenderColor(FColor::White);
	ReportRangeText->SetTextMaterial(HeadsetTextMaterial.Object);
	ReportRangeText->SetVisibility(false);
	for (int32 I = 0; I < 24; ++I)
	{
		auto* Bar = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("ReportBar%d"), I));
		Bar->SetupAttachment(PaletteRoot);
		Bar->SetStaticMesh(CubeMesh.Object);
		Bar->SetMaterial(0, PanelMaterial.Object);
		Bar->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Bar->SetCastShadow(false);
		Bar->SetVisibility(false);
		ReportBars.Add(Bar);
	}
	ReportZeroLine = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ReportZeroLine"));
	ReportZeroLine->SetupAttachment(PaletteRoot);
	ReportZeroLine->SetStaticMesh(CubeMesh.Object);
	ReportZeroLine->SetMaterial(0, DemandRailMaterial.Get());
	ReportZeroLine->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ReportZeroLine->SetCastShadow(false);
	ReportZeroLine->SetVisibility(false);

	PaletteBackground->SetCastShadow(false);
	for (UStaticMeshComponent* Button : PaletteButtons)
	{
		Button->SetMaterial(0, DemandRail.Object);
		Button->SetCastShadow(false);
	}
	// All panel chrome shares emissive/unlit UI materials, independent of the city lighting.
	auto MakePanel = [this](const TCHAR* Name, USceneComponent* Parent, UMaterialInterface* Material)
	{
		UStaticMeshComponent* Panel = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Panel->SetupAttachment(Parent);
		Panel->SetStaticMesh(CubeMesh.Object);
		Panel->SetMaterial(0, Material);
		Panel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Panel->SetCastShadow(false);
		return Panel;
	};
	for (int32 Index = 0; Index < FCityConditionIndicators::MaximumBadges; ++Index)
	{
		auto Name = [Index](const TCHAR* Part) { return FString::Printf(TEXT("Condition%s%d"), Part, Index); };
		auto* Root = CreateDefaultSubobject<USceneComponent>(*Name(TEXT("Root")));
		Root->SetupAttachment(PaletteRoot);
		ConditionRoots.Add(Root);
		auto* Background = MakePanel(*Name(TEXT("Background")), Root, PanelMaterial.Object);
		Background->SetRelativeScale3D(FVector(.004, .07, .06));
		ConditionBackgrounds.Add(Background);
		for (int32 Row = 0; Row < 2; ++Row)
		{
			auto* Icon =
			    CreateDefaultSubobject<UStaticMeshComponent>(*Name(Row == 0 ? TEXT("Icon") : TEXT("CrimeIcon")));
			Icon->SetupAttachment(Root);
			Icon->SetStaticMesh(PlaneMesh.Object);
			Icon->SetMaterial(0, CrimeIcon.Object);
			Icon->SetRelativeLocation(FVector(.4, 0, .65));
			Icon->SetRelativeRotation(FRotator(0, -90, 90));
			Icon->SetRelativeScale3D(FVector(.027));
			Icon->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Icon->SetCastShadow(false);
			(Row == 0 ? ConditionIcons : ConditionCrimeIcons).Add(Icon);
			auto* Text = CreateDefaultSubobject<UTextRenderComponent>(*Name(Row == 0 ? TEXT("Title") : TEXT("Detail")));
			Text->SetupAttachment(Root);
			Text->SetRelativeLocation(FVector(.4, 0, -1.5));
			Text->SetHorizontalAlignment(EHTA_Center);
			Text->SetVerticalAlignment(EVRTA_TextCenter);
			Text->SetWorldSize(.8);
			Text->SetTextMaterial(HeadsetTextMaterial.Object);
			Text->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Text->SetCastShadow(false);
			(Row == 0 ? ConditionTitles : ConditionDetails).Add(Text);
		}
		auto* Rail = MakePanel(*Name(TEXT("Rail")), Root, DemandRail.Object);
		Rail->SetRelativeLocation(FVector(.35, 0, -2.35));
		Rail->SetRelativeScale3D(FVector(.002, .055, .0012));
		ConditionRails.Add(Rail);
		auto* Fill = MakePanel(*Name(TEXT("Fill")), Root, ResidentialDemand.Object);
		ConditionFills.Add(Fill);
		Root->SetVisibility(false, true);
	}
	ConditionLeaderInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ConditionLeaderInstances"));
	ConditionLeaderInstances->SetupAttachment(PaletteRoot);
	ConditionLeaderInstances->SetStaticMesh(CubeMesh.Object);
	ConditionLeaderInstances->SetMaterial(0, DemandRail.Object);
	ConditionLeaderInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ConditionLeaderInstances->SetCastShadow(false);
	ConditionLeaderInstances->SetCanEverAffectNavigation(false);
	ConditionLeaderInstances->SetVisibility(false);
	ObjectiveRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ObjectiveRoot"));
	ObjectiveRoot->SetupAttachment(PaletteRoot);
	ObjectiveToggleRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ObjectiveToggleRoot"));
	ObjectiveToggleRoot->SetupAttachment(PaletteRoot);
	ObjectiveToggle = MakePanel(TEXT("ObjectiveToggle"), ObjectiveToggleRoot, PanelMaterial.Object);
	ObjectiveToggle->SetRelativeScale3D(FVector(.008, .06, .06));
	ObjectiveToggleIcon = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ObjectiveToggleIcon"));
	ObjectiveToggleIcon->SetupAttachment(ObjectiveToggleRoot);
	ObjectiveToggleIcon->SetRelativeLocation(FVector(1, 0, 0));
	ObjectiveToggleIcon->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ObjectiveToggleIcon->SetCastShadow(false);
	// A pooled vector checklist shares the unlit UI material and geometric MSAA.
	TArray<FVector> IconVertices;
	TArray<int32> IconTriangles;
	auto AddStroke = [&IconVertices, &IconTriangles](FVector2D Start, FVector2D End)
	{
		const FVector2D Direction = (End - Start).GetSafeNormal();
		const FVector2D Edge(-Direction.Y * .085, Direction.X * .085);
		const int32 Base = IconVertices.Num();
		for (const FVector2D& Point : {Start + Edge, End + Edge, End - Edge, Start - Edge})
			IconVertices.Add(FVector(0, Point.X, Point.Y));
		IconTriangles.Append(
		    {Base, Base + 1, Base + 2, Base, Base + 2, Base + 3, Base + 2, Base + 1, Base, Base + 3, Base + 2, Base});
	};
	for (const double Row : {1.0, 0.0, -1.0})
	{
		AddStroke(FVector2D(1.6, Row + .02), FVector2D(1.25, Row - .28));
		AddStroke(FVector2D(1.25, Row - .28), FVector2D(.85, Row + .3));
		AddStroke(FVector2D(.4, Row), FVector2D(-1.55, Row));
	}
	ObjectiveToggleIcon->CreateMeshSection(0, IconVertices, IconTriangles, {}, {}, {}, {}, false);
	ObjectiveToggleIcon->SetMaterial(0, DemandRail.Object);
	ObjectiveToggleHelp = CreateDefaultSubobject<UTextRenderComponent>(TEXT("ObjectiveToggleHelp"));
	ObjectiveToggleHelp->SetupAttachment(ObjectiveToggleRoot);
	ObjectiveToggleHelp->SetRelativeLocation(FVector(1, 0, -4));
	ObjectiveToggleHelp->SetHorizontalAlignment(EHTA_Center);
	ObjectiveToggleHelp->SetWorldSize(1.1f);
	ObjectiveToggleHelp->SetTextMaterial(HeadsetTextMaterial.Object);
	ObjectiveToggleHelp->SetCastShadow(false);
	ObjectiveToggleHelp->SetVisibility(false);
	ObjectiveBackground = MakePanel(TEXT("ObjectiveBackground"), ObjectiveRoot, PanelMaterial.Object);
	ObjectiveBackground->SetRelativeLocation(FVector(0, 0, -10.25));
	ObjectiveBackground->SetRelativeScale3D(FVector(.008, .30, .245));
	ObjectiveHeader = CreateDefaultSubobject<UTextRenderComponent>(TEXT("ObjectiveHeader"));
	ObjectiveHeader->SetupAttachment(ObjectiveRoot);
	ObjectiveHeader->SetRelativeLocation(FVector(.6, 0, 0));
	ObjectiveHeader->SetHorizontalAlignment(EHTA_Center);
	ObjectiveHeader->SetVerticalAlignment(EVRTA_TextCenter);
	ObjectiveHeader->SetWorldSize(1.35f);
	ObjectiveText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("ObjectiveText"));
	ObjectiveText->SetupAttachment(ObjectiveRoot);
	ObjectiveText->SetRelativeLocation(FVector(.6, 13.5, -3.4));
	ObjectiveText->SetHorizontalAlignment(EHTA_Left);
	ObjectiveText->SetVerticalAlignment(EVRTA_TextTop);
	ObjectiveText->SetWorldSize(1.1f);
	for (UTextRenderComponent* Label : {ObjectiveHeader.Get(), ObjectiveText.Get()})
	{
		Label->SetTextRenderColor(FColor::White);
		Label->SetCastShadow(false);
		if (HeadsetTextMaterial.Succeeded())
			Label->SetTextMaterial(HeadsetTextMaterial.Object);
	}
	ObjectiveRail = MakePanel(TEXT("ObjectiveRail"), ObjectiveRoot, DemandRail.Object);
	ObjectiveRail->SetRelativeLocation(FVector(.6, 0, -15.5));
	ObjectiveRail->SetRelativeScale3D(FVector(.006, .27, .006));
	ObjectiveFill = MakePanel(TEXT("ObjectiveFill"), ObjectiveRoot, ResidentialDemand.Object);
	for (int32 Index = 0; Index < 2; ++Index)
	{
		const FVector Position(.7, Index == 0 ? 7 : -7, -19.5);
		UStaticMeshComponent* Button =
		    MakePanel(*FString::Printf(TEXT("ObjectiveFooter%d"), Index), ObjectiveRoot, DemandRail.Object);
		Button->SetRelativeLocation(Position);
		Button->SetRelativeScale3D(FVector(.006, .13, .032));
		ObjectiveFooterButtons.Add(Button);
		UTextRenderComponent* Label =
		    CreateDefaultSubobject<UTextRenderComponent>(*FString::Printf(TEXT("ObjectiveFooterLabel%d"), Index));
		Label->SetupAttachment(ObjectiveRoot);
		Label->SetRelativeLocation(Position + FVector(.5, 0, 0));
		Label->SetHorizontalAlignment(EHTA_Center);
		Label->SetVerticalAlignment(EVRTA_TextCenter);
		Label->SetWorldSize(1.1f);
		Label->SetTextRenderColor(FColor::White);
		Label->SetCastShadow(false);
		if (HeadsetTextMaterial.Succeeded())
			Label->SetTextMaterial(HeadsetTextMaterial.Object);
		ObjectiveFooterLabels.Add(Label);
	}
	HeatMapLegendBackground = MakePanel(TEXT("HeatMapLegendBackground"), HeadsetStatusRoot, PanelMaterial.Object);
	HeatMapLegendBackground->SetRelativeLocation(FVector(-0.7, 0, 3.0));
	HeatMapLegendBackground->SetRelativeScale3D(FVector(0.006, 0.42, 0.085));
	HeatMapLegendBackground->SetVisibility(false);
	HeatMapLegend = CreateDefaultSubobject<UTextRenderComponent>(TEXT("HeatMapLegend"));
	HeatMapLegend->SetupAttachment(HeadsetStatusRoot);
	HeatMapLegend->SetRelativeLocation(FVector(0, 0, 7.0));
	HeatMapLegend->SetHorizontalAlignment(EHTA_Center);
	HeatMapLegend->SetVerticalAlignment(EVRTA_TextTop);
	HeatMapLegend->SetWorldSize(1.05f);
	HeatMapLegend->SetTextRenderColor(FColor::White);
	if (HeadsetTextMaterial.Succeeded())
		HeatMapLegend->SetTextMaterial(HeadsetTextMaterial.Object);
	HeatMapLegend->SetVisibility(false);
	InspectorBackground = MakePanel(TEXT("InspectorBackground"), InspectorStatusRoot, PanelMaterial.Object);
	InspectorUpgradeButton = MakePanel(TEXT("InspectorUpgradeButton"), InspectorStatusRoot, CommercialDemand.Object);
	InspectorUpgradeLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("InspectorUpgradeLabel"));
	InspectorUpgradeLabel->SetupAttachment(InspectorStatusRoot);
	InspectorUpgradeLabel->SetHorizontalAlignment(EHTA_Center);
	InspectorUpgradeLabel->SetVerticalAlignment(EVRTA_TextCenter);
	InspectorUpgradeLabel->SetWorldSize(1.3f);
	InspectorUpgradeLabel->SetTextRenderColor(FColor::White);
	InspectorUpgradeLabel->SetCastShadow(false);
	if (HeadsetTextMaterial.Succeeded())
		InspectorUpgradeLabel->SetTextMaterial(HeadsetTextMaterial.Object);
	InspectorLeader = MakePanel(TEXT("InspectorLeader"), PaletteRoot, DemandRail.Object);
	for (int32 Index = 0; Index < 3; ++Index)
	{
		InspectorMetricRails.Add(
		    MakePanel(*FString::Printf(TEXT("InspectorRail%d"), Index), InspectorStatusRoot, DemandRail.Object));
		InspectorMetricFills.Add(
		    MakePanel(*FString::Printf(TEXT("InspectorFill%d"), Index), InspectorStatusRoot, CommercialDemand.Object));
		UTextRenderComponent* Label =
		    CreateDefaultSubobject<UTextRenderComponent>(*FString::Printf(TEXT("InspectorMetric%d"), Index));
		Label->SetupAttachment(InspectorStatusRoot);
		Label->SetHorizontalAlignment(EHTA_Center);
		Label->SetVerticalAlignment(EVRTA_TextTop);
		Label->SetWorldSize(1.05f);
		Label->SetTextRenderColor(FColor::White);
		if (HeadsetTextMaterial.Succeeded())
			Label->SetTextMaterial(HeadsetTextMaterial.Object);
		InspectorMetricLabels.Add(Label);
	}
	for (int32 Index = 0; Index < 6; ++Index)
	{
		const float Left = -(Index % 2) * 18.5f;
		const float Top = -3.0f - (Index / 2) * 8.5f;
		auto* Background =
		    MakePanel(*FString::Printf(TEXT("CityInfoBackground%d"), Index), NeedsStatusRoot, DemandRail.Object);
		Background->SetRelativeLocation(FVector(.1f, Left - 8.3f, Top - 3.1f));
		Background->SetRelativeScale3D(FVector(.003f, .177f, .078f));
		Background->SetVisibility(false);
		CityInfoBackgrounds.Add(Background);
		for (int32 Part = 0; Part < 3; ++Part)
		{
			const TCHAR* Kind = Part == 0 ? TEXT("Title") : Part == 1 ? TEXT("Labels") : TEXT("Values");
			auto* Text =
			    CreateDefaultSubobject<UTextRenderComponent>(*FString::Printf(TEXT("CityInfo%s%d"), Kind, Index));
			Text->SetupAttachment(NeedsStatusRoot);
			Text->SetRelativeLocation(FVector(.7f, Part == 2 ? Left - 15.8f : Left, Top - (Part == 0 ? 0.f : 1.8f)));
			Text->SetHorizontalAlignment(Part == 2 ? EHTA_Right : EHTA_Left);
			Text->SetVerticalAlignment(EVRTA_TextTop);
			Text->SetWorldSize(Part == 0 ? 1.1f : 1.05f);
			Text->SetTextMaterial(HeadsetTextMaterial.Object);
			Text->SetTextRenderColor(Part == 0 ? FColor(155, 210, 255) : FColor::White);
			Text->SetVisibility(false);
			(Part == 0 ? CityInfoTitles : Part == 1 ? CityInfoLabels : CityInfoValues).Add(Text);
		}
	}
	NeedsBackground = MakePanel(TEXT("NeedsBackground"), NeedsStatusRoot, PanelMaterial.Object);
	NeedsBackground->SetRelativeLocation(FVector(-0.8, -12.0, -16.0));
	NeedsBackground->SetRelativeScale3D(FVector(0.008, 0.32, 0.39));
	NeedsToggleRoot = CreateDefaultSubobject<USceneComponent>(TEXT("NeedsToggleRoot"));
	NeedsToggleRoot->SetupAttachment(PaletteRoot);
	NeedsToggleBackground = MakePanel(TEXT("NeedsToggleBackground"), NeedsToggleRoot, PanelMaterial.Object);
	NeedsToggleBackground->SetRelativeScale3D(FVector(0.008, 0.06, 0.06));
	NeedsToggleIcon = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("NeedsToggleIcon"));
	NeedsToggleIcon->SetupAttachment(NeedsToggleRoot);
	NeedsToggleIcon->SetStaticMesh(PlaneMesh.Object);
	NeedsToggleIcon->SetMaterial(0, ChartIcon.Object);
	NeedsToggleIcon->SetRelativeLocation(FVector(1.0, 0.0, 0.0));
	NeedsToggleIcon->SetRelativeRotation(FRotator(0.0, -90.0, 90.0));
	NeedsToggleIcon->SetRelativeScale3D(FVector(0.04));
	NeedsToggleIcon->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	NeedsToggleIcon->SetCastShadow(false);
	NeedsToggleHelp = CreateDefaultSubobject<UTextRenderComponent>(TEXT("NeedsToggleHelp"));
	NeedsToggleHelp->SetupAttachment(NeedsToggleRoot);
	NeedsToggleHelp->SetRelativeLocation(FVector(1.0, 0.0, -4.0));
	NeedsToggleHelp->SetWorldSize(1.1f);
	NeedsToggleHelp->SetHorizontalAlignment(EHTA_Center);
	NeedsToggleHelp->SetTextMaterial(HeadsetTextMaterial.Object);
	NeedsText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("NeedsText"));
	NeedsText->SetupAttachment(NeedsStatusRoot);
	NeedsText->SetRelativeLocation(FVector(.7f, 0.0f, -31.0f));
	NeedsText->SetHorizontalAlignment(EHTA_Left);
	NeedsText->SetVerticalAlignment(EVRTA_TextTop);
	NeedsText->SetWorldSize(1.15f);
	NeedsText->SetTextRenderColor(FColor::White);
	if (HeadsetTextMaterial.Succeeded())
	{
		NeedsText->SetTextMaterial(HeadsetTextMaterial.Object);
	}
	GrowthNeedsBackground = MakePanel(TEXT("GrowthNeedsBackground"), NeedsStatusRoot, DemandRail.Object);
	GrowthNeedsBackground->SetRelativeLocation(FVector(.1f, -17.5f, -32.f));
	GrowthNeedsBackground->SetRelativeScale3D(FVector(.003f, .362f, .08f));
	GrowthNeedsBackground->SetVisibility(false);
	GrowthNeedsTitle = CreateDefaultSubobject<UTextRenderComponent>(TEXT("GrowthNeedsTitle"));
	GrowthNeedsTitle->SetupAttachment(NeedsStatusRoot);
	GrowthNeedsTitle->SetRelativeLocation(FVector(.7f, 0.f, -29.f));
	GrowthNeedsTitle->SetHorizontalAlignment(EHTA_Left);
	GrowthNeedsTitle->SetVerticalAlignment(EVRTA_TextTop);
	GrowthNeedsTitle->SetWorldSize(1.15f);
	GrowthNeedsTitle->SetTextRenderColor(FColor(155, 210, 255));
	GrowthNeedsTitle->SetTextMaterial(HeadsetTextMaterial.Object);
	GrowthNeedsTitle->SetText(FText::FromString(TEXT("NEEDS FOR GROWTH")));
	GrowthNeedsTitle->SetVisibility(false);
	CalendarText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("CalendarText"));
	CalendarText->SetupAttachment(HeadsetStatusRoot);
	// The totals panel front is X=0. Keep text in front, including in both eyes.
	CalendarText->SetRelativeLocation(FVector(0.7f, 0.0f, 18.0f));
	CalendarText->SetHorizontalAlignment(EHTA_Center);
	CalendarText->SetVerticalAlignment(EVRTA_TextCenter);
	CalendarText->SetWorldSize(1.45f);
	CalendarText->SetTextRenderColor(FColor::White);
	if (HeadsetTextMaterial.Succeeded())
		CalendarText->SetTextMaterial(HeadsetTextMaterial.Object);
	CityTotalsBackground = MakePanel(TEXT("CityTotalsBackground"), HeadsetStatusRoot, PanelMaterial.Object);
	CityTotalsBackground->SetRelativeLocation(FVector(0.f, 0.f, 6.f));
	CityTotalsBackground->SetRelativeScale3D(FVector(.006f, .38f, .022f));
	CityTotalsText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("CityTotalsText"));
	CityTotalsText->SetupAttachment(HeadsetStatusRoot);
	CityTotalsText->SetRelativeLocation(FVector(0.7f, 0.f, 14.8f));
	CityTotalsText->SetHorizontalAlignment(EHTA_Center);
	CityTotalsText->SetVerticalAlignment(EVRTA_TextCenter);
	CityTotalsText->SetWorldSize(1.3f);
	CityTotalsText->SetTextRenderColor(FColor::White);
	CityTotalsText->SetTextMaterial(HeadsetTextMaterial.Object);
	for (int32 Index = 0; Index < 2; ++Index)
	{
		auto* Text = CreateDefaultSubobject<UTextRenderComponent>(*FString::Printf(TEXT("CityTrendText%d"), Index));
		Text->SetupAttachment(HeadsetStatusRoot);
		Text->SetRelativeLocation(FVector(.7f, Index == 0 ? 11.f : -11.f, 12.8f));
		Text->SetHorizontalAlignment(EHTA_Center);
		Text->SetVerticalAlignment(EVRTA_TextCenter);
		Text->SetWorldSize(1.f);
		Text->SetTextMaterial(HeadsetTextMaterial.Object);
		CityTrendTexts.Add(Text);
		auto* Arrow =
		    CreateDefaultSubobject<UProceduralMeshComponent>(*FString::Printf(TEXT("CityTrendArrow%d"), Index));
		Arrow->SetupAttachment(HeadsetStatusRoot);
		Arrow->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Arrow->SetCastShadow(false);
		const TArray<FVector> Vertices = {{0, -.2, -.6}, {0, .2, -.6}, {0, .2, 0}, {0, .6, 0},
		                                  {0, 0, .7},    {0, -.6, 0},  {0, -.2, 0}};
		const TArray<int32> Triangles = {0, 1, 2, 0, 2, 6, 3, 4, 5, 2, 5, 6, 2, 3, 5,
		                                 2, 1, 0, 6, 2, 0, 5, 4, 3, 6, 5, 2, 5, 3, 2};
		Arrow->CreateMeshSection(0, Vertices, Triangles, {}, {}, {}, {}, false);
		Arrow->SetMaterial(0, DemandRailMaterial);
		CityTrendArrows.Add(Arrow);
	}

	CityBudgetDetail = CreateDefaultSubobject<UTextRenderComponent>(TEXT("CityBudgetDetail"));
	CityBudgetDetail->SetupAttachment(HeadsetStatusRoot);
	CityBudgetDetail->SetRelativeLocation(FVector(.7f, 0, 11.1f));
	CityBudgetDetail->SetHorizontalAlignment(EHTA_Center);
	CityBudgetDetail->SetVerticalAlignment(EVRTA_TextCenter);
	CityBudgetDetail->SetWorldSize(.95f);
	CityBudgetDetail->SetTextMaterial(HeadsetTextMaterial.Object);
	CityBudgetDetail->SetTextRenderColor(FColor(205, 215, 225));
	CityLastNetText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("CityLastNetText"));
	CityLastNetText->SetupAttachment(HeadsetStatusRoot);
	CityLastNetText->SetRelativeLocation(FVector(.7f, 0, 9.5f));
	CityLastNetText->SetHorizontalAlignment(EHTA_Center);
	CityLastNetText->SetVerticalAlignment(EVRTA_TextCenter);
	CityLastNetText->SetWorldSize(.95f);
	CityLastNetText->SetTextMaterial(HeadsetTextMaterial.Object);
	CityLastNetText->SetTextRenderColor(FColor(205, 215, 225));
	ConditionSummaryText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("ConditionSummary"));
	ConditionSummaryText->SetupAttachment(HeadsetStatusRoot);
	ConditionSummaryText->SetRelativeLocation(FVector(.7, 0, 5.3));
	ConditionSummaryText->SetHorizontalAlignment(EHTA_Center);
	ConditionSummaryText->SetVerticalAlignment(EVRTA_TextCenter);
	ConditionSummaryText->SetWorldSize(.8);
	ConditionSummaryText->SetTextMaterial(HeadsetTextMaterial.Object);
	ConditionSummaryText->SetTextRenderColor(FColor(255, 210, 120));
	ConditionSummaryText->SetVisibility(false);

	for (int32 Index = 0; Index < 4; ++Index)
	{
		UStaticMeshComponent* Button =
		    CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("TimeControlButton%d"), Index));
		Button->SetupAttachment(HeadsetStatusRoot);
		Button->SetStaticMesh(CubeMesh.Object);
		Button->SetMaterial(0, DemandRailMaterial);
		Button->SetRelativeLocation(FVector(0.3f, -6.75f + Index * 4.5f, 7.5f));
		Button->SetRelativeScale3D(FVector(0.006f, 0.018f, 0.012f));
		Button->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		TimeControlButtons.Add(Button);
		UTextRenderComponent* Label =
		    CreateDefaultSubobject<UTextRenderComponent>(*FString::Printf(TEXT("TimeControlLabel%d"), Index));
		Label->SetupAttachment(HeadsetStatusRoot);
		Label->SetRelativeLocation(FVector(0.7f, -6.75f + Index * 4.5f, 7.5f));
		Label->SetHorizontalAlignment(EHTA_Center);
		Label->SetVerticalAlignment(EVRTA_TextCenter);
		Label->SetWorldSize(1.1f);
		Label->SetTextRenderColor(FColor::White);
		if (HeadsetTextMaterial.Succeeded())
			Label->SetTextMaterial(HeadsetTextMaterial.Object);
		Label->SetText(FText::FromString(Index == 0   ? TEXT("PLAY")
		                                 : Index == 1 ? TEXT("1X")
		                                 : Index == 2 ? TEXT("3X")
		                                              : TEXT("5X")));
		TimeControlLabels.Add(Label);
	}

	ConstructionStatusRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ConstructionStatusRoot"));
	ConstructionStatusRoot->SetupAttachment(PaletteRoot);
	ConstructionStatusRoot->SetAbsolute(true, true, true);
	ConstructionLeader = MakePanel(TEXT("ConstructionLeader"), PaletteRoot, CommercialDemand.Object);
	ConstructionLeader->SetAbsolute(true, true, true);
	ConstructionLeader->SetVisibility(false);
	RoadStatusBackground = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RoadStatusBackground"));
	RoadStatusBackground->SetupAttachment(ConstructionStatusRoot);
	RoadStatusBackground->SetStaticMesh(CubeMesh.Object);
	RoadStatusBackground->SetMaterial(0, PanelMaterial.Object);
	RoadStatusBackground->SetRelativeLocation(FVector(.3f, 0.f, 4.f));
	RoadStatusBackground->SetRelativeScale3D(FVector(.006f, .66f, .065f));
	RoadStatusBackground->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RoadStatusBackground->SetCastShadow(false);
	RoadStatusText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("RoadStatusText"));
	RoadStatusText->SetupAttachment(ConstructionStatusRoot);
	RoadStatusText->SetRelativeLocation(FVector(.7f, 0.f, 0.f));
	RoadStatusText->SetHorizontalAlignment(EHTA_Center);
	RoadStatusText->SetVerticalAlignment(EVRTA_TextTop);
	RoadStatusText->SetWorldSize(1.6f);
	RoadStatusText->SetCastShadow(false);
	RoadStatusText->SetTextRenderColor(FColor::White);
	if (HeadsetTextMaterial.Succeeded())
		RoadStatusText->SetTextMaterial(HeadsetTextMaterial.Object);
	RoadStatusText->SetVisibility(false);
	RoadStatusBackground->SetVisibility(false);
	PriceBlockedIcon = CreateDefaultSubobject<UTextRenderComponent>(TEXT("PriceBlockedIcon"));
	PriceBlockedIcon->SetupAttachment(ConstructionStatusRoot);
	PriceBlockedIcon->SetCastShadow(false);
	PriceBlockedIcon->SetRelativeLocation(FVector(.8f, 18.f, 4.f));
	PriceBlockedIcon->SetWorldSize(3.f);
	PriceBlockedIcon->SetText(FText::FromString(TEXT("$")));
	PriceBlockedIcon->SetHorizontalAlignment(EHTA_Center);
	PriceBlockedIcon->SetVerticalAlignment(EVRTA_TextCenter);
	PriceBlockedIcon->SetTextMaterial(HeadsetTextMaterial.Object);
	PriceBlockedIcon->SetTextRenderColor(FColor(255, 100, 80));
	PriceBlockedIcon->SetVisibility(false);
	PriceBlockedSlash = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PriceBlockedSlash"));
	PriceBlockedSlash->SetupAttachment(ConstructionStatusRoot);
	PriceBlockedSlash->SetStaticMesh(CubeMesh.Object);
	PriceBlockedSlash->SetMaterial(0, IndustrialDemandMaterial);
	PriceBlockedSlash->SetRelativeLocation(FVector(1.f, 18.f, 4.f));
	PriceBlockedSlash->SetRelativeRotation(FRotator(0, 0, 45));
	PriceBlockedSlash->SetRelativeScale3D(FVector(.003f, .034f, .003f));
	PriceBlockedSlash->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PriceBlockedSlash->SetCastShadow(false);
	PriceBlockedSlash->SetVisibility(false);

	for (int32 Index = 0; Index < 5; ++Index)
	{
		const float RowZ = CapacityBarTopZ - Index * CapacityBarRowSpacing;
		UStaticMeshComponent* Rail =
		    CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("NeedsCapacityRail%d"), Index));
		Rail->SetupAttachment(NeedsStatusRoot);
		Rail->SetStaticMesh(CubeMesh.Object);
		Rail->SetMaterial(0, DemandRail.Object);
		Rail->SetRelativeLocation(FVector(0.4f, DemandBarCenterY, RowZ));
		Rail->SetRelativeScale3D(FVector(0.004f, 0.12f, 0.006f));
		Rail->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Rail->SetVisibility(false);
		NeedsCapacityRails.Add(Rail);

		UStaticMeshComponent* Fill =
		    CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("NeedsCapacityFill%d"), Index));
		Fill->SetupAttachment(NeedsStatusRoot);
		Fill->SetStaticMesh(CubeMesh.Object);
		Fill->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Fill->SetVisibility(false);
		NeedsCapacityFills.Add(Fill);

		UTextRenderComponent* Label =
		    CreateDefaultSubobject<UTextRenderComponent>(*FString::Printf(TEXT("NeedsCapacityLabel%d"), Index));
		Label->SetupAttachment(NeedsStatusRoot);
		Label->SetRelativeLocation(FVector(1.1f, -2.7f, RowZ));
		Label->SetHorizontalAlignment(EHTA_Right);
		Label->SetVerticalAlignment(EVRTA_TextCenter);
		Label->SetWorldSize(0.95f);
		Label->SetTextRenderColor(FColor::White);
		if (HeadsetTextMaterial.Succeeded())
		{
			Label->SetTextMaterial(HeadsetTextMaterial.Object);
		}
		Label->SetVisibility(false);
		NeedsCapacityLabels.Add(Label);
	}
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const float RowZ = DemandBarTopZ - Index * DemandBarRowSpacing;
		UStaticMeshComponent* Rail =
		    CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("NeedsDemandRail%d"), Index));
		Rail->SetupAttachment(NeedsStatusRoot);
		Rail->SetStaticMesh(CubeMesh.Object);
		Rail->SetMaterial(0, DemandRail.Object);
		Rail->SetRelativeLocation(FVector(0.4f, DemandBarCenterY, RowZ));
		Rail->SetRelativeScale3D(FVector(0.004f, 0.12f, 0.006f));
		Rail->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Rail->SetVisibility(false);
		NeedsDemandRails.Add(Rail);

		UStaticMeshComponent* Fill =
		    CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("NeedsDemandFill%d"), Index));
		Fill->SetupAttachment(NeedsStatusRoot);
		Fill->SetStaticMesh(CubeMesh.Object);
		Fill->SetRelativeLocation(FVector(0.8f, DemandBarCenterY, RowZ));
		Fill->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Fill->SetVisibility(false);
		NeedsDemandFills.Add(Fill);

		UStaticMeshComponent* ZeroMarker =
		    CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("NeedsDemandZero%d"), Index));
		ZeroMarker->SetupAttachment(NeedsStatusRoot);
		ZeroMarker->SetStaticMesh(CubeMesh.Object);
		ZeroMarker->SetMaterial(0, DemandRail.Object);
		ZeroMarker->SetRelativeLocation(FVector(1.0f, DemandBarCenterY, RowZ));
		ZeroMarker->SetRelativeScale3D(FVector(0.005f, 0.008f, 0.012f));
		ZeroMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ZeroMarker->SetVisibility(false);
		NeedsDemandZeroMarkers.Add(ZeroMarker);

		UTextRenderComponent* Label =
		    CreateDefaultSubobject<UTextRenderComponent>(*FString::Printf(TEXT("NeedsDemandLabel%d"), Index));
		Label->SetupAttachment(NeedsStatusRoot);
		Label->SetRelativeLocation(FVector(1.1f, -2.7f, RowZ));
		Label->SetHorizontalAlignment(EHTA_Right);
		Label->SetVerticalAlignment(EVRTA_TextCenter);
		Label->SetWorldSize(1.1f);
		Label->SetTextRenderColor(Index == 0   ? FColor(56, 184, 82)
		                          : Index == 1 ? FColor(31, 117, 235)
		                                       : FColor(242, 199, 20));
		if (HeadsetTextMaterial.Succeeded())
		{
			Label->SetTextMaterial(HeadsetTextMaterial.Object);
		}
		Label->SetVisibility(false);
		NeedsDemandLabels.Add(Label);
	}
	VisiblePaletteEntries = FCityPaletteModel::Entries(PalettePage);
	SetPaletteOpen(false);
}
