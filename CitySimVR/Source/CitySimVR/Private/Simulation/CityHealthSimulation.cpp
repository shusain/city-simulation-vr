#include "Simulation/CityHealthSimulation.h"

bool FCityHealthSimulation::ValidateState(const FCityHealthSnapshot& S, FString& Error)
{
	if (S.LastStep < 0 || S.IntroductionStep < -1 || S.IntroductionStep > S.LastStep || S.LastCheckStep < 0 ||
	    S.LastCheckStep > S.LastStep || S.TotalRecovered < 0 || S.Patients.Num() > 100000)
	{
		Error = TEXT("Invalid health clock or population bound");
		return false;
	}
	TSet<uint64> Ids;
	for (const auto& P : S.Patients)
	{
		if (!P.CitizenId || Ids.Contains(P.CitizenId) || static_cast<uint8>(P.Illness) > 1 ||
		    static_cast<uint8>(P.Care) > 6 || P.OnsetStep < 0 || P.OnsetStep > S.LastStep ||
		    P.LastTreatmentStep < P.OnsetStep || P.LastTreatmentStep > S.LastStep || P.TreatmentSteps < 0 ||
		    P.TreatmentSteps > S.LastStep - P.OnsetStep || P.ExposureRisk < 0 || P.ExposureRisk > 100 ||
		    (P.HospitalId == 0) != (P.Care == ECityHealthCare::None || P.Care == ECityHealthCare::HomeRecovery) ||
		    (P.Illness == ECityIllness::Severe &&
		     (P.Care == ECityHealthCare::HomeRecovery || P.Care == ECityHealthCare::WalkIn || P.Care == ECityHealthCare::DriveIn || P.Care == ECityHealthCare::ReturningHome)) ||
		    (P.Illness == ECityIllness::Mild && (P.Care == ECityHealthCare::AwaitingAmbulance || P.Care == ECityHealthCare::Hospital)))
		{
			Error = TEXT("Invalid or duplicate health patient");
			return false;
		}
		Ids.Add(P.CitizenId);
	}
	return true;
}
bool FCityHealthSimulation::RestoreState(const FCityHealthSnapshot& S, FString& Error)
{
	if (!ValidateState(S, Error))
		return false;
	State = S;
	State.Patients.Sort([](const auto& A, const auto& B) { return A.CitizenId < B.CitizenId; });
	return true;
}
bool FCityHealthSimulation::StartIllness(uint64 Id, ECityIllness Illness, int32 Risk)
{
	if (!Id || static_cast<uint8>(Illness) > 1 || State.Find(Id) || State.Patients.Num() >= 100000)
		return false;
	FCityHealthPatient P;
	P.CitizenId = Id;
	P.Illness = Illness;
	P.OnsetStep = P.LastTreatmentStep = State.LastStep;
	P.ExposureRisk = FMath::Clamp(Risk, 0, 100);
	State.Patients.Add(P);
	State.Patients.Sort([](const auto& A, const auto& B) { return A.CitizenId < B.CitizenId; });
	return true;
}
bool FCityHealthSimulation::ReserveCare(uint64 Id, uint64 Hospital, ECityHealthCare Care, int32 WalkingSteps,
                                        const FCityHealthContext& Context)
{
	auto* P = State.Patients.FindByPredicate([Id](const auto& X) { return X.CitizenId == Id; });
	const auto* H = Context.Hospitals.FindByPredicate([Hospital](const auto& X) { return X.EntityId == Hospital; });
	const bool Outpatient = Care == ECityHealthCare::WalkIn || Care == ECityHealthCare::DriveIn;
	if (!P || P->Care != ECityHealthCare::None || !H || !H->EntityId || !H->bOperational || H->Capacity <= 0 ||
	    (P->Illness == ECityIllness::Mild ? !Outpatient : Care != ECityHealthCare::AwaitingAmbulance)) return false;
	if (Care == ECityHealthCare::WalkIn && (WalkingSteps < 0 || WalkingSteps > MaximumWalkingDistance)) return false;
	int32 Used = 0;
	for (const auto& X : State.Patients)
		Used += X.HospitalId == Hospital && (Outpatient ? X.UsesOutpatientPlace() : X.UsesBed()) ? 1 : 0;
	if (Used >= H->Capacity) return false;
	P->HospitalId = Hospital;
	P->Care = Care;
	P->LastTreatmentStep = State.LastStep;
	return true;
}
bool FCityHealthSimulation::ReleaseCare(uint64 Id)
{
	auto* P = State.Patients.FindByPredicate([Id](const auto& X) { return X.CitizenId == Id; });
	if (!P)
		return false;
	P->HospitalId = 0;
	P->Care = ECityHealthCare::None;
	P->LastTreatmentStep = State.LastStep;
	return true;
}
bool FCityHealthSimulation::ConfirmArrival(uint64 Id, uint64 Hospital)
{
	auto* P = State.Patients.FindByPredicate([Id](const auto& X) { return X.CitizenId == Id; });
	if (!P || P->HospitalId != Hospital ||
	    (P->Care != ECityHealthCare::AwaitingAmbulance && !P->UsesOutpatientPlace()))
		return false;
	P->Care = P->Illness == ECityIllness::Severe ? ECityHealthCare::Hospital : ECityHealthCare::ReturningHome;
	P->LastTreatmentStep = State.LastStep;
	return true;
}
bool FCityHealthSimulation::ConfirmHome(uint64 Id)
{
	auto* P = State.Patients.FindByPredicate([&](const auto& X) { return X.CitizenId == Id; });
	if (!P || P->Care != ECityHealthCare::ReturningHome) return false;
	P->HospitalId = 0;
	P->Care = ECityHealthCare::HomeRecovery;
	P->LastTreatmentStep = State.LastStep;
	return true;
}
void FCityHealthSimulation::AdvanceToStep(int64 Step, const FCityHealthContext& Context, const FCityHealthTuning& T)
{
	if (Step <= State.LastStep)
		return;
	State.LastStep = Step;
	TSet<uint64> Citizens;
	for (const auto& C : Context.Citizens)
		Citizens.Add(C.CitizenId);
	State.Patients.RemoveAll([&](const auto& P) { return !Citizens.Contains(P.CitizenId); });
	TMap<uint64, int32> Used;
	TSet<uint64> Recovered;
	for (auto& P : State.Patients)
	{
		const int64 Elapsed = Step - P.LastTreatmentStep;
		P.LastTreatmentStep = Step;
		const auto* C = Context.Citizens.FindByPredicate([&](const auto& X) { return X.CitizenId == P.CitizenId; });
		if (!C || C->bDetained) continue;
		if (P.Care != ECityHealthCare::HomeRecovery)
		{
			if (!P.HospitalId) continue;
			const auto* H = Context.Hospitals.FindByPredicate([&](const auto& X) { return X.EntityId == P.HospitalId; });
			if (!H) { P.Care = ECityHealthCare::None; P.HospitalId = 0; continue; }
			// A dispensed course can return home during a service outage; ward care still needs operation.
			if (P.Care == ECityHealthCare::ReturningHome) continue;
			if (!P.UsesBed() || !H->bOperational || ++Used.FindOrAdd(P.HospitalId) > H->Capacity || P.Care != ECityHealthCare::Hospital) continue;
		}
		const int64 Required = FMath::Clamp<int64>(P.Illness == ECityIllness::Severe ? T.SevereTreatmentSteps : T.MildTreatmentSteps, 1, 36500);
		P.TreatmentSteps = FMath::Min(Required, P.TreatmentSteps + Elapsed);
		if (P.TreatmentSteps >= Required)
			Recovered.Add(P.CitizenId);
	}
	State.Patients.RemoveAll(
	    [&](const auto& P)
	    {
		    if (!Recovered.Contains(P.CitizenId))
			    return false;
		    ++State.TotalRecovered;
		    return true;
	    });
	if (State.IntroductionStep < 0)
	{
		if (Context.Citizens.Num() < T.PopulationThreshold || Context.SecondaryResidents < T.EducatedThreshold ||
		    !Context.bSafetyEstablished)
			return;
		State.IntroductionStep = Step;
		State.LastCheckStep = Step;
		return;
	}
	const int64 Interval = FMath::Clamp<int64>(T.CheckIntervalSteps, 1, 36500);
	while (Step - State.LastCheckStep >= Interval)
	{
		State.LastCheckStep += Interval;
		if (State.LastCheckStep - State.IntroductionStep < FMath::Clamp<int64>(T.GraceSteps, 0, 36500))
			continue;
		for (const auto& C : Context.Citizens)
		{
			if (!C.CitizenId || C.bDetained || State.Find(C.CitizenId))
				continue;
			const int32 Risk =
			    static_cast<int32>(FMath::Clamp<int64>(static_cast<int64>(T.BaseRisk) + C.AdditionalRisk, 0, 100));
			const uint32 Roll =
			    HashCombine(T.RandomSeed, HashCombine(GetTypeHash(C.CitizenId), GetTypeHash(State.LastCheckStep)));
			if (Roll % 100 < static_cast<uint32>(Risk) &&
			    StartIllness(C.CitizenId, (Roll >> 16) % 5 == 0 ? ECityIllness::Severe : ECityIllness::Mild, Risk))
			{
				auto* P = State.Patients.FindByPredicate([&](const auto& X) { return X.CitizenId == C.CitizenId; });
				P->OnsetStep = State.LastCheckStep;
			}
		}
	}
}
