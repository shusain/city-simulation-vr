#pragma once
#include "Presentation/CityIndustrialEntrances.h"
namespace CityUtilityEntrances
{
    CITYSIMVR_API bool IsDefinition(FName Id);
    CITYSIMVR_API TConstArrayView<FCityResidentialEntrance> All();
    CITYSIMVR_API const FCityResidentialEntrance* Find(const FString& MeshName);
    CITYSIMVR_API TConstArrayView<FCityIndustrialBay> Bays(const FString& MeshName);
}
