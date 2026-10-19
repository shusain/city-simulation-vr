#include "Performance/CityBenchmarkProbe.h"

#include "DynamicRHI.h"
#include "Engine/Engine.h"
#include "HAL/PlatformMemory.h"
#include "IHeadMountedDisplay.h"
#include "IXRTrackingSystem.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/SecureHash.h"
#include "Persistence/CityPersistence.h"
#include "OculusXRFunctionLibrary.h"
#include "RenderTimer.h"
#include "RHIStats.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Interaction/CityVRToolController.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Presentation/CityDesktopHud.h"
#include "Sandbox/CitySandboxEnvironment.h"
#include "Serialization/JsonSerializer.h"
#include "UnrealClient.h"

#if PLATFORM_ANDROID
#include "Android/AndroidPlatformThermal.h"
#endif

namespace
{
	double BytesToMiB(const uint64 Bytes)
	{
		return static_cast<double>(Bytes) / (1024.0 * 1024.0);
	}
} // namespace

ACityBenchmarkProbe::ACityBenchmarkProbe()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ACityBenchmarkProbe::BeginPlay()
{
	Super::BeginPlay();
	SimulationSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UCitySimulationSubsystem>() : nullptr;
	FParse::Value(FCommandLine::Get(), TEXT("CityBenchmarkWarmup="), WarmupSeconds);
	FParse::Value(FCommandLine::Get(), TEXT("CityBenchmarkDuration="), DurationSeconds);
	FParse::Value(FCommandLine::Get(), TEXT("CityBenchmarkSampleInterval="), SampleIntervalSeconds);
	WarmupSeconds = FMath::Clamp(WarmupSeconds, 0.0, 300.0);
	DurationSeconds = FMath::Clamp(DurationSeconds, 1.0, 3600.0);
	SampleIntervalSeconds = FMath::Clamp(SampleIntervalSeconds, 1.0, 60.0);
	bExitAfterCapture = FParse::Param(FCommandLine::Get(), TEXT("CityBenchmarkExit"));
	FParse::Value(FCommandLine::Get(), TEXT("CityBenchmarkReport="), ReportPath);
	FParse::Value(FCommandLine::Get(), TEXT("CityBenchmarkTargetHz="), TargetRefreshHz);
	TargetRefreshHz = FMath::Clamp(TargetRefreshHz, 0.0, 1000.0);
#if PLATFORM_ANDROID
	bDrawStatus = true;
#else
	bDrawStatus = FParse::Param(FCommandLine::Get(), TEXT("CityBenchmarkOverlay"));
#endif
}

void ACityBenchmarkProbe::ConfigureScene()
{
	if (SimulationSubsystem && SimulationSubsystem->IsBenchmarkActive())
	{
		FString HudMode;
		FParse::Value(FCommandLine::Get(), TEXT("CityBenchmarkHud="), HudMode);
		for (TActorIterator<ACityVRToolController> It(GetWorld()); It; ++It)
		{
			if (It->IsDesktopUi())
			{
				It->SetCitySummaryVisible(HudMode == TEXT("Stats"));
				It->SetObjectivesVisible(HudMode != TEXT("Hidden"));
				if (HudMode == TEXT("Menu"))
					It->ShowGameSettings();
			}
		}
		TArray<UUserWidget*> Widgets;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Widgets, UCityDesktopHud::StaticClass(), true);
		if (!Widgets.IsEmpty())
		{
			DesktopHud = Cast<UCityDesktopHud>(Widgets[0]);
			if (HudMode == TEXT("Hidden"))
				DesktopHud->SetVisibility(ESlateVisibility::Collapsed);
		}
		FString Lighting = TEXT("Day");
		FParse::Value(FCommandLine::Get(), TEXT("CityBenchmarkLighting="), Lighting);
		for (TActorIterator<ACitySandboxEnvironment> It(GetWorld()); It; ++It)
			It->SetLightingPreset(Lighting == TEXT("Night")  ? ECityLightingPreset::Night
			                      : Lighting == TEXT("Dusk") ? ECityLightingPreset::Dusk
			                                                 : ECityLightingPreset::Day);
	}
}

void ACityBenchmarkProbe::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!SimulationSubsystem || !SimulationSubsystem->IsBenchmarkActive() || bCaptureComplete)
	{
		return;
	}

	// All actors have begun play now, including benchmark/catalog and HUD setup.
	if (!bSceneConfigured)
	{
		ConfigureScene();
		bSceneConfigured = true;
	}
	// Actor DeltaSeconds can be clamped during a long simulation hitch. Measure
	// real elapsed frames/capture duration, independent of city time and time dilation.
	const double Now = FPlatformTime::Seconds();
	const double WallDelta = LastFrameClock > 0 ? FMath::Max(0.0, Now - LastFrameClock) : 0;
	LastFrameClock = Now;
	SinceLaunchSeconds += WallDelta;
	if (!bCaptureStarted)
	{
		if (GEngine && bDrawStatus)
		{
			GEngine->AddOnScreenDebugMessage(9200, 0.0f, FColor::Yellow,
			                                 FString::Printf(TEXT("%s\nBenchmark warmup %.0f s"),
			                                                 *SimulationSubsystem->GetBenchmarkStatus(),
			                                                 FMath::Max(0.0, WarmupSeconds - SinceLaunchSeconds)),
			                                 false);
		}
		int64 RequestedStartStep = 0;
		FParse::Value(FCommandLine::Get(), TEXT("CityBenchmarkCaptureStartStep="), RequestedStartStep);
		if (SinceLaunchSeconds >= WarmupSeconds &&
		    SimulationSubsystem->GetEconomySnapshot().FixedStepId >= RequestedStartStep)
		{
			BeginCapture();
		}
		return;
	}

	const double FrameMs = WallDelta * 1000.0;
	const double GameMs = FPlatformTime::ToMilliseconds(GGameThreadTime);
	const double RenderMs = FPlatformTime::ToMilliseconds(GRenderThreadTime);
	const uint32 GpuCycles = RHIGetGPUFrameCycles();
	FOculusXRPerformanceMetrics MetaMetrics;
#if PLATFORM_ANDROID
	UOculusXRFunctionLibrary::GetPerformanceMetrics(MetaMetrics);
	bMetaMetricsAvailable |= MetaMetrics.AppGpuTime > 0;
#endif
	const double GpuMs = MetaMetrics.AppGpuTime > 0.0f ? MetaMetrics.AppGpuTime
	                     : GpuCycles > 0               ? FPlatformTime::ToMilliseconds(GpuCycles)
	                                                   : 0.0;
	CompositorDroppedFrames = MetaMetrics.DroppedFrames;
	LatestGpuUtilization = MetaMetrics.GpuUtil;
	LatestCpuUtilization = MetaMetrics.CpuUtilWorst;
	SumFrameMs += FrameMs;
	SumGameMs += GameMs;
	SumRenderMs += RenderMs;
	PeakFrameMs = FMath::Max(PeakFrameMs, FrameMs);
	FrameTiming.Add(FrameMs);
	FramesOver30Budget += FrameMs > 1000.0 / 30.0 ? 1 : 0;
	TimeOver30BudgetMs += FrameMs > 1000.0 / 30.0 ? FrameMs : 0;
	const int32 TrafficRecords = SimulationSubsystem->GetTrafficSnapshot().Vehicles.Num();
	SumTrafficRecords += TrafficRecords;
	PeakTrafficRecords = FMath::Max(PeakTrafficRecords, TrafficRecords);
	++SampleCount;
	const int32 DrawCalls = FMath::Max(0, GNumDrawCallsRHI[0]);
	SumDrawCalls += DrawCalls;
	PeakDrawCalls = FMath::Max(PeakDrawCalls, DrawCalls);
	SumPrimitives += FMath::Max(0, GNumPrimitivesDrawnRHI[0]);
	if (GpuMs > 0.0)
	{
		SumGpuMs += GpuMs;
		++GpuSampleCount;
	}
	const double TargetFrameSeconds = 1.0 / FMath::Max(1.0, TargetRefreshHz);
	if (TargetRefreshHz > 0 && WallDelta > TargetFrameSeconds * 1.5)
	{
		const int32 MissedFrames = FMath::Max(1, FMath::FloorToInt(WallDelta / TargetFrameSeconds) - 1);
		EstimatedMissedFrames += static_cast<uint64>(MissedFrames);
	}

	CaptureSeconds += WallDelta;
	SinceSampleSeconds += WallDelta;
	if (SinceSampleSeconds >= SampleIntervalSeconds)
	{
		SinceSampleSeconds = 0.0;
		EmitSample(false);
	}
	if (CaptureSeconds >= DurationSeconds)
	{
		bCaptureComplete = true;
		EmitSample(true);
		SaveReport();
		if (bExitAfterCapture)
			FPlatformMisc::RequestExit(false);
	}
}

void ACityBenchmarkProbe::BeginCapture()
{
#if PLATFORM_ANDROID
	UOculusXRFunctionLibrary::SetDisplayFrequency(TargetRefreshHz > 0 ? TargetRefreshHz : 72.0f);
	TargetRefreshHz = UOculusXRFunctionLibrary::GetCurrentDisplayFrequency();
	if (TargetRefreshHz < 1.0)
	{
		TargetRefreshHz = 72.0;
	}
#endif
	FIntPoint RenderTargetSize = FIntPoint::ZeroValue;
	if (GEngine && GEngine->XRSystem.IsValid())
	{
		if (IHeadMountedDisplay* Hmd = GEngine->XRSystem->GetHMDDevice())
		{
			RenderTargetSize = Hmd->GetIdealRenderTargetSize();
		}
	}
	FString OsLabel;
	FString OsSubLabel;
	FPlatformMisc::GetOSVersions(OsLabel, OsSubLabel);
	FOculusXRPerformanceMetrics MetaMetrics;
#if PLATFORM_ANDROID
	UOculusXRFunctionLibrary::GetPerformanceMetrics(MetaMetrics);
#endif
	CompositorDroppedFramesAtStart = MetaMetrics.DroppedFrames;
	CompositorDroppedFrames = MetaMetrics.DroppedFrames;
	bCaptureStarted = true;
	CaptureStartMonth = SimulationSubsystem->GetEconomySnapshot().LastBilledMonthId;
	CaptureStartStep = SimulationSubsystem->GetEconomySnapshot().FixedStepId;
	CaptureStartLots = SimulationSubsystem->GetPlacementModel().GetEntities().Num();
	CaptureStartCitizens = SimulationSubsystem->GetPopulationSnapshot().Citizens.Num();
	CaptureStartSpatialStats = SimulationSubsystem->GetPlacementModel().GetSpatialCacheStats();
	SimulationSubsystem->ResetBenchmarkTimings();
	if (DesktopHud.IsValid())
		DesktopHud->ResetPresentationMetrics();
	if (FParse::Param(FCommandLine::Get(), TEXT("CityBenchmarkCapture")) && !ReportPath.IsEmpty())
		FScreenshotRequest::RequestScreenshot(FPaths::GetPath(ReportPath) / TEXT("Scene.png"), true, false);
	UE_LOG(
	    LogTemp, Display,
	    TEXT(
	        "CITYSIM_BENCHMARK_BEGIN %s DurationSec=%.0f RefreshHz=%.1f RenderTarget=%dx%d BuildConfig=%s Device=%s OS=%s/%s CPU=%s GPU=%s"),
	    *SimulationSubsystem->GetBenchmarkStatus(), DurationSeconds, TargetRefreshHz, RenderTargetSize.X,
	    RenderTargetSize.Y, LexToString(FApp::GetBuildConfiguration()), *FPlatformMisc::GetDeviceMakeAndModel(),
	    *OsLabel, *OsSubLabel, *FPlatformMisc::GetCPUBrand(), *FPlatformMisc::GetPrimaryGPUBrand());
}

FString ACityBenchmarkProbe::BuildStatus(const bool bFinal) const
{
	const double Divisor = FMath::Max<uint64>(1, SampleCount);
	const double GpuDivisor = FMath::Max<uint64>(1, GpuSampleCount);
	const FPlatformMemoryStats Memory = FPlatformMemory::GetStats();
	double ThermalStress = -1.0;
#if PLATFORM_ANDROID
	ThermalStress = FAndroidPlatformThermal::GetThermalStress(FAndroidPlatformThermal::FIVE_SEC);
#endif
	const FString Refresh = TargetRefreshHz > 0 ? FString::Printf(TEXT("%.1fHz"), TargetRefreshHz) : TEXT("N/A");
	const FString Dropped =
	    bMetaMetricsAvailable
	        ? FString::FromInt(FMath::Max(0, CompositorDroppedFrames - CompositorDroppedFramesAtStart))
	        : TEXT("N/A");
	const FString Missed = TargetRefreshHz > 0 ? FString::Printf(TEXT("%llu"), EstimatedMissedFrames) : TEXT("N/A");
	return FString::Printf(
	    TEXT(
	        "%s\n%s %.0f/%.0fs @ %s\nFrame %.2fms | GT %.2f | RT %.2f | GPU %.2f\nPeak %.2fms | dropped %s | app-missed~ %s\nCPU/GPU util %.0f/%.0f%% | mem %.0f/%.0f MiB | thermal %.2f"),
	    *SimulationSubsystem->GetBenchmarkStatus(), bFinal ? TEXT("COMPLETE") : TEXT("CAPTURE"), CaptureSeconds,
	    DurationSeconds, *Refresh, SumFrameMs / Divisor, SumGameMs / Divisor, SumRenderMs / Divisor,
	    GpuSampleCount > 0 ? SumGpuMs / GpuDivisor : -1.0, PeakFrameMs, *Dropped, *Missed,
	    bMetaMetricsAvailable ? LatestCpuUtilization : -1.0, bMetaMetricsAvailable ? LatestGpuUtilization : -1.0,
	    BytesToMiB(Memory.UsedPhysical), BytesToMiB(Memory.PeakUsedPhysical), ThermalStress);
}

void ACityBenchmarkProbe::EmitSample(const bool bFinal)
{
	const FString Status = BuildStatus(bFinal);
	if (GEngine && bDrawStatus)
	{
		GEngine->AddOnScreenDebugMessage(9200, bFinal ? 30.0f : 0.0f, bFinal ? FColor::Green : FColor::Yellow, Status,
		                                 false);
	}
	UE_LOG(LogTemp, Display, TEXT("%s %s"),
	       bFinal ? TEXT("CITYSIM_BENCHMARK_COMPLETE") : TEXT("CITYSIM_BENCHMARK_SAMPLE"),
	       *Status.Replace(TEXT("\n"), TEXT(" | ")));
}

void ACityBenchmarkProbe::SaveReport() const
{
	if (ReportPath.IsEmpty())
		return;
	auto Report = MakeShared<FJsonObject>();
	const double Divisor = FMath::Max<uint64>(1, SampleCount);
	Report->SetStringField(TEXT("profile"),
	                       FParse::Param(FCommandLine::Get(), TEXT("CityDesktop")) ? TEXT("Desktop") : TEXT("XR"));
	Report->SetStringField(TEXT("scene"), SimulationSubsystem->GetBenchmarkStatus());
	Report->SetStringField(
	    TEXT("scope"),
	    SimulationSubsystem->GetBenchmarkResult().Config.bPopulated
	        ? TEXT(
	              "Generated operational connected neighbourhood with real residents/jobs/traffic; generous funds, no zoning growth; not natural progression or larger-map acceptance")
	    : SimulationSubsystem->GetBenchmarkResult().Config.SavedCityPath.IsEmpty()
	        ? TEXT("Synthetic paused rendering stress; not a populated service-loop or headset acceptance test")
	        : TEXT(
	              "Validated isolated saved-city simulation/rendering; controlled weather is declared; no headset acceptance"));
	const auto& Config = SimulationSubsystem->GetBenchmarkResult().Config;
	Report->SetNumberField(TEXT("gridWidth"), FCityGrid::Width);
	Report->SetNumberField(TEXT("gridHeight"), FCityGrid::Height);
	Report->SetBoolField(TEXT("generatedPopulated"), Config.bPopulated);
	Report->SetBoolField(TEXT("generatedLateGame"), Config.bLateGame);
	Report->SetNumberField(TEXT("waterRulesVersion"),
	                       SimulationSubsystem->GetTerrain().GetParameters().WaterRulesVersion);
	Report->SetBoolField(TEXT("experimentalGrid"), FCityGrid::Width != 64);
	int32 BuildingCells = 0;
	auto Types = MakeShared<FJsonObject>();
	for (const auto& Pair : SimulationSubsystem->GetPlacementModel().GetEntities())
	{
		BuildingCells += Pair.Value.FootprintSize.X * Pair.Value.FootprintSize.Y;
		const FString Type = Pair.Value.DefinitionId.ToString();
		double Count = 0;
		Types->TryGetNumberField(Type, Count);
		Types->SetNumberField(Type, Count + 1);
	}
	Report->SetObjectField(TEXT("buildingTypes"), Types);
	Report->SetNumberField(TEXT("buildingCells"), BuildingCells);
	Report->SetNumberField(TEXT("developedCellPercent"),
	                       (BuildingCells + SimulationSubsystem->GetPlacementModel().GetRoadCells().Num()) * 100.0 /
	                           FCityGrid::CellCount);
	const auto& Services = SimulationSubsystem->GetServiceSnapshot();
	Report->SetNumberField(TEXT("operationalHomesEnd"), Services.OperationalHomes);
	Report->SetNumberField(TEXT("operationalShopsEnd"), Services.OperationalShops);
	Report->SetNumberField(TEXT("powerDemandEnd"), Services.PowerDemand);
	Report->SetNumberField(TEXT("powerCapacityEnd"), Services.PowerCapacity);
	Report->SetNumberField(TEXT("waterDemandEnd"), Services.ServiceDemand);
	Report->SetNumberField(TEXT("waterCapacityEnd"), Services.ServiceCapacity);
	const auto& Spatial = SimulationSubsystem->GetPlacementModel().GetSpatialCacheStats();
	Report->SetNumberField(TEXT("spatialInvalidations"),
	                       Spatial.Invalidations - CaptureStartSpatialStats.Invalidations);
	Report->SetNumberField(TEXT("frontageBuilds"), Spatial.FrontageBuilds - CaptureStartSpatialStats.FrontageBuilds);
	Report->SetNumberField(TEXT("frontageHits"), Spatial.FrontageHits - CaptureStartSpatialStats.FrontageHits);
	Report->SetNumberField(TEXT("roadReachBuilds"), Spatial.ReachBuilds - CaptureStartSpatialStats.ReachBuilds);
	Report->SetNumberField(TEXT("roadReachHits"), Spatial.ReachHits - CaptureStartSpatialStats.ReachHits);
	Report->SetBoolField(TEXT("uncachedSpatial"),
	                     FParse::Param(FCommandLine::Get(), TEXT("CityBenchmarkUncachedSpatial")));
	Report->SetBoolField(TEXT("uncachedRouting"), !SimulationSubsystem->GetRoadNetworkSnapshot().bUsePreparedRouting);
	Report->SetBoolField(TEXT("referenceServices"), !Services.Water.bPrepareQueries);
	Report->SetStringField(
	    TEXT("fixtureInitialization"),
	    Config.bLateGame
	        ? TEXT(
	              "Mixed catalog lots, medium/high levels, fully upgraded utilities, current water rules and mature qualifications; completed opening deliveries; 60 real bootstrap steps. Generous treasury, no zoning growth/disasters; controlled late-game stress, not natural progression.")
	        : TEXT("Basic populated fixture uses 8 bootstrap steps; synthetic paused fixture uses none."));
	Report->SetNumberField(TEXT("roadCells"), SimulationSubsystem->GetPlacementModel().GetRoadCells().Num());
	Report->SetNumberField(TEXT("citizenRouteBudget"), Config.CitizenRouteBudget);
	Report->SetNumberField(TEXT("visibleTrafficLimit"), Config.VisibleTraffic);
	Report->SetNumberField(TEXT("trafficRecordsMean"), SumTrafficRecords / Divisor);
	Report->SetNumberField(TEXT("trafficRecordsPeak"), PeakTrafficRecords);
	Report->SetNumberField(TEXT("desktop30FpsBudgetMs"), 1000.0 / 30);
	Report->SetNumberField(TEXT("framesOver30BudgetPercent"), FramesOver30Budget * 100.0 / Divisor);
	Report->SetNumberField(TEXT("timeOver30BudgetPercent"), TimeOver30BudgetMs / FMath::Max(1.0, SumFrameMs) * 100);
	Report->SetStringField(TEXT("frameTimingSource"), TEXT("MonotonicWallClock"));
	Report->SetStringField(TEXT("weatherControl"), Config.Weather);
	Report->SetNumberField(TEXT("simulationSpeed"),
	                       SimulationSubsystem->IsSimulationPaused() ? 0 : SimulationSubsystem->GetSimulationSpeed());
	Report->SetNumberField(TEXT("captureStartMonth"), CaptureStartMonth);
	Report->SetNumberField(TEXT("captureEndMonth"), SimulationSubsystem->GetEconomySnapshot().LastBilledMonthId);
	Report->SetNumberField(TEXT("captureStartStep"), CaptureStartStep);
	int64 RequestedStartStep = 0;
	FParse::Value(FCommandLine::Get(), TEXT("CityBenchmarkCaptureStartStep="), RequestedStartStep);
	Report->SetNumberField(TEXT("requestedCaptureStartStep"), RequestedStartStep);
	Report->SetNumberField(TEXT("captureEndStep"), SimulationSubsystem->GetEconomySnapshot().FixedStepId);
	Report->SetNumberField(TEXT("lotsStart"), CaptureStartLots);
	Report->SetNumberField(TEXT("lotsEnd"), SimulationSubsystem->GetPlacementModel().GetEntities().Num());
	Report->SetNumberField(TEXT("citizensStart"), CaptureStartCitizens);
	Report->SetNumberField(TEXT("citizensEnd"), SimulationSubsystem->GetPopulationSnapshot().Citizens.Num());
	// Exact authoritative endpoint comparison is generated after sampling stops.
	// Wall-clock accumulator fractions differ between captures; fixed-step state
	// and exact int64 finances remain included in the normal save serialization.
	auto State = SimulationSubsystem->BuildSaveDocument();
	State.EconomyAccumulatorSeconds = 0;
	FString StateJson, StateError;
	if (FCityPersistence::Serialize(State, StateJson, StateError))
	{
		FTCHARToUTF8 Bytes(*StateJson);
		FSHAHash Hash;
		FSHA1::HashBuffer(Bytes.Get(), Bytes.Length(), Hash.Hash);
		Report->SetStringField(TEXT("authoritativeStateSha1"), Hash.ToString());
	}
	else
		UE_LOG(LogTemp, Error, TEXT("CITYSIM_BENCHMARK_REPORT_FAILED State=%s"), *StateError);
	Report->SetNumberField(TEXT("activeTransitTripsEnd"),
	                       SimulationSubsystem->GetPopulationSnapshot().TransitTrips.Num());
	Report->SetNumberField(TEXT("terrainSeed"), SimulationSubsystem->GetTerrain().GetParameters().Seed);
	Report->SetBoolField(TEXT("precipitationEnd"), SimulationSubsystem->GetEnvironmentSnapshot().bPrecipitation);
	if (const auto* Timings = SimulationSubsystem->GetBenchmarkTimings())
	{
		auto PhaseReport = [&](const TCHAR* Name, const FCityFrameTiming& Timing)
		{
			auto Phase = MakeShared<FJsonObject>();
			Phase->SetNumberField(TEXT("samples"), Timing.Count);
			Phase->SetNumberField(TEXT("meanMs"), Timing.SumMs / FMath::Max<uint64>(1, Timing.Count));
			Phase->SetNumberField(TEXT("peakMs"), Timing.PeakMs);
			Report->SetObjectField(Name, Phase);
		};
		PhaseReport(TEXT("derivedRefresh"), Timings->Derived);
		PhaseReport(TEXT("roadNetworkRefresh"), Timings->RoadNetwork);
		PhaseReport(TEXT("serviceRefresh"), Timings->Services);
		PhaseReport(TEXT("parkRefresh"), Timings->Parks);
		PhaseReport(TEXT("needsRefresh"), Timings->Needs);
		PhaseReport(TEXT("fixedStep"), Timings->FixedSteps);
		PhaseReport(TEXT("initialServicesAndScenario"), Timings->InitialServices);
		Report->SetBoolField(TEXT("referenceCare"), Config.bReferenceCare);
		Report->SetNumberField(TEXT("reusedInitialServices"), Timings->ReusedInitialServices);
		Report->SetNumberField(TEXT("evaluatedInitialServices"), Timings->EvaluatedInitialServices);
		Report->SetBoolField(TEXT("referenceServiceReuse"),
		                     Timings->EvaluatedInitialServices > 0 && Timings->ReusedInitialServices == 0);
		PhaseReport(TEXT("serviceProviders"), Timings->ServiceEvaluation.Providers);
		PhaseReport(TEXT("serviceWaterNetwork"), Timings->ServiceEvaluation.WaterNetwork);
		PhaseReport(TEXT("serviceConsumers"), Timings->ServiceEvaluation.Consumers);
		PhaseReport(TEXT("servicePublicStaff"), Timings->ServiceEvaluation.PublicStaff);
		PhaseReport(TEXT("serviceEducation"), Timings->ServiceEvaluation.Education);
		PhaseReport(TEXT("serviceParks"), Timings->ServiceEvaluation.Parks);
		PhaseReport(TEXT("needsMarket"), Timings->NeedsEvaluation.Market);
		PhaseReport(TEXT("forecastCopy"), Timings->NeedsEvaluation.Forecast.Copy);
		PhaseReport(TEXT("forecastReconcile"), Timings->NeedsEvaluation.Forecast.Reconcile);
		PhaseReport(TEXT("forecastSettlement"), Timings->NeedsEvaluation.Forecast.Settlement);
		PhaseReport(TEXT("forecastAggregate"), Timings->NeedsEvaluation.Forecast.Aggregate);
		PhaseReport(TEXT("forecastPublicPayroll"), Timings->NeedsEvaluation.Forecast.PublicPayroll);
		PhaseReport(TEXT("forecastPrivatePayroll"), Timings->NeedsEvaluation.Forecast.PrivatePayroll);
		PhaseReport(TEXT("forecastPurchases"), Timings->NeedsEvaluation.Forecast.Purchases);
		PhaseReport(TEXT("forecastHouseholdBalances"), Timings->NeedsEvaluation.Forecast.HouseholdBalances);
		PhaseReport(TEXT("forecastReconcileLedger"), Timings->NeedsEvaluation.Forecast.ReconcileLedger);
		PhaseReport(TEXT("forecastReconcileHiring"), Timings->NeedsEvaluation.Forecast.ReconcileHiring);
		PhaseReport(TEXT("forecastReconcileHousing"), Timings->NeedsEvaluation.Forecast.ReconcileHousing);
		PhaseReport(TEXT("economyServices"), Timings->EconomyEvaluation.Services);
		PhaseReport(TEXT("economyBilling"), Timings->EconomyEvaluation.Billing);
		PhaseReport(TEXT("economyForecastCopy"), Timings->EconomyEvaluation.Forecast.Copy);
		PhaseReport(TEXT("economyForecastReconcile"), Timings->EconomyEvaluation.Forecast.Reconcile);
		PhaseReport(TEXT("economyForecastSettlement"), Timings->EconomyEvaluation.Forecast.Settlement);
		PhaseReport(TEXT("economyForecastAggregate"), Timings->EconomyEvaluation.Forecast.Aggregate);
		Report->SetNumberField(TEXT("preparedPopulationForecasts"),
		                       Timings->NeedsEvaluation.Forecast.PreparedQueries +
		                           Timings->EconomyEvaluation.Forecast.PreparedQueries);
		Report->SetNumberField(TEXT("referencePopulationForecasts"),
		                       Timings->NeedsEvaluation.Forecast.ReferenceQueries +
		                           Timings->EconomyEvaluation.Forecast.ReferenceQueries);
		Report->SetBoolField(TEXT("referencePopulation"), Timings->NeedsEvaluation.Forecast.ReferenceQueries +
		                                                          Timings->EconomyEvaluation.Forecast.ReferenceQueries >
		                                                      0);
		PhaseReport(TEXT("SchedulePrune"), Timings->SchedulePrune);
		PhaseReport(TEXT("ScheduleHospital"), Timings->ScheduleHospital);
		PhaseReport(TEXT("ScheduleWaste"), Timings->ScheduleWaste);
		PhaseReport(TEXT("ScheduleEmergency"), Timings->ScheduleEmergency);
		PhaseReport(TEXT("ScheduleOpenings"), Timings->ScheduleOpenings);
		PhaseReport(TEXT("ScheduleFreight"), Timings->ScheduleFreight);
		PhaseReport(TEXT("ScheduleCitizens"), Timings->ScheduleCitizens);
		PhaseReport(TEXT("trafficSnapshotCopy"), Timings->TrafficSnapshotCopy);
		PhaseReport(TEXT("trafficCompletions"), Timings->TrafficCompletions);
		PhaseReport(TEXT("deliveryHolds"), Timings->DeliveryHolds);
		PhaseReport(TEXT("monthlyPopulationCheck"), Timings->MonthlyPopulationCheck);
		PhaseReport(TEXT("livePopulationReconcile"), Timings->LivePopulationEvaluation.Reconcile);
		PhaseReport(TEXT("livePopulationSettlement"), Timings->LivePopulationEvaluation.Settlement);
		PhaseReport(TEXT("livePopulationPublicPayroll"), Timings->LivePopulationEvaluation.PublicPayroll);
		PhaseReport(TEXT("livePopulationPrivatePayroll"), Timings->LivePopulationEvaluation.PrivatePayroll);
		PhaseReport(TEXT("livePopulationPurchases"), Timings->LivePopulationEvaluation.Purchases);
		PhaseReport(TEXT("livePopulationHouseholdBalances"), Timings->LivePopulationEvaluation.HouseholdBalances);
		Report->SetNumberField(TEXT("maximumStepsPerTick"), Timings->MaximumStepsPerTick);
		Report->SetNumberField(TEXT("catchUpTicks"), Timings->CatchUpTicks);
		PhaseReport(TEXT("needsZones"), Timings->NeedsEvaluation.Zones);
		PhaseReport(TEXT("needsDesirability"), Timings->NeedsEvaluation.Desirability);
		Report->SetNumberField(TEXT("preparedWaterQueries"), Timings->ServiceEvaluation.PreparedWaterQueries);
		Report->SetNumberField(TEXT("referenceWaterQueries"), Timings->ServiceEvaluation.ReferenceWaterQueries);
		Report->SetNumberField(TEXT("preparedEducationQueries"), Timings->ServiceEvaluation.PreparedEducationQueries);
		Report->SetNumberField(TEXT("referenceEducationCitizens"),
		                       Timings->ServiceEvaluation.ReferenceEducationCitizens);
		PhaseReport(TEXT("populationReconcileAndHospital"), Timings->PopulationReconcile);
		PhaseReport(TEXT("schedulingAndTraffic"), Timings->SchedulingTraffic);
		PhaseReport(TEXT("trafficUpdate"), Timings->Routing.TrafficUpdate);
		PhaseReport(TEXT("trafficAdvance"), Timings->Routing.TrafficAdvance);
		PhaseReport(TEXT("routePreparation"), Timings->Routing.Preparation);
		PhaseReport(TEXT("routeSearch"), Timings->Routing.Search);
		PhaseReport(TEXT("routeConversion"), Timings->Routing.Conversion);
		Report->SetNumberField(TEXT("preparedRouteSearches"), Timings->Routing.PreparedSearches);
		Report->SetNumberField(TEXT("referenceRouteBuilds"), Timings->Routing.ReferenceBuilds);
		Report->SetNumberField(TEXT("indexedRouteSegments"), Timings->Routing.IndexedSegments);
		PhaseReport(TEXT("environmentAdvance"), Timings->EnvironmentAdvance);
		PhaseReport(TEXT("developmentAndGrowth"), Timings->DevelopmentGrowth);
		PhaseReport(TEXT("incidentsAndEducation"), Timings->Incidents);
		Report->SetStringField(
		    TEXT("phaseTimingScope"),
		    TEXT(
		        "Per-call inclusive time; fixedStep includes all step phases and derived calls; derived includes network/services/parks/needs; development may include a derived refresh. Month timing includes the entire settling tick, not only billing. Do not sum nested means."));
		Report->SetNumberField(TEXT("simulationTickSamples"), Timings->Ticks.Count);
		Report->SetNumberField(TEXT("simulationTickMeanMs"),
		                       Timings->Ticks.SumMs / FMath::Max<uint64>(1, Timings->Ticks.Count));
		Report->SetNumberField(TEXT("simulationTickP95Ms"), Timings->Ticks.Percentile(95));
		Report->SetNumberField(TEXT("simulationTickPeakMs"), Timings->Ticks.PeakMs);
		Report->SetNumberField(TEXT("monthlyTickSamples"), Timings->MonthlyTicks.Count);
		if (Timings->MonthlyTicks.Count)
		{
			Report->SetNumberField(TEXT("monthlyTickMeanMs"),
			                       Timings->MonthlyTicks.SumMs / Timings->MonthlyTicks.Count);
			Report->SetNumberField(TEXT("monthlyTickPeakMs"), Timings->MonthlyTicks.PeakMs);
		}
		else
		{
			Report->SetField(TEXT("monthlyTickMeanMs"), MakeShared<FJsonValueNull>());
			Report->SetField(TEXT("monthlyTickPeakMs"), MakeShared<FJsonValueNull>());
		}
		Report->SetStringField(TEXT("monthlyTickScope"),
		                       TEXT("Whole simulation tick containing settlement, not settlement-only CPU time"));
	}
	Report->SetNumberField(TEXT("rhiDrawCallsMean"), SumDrawCalls / Divisor);
	Report->SetNumberField(TEXT("rhiDrawCallsPeak"), PeakDrawCalls);
	Report->SetNumberField(TEXT("rhiPrimitivesMean"), SumPrimitives / Divisor);
	Report->SetStringField(
	    TEXT("rhiCounterScope"),
	    TEXT("RHI GPU0 last-frame counters sampled on game thread; includes all rendered passes/UI"));
	Report->SetStringField(TEXT("build"), LexToString(FApp::GetBuildConfiguration()));
	Report->SetStringField(TEXT("cpu"), FPlatformMisc::GetCPUBrand());
	Report->SetStringField(TEXT("gpu"), FPlatformMisc::GetPrimaryGPUBrand());
	Report->SetStringField(TEXT("rhi"), GDynamicRHI ? GDynamicRHI->GetName() : TEXT("Unavailable"));
	Report->SetNumberField(TEXT("captureSeconds"), CaptureSeconds);
	Report->SetNumberField(TEXT("frames"), SampleCount);
	Report->SetNumberField(TEXT("frameMeanMs"), SumFrameMs / Divisor);
	Report->SetNumberField(TEXT("frameP50Ms"), FrameTiming.Percentile(50));
	Report->SetNumberField(TEXT("frameP95Ms"), FrameTiming.Percentile(95));
	Report->SetNumberField(TEXT("frameP99Ms"), FrameTiming.Percentile(99));
	Report->SetNumberField(TEXT("percentileResolutionMs"), FCityFrameTiming::ResolutionMs);
	Report->SetNumberField(TEXT("framePeakMs"), PeakFrameMs);
	Report->SetNumberField(TEXT("gameMeanMs"), SumGameMs / Divisor);
	Report->SetNumberField(TEXT("renderMeanMs"), SumRenderMs / Divisor);
	if (GpuSampleCount)
		Report->SetNumberField(TEXT("gpuMeanMs"), SumGpuMs / GpuSampleCount);
	else
		Report->SetField(TEXT("gpuMeanMs"), MakeShared<FJsonValueNull>());
	Report->SetStringField(TEXT("gpuTimingSource"), bMetaMetricsAvailable ? TEXT("Meta")
	                                                : GpuSampleCount      ? TEXT("RHI frame cycles")
	                                                                      : TEXT("Unavailable"));
	Report->SetNumberField(TEXT("gpuSamples"), GpuSampleCount);
	if (TargetRefreshHz > 0)
	{
		Report->SetNumberField(TEXT("targetHz"), TargetRefreshHz);
		Report->SetNumberField(TEXT("estimatedMissedFrames"), EstimatedMissedFrames);
	}
	else
	{
		Report->SetField(TEXT("targetHz"), MakeShared<FJsonValueNull>());
		Report->SetField(TEXT("estimatedMissedFrames"), MakeShared<FJsonValueNull>());
	}
	if (bMetaMetricsAvailable)
		Report->SetNumberField(TEXT("metaDroppedFrames"),
		                       FMath::Max(0, CompositorDroppedFrames - CompositorDroppedFramesAtStart));
	else
		Report->SetField(TEXT("metaDroppedFrames"), MakeShared<FJsonValueNull>());
	const auto Memory = FPlatformMemory::GetStats();
	Report->SetNumberField(TEXT("memoryMiB"), BytesToMiB(Memory.UsedPhysical));
	Report->SetNumberField(TEXT("peakMemoryMiB"), BytesToMiB(Memory.PeakUsedPhysical));
	Report->SetBoolField(TEXT("simulationPaused"), SimulationSubsystem->IsSimulationPaused());
	Report->SetNumberField(TEXT("hudPresentationMeanMs"),
	                       DesktopHud.IsValid() ? DesktopHud->GetAveragePresentationMs() : 0);
	uint64 Instances = 0;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->IsHidden())
			continue;
		TInlineComponentArray<UInstancedStaticMeshComponent*> Components(*It);
		for (const auto* Component : Components)
			if (Component->IsVisible())
				Instances += Component->GetInstanceCount();
	}
	Report->SetNumberField(TEXT("visibleInstanceCandidates"), Instances);
	Report->SetStringField(TEXT("instanceScope"),
	                       TEXT("Visible components before frustum/occlusion culling; not draw calls"));
	if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport)
	{
		const auto Size = GEngine->GameViewport->Viewport->GetSizeXY();
		Report->SetNumberField(TEXT("width"), Size.X);
		Report->SetNumberField(TEXT("height"), Size.Y);
	}
	for (const TCHAR* Name :
	     {TEXT("r.HDR.EnableHDROutput"), TEXT("r.HDR.Display.OutputDevice"), TEXT("r.DefaultFeature.AutoExposure.Bias"),
	      TEXT("r.TonemapperGamma"), TEXT("r.BloomQuality"), TEXT("r.VSync"), TEXT("t.MaxFPS"),
	      TEXT("sg.ShadowQuality"), TEXT("r.ShadowQuality"), TEXT("r.Shadow.MaxCSMResolution"),
	      TEXT("r.Shadow.CSM.MaxCascades"), TEXT("r.Shadow.DistanceScale"), TEXT("r.ScreenPercentage"),
	      TEXT("r.MSAACount"), TEXT("r.AntiAliasingMethod"), TEXT("r.SecondaryScreenPercentage.GameViewport"),
	      TEXT("EnableHighDPIAwareness")})
		if (const auto* CVar = IConsoleManager::Get().FindConsoleVariable(Name))
			Report->SetNumberField(Name, CVar->GetFloat());
	for (TActorIterator<ACitySandboxEnvironment> It(GetWorld()); It; ++It)
	{
		if (const auto* Sun = It->FindComponentByClass<UDirectionalLightComponent>())
		{
			Report->SetNumberField(TEXT("sunShadowDistanceCm"), Sun->DynamicShadowDistanceMovableLight);
			Report->SetNumberField(TEXT("sunShadowCascades"), Sun->DynamicShadowCascades);
			Report->SetNumberField(TEXT("sunShadowResolutionScale"), Sun->ShadowResolutionScale);
			Report->SetNumberField(TEXT("sunShadowBias"), Sun->ShadowBias);
			Report->SetNumberField(TEXT("sunShadowSlopeBias"), Sun->ShadowSlopeBias);
			Report->SetNumberField(TEXT("sunShadowSharpen"), Sun->ShadowSharpen);
			Report->SetNumberField(TEXT("sunCascadeDistributionExponent"), Sun->CascadeDistributionExponent);
		}
		if (const auto* Color = It->FindComponentByClass<UPostProcessComponent>())
		{
			Report->SetBoolField(TEXT("windowsNeutralColor"), Color->bEnabled);
			Report->SetNumberField(TEXT("neutralExposureBias"), Color->Settings.AutoExposureBias);
			Report->SetNumberField(TEXT("neutralToneCurve"), Color->Settings.ToneCurveAmount);
			Report->SetNumberField(TEXT("neutralBloom"), Color->Settings.BloomIntensity);
		}
	}
	FString Json;
	FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReportPath), true);
	if (!FFileHelper::SaveStringToFile(Json, *ReportPath))
		UE_LOG(LogTemp, Error, TEXT("CITYSIM_BENCHMARK_REPORT_FAILED %s"), *ReportPath);
}
