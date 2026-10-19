#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "GameFramework/GameUserSettings.h"
#include "Sandbox/CityDesktopVideoSettings.h"
#include "HAL/IConsoleManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityDesktopVideoTest, "CitySim.Desktop.VideoConfirmationAndRecovery",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityDesktopVideoTest::RunTest(const FString& Parameters)
{
	auto* Settings = NewObject<UGameUserSettings>();
	Settings->SetScreenResolution(FIntPoint(1280, 800));
	Settings->SetFullscreenMode(EWindowMode::Windowed);
	Settings->SetVSyncEnabled(false);
	FCityDesktopVideoSettings Video;
	Video.Initialize(Settings, {FIntPoint(800, 600), FIntPoint(1280, 720), FIntPoint(1920, 1080)},
	                 FIntPoint(1920, 1080), false, false);
	TestFalse(TEXT("Unchanged preferences cannot be applied"), Video.Apply(10));
	TestFalse(TEXT("Keep is unavailable without a preview"), Video.Keep());
	Video.CycleMode();
	TestEqual(TEXT("Fullscreen chooses a supported mode instead of an arbitrary window size"), Video.GetResolution(),
	          FIntPoint(1920, 1080));
	Video.CycleResolution(-1);
	TestEqual(TEXT("Unsupported small sizes are excluded from fullscreen choices"), Video.GetResolution(),
	          FIntPoint(1280, 720));
	TestTrue(TEXT("Changed display preferences enter preview"), Video.Apply(10));
	TestEqual(TEXT("Preview updates the live settings"), Settings->GetFullscreenMode(), EWindowMode::Fullscreen);
	Video.CycleMode();
	TestEqual(TEXT("Editing is locked while a preview awaits confirmation"), Video.GetMode(), EWindowMode::Fullscreen);
	TestFalse(TEXT("Deadline does not revert early"), Video.Tick(24.9));
	TestTrue(TEXT("Wall-time deadline reverts even if city time is paused"), Video.Tick(25));
	TestEqual(TEXT("Timeout restores previous size"), Settings->GetScreenResolution(), FIntPoint(1280, 800));
	TestEqual(TEXT("Timeout restores previous window mode"), Settings->GetFullscreenMode(), EWindowMode::Windowed);
	Video.CycleMode();
	Video.CycleMode();
	TestEqual(TEXT("Borderless fills the desktop"), Video.GetResolution(), FIntPoint(1920, 1080));
	Video.CycleResolution(-1);
	TestEqual(TEXT("Borderless cannot choose a conflicting display resolution"), Video.GetResolution(),
	          FIntPoint(1920, 1080));
	Video.ToggleVSync();
	Video.Apply(30);
	Video.BeginEdit();
	TestFalse(TEXT("Leaving a preview reverts before beginning another edit"), Settings->IsVSyncEnabled());
	Video.ToggleVSync();
	TestTrue(TEXT("A new VSync preview is available"), Video.Apply(40));
	TestTrue(TEXT("Keep confirms a visible preview"), Video.Keep());
	TestFalse(TEXT("Confirmed preference has no outstanding timer"), Video.Tick(100));
	Video.Cancel();
	TestTrue(TEXT("Cancel does not undo a confirmed preference"), Settings->IsVSyncEnabled());
	TestEqual(TEXT("Confirmed resolution remains unchanged after Keep"), Settings->GetLastConfirmedScreenResolution(),
	          FIntPoint(1280, 800));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityDesktopQualityTest, "CitySim.Desktop.Native4KQualityRecovery",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityDesktopQualityTest::RunTest(const FString& Parameters)
{
	auto* Settings = NewObject<UGameUserSettings>();
	Settings->SetScreenResolution(FIntPoint(3840, 2160));
	Settings->SetFullscreenMode(EWindowMode::WindowedFullscreen);
	FCityDesktopVideoSettings Video;
	Video.Initialize(Settings, {FIntPoint(2560, 1440), FIntPoint(3840, 2160)}, FIntPoint(3840, 2160), false, false);
	TestEqual(TEXT("Default scene scale is native, independent of DPI"), Video.GetRenderScale(), 100);
	Video.CycleRenderScale();
	Video.CycleAntiAliasing();
	TestEqual(TEXT("Supersampling is available without resizing the display"), Video.GetRenderScale(), 125);
	TestEqual(TEXT("Higher MSAA is available"), Video.GetAntiAliasingSamples(), 8);
	Video.Apply(10);
	Video.CycleAntiAliasing();
	TestEqual(TEXT("Preview locks AA edits"), Video.GetAntiAliasingSamples(), 8);
	Video.Tick(25);
	TestEqual(TEXT("Timeout restores scene scale"), Video.GetRenderScale(), 100);
	TestEqual(TEXT("Timeout restores AA"), Video.GetAntiAliasingSamples(), 4);
	Video.CycleMode();
	Video.CycleMode();
	Video.CycleResolution(-1);
	TestEqual(TEXT("4K fullscreen can select lower supported output"), Video.GetResolution(), FIntPoint(2560, 1440));
	Video.Shutdown();
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityDesktopLiveQualityTest, "CitySim.Desktop.LiveQualityOwnership",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityDesktopLiveQualityTest::RunTest(const FString& Parameters)
{
	auto* Scale = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"));
	auto* AA = IConsoleManager::Get().FindConsoleVariable(TEXT("r.MSAACount"));
	auto* Secondary = IConsoleManager::Get().FindConsoleVariable(TEXT("r.SecondaryScreenPercentage.GameViewport"));
	const float PreviousScale = Scale->GetFloat(), PreviousAA = AA->GetFloat(),
	            PreviousSecondary = Secondary->GetFloat();
	auto* Settings = NewObject<UGameUserSettings>();
	Settings->SetScreenResolution(FIntPoint(3840, 2160));
	FCityDesktopVideoSettings Video;
	// Read preferences but do not Keep/flush; no viewport resize or player profile mutation.
	Video.Initialize(Settings, {}, FIntPoint(3840, 2160), false, true);
	TestEqual(TEXT("Desktop owns actual scene scale"), Scale->GetInt(), Video.GetRenderScale());
	TestEqual(TEXT("Desktop disables secondary DPI scene reduction"), Secondary->GetInt(), 100);
	const int32 ConfirmedAA = Video.GetAntiAliasingSamples();
	Video.CycleAntiAliasing();
	Video.Apply(10);
	TestEqual(TEXT("Preview updates actual MSAA"), AA->GetInt(), Video.GetAntiAliasingSamples());
	Video.Tick(25);
	TestEqual(TEXT("Timeout restores actual MSAA"), AA->GetInt(), ConfirmedAA);
	Video.Shutdown();
	TestEqual(TEXT("Ending desktop ownership restores prior scale"), Scale->GetFloat(), PreviousScale);
	TestEqual(TEXT("Ending desktop ownership restores prior AA"), AA->GetFloat(), PreviousAA);
	TestEqual(TEXT("Ending desktop ownership restores prior DPI scaling"), Secondary->GetFloat(), PreviousSecondary);
	return true;
}
#endif
