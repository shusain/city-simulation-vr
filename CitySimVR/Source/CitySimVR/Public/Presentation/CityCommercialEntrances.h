#pragma once
#include "Presentation/CityResidentialEntrances.h"

namespace CityCommercialEntrances
{
    CITYSIMVR_API TConstArrayView<FCityResidentialEntrance> All();
    CITYSIMVR_API const FCityResidentialEntrance* Find(const FString& MeshName);
}
