# Asset and tool organization

[Issue #150](https://git.shaun-husain.com/CitySimVR/citysim/issues/150) tracks this review. New maintained names describe purpose. Task/issue IDs identify work and provenance rather than asset capabilities or production folders.

## Semantic locations

| Previous location/name | Current purpose/name |
| --- | --- |
| `/Game/CitySim/Audio/D58` | `/Game/CitySim/Audio/Soundscape`, preserving Ambience, Sirens, Effects, UI and Graphs |
| `/Game/CitySim/Art/Prototypes/D79` | `/Game/CitySim/Art/Prototypes/BuildingSurfaces` |
| Park animation `_D83` suffix | Same descriptive animation name without task suffix |
| Citizen `AS_Citizen_Walk_D121`, `BS_Citizen_StreetWalk_D121` | `AS_Citizen_Walk`, `BS_Citizen_StreetWalk` |
| `M_D122ShopWindows`, `M_D122RepairWindows` | `M_StorefrontRoomWindows`, `M_RepairDisplayWindows` |
| `MI_D122Room_*`, `T_D122Room_*`, `T_D122RepairDisplays` | `MI_StorefrontRoom_*`, `T_StorefrontRoom_*`, `T_RepairDisplays` |
| `AudioSource/D58`, `AudioSource/D82` | `AudioSource/Soundscape`, `AudioSource/Music/CitySoundtrack` |
| `ArtSource/Derived/D122`, `D122Rooms` | `ArtSource/Derived/EditorialArt`, `StorefrontRooms` |
| Editable material-study filename with `D104` | `ArtSource/Blender/Studies/CityBuildingMaterials.blend` |
| Authored export filenames with `_D83`, `_D88`, `_D121` | Same descriptive names without task suffix; payloads retained |

`Tools/Assets/Manifests/CityAssetOrganization.json` records the exact 247 native moves and 164 source relocations with original payload hashes. Active manifests, producers, native constructors, tests and import paths follow the semantic names. The presentation art tests use `StorefrontRoomReferences`, `NewspaperRendering` and `Desktop.StorefrontRooms`.

The old soundscape checkout converted acceptance JSON newlines inconsistently: two snapshots require CRLF and two require LF to match their already-recorded accepted hashes. The relocated bank restores those exact pinned bytes and disables Git newline conversion for this source tree. The relocation manifest also records the two original Git blob hashes and the byte repair. No JSON content or acceptance hash is changed.

Unreal packages are renamed with dependent packages loaded/saved and scoped redirector fixup. The124 live sound/animation assets explicitly preserve redirectors using ObjectTools::RenameSingleObject, the engine operation behind AssetTools. The other123 study/window assets use normal AssetTools batch rename. This avoids the unattended CDO information dialog and prevents the no-redirector path from retaining unchanged originals, without changing unattended flags or answering arbitrary dialogs. Soft paths, registry notifications, metadata and thumbnail transfer remain engine-owned. Before cleanup, every reviewed object must have its expected destination path and no remaining old file may match the original hash. After dependent saves and an AssetRegistry refresh, cleanup accepts only real redirectors with zero remaining package referencers and calls normal ObjectTools::DeleteObjects without forced deletion. AssetTools::FixupReferencers is avoided because its5.7 implementation unconditionally opens a modal report. The preparation receipt records classes, direct referencers, changed hashes, removed old packages and a registry scan for stale hard/soft package references. Source import filenames are stored relative to their asset packages, without reimporting or changing recorded source hashes/timestamps (including unknown originals). Unreal's default sanitization makes external source paths relative to the Engine directory and embeds the ephemeral CI checkout, so it is intentionally avoided. Resolved new filenames/source hashes are read back, and every changed package is checked for temporary workspace strings before acceptance. The prepared bytes must be committed and checked again before packaging; preparation changes an ephemeral input tree and does not establish the final executable revision.

This is an early-development path change. Old external references to these production packages need updating; no compatibility redirectors are retained. Current game saves store simulation/catalog identities, which are unchanged. Player files and the preserved VR template are retained.

Hard references keep their UObject pointer during a rename, so their owner packages can stay clean. The migration saves every loaded referencer unconditionally after the complete batch. Fresh-process checks additionally verify exact Wave Player object literals and both idle/walk blend samples; a zero-stale-edge registry result alone does not establish their correctness. Focused follow-up repairs changed only21 Wave Asset literals and one idle sample reference, preserving graph nodes/connections/other literals and blend sample settings. Final hashes are composed in `evidence/tool-organization-final-native.json` from the base audit and the two scoped repair receipts.

## Tool boundaries

Use the functional entry points in [Tools/README.md](../Tools/README.md). Canonical preview code lives under Assets/Blender and Assets/Preview, temporary galleries under Capture, texture derivation under Assets/Textures, native imports under Assets/Unreal and staged soundscape import under Audio/Soundscape. Maintained packaging commands live under Build, interactive profiles under Launch and APK deployment under Distribution, with original root command forwarders and unchanged parameter contracts. Validation and CI orchestration have their own directories. Shared approval verification lives in Assets/Manifests/CityApprovedSources.py; it checks pinned identity/hash plus both artifact lists before callers derive textures.

Eight PowerShell command-forwarding fixtures and original-body comparisons cover paths, quotes/brackets, explicit false switches, defaults and typed values without launching native operations. The Windows candidate exercises the moved packaging command; no deployment or Quest package is implied.

Editorial derivation/import share explicit `housing`, `commerce`, `automation` and `repairs` keys with `newspaper_illustration` or `repair_display` usage. Compression, masking and repair selection use those roles; source issue numbers identify pinned approvals only. Missing, duplicate or mismatched roles fail before native writes. Maintained editorial, storefront and soundscape imports also store package-relative source filenames through the shared Editor API.

The six numbered editorial/room wrappers and numbered condition-icon wrapper are retired by #283; use the canonical implementations. The semantic root soundscape commands still forward. Texture derivation and native import commands have `main()` guards; importing a producer does not derive/import assets. The historical native-window guards in completed one-shot importers are retained; organizing a script does not authorize replaying its completed integration.

Many root tools are completed studies or tightly pinned integration recipes. They are retained as historical entry points, with explicit paths updated where a source/package moved. Do not mass-move or replay them. Extract a shared operation when a maintained workflow needs it. The remaining repeated process/root/report/capture/preview/manifest/approval operations now have explicit shared APIs. This review does not justify a new general tooling framework.

## Provenance exceptions

Keep generated `ArtSource/Generated/<backend>/issue-N/vN` exports, approval manifests, original source-internal object/take names and immutable acceptance snapshots. Approved source payloads are moved byte-for-byte rather than reauthored or re-exported. Historical Artifacts, changelog entries, evidence files, report names and their links retain their original identifiers. Use the relocation manifest to interpret their original paths. Historical migration manifests still record the retired numbered wrappers. Current automation profiles/flags and the `Drone CI` owner are semantic; see [semantic naming](SEMANTIC_NAMING.md).

Eighteen retained SurfacePrototype study meshes also retain original self-path strings inside Interchange import-node source snapshots. These are authoring metadata rather than live package-reference edges; the studies' actual package names and loaded hard/soft dependencies use BuildingSurfaces. They remain an explicit source-internal provenance exception.

Offline tests establish source identity and script contracts. Focused native tests establish actual audio/animation/storefront references and behavior. Windows cooked/startup/capture checks establish the candidate contents; human playtest remains separate. Quest coverage is deferred under the PC-first policy unless a concrete platform risk is identified.

The final [delivery receipt](evidence/tool-organization-delivery.json) identifies passing Editor237 and Windows238 at44495c5, all exact Editor/Client cases, cooked inventory, executed/ZIP/extracted byte identity and inspected capture hashes. Shaun accepted candidate238 on2026-10-18 and authorized PR162 merge/150 closeout; see [acceptance](evidence/tool-organization-acceptance.json) and CHANGELOG. The completed150 card is removed from the active queue. The [attached receipt](https://git.shaun-husain.com/attachments/1273333f-bb60-4905-b08f-4c9e06541b07), [cook proof](https://git.shaun-husain.com/attachments/bfdfb635-c24c-4882-a7e4-1e293aa4e592) and [Drone result](https://git.shaun-husain.com/attachments/6047d385-0e0b-48c1-b238-769fa7c1c1f3) are reviewable on150.
