# Citizen animation authoring rig

D87 adds `ArtSource/Blender/CityCitizenControlRig.blend` for making new citizen animations. It contains Casual, Office and Worker, each with the existing full-detail and LOD1 meshes. Each rig has20 original skeleton bones plus8 non-deforming animation controls. The original D83 blends, accepted17-clip set, exported FBXs and native assets are retained. This file is an authoring tool; it does not change animation in a running game.

## Pose and animate

Open the file in Blender5.0.1. It opens in Pose Mode with Casual selected and a new planted-crouch demo. Play frames1–48 at24fps; frame49 is the duplicate loop endpoint. The saved armature collections separate the controls from the hidden limb bones:

| Collection / control | Use |
| --- | --- |
| **Body — FK controls**, blue | `root` moves/rotates the entire body and all handles. Move `pelvis` to lower/raise the body while the feet stay in place. Rotate `spine`, `chest`, `neck`, `head` and clavicles for the upper body. |
| **Hands and feet — IK controls**, green left / gold right | Move `CTRL_hand.L/R` to position the wrists; the two arm bones solve automatically. Move `CTRL_foot.L/R` to position the ankles. Rotate these controls for wrist/foot orientation. |
| **Elbows and knees — pole controls**, diamonds | Move `CTRL_elbow_pole.L/R` or `CTRL_knee_pole.L/R` to steer the bend plane. Keep these away from the line between the limb root and its target. |
| **Limb bones — inspect only**, hidden | Reveal to inspect the actual constraints and IK settings. Do not key these directly or clear their pose rotations: they carry the preferred bend seed. |

For a new animation, unlink the demo in the Action Editor, create a new action, select the Body, IK and Pole controls, and key their **Location & Rotation**. Start with a neutral control pose (`Alt-G`, `Alt-R` on those controls only). With global transform orientation, select `pelvis` and use `G Z` to crouch; the ankle targets remain planted. Select a hand and use `G X` to reach forward. Rotate the hand/foot handle to orient it. Scale channels are locked. The other palettes and LODs are separate rigs with the same control names; select the desired armature in the Outliner and reveal its mesh/rig if hidden. LOD1 starts hidden.

The old D83 actions key deform bones and use their original rolls. Review them in `CityCitizenRigPrototype.blend` or `CityParkAnimationPreview.blend`; assigning them directly to this constrained rig is not supported. This new file contains only the new control demo, so incompatible clips do not clutter the Action Editor. FK/IK switching and automatic transfer of old clips are future additions if needed.

## Limits

The limits are practical animation defaults relative to the authoring rest pose, not a medical anatomy model. Blender shows local axes for each bone. The vertical body bones use localY for twist; the normalized arm/leg hinge uses localZ. Local bone axes are different from global scene axes.

| Joint | LocalX | LocalY | LocalZ |
| --- | --- | --- | --- |
| Pelvis | ±35° | ±50° | ±35° |
| Spine | ±25° | ±35° | ±35° |
| Chest | ±35° | ±60° | ±45° |
| Neck | ±25° | ±45° | ±35° |
| Head | ±35° | ±65° | ±40° |
| Clavicle | ±30° | ±30° | ±30° |
| Wrist | ±40° | ±60° | ±70° |
| Ankle | ±45° | ±45° | ±75° |
| Elbow | Locked | Locked | −145° to +5° |
| Knee | Locked | Locked | −5° to +150° |

Each arm/leg IK chain has length2 and stretch disabled. The solver itself has the hinge-axis limits enabled; the matching Limit Rotation constraints also document the intended range. Body, wrist and ankle limits clamp their evaluated rotations. Shoulder/hip orientation is determined by the endpoint and pole rather than a separate enforced anatomical cone. Bone translation is locked except on `root`, `pelvis` and endpoint/pole controls. All bone scale channels are locked.

An unreachable handle can move beyond the limb's reach: the limb stops at its length/angle limit. The rig does not perform collisions, prevent body penetration or guarantee a comfortable pose for every possible handle/pole placement. Keep poles reasonably placed and review silhouettes while authoring. Flat feet can tilt when a requested pose exceeds the ankle's range; rotate/reposition the foot or reduce the pose in that case.

## Bake to the existing skeleton

The authoring derivative aligns upper/lower limb rolls while retaining every original joint position and length. Shoes use rigid foot weights for stable sole placement. The bake converts evaluated skin transforms back to the retained original bind matrices, removes the8 controls and all authoring constraints, and writes only the original20 bones. Pelvis translation is carried on the non-deforming root in the baked clip; this preserves skin movement and avoids Blender FBX auto-connection suppressing translated pelvis motion. Native adoption should retain in-place cosmetic playback without extracting root motion into authoritative agent movement.

Run from the repository root, choosing a **new** output path. The tool refuses to overwrite an existing export and never modifies the `.blend`:

```powershell
& 'C:\Program Files\Blender Foundation\Blender 5.0\blender.exe' `
  --background --factory-startup --threads 2 --python-exit-code 1 `
  --python Tools/Bake-CityCitizenControlRig.py -- `
  --source ArtSource/Blender/CityCitizenControlRig.blend `
  --mesh SK_Citizen_Rig_Casual --action D87_Demo_PlantedCrouch `
  --start 1 --end 49 --output Artifacts/D87/My-Crouch.fbx
```

For your own motion, save a working copy of the authoring file, then substitute that file, action and frame range. This is a meters-based authoring FBX, not a replacement for the native centimeter staging/import workflow in `docs/ENVIRONMENT.md`. A deliberate future animation adoption includes that staging step and focused native readback; no runtime asset promotion is performed by this tool.

## Generation and focused verification

`Tools/Create-CityCitizenControlRig.py` generates the source and `ArtSource/Manifests/city_citizen_control_rig.json` from accepted D83. The generator refuses to overwrite an existing authoring file by default. `-- --replace-generated` is allowed only when its SHA256 still matches the last generated manifest; edited artist work must be preserved separately.

```powershell
# Read the saved source; exercise posing/limits and a clean FBX readback:
& 'C:\Program Files\Blender Foundation\Blender 5.0\blender.exe' `
  --background --factory-startup --threads 2 --python-exit-code 1 `
  --python Tools/Validate-CityCitizenControlRig.py
```

The focused check covers all6 saved rigs and24 neutral/crouch/reach/step poses, forward knees, planted shoe soles, reachable targets, pole steering, excessive fold limits, unreachable no-stretch cases and evaluated body/wrist/ankle clamps. It also checks49 demo frames, the restored20-bone bind and a7-frame FBX deformation readback. Evidence is in `Artifacts/D87/Validation.json` and `Validation.log`. Accepted input hashes are checked before/after; no Editor/game/package/device pass is implied.

`Tools/Render-CityCitizenControlRig.py` creates read-only source views and12 sampled demo frames. `Tools/Label-CityCitizenControlRig.py`, using the existing bundled Python/Pillow, makes4 labeled views and `Artifacts/D87/Preview/Planted-Crouch.gif`. These are source renders with projected controls, not application screenshots.

## Sidewalk walk source preview — D121

For the current D121 v2 sidewalk walk source, open `ArtSource/Blender/CityCitizenWalkCycle-v2.blend` and play1–28 at24fps (29 is the duplicate endpoint). All three outfits and both LODs retain these D87 controls and separate `D121_Walk_*_Controls` actions. v2 aligns hands with the forearms and adds visible shoulder/hip counter-twist plus a6.5-degree forward lean; the broad v1 stride is retained. The original v1 source/media, D87 authoring source and accepted park actions remain unchanged. **User approved v2 on2026-10-15; D121.2 is delivered as maintained Windows `dev-27bb30632e82c9f1af04` / schema68 with a verified shared Quest archive.** The source is frozen; preserve it and use another path/revision for later art edits. `Artifacts/D121/V2/Preview/Review.html` retains the pre-approval review media. [D121 source contract/integration plan](https://git.shaun-husain.com/CitySimVR/citysim/issues/49#issuecomment-3118) records exact source identity, approval receipt, validation and intended movement-driven runtime playback. User accepted the integrated walk in game on2026-10-15; D121 is complete. [Acceptance receipt](evidence/D121-acceptance.json). Named physical PCVR/Quest observations remain unconfirmed under D114.

## Refined baseball and soccer clips — D88

`ArtSource/Blender/CityCitizenSportsRefinement.blend` contains five editable control actions: BaseballPitch, BaseballSwing, SoccerKick, SoccerRun and GoalkeeperSave. Choose the matching `D88_` scene, press Numpad0 for its authored camera, then play the24fps timeline. Select the scene's `_Rig` and enter Pose Mode to edit its targets/body controls. `D88_BaseballPair` links the two baseball actors and shows their synchronized choreography. Save an artist working copy before changing generated sources.

The pitcher gathers, lifts the lead leg, plants, throws and follows through. The batter loads, swings with both hands on the shaft and follows through. Both now use72 frames: release36, strike49, with the visible ball handed off between their prop tracks at contact. Playback is1–72;73 is the duplicate endpoint. Pitch/swing face the existing mound/home locations. The swing was previously48 frames; its longer period is intentional for the paired timing.

Soccer uses48 frames (49 is the duplicate endpoint). The kick has a planted support foot, backswing and follow-through; the ball stays stationary until impact25. The in-place run uses opposing arms and alternating grounded/recovery feet. The keeper crouches, reaches, catches at25 and gathers the ball before releasing the pose. These are cosmetic presentation loops, not a match simulation. Other sports, walking/idle and the original accepted D83 action sources remain intact.

`CityCitizenSportsMotion.py` authors target/body curves. `Create-CityCitizenSportsRefinement.py` produces the source and manifest; it refuses artist-edited sources when regenerating. `Validate-CityCitizenSportsRefinement.py` reads293 saved frames and checks finite/nondegenerate skin, IK/hinge limits, ground/contact and loop seams. Bat/ball attachments use actual rigid hand-ring mesh centers rather than the old hand-bone tips. `Prepare-CityCitizenSportsRefinement.py` bakes the original20-bone reference, stages five native centimeter clips and six field mesh/LOD plus fallback exports, and performs independent deformed FBX readbacks. No Blender constraints/control bones enter the native skeleton.

`Import-CityCitizenSportsRefinement.ps1` requires an explicit D88 native-window claim and no competing process. It updates only three paired field skeletal meshes, their three paired static fallbacks and five existing animation packages. `Promote-CityCitizenSportsRefinement.py` adopts those manifest rows while retaining original entries; `Generate-CityParkMotionData.py` updates synchronized prop samples. The shared skeleton, unrelated statics, props and other nine clips remain unchanged. `Test-CityCitizenSportsRefinement.ps1` runs only the three affected native park cases, including real CPU skin contacts/grounding for all outfits and both LODs. Native capture and packaging remain deliberate integration operations under the same exclusive window.

Evidence and media are in `Artifacts/D88`:293-frame source checks, five native clip FBX readbacks, warnings-as-errors Editor build, three native cases,16 actual pooled game captures, isolated Windows/Quest packages, five-capture packaged Windows smoke and cooked/embedded content verification. The local Desktop launcher is `Artifacts/D88/Play-D88-Desktop.ps1`; it retains the existing Desktop profile. Source GIFs are under `Preview`; actual native stills are under `NativePreview`. Headset appearance/user animation review remain separate observations. No publication or device installation is performed.

## Citizen-relative sports scale — D88 follow-up

The user accepts the rigged motions overall and requests a relative-scale check before the next delivery. `CityCitizenSportsScale.blend` is a separate derivative with the same five approved control actions, resized park elements and corrected ball contacts. The original D88 source/FBXs remain recovery sources. `Goal-Before`, `Goal-After` and `Soccer-Field` are frozen scale galleries; the five `D88_` scenes remain editable. The frozen keeper stands at the actual runtime offset of -21m, one meter in front of the west goal. Work on a saved artist copy.

Dimensions below are authored meters. Current main-project building transforms use unit scale for authored meshes; the shared board transform preserves these ratios. Citizen height is measured from the casual rest mesh at its .811 carrier scale, about1.43m. Fields retain their compact recreational layout and7x7 logical footprint; shrinking the whole park would also shrink its people and upset all authored contacts.

| Element | Original | Adjusted |
| --- | --- | --- |
| Soccer pitch | 44x30m | 44x30m |
| Goal post centers / post height / depth | 6m /2.4m /2m | 4.8m /2m /1.6m |
| Goal clear height above lawn | 2.37m | 1.908m, about1.33 citizen heights |
| Baseball base width / thickness | .85m /.12m | .42m /.04m |
| Bench length / seat depth / seat height above lawn | 3m /1m /.58m | 2.4m /.6m /.42m |
| Baseball diameter | .11m | .08m |
| Soccer ball diameter | .26m | .22m |
| Bat length | .85m | .85m |

The lawn top now meets the existing .2m animation ground plane. Soccer paint and baseball baseline height/thickness are reduced rather than leaving raised rails across the playing surface. Mound position/height, base centers, end lines, penalty boxes, park entrances/footprint and the approved skeletal clips remain preserved. The smaller soccer ball is repositioned for exact toe contact and grounded running; keeper ball centers use the intersection of the two palm-contact spheres during the hold. This changes prop placement, not approved skin/control curves. The basketball/skate assets are outside this baseball/soccer refinement batch.

`Prepare-CitySportsParkScale.py` produces the separate source, four FBXs and `city_citizen_sports_scale.json`, with mesh/color/UV readbacks. `Validate-CitySportsParkScale.py` reads the saved derivative and compares all five original control actions plus293 contact/loop frames. `Render-CitySportsParkScale.py` and `Label-CitySportsParkScale.py` produce matched source comparison views under `Artifacts/D88/Scale/Preview`.

Native adoption requires the exclusive D88 window after the current owner releases: capture `Check-CitySportsParkScale.py --capture`, run `Import-CitySportsParkScale.ps1`, `Promote-CitySportsParkScale.py` and `Generate-CityParkMotionData.py`, then `Test-CitySportsParkScale.ps1 -BuildEditor`. Only two existing park packages/two existing ball packages are imported; skeletons/clips/catalog/maps are not saved. Main starter/integration manifests adopt those geometries and the five updated prop tracks while retaining prior metadata. `Capture-CityCitizenSportsRefinement.ps1 -ScaleReview` writes18 actual pooled views to the separate Scale/NativePreview directory. Native checks/package evidence are recorded separately after execution; source success alone does not establish native or headset rendering.

Completed native adoption2026-10-13: source/FBX checks, warnings-as-errors Editor, both focused native cases,18 actual pooled captures, Windows/Quest delivery, five-capture Windows packaged smoke and actual cooked/APK-container checks pass. `Artifacts/D88/Scale/Implementation.json` records identities/evidence. Use `Scale/Play-D88-Scale-Desktop.ps1` for an isolated review profile and copied sports city; the new binary includes D89 schema61. The prior D88 launcher/package remains a separate historical version. Actual headset appearance/cadence and user new-size feedback remain pending; no installation/publication is performed.
