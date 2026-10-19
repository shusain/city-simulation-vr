#pragma once
#include "Presentation/CityCommercialEntrances.h"

struct CITYSIMVR_API FCityIndustrialBay
{
    FVector EntryCm;
    double WidthCm;
    bool bRaisedDock;
    bool bGroundStart = false; // Open staging areas start on terrain, not a building sill.
};
namespace CityIndustrialEntrances
{
    CITYSIMVR_API TConstArrayView<FCityResidentialEntrance> All();
    CITYSIMVR_API const FCityResidentialEntrance* Find(const FString& MeshName);
    CITYSIMVR_API TConstArrayView<FCityIndustrialBay> Bays(const FString& MeshName);
}
