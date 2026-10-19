#pragma once

#include "CoreMinimal.h"

struct CITYSIMVR_API FCityInstanceHandle
{
	FName DefinitionId;
	int32 InstanceIndex = INDEX_NONE;
};

class CITYSIMVR_API FCityInstanceIndex
{
public:
	int32 Add(FName DefinitionId, uint64 EntityId);
	bool Remove(uint64 EntityId);
	const FCityInstanceHandle* Find(uint64 EntityId) const;
	uint64 GetEntityAt(FName DefinitionId, int32 InstanceIndex) const;
	void Reset();

private:
	TMap<uint64, FCityInstanceHandle> EntityToInstance;
	TMap<FName, TArray<uint64>> InstanceToEntity;
};
