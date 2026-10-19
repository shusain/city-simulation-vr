#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "StaticMeshResources.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialParameters.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionPower.h"
#include "Materials/MaterialExpressionOneMinus.h"
#include "Materials/MaterialExpressionVertexColor.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionSaturate.h"
#include "Materials/MaterialExpressionSine.h"
#include "Materials/MaterialExpressionTime.h"
#include "Presentation/CityBoardActor.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Simulation/CityBuildingDefinition.h"

namespace
{
	struct FExpectedCityMesh
	{
		const TCHAR* Path;
		FVector DimensionsCm;
		TArray<FName> Slots;
		bool bExpectGroundPivot = true;
		int32 ExpectedTriangles = 0;
		double ExpectedMinimumZCm = 0;
	};
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityArtKitImportTest, "CitySim.Art.ImportedKit",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityArtKitImportTest::RunTest(const FString& Parameters)
{
	const TArray<FExpectedCityMesh> Expected = {
	    {TEXT("/Game/CitySim/Art/Meshes/SM_Building_Police_A.SM_Building_Police_A"),
	     FVector(2400.0000, 1840.0000, 900.0000),
	     {TEXT("Police"), TEXT("Roof"), TEXT("BuildingWindowOpaque"), TEXT("CivicBrick_Police"), TEXT("CivicAsphaltRoof")},
	     true, 672, 0.0000},
	    {TEXT("/Game/CitySim/Art/Meshes/SM_Building_Fire_A.SM_Building_Fire_A"),
	     FVector(2400.0000, 1840.0000, 940.0000),
	     {TEXT("Fire"), TEXT("Roof"), TEXT("BuildingWindowOpaque"), TEXT("CivicBrick_Fire"), TEXT("CivicAsphaltRoof")},
	     true, 772, 0.0000},
	    {TEXT("/Game/CitySim/Art/Meshes/SM_Traffic_PoliceCar_A.SM_Traffic_PoliceCar_A"),
	     FVector(380, 210, 187),
	     {TEXT("Police"), TEXT("Roof")}},
	    {TEXT("/Game/CitySim/Art/Meshes/SM_Traffic_FireEngine_A.SM_Traffic_FireEngine_A"),
	     FVector(620, 210, 278),
	     {TEXT("Fire"), TEXT("Roof")}},
	    {TEXT("/Game/CitySim/Art/Meshes/SM_Calibration_1m.SM_Calibration_1m"),
	     FVector(100.0, 100.0, 100.0),
	     {TEXT("Trim")}},
	    {TEXT("/Game/CitySim/Art/Meshes/SM_Building_Residential_A.SM_Building_Residential_A"),
	     FVector(1055.9999, 1327.9998, 580.0000),
	     {TEXT("ResidentialBrick"), TEXT("HomeSolidDetails"), TEXT("RoofShingles"), TEXT("HomeWindowOpaque")},
	     true, 668, -20.0000},
	    {TEXT("/Game/CitySim/Art/Meshes/SM_Building_Residential_Medium_A.SM_Building_Residential_Medium_A"),
	     FVector(1442.9999, 1761.5000, 2200.0000),
	     {TEXT("ResidentialBrick"), TEXT("Roof"), TEXT("RoofMembrane"), TEXT("BuildingWindowOpaque")},
	     true, 2138, 0.0000},
	    {TEXT("/Game/CitySim/Art/Meshes/SM_Building_Residential_High_A.SM_Building_Residential_High_A"),
	     FVector(1400.0000, 1740.0000, 4100.0000),
	     {TEXT("ResidentialBrick"), TEXT("Roof"), TEXT("Mechanical"), TEXT("RoofMembrane"), TEXT("BuildingWindowOpaque")},
	     true, 3854, 0.0000},
	    {TEXT("/Game/CitySim/Art/Meshes/SM_Building_Residential_Construction_A.SM_Building_Residential_Construction_A"),
	     FVector(1500.0, 1790.0, 2000.0),
	     {TEXT("Roof"), TEXT("Trim")}},
	    {TEXT("/Game/CitySim/Art/Meshes/SM_Construction_Crane_Jib_A.SM_Construction_Crane_Jib_A"),
	     FVector(1450.0, 180.0, 180.0),
	     {TEXT("Roof"), TEXT("Trim")},
	     false},
	    {TEXT("/Game/CitySim/Art/Meshes/SM_Construction_Crane_Hook_A.SM_Construction_Crane_Hook_A"),
	     FVector(100.0, 80.0, 138.0),
	     {TEXT("Roof"), TEXT("Trim")},
	     false},
	    {TEXT("/Game/CitySim/Art/Meshes/SM_Building_Shop_A.SM_Building_Shop_A"),
	     FVector(1542.9999, 1761.5000, 1200.0000),
	     {TEXT("CommercialBrick"), TEXT("Roof"), TEXT("RoofMembrane"), TEXT("StoreSignTall"), TEXT("BuildingWindowOpaque")},
	     true, 470, 0.0000},
	    {TEXT("/Game/CitySim/Art/Meshes/SM_Building_Shop_Medium_A.SM_Building_Shop_Medium_A"),
	     FVector(1542.9999, 1761.5000, 2300.0000),
	     {TEXT("CommercialBrick"), TEXT("Roof"), TEXT("RoofMembrane"), TEXT("StoreSignWide"), TEXT("BuildingWindowOpaque")},
	     true, 1682, 0.0000},
	    {TEXT("/Game/CitySim/Art/Meshes/SM_Building_Shop_High_A.SM_Building_Shop_High_A"),
	     FVector(1542.9999, 1761.5000, 3900.0000),
	     {TEXT("CommercialBrick"), TEXT("Roof"), TEXT("RoofMembrane"), TEXT("StoreSignWide"), TEXT("BuildingWindowOpaque")},
	     true, 2906, 0.0000},
	    {TEXT("/Game/CitySim/Art/Meshes/SM_Building_Factory_A.SM_Building_Factory_A"),
	     FVector(1600.0000, 1790.0000, 1700.0000),
	     {TEXT("IndustrialBrick"), TEXT("Roof"), TEXT("IndustrialMechanical"), TEXT("RoofMetal"), TEXT("BuildingWindowOpaque")},
	     true, 616, 0.0000},
	    {TEXT("/Game/CitySim/Art/Meshes/SM_Building_Factory_Medium_A.SM_Building_Factory_Medium_A"),
	     FVector(1629.7543, 1804.8771, 2800.0000),
	     {TEXT("IndustrialBrick"), TEXT("Roof"), TEXT("IndustrialMechanical"), TEXT("RoofMetal"), TEXT("BuildingWindowOpaque")},
	     true, 1914, 0.0000},
	    {TEXT("/Game/CitySim/Art/Meshes/SM_Building_Factory_High_A.SM_Building_Factory_High_A"),
	     FVector(1629.7543, 1804.8771, 4100.0000),
	     {TEXT("IndustrialBrick"), TEXT("Roof"), TEXT("IndustrialMechanical"), TEXT("RoofMetal"), TEXT("BuildingWindowOpaque")},
	     true, 2332, 0.0000},
	    {TEXT("/Game/CitySim/Art/Meshes/SM_Building_Power_A.SM_Building_Power_A"),
	     FVector(1600.0000, 1840.0000, 1604.0001),
	     {TEXT("Power"), TEXT("Trim"), TEXT("CivicBrick_Power"), TEXT("CivicAsphaltRoof")},
	     true, 540, 0.0000},
	    {TEXT("/Game/CitySim/Art/Meshes/SM_Building_Service_A.SM_Building_Service_A"),
	     FVector(1600.0000, 1740.0000, 1300.0000),
	     {TEXT("Service"), TEXT("Trim"), TEXT("CivicBrick_Service"), TEXT("CivicAsphaltRoof")},
	     true, 2228, 0.0000},
	    {TEXT("/Game/CitySim/Art/Meshes/SM_Building_Park_A.SM_Building_Park_A"),
	     FVector(2200.0, 2200.0, 735.0),
	     {TEXT("Park"), TEXT("Trim"), TEXT("Bark"), TEXT("Canopy")}},
	    {TEXT("/Game/CitySim/Art/Meshes/SM_Road_Straight.SM_Road_Straight"),
	     FVector(320.0, 840.0, 18.0),
	     {TEXT("Road")}},
	    {TEXT("/Game/CitySim/Art/Meshes/SM_Road_Corner.SM_Road_Corner"), FVector(580.0, 580.0, 18.0), {TEXT("Road")}},
	    {TEXT("/Game/CitySim/Art/Meshes/SM_Road_T.SM_Road_T"), FVector(840.0, 580.0, 18.0), {TEXT("Road")}},
	    {TEXT("/Game/CitySim/Art/Meshes/SM_Road_Cross.SM_Road_Cross"), FVector(840.0, 840.0, 18.0), {TEXT("Road")}},
	};

	for (const FExpectedCityMesh& Item : Expected)
	{
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, Item.Path);
		if (!TestNotNull(FString::Printf(TEXT("Mesh loads: %s"), Item.Path), Mesh))
		{
			continue;
		}
		const FBox Bounds = Mesh->GetBoundingBox();
		TestTrue(FString::Printf(TEXT("Dimensions match manifest: %s"), Item.Path),
		         Bounds.GetSize().Equals(Item.DimensionsCm, 1.0));
		if (Item.bExpectGroundPivot)
		{
			TestTrue(FString::Printf(TEXT("Authored ground/walk pivot: %s"), Item.Path),
			         FMath::IsNearlyEqual(Bounds.Min.Z, Item.ExpectedMinimumZCm, 0.5));
		}
		const FString AssetPath(Item.Path);
		if (AssetPath.Contains(TEXT("Police")) || AssetPath.Contains(TEXT("Fire")))
		{
			const int32 Budget = AssetPath.Contains(TEXT("Building_")) ? 800 : 500;
			TestTrue(TEXT("Civic meshes stay within station/vehicle budgets"),
			         Mesh->GetNumTriangles(0) > 0 && Mesh->GetNumTriangles(0) <= Budget);
			TestEqual(TEXT("Civic mesh retains reviewed sections"), Mesh->GetNumSections(0), Item.Slots.Num());
			if (AssetPath.Contains(TEXT("Fire")))
			{
				const FColorVertexBuffer& Colors =
				    Mesh->GetRenderData()->LODResources[0].VertexBuffers.ColorVertexBuffer;
				int32 Lights = 0;
				for (uint32 V = 0; V < Colors.GetNumVertices(); ++V)
					Lights += Colors.VertexColor(V).A == 0 ? 1 : 0;
				TestTrue(TEXT("Fire roof lights survive FBX colors"), Lights > 0);
				const auto* Detail = Mesh->GetStaticMaterials().FindByPredicate(
				    [](const auto& M) { return M.MaterialSlotName == TEXT("Roof"); });
				TestTrue(TEXT("Fire roof lights use emissive civic detail material"),
				         Detail && Detail->MaterialInterface &&
				             Detail->MaterialInterface->GetName() == TEXT("MI_CityFireDetail"));
			}
			if (AssetPath.Contains(TEXT("Police")))
			{
				const FColorVertexBuffer& Colors =
				    Mesh->GetRenderData()->LODResources[0].VertexBuffers.ColorVertexBuffer;
				int32 Red = 0, Blue = 0;
				for (uint32 Index = 0; Index < Colors.GetNumVertices(); ++Index)
				{
					const FColor C = Colors.VertexColor(Index);
					Red += C.A == 0 && C.R > C.B * 2 ? 1 : 0;
					Blue += C.A == 0 && C.B > C.R * 2 ? 1 : 0;
				}
				TestTrue(TEXT("Police red/blue roof caps survive FBX vertex colors"), Red > 0 && Blue > 0);
			}
		}
		const bool bArchitecture =
		    !AssetPath.Contains(TEXT("Construction")) &&
		    (AssetPath.Contains(TEXT("Building_Residential")) || AssetPath.Contains(TEXT("Building_Shop")) ||
		     AssetPath.Contains(TEXT("Building_Factory")));
		if (AssetPath.Contains(TEXT("Construction")))
		{
			TestTrue(TEXT("construction details stay within pooled geometry budget"),
			         Mesh->GetNumTriangles(0) > 0 && Mesh->GetNumTriangles(0) <= 500);
			TestTrue(TEXT("construction details retain at most two sections"), Mesh->GetNumSections(0) <= 2);
			if (AssetPath.Contains(TEXT("Crane_Hook")))
				TestTrue(TEXT("hook retains top cable attachment pivot"), FMath::IsNearlyZero(Bounds.Max.Z, .1));
		}
		if (AssetPath.Contains(TEXT("Building_Power")) || AssetPath.Contains(TEXT("Building_Service")))
		{
			TestEqual(TEXT("Utility retains accepted BuildingSurfaces detail count"), Mesh->GetNumTriangles(0),
			          Item.ExpectedTriangles);
			TestEqual(TEXT("Utility retains reviewed surface sections"), Mesh->GetNumSections(0), Item.Slots.Num());
		}
		if (bArchitecture)
		{
			TestEqual(TEXT("Building retains approved SeasonalBuildings detail count"), Mesh->GetNumTriangles(0),
			          Item.ExpectedTriangles);
			TestEqual(TEXT("Building retains its specific surface sections"), Mesh->GetNumSections(0),
			          Item.Slots.Num());
			const FColorVertexBuffer& Colors = Mesh->GetRenderData()->LODResources[0].VertexBuffers.ColorVertexBuffer;
			int32 WindowVertices = 0, SolidVertices = 0;
			for (uint32 Vertex = 0; Vertex < Colors.GetNumVertices(); ++Vertex)
			{
				WindowVertices += Colors.VertexColor(Vertex).A == 0;
				SolidVertices += Colors.VertexColor(Vertex).A == 255;
			}
			TestTrue(FString::Printf(TEXT("FBX retains separate window and non-window vertex masks: %s"), Item.Path),
			         WindowVertices > 0 && SolidVertices > 0);
		}
		const TArray<FStaticMaterial>& Materials = Mesh->GetStaticMaterials();
		TestEqual(FString::Printf(TEXT("Slot count: %s"), Item.Path), Materials.Num(), Item.Slots.Num());
		for (const FName Slot : Item.Slots)
		{
			const FStaticMaterial* Material = Materials.FindByPredicate(
			    [Slot](const FStaticMaterial& Candidate) { return Candidate.ImportedMaterialSlotName == Slot; });
			TestNotNull(FString::Printf(TEXT("Semantic slot %s: %s"), *Slot.ToString(), Item.Path), Material);
			if (Material)
			{
				TestNotNull(FString::Printf(TEXT("Remapped material %s: %s"), *Slot.ToString(), Item.Path),
				            Material->MaterialInterface.Get());
				const FString MaterialPath =
				    Material->MaterialInterface ? Material->MaterialInterface->GetPathName() : FString();
				if (bArchitecture && (Slot == TEXT("Trim") || Slot == TEXT("Roof")))
					TestEqual(TEXT("Building details retain shared masked-window material after reimport"),
					          MaterialPath, FString(TEXT("/Game/CitySim/Art/Materials/MI_Roof.MI_Roof")));

				TestTrue(
				    FString::Printf(TEXT("Uses shared city material %s: %s (actual %s)"), *Slot.ToString(), Item.Path,
				                    *MaterialPath),
				    bArchitecture ? MaterialPath == FString::Printf(
				                        TEXT("/Game/CitySim/Art/Materials/%s_%s.%s_%s"),
				                        Slot.ToString().Contains(TEXT("Opaque")) || Slot == TEXT("HomeSolidDetails") ? TEXT("M") : TEXT("MI"),
				                        *Slot.ToString(),
				                        Slot.ToString().Contains(TEXT("Opaque")) || Slot == TEXT("HomeSolidDetails") ? TEXT("M") : TEXT("MI"),
				                        *Slot.ToString())
				    : (Slot == TEXT("Bark") || Slot == TEXT("Canopy"))
				        ? MaterialPath ==
				              FString::Printf(TEXT("/Game/CitySim/Art/Materials/MI_Environment%s.MI_Environment%s"),
				                              *Slot.ToString(), *Slot.ToString())
				        : MaterialPath.StartsWith(TEXT("/Game/CitySim/Art/Materials/")));
			}
		}
	}

	UCityBuildingCatalog* Catalog = LoadObject<UCityBuildingCatalog>(
	    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
	if (TestNotNull(TEXT("Building catalog loads"), Catalog))
	{
		for (const FName DefinitionId : {FName(TEXT("Home")), FName(TEXT("Shop")), FName(TEXT("Factory")),
		                                 FName(TEXT("Power")), FName(TEXT("Service"))})
		{
			const FCityBuildingDefinition* Definition = Catalog->FindDefinition(DefinitionId);
			if (TestNotNull(FString::Printf(TEXT("Authored definition exists: %s"), *DefinitionId.ToString()),
			                Definition))
			{
				TestEqual(FString::Printf(TEXT("Logical entrance mask is local -Y: %s"), *DefinitionId.ToString()),
				          Definition->EntranceSides, static_cast<int32>(ECityBuildingEntranceSide::NegativeY));
				TestEqual(FString::Printf(TEXT("FBX frontage correction is two turns: %s"), *DefinitionId.ToString()),
				          Definition->PresentationYawOffsetQuarterTurns, 2);
			}
		}
		const FCityBuildingDefinition* Home = Catalog->FindDefinition(TEXT("Home"));
		if (TestNotNull(TEXT("Residential development definition exists"), Home))
		{
			TestEqual(TEXT("Residential construction takes three game days after delivery"), Home->ConstructionSteps,
			          int64(3));
			TestTrue(TEXT("Residential construction mesh is authored"), !Home->ConstructionMesh.IsNull());
			TestEqual(TEXT("Residential upgrades use residential demand"), Home->DevelopmentDemandType,
			          ECityDevelopmentDemandType::Residential);
			TestEqual(TEXT("Residential has three density levels"), Home->DensityLevels.Num(), 3);
			const FCityBuildingLevelDefinition* Low = Home->FindDensityLevel(ECityDensityLevel::Low);
			const FCityBuildingLevelDefinition* Medium = Home->FindDensityLevel(ECityDensityLevel::Medium);
			const FCityBuildingLevelDefinition* High = Home->FindDensityLevel(ECityDensityLevel::High);
			TestNotNull(TEXT("Low-density data exists"), Low);
			TestNotNull(TEXT("Medium-density data exists"), Medium);
			TestNotNull(TEXT("High-density data exists"), High);
			if (Low && Medium && High)
			{
				TestEqual(TEXT("Low-density capacity"), Low->ResidentsProvided, 10);
				TestEqual(TEXT("Medium-density capacity"), Medium->ResidentsProvided, 20);
				TestEqual(TEXT("High-density capacity"), High->ResidentsProvided, 40);
				TestTrue(TEXT("Density meshes are authored independently"),
				         Low->PresentationMesh != Medium->PresentationMesh &&
				             Medium->PresentationMesh != High->PresentationMesh);
			}
		}
		for (const TPair<FName, ECityDevelopmentDemandType>& Family :
		     {TPair<FName, ECityDevelopmentDemandType>(TEXT("Shop"), ECityDevelopmentDemandType::Commercial),
		      TPair<FName, ECityDevelopmentDemandType>(TEXT("Factory"), ECityDevelopmentDemandType::Industrial)})
		{
			const FCityBuildingDefinition* Definition = Catalog->FindDefinition(Family.Key);
			if (TestNotNull(FString::Printf(TEXT("Density family exists: %s"), *Family.Key.ToString()), Definition))
			{
				TestEqual(TEXT("Family uses its own demand channel"), Definition->DevelopmentDemandType, Family.Value);
				TestEqual(TEXT("Private family construction takes three game days"), Definition->ConstructionSteps,
				          int64(3));
				TestEqual(TEXT("Family has low/medium/high levels"), Definition->DensityLevels.Num(), 3);
				const FCityBuildingLevelDefinition* Low = Definition->FindDensityLevel(ECityDensityLevel::Low);
				const FCityBuildingLevelDefinition* Medium = Definition->FindDensityLevel(ECityDensityLevel::Medium);
				const FCityBuildingLevelDefinition* High = Definition->FindDensityLevel(ECityDensityLevel::High);
				if (TestNotNull(TEXT("Low level exists"), Low) && TestNotNull(TEXT("Medium level exists"), Medium) &&
				    TestNotNull(TEXT("High level exists"), High))
				{
					TestTrue(TEXT("Jobs increase by density"),
					         Low->JobsProvided < Medium->JobsProvided && Medium->JobsProvided < High->JobsProvided);
					TestTrue(TEXT("Output increases by density"), Low->EconomicOutput < Medium->EconomicOutput &&
					                                                  Medium->EconomicOutput < High->EconomicOutput);
					TestTrue(TEXT("Power increases by density"),
					         Low->PowerDemand < Medium->PowerDemand && Medium->PowerDemand < High->PowerDemand);
					TestTrue(TEXT("Water increases by density"),
					         Low->WaterDemand < Medium->WaterDemand && Medium->WaterDemand < High->WaterDemand);
					TestTrue(TEXT("Upkeep increases by density"),
					         Low->MonthlyUpkeep < Medium->MonthlyUpkeep && Medium->MonthlyUpkeep < High->MonthlyUpkeep);
					TestTrue(TEXT("Density meshes are distinct"),
					         Low->PresentationMesh != Medium->PresentationMesh &&
					             Medium->PresentationMesh != High->PresentationMesh);
				}
			}
		}
	}

	const TArray<TPair<const TCHAR*, FLinearColor>> ExpectedMaterialColors = {
	    {TEXT("/Game/CitySim/Art/Materials/MI_CityResidential.MI_CityResidential"), FLinearColor(0.22f, 0.58f, 0.32f)},
	    {TEXT("/Game/CitySim/Art/Materials/MI_CityCommercial.MI_CityCommercial"), FLinearColor(0.12f, 0.42f, 0.78f)},
	    {TEXT("/Game/CitySim/Art/Materials/MI_CityIndustrial.MI_CityIndustrial"), FLinearColor(0.86f, 0.64f, 0.10f)},
	    {TEXT("/Game/CitySim/Art/Materials/MI_CityPower.MI_CityPower"), FLinearColor(0.70f, 0.22f, 0.08f)},
	    {TEXT("/Game/CitySim/Art/Materials/MI_CityService.MI_CityService"), FLinearColor(0.08f, 0.52f, 0.58f)},
	    {TEXT("/Game/CitySim/Art/Materials/MI_CityPark.MI_CityPark"), FLinearColor(0.16f, 0.58f, 0.20f)},
	    {TEXT("/Game/CitySim/Art/Materials/MI_CityRoof.MI_CityRoof"), FLinearColor(0.10f, 0.14f, 0.19f)},
	    {TEXT("/Game/CitySim/Art/Materials/MI_CityWindowDetail.MI_CityWindowDetail"),
	     FLinearColor(0.10f, 0.14f, 0.19f)},
	    {TEXT("/Game/CitySim/Art/Materials/MI_CityRoad.MI_CityRoad"), FLinearColor(0.055f, 0.065f, 0.08f)},
	    {TEXT("/Game/CitySim/Art/Materials/MI_CityRoadMarking.MI_CityRoadMarking"), FLinearColor(0.92f, 0.78f, 0.28f)},
	    {TEXT("/Game/CitySim/Art/Materials/MI_CityTrim.MI_CityTrim"), FLinearColor(0.95f, 0.62f, 0.16f)},
	};
	for (const TPair<const TCHAR*, FLinearColor>& ExpectedMaterial : ExpectedMaterialColors)
	{
		UMaterialInterface* Instance = LoadObject<UMaterialInterface>(nullptr, ExpectedMaterial.Key);
		if (TestNotNull(FString::Printf(TEXT("Material loads: %s"), ExpectedMaterial.Key), Instance))
		{
			UMaterial* MasterMaterial = Instance->GetMaterial();
			TestEqual(FString::Printf(TEXT("Uses CitySim master material: %s"), ExpectedMaterial.Key),
			          MasterMaterial ? MasterMaterial->GetPathName() : FString(),
			          FString(FString(ExpectedMaterial.Key).Contains(TEXT("WindowDetail"))
			                      ? TEXT("/Game/CitySim/Art/Materials/M_CityWindowDetail.M_CityWindowDetail")
			                      : TEXT("/Game/CitySim/Art/Materials/M_CityColor.M_CityColor")));
			TestTrue(FString::Printf(TEXT("Master supports instanced static meshes: %s"), ExpectedMaterial.Key),
			         MasterMaterial && MasterMaterial->GetUsageByFlag(MATUSAGE_InstancedStaticMeshes));
			FLinearColor Actual;
			TestTrue(FString::Printf(TEXT("SurfaceColor override exists: %s"), ExpectedMaterial.Key),
			         Instance->GetVectorParameterValue(FHashedMaterialParameterInfo(TEXT("CityColor")), Actual, true));
			TestTrue(FString::Printf(TEXT("Authored color persists: %s"), ExpectedMaterial.Key),
			         Actual.Equals(ExpectedMaterial.Value, 0.001f));
		}
	}
	UMaterial* OverlayFadeMaster =
	    LoadObject<UMaterial>(nullptr, TEXT("/Game/CitySim/Art/Materials/M_CityOverlayFade.M_CityOverlayFade"));
	if (TestNotNull(TEXT("Overlay fade master loads"), OverlayFadeMaster))
	{
		TestEqual(TEXT("Overlay fade is translucent"), OverlayFadeMaster->GetBlendMode(), BLEND_Translucent);
		TestEqual(TEXT("Overlay fade is unlit"), OverlayFadeMaster->GetShadingModels(),
		          FMaterialShadingModelField(MSM_Unlit));
		TestTrue(TEXT("Overlay fade supports instanced meshes"),
		         OverlayFadeMaster->GetUsageByFlag(MATUSAGE_InstancedStaticMeshes));
	}
	for (const TCHAR* Variant :
	     {TEXT("MI_CityDefault_OverlayFade"), TEXT("MI_CityFacade_OverlayFade"), TEXT("MI_CityResidential_OverlayFade"),
	      TEXT("MI_CityCommercial_OverlayFade"), TEXT("MI_CityPower_OverlayFade"), TEXT("MI_CityService_OverlayFade"),
	      TEXT("MI_CityRoof_OverlayFade"), TEXT("MI_CityRoad_OverlayFade"), TEXT("MI_CityTrim_OverlayFade")})
	{
		UMaterialInterface* FadeInstance = LoadObject<UMaterialInterface>(
		    nullptr, *FString::Printf(TEXT("/Game/CitySim/Art/Materials/%s.%s"), Variant, Variant));
		TestNotNull(FString::Printf(TEXT("Overlay fade variant loads: %s"), Variant), FadeInstance);
		TestEqual(FString::Printf(TEXT("Overlay fade variant uses shared master: %s"), Variant),
		          FadeInstance && FadeInstance->GetMaterial() ? FadeInstance->GetMaterial()->GetPathName() : FString(),
		          FString(TEXT("/Game/CitySim/Art/Materials/M_CityOverlayFade.M_CityOverlayFade")));
	}
	UMaterialInterface* TerrainInstance =
	    LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/CitySim/Art/Materials/MI_CityTerrain.MI_CityTerrain"));
	if (TestNotNull(TEXT("Terrain material instance loads"), TerrainInstance))
	{
		TestEqual(TEXT("Terrain material uses project-owned master rather than template grid"),
		          TerrainInstance->GetMaterial() ? TerrainInstance->GetMaterial()->GetPathName() : FString(),
		          FString(TEXT("/Game/CitySim/Art/Materials/M_CityTerrain.M_CityTerrain")));
		UTexture* Variation = nullptr;
		TestTrue(TEXT("terrain variation texture is referenced by the material for cooking"),
		         TerrainInstance->GetTextureParameterValue(FHashedMaterialParameterInfo(TEXT("TerrainVariation")),
		                                                   Variation));
		if (UTexture2D* Detail = Cast<UTexture2D>(Variation))
		{
#if WITH_EDITORONLY_DATA
			TestEqual(TEXT("terrain mask remains small"), Detail->Source.GetSizeX(), int64(128));
			TestTrue(TEXT("terrain mask allows mip generation"), Detail->MipGenSettings != TMGS_NoMipmaps);
#endif
			TestFalse(TEXT("terrain mask uses linear sampling"), Detail->SRGB);
		}
		else
			AddError(TEXT("Terrain variation must reference its 2D mask"));
		float GridWidth = 0.0f;
		TestTrue(
		    TEXT("Terrain logical-grid width override exists"),
		    TerrainInstance->GetScalarParameterValue(FHashedMaterialParameterInfo(TEXT("GridWidth")), GridWidth, true));
		TestTrue(TEXT("Terrain logical-grid guide remains subtle"), GridWidth > 0.0f && GridWidth <= 0.03f);
	}
	UMaterialInterface* WaterInstance =
	    LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/CitySim/Art/Materials/MI_CityWater.MI_CityWater"));
	if (TestNotNull(TEXT("Water material instance loads"), WaterInstance))
	{
		UMaterial* WaterMaster = WaterInstance->GetMaterial();
		TestEqual(TEXT("Water material uses project-owned master"),
		          WaterMaster ? WaterMaster->GetPathName() : FString(),
		          FString(TEXT("/Game/CitySim/Art/Materials/M_CityWater.M_CityWater")));
		if (WaterMaster)
		{
			TestEqual(TEXT("Water material is translucent"), WaterMaster->GetBlendMode(), BLEND_Translucent);
			TestEqual(TEXT("Water material is unlit"), WaterMaster->GetShadingModels(),
			          FMaterialShadingModelField(MSM_Unlit));
			TestTrue(TEXT("Water material is two-sided"), WaterMaster->IsTwoSided());
		}
	}
	UMaterial* IconMaster = LoadObject<UMaterial>(nullptr, TEXT("/Game/CitySim/UI/Icons/M_CityUiIcon.M_CityUiIcon"));
	if (TestNotNull(TEXT("UI icon master loads"), IconMaster))
	{
		TestEqual(TEXT("UI icon material is masked"), IconMaster->GetBlendMode(), BLEND_Masked);
		TestEqual(TEXT("UI icon material is unlit"), IconMaster->GetShadingModels(),
		          FMaterialShadingModelField(MSM_Unlit));
		TestTrue(TEXT("UI icon material is two-sided"), IconMaster->IsTwoSided());
	}
	for (const TCHAR* IconName : {TEXT("download"), TEXT("upload"), TEXT("play_arrow"), TEXT("restart_alt"),
	                              TEXT("arrow_back"), TEXT("close"), TEXT("settings"), TEXT("bar_chart")})
	{
		UMaterialInterface* IconInstance = LoadObject<UMaterialInterface>(
		    nullptr, *FString::Printf(TEXT("/Game/CitySim/UI/Icons/MI_Icon_%s.MI_Icon_%s"), IconName, IconName));
		TestNotNull(FString::Printf(TEXT("Material Symbol instance loads: %s"), IconName), IconInstance);
		TestEqual(FString::Printf(TEXT("Material Symbol uses shared unlit master: %s"), IconName),
		          IconInstance && IconInstance->GetMaterial() ? IconInstance->GetMaterial()->GetPathName() : FString(),
		          FString(TEXT("/Game/CitySim/UI/Icons/M_CityUiIcon.M_CityUiIcon")));
	}
	UMaterial* UiBarMaster = LoadObject<UMaterial>(nullptr, TEXT("/Game/CitySim/UI/M_CityUiBar.M_CityUiBar"));
	if (TestNotNull(TEXT("UI bar master loads"), UiBarMaster))
	{
		TestEqual(TEXT("UI bar material is opaque"), UiBarMaster->GetBlendMode(), BLEND_Opaque);
		TestEqual(TEXT("UI bar material is unlit"), UiBarMaster->GetShadingModels(),
		          FMaterialShadingModelField(MSM_Unlit));
	}
	for (const TCHAR* Variant : {TEXT("Rail"), TEXT("Residential"), TEXT("Commercial"), TEXT("Industrial")})
	{
		UMaterialInterface* BarInstance = LoadObject<UMaterialInterface>(
		    nullptr, *FString::Printf(TEXT("/Game/CitySim/UI/MI_CityUiBar%s.MI_CityUiBar%s"), Variant, Variant));
		TestNotNull(FString::Printf(TEXT("UI bar variant loads: %s"), Variant), BarInstance);
		TestEqual(FString::Printf(TEXT("UI bar variant uses shared unlit master: %s"), Variant),
		          BarInstance && BarInstance->GetMaterial() ? BarInstance->GetMaterial()->GetPathName() : FString(),
		          FString(TEXT("/Game/CitySim/UI/M_CityUiBar.M_CityUiBar")));
	}
	return true;
}

#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityCivicLightMaterialTest, "CitySim.Art.CivicLightEmission",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityCivicLightMaterialTest::RunTest(const FString& Parameters)
{
	UMaterial* Material =
	    LoadObject<UMaterial>(nullptr, TEXT("/Game/CitySim/Art/Materials/M_CityCivicDetail.M_CityCivicDetail"));
	if (!TestNotNull(TEXT("Civic light parent loads"), Material))
		return false;
	const auto* Brightness =
	    Cast<UMaterialExpressionMultiply>(Material->GetExpressionInputForProperty(MP_EmissiveColor)->Expression);
	const auto* Base =
	    Cast<UMaterialExpressionLinearInterpolate>(Material->GetExpressionInputForProperty(MP_BaseColor)->Expression);
	if (!TestNotNull(TEXT("Emission multiplier is connected"), Brightness) ||
	    !TestNotNull(TEXT("Lamp surface color is connected"), Base))
		return false;
	const auto* Glow = Cast<UMaterialExpressionMultiply>(Brightness->A.Expression);
	if (!TestNotNull(TEXT("Masked lamp color feeds emission"), Glow))
		return false;
	const auto* LinearColor = Cast<UMaterialExpressionPower>(Glow->A.Expression);
	if (!TestNotNull(TEXT("Lamp RGB is decoded from the mesh sRGB bytes"), LinearColor))
		return false;
	const auto* Vertex = Cast<UMaterialExpressionVertexColor>(LinearColor->Base.Expression);
	const auto* Mask = Cast<UMaterialExpressionOneMinus>(Glow->B.Expression);
	if (!TestNotNull(TEXT("Emission receives actual vertex color instead of constant zero"), Vertex) ||
	    !TestNotNull(TEXT("Emission has a lamp mask"), Mask))
		return false;
	TestTrue(TEXT("Emission reads the vertex RGB output"), LinearColor->Base.OutputIndex == 0 &&
	                                                           LinearColor->Base.MaskR && LinearColor->Base.MaskG &&
	                                                           LinearColor->Base.MaskB && !LinearColor->Base.MaskA &&
	                                                           FMath::IsNearlyEqual(LinearColor->ConstExponent, 2.2f));
	TestTrue(TEXT("Lamp mask reads inverse vertex alpha"),
	         Mask->Input.Expression == Vertex && Mask->Input.OutputIndex == 4 && Mask->Input.MaskA);
	const auto* Lens = Cast<UMaterialExpressionMultiply>(Base->B.Expression);
	TestTrue(TEXT("Off lenses retain dim non-emissive color"), Lens && Lens->A.Expression == LinearColor &&
	                                                               !Lens->B.Expression &&
	                                                               FMath::IsNearlyEqual(Lens->ConstB, .18f));
	TestTrue(TEXT("Surface uses the same lamp mask"), Base->Alpha.Expression == Mask);
	const auto* Blend = Cast<UMaterialExpressionLinearInterpolate>(Brightness->B.Expression);
	if (!TestNotNull(TEXT("Beacon brightness control is connected"), Blend))
		return false;
	const auto* Enabled = Cast<UMaterialExpressionScalarParameter>(Blend->Alpha.Expression);
	TestTrue(TEXT("Parked lamps have zero emission"), Enabled && Enabled->ParameterName == TEXT("BeaconPulse") &&
	                                                      Enabled->DefaultValue == 0 && !Blend->A.Expression &&
	                                                      Blend->ConstA == 0);
	const auto* OutputScale = Cast<UMaterialExpressionMultiply>(Blend->B.Expression);
	if (!TestNotNull(TEXT("Response brightness is connected"), OutputScale))
		return false;
	TestTrue(TEXT("Response brightness peaks at four"), !OutputScale->B.Expression && OutputScale->ConstB == 4);
	const auto* Pair = Cast<UMaterialExpressionLinearInterpolate>(OutputScale->A.Expression);
	if (!TestNotNull(TEXT("Lamp pair shares one curve"), Pair))
		return false;
	const auto* Red = Cast<UMaterialExpressionAdd>(Pair->A.Expression);
	const auto* Opposite = Cast<UMaterialExpressionOneMinus>(Pair->B.Expression);
	if (!TestNotNull(TEXT("Red ramp is connected"), Red) || !TestNotNull(TEXT("Opposite ramp is connected"), Opposite))
		return false;
	TestTrue(TEXT("Other lamp is the exact inverse of the same red ramp"), Opposite->Input.Expression == Red);
	const auto* Amplitude = Cast<UMaterialExpressionMultiply>(Red->A.Expression);
	if (!TestNotNull(TEXT("Shared ramp amplitude is connected"), Amplitude))
		return false;
	const auto* Sine = Cast<UMaterialExpressionSine>(Amplitude->A.Expression);
	if (!TestNotNull(TEXT("One shared oscillator drives both lamps"), Sine))
		return false;
	const auto* Frequency = Cast<UMaterialExpressionMultiply>(Sine->Input.Expression);
	if (!TestNotNull(TEXT("Render-time frequency is connected"), Frequency))
		return false;
	TestTrue(TEXT("Shared curve ranges zero to one over two seconds"),
	         !Red->B.Expression && Red->ConstB == .5f && !Amplitude->B.Expression && Amplitude->ConstB == .5f &&
	             !Frequency->B.Expression && Frequency->ConstB == .5f && Sine->Period == 1 &&
	             Cast<UMaterialExpressionTime>(Frequency->A.Expression));
	const auto* SideClamp = Cast<UMaterialExpressionSaturate>(Pair->Alpha.Expression);
	const auto* SideScale = SideClamp ? Cast<UMaterialExpressionMultiply>(SideClamp->Input.Expression) : nullptr;
	const auto* Side = SideScale ? Cast<UMaterialExpressionSubtract>(SideScale->A.Expression) : nullptr;
	if (!TestNotNull(TEXT("Actual lamp color selects the side"), Side))
		return false;
	TestTrue(TEXT("Side selector reads the actual blue channel"), Side->A.Expression == Vertex &&
	                                                                  Side->A.OutputIndex == 3 && Side->A.MaskB &&
	                                                                  !Side->B.Expression && !SideScale->B.Expression);
	// Read constants from the saved graph and sample the complete cycle, including
	// both zero crossings. The inverse connection above makes this an asset check.
	for (int32 Sample = 0; Sample <= 200; ++Sample)
	{
		const float Seconds = Sample * .01f;
		const float RedRamp =
		    Red->ConstB + Amplitude->ConstB * FMath::Sin(Seconds * Frequency->ConstB * 2 * PI / Sine->Period);
		const float RedBrightness = OutputScale->ConstB * RedRamp,
		            OtherBrightness = OutputScale->ConstB * (1 - RedRamp);
		TestTrue(TEXT("Pair has no dark gap anywhere in the cycle"),
		         RedBrightness >= -.0001f && OtherBrightness >= -.0001f &&
		             FMath::IsNearlyEqual(RedBrightness + OtherBrightness, 4.f, .0001f) &&
		             FMath::Max(RedBrightness, OtherBrightness) >= 1.9999f);
	}
	auto* Board = GetMutableDefault<ACityBoardActor>();
	for (const TCHAR* Service : {TEXT("Fire"), TEXT("Police")})
	{
		const FString ResponsePath = FString::Printf(
		    TEXT("/Game/CitySim/Art/Materials/MI_City%sResponseDetail.MI_City%sResponseDetail"), Service, Service);
		UMaterialInterface* Response = LoadObject<UMaterialInterface>(nullptr, *ResponsePath);
		if (!TestNotNull(TEXT("Moving response material loads"), Response))
			continue;
		float Pulse = 0;
		TestTrue(TEXT("Moving material uses repaired civic graph and enables pulse"),
		         Response->GetMaterial() == Material &&
		             Response->GetScalarParameterValue(FMaterialParameterInfo(TEXT("BeaconPulse")), Pulse) &&
		             Pulse == 1);
		const FName PoolName = *FString::Printf(TEXT("%sResponseInstances"), Service);
		const auto* Pool = Cast<UInstancedStaticMeshComponent>(Board->GetDefaultSubobjectByName(PoolName));
		if (!TestNotNull(TEXT("Actual response pool exists"), Pool))
			continue;
		UStaticMesh* Mesh = Pool->GetStaticMesh();
		if (!TestNotNull(TEXT("Response mesh exists"), Mesh))
			continue;
		const int32 RoofIndex = Mesh->GetMaterialIndex(TEXT("Roof"));
		if (!TestTrue(TEXT("Moving roof section exists"), RoofIndex != INDEX_NONE))
			continue;
		TestTrue(TEXT("Moving pool selects pulse material on roof section"), Pool->GetMaterial(RoofIndex) == Response);
		float ParkedPulse = -1;
		TestTrue(TEXT("Mesh's parked roof disables emission"),
		         Mesh->GetMaterial(RoofIndex)->GetScalarParameterValue(FMaterialParameterInfo(TEXT("BeaconPulse")),
		                                                               ParkedPulse) &&
		             ParkedPulse == 0);
		const auto& Lod = Mesh->GetRenderData()->LODResources[0];
		int32 ColoredLights = 0, RedLights = 0, OtherLights = 0;
		for (const auto& Section : Lod.Sections)
			if (Section.MaterialIndex == RoofIndex)
				for (uint32 I = Section.FirstIndex; I < Section.FirstIndex + Section.NumTriangles * 3; ++I)
				{
					const FColor C = Lod.VertexBuffers.ColorVertexBuffer.VertexColor(Lod.IndexBuffer.GetIndex(I));
					if (C.A == 0 && (C.R > 128 || C.B > 128))
					{
						++ColoredLights;
						const bool bRed = C.R > C.B * 2;
						const float Selection =
						    FMath::Clamp((C.B / 255.f - Side->ConstB) * SideScale->ConstB, 0.f, 1.f);
						TestTrue(TEXT("Imported red and blue/white lenses use strictly opposite ramps"),
						         Selection == (bRed ? 0.f : 1.f));
						if (bRed)
							++RedLights;
						else
							++OtherLights;
					}
				}
		TestTrue(TEXT("Both lamp colors belong to the emissive roof section"),
		         ColoredLights > 0 && RedLights > 0 && OtherLights > 0);
	}
	return true;
}

#endif // WITH_EDITOR

#endif
