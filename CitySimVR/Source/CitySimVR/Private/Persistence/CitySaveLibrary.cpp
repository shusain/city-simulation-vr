#include "Persistence/CitySaveLibrary.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
namespace
{
// Archive metadata is read without instantiating or upgrading historical simulation state.
bool ReadMetadata(const FString& Json, const FString& ExpectedId, FCitySaveSlot& Slot)
{
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid())
		return false;
	FString Id, Name;
	if (!Root->TryGetStringField(TEXT("cityId"), Id) || !FCityIdentity::IsValidId(Id) ||
	    (!ExpectedId.IsEmpty() && Id != ExpectedId) || !Root->TryGetStringField(TEXT("cityName"), Name))
		return false;
	Name = Name.TrimStartAndEnd();
	if (Name.IsEmpty() || Name.Len() > 32)
		return false;
	for (const TCHAR C : Name)
		if (C < 32 || C == 127)
			return false;
	Slot.Identity.Id = Id;
	Slot.Identity.Name = Name;
	Slot.Identity.CreatedGameRevision.Reset();
	Slot.LastSavedGameRevision.Reset();
	const auto Revision = [&](const TCHAR* Field, FString& Value) {
		FString Text;
		if (Root->TryGetStringField(Field, Text) && FCityGameRevision::IsValid(Text))
			Value = Text;
	};
	Revision(TEXT("createdGameRevision"), Slot.Identity.CreatedGameRevision);
	Revision(TEXT("lastSavedGameRevision"), Slot.LastSavedGameRevision);
	const auto Number = [&](const TCHAR* Field, const int32 Fallback) {
		double V;
		return Root->TryGetNumberField(Field, V) && FMath::IsFinite(V) && V == FMath::TruncToDouble(V) &&
		               V >= 0 && V <= MAX_int32
		           ? static_cast<int32>(V)
		           : Fallback;
	};
	Slot.SchemaVersion = Number(TEXT("schemaVersion"), -1);
	Slot.GridRulesVersion = Number(TEXT("gridRulesVersion"), -1);
	Slot.Dimensions = FCityGridDimensions(Number(TEXT("gridWidth"), 0), Number(TEXT("gridHeight"), 0));
	Slot.Terrain.Dimensions = Slot.Dimensions;
	const int32 Difficulty = Number(TEXT("difficulty"), 1);
	if (Difficulty <= 2)
		Slot.Identity.Difficulty = static_cast<ECityDifficulty>(Difficulty);
	FString Funds;
	if (Root->TryGetStringField(TEXT("funds"), Funds))
		LexTryParseString(Slot.Funds, *Funds);
	const TSharedPtr<FJsonObject>* Economy;
	if (Root->TryGetObjectField(TEXT("economy"), Economy))
	{
		FString Month;
		double Residents;
		if ((*Economy)->TryGetStringField(TEXT("monthId"), Month))
			LexTryParseString(Slot.MonthId, *Month);
		if ((*Economy)->TryGetNumberField(TEXT("residents"), Residents) && FMath::IsFinite(Residents) &&
		    Residents >= 0 && Residents <= MAX_int32)
			Slot.Residents = static_cast<int32>(Residents);
	}
	Slot.bSupported = false;
	Slot.UnsupportedReason =
	    FString::Printf(TEXT("Unsupported format %d (current %d).\nArchive; use an older matching build."),
	                    Slot.SchemaVersion, FCitySaveDocument::CurrentSchemaVersion);
	return true;
}
TArray<FCitySaveSlot> Discover(const FString& Directory, const bool bArchived,
                               const FString& OriginalPath = FString())
{
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *FPaths::Combine(Directory, TEXT("*.json*")), true, false);
	Files.Sort();
	TArray<FCitySaveSlot> Result;
	TSet<FString> Seen;
	const auto Add = [&](const FString& Path, const FString& ExpectedId, const bool bBackup,
	                     const bool bOriginalSingleFile) {
		FString Json, Error;
		if (!FCityPersistence::ReadFile(Path, Json, Error))
			return;
		FCitySaveDocument Document;
		FCitySaveSlot Slot;
		ECitySaveReadFailure Failure;
		if (FCityPersistence::Deserialize(Json, Document, Error, ECitySavePlatform::Any, &Failure))
		{
			Slot.Dimensions = Document.GetDimensions();
			Slot.GridRulesVersion = Document.GridRulesVersion;
			Slot.Identity = Document.Identity;
			Slot.LastSavedGameRevision = Document.LastSavedGameRevision;
			Slot.Terrain = Document.Terrain;
			Slot.Funds = Document.Placement.Funds;
			Slot.MonthId = FCityCalendar::MonthIndex(Document.Economy.FixedStepId);
			Slot.Residents = Document.Population.Citizens.Num();
			Slot.ReportMonths = Document.Reports.Monthly().Num();
		}
		else
		{
			if (!ReadMetadata(Json, ExpectedId, Slot))
				return;
			// A corrupt current primary may still recover through its valid backup.
			if ((Slot.SchemaVersion == FCitySaveDocument::CurrentSchemaVersion ||
			     Slot.SchemaVersion == FCitySaveDocument::PrePantrySchemaVersion ||
			     Slot.SchemaVersion == FCitySaveDocument::PreOutpatientSchemaVersion ||
			     Slot.SchemaVersion == FCitySaveDocument::PreAiSchemaVersion) &&
			    Failure == ECitySaveReadFailure::InvalidData)
			{
				if (!bBackup)
					return;
				Slot.UnsupportedReason = TEXT("Invalid current city data; original files retained.");
			}
		}
		if ((!ExpectedId.IsEmpty() && Slot.Identity.Id != ExpectedId) || Seen.Contains(Slot.Identity.Id))
			return;
		Seen.Add(Slot.Identity.Id);
		Slot.Path = bBackup ? Path.LeftChop(4) : Path;
		Slot.LastSaved = IFileManager::Get().GetTimeStamp(*Path);
		Slot.bBackupOnly = bBackup;
		Slot.bArchived = bArchived;
		Slot.bOriginalSingleFile = bOriginalSingleFile;
		Result.Add(MoveTemp(Slot));
	};
	// Primary files win. Interrupted .tmp files and the index are never city documents.
	for (const auto& File : Files)
		if (File.EndsWith(TEXT(".json")) && FCityIdentity::IsValidId(File.LeftChop(5)))
			Add(FPaths::Combine(Directory, File), File.LeftChop(5), false, false);
	for (const auto& File : Files)
		if (File.EndsWith(TEXT(".json.bak")) && FCityIdentity::IsValidId(File.LeftChop(9)))
			Add(FPaths::Combine(Directory, File), File.LeftChop(9), true, false);
	if (!OriginalPath.IsEmpty())
	{
		Add(OriginalPath, FString(), false, true);
		Add(OriginalPath + TEXT(".bak"), FString(), true, true);
	}
	Result.Sort([](const auto& A, const auto& B) {
		return A.LastSaved == B.LastSaved ? A.Identity.Id < B.Identity.Id : A.LastSaved > B.LastSaved;
	});
	if (Result.Num() > FCitySaveLibrary::MaxCities)
		Result.SetNum(FCitySaveLibrary::MaxCities);
	return Result;
}
bool WriteIndex(const FString& Directory, TArray<FCitySaveSlot> Slots, FString& Error,
                const bool bSimulateInterruption)
{
	if (Slots.Num() > FCitySaveLibrary::MaxCities)
		Slots.SetNum(FCitySaveLibrary::MaxCities);
	auto Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("version"), 3);
	TArray<TSharedPtr<FJsonValue>> Entries;
	for (const auto& S : Slots)
	{
		if (S.bOriginalSingleFile)
			continue;
		auto O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("id"), S.Identity.Id);
		O->SetStringField(TEXT("name"), S.Identity.Name);
		O->SetNumberField(TEXT("schemaVersion"), S.SchemaVersion);
		O->SetStringField(TEXT("createdGameRevision"), S.Identity.CreatedGameRevision);
		O->SetStringField(TEXT("lastSavedGameRevision"), S.LastSavedGameRevision);
		O->SetBoolField(TEXT("supported"), S.bSupported);
		O->SetNumberField(TEXT("difficulty"), static_cast<int32>(S.Identity.Difficulty));
		O->SetBoolField(TEXT("archived"), S.bArchived);
		O->SetBoolField(TEXT("backupOnly"), S.bBackupOnly);
		O->SetStringField(TEXT("savedUtc"), S.LastSaved.ToIso8601());
		O->SetStringField(TEXT("funds"), LexToString(S.Funds));
		O->SetStringField(TEXT("month"), LexToString(S.MonthId));
		O->SetNumberField(TEXT("residents"), S.Residents);
		O->SetNumberField(TEXT("terrainSeed"), S.Terrain.Seed);
		O->SetNumberField(TEXT("reportMonths"), S.ReportMonths);
		O->SetNumberField(TEXT("gridWidth"), S.Dimensions.GetWidth());
		O->SetNumberField(TEXT("gridHeight"), S.Dimensions.GetHeight());
		O->SetNumberField(TEXT("gridRulesVersion"), S.GridRulesVersion);
		Entries.Add(MakeShared<FJsonValueObject>(O));
	}
	Root->SetArrayField(TEXT("cities"), Entries);
	FString Json;
	if (!FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Json)))
	{
		Error = TEXT("Could not serialize library index");
		return false;
	}
	FString Existing, ReadError;
	if (!bSimulateInterruption &&
	    FCityPersistence::ReadFile(FCitySaveLibrary::IndexPath(Directory), Existing, ReadError) &&
	    Existing == Json)
		return true;
	// A stale, missing, corrupt or interrupted index is rebuilt solely from committed documents.
	return FCityPersistence::WriteAtomic(FCitySaveLibrary::IndexPath(Directory), Json, Error,
	                                     bSimulateInterruption);
}

} // namespace
FString FCitySaveLibrary::Directory()
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames/Cities"));
}
FString FCitySaveLibrary::ArchiveDirectory(const FString& Directory)
{
	return FPaths::Combine(Directory, TEXT("Archived"));
}
FString FCitySaveLibrary::IndexPath(const FString& Directory)
{
	return FPaths::Combine(Directory, TEXT("LibraryIndex.json"));
}
FString FCitySaveLibrary::PathFor(const FString& Directory, const FString& Id)
{
	return FCityIdentity::IsValidId(Id) ? FPaths::Combine(Directory, Id + TEXT(".json")) : FString();
}
TArray<FCitySaveSlot> FCitySaveLibrary::List(const FString& Directory, const FString& OriginalPath,
                                             const bool bArchived)
{
	auto Active = Discover(Directory, false, OriginalPath);
	auto Archived = Discover(ArchiveDirectory(Directory), true);
	auto Catalog = Active;
	Catalog.RemoveAll([](const auto& S) { return S.bOriginalSingleFile; });
	Catalog.Append(Archived);
	FString CacheError;
	WriteIndex(Directory, Catalog, CacheError, false);
	// Never depend on cache success to show or load a city.
	return bArchived ? Archived : Active;
}
int32 FCitySaveLibrary::Count(const FString& Directory)
{
	TSet<FString> Ids;
	for (const auto& Slot : Discover(Directory, false))
		Ids.Add(Slot.Identity.Id);
	for (const auto& Slot : Discover(ArchiveDirectory(Directory), true))
		if (Slot.bSupported)
			Ids.Add(Slot.Identity.Id);
	return Ids.Num();
}
bool FCitySaveLibrary::RefreshIndex(const FString& Directory, FString& Error,
                                    const bool bSimulateInterruption)
{
	auto Slots = Discover(Directory, false);
	Slots.Append(Discover(ArchiveDirectory(Directory), true));
	return WriteIndex(Directory, Slots, Error, bSimulateInterruption);
}
bool FCitySaveLibrary::SetArchived(const FString& Directory, const FCitySaveSlot& Slot, const bool bArchived,
                                   FString& Error)
{
	if (Slot.bArchived == bArchived || !FCityIdentity::IsValidId(Slot.Identity.Id))
	{
		Error = TEXT("Choose a named city in the other library view");
		return false;
	}
	const FString Source =
	    Slot.bOriginalSingleFile
	        ? FCityPersistence::GetDefaultSavePath()
	        : PathFor(Slot.bArchived ? ArchiveDirectory(Directory) : Directory, Slot.Identity.Id);
	const FString Target = PathFor(bArchived ? ArchiveDirectory(Directory) : Directory, Slot.Identity.Id);
	if (FPaths::ConvertRelativePathToFull(Slot.Path) != FPaths::ConvertRelativePathToFull(Source))
	{
		Error = TEXT("City path does not match the library");
		return false;
	}
	auto& Files = IFileManager::Get();
	if (Files.FileExists(*Target) || Files.FileExists(*(Target + TEXT(".bak"))))
	{
		Error = TEXT("Another copy occupies the destination; both copies were kept");
		return false;
	}
	const auto Current = Discover(Slot.bArchived ? ArchiveDirectory(Directory) : Directory, Slot.bArchived,
	                              Slot.bOriginalSingleFile ? Source : FString());
	if (!Current.ContainsByPredicate([&](const auto& S) { return S.Identity.Id == Slot.Identity.Id; }))
	{
		Error = TEXT("City is missing or unreadable; files were kept");
		return false;
	}
	if (!Files.MakeDirectory(*FPaths::GetPath(Target), true))
	{
		Error = TEXT("Could not create the destination folder");
		return false;
	}
	TArray<FString> Moved;
	for (const FString& Suffix : {FString(), FString(TEXT(".bak"))})
	{
		if (!Files.FileExists(*(Source + Suffix)))
			continue;
		if (!Files.Move(*(Target + Suffix), *(Source + Suffix), false, false, false, true))
		{
			bool bRolledBack = true;
			for (const auto& Prior : Moved)
				bRolledBack &= Files.Move(*(Source + Prior), *(Target + Prior), false, false, false, true);
			Error = bRolledBack ? TEXT("Could not move city; original files retained")
			                    : TEXT("Move interrupted; copies remain recoverable in both library folders");
			return false;
		}
		Moved.Add(Suffix);
	}
	FString CacheError;
	RefreshIndex(Directory, CacheError);
	return true;
}
