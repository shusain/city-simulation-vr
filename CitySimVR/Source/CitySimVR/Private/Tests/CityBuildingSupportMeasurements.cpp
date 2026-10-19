#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Presentation/CityBuildingSupports.h"
#include "Simulation/CityBuildingDefinition.h"
#include "StaticMeshResources.h"

// Read-only measurement of approved native meshes. The baked runtime table is
// deliberately separate from source authoring and requires no cooked CPU mesh.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityBuildingSupportMeasureTest, "CitySim.Terrain.BuildingSupportMeasurements",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityBuildingSupportMeasureTest::RunTest(const FString&)
{
	auto* Catalog = LoadObject<UCityBuildingCatalog>(nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
	if (!TestNotNull(TEXT("Support measurement catalog"), Catalog)) return false;
	TMap<FString, FName> Meshes;
	auto Add = [&](const TSoftObjectPtr<UStaticMesh>& Mesh, FName Definition) {
		if (!Mesh.IsNull()) Meshes.Add(Mesh.ToSoftObjectPath().GetLongPackageName(), Definition); };
	for (const auto& D : Catalog->Definitions)
	{
		// Parks/sports are deliberately graded sites, not rigid buildings.
		if (D.DefinitionId.ToString().Contains(TEXT("Park"))) continue;
		Add(D.PresentationMesh, D.DefinitionId);
		for (const auto& L : D.DensityLevels) Add(L.PresentationMesh, D.DefinitionId);
	}
	for (const auto& V : Catalog->CommercialPresentationVariants)
		for (const auto& M : V.DensityMeshes) Add(M, TEXT("Shop"));
	for (const auto& V : Catalog->LowDensityPresentationVariants)
	{
		Add(V.PresentationMesh, V.DefinitionId);
		for (const auto& Pair : V.BusinessMeshes) Add(Pair.Value, V.DefinitionId);
	}
	Meshes.Add(TEXT("/Game/CitySim/Art/Meshes/SM_Building_Residential_SeasonalSpace_A"), TEXT("Home"));
	Meshes.Add(TEXT("/Game/CitySim/Art/HomeGroundPilot/SM_CompactHome_GroundPilot"), TEXT("Home"));
	TArray<FString> Names; Meshes.GetKeys(Names); Names.Sort();
	TArray<TSharedPtr<FJsonValue>> Records;
	for (const auto& Name : Names)
	{
		auto* Mesh = LoadObject<UStaticMesh>(nullptr, *Name);
		if (!TestNotNull(*Name, Mesh) || !Mesh->GetRenderData() || Mesh->GetRenderData()->LODResources.IsEmpty()) continue;
		const auto& Lod = Mesh->GetRenderData()->LODResources[0];
		const int32 Count = Lod.VertexBuffers.PositionVertexBuffer.GetNumVertices();
		const auto Indices = Lod.IndexBuffer.GetArrayView();
		TArray<FVector> Vertices; TArray<int32> Parent; TMap<FIntVector, int32> Weld;
		Vertices.Reserve(Count); Parent.Reserve(Count);
		for (int32 I = 0; I < Count; ++I)
		{
			const FVector P(Lod.VertexBuffers.PositionVertexBuffer.VertexPosition(I)); Vertices.Add(P); Parent.Add(I);
			const FIntVector Key(FMath::RoundToInt(P.X * 10), FMath::RoundToInt(P.Y * 10), FMath::RoundToInt(P.Z * 10));
			if (const int32* Existing = Weld.Find(Key)) Parent[I] = *Existing; else Weld.Add(Key, I);
		}
		auto Root = [&](int32 I) { while (Parent[I] != I) { Parent[I] = Parent[Parent[I]]; I = Parent[I]; } return I; };
		for (int32 I = 0; I < Indices.Num(); I += 3)
		{
			const int32 A = Root(Indices[I]); Parent[Root(Indices[I + 1])] = A; Parent[Root(Indices[I + 2])] = A;
		}
		TMap<int32, FBox> Islands;
		for (int32 I = 0; I < Count; ++I)
		{
			const int32 R = Root(I);
			if (auto* Box = Islands.Find(R)) *Box += Vertices[I]; else Islands.Add(R, FBox(Vertices[I], Vertices[I]));
		}
		const double MinimumHeight = Meshes[Name] == TEXT("Home") ? 180 : Meshes[Name] == TEXT("SolarPower") ? 100 : 350;
		TArray<TSharedPtr<FJsonValue>> Regions, Parts;
		TSet<FString> Unique;
		int32 ExcludedLowFloors = 0;
		auto HasStructuralHeight = [&](int32 Island, double X, double Y, double Floor) {
			double Top = Floor;
			for (int32 I = 0; I < Indices.Num(); I += 3)
			{
				if (Root(Indices[I]) != Island) continue;
				const FVector A = Vertices[Indices[I]], C = Vertices[Indices[I + 1]], D = Vertices[Indices[I + 2]];
				const double Denominator = (C.Y - D.Y) * (A.X - D.X) + (D.X - C.X) * (A.Y - D.Y);
				if (FMath::Abs(Denominator) < .001) continue;
				const double U = ((C.Y - D.Y) * (X - D.X) + (D.X - C.X) * (Y - D.Y)) / Denominator;
				const double V = ((D.Y - A.Y) * (X - D.X) + (A.X - D.X) * (Y - D.Y)) / Denominator;
				if (U >= -.0001 && V >= -.0001 && U + V <= 1.0001)
					Top = FMath::Max(Top, U * A.Z + V * C.Z + (1 - U - V) * D.Z);
			}
			// The island has already passed the family height gate. A local
			// meter of structure distinguishes floors/posts from thin connected paving.
			return Top - Floor >= 100;
		};
		for (const auto& Part : Islands)
		{
			const FBox& B = Part.Value;
			if (B.Min.Z > 80 || B.GetSize().Z < MinimumHeight || B.GetSize().X < 5 || B.GetSize().Y < 5) continue;
			auto Info = MakeShared<FJsonObject>();
			Info->SetNumberField(TEXT("width"), B.GetSize().X); Info->SetNumberField(TEXT("depth"), B.GetSize().Y);
			Info->SetNumberField(TEXT("height"), B.GetSize().Z); Info->SetNumberField(TEXT("bottom"), B.Min.Z);
			Parts.Add(MakeShared<FJsonValueObject>(Info));
			int32 BottomFaces = 0;
			auto Region = [&](double X0, double Y0, double X1, double Y1, bool Fallback) {
				const FString Key = FString::Printf(TEXT("%.1f,%.1f,%.1f,%.1f,%.1f"), X0, Y0, X1, Y1, B.Min.Z);
				if (Unique.Contains(Key)) return; Unique.Add(Key);
				auto R = MakeShared<FJsonObject>();
				R->SetNumberField(TEXT("x0"), X0); R->SetNumberField(TEXT("y0"), Y0); R->SetNumberField(TEXT("x1"), X1); R->SetNumberField(TEXT("y1"), Y1);
				R->SetNumberField(TEXT("bottom_z"), B.Min.Z); R->SetBoolField(TEXT("fallback"), Fallback);
				Regions.Add(MakeShared<FJsonValueObject>(R)); };
			for (int32 I = 0; I < Indices.Num(); I += 3)
			{
				if (Root(Indices[I]) != Part.Key) continue;
				const FVector A = Vertices[Indices[I]], C = Vertices[Indices[I + 1]], D = Vertices[Indices[I + 2]];
				if (FMath::Max3(A.Z, C.Z, D.Z) > B.Min.Z + 2) continue;
				const double X0 = FMath::Min3(A.X, C.X, D.X), X1 = FMath::Max3(A.X, C.X, D.X);
				const double Y0 = FMath::Min3(A.Y, C.Y, D.Y), Y1 = FMath::Max3(A.Y, C.Y, D.Y);
				if (X1 - X0 < 5 || Y1 - Y0 < 5) continue;
				++BottomFaces;
				// Older assets weld their thin walk into the door's tall island.
				// Test height over each floor triangle, not the island's global AABB.
				if (!HasStructuralHeight(Part.Key, (A.X + C.X + D.X) / 3, (A.Y + C.Y + D.Y) / 3, B.Min.Z))
				{ ++ExcludedLowFloors; continue; }
				Region(X0, Y0, X1, Y1, false);
			}
			if (!BottomFaces && HasStructuralHeight(Part.Key, B.GetCenter().X, B.GetCenter().Y, B.Min.Z))
				Region(B.Min.X, B.Min.Y, B.Max.X, B.Max.Y, true);
		}
		TestTrue(*(TEXT("Grounded structural supports measured: ") + Name), !Regions.IsEmpty());
		const auto Baked = CityBuildingSupports::Find(Name);
		TestTrue(*(TEXT("Cooked-safe support data exists: ") + Name), !Baked.IsEmpty());
		if (Name.Contains(TEXT("/Commercial/")) || Name.EndsWith(TEXT("SM_Building_Shop_A")) ||
		    Name.EndsWith(TEXT("SM_Building_Residential_Medium_A")) || Name.EndsWith(TEXT("SM_Building_Residential_High_A")))
		{
			TestTrue(TEXT("Legacy door-connected walk triangles rejected"), ExcludedLowFloors > 0);
			for (const auto& B : Baked)
				TestFalse(TEXT("Concrete must not extend beneath the legacy private walk toward the road"),
				          B.MinX <= 0 && B.MaxX >= 0 && B.MinY <= 950 && B.MaxY >= 950);
		}
		auto Contains = [](double X, double Y, double Z, double X0, double Y0, double X1, double Y1, double Floor) {
			return FMath::Abs(Z - Floor) < .02 && X >= X0 - .02 && X <= X1 + .02 && Y >= Y0 - .02 && Y <= Y1 + .02; };
		for (const auto& R : Regions)
		{
			const auto O = R->AsObject();
			for (const double X : {O->GetNumberField(TEXT("x0")), O->GetNumberField(TEXT("x1"))})
				for (const double Y : {O->GetNumberField(TEXT("y0")), O->GetNumberField(TEXT("y1"))})
				{
					bool Covered = false;
					for (const auto& B : Baked) Covered |= Contains(X, Y, O->GetNumberField(TEXT("bottom_z")), B.MinX, B.MinY, B.MaxX, B.MaxY, B.BottomZ);
					TestTrue(TEXT("Baked supports cover the measured structural floor"), Covered);
				}
		}
		for (const auto& B : Baked)
			for (const double X : {B.MinX, (B.MinX + B.MaxX) * .5, B.MaxX})
				for (const double Y : {B.MinY, (B.MinY + B.MaxY) * .5, B.MaxY})
				{
					bool Structural = false;
					for (const auto& R : Regions)
					{
						const auto O = R->AsObject();
						Structural |= Contains(X, Y, B.BottomZ, O->GetNumberField(TEXT("x0")), O->GetNumberField(TEXT("y0")), O->GetNumberField(TEXT("x1")), O->GetNumberField(TEXT("y1")), O->GetNumberField(TEXT("bottom_z")));
					}
					TestTrue(TEXT("Baked supports do not add outdoor paving/courtyard coverage"), Structural);
				}
		auto Record = MakeShared<FJsonObject>(); Record->SetStringField(TEXT("mesh"), Name);
		Record->SetStringField(TEXT("definition"), Meshes[Name].ToString()); Record->SetArrayField(TEXT("regions_cm"), Regions);
		Record->SetArrayField(TEXT("selected_parts_cm"), Parts); Record->SetNumberField(TEXT("vertices"), Count);
		Record->SetNumberField(TEXT("excluded_low_clearance_floor_triangles"), ExcludedLowFloors);
		Records.Add(MakeShared<FJsonValueObject>(Record));
	}
	FString Directory; FParse::Value(FCommandLine::Get(), TEXT("ReportExportPath="), Directory);
	if (Directory.IsEmpty()) Directory = FPaths::ProjectSavedDir() / TEXT("Automation");
	Directory /= TEXT("GroundPlotsBuildingSupports"); IFileManager::Get().MakeDirectory(*Directory, true);
	auto Document = MakeShared<FJsonObject>(); Document->SetArrayField(TEXT("assets"), Records);
	Document->SetStringField(TEXT("units"), TEXT("native mesh-local cm; grounded structural islands only, flat pads/paths excluded"));
	FString Json; const auto Writer = TJsonWriterFactory<>::Create(&Json); FJsonSerializer::Serialize(Document, Writer);
	TestTrue(TEXT("Native support measurements saved"), FFileHelper::SaveStringToFile(Json, *(Directory / TEXT("Measurements.json"))));
	AddInfo(FString::Printf(TEXT("CITYSIM_GROUNDPLOTS_SUPPORT_MEASUREMENTS Assets=%d Directory=%s"), Records.Num(), *Directory));
	return true;
}
#endif
