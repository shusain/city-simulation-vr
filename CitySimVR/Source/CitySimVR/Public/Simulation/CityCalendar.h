#pragma once
#include "CoreMinimal.h"

// One authoritative fine clock drives traffic, daily budgets, the calendar and light.
struct CITYSIMVR_API FCityCalendar
{
    static constexpr int32 DefaultStepsPerDay = 150;
    static constexpr int32 DaysPerMonth = 25;
    static constexpr int32 MonthsPerYear = 12;
    static int64 DayIndex(int64 Step, int32 StepsPerDay = DefaultStepsPerDay)
    { return FMath::Max<int64>(0, Step) / FMath::Max(1, StepsPerDay); }
    static int64 MonthIndex(int64 Step, int32 StepsPerDay = DefaultStepsPerDay)
    { return DayIndex(Step, StepsPerDay) / DaysPerMonth; }
    static double DayFraction(int64 Step, double Progress, int32 StepsPerDay)
    { return (Step % FMath::Max(1, StepsPerDay) + FMath::Clamp(Progress, 0.0, 1.0)) / FMath::Max(1, StepsPerDay); }
    static int64 HalfDayIndex(int64 Step, double Progress, int32 StepsPerDay)
    {
        return DayIndex(Step, StepsPerDay) * 2 + (DayFraction(Step, Progress, StepsPerDay) + 1.e-8 >= 0.5 ? 1 : 0);
    }
    static int64 HalfDayEndStep(int64 HalfDay, int32 StepsPerDay = DefaultStepsPerDay)
    { return (HalfDay * FMath::Max(1, StepsPerDay) + 1) / 2; }
    static int32 Duration(double Hours, int32 StepsPerDay = DefaultStepsPerDay)
    { return FMath::Max(1, FMath::CeilToInt(Hours * FMath::Max(1, StepsPerDay) / 24.0)); }
    static int64 NextMorning(int64 Step, uint64 CitizenId, int32 StepsPerDay)
    {
        const int64 Day = DayIndex(Step, StepsPerDay);
        const int64 Morning = Day * StepsPerDay + Duration(6.0, StepsPerDay) +
            CitizenId % FMath::Max(1, Duration(1.0, StepsPerDay));
        return Morning > Step ? Morning : Morning + StepsPerDay;
    }
};
