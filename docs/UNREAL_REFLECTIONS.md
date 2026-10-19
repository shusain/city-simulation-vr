# Building materials and Unreal reflection troubleshooting

Verified with Unreal 5.7.4, the existing forward renderer and D3D11 Windows captures on 2026-10-14. D104's PC building/material result is user-accepted. This guide owns reusable findings; DEVELOPMENT_TASKS owns current work, CHANGELOG owns acceptance, and ENVIRONMENT owns build commands. Engine behavior must be rechecked after renderer/engine changes.

## Current material and runtime contract

| Surface | Metallic | Roughness | Treatment |
| --- | --- | --- | --- |
| Bare corrugated factory walls | 1 | Mapped, approximately 0.144 | Existing ORM roughness multiplied by 0.4; HQ forward reflections |
| Exposed garage/roller/workshop doors and metal frames | 1 | 0.22 | Dedicated IndustrialDoorMetal section; HQ forward reflections |
| Exposed factory mechanical parts and bollards | 1 | 0.18 | Separate hardware steel; HQ forward reflections |
| Painted panels, wood, siding and masonry | 0 | Their authored maps | Preserve the painted/nonmetallic coating |
| Destroyed stand-ins | Existing charred material | Rough | Existing scripted damage; no shiny replacement steel or runtime Chaos |

Use the actual imported material graph and mesh slot binding when diagnosing a surface. FBX slot names alone do not supply a physically based Unreal material. Door faces previously shared the wall/trim section; lowering a wall material's roughness would affect unrelated faces. `CityIndustrialMetalMaterials.py` selects exact authored entrance faces and supplies the dedicated steel contracts. Glass and access paths retain their earlier assignments.

Windows Desktop/PCVR uses one 64px HDR SceneCaptureCube at the board centre, refreshed every 0.5 seconds through the existing environment update, including while the city is paused. Its six faces and native mip chain replace the skylight's processed **specular** cube on the GPU. The specified Engine daylight cubemap retains authored diffuse irradiance. Capture-only dynamic shadows and recursive reflection-environment rendering are disabled; main-view shadows remain. Actor/component capture ticking is disabled; buildings do not acquire individual ticks or captures.

Android/Quest keeps the persisted static room reflection. `-CityStaticReflections` selects that fallback on Windows. The shared cube has approximate spatial correspondence and native mip roughness filtering; it is not a local planar mirror or a custom GGX convolution. PCVR uses the same Windows implementation. Actual headset appearance/cost and larger-city cost remain unconfirmed.

Implementation: [CitySandboxEnvironment.cpp](../CitySimVR/Source/CitySimVR/Private/Sandbox/CitySandboxEnvironment.cpp), [header](../CitySimVR/Source/CitySimVR/Public/Sandbox/CitySandboxEnvironment.h), [industrial contracts](../Tools/CityIndustrialMetalMaterials.py).

## Make isolated samples show reflections

Keep the neutral same-camera geometry comparison, and add a separate reflection-revealing perspective scene. User feedback: neutral isolated samples conceal the effect, while the in-game surroundings show it clearly. This is guidance for future review setups, not a claim that the existing neutral sheets were regenerated.

1. Use a perspective camera, manual exposure, fixed framing and unchanged material values. A flat plate reveals reflected image content; a curved cylinder/sphere shows a moving highlight. Keep an actual building/door alongside them.
2. Add recognizable matte red/green objects, a light card and a dark card around the receiver. Use varied lighting directions and bright/shaded surroundings. Illuminate the reflected objects; a direct light highlight alone does not prove that scene geometry is reflected.
3. Hide a diagnostic object from the review camera only, preserving its participation in the reflection capture. Avoid actor-wide hiding, which removes the object from the capture too. Place cues where the receiver's surface normal reflects them toward the camera, within the intended capture coverage.
4. Compare the cue present, removed with a deliberately stale capture, and removed after refresh. Retain a no-probe/uniform-cube negative and a moderate-roughness comparison. Record source material, projection, capture position, exposure, cube, mixing policy and resolution.
5. Review one actual packaged scene as well. Editor automatic probe updates, an isolated render target and packaged gameplay are distinct paths. Preserve scale/camera/lighting across before-after comparisons.

The controlled plate test used dominant red/green pixels above 100 and at least 40 above the other channels. This is a fixture-specific diagnostic, not a universal metal-quality score. Geometry sheets may stay orthographic; use separate perspective views to judge reflective surfaces.

## Diagnosed failures and the check that distinguishes them

| Symptom | Finding in this project | Focused next check |
| --- | --- | --- |
| Bright but flat gray metal | A uniform environment has no recognizable reflected features | Add lit colored geometry/light-dark cards; compare hidden-cue present/removed |
| Reflection absent only in a sheet | Matching orthographic SceneCapture2D had no strong colored cues; perspective did | Change projection alone before changing the material, probe or lighting |
| Dark rough metal in an unbaked scene | Legacy indirect-light mixing suppressed the cube | Check the scene-scoped `r.ReflectionEnvironmentLightmapMixing=0` policy; do not change project-wide lighting blindly |
| Fuzzy cue despite a working capture | Approximately 0.36 roughness heavily blurred detail | Compare an isolated 0.05 control; retain appropriate production roughness rather than making every surface a mirror |
| Editor works, package does not | Editor probe refresh is not persisted cooked capture data | Save/build the static reflection data, cook its HDR and inspect the actual package |
| New buildings do not appear in reflections | Ordinary reflection probes are static at runtime | Check the Windows dynamic branch, refresh count and actual present/removed cue |
| Real Time Capture does not include the city | Skylight real-time sky/cloud capture is not the full-city solution used here | Use the explicit scene cube; do not assume the checkbox captures ordinary buildings |
| CPU pause or GPU peak on refresh | Recurring RecaptureSky and six-view shadow work were costly | Retain GPU-only copying/capture-only shadow policy; investigate only an observed regression |
| GPU copy has incorrect cube content | A cube resource also exposes a scratch 2D render target | Copy `SourceResource->TextureRHI`, all six slices/all available mips; do not copy `GetRenderTargetTexture()` |
| Smooth flag still looks faceted | Authored cylinder vertices are disconnected per face | Preserve custom averaged curved-wall corner normals, with hard rims/caps; this is mesh data |
| Damaged door loses its semantic FBX section | FBX merged duplicate material objects | Give the charred door a distinct Blender material while binding its Unreal slot to existing MI_CharredMechanical |

The orthographic finding is specific to the tested forward/offscreen path; it is not a general assertion that all Unreal orthographic reflections are unsupported. A preview lighting build did not recover its cues. Perspective reflected the red cube/green sphere before and after that build, so Lightmass was not the missing operation in that fixture.

The sky dome is excluded from scene sky capture. A fixed specified `/Engine/MapTemplates/Sky/DaylightAmbientCubemap` prevents an empty upper hemisphere from blackening shaded building walls. The room HDR supplies separate specular features. Keep diffuse lighting and the test's specular environment roles distinct.

The inspected MeatSaber handle supplied a useful metallic1/roughness0.05 reference and sphere probes. Its config's Lumen selection was not adopted for this forward project. Do not switch rendering frameworks to solve a material/projection/capture problem without evidence.

## Authoring, persistence and focused verification

Production blend/manifest: `ArtSource/Blender/CityLowDensityBuildings.blend` and `ArtSource/Manifests/city_low_density_buildings.json`. Approved D103/D104 study sources are retained inputs. Regenerate exports only for concrete authoring changes; review composition does not require rewriting the source.

- Native paired sheets: `Render-CityUnrealPreviewSheet.ps1` with the D104 batch; `Capture-CityReflectiveMaterials.py` and `Test-CityReflectiveMaterials.ps1` provide the controlled reflection matrix. These use isolated artifact projects/output directories.
- Static room HDR changes: `Prepare-CityRoomReflection.py`, then `Test-CityLowDensityIntegration.ps1 -Mode Bake -RunName <fresh>`. `Bake-CityLowDensityReflection.py` imports the cube and calls the existing native reflection-bake helper, saving CitySandbox/BuiltData. This is reflection build data, not Lightmass.
- Changed cylinder/door source: use the focused update/import tools and checked FBX readback. `Prepare-CityLowDensityIntegration.py` retains the hollow/smooth flues, new metal sections and paired charred recipe for subsequent regeneration.
- Dynamic proof: `Test-CityDynamicReflections.ps1 -Mode Proof -RunName <fresh>` uses an isolated profile and the actual factory metal. The wrapper copies its fixture to the profile rather than relying on packaged ProjectDir. Optional SequentialCubeFaces is diagnostic, not a shipped setting.
- Industrial steel review: `Test-CityIndustrialMetalMaterials.ps1 -Mode Before|Import|After -RunName <fresh>`; capture Before before importing a new revision. The composer uses matching camera/triangle records. Preserve an accepted output revision before overwriting later captures.
- At delivery, use existing build/package scripts and the actual game fixture. Verify changed cooked asset headers and the Quest embedded payload. No device installation or full suite follows automatically from an art revision.

Python helpers use explicit UTF-8 when writing prose/JSON. Encode the complete text before atomically replacing shared docs. In Blender, reacquire UV/color RNA handles after creating layers; stale handles caused a reproducible allocator crash. Never treat a stale BootstrapError/error file as a new run's outcome; require a fresh report and successful log.

## Retained evidence and acceptance

| Evidence | Result | Local receipt/gallery |
| --- | --- | --- |
| Controlled reflection matrix | 92 captures; perspective hidden cues approximately 35,147 red / 18,064 green before bake; orthographic and no-probe/uniform negatives had no strong cues | [Reflectivity gallery](../Artifacts/D104/Reflectivity/Review.html) / Verification.json |
| Actual Windows dynamic scene reflection | Factory material hidden red cue 838 pixels; removal plus refresh 0, stale control retained it | [Dynamic gallery](../Artifacts/D104/DynamicReflections/Review.html) / Implementation.json |
| One bounded workstation run | Twelve buildings; GPU mean 2.1883ms static / 2.1957ms dynamic, p99 3.5885 / 3.6763ms | DynamicReflections proof BoundedCost.json; no headset/larger-city guarantee |
| Industrial steel final delivery | 20 native comparisons, 17 packaged views, four focused cases; all24 source meshes retain exact geometry/UV/RGBA/normals; Windows/Quest packages verified | [Industrial metal gallery](../Artifacts/D104/IndustrialMetal/Review.html) / Implementation.json |

Final integrated workspace revision `dev-a5b94d7b297846bf1af4`, save format67; user accepts the PC metallic features and in-game appearance. Future changes may supersede executable/source hashes. Receipt verifiers target identified frozen/current inputs; do not expect a historical delivery verifier to pass against arbitrary later assets. Follow CHANGELOG/ENVIRONMENT for the latest maintained build.

D115's offline Chaos prototype is retained reference evidence. Detailed interiors/support structure and convincing fracture remain future research; user chooses the existing stand-ins/scripted damage now. Any future destruction may be prebaked to keep runtime cost bounded.

## Epic references

These explain the stock renderer features; the custom GPU cube-copy behavior and measured diagnoses above come from this repository's source and native receipts.

- [Reflection captures](https://dev.epicgames.com/documentation/en-us/unreal-engine/reflections-captures-in-unreal-engine): static runtime probes, approximate projection, roughness mips and lightmap mixing.
- [Forward shading](https://dev.epicgames.com/documentation/en-us/unreal-engine/forward-shading-renderer-in-unreal-engine): per-material HQ reflection choices and renderer feature constraints.
- [Sky Lights, UE5.7](https://dev.epicgames.com/documentation/en-us/unreal-engine/sky-lights-in-unreal-engine?application_version=5.7): specified/captured sources, mobility and real-time sky capture behavior.
