#pragma once
#include "CoreMinimal.h"
#include "Simulation/CityDisasterSimulation.h"
// Presentation cursor: consumes blocked onsets, suppresses restored active events.
class CITYSIMVR_API FCityDisasterAlarmGate
{
public:
	bool Observe(const FCityDisasterSnapshot& State, uint64 Session, bool Blocked)
	{
		const auto* Event = State.Current();
		const bool Active = Event && State.LastFixedStepId >= Event->StartStep;
		if (Session != ObservedSession)
		{
			ObservedSession = Session;
			SeenId = Event && !Active ? Event->Id - 1 : State.TotalEvents;
			return false;
		}
		if (!Active || Event->Id <= SeenId)
			return false;
		SeenId = Event->Id;
		return !Blocked;
	}

private:
	uint64 ObservedSession = MAX_uint64;
	int64 SeenId = 0;
};
