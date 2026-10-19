#pragma once
#include "CoreMinimal.h"
#include "Simulation/CityWasteSimulation.h"
class FJsonObject;
class CITYSIMVR_API FCityWastePersistence
{
public:
	static TSharedPtr<FJsonObject> Write(const FCityWasteSnapshot& State);
	static bool Read(const TSharedPtr<FJsonObject>& Object, FCityWasteSnapshot& State, int64 MaximumStep,
	                 FString& Error, FCityGridDimensions Dimensions = FCityGridDimensions());
};
