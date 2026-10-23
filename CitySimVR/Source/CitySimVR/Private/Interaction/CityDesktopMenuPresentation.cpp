#include "Interaction/CityVRToolController.h"
#include "Engine/World.h"
#include "Commands/CityPlacementModel.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Simulation/CityCommercialAi.h"
#include "Simulation/CityHousingHistory.h"
#include "Simulation/CityHappinessSimulation.h"
#include "Simulation/CityHousingMigration.h"
#include "Simulation/CityTechnology.h"

namespace
{
FString Money(int64 Value) { return TEXT("$") + FText::AsNumber(Value).ToString(); }
FCityUiGauge Row(const FString& Label, const FString& Value)
{
    FCityUiGauge R; R.Label = FText::FromString(Label); R.Value = FText::FromString(Value); return R;
}
}

void ACityVRToolController::PopulateDesktopMenu(FCityUiSnapshot& Data) const
{
    PopulateDesktopSettings(Data);
    PopulateDesktopEntry(Data);
    // Order the desktop projection, never the authoritative category/metric IDs or VR target slots.
    const TArray<ECityPaletteAction> CityOrder = {
        ECityPaletteAction::OpenTaxes, ECityPaletteAction::OpenLoans, ECityPaletteAction::OpenReports,
        ECityPaletteAction::OpenQuarterlyNews, ECityPaletteAction::OverlayHappiness, ECityPaletteAction::OpenHousingHistory,
        ECityPaletteAction::OverlayCrime, ECityPaletteAction::OverlayFire, ECityPaletteAction::OverlayParks,
        ECityPaletteAction::OverlayEducation, ECityPaletteAction::OverlayHealth, ECityPaletteAction::OverlayWaste,
        ECityPaletteAction::OpenPolicies, ECityPaletteAction::OpenTechnology};
    const TArray<ECityPaletteAction> ReportOrder = {
        ECityPaletteAction::ReportHourly, ECityPaletteAction::ReportDaily, ECityPaletteAction::ReportWeekly, ECityPaletteAction::ReportMonthly,
        ECityPaletteAction::ReportUtilities, ECityPaletteAction::ReportPeople, ECityPaletteAction::ReportStaff,
        ECityPaletteAction::ReportGoods, ECityPaletteAction::ReportMoney, ECityPaletteAction::ReportHealth, ECityPaletteAction::ReportWaste,
        ECityPaletteAction::ReportMetricPrevious, ECityPaletteAction::ReportMetricNext,
        ECityPaletteAction::ReportPeriodPrevious, ECityPaletteAction::ReportPeriodNext, ECityPaletteAction::Back};
    if (PalettePage == ECityPalettePage::City || PalettePage == ECityPalettePage::Reports)
    {
        const auto& Order = PalettePage == ECityPalettePage::City ? CityOrder : ReportOrder;
        Data.MenuEntries.StableSort([&](const FCityUiActionView& A, const FCityUiActionView& B) {
            const int32 IA = Order.IndexOfByKey(static_cast<ECityPaletteAction>(A.ActionId));
            const int32 IB = Order.IndexOfByKey(static_cast<ECityPaletteAction>(B.ActionId));
            return (IA == INDEX_NONE ? MAX_int32 : IA) < (IB == INDEX_NONE ? MAX_int32 : IB);
        });
        for (int32 I = 0; I < Data.MenuEntries.Num(); ++I)
        {
            auto& E = Data.MenuEntries[I];
            E.bStartNewRow = PalettePage == ECityPalettePage::Reports && (I == 4 || I == 11 || I == 15);
            if (PalettePage == ECityPalettePage::City && (I == 0 || I == 3 || I == 6 || I == 12))
                E.Group = FText::FromString(I == 0 ? TEXT("CITY BUDGET") : I == 3 ? TEXT("CITY LIFE") :
                    I == 6 ? TEXT("GROWING SERVICES") : TEXT("ADVANCED PROGRAMS"));
        }
        if (PalettePage == ECityPalettePage::City)
            Data.MenuBody = FText::GetEmpty();
    }
    if (PalettePage != ECityPalettePage::Loans && PalettePage != ECityPalettePage::LoanReview &&
        PalettePage != ECityPalettePage::Policies && PalettePage != ECityPalettePage::PolicyReview &&
        PalettePage != ECityPalettePage::TechnologyReview && PalettePage != ECityPalettePage::CommercialAi &&
        PalettePage != ECityPalettePage::HousingHistory) return;
    const auto* Sim = GetWorld()->GetSubsystem<UCitySimulationSubsystem>();
    if (!Sim) return;
    const auto& P = Sim->GetPopulationSnapshot();
    const auto& S = Sim->GetServiceSnapshot();
    const auto& E = Sim->GetEconomySnapshot();
    const auto Budget = Sim->GetProjectedBudget();
    const int64 Cash = Sim->GetPlacementModel().GetFunds();
    auto Metric = [&](const FString& Label, const FString& Value) { Data.MenuMetrics.Add(Row(Label, Value)); };
    auto Section = [&](const FString& Title, const FString& Body, bool Collapse = false, bool Warning = false) -> FCityUiMenuSection& {
        auto& R = Data.MenuSections.AddDefaulted_GetRef(); R.Id = Title; R.Title = FText::FromString(Title);
        R.Body = FText::FromString(Body); R.bCollapsible = Collapse; R.bWarning = Warning; return R;
    };
    auto Message = [&](const FString& Text) { if (!Text.IsEmpty()) Section(TEXT("Status"), Text, false, true); };
    if (PalettePage == ECityPalettePage::Loans)
    {
        const auto& L = E.Loan;
        Data.MenuStatus = FText::FromString(L.Payoff() ? FString::Printf(TEXT("ACTIVE LOAN | %d / 24 scheduled days left"), L.MonthsRemaining) : TEXT("NO ACTIVE LOAN"));
        Metric(TEXT("City treasury"), Money(Cash)); Metric(TEXT("Next settlement due"), Money(L.NextPayment()));
        auto& B = Section(TEXT("Balance"), TEXT("External civic lender"));
        B.Rows = {Row(TEXT("Principal"), Money(L.Principal)), Row(TEXT("Interest owed"), Money(L.InterestDue)), Row(TEXT("Full payoff now"), Money(L.Payoff()))};
        if (L.Arrears()) Section(TEXT("UNPAID DUES"), Money(L.Arrears()) + TEXT(" still owed. Interest continues on unpaid principal."), false, true);
        auto& F = Section(TEXT("Daily forecast"), TEXT(""));
        F.Rows = {Row(TEXT("Operating"), Money(Budget.Net() + Budget.DebtService)), Row(TEXT("After debt"), Money(Budget.Net() + Budget.DebtService - L.NextPayment()))};
        auto& Last = Section(TEXT("Last settlement"), TEXT(""), true);
        Last.Rows = {Row(TEXT("Borrowed"), Money(L.LastBorrowed)), Row(TEXT("Principal paid"), Money(L.LastPrincipalPaid)), Row(TEXT("Interest paid"), Money(L.LastInterestPaid))};
        Section(TEXT("Borrowing creates debt"), TEXT("It is not tax income.")); Message(LoanMessage);
    }
    else if (PalettePage == ECityPalettePage::LoanReview)
    {
        const auto& L = E.Loan;
        if (bLoanPayoffReview)
        {
            Data.MenuStatus = FText::FromString(TEXT("PAY OFF CURRENT LOAN"));
            Metric(TEXT("Full payoff now"), Money(L.Payoff())); Metric(TEXT("Cash after payoff"), Money(Cash - L.Payoff()));
            auto& R = Section(TEXT("Treasury to external civic lender"), TEXT(""));
            R.Rows = {Row(TEXT("Principal"), Money(L.Principal)), Row(TEXT("Interest owed"), Money(L.InterestDue)), Row(TEXT("City cash now"), Money(Cash))};
            if (Cash < L.Payoff()) Section(TEXT("INSUFFICIENT CITY CASH"), TEXT("Need ") + Money(L.Payoff() - Cash) + TEXT(" more to pay off this balance."), false, true);
            Section(TEXT("No early payoff fee"), TEXT("No future interest after payoff. Confirm pays the displayed current balance."));
        }
        else
        {
            Data.MenuStatus = FText::FromString(TEXT("BORROW ") + Money(PendingLoanAmount));
            const int64 First = PendingLoanAmount - PendingLoanAmount * 23 / 24 + FCityLoanState::MonthlyInterest(PendingLoanAmount);
            int64 Interest = 0;
            for (int32 Remaining = 24; Remaining > 0; --Remaining)
                Interest += FCityLoanState::MonthlyInterest(PendingLoanAmount * Remaining / 24);
            Metric(TEXT("Cash after borrowing"), Money(Cash + PendingLoanAmount)); Metric(TEXT("First daily payment"), Money(First));
            Section(TEXT("Repayment"), TEXT("24 daily principal installments.\n0.5% / day on remaining principal; payments decline."));
            auto& R = Section(TEXT("On-time cost"), TEXT(""));
            R.Rows = {Row(TEXT("Total repayment"), Money(PendingLoanAmount + Interest)), Row(TEXT("Included interest"), Money(Interest)), Row(TEXT("Daily forecast after first payment"), Money(Budget.Net() - First))};
            Section(TEXT("Missed dues stay owed"), TEXT("Interest continues on unpaid principal. Operating bills are paid first."), false, true);
            Section(TEXT("Payment details"), TEXT("External civic lender transfers the borrowed amount to the city. Interest rounds up to whole dollars. One loan at a time. Early payoff has no fee."), true);
        }
        Message(LoanMessage);
    }
    else if (PalettePage == ECityPalettePage::Policies)
    {
        Metric(TEXT("City cash"), Money(Cash)); Metric(TEXT("Forecast policies / day"), Money(Budget.PolicyCost));
        for (int32 I = 0; I < 2; ++I)
        {
            const auto& R = E.Policies.Programs[I];
            auto& B = Section(FCityPolicyState::Name(static_cast<ECityPolicy>(I)), R.bEnabled ? TEXT("ENABLED") : TEXT("OFF"));
            if (E.Policies.LastSettledMonth)
                B.Rows = {Row(TEXT("Last day paid / eligible"), FString::Printf(TEXT("%lld / %lld"), R.Recipients, R.Eligible)), Row(TEXT("Grants / administration paid"), Money(R.GrantsPaid) + TEXT(" / ") + Money(R.AdministrationPaid))};
            if (R.Due() && !R.AdministrationPaid) Section(TEXT("UNFUNDED LAST DAY"), TEXT("No benefit paid for this program."), false, true);
        }
        if (!E.Policies.LastSettledMonth) Section(TEXT("Last settlement"), TEXT("No policy settlement recorded yet."));
        Section(TEXT("Funding order"), TEXT("Bills and lender first. Reserve assistance before bursaries. Changes apply at the next daily settlement."));
        Section(TEXT("Program details"), TEXT("No eligible recipients: no administration charge. Each program is funded in full or skipped without arrears."), true); Message(PolicyMessage);
    }
    else if (PalettePage == ECityPalettePage::PolicyReview)
    {
        const bool Enable = bPendingPolicyEnable;
        Data.MenuStatus = FText::FromString(FString(Enable ? TEXT("ENABLE ") : TEXT("DISABLE ")) + FCityPolicyState::Name(PendingPolicy));
        auto Candidate = E.Policies; Candidate.SetEnabled(PendingPolicy, Enable);
        const auto Q = FCityPolicyState::Quote(PendingPolicy, P, S, Sim->GetEconomyTuning(), !E.HasOperatingUpkeepShortfall());
        const int64 NewCost = Candidate.Forecast(P, S, Sim->GetEconomyTuning(), !E.HasOperatingUpkeepShortfall());
        Metric(TEXT("Program forecast / day"), Money(Enable ? Q.Total() : 0));
        Metric(TEXT("Budget after / day"), Money(Budget.Net() + Budget.PolicyCost - NewCost));
        Section(TEXT("Who receives it"), PendingPolicy == ECityPolicy::ReserveAssistance ?
            TEXT("Residents short of a basic purchase buffer. Top up by at most $5 per eligible resident / day.") :
            TEXT("Residents with an operating school place: $2 per served learner / day. No instant qualification. Detention and medical leave exclude bursaries."));
        auto& R = Section(TEXT("Current eligibility quote"), Enable ? TEXT("") : TEXT("Disabling stops future funding; eligibility below is for reference."));
        R.Rows = {Row(TEXT("Eligible residents"), FText::AsNumber(Q.Eligible).ToString()), Row(TEXT("Resident grants"), Money(Q.Grants)), Row(TEXT("Administration"), Money(Q.Administration())), Row(TEXT("City cash"), Money(Cash))};
        if (Enable && Q.Total() > Cash) Section(TEXT("CURRENT CASH BELOW FORECAST COST"), TEXT("Funding is checked at the next settlement; the full program must be affordable then."), false, true);
        Section(TEXT("No money moves now"), TEXT("Change takes effect at the next daily settlement."));
        Section(TEXT("Funding rules"), TEXT("Treasury pays eligible residents; administration goes to an external civic provider. Full program funded or no charge/benefit. Skipped optional programs create no arrears."), true); Message(PolicyMessage);
    }
    else if (PalettePage == ECityPalettePage::TechnologyReview)
    {
        const auto* Catalog = Sim->GetBuildingCatalog();
        const auto* D = Catalog ? Catalog->FindDefinition(TechnologyReviewDefinition) : nullptr;
        if (!D) { Section(TEXT("Technology catalog unavailable"), TEXT("")); return; }
        Data.MenuStatus = FText::FromString(D->DisplayName + (D->PrerequisitesMet(&S) ? TEXT(" | PREREQUISITES MET") : TEXT(" | LOCKED")));
        Metric(TEXT("Build cost"), Money(D->Cost)); Metric(TEXT("Upkeep / day"), Money(D->MonthlyUpkeep) + TEXT(" + payroll"));
        Section(TEXT("Prerequisites"), D->DescribePrerequisites(S, Catalog));
        Section(TEXT("Required to operate"), FString::Printf(TEXT("Power %d | Water %d | Staff %d (%s)\nAt least 50%% qualified, paid staff."), D->PowerDemand, D->WaterDemand, D->JobsProvided, D->MinimumStaffEducation == 3 ? TEXT("Higher") : TEXT("Secondary+")));
        Section(TEXT("Operating facilities"), FString::Printf(TEXT("Exchange %d | Computing %d | Research %d"), S.OperatingFacilities.FindRef(TEXT("DigitalExchange")), S.OperatingFacilities.FindRef(TEXT("ComputingCenter")), S.OperatingFacilities.FindRef(TEXT("ResearchCampus"))));
        const FName Id = TechnologyReviewDefinition;
        Section(TEXT("Unlocks"), Id == TEXT("DigitalExchange") ? TEXT("Connected computing facilities") : Id == TEXT("ComputingCenter") ? TEXT("A research campus") : Id == TEXT("ResearchCampus") ? TEXT("An AI institute") : Id == TEXT("AIInstitute") ? TEXT("Enterprise AI and future robotics") : TEXT("Optional 2-shop commercial pilot"));
        Section(TEXT("Outages suspend capability"), TEXT("Dependent facilities also pause. Construct facilities under Build > Technology."), false, true);
    }
    else if (PalettePage == ECityPalettePage::CommercialAi)
    {
        Data.MenuStatus = FText::FromString(P.bCommercialAiPilot ? TEXT("PILOT ON") : TEXT("PILOT OFF"));
        Metric(TEXT("Paid shops / slots"), FString::Printf(TEXT("%d / %d"), S.CommercialAiShops.Num(), S.OperatingAiCenters.Num() * FCityCommercialAi::ShopsPerCenter));
        Metric(TEXT("Software fee"), TEXT("$5 / shop / month"));
        Section(TEXT("Provider availability"), FString::Printf(TEXT("Operating institutes %d | Enterprise centers %d\nEach center serves 2 eligible shops, in stable ID order."), S.OperatingFacilities.FindRef(TEXT("AIInstitute")), S.OperatingAiCenters.Num()));
        if (P.bCommercialAiPilot && (S.OperatingFacilities.FindRef(TEXT("AIInstitute")) <= 0 || S.OperatingAiCenters.IsEmpty()))
            Section(TEXT("PROVIDER OFFLINE"), TEXT("Restore an operating institute and enterprise center. Shops retain ordinary operation."), false, true);
        Section(TEXT("What changes"), TEXT("One fewer vacancy; specialist roles require Higher.\n+25% retail capacity requires qualified, paid staff. Stock, freight and buyers are still required."));
        int32 Count = 0;
        for (const auto& B : P.Businesses)
        {
            if (B.bIndustrial || Count >= 3) continue;
            ++Count; Section(FString::Printf(TEXT("Shop #%llu"), B.EntityId), FCityCommercialAi::Status(P, S, B.EntityId));
        }
        Section(TEXT("Payment and limits"), TEXT("Paid to an external software provider after full payroll, at the first eligible daily settlement. Pause/outage restores roles; no prepaid refund. City facilities still incur public bills."), true);
    }
    else if (PalettePage == ECityPalettePage::HousingHistory)
    {
        Data.MenuStatus = FText::FromString(FCityHousingHistory::Warnings(P));
        if (P.HousingEvents.IsEmpty()) Section(TEXT("No housing events yet"), TEXT("New monthly housing decisions will appear here. Past events are not inferred. Inspect warning buildings for current needs."));
        const int32 PerPage = FCityHousingHistory::EventsPerPage;
        const int32 Page = FMath::Clamp(HousingHistoryPage, 0, FMath::Max(0, (P.HousingEvents.Num() - 1) / PerPage));
        const int32 First = P.HousingEvents.Num() - 1 - Page * PerPage;
        for (int32 I = First; I >= 0 && I > First - PerPage; --I)
        {
            const auto& H = P.HousingEvents[I];
            const TCHAR* Outcome = H.Kind == ECityHousingEventKind::Relocated ? TEXT("MOVED") : H.Kind == ECityHousingEventKind::Departed ? TEXT("LEFT CITY") : H.Kind == ECityHousingEventKind::Recovered ? TEXT("SEARCH ENDED") : H.Kind == ECityHousingEventKind::Waiting ? TEXT("MOVE DELAYED") : TEXT("HOME SEARCH");
            FString EventBody = H.DestinationHome ? FString::Printf(TEXT("Home #%llu (%d,%d) -> Home #%llu (%d,%d)\nOutlook %d -> %d | Moving fee %s"), H.SourceHome, H.SourceCell.X, H.SourceCell.Y, H.DestinationHome, H.DestinationCell.X, H.DestinationCell.Y, H.Outlook, H.DestinationOutlook, *Money(H.Money)) :
                (H.SourceHome ? FString::Printf(TEXT("Home #%llu (%d,%d)\n"), H.SourceHome, H.SourceCell.X, H.SourceCell.Y) : FString(TEXT("No home\n"))) + FCityHousingMigration::Describe(static_cast<ECityHousingStatus>(H.HousingStatus));
            if (H.Kind == ECityHousingEventKind::Departed)
                EventBody += TEXT("\nNo viable affordable home after six unresolved low months. Cash carried out ") + Money(H.Money);
            EventBody += TEXT("\nConcerns: ") + FCityHappinessEvaluator::DescribeConcerns(H.Concerns);
            Section(FString::Printf(TEXT("%s | %d residents | Month %lld"), Outcome, H.Residents, H.Month), EventBody);
            auto& Detail = Section(TEXT("Event details and concerns"), FCityHousingHistory::Describe(H), true);
            Detail.Id = FString::Printf(TEXT("Housing-%llu"), H.Id);
        }
        Section(TEXT("Newest first"), FString::Printf(TEXT("Page %d / %d | latest 32 decisions"), Page + 1, FMath::Max(1, (P.HousingEvents.Num() + PerPage - 1) / PerPage)));
    }
    for (auto& Action : Data.MenuEntries)
    {
        const auto Id = static_cast<ECityPaletteAction>(Action.ActionId);
        if (Id == ECityPaletteAction::ConfirmLoan) Action.Label = FText::FromString(bLoanPayoffReview ? TEXT("Pay off loan") : TEXT("Borrow ") + Money(PendingLoanAmount));
        if (Id == ECityPaletteAction::ConfirmPolicy) Action.Label = FText::FromString(FString(bPendingPolicyEnable ? TEXT("Enable ") : TEXT("Disable ")) + FCityPolicyState::Name(PendingPolicy));
    }
}
