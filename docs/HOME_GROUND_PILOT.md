# #125 Compact Home ground pilot

Shaun's next selected step is one building type on more demanding terrain,
before any city-wide baked-paving removal or parcel resizing. PR147 remains
draft. Earlier native174/candidate175 support evidence is retained separately.

The A/brick Compact Home gets a new derivative mesh with its six-face rigid
walk removed. Original approved sources/native meshes and B/C/D/damaged/taller
presentations remain intact. Retained geometry, UVs, window corner colors and
four semantic material slots are copied unchanged; Blender FBX readback passes
at656 triangles. The mapping also covers the identical starter/seasonal body.

The building remains level at the established highest-parcel-sample +40cm floor.
Measured foundations support its structural body, not its16m-square parcel.
The pilot starts a130cm pedestrian walk at the actual native doorway (0,467,0),
with a50cm level landing and solid steps where needed. Its ground portion is
clipped against the rendered terrain triangles, retaining crests/hollows and
cross-slopes. The final metre joins the rendered sidewalk's individual heights;
unpaved roads use the road shoulder. It stops at the public edge rather than
the carriageway center. Side faces extend beneath terrain to close paving edges.

Foundations and pilot walk use shared rough grey concrete with deterministic
seamless aggregate. Standard three-plane texture projection avoids stretching
grain on taller supports; a runtime scalar accounts for miniature board scale.
No custom shader/plugin is introduced. Other authored paths remain unchanged.

Drone182/202e0bd prepared three new native packages (home, texture, material),
passed the warnings-as-errors Editor build154.45s and19 exact checks, including
108 direct native structural presentations and the nonplanar walk cases.
The native study passed ten cases/20 reviewed captures, with sampled endpoint
contact errors below0.001cm and plot relief up to233cm. The stronger generated
city saves/reloads with every terrain sample identical and selects A/brick
under the normal catalog (real entity2), without a transient mesh override.

The three retained native products are byte-verified and committed. Temporary
preparation/visual manifests are removed after their successful pass; normal
delivery builds consume the committed packages. Local Unreal import or duplicate
compilation was unnecessary. Artifacts/D129/HomePilot/Build182 retains original
PNGs, contact metrics, the synthetic save, native products and import receipts.

Native study: flat/uphill/downhill, crest, hollow, wavy terrain in all four
rotations, plus an increased-relief generated city. The generated city must
save/reload with identical terrain samples; synthetic height overrides are
capture-only and never advertised as reproducible player saves. All profiles
and output folders are isolated; previous player files/candidates are preserved.

Native views show closed concrete side faces, the retained doorway threshold,
terrain-following approaches and contact at the sidewalk in all four rotations.
The existing40cm floor/67cm sidewalk convention stays visible in the flat case;
this pilot does not change the city's public paving height policy.

Remaining acceptance: final committed-input CI and identified candidate delivery,
then user review of the one-home path/steps and concrete appearance. Extreme elevations,
partial frontage, other road widths and vehicle driveways need explicit review
before extending this pilot to the other assets. No all-terrain/stair-accessibility,
physical-input, headset or comfort acceptance is claimed by automated geometry.

## Road contact and long-flight rails — requested2026-10-18

Candidate191's home/stairs/concrete appearance is accepted; public road separation
and missing long-flight rails need revision before broader application. Asphalt
moves from terrain+59cm to+4cm, sidewalk+67cm to+12cm (8cm curb). Public
paving clips to the rendered terrain triangles rather than bridging diagonals;
bridge/deck authority stays unchanged while its complete visual assembly moves
55cm lower. Bodies/supports/curbs/paint/traffic follow that datum. Center paint
conforms to terrain; street feet meet the sidewalk rather than the old104cm plane.
The private walk retains its6cm lift and a small6cm final curb blend.

Flights with more than four actual descending risers after terrain merging get
both-side90cm handrails,4cm profiles and posts no more than75cm apart. Rails are
batched in entrance component section2, use the existing road-support material,
and share rebuild/overlay/demolition lifecycle. No source/native asset changes.
Focused CI/native visual validation and a fresh Windows candidate are pending.

## Accepted2026-10-18

Shaun confirms road connection and rails in Windows201 and authorizes merge.
f0a4fbf / dev-cc0c8ac35d4bcbfe3a11 is accepted; Drone199/Windows201
receipts and acceptance are in docs/evidence/home-road-rails-delivery.json and
home-ground-pilot-acceptance.json. Temporary EditorVisualTests selection is
removed at closeout; accepted gameplay Source/Content stay identical. Broader
#125 rollout starts with residential, then commercial, industrial, specialized
buildings. Power/water assets are explicitly deferred until their updates finish.
