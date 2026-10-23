#pragma once
#include "Simulation/CitySimulationSubsystem.h"
#include "Presentation/CityUiContent.h"

// Automation-only fixture: use the real clock/settlement hooks, never manufactured report rows.
namespace CitySmokeReports
{
inline bool Seed(UCitySimulationSubsystem& Sim, FString& Error)
{
    Error.Reset();
    const auto& Tuning = Sim.GetEconomyTuning();
    const int64 Start = Sim.GetEconomySnapshot().FixedStepId;
    const int64 Steps = int64(Tuning.FixedStepsPerMonth) * FCityCalendar::DaysPerMonth * 3;
    if (Steps <= 0 || Steps > 1000000 || Start > MAX_int64 - Steps)
    { Error = TEXT("Invalid smoke report calendar budget"); return false; }
    const int32 Speed = Sim.GetSimulationSpeed();
    const bool Paused = Sim.IsSimulationPaused();
    Sim.SetSimulationSpeed(1); Sim.SetSimulationPaused(false);
    const int64 Target = Start + Steps;
    for (int64 Attempt = 0; Attempt <= Steps && Sim.GetEconomySnapshot().FixedStepId < Target; ++Attempt)
    {
        const int64 Before = Sim.GetEconomySnapshot().FixedStepId;
        Sim.Tick(static_cast<float>(Tuning.FixedStepSeconds) + .0001f);
        if (Sim.GetEconomySnapshot().FixedStepId <= Before)
        { Error = TEXT("Smoke report clock did not advance; fixture may be suspended"); break; }
    }
    Sim.SetSimulationPaused(Paused); Sim.SetSimulationSpeed(Speed);
    const auto Months = Sim.GetReportHistory().Monthly();
    if (!Error.IsEmpty()) return false;
    if (Sim.GetEconomySnapshot().FixedStepId < Target || Months.Num() < 3)
    { Error = TEXT("Smoke requires at least three genuine settled monthly observations"); return false; }
    return true;
}
inline bool SelectedPeriod(const FCityUiReportChart& Chart, int64& Period)
{
    if (!Chart.Periods.IsValidIndex(Chart.Selected) || !Chart.Values.IsValidIndex(Chart.Selected) ||
        !Chart.Bars.IsValidIndex(Chart.Selected)) return false;
    Period = Chart.Periods[Chart.Selected]; return true;
}
}
