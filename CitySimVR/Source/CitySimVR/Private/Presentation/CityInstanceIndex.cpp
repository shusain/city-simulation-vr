#include "Presentation/CityInstanceIndex.h"

int32 FCityInstanceIndex::Add(const FName DefinitionId, const uint64 EntityId)
{
	if (DefinitionId.IsNone() || EntityId == 0 || EntityToInstance.Contains(EntityId))
	{
		return INDEX_NONE;
	}

	TArray<uint64>& Entities = InstanceToEntity.FindOrAdd(DefinitionId);
	const int32 InstanceIndex = Entities.Add(EntityId);
	EntityToInstance.Add(EntityId, FCityInstanceHandle{DefinitionId, InstanceIndex});
	return InstanceIndex;
}

bool FCityInstanceIndex::Remove(const uint64 EntityId)
{
	const FCityInstanceHandle* ExistingHandle = EntityToInstance.Find(EntityId);
	if (!ExistingHandle)
	{
		return false;
	}

	const FName DefinitionId = ExistingHandle->DefinitionId;
	const int32 RemovedIndex = ExistingHandle->InstanceIndex;
	TArray<uint64>* Entities = InstanceToEntity.Find(DefinitionId);
	if (!Entities || !Entities->IsValidIndex(RemovedIndex) || (*Entities)[RemovedIndex] != EntityId)
	{
		return false;
	}

	Entities->RemoveAt(RemovedIndex);
	EntityToInstance.Remove(EntityId);
	for (int32 Index = RemovedIndex; Index < Entities->Num(); ++Index)
	{
		EntityToInstance.FindChecked((*Entities)[Index]).InstanceIndex = Index;
	}
	return true;
}

const FCityInstanceHandle* FCityInstanceIndex::Find(const uint64 EntityId) const
{
	return EntityToInstance.Find(EntityId);
}

uint64 FCityInstanceIndex::GetEntityAt(const FName DefinitionId, const int32 InstanceIndex) const
{
	const TArray<uint64>* Entities = InstanceToEntity.Find(DefinitionId);
	return Entities && Entities->IsValidIndex(InstanceIndex) ? (*Entities)[InstanceIndex] : 0;
}

void FCityInstanceIndex::Reset()
{
	EntityToInstance.Reset();
	InstanceToEntity.Reset();
}
