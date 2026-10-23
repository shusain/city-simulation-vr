#if WITH_DEV_AUTOMATION_TESTS

#include "Interaction/CityPaletteModel.h"
#include "Interaction/CityVRToolController.h"
#include "Presentation/CityBoardActor.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Components/TextRenderComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Presentation/CityDesktopIcon.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityDrawerLayoutTest, "CitySim.Input.DrawerLayout",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityDrawerLayoutTest::RunTest(const FString& Parameters)
{
	for (const bool bHand : {false, true})
	{
		for (const ECityPalettePage Page : {ECityPalettePage::Root,
		                                    ECityPalettePage::WasteFacilities,
		                                    ECityPalettePage::EducationFacilities,
		                                    ECityPalettePage::PowerFacilities,
		                                    ECityPalettePage::WaterFacilities,
		                                    ECityPalettePage::Build,
		                                    ECityPalettePage::Parks,
		                                    ECityPalettePage::Roads,
		                                    ECityPalettePage::Reports,
		                                    ECityPalettePage::Zone,
		                                    ECityPalettePage::City,
		                                    ECityPalettePage::Taxes,
		                                    ECityPalettePage::Settings,
		                                    ECityPalettePage::Disasters,
		                                    ECityPalettePage::DisasterHistory,
		                                    ECityPalettePage::Audio,
		                                    ECityPalettePage::SafetyTests,
		                                    ECityPalettePage::Lighting,
		                                    ECityPalettePage::Overlays,
		                                    ECityPalettePage::ArchiveReview,
		                                    ECityPalettePage::MainMenu,
		                                    ECityPalettePage::NewCitySetup,
		                                    ECityPalettePage::NewCityReview,
		                                    ECityPalettePage::LoadReview})
		{
			const auto Entries = FCityPaletteModel::SurfaceEntries(Page, bHand);
			const auto Surface = FCityPaletteModel::SurfaceRect(Page, bHand);
			TestTrue(TEXT("Every page fits the bounded component pool"),
			         Entries.Num() <= FCityPaletteModel::MaximumEntryCount);
			for (int32 Index = 0; Index < Entries.Num(); ++Index)
			{
				const auto Rect = FCityPaletteModel::ButtonRect(Page, Index, Entries[Index].Action, bHand);
				TestTrue(TEXT("Target center hits"), Rect.Contains(Rect.Center));
				TestFalse(TEXT("Outside target misses"),
				          Rect.Contains(Rect.Center + FVector(0, Rect.HalfSize.X + 0.01, 0)));
				TestTrue(FString::Printf(TEXT("Full target is inside background: page=%d hand=%d index=%d"),
				                         static_cast<int32>(Page), bHand, Index),
				         Surface.Contains(Rect.Center + FVector(0, Rect.HalfSize.X, Rect.HalfSize.Y)) &&
				             Surface.Contains(Rect.Center - FVector(0, Rect.HalfSize.X, Rect.HalfSize.Y)));
				for (int32 Other = 0; Other < Index; ++Other)
				{
					const auto Prior = FCityPaletteModel::ButtonRect(Page, Other, Entries[Other].Action, bHand);
					TestTrue(
					    FString::Printf(
					        TEXT("Independent actions have disjoint hit targets: page=%d hand=%d index=%d other=%d"),
					        static_cast<int32>(Page), bHand, Index, Other),
					    FMath::Abs(Rect.Center.Y - Prior.Center.Y) > Rect.HalfSize.X + Prior.HalfSize.X ||
					        FMath::Abs(Rect.Center.Z - Prior.Center.Z) > Rect.HalfSize.Y + Prior.HalfSize.Y);
				}
				if (!bHand && !FCityPaletteModel::IsEntryPage(Page) && Index < FCityPaletteModel::BarEntryCount)
				{
					const auto Root = FCityPaletteModel::SurfaceEntries(ECityPalettePage::Root, false);
					TestEqual(TEXT("Bar actions stay stable across pages"), Entries[Index].Action, Root[Index].Action);
					TestEqual(TEXT("Bar never shifts vertically"), Rect.Center.Z, 0.0);
				}
			}
		}
	}
	TestEqual(TEXT("Road Back goes to Build"), FCityPaletteModel::ParentPage(ECityPalettePage::Roads),
	          ECityPalettePage::Build);
	TestEqual(TEXT("Settings Back goes to root"), FCityPaletteModel::ParentPage(ECityPalettePage::Settings),
	          ECityPalettePage::Root);
	TestEqual(TEXT("Lighting Back goes to settings"), FCityPaletteModel::ParentPage(ECityPalettePage::Lighting),
	          ECityPalettePage::Settings);
	TestEqual(TEXT("Taxes Back goes to City"), FCityPaletteModel::ParentPage(ECityPalettePage::Taxes),
	          ECityPalettePage::City);
	for (const auto Hand : {ECityControllerHand::Left, ECityControllerHand::Right})
	{
		TestEqual(TEXT("Both hands consume menu misses"), FCityToolInputRouter::RouteTrigger(Hand, Hand, true, false),
		          ECityTriggerRoute::None);
		TestEqual(TEXT("Both hands select menu hits"), FCityToolInputRouter::RouteTrigger(Hand, Hand, true, true),
		          ECityTriggerRoute::SelectPalette);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityDrawerInteractionTest, "CitySim.Input.DrawerInteraction",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityDrawerInteractionTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues Values = UWorld::InitializationValues()
	                                                .AllowAudioPlayback(false)
	                                                .CreatePhysicsScene(false)
	                                                .CreateNavigation(false)
	                                                .CreateAISystem(false)
	                                                .ShouldSimulatePhysics(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("CityDrawerTest"), nullptr, true,
	                                    ERHIFeatureLevel::Num, &Values);
	ACityVRToolController* Controller = World->SpawnActor<ACityVRToolController>();
	Controller->Board = World->SpawnActor<ACityBoardActor>();
	// Supply the map-authored catalog before exercising the real board command path.
	FObjectProperty* CatalogProperty =
	    FindFProperty<FObjectProperty>(ACityBoardActor::StaticClass(), TEXT("BuildingCatalog"));
	CatalogProperty->SetObjectPropertyValue_InContainer(
	    Controller->Board,
	    LoadObject<UObject>(nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog")));
	Controller->Board->DispatchBeginPlay();
	AActor* CameraOwner = World->SpawnActor<AActor>();
	UCameraComponent* Camera = NewObject<UCameraComponent>(CameraOwner);
	CameraOwner->SetRootComponent(Camera);
	Camera->RegisterComponent();
	Controller->HeadsetCamera = Camera;
	Controller->UpdateHeadsetStatusAttachment();
	Controller->SetPaletteOpen(true);
	TestTrue(TEXT("View drawer attaches to camera"), Controller->PaletteRoot->GetAttachParent() == Camera);
	Controller->CyclePaletteAnchor();
	const FTransform StablePose = Controller->PaletteRoot->GetComponentTransform();
	Camera->SetWorldLocationAndRotation(FVector(30, 20, 10), FRotator(10, 25, 0));
	Controller->UpdatePaletteAttachment();
	TestTrue(TEXT("Stable drawer stays fixed as head moves"),
	         Controller->PaletteRoot->GetComponentTransform().Equals(StablePose));
	Controller->SetPaletteOpen(false);
	Controller->SetPaletteOpen(true);
	TestFalse(TEXT("Reopen recenters stable drawer"),
	          Controller->PaletteRoot->GetComponentTransform().Equals(StablePose));

	Controller->SetPalettePage(ECityPalettePage::Taxes);
	Controller->ResidentialTaxControl.Begin(1000);
	Controller->ResidentialTaxControl.StepBy(2);
	Controller->IndustrialTaxControl.Begin(1000);
	Controller->IndustrialTaxControl.StepBy(3);
	Controller->SetPalettePage(ECityPalettePage::Zone);
	TestEqual(TEXT("Category switch discards pending taxes"), Controller->ResidentialTaxControl.GetPendingValue(),
	          1000);
	TestFalse(TEXT("Category switch ends tax edit"), Controller->ResidentialTaxControl.IsEditing());
	TestEqual(TEXT("Category switch discards industrial policy too"),
	          Controller->IndustrialTaxControl.GetPendingValue(), 1000);
	for (const bool bHand : {false, true})
	{
		Controller->PaletteAnchorMode = bHand ? 2 : 0;
		Controller->BeginTaxEdit();
		Controller->SetPalettePage(ECityPalettePage::Taxes);
		auto FindAction = [&](ECityPaletteAction Action)
		{
			return Controller->VisiblePaletteEntries.IndexOfByPredicate([&](const auto& E)
			                                                            { return E.Action == Action; });
		};
		const int32 Track = FindAction(ECityPaletteAction::TaxIndustrialTrack);
		if (!TestTrue(TEXT("Industrial track is visible in fixed/hand menus"), Track != INDEX_NONE))
			continue;
		const auto Rect = FCityPaletteModel::ButtonRect(ECityPalettePage::Taxes, Track,
		                                                ECityPaletteAction::TaxIndustrialTrack, bHand);
		const FVector Normal = Controller->PaletteRoot->GetForwardVector();
		Controller->ExecutePaletteAction(Track);
		for (const int32 Rate : {0, 2500})
		{
			const double Y = Rect.Center.Y + (Rate == 0 ? 7.0 : -7.0);
			const FVector Point = Controller->PaletteRoot->GetComponentTransform().TransformPosition(
			    FVector(Rect.Center.X, Y, Rect.Center.Z));
			Controller->UpdateTaxDragFromRay(Point + Normal * 40.0, -Normal);
			TestEqual(TEXT("Industrial drag uses minimum-left/maximum-right"),
			          Controller->IndustrialTaxControl.GetPendingValue(), Rate);
		}
		Controller->ExecutePaletteAction(FindAction(ECityPaletteAction::TaxIndustrialDown));
		TestEqual(TEXT("Industrial minus steps by exactly1%"), Controller->IndustrialTaxControl.GetPendingValue(),
		          2400);
		TestTrue(TEXT("Industrial preview stays uncommitted"),
		         World->GetSubsystem<UCitySimulationSubsystem>()->GetEconomySnapshot().IndustrialTaxBasisPoints ==
		             1000);
		const auto LabelBounds = Controller->PaletteLabels[Track]
		                             ->CalcBounds(Controller->PaletteLabels[Track]->GetRelativeTransform())
		                             .GetBox();
		TestTrue(TEXT("Industrial label fits its column"), LabelBounds.Min.Y >= Rect.Center.Y - Rect.HalfSize.X &&
		                                                       LabelBounds.Max.Y <= Rect.Center.Y + Rect.HalfSize.X);
		Controller->ExecutePaletteAction(FindAction(ECityPaletteAction::ApplyTaxes));
		TestEqual(TEXT("Apply commits industrial rate"),
		          World->GetSubsystem<UCitySimulationSubsystem>()->GetEconomySnapshot().IndustrialTaxBasisPoints, 2400);
		Controller->BeginTaxEdit();
		Controller->SetPalettePage(ECityPalettePage::Taxes);
		Controller->IndustrialTaxControl.StepBy(-2);
		Controller->ExecutePaletteAction(FindAction(ECityPaletteAction::CancelTaxes));
		TestEqual(TEXT("Cancel keeps committed industrial rate"),
		          World->GetSubsystem<UCitySimulationSubsystem>()->GetEconomySnapshot().IndustrialTaxBasisPoints, 2400);
		World->GetSubsystem<UCitySimulationSubsystem>()->SetTaxRates(1000, 1000, 1000);
	}
	for (const bool bHand : {false, true})
	{
		Controller->PaletteAnchorMode = bHand ? 2 : 0;
		Controller->SetPalettePage(ECityPalettePage::Parks);
		for (int32 I = 0; I < Controller->VisiblePaletteEntries.Num(); ++I)
		{
			const auto& Entry = Controller->VisiblePaletteEntries[I];
			if (Entry.bIconOnly)
				continue;
			const auto Rect = FCityPaletteModel::ButtonRect(ECityPalettePage::Parks, I, Entry.Action, bHand);
			const auto Bounds =
			    Controller->PaletteLabels[I]->CalcBounds(Controller->PaletteLabels[I]->GetRelativeTransform()).GetBox();
			TestTrue(TEXT("Expanded parks labels fit fixed/hand targets"),
			         Bounds.Min.Y >= Rect.Center.Y - Rect.HalfSize.X &&
			             Bounds.Max.Y <= Rect.Center.Y + Rect.HalfSize.X);
		}
		for (const auto Action : {ECityPaletteAction::SelectBasketballPark, ECityPaletteAction::SelectSkatePark,
		                          ECityPaletteAction::SelectBaseballPark, ECityPaletteAction::SelectSoccerPark})
		{
			const int32 I =
			    Controller->VisiblePaletteEntries.IndexOfByPredicate([&](const auto& E) { return E.Action == Action; });
			if (TestTrue(TEXT("Sports action present in fixed and hand menus"), I != INDEX_NONE))
				Controller->ExecutePaletteAction(I);
			const auto Tool = Action == ECityPaletteAction::SelectBasketballPark ? ECityToolMode::BasketballPark
			                  : Action == ECityPaletteAction::SelectSkatePark    ? ECityToolMode::SkatePark
			                  : Action == ECityPaletteAction::SelectBaseballPark ? ECityToolMode::BaseballPark
			                                                                     : ECityToolMode::SoccerPark;
			TestEqual(TEXT("Real drawer action selects sports park"), Controller->Board->GetToolMode(), Tool);
		}
	}
	Controller->PaletteAnchorMode = 0;

	for (const auto Hand : {ECityControllerHand::Left, ECityControllerHand::Right})
	{
		Controller->DominantHand = Hand;
		Controller->SetPaletteOpen(true);
		Controller->SetPalettePage(ECityPalettePage::Zone);
		const int32 Residential = Controller->VisiblePaletteEntries.IndexOfByPredicate(
		    [](const FCityPaletteEntry& Entry) { return Entry.Action == ECityPaletteAction::SelectResidentialZone; });
		Controller->ExecutePaletteAction(Residential);
		// Use the input callbacks so the test cannot drift from the actual input bit assignments.
		Controller->bManipulationSession = true;
		Controller->LeftTrigger();
		Controller->RightTrigger();
		Controller->bManipulationSession = false;
		Controller->SetPaletteOpen(false);
		TestEqual(TEXT("Closing retains selected tool"), Controller->Board->GetToolMode(), ECityToolMode::Zone);
		TestTrue(TEXT("Held triggers suppressed on close"), Controller->SuppressedTriggerMask != 0);
		Controller->LeftTriggerReleased();
		Controller->RightTriggerReleased();
		TestEqual(TEXT("Fresh release rearms triggers"), Controller->SuppressedTriggerMask, static_cast<uint16>(0));
	}
	Controller->SetPaletteOpen(true);
	Controller->SetPalettePage(ECityPalettePage::Taxes);
	Controller->ResidentialTaxControl.Begin(1000);
	Controller->ResidentialTaxControl.StepBy(1);
	Controller->BeginManipulationSession();
	TestFalse(TEXT("Manipulation closes the drawer"), Controller->bPaletteOpen);
	TestEqual(TEXT("Manipulation discards pending taxes"), Controller->ResidentialTaxControl.GetPendingValue(), 1000);
	Controller->EndManipulationSession();
	Controller->SetPaletteOpen(true);
	Controller->HandleApplicationBackgrounded();
	TestFalse(TEXT("Suspend closes the drawer"), Controller->bPaletteOpen);
	TestFalse(TEXT("Suspend requires fresh input"), Controller->LifecycleInputGate.CanAcceptInput());
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityBuildMenuOrganizationTest, "CitySim.Desktop.BuildMenuOrganization",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityBuildMenuOrganizationTest::RunTest(const FString& Parameters)
{
	const auto Build = FCityPaletteModel::Entries(ECityPalettePage::Build);
	const TArray<ECityPaletteAction> Order = {ECityPaletteAction::OpenRoads,
	    ECityPaletteAction::OpenPowerFacilities, ECityPaletteAction::OpenWaterFacilities,
	    ECityPaletteAction::SelectPolice, ECityPaletteAction::SelectFire, ECityPaletteAction::OpenParks,
	    ECityPaletteAction::OpenEducationFacilities, ECityPaletteAction::SelectHospital,
	    ECityPaletteAction::OpenWasteFacilities, ECityPaletteAction::OpenTechnologyFacilities,
	    ECityPaletteAction::Back};
	TestEqual(TEXT("Build contains the objective sequence and Back"), Build.Num(), Order.Num());
	for (int32 I = 0; I < FMath::Min(Build.Num(), Order.Num()); ++I)
		TestEqual(TEXT("Build follows the player objective sequence"), Build[I].Action, Order[I]);
	const auto Waste = FCityPaletteModel::Entries(ECityPalettePage::WasteFacilities);
	TestEqual(TEXT("Waste construction has two facilities and Back"), Waste.Num(), 3);
	for (const auto& Entry : Waste)
		TestTrue(TEXT("Waste construction has no diagnostic actions"),
		    Entry.Action == ECityPaletteAction::SelectWasteDepot || Entry.Action == ECityPaletteAction::SelectRecycling ||
		    Entry.Action == ECityPaletteAction::Back);
	TestTrue(TEXT("Waste overlay remains discoverable in Overlays"),
	    FCityPaletteModel::Entries(ECityPalettePage::Overlays).ContainsByPredicate(
	        [](const auto& E) { return E.Action == ECityPaletteAction::OverlayWaste; }));
	TestTrue(TEXT("Waste history remains discoverable in Reports"),
	    FCityPaletteModel::Entries(ECityPalettePage::Reports).ContainsByPredicate(
	        [](const auto& E) { return E.Action == ECityPaletteAction::ReportWaste; }));
	// Check the affected surfaces without making unrelated City/Settings layout a gate for this slice.
	for (const bool bHand : {false, true})
		for (const auto Page : {ECityPalettePage::Build, ECityPalettePage::Roads, ECityPalettePage::PowerFacilities,
		    ECityPalettePage::WaterFacilities, ECityPalettePage::EducationFacilities, ECityPalettePage::Parks,
		    ECityPalettePage::WasteFacilities, ECityPalettePage::TechnologyFacilities,
		    ECityPalettePage::Overlays, ECityPalettePage::Reports})
		{
			const auto Entries = FCityPaletteModel::SurfaceEntries(Page, bHand);
			const auto Surface = FCityPaletteModel::SurfaceRect(Page, bHand);
			TestTrue(TEXT("Affected menu fits component pool"), Entries.Num() <= FCityPaletteModel::MaximumEntryCount);
			for (int32 I = 0; I < Entries.Num(); ++I)
			{
				const auto Rect = FCityPaletteModel::ButtonRect(Page, I, Entries[I].Action, bHand);
				TestTrue(TEXT("Affected targets fit their surface"),
				    Surface.Contains(Rect.Center + FVector(0, Rect.HalfSize.X, Rect.HalfSize.Y)) &&
				    Surface.Contains(Rect.Center - FVector(0, Rect.HalfSize.X, Rect.HalfSize.Y)));
				for (int32 J = 0; J < I; ++J)
				{
					const auto Other = FCityPaletteModel::ButtonRect(Page, J, Entries[J].Action, bHand);
					TestTrue(TEXT("Affected targets do not overlap"),
					    FMath::Abs(Rect.Center.Y - Other.Center.Y) > Rect.HalfSize.X + Other.HalfSize.X ||
					    FMath::Abs(Rect.Center.Z - Other.Center.Z) > Rect.HalfSize.Y + Other.HalfSize.Y);
				}
			}
		}
	for (const auto Page : {ECityPalettePage::Build, ECityPalettePage::Roads, ECityPalettePage::PowerFacilities,
	    ECityPalettePage::WaterFacilities, ECityPalettePage::EducationFacilities, ECityPalettePage::Parks,
	    ECityPalettePage::WasteFacilities, ECityPalettePage::TechnologyFacilities})
	{
		TSet<FName> Symbols;
		for (const auto& Entry : FCityPaletteModel::Entries(Page))
		{
			if (FCityPaletteModel::ConstructionDefinition(Entry.Action).IsNone() && Page != ECityPalettePage::Roads)
				continue;
			if (Entry.Action == ECityPaletteAction::Back) continue;
			const FName Symbol = UCityDesktopIcon::ForAction(static_cast<int32>(Entry.Action));
			TestFalse(TEXT("Construction choices have pictograms"), Symbol.IsNone());
			TestFalse(TEXT("Choices within a construction menu have distinct pictograms"), Symbols.Contains(Symbol));
			Symbols.Add(Symbol);
		}
	}
	return true;
}
#endif
