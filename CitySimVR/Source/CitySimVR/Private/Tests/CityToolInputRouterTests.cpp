#include "Interaction/CityToolInputRouter.h"
#include "Interaction/CityPaletteModel.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityPaletteHierarchyTest, "CitySim.Input.PaletteHierarchy",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityPaletteHierarchyTest::RunTest(const FString& Parameters)
{
	const FRotator IconRotation(0.0f, -90.0f, 90.0f);
	TestTrue(TEXT("Icon image right aligns with viewer right"),
	         IconRotation.RotateVector(FVector::ForwardVector).Equals(-FVector::RightVector));
	TestTrue(TEXT("Icon image up aligns with viewer up"),
	         IconRotation.RotateVector(FVector::RightVector).Equals(-FVector::UpVector));
	TestTrue(TEXT("Icon plane normal aligns with palette forward"),
	         IconRotation.RotateVector(FVector::UpVector).Equals(FVector::ForwardVector));

	const TArray<FCityPaletteEntry> Root = FCityPaletteModel::Entries(ECityPalettePage::Root);
	TestTrue(TEXT("Root remains compact"), Root.Num() <= 7);
	TestFalse(TEXT("Grip-driven root has no redundant close control"),
	          Root.ContainsByPredicate([](const FCityPaletteEntry& Entry)
	                                   { return Entry.Action == ECityPaletteAction::Close; }));

	for (const ECityPalettePage Page :
	     {ECityPalettePage::Build, ECityPalettePage::Roads, ECityPalettePage::Zone, ECityPalettePage::City,
	      ECityPalettePage::Settings, ECityPalettePage::Lighting, ECityPalettePage::Overlays})
	{
		const TArray<FCityPaletteEntry> Entries = FCityPaletteModel::Entries(Page);
		TestTrue(TEXT("Submenu fits bounded component pool"),
		         Entries.Num() <= FCityPaletteModel::MaximumEntryCount - FCityPaletteModel::BarEntryCount);
		TestTrue(TEXT("Submenu has Back"),
		         Entries.ContainsByPredicate([](const FCityPaletteEntry& Entry)
		                                     { return Entry.Action == ECityPaletteAction::Back; }));
		TestFalse(TEXT("Grip-driven submenu has no redundant close control"),
		          Entries.ContainsByPredicate([](const FCityPaletteEntry& Entry)
		                                      { return Entry.Action == ECityPaletteAction::Close; }));
	}

	for (bool Hand : {false, true})
	{
		const auto Build = FCityPaletteModel::SurfaceEntries(ECityPalettePage::Build, Hand);
		const auto Surface = FCityPaletteModel::SurfaceRect(ECityPalettePage::Build, Hand);
		for (int32 I = 0; I < Build.Num(); ++I)
		{
			const auto A = FCityPaletteModel::ButtonRect(ECityPalettePage::Build, I, Build[I].Action, Hand);
			TestTrue(TEXT("Expanded build targets fit the surface"),
			         Surface.Contains(A.Center + FVector(0, A.HalfSize.X, A.HalfSize.Y)) &&
			             Surface.Contains(A.Center - FVector(0, A.HalfSize.X, A.HalfSize.Y)));
			for (int32 J = 0; J < I; ++J)
			{
				const auto B = FCityPaletteModel::ButtonRect(ECityPalettePage::Build, J, Build[J].Action, Hand);
				TestTrue(TEXT("Expanded build targets do not overlap"),
				         FMath::Abs(A.Center.Y - B.Center.Y) >= A.HalfSize.X + B.HalfSize.X ||
				             FMath::Abs(A.Center.Z - B.Center.Z) >= A.HalfSize.Y + B.HalfSize.Y);
			}
		}
	}

	const TArray<FCityPaletteEntry> City = FCityPaletteModel::Entries(ECityPalettePage::City);
	const TArray<FCityPaletteEntry> Settings = FCityPaletteModel::Entries(ECityPalettePage::Settings);
	// Game-level controls share the settings drawer; City retains tax policy.
	for (const ECityPaletteAction IconAction : {ECityPaletteAction::Reset, ECityPaletteAction::Back})
	{
		const FCityPaletteEntry* Entry = Settings.FindByPredicate([IconAction](const FCityPaletteEntry& Candidate)
		                                                          { return Candidate.Action == IconAction; });
		TestNotNull(TEXT("Required compact icon exists"), Entry);
		if (Entry)
		{
			TestTrue(TEXT("Compact control is icon-only"), Entry->bIconOnly);
			TestTrue(TEXT("Icon exposes shared hover help"), FCString::Strlen(Entry->Help) > 0);
		}
	}
	for (const ECityPaletteAction IconAction : {ECityPaletteAction::Save})
	{
		const FCityPaletteEntry* Entry = Settings.FindByPredicate([IconAction](const FCityPaletteEntry& Candidate)
		                                                          { return Candidate.Action == IconAction; });
		TestNotNull(TEXT("Settings persistence icon exists"), Entry);
		if (Entry)
			TestTrue(TEXT("Persistence action remains icon-only"), Entry->bIconOnly);
	}
	for (const auto Action :
	     {ECityPaletteAction::CycleAnchor, ECityPaletteAction::ToggleDebugInfo, ECityPaletteAction::OpenLighting,
	      ECityPaletteAction::Save, ECityPaletteAction::ReviewLoad, ECityPaletteAction::Reset})
	{
		TestTrue(
		    TEXT("Game actions live in settings"),
		    Settings.ContainsByPredicate([Action](const FCityPaletteEntry& Entry) { return Entry.Action == Action; }));
		TestFalse(TEXT("City excludes game settings"), City.ContainsByPredicate([Action](const FCityPaletteEntry& Entry)
		                                                                        { return Entry.Action == Action; }));
	}
	TestFalse(TEXT("Settings does not load immediately"),
	          Settings.ContainsByPredicate([](const auto& Entry) { return Entry.Action == ECityPaletteAction::Load; }));
	TestTrue(TEXT("Settings exposes main menu"),
	         Settings.ContainsByPredicate([](const auto& Entry)
	                                      { return Entry.Action == ECityPaletteAction::OpenMainMenu; }));
	TestTrue(TEXT("Root exposes gear with helper text"),
	         Root.ContainsByPredicate(
	             [](const FCityPaletteEntry& Entry)
	             {
		             return Entry.Action == ECityPaletteAction::OpenSettings && Entry.bIconOnly &&
		                    FCString::Strlen(Entry.Help) > 0;
	             }));
	const TArray<FCityPaletteEntry> Taxes = FCityPaletteModel::Entries(ECityPalettePage::Taxes);
	TestTrue(TEXT("Tax policy exposes an explicit apply action"),
	         Taxes.ContainsByPredicate([](const FCityPaletteEntry& Entry)
	                                   { return Entry.Action == ECityPaletteAction::ApplyTaxes; }));
	TestTrue(TEXT("Tax policy exposes an explicit cancel action"),
	         Taxes.ContainsByPredicate([](const FCityPaletteEntry& Entry)
	                                   { return Entry.Action == ECityPaletteAction::CancelTaxes; }));

	const TArray<FCityPaletteEntry> Zone = FCityPaletteModel::Entries(ECityPalettePage::Zone);
	int32 FunctionalZoneCount = 0;
	for (const FCityPaletteEntry& Entry : Zone)
	{
		if (Entry.Action == ECityPaletteAction::SelectResidentialZone ||
		    Entry.Action == ECityPaletteAction::SelectCommercialZone ||
		    Entry.Action == ECityPaletteAction::SelectIndustrialZone)
		{
			++FunctionalZoneCount;
		}
	}
	TestEqual(TEXT("All three functional zone types are exposed"), FunctionalZoneCount, 3);
	const TArray<FCityPaletteEntry> Roads = FCityPaletteModel::Entries(ECityPalettePage::Roads);
	int32 RoadClassCount = 0;
	for (const FCityPaletteEntry& Entry : Roads)
	{
		if (Entry.Action == ECityPaletteAction::SelectDirtRoad || Entry.Action == ECityPaletteAction::SelectLocalRoad ||
		    Entry.Action == ECityPaletteAction::SelectAvenue || Entry.Action == ECityPaletteAction::SelectHighway)
		{
			++RoadClassCount;
		}
	}
	TestEqual(TEXT("All four road classes are exposed"), RoadClassCount, 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityToolInputRouterTest, "CitySim.Input.ToolRouting",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityToolInputRouterTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Right dominant trigger confirms"),
	          FCityToolInputRouter::RouteTrigger(ECityControllerHand::Right, ECityControllerHand::Right, false, false),
	          ECityTriggerRoute::ConfirmTool);
	TestEqual(TEXT("Off hand trigger does not issue a tool command"),
	          FCityToolInputRouter::RouteTrigger(ECityControllerHand::Left, ECityControllerHand::Right, true, true),
	          ECityTriggerRoute::None);
	TestEqual(TEXT("Palette hit consumes dominant trigger"),
	          FCityToolInputRouter::RouteTrigger(ECityControllerHand::Right, ECityControllerHand::Right, true, true),
	          ECityTriggerRoute::SelectPalette);
	TestEqual(TEXT("Open drawer miss cannot issue a board command"),
	          FCityToolInputRouter::RouteTrigger(ECityControllerHand::Right, ECityControllerHand::Right, true, false),
	          ECityTriggerRoute::None);
	TestEqual(TEXT("Left-handed mode swaps dominant routing"),
	          FCityToolInputRouter::RouteTrigger(ECityControllerHand::Left, ECityControllerHand::Left, false, false),
	          ECityTriggerRoute::ConfirmTool);
	TestEqual(TEXT("Other hand is deterministic"), FCityToolInputRouter::OtherHand(ECityControllerHand::Left),
	          ECityControllerHand::Right);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityLifecycleInputGateTest, "CitySim.Input.LifecycleFreshEdges",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityLifecycleInputGateTest::RunTest(const FString& Parameters)
{
	FCityLifecycleInputGate Gate;
	TestTrue(TEXT("Input starts enabled"), Gate.CanAcceptInput());
	Gate.Suspend();
	TestFalse(TEXT("Suspend rejects stale edges"), Gate.CanAcceptInput());
	Gate.Update(true);
	TestFalse(TEXT("Held input keeps the gate closed"), Gate.CanAcceptInput());
	Gate.Update(false);
	TestTrue(TEXT("Full release permits fresh edges"), Gate.CanAcceptInput());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityTemplateMenuIsolationTest, "CitySim.Input.TemplateMenuSourceBindings",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityTemplateMenuIsolationTest::RunTest(const FString& Parameters)
{
	UInputMappingContext* TemplateContext =
	    LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/XRFramework/Input/IMC_Default.IMC_Default"));
	UInputAction* LeftMenuAction = LoadObject<UInputAction>(
	    nullptr, TEXT("/Game/XRFramework/Input/Actions/IA_Menu_Toggle_Left.IA_Menu_Toggle_Left"));
	UInputAction* RightMenuAction = LoadObject<UInputAction>(
	    nullptr, TEXT("/Game/XRFramework/Input/Actions/IA_Menu_Toggle_Right.IA_Menu_Toggle_Right"));
	TestNotNull(TEXT("Template context loads"), TemplateContext);
	TestNotNull(TEXT("Left template menu action loads"), LeftMenuAction);
	TestNotNull(TEXT("Right template menu action loads"), RightMenuAction);
	if (!TemplateContext || !LeftMenuAction || !RightMenuAction)
	{
		return false;
	}

	bool bFoundQuestY = false;
	bool bFoundQuestB = false;
	int32 OriginalMenuMappings = 0;
	for (const FEnhancedActionKeyMapping& Mapping : TemplateContext->GetMappings())
	{
		if (Mapping.Action == LeftMenuAction || Mapping.Action == RightMenuAction)
		{
			++OriginalMenuMappings;
			bFoundQuestY |= Mapping.Key == EKeys::OculusTouch_Left_Y_Click;
			bFoundQuestB |= Mapping.Key == EKeys::OculusTouch_Right_B_Click;
		}
	}
	TestTrue(TEXT("Quest Y is a source template menu binding"), bFoundQuestY);
	TestTrue(TEXT("Quest B is a source template menu binding"), bFoundQuestB);

	TestTrue(TEXT("Source context contains menu mappings"), OriginalMenuMappings > 0);

	UInputMappingContext* CityFaceContext =
	    LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/CitySim/Input/IMC_CityFaceButtons.IMC_CityFaceButtons"));
	TestNotNull(TEXT("City face context loads"), CityFaceContext);
	if (!CityFaceContext)
	{
		return false;
	}
	TSet<FKey> CityFaceKeys;
	for (const FEnhancedActionKeyMapping& Mapping : CityFaceContext->GetMappings())
	{
		CityFaceKeys.Add(Mapping.Key);
	}
	TestEqual(TEXT("City face context has exactly four mappings"), CityFaceContext->GetMappings().Num(), 4);
	TestTrue(TEXT("City face context registers Quest A"), CityFaceKeys.Contains(EKeys::OculusTouch_Right_A_Click));
	TestTrue(TEXT("City face context registers Quest B"), CityFaceKeys.Contains(EKeys::OculusTouch_Right_B_Click));
	TestTrue(TEXT("City face context registers Quest X"), CityFaceKeys.Contains(EKeys::OculusTouch_Left_X_Click));
	TestTrue(TEXT("City face context registers Quest Y"), CityFaceKeys.Contains(EKeys::OculusTouch_Left_Y_Click));
	return true;
}

#endif
