#pragma once
#include "CoreMinimal.h"
#include "Presentation/CitySoundscapeAssets.h"
#include "Simulation/CityTrafficSimulation.h"
#include "Simulation/CityDisasterSimulation.h"

namespace CitySoundscape
{
	inline constexpr int32 VehicleLimit = 6, SirenLimit = 16, BedLimit = 8, ShotLimit = 3;
	inline constexpr int32 TrafficLimit = 2, TrafficSlots = 3;
	inline constexpr int32 PoolSize = SirenLimit + BedLimit + 2 + TrafficSlots + ShotLimit;
	inline bool IsRegularTraffic(const FCityTrafficVehicleState& Vehicle)
	{
		return !Vehicle.bCompleted && (Vehicle.VehicleType == ECityTripVehicleType::Car ||
		                               Vehicle.VehicleType == ECityTripVehicleType::FreightTruck ||
		                               Vehicle.VehicleType == ECityTripVehicleType::GarbageTruck);
	}
	inline ECityAudioCue TrafficLoop(const FCityTrafficVehicleState& Vehicle)
	{
		const bool Truck = Vehicle.VehicleType != ECityTripVehicleType::Car;
		return Vehicle.SpeedUnitsPerStep > 0 ? (Truck ? ECityAudioCue::TruckDriving : ECityAudioCue::CarDriving)
		                                     : (Truck ? ECityAudioCue::TruckIdle : ECityAudioCue::CarIdle);
	}
	inline bool IsTrafficLoop(ECityAudioCue Cue)
	{
		return Cue >= ECityAudioCue::CarIdle && Cue <= ECityAudioCue::TruckDriving;
	}
	inline int64 TownAlarmEndStep(const FCityDisasterEvent& Event)
	{
		return Event.Kind == ECityNaturalDisaster::Earthquake
		           ? Event.WarningStep + FMath::DivideAndRoundUp(Event.EndStep - Event.WarningStep, int64(2))
		           : Event.EndStep;
	}
	inline constexpr float InterruptSeconds = .04f, PhaseBlendSeconds = .04f;
	inline float AccentPrimaryGain(ECityAudioCue Primary, ECityAudioCue Accent)
	{
		return Primary == ECityAudioCue::PoliceLoop && Accent == ECityAudioCue::PoliceAlternate ? 0.f
		       : Primary == ECityAudioCue::FireLoop && Accent == ECityAudioCue::FireHorn        ? .5f
		                                                                                        : 1.f;
	}
	inline bool IsResponding(const FCityTrafficVehicleState& Vehicle)
	{
		return Vehicle.bEmergencyResponding && !Vehicle.bReturning && !Vehicle.bCompleted &&
		       (Vehicle.VehicleType == ECityTripVehicleType::PoliceCar ||
		        Vehicle.VehicleType == ECityTripVehicleType::FireEngine ||
		        Vehicle.VehicleType == ECityTripVehicleType::Ambulance);
	}
	inline ECityAudioCue ResponseLoop(ECityTripVehicleType Type)
	{
		return Type == ECityTripVehicleType::PoliceCar    ? ECityAudioCue::PoliceLoop
		       : Type == ECityTripVehicleType::FireEngine ? ECityAudioCue::FireLoop
		                                                  : ECityAudioCue::AmbulanceLoop;
	}
	inline float AreaWeight(double DistanceCells, double RadiusCells)
	{
		if (!FMath::IsFinite(DistanceCells) || !FMath::IsFinite(RadiusCells) || RadiusCells <= 0)
			return 0;
		const double T = FMath::Clamp(1.0 - FMath::Max(0.0, DistanceCells) / RadiusCells, 0.0, 1.0);
		return static_cast<float>(T * T * (3 - 2 * T));
	}
	inline float LocalDetail(double HeightCells)
	{
		return AreaWeight(HeightCells, 48.0);
	}
	inline bool IsVehicleSiren(ECityAudioCue Cue)
	{
		return Cue <= ECityAudioCue::AmbulanceOutro || Cue == ECityAudioCue::PoliceAlternate ||
		       Cue == ECityAudioCue::FireHorn;
	}
	inline float VehicleSirenGain(double DistanceCells)
	{
		if (!FMath::IsFinite(DistanceCells))
			return 0;
		const float Nearby = AreaWeight(FMath::Max(0.0, DistanceCells - 8), 22);
		// Preserve close camera detail; the distant city bed is at most1.2%.
		return .95f * Nearby * Nearby + .012f * AreaWeight(DistanceCells, 96);
	}
	inline float Approach(float Current, float Target, double Seconds, double TimeConstant = 1.2)
	{
		return FMath::Lerp(Current, Target,
		                   static_cast<float>(1.0 - FMath::Exp(-FMath::Max(0.0, Seconds) / TimeConstant)));
	}
	// Session/load and blocked frames consume notifications, never replaying stale edges.
	class FTransitionGate
	{
	public:
		bool Observe(uint64 Session, bool Blocked, int64 Value)
		{
			const bool Changed = Session == SeenSession && Value != SeenValue && !Blocked && !WasBlocked;
			SeenSession = Session;
			SeenValue = Value;
			WasBlocked = Blocked;
			return Changed;
		}

	private:
		uint64 SeenSession = MAX_uint64;
		int64 SeenValue = 0;
		bool WasBlocked = true;
	};
} // namespace CitySoundscape
