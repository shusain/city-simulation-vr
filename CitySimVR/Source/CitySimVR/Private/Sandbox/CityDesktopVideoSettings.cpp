#include "Sandbox/CityDesktopVideoSettings.h"
#include "GameFramework/GameUserSettings.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/IConsoleManager.h"

void FCityDesktopVideoSettings::Initialize(UGameUserSettings* InSettings, const TArray<FIntPoint>& Supported,
                                           const FIntPoint Native, const bool bInApplyToViewport, const bool bInPersist)
{
	Settings = InSettings;
	bApplyToViewport = bInApplyToViewport;
	bPersist = bInPersist;
	NativeResolution = Native.X >= 1024 && Native.Y >= 720 ? Native : FIntPoint(1280, 800);
	Resolutions.Reset();
	FullscreenResolutions.Reset();
	for (const auto Size : Supported)
		if (Size.X >= 1024 && Size.Y >= 720 && Size.X <= NativeResolution.X && Size.Y <= NativeResolution.Y)
			FullscreenResolutions.AddUnique(Size);
	FullscreenResolutions.AddUnique(NativeResolution);
	FullscreenResolutions.Sort([](const FIntPoint& A, const FIntPoint& B)
	                           { return A.X == B.X ? A.Y < B.Y : A.X < B.X; });
	Resolutions = FullscreenResolutions;
	for (const auto Size : {FIntPoint(1024, 720), FIntPoint(1280, 720), FIntPoint(1280, 800), FIntPoint(1600, 900),
	                        FIntPoint(1920, 1080), FIntPoint(2560, 1440)})
		if (Size.X <= NativeResolution.X && Size.Y <= NativeResolution.Y)
			Resolutions.AddUnique(Size);
	Resolutions.AddUnique(NativeResolution);
	if (Settings && Settings->GetScreenResolution().X >= 1024 && Settings->GetScreenResolution().Y >= 720)
		Resolutions.AddUnique(Settings->GetScreenResolution());
	Resolutions.Sort([](const FIntPoint& A, const FIntPoint& B) { return A.X == B.X ? A.Y < B.Y : A.X < B.X; });
	Confirmed =
	    Settings ? FChoice{Settings->GetScreenResolution(), Settings->GetFullscreenMode(), Settings->IsVSyncEnabled()}
	             : FChoice{};
	if (bPersist)
	{
		GConfig->GetInt(TEXT("CitySim.DesktopVideo"), TEXT("RenderScale"), Confirmed.RenderScale, GGameUserSettingsIni);
		GConfig->GetInt(TEXT("CitySim.DesktopVideo"), TEXT("MSAASamples"), Confirmed.MSAASamples, GGameUserSettingsIni);
	}
	if (Confirmed.RenderScale != 50 && Confirmed.RenderScale != 75 && Confirmed.RenderScale != 100 &&
	    Confirmed.RenderScale != 125)
		Confirmed.RenderScale = 100;
	if (Confirmed.MSAASamples != 2 && Confirmed.MSAASamples != 4 && Confirmed.MSAASamples != 8)
		Confirmed.MSAASamples = 4;
	Pending = Confirmed;
	if (bApplyToViewport || bPersist)
	{
		QualityTag = FName(*FString::Printf(TEXT("CityDesktopQuality_%p"), this));
		for (const TCHAR* Name :
		     {TEXT("r.ScreenPercentage"), TEXT("r.SecondaryScreenPercentage.GameViewport"), TEXT("r.MSAACount")})
			if (IConsoleManager::Get().FindConsoleVariable(Name))
				QualityCVars.Add({Name});
		SetQuality(Confirmed);
	}
}
void FCityDesktopVideoSettings::BeginEdit()
{
	Cancel();
	if (!Settings)
		return;
	Confirmed.Resolution = Settings->GetScreenResolution();
	Confirmed.Mode = Settings->GetFullscreenMode();
	Confirmed.bVSync = Settings->IsVSyncEnabled();
	Pending = Confirmed;
}
void FCityDesktopVideoSettings::CycleMode()
{
	if (!Settings || IsConfirming())
		return;
	Pending.Mode = Pending.Mode == EWindowMode::Fullscreen           ? EWindowMode::WindowedFullscreen
	               : Pending.Mode == EWindowMode::WindowedFullscreen ? EWindowMode::Windowed
	                                                                 : EWindowMode::Fullscreen;
	if (Pending.Mode == EWindowMode::WindowedFullscreen)
		Pending.Resolution = NativeResolution;
	else if (Pending.Mode == EWindowMode::Fullscreen && !FullscreenResolutions.Contains(Pending.Resolution))
		Pending.Resolution = NativeResolution;
}
void FCityDesktopVideoSettings::CycleResolution(const int32 Direction)
{
	if (!Settings || IsConfirming() || Pending.Mode == EWindowMode::WindowedFullscreen || Resolutions.IsEmpty())
		return;
	const auto& Choices = Pending.Mode == EWindowMode::Fullscreen ? FullscreenResolutions : Resolutions;
	const int32 Index = FMath::Max(0, Choices.IndexOfByKey(Pending.Resolution));
	Pending.Resolution = Choices[(Index + (Direction < 0 ? Choices.Num() - 1 : 1)) % Choices.Num()];
}
void FCityDesktopVideoSettings::ToggleVSync()
{
	if (Settings && !IsConfirming())
		Pending.bVSync = !Pending.bVSync;
}
bool FCityDesktopVideoSettings::IsDirty() const
{
	return Settings && !(Pending == Confirmed);
}
void FCityDesktopVideoSettings::CycleRenderScale()
{
	if (Settings && !IsConfirming())
		Pending.RenderScale = Pending.RenderScale == 50    ? 75
		                      : Pending.RenderScale == 75  ? 100
		                      : Pending.RenderScale == 100 ? 125
		                                                   : 50;
}
void FCityDesktopVideoSettings::CycleAntiAliasing()
{
	if (Settings && !IsConfirming())
		Pending.MSAASamples = Pending.MSAASamples == 2 ? 4 : Pending.MSAASamples == 4 ? 8 : 2;
}
void FCityDesktopVideoSettings::SetQuality(const FChoice& Choice)
{
	for (auto& Owned : QualityCVars)
		if (auto* Var = IConsoleManager::Get().FindConsoleVariable(Owned.Name))
		{
			const float Value = FCString::Strcmp(Owned.Name, TEXT("r.MSAACount")) == 0          ? Choice.MSAASamples
			                    : FCString::Strcmp(Owned.Name, TEXT("r.ScreenPercentage")) == 0 ? Choice.RenderScale
			                                                                                    : 100;
			// Tagged game overrides restore the prior layer when this desktop world ends.
			// Explicit console/benchmark overrides remain higher priority.
			Var->Set(Value, ECVF_SetByGameOverride, QualityTag);
		}
}
void FCityDesktopVideoSettings::Shutdown()
{
	Cancel();
	for (const auto& Owned : QualityCVars)
		if (auto* Var = IConsoleManager::Get().FindConsoleVariable(Owned.Name))
			Var->Unset(ECVF_SetByGameOverride, QualityTag);
	QualityCVars.Reset();
}
void FCityDesktopVideoSettings::SetLive(const FChoice& Choice)
{
	Settings->SetScreenResolution(Choice.Resolution);
	Settings->SetFullscreenMode(Choice.Mode);
	Settings->SetVSyncEnabled(Choice.bVSync);
	if (bApplyToViewport)
	{
		Settings->ApplyResolutionSettings(false);
		Settings->ApplyNonResolutionSettings();
	}
	SetQuality(Choice);
}
bool FCityDesktopVideoSettings::Apply(const double Now)
{
	if (!IsDirty() || IsConfirming() || !FMath::IsFinite(Now))
		return false;
	SetLive(Pending);
	Deadline = Now + ConfirmationSeconds;
	return true;
}
bool FCityDesktopVideoSettings::Keep()
{
	if (!Settings || !IsConfirming())
		return false;
	Settings->ConfirmVideoMode();
	Confirmed = Pending;
	Deadline = 0;
	if (bPersist)
	{
		Settings->SaveSettings();
		GConfig->SetBool(TEXT("CitySim.DesktopVideo"), TEXT("Initialized"), true, GGameUserSettingsIni);
		GConfig->SetInt(TEXT("CitySim.DesktopVideo"), TEXT("RenderScale"), Confirmed.RenderScale, GGameUserSettingsIni);
		GConfig->SetInt(TEXT("CitySim.DesktopVideo"), TEXT("MSAASamples"), Confirmed.MSAASamples, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
	return true;
}
void FCityDesktopVideoSettings::Cancel()
{
	if (Settings && IsConfirming())
		SetLive(Confirmed);
	Deadline = 0;
	Pending = Confirmed;
}
bool FCityDesktopVideoSettings::Tick(const double Now)
{
	if (IsConfirming() && Now >= Deadline)
	{
		Cancel();
		return true;
	}
	return false;
}
FString FCityDesktopVideoSettings::ModeLabel() const
{
	return Pending.Mode == EWindowMode::Fullscreen           ? TEXT("FULLSCREEN")
	       : Pending.Mode == EWindowMode::WindowedFullscreen ? TEXT("BORDERLESS")
	                                                         : TEXT("WINDOWED");
}
FString FCityDesktopVideoSettings::Description(const double Now) const
{
	const FString Size = FString::Printf(TEXT("Resolution: %d x %d"), Pending.Resolution.X, Pending.Resolution.Y);
	if (IsConfirming())
		return Size + FString::Printf(TEXT("\nKeep this display mode? Reverts in %d seconds."),
		                              FMath::Max(0, FMath::CeilToInt(Deadline - Now)));
	return Size + FString::Printf(TEXT("\nScene: %d%% | MSAA %dx"), Pending.RenderScale, Pending.MSAASamples) +
	       (Pending.Mode == EWindowMode::WindowedFullscreen
	            ? TEXT(
	                  "\nBorderless uses desktop resolution. Click MODE for Fullscreen/Windowed display sizes. Scene scale changes map detail without resizing the UI.")
	            : TEXT("\nApply, then Keep within 15 seconds. Esc or Back discards changes."));
}
