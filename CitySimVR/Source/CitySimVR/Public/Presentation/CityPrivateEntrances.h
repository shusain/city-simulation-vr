#pragma once
#include "Presentation/CityCivicEntrances.h"
#include "Presentation/CityUtilityEntrances.h"
namespace CityPrivateEntrances
{
    inline const FCityResidentialEntrance* Find(const FString& MeshName)
    {
        if(const auto* Entry=CityResidentialEntrances::Find(MeshName)) return Entry;
        if(const auto* Entry=CityCommercialEntrances::Find(MeshName)) return Entry;
        if(const auto* Entry=CityIndustrialEntrances::Find(MeshName)) return Entry;
        if(const auto* Entry=CityCivicEntrances::Find(MeshName)) return Entry;
        return CityUtilityEntrances::Find(MeshName);
    }
    inline TConstArrayView<FCityIndustrialBay> Bays(const FString& MeshName)
    {
        const auto Industrial=CityIndustrialEntrances::Bays(MeshName);
        if(!Industrial.IsEmpty()) return Industrial;
        const auto Civic=CityCivicEntrances::Bays(MeshName);
        return Civic.IsEmpty()?CityUtilityEntrances::Bays(MeshName):Civic;
    }
}
