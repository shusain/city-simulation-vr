#pragma once
#include "CoreMinimal.h"

// Stable identities; treatment is authority data, never an Actor timer.
enum class ECityIllness : uint8
{
	Mild,
	Severe
};
enum class ECityHealthCare : uint8
{
	None,
	HomeRecovery,
	WalkIn,
	AwaitingAmbulance,
	Hospital,
	DriveIn,
	ReturningHome
};
struct CITYSIMVR_API FCityHealthPatient
{
	uint64 CitizenId = 0, HospitalId = 0;
	ECityIllness Illness = ECityIllness::Mild;
	ECityHealthCare Care = ECityHealthCare::None;
	int64 OnsetStep = 0, TreatmentSteps = 0, LastTreatmentStep = 0;
	int32 ExposureRisk = 0;
	bool InterruptsWork() const
	{
		return Illness == ECityIllness::Severe || Care != ECityHealthCare::None;
	}
	bool UsesBed() const { return Illness == ECityIllness::Severe && (Care == ECityHealthCare::AwaitingAmbulance || Care == ECityHealthCare::Hospital); }
	bool UsesOutpatientPlace() const { return Care == ECityHealthCare::WalkIn || Care == ECityHealthCare::DriveIn; }
	bool ReceivingCare() const { return Care == ECityHealthCare::HomeRecovery || Care == ECityHealthCare::ReturningHome || Care == ECityHealthCare::Hospital; }
	bool AwaitingCare() const { return UsesOutpatientPlace() || Care == ECityHealthCare::AwaitingAmbulance; }
};
struct CITYSIMVR_API FCityHealthSnapshot
{
	int64 LastStep = 0, IntroductionStep = -1, LastCheckStep = 0, TotalRecovered = 0;
	TArray<FCityHealthPatient> Patients;
	const FCityHealthPatient* Find(uint64 CitizenId) const
	{
		return Patients.FindByPredicate([CitizenId](const auto& P) { return P.CitizenId == CitizenId; });
	}
};
struct FCityHealthCitizenExposure
{
	uint64 CitizenId = 0;
	int32 AdditionalRisk = 0;
	bool bDetained = false;
};
struct FCityHealthHospital
{
	uint64 EntityId = 0;
	int32 Capacity = 0;
	bool bOperational = false;
};
struct FCityHealthContext
{
	int32 SecondaryResidents = 0;
	bool bSafetyEstablished = false;
	TArray<FCityHealthCitizenExposure> Citizens;
	TArray<FCityHealthHospital> Hospitals;
};
struct FCityHealthTuning
{
	uint32 RandomSeed = 7429;
	int32 PopulationThreshold = 250, EducatedThreshold = 25, BaseRisk = 2;
	int64 CheckIntervalSteps = 25, GraceSteps = 150, MildTreatmentSteps = 25, SevereTreatmentSteps = 50;
};
class CITYSIMVR_API FCityHealthSimulation
{
public:
	static constexpr int32 MaximumWalkingDistance = 8;
	const FCityHealthSnapshot& GetSnapshot() const
	{
		return State;
	}
	bool RestoreState(const FCityHealthSnapshot& Snapshot, FString& Error);
	static bool ValidateState(const FCityHealthSnapshot& Snapshot, FString& Error);
	void AdvanceToStep(int64 Step, const FCityHealthContext& Context, const FCityHealthTuning& Tuning);
	bool StartIllness(uint64 CitizenId, ECityIllness Illness, int32 Risk = 0);
	// Caller supplies a validated pedestrian route length. Ambulance reservation is not arrival.
	bool ReserveCare(uint64 CitizenId, uint64 HospitalId, ECityHealthCare Care, int32 WalkingSteps,
	                 const FCityHealthContext& Context);
	bool ReleaseCare(uint64 CitizenId);
	bool ConfirmArrival(uint64 CitizenId, uint64 HospitalId);
	bool ConfirmHome(uint64 CitizenId);

private:
	FCityHealthSnapshot State;
};
