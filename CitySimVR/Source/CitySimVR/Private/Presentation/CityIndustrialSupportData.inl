// #125 structural hall, personnel sill, freight sill or raised loading ledge only.
if(Name==TEXT("SM_Building_Factory_A_IndustrialSupport"))
{
    static const FCityBuildingSupportRegion Regions[]={
        {-750,-700,750,400,0},
        {454,400,586,444,16},
        {-291.5,411,291.5,437,8},
    };
    return MakeArrayView(Regions);
}
if(Name==TEXT("SM_Building_Factory_High_A_IndustrialSupport"))
{
    static const FCityBuildingSupportRegion Regions[]={
        {-750,-700,750,400,0},
        {454,400,586,444,16},
        {-291.5,411,291.5,437,8},
    };
    return MakeArrayView(Regions);
}
if(Name==TEXT("SM_Building_Factory_Medium_A_IndustrialSupport"))
{
    static const FCityBuildingSupportRegion Regions[]={
        {-750,-700,750,400,0},
        {454,400,586,444,16},
        {-291.5,411,291.5,437,8},
    };
    return MakeArrayView(Regions);
}
if(Name==TEXT("SM_LowDensity_Factory_A_IndustrialSupport"))
{
    static const FCityBuildingSupportRegion Regions[]={
        {-750,-700,750,400,0},
        {454,400,586,444,16},
        {-291.5,411,291.5,437,8},
        {-350,490,-326,514,0},
        {326,490,350,514,0},
    };
    return MakeArrayView(Regions);
}
if(Name==TEXT("SM_LowDensity_Factory_B_IndustrialSupport"))
{
    static const FCityBuildingSupportRegion Regions[]={
        {-750,-700,750,400,0},
        {454,400,586,444,16},
        {-291.5,411,291.5,437,8},
        {-350,490,-326,514,0},
        {326,490,350,514,0},
    };
    return MakeArrayView(Regions);
}
if(Name==TEXT("SM_LowDensity_Factory_C_IndustrialSupport"))
{
    static const FCityBuildingSupportRegion Regions[]={
        {-750,-700,750,400,0},
        {454,400,586,444,16},
        {-320,405,320,507,0},
    };
    return MakeArrayView(Regions);
}
if(Name==TEXT("SM_LowDensity_Factory_D_IndustrialSupport"))
{
    static const FCityBuildingSupportRegion Regions[]={
        {-750,-700,750,400,0},
        {454,400,586,444,16},
        {-286.0,411,-8.0,437,8},
        {8.0,411,286.0,437,8},
        {-350,490,-326,514,0},
        {326,490,350,514,0},
    };
    return MakeArrayView(Regions);
}
