# Proposed single-batch native recipe — source code only

## Current first-pass material policy (supersedes the original45c proposal)

The native reviewer selected **three shared structural masters**, replacing the
original46 value/name-sensitive parents.31 source graphs map to Solid,19 to PBR
and1 to PV. Every one of the51 source graphs still passes the unchanged source
validator; names, paths, texture identities, source values and plant bounds do
not select a parent. Scalar/vector/texture instance parameters carry all actual
source constants, tints, per-map UV scales, roughness/metallic scales, normal
strength, AO weight and indexed PV grid controls. Runtime setters immediately
read back each parameter and assert its expected value before saving.

Solid has7 parameters, PBR16 and PV12. The PBR graph includes uniform source
tint/AO weights, allowing implicit/scaled UVs and AO-linked/non-AO sources to
share one sampler-compatible graph without losing source links. PV retains the
actual12x8 frequencies and.018 thresholds through one stock Frac/strict
LessThan/Max/Lerp graph. Compile-time differences are explicit: no textures for
Solid/PV; color/normal/ORM sampler types and tangent-normal graph for PBR.

**Source building-concrete3D noise and derivative bump are omitted in this first
native pass**, matching nuclear's solid-material policy. This is an intentional
proposed visual difference, not parity. Four source concrete graphs carry an
explicit omission record; no concrete procedural implementation remains in the
recipe. Foundation units retain the existing textured M_Concrete unchanged.
There is no new texture, custom shader or performance program.

`CityApprovedUtilityTemplates.py` defines/extracts the three structures and
effective material policy. Both native receipts record that policy and reject a
stale/different phase receipt. The frozen import plan and every old
source/LOD/geometry/native-recipe proof remain byte-identical. Its original
noise-approximation note is retained as `superseded_frozen_plan_note`; it is not
the current execution policy.

The current offline report is
`Artifacts/UtilitySourcePreparation/SharedTemplates-OfflineVerification.json`.
It includes all51 mappings, source-origin/default traces, three representative
parameter sets, healthy/damaged defaults, name/value/texture stability checks,
705 source-vs-template PV samples including threshold edges, and negative
sampler/missing-link/invalid-grid plus original node/cycle cases. Input and
render-probe validator function ASTs are unchanged from45c. No Unreal execution
or native compile/instance-readback pass is claimed by offline checks.

The scoped `ArtSource/Exports/UtilityIntegration/README.md -text` rule preserves
that independently pinned file's exact LF bytes after Windows checkout. It does
not normalize or change any approved/prepared asset or README content.

---

## Import and validation contracts

This recipe extends exact local preparation head
`33c7f223c59a8f7af7c9419df458e160a26e42f0`. All four existing preparation/proof
manifests and all approved sources/LODs/foundation/wind derivatives are unchanged.
The recipe has **not** run in Unreal: no native asset, shader compile, catalog,
C++, CI manifest, CI job, push or new preview was produced here.

`ArtSource/Manifests/city_utility_native_import_plan.json` contains the exact
inputs, indexed Blender source sockets, canonical bindings, source render-UV
channel and selected actual surface probes. Indexed sockets preserve duplicate
Math `Value` inputs (PV multipliers12/8 and threshold.018). The read-only Blender
inspector wrote JSON facts only. The original45c offline proof
`city_utility_native_recipe_offline_verification.json` is preserved as history;
the current shared-template report is identified above.

## One future job, two preparation phases

The native owner reviews the code and visual proposals, reserves the normal
shared native window and selects these two paths **in this order** in the one
utility batch's existing AssetPreparation process:

```text
Tools/Import-CityApprovedUtilityMaterials.py
Tools/Import-CityApprovedUtilityMeshes.py
```

Both match the current `^Tools/[A-Za-z0-9_-]+\.py$` CI rule. Each receives the
existing900s script cap; actual native phase durations are unmeasured. This task
does not change that manifest or schedule a job. No per-asset PR/job is needed.
The phase implementation modules live under `Tools/Assets/Unreal`.

## Strict preflight and scope

Pure-Python `CityApprovedUtilityPlan.preflight()` verifies the four independently
pinned raw preparation/proof manifests, all11 exact approval/artifact contracts,
the233 protected original/reference files, all relevant blend/FBX hashes, actual
shared PNG SHA256/MD5, frozen whole/wind/foundation source mappings, supported
graph topology/types and input paths. No CRLF recovery/normalization or source
substitution occurs. The two new recipe JSON files have scoped `-text` rules.

Native entry additionally requires UE5.7.4 and the actual calling project root.
All new material/mesh saves are restricted to
`/Game/CitySim/Art/Utilities/ApprovedBatch`. It never imports shared textures or
saves nuclear/shared/catalog objects. The existing concrete binding is
`/Game/CitySim/Art/HomeGroundPilot/M_Concrete` and remains read-only.

Before mutation, each of the six existing shared texture packages must expose
its actual hidden `AssetImportData` import tag (`UObject::SourceFileTagName()`
resolves to this name in UE5.7 `Obj.cpp`). Its `FileMD5` must match the repository
PNG whose SHA256 matches the pinned source. This follows UE5.7
`UTexture::GetAssetRegistryTags`/`FAssetImportInfo::ToJson`; a missing/stale import
record fails rather than reimporting or accepting it. Actual sRGB, compression
and normal `flip_green_channel=False` are checked. Package hashes for these
textures, stock concrete and any existing nuclear inputs are checked unchanged
after each phase. Import provenance and package hashes are evidence of inputs;
they do not establish visual parity or constitute a pixel-export comparison.

## Materials and proposed visual differences

Phase1 maps51 indexed source graphs to three scoped stock-node parents and
explicit healthy/damaged instances. Source constants, names, paths and texture
identities never produce additional parents. It copies source scalar/color constants,
tint multiplies, UV scale, image/channel links, normal-map strength and PV
math/mix operations. Legacy FBX flips UV V: source procedural coordinates are
recovered as `(u,1-v)`, then converted back to image-origin coordinates when
sampling shared maps. This preserves scale offsets as well as orientation.
All texture connections use UE5.7's shortened `UVs` pin. `If` uses `A > B`,
`A == B`, `A < B`, as confirmed in the installed graph/API source.

Source concrete noise/bump is deliberately omitted for this first pass. Solid
building surfaces share one flat-normal parent; foundation units continue to
use existing textured M_Concrete. No procedural concrete graph, custom shader,
function asset, plugin, baked map or texture generation is introduced.

The plan and native receipts explicitly retain these **proposals for review**:

- Source concrete noise/bump is absent from building materials in this first
  pass, matching nuclear's solid-material approach. This is a proposed visual
  difference, not parity; textured runtime foundation materials are unchanged.
- UE opaque Default Lit replaces Blender Principled lighting/lobes. Native
  material and light-direction review remain necessary.
- Tangent-normal strength uses decoded XY weighting/normalization and existing
  shared-map green-channel policy; native normal direction still needs review.
- Disconnected source ORM.R stays disconnected; source AO/color multiply links
  are preserved without silently adding extra ambient darkening.
- Initial LOD screens `[1,.12,.025]` mirror the nuclear option only. No Android
  minimum, VR budget or performance claim is selected here.

Damage follows the current nuclear policy without geometry deformation: detailed
image/PV base colour receives tint(.16,.13,.11); solid regions receive
(.065,.055,.045) and roughness.95. Original maps/normals/metallic remain attached.
All damaged wholes retain the healthy source geometry/LODs. No fracture/collapse
or moving damaged-role adaptation is introduced.

## Mesh imports and checks

Phase2 requires Phase1's same-plan, same-recipe-code material receipt and exact
saved material hashes. It uses legacy FbxFactory (scene/unit conversion, scale1,
zero translation/rotation, no pivot bake/recentre, import source normals, no
auto collision/Nanite/lightmap UVs), imports the prepared LODs, and explicitly
binds semantic material slots by name. Source normals are not recomputed;
tangents are rebuilt. Full-precision UVs/high-precision tangent storage are
selected for source fidelity; their native platform cost is unmeasured.

The intended37 new mesh assets are:

- 11 approved whole LOD0s with prepared LOD1/2, plus11 matching damaged wholes;
- 3 wind roles, each with all3 LODs (nine source FBXs);
- 12 unit foundation groups, preserving their source-top-Z offsets.

All54 imported source LOD files are pinned. Actual render-buffer checks cover
LOD count, exact triangles by semantic material, exact nonempty sections and
bindings, signed bounds/pivot/source→native basis, UV-channel counts and selected
position/UV0/normal surface probes at every LOD. Source-empty material slots may
survive FBX as empty sections; only declared slots are allowed and every nonempty
section must exactly match its source triangle count. UV0 must match the recorded
FBX V flip. Probes allow.2cm position/.0002UV and normal dot>.99999; actual errors
are retained. Other authored UV channels are retained/count-checked; their entire
native buffers are not exhaustively compared. Foundation expected local bounds
retain top0/bottom-100cm; offsets/depth-only runtime placement remain #125 work.

Existing owned damaged destinations are reused only if the same exact
geometry/LOD/material checks pass. No native asset deletion is performed.
Fresh imports and owned same-input reruns use the same scoped names.

## Receipts and remaining acceptance

Successful phases append only their scoped saved packages to
`Artifacts/CIAssetPreparation/Files.txt`, preserving other preparations' entries.
`ApprovedUtilityMaterials.json` contains source mapping/approval hashes, recipe
hashes, actual input provenance, bindings, shader statistics and native package
hashes. `ApprovedUtilityMeshes.json` adds exact source/native LOD mappings,
render-buffer probes/bounds/sections, offsets and package hashes. Progress is
explicitly partial until the final receipt; stale owned final receipts are
removed at phase start. Catalog/runtime adoption is never performed.

Offline checks actually run: Python AST, all frozen input/texture hashes,
51 graph schemas/cycles, rejection of invalid/cyclic graphs, independent source
PV colours/12×8 periods, wrapper/cap rules,37 unique target paths/54 source LOD
records, enabled existing ProceduralMeshComponent and hashed installed-engine
API/header references. No mocked result is presented as native validation.

Actual Python reflection/API calls, shader compilation, phase timings, native
imports/visuals, weather yaw/spin, foundation/portal/path/stair/plot/upgrade/save
runtime behavior, Windows playtest and physical headset observations remain
unconfirmed. #125 owns full code review, execution and retained-product commits.
The other15 reviewed sources remain outside this recipe.
