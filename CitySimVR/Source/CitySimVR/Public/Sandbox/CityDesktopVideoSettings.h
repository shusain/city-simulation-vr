#pragma once

#include "CoreMinimal.h"
#include "GenericPlatform/GenericWindow.h"

class UGameUserSettings;

// Desktop presentation preferences only. Pending changes never overwrite the
// confirmed on-disk mode until Keep; the deadline uses wall time, not city time.
class FCityDesktopVideoSettings
{
public:
	void Initialize(UGameUserSettings* InSettings, const TArray<FIntPoint>& Supported, FIntPoint Native,
	                bool bInApplyToViewport = true, bool bInPersist = true);
	void BeginEdit();
	void CycleMode();
	void CycleResolution(int32 Direction);
	void ToggleVSync();
	void CycleRenderScale();
	void CycleAntiAliasing();
	void Shutdown();
	bool Apply(double Now);
	bool Keep();
	void Cancel();
	bool Tick(double Now);
	bool IsReady() const
	{
		return Settings != nullptr;
	}
	bool IsConfirming() const
	{
		return Deadline > 0;
	}
	bool IsDirty() const;
	FString ModeLabel() const;
	FString Description(double Now) const;
	FIntPoint GetResolution() const
	{
		return Pending.Resolution;
	}
	EWindowMode::Type GetMode() const
	{
		return Pending.Mode;
	}
	bool IsVSyncEnabled() const
	{
		return Pending.bVSync;
	}
	static constexpr double ConfirmationSeconds = 15;
	int32 GetRenderScale() const
	{
		return Pending.RenderScale;
	}
	int32 GetAntiAliasingSamples() const
	{
		return Pending.MSAASamples;
	}

private:
	struct FChoice
	{
		FIntPoint Resolution = FIntPoint(1280, 800);
		EWindowMode::Type Mode = EWindowMode::Windowed;
		bool bVSync = false;
		int32 RenderScale = 100;
		int32 MSAASamples = 4;
		bool operator==(const FChoice& Other) const
		{
			return Resolution == Other.Resolution && Mode == Other.Mode && bVSync == Other.bVSync &&
			       RenderScale == Other.RenderScale && MSAASamples == Other.MSAASamples;
		}
	};
	void SetLive(const FChoice& Choice);
	void SetQuality(const FChoice& Choice);
	struct FOwnedCVar
	{
		const TCHAR* Name;
	};
	TArray<FOwnedCVar> QualityCVars;
	FName QualityTag;
	UGameUserSettings* Settings = nullptr; // Engine-owned; test instances stay rooted by their caller.
	TArray<FIntPoint> Resolutions;
	TArray<FIntPoint> FullscreenResolutions;
	FIntPoint NativeResolution = FIntPoint(1280, 800);
	FChoice Confirmed, Pending;
	double Deadline = 0;
	bool bApplyToViewport = true;
	bool bPersist = true;
};
