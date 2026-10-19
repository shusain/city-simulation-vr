#include "Presentation/CityIndustrialEntrances.h"
#include "Misc/Paths.h"
TConstArrayView<FCityResidentialEntrance> CityIndustrialEntrances::All()
{
    static const FCityResidentialEntrance Entries[]={
        {TEXT("SM_LowDensity_Factory_A"),TEXT("/Game/CitySim/Art/IndustrialGround/SM_IndustrialGround_LowDensity_Factory_A"),TEXT("SM_LowDensity_Factory_A_IndustrialSupport"),FVector(520,444,20),10,60,130},
        {TEXT("SM_LowDensity_Factory_A_Damaged"),TEXT("/Game/CitySim/Art/IndustrialGround/SM_IndustrialGround_LowDensity_Factory_A_Damaged"),TEXT("SM_LowDensity_Factory_A_IndustrialSupport"),FVector(520,444,20),10,60,130},
        {TEXT("SM_LowDensity_Factory_B"),TEXT("/Game/CitySim/Art/IndustrialGround/SM_IndustrialGround_LowDensity_Factory_B"),TEXT("SM_LowDensity_Factory_B_IndustrialSupport"),FVector(520,444,20),10,60,130},
        {TEXT("SM_LowDensity_Factory_B_Damaged"),TEXT("/Game/CitySim/Art/IndustrialGround/SM_IndustrialGround_LowDensity_Factory_B_Damaged"),TEXT("SM_LowDensity_Factory_B_IndustrialSupport"),FVector(520,444,20),10,60,130},
        {TEXT("SM_LowDensity_Factory_C"),TEXT("/Game/CitySim/Art/IndustrialGround/SM_IndustrialGround_LowDensity_Factory_C"),TEXT("SM_LowDensity_Factory_C_IndustrialSupport"),FVector(520,444,20),10,60,130},
        {TEXT("SM_LowDensity_Factory_C_Damaged"),TEXT("/Game/CitySim/Art/IndustrialGround/SM_IndustrialGround_LowDensity_Factory_C_Damaged"),TEXT("SM_LowDensity_Factory_C_IndustrialSupport"),FVector(520,444,20),10,60,130},
        {TEXT("SM_LowDensity_Factory_D"),TEXT("/Game/CitySim/Art/IndustrialGround/SM_IndustrialGround_LowDensity_Factory_D"),TEXT("SM_LowDensity_Factory_D_IndustrialSupport"),FVector(520,444,20),10,60,130},
        {TEXT("SM_LowDensity_Factory_D_Damaged"),TEXT("/Game/CitySim/Art/IndustrialGround/SM_IndustrialGround_LowDensity_Factory_D_Damaged"),TEXT("SM_LowDensity_Factory_D_IndustrialSupport"),FVector(520,444,20),10,60,130},
        {TEXT("SM_Building_Factory_A"),TEXT("/Game/CitySim/Art/IndustrialGround/SM_IndustrialGround_Building_Factory_A"),TEXT("SM_Building_Factory_A_IndustrialSupport"),FVector(520,444,20),10,60,130},
        {TEXT("SM_Building_Factory_Medium_A"),TEXT("/Game/CitySim/Art/IndustrialGround/SM_IndustrialGround_Building_Factory_Medium_A"),TEXT("SM_Building_Factory_Medium_A_IndustrialSupport"),FVector(520,444,20),10,60,130},
        {TEXT("SM_Building_Factory_High_A"),TEXT("/Game/CitySim/Art/IndustrialGround/SM_IndustrialGround_Building_Factory_High_A"),TEXT("SM_Building_Factory_High_A_IndustrialSupport"),FVector(520,444,20),10,60,130},
        {TEXT("SM_Building_Factory_A_Damaged"),TEXT("/Game/CitySim/Art/IndustrialGround/SM_IndustrialGround_Building_Factory_A_Damaged"),TEXT("SM_Building_Factory_A_IndustrialSupport"),FVector(520,444,20),10,60,130},
        {TEXT("SM_Building_Factory_Medium_A_Damaged"),TEXT("/Game/CitySim/Art/IndustrialGround/SM_IndustrialGround_Building_Factory_Medium_A_Damaged"),TEXT("SM_Building_Factory_Medium_A_IndustrialSupport"),FVector(520,444,20),10,60,130},
        {TEXT("SM_Building_Factory_High_A_Damaged"),TEXT("/Game/CitySim/Art/IndustrialGround/SM_IndustrialGround_Building_Factory_High_A_Damaged"),TEXT("SM_Building_Factory_High_A_IndustrialSupport"),FVector(520,444,20),10,60,130},
    };
    return MakeArrayView(Entries);
}
const FCityResidentialEntrance* CityIndustrialEntrances::Find(const FString& MeshName)
{
    for(const auto& Entry:All())
        if(MeshName==Entry.OriginalMesh || MeshName==FPaths::GetBaseFilename(Entry.TargetMesh)) return &Entry;
    return nullptr;
}
TConstArrayView<FCityIndustrialBay> CityIndustrialEntrances::Bays(const FString& MeshName)
{
    if(MeshName==TEXT("SM_LowDensity_Factory_A") || MeshName==TEXT("SM_IndustrialGround_LowDensity_Factory_A"))
    {
        static const FCityIndustrialBay Entries[]={{FVector(0,437,13),565.0,false}};
        return MakeArrayView(Entries);
    }
    if(MeshName==TEXT("SM_LowDensity_Factory_A_Damaged") || MeshName==TEXT("SM_IndustrialGround_LowDensity_Factory_A_Damaged"))
    {
        static const FCityIndustrialBay Entries[]={{FVector(0,437,13),565.0,false}};
        return MakeArrayView(Entries);
    }
    if(MeshName==TEXT("SM_LowDensity_Factory_B") || MeshName==TEXT("SM_IndustrialGround_LowDensity_Factory_B"))
    {
        static const FCityIndustrialBay Entries[]={{FVector(0,437,13),565.0,false}};
        return MakeArrayView(Entries);
    }
    if(MeshName==TEXT("SM_LowDensity_Factory_B_Damaged") || MeshName==TEXT("SM_IndustrialGround_LowDensity_Factory_B_Damaged"))
    {
        static const FCityIndustrialBay Entries[]={{FVector(0,437,13),565.0,false}};
        return MakeArrayView(Entries);
    }
    if(MeshName==TEXT("SM_LowDensity_Factory_C") || MeshName==TEXT("SM_IndustrialGround_LowDensity_Factory_C"))
    {
        static const FCityIndustrialBay Entries[]={{FVector(0,507,8),565.0,true}};
        return MakeArrayView(Entries);
    }
    if(MeshName==TEXT("SM_LowDensity_Factory_C_Damaged") || MeshName==TEXT("SM_IndustrialGround_LowDensity_Factory_C_Damaged"))
    {
        static const FCityIndustrialBay Entries[]={{FVector(0,507,8),565.0,true}};
        return MakeArrayView(Entries);
    }
    if(MeshName==TEXT("SM_LowDensity_Factory_D") || MeshName==TEXT("SM_IndustrialGround_LowDensity_Factory_D"))
    {
        static const FCityIndustrialBay Entries[]={{FVector(-147.0,437,13),260.0,false},{FVector(147.0,437,13),260.0,false}};
        return MakeArrayView(Entries);
    }
    if(MeshName==TEXT("SM_LowDensity_Factory_D_Damaged") || MeshName==TEXT("SM_IndustrialGround_LowDensity_Factory_D_Damaged"))
    {
        static const FCityIndustrialBay Entries[]={{FVector(-147.0,437,13),260.0,false},{FVector(147.0,437,13),260.0,false}};
        return MakeArrayView(Entries);
    }
    if(MeshName==TEXT("SM_Building_Factory_A") || MeshName==TEXT("SM_IndustrialGround_Building_Factory_A"))
    {
        static const FCityIndustrialBay Entries[]={{FVector(0,437,13),565.0,false}};
        return MakeArrayView(Entries);
    }
    if(MeshName==TEXT("SM_Building_Factory_Medium_A") || MeshName==TEXT("SM_IndustrialGround_Building_Factory_Medium_A"))
    {
        static const FCityIndustrialBay Entries[]={{FVector(0,437,13),565.0,false}};
        return MakeArrayView(Entries);
    }
    if(MeshName==TEXT("SM_Building_Factory_High_A") || MeshName==TEXT("SM_IndustrialGround_Building_Factory_High_A"))
    {
        static const FCityIndustrialBay Entries[]={{FVector(0,437,13),565.0,false}};
        return MakeArrayView(Entries);
    }
    if(MeshName==TEXT("SM_Building_Factory_A_Damaged") || MeshName==TEXT("SM_IndustrialGround_Building_Factory_A_Damaged"))
    {
        static const FCityIndustrialBay Entries[]={{FVector(0,437,13),565.0,false}};
        return MakeArrayView(Entries);
    }
    if(MeshName==TEXT("SM_Building_Factory_Medium_A_Damaged") || MeshName==TEXT("SM_IndustrialGround_Building_Factory_Medium_A_Damaged"))
    {
        static const FCityIndustrialBay Entries[]={{FVector(0,437,13),565.0,false}};
        return MakeArrayView(Entries);
    }
    if(MeshName==TEXT("SM_Building_Factory_High_A_Damaged") || MeshName==TEXT("SM_IndustrialGround_Building_Factory_High_A_Damaged"))
    {
        static const FCityIndustrialBay Entries[]={{FVector(0,437,13),565.0,false}};
        return MakeArrayView(Entries);
    }
    return {};
}
