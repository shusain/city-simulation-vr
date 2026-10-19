#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Performance/CityFrameTiming.h"
#include "Simulation/CitySpatialCache.h"
#include "CityBenchmarkProbe.generated.h"

class UCitySimulationSubsystem;

UCLASS()
class CITYSIMVR_API ACityBenchmarkProbe : public AActor
{
	GENERATED_BODY()

public:
	ACityBenchmarkProbe();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	void ConfigureScene();
	void BeginCapture();
	void EmitSample(bool bFinal);
	FString BuildStatus(bool bFinal) const;
	void SaveReport() const;

	UPROPERTY(Transient)
	TObjectPtr<UCitySimulationSubsystem> SimulationSubsystem;

	double SinceLaunchSeconds = 0.0;
	double LastFrameClock = 0.0;
	double CaptureSeconds = 0.0;
	double SinceSampleSeconds = 0.0;
	double WarmupSeconds = 10.0;
	double DurationSeconds = 1200.0;
	double SampleIntervalSeconds = 10.0;
	double TargetRefreshHz = 0.0;
	FCityFrameTiming FrameTiming;
	bool bMetaMetricsAvailable = false;
	bool bExitAfterCapture = false;
	bool bDrawStatus = false;
	bool bSceneConfigured = false;
	FString ReportPath;
	TWeakObjectPtr<class UCityDesktopHud> DesktopHud;
	double SumFrameMs = 0.0;
	double SumGameMs = 0.0;
	double SumRenderMs = 0.0;
	double SumGpuMs = 0.0;
	double LatestGpuUtilization = 0.0;
	double LatestCpuUtilization = 0.0;
	double PeakFrameMs = 0.0;
	uint64 SampleCount = 0;
	uint64 FramesOver30Budget = 0;
	double TimeOver30BudgetMs = 0;
	double SumTrafficRecords = 0;
	int32 PeakTrafficRecords = 0;
	uint64 GpuSampleCount = 0;
	double SumDrawCalls = 0, SumPrimitives = 0;
	int32 PeakDrawCalls = 0;
	int64 CaptureStartMonth = 0, CaptureStartStep = 0;
	int32 CaptureStartLots = 0, CaptureStartCitizens = 0;
	FCitySpatialCacheStats CaptureStartSpatialStats;
	uint64 EstimatedMissedFrames = 0;
	int32 CompositorDroppedFramesAtStart = 0;
	int32 CompositorDroppedFrames = 0;
	bool bCaptureStarted = false;
	bool bCaptureComplete = false;
};
