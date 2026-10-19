#include "Simulation/CityCommercialAi.h"
#include "Simulation/CityCalendar.h"
#include "Simulation/CityPopulationSimulation.h"
#include "Simulation/CityGrowthSimulation.h"

int64 FCityCommercialAi::BillingPeriod(const int64 Day)
{
    return Day > 0 ? Day / FCityCalendar::DaysPerMonth + 1 : 0;
}

TArray<uint64> FCityCommercialAi::Eligible(const FCityPopulationSnapshot& P, const FCityServiceSnapshot& S)
{
    TArray<uint64> Ids;
    if (!P.bCommercialAiPilot || S.OperatingFacilities.FindRef(TEXT("AIInstitute")) <= 0) return Ids;
    const int32 Capacity = S.OperatingAiCenters.Num() * ShopsPerCenter;
    for (const auto& B : P.Businesses)
        if (!B.bIndustrial && B.bInitialFreightReceived && (S.CommercialAiShops.Contains(B.EntityId) || (S.EntityJobs.FindRef(B.EntityId) >= 2 &&
             S.EntityJobs.FindRef(B.EntityId) > S.EntitySkilledJobs.FindRef(B.EntityId))) &&
            S.GetFailure(B.EntityId) == ECityOperationFailure::None)
            Ids.Add(B.EntityId);
    Ids.Sort();
    if (Ids.Num() > Capacity) Ids.SetNum(Capacity);
    return Ids;
}

void FCityCommercialAi::Apply(const FCityPopulationSnapshot* P, FCityServiceSnapshot& S)
{
    if (!P) return;
    const auto Selected = Eligible(*P, S);
    for (int32 Index = 0; Index < Selected.Num(); ++Index)
    {
        const uint64 Id = Selected[Index];
        S.CommercialAiProviders.Add(Id, S.OperatingAiCenters[Index / ShopsPerCenter]);
        const auto* B = P->Businesses.FindByPredicate([Id](const auto& V) { return V.EntityId == Id; });
        if (!B || B->AiFeePaid != MonthlyFee || B->AiPaidMonth != BillingPeriod(P->LastSettledMonthId) ||
            B->AiPaidMonth <= 0) continue;
        S.CommercialAiShops.Add(Id);
        --S.EntityJobs.FindChecked(Id);
        --S.Jobs;
        --S.CommercialJobs;
        S.EntitySkilledJobs.Add(Id, FMath::Clamp(S.EntitySkilledJobs.FindRef(Id), 1, S.EntityJobs.FindRef(Id)));
        S.EntitySkilledMinimumEducation.Add(Id, 3);
        const bool Qualified = P->Citizens.ContainsByPredicate([Id](const auto& C) {
            return C.WorkplaceEntityId == Id && C.bSkilledJob && C.Education == ECityEducationLevel::Higher &&
                   !C.IsDetained() && !C.bOnMedicalLeave;
        });
        if (Qualified && B->PayrollDue > 0 && B->PayrollPaid == B->PayrollDue &&
            P->ActiveWorkers(Id) * 2 >= S.EntityJobs.FindRef(Id))
        {
            S.CommercialAiStaffedShops.Add(Id);
            const int32 Extra = S.EntityOutput.FindRef(Id) / 4;
            S.EntityOutput.FindChecked(Id) += Extra;
            S.CommercialOutput += Extra;
        }
    }
}

void FCityCommercialAi::Settle(FCityPopulationSnapshot& P, const FCityServiceSnapshot* S, const int64 Day)
{
    if (!S) return;
    const auto Selected = Eligible(P, *S);
    const int64 Period = BillingPeriod(Day);
    for (int32 Index = 0; Index < Selected.Num(); ++Index)
    {
        const uint64 Id = Selected[Index];
        auto* B = P.Businesses.FindByPredicate([Id](const auto& V) { return V.EntityId == Id; });
        // Payroll is already settled. One whole fee per calendar period; no arrears/subsidy/refund.
        if (B && B->AiPaidMonth != Period && Period > 0 && B->PayrollDue > 0 &&
            B->PayrollPaid == B->PayrollDue && B->Balance >= MonthlyFee)
        {
            B->Balance -= MonthlyFee;
            B->AiFeePaid = MonthlyFee;
            B->AiPaidMonth = Period;
            B->AiCenterAtPayment = S->OperatingAiCenters[Index / ShopsPerCenter];
            B->AiJobsAtPayment = S->EntityJobs.FindRef(Id) + (S->CommercialAiShops.Contains(Id) ? 1 : 0);
            B->AiSpecialistsAtPayment = S->EntitySkilledJobs.FindRef(Id);
        }
    }
}

FString FCityCommercialAi::Status(const FCityPopulationSnapshot& P, const FCityServiceSnapshot& S, const uint64 Id)
{
    const auto* B = P.Businesses.FindByPredicate([Id](const auto& V) { return V.EntityId == Id; });
    if (!B || B->bIndustrial) return TEXT("Not a commercial workplace");
    if (!P.bCommercialAiPilot) return TEXT("Paused - ordinary roles/capacity");
    if (S.OperatingFacilities.FindRef(TEXT("AIInstitute")) <= 0 || S.OperatingAiCenters.IsEmpty())
        return TEXT("Provider offline - restore institute/enterprise services");
    if (S.GetFailure(Id) != ECityOperationFailure::None || !B->bInitialFreightReceived)
        return TEXT("Shop unavailable - restore service/opening freight");
    if (!S.CommercialAiProviders.Contains(Id)) return TEXT("Outside pilot slots - ordinary operation");
    if (!S.CommercialAiShops.Contains(Id))
        return B->PayrollDue > B->PayrollPaid ? TEXT("Awaiting full payroll before software payment")
             : B->Balance < MonthlyFee ? TEXT("Awaiting payment - shop needs $5 after payroll")
             : TEXT("Awaiting next daily settlement payment");
    const bool Qualified = P.Citizens.ContainsByPredicate([Id](const auto& C) {
        return C.WorkplaceEntityId == Id && C.bSkilledJob && C.Education == ECityEducationLevel::Higher &&
               !C.IsDetained() && !C.bOnMedicalLeave;
    });
    if (!Qualified || P.ActiveWorkers(Id) * 2 < S.EntityJobs.FindRef(Id))
        return TEXT("Paid pilot - insufficient Higher/active staff; no capacity bonus");
    if (B->PayrollDue <= 0 || B->PayrollPaid != B->PayrollDue)
        return TEXT("Paid pilot - payroll shortage; no capacity bonus");
    return TEXT("Paid pilot active - staffed retail capacity +25%");
}

FString FCityCommercialAi::Review(const FCityPopulationSnapshot& P, const FCityServiceSnapshot& S)
{
    FString Text = FString::Printf(TEXT("Commercial pilot: %s | paid %d / slots %d\n"
        "Operating institute %d | enterprise centers %d\n"
        "Each center serves 2 eligible shops, stable ID order.\n"
        "$5/shop/calendar month to external software provider.\n"
        "Charged after full payroll; first eligible daily settlement.\n"
        "One fewer vacancy; specialist roles require Higher.\n"
        "+25%% retail capacity requires qualified paid staff.\n"
        "Stock, freight and buyers still required.\n"
        "Pause/outage restores roles; no prepaid refund.\n"),
        P.bCommercialAiPilot ? TEXT("ON") : TEXT("OFF"), S.CommercialAiShops.Num(),
        S.OperatingAiCenters.Num() * ShopsPerCenter, S.OperatingFacilities.FindRef(TEXT("AIInstitute")), S.OperatingAiCenters.Num());
    int32 Count = 0;
    for (const auto& B : P.Businesses)
    {
        if (B.bIndustrial || ++Count > 3) continue;
        Text += FString::Printf(TEXT("Shop #%llu: %s\n"), B.EntityId, *Status(P, S, B.EntityId));
    }
    return Text;
}
