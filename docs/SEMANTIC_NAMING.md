# Semantic source and automation names

Current folders, commands, test fixtures and profiles describe their purpose.
Gitea issue numbers belong in links and provenance. The accepted asset migration
is [#150](https://git.shaun-husain.com/CitySimVR/citysim/issues/150); the remaining
automation/contributor cleanup is [#283](https://git.shaun-husain.com/CitySimVR/citysim/issues/283).

## Maintained interfaces

Use [Tools/README.md](../Tools/README.md) for supported commands. The seven
numbered icon/editorial/room wrappers are retired; their canonical replacements
are recorded in `Tools/Assets/Manifests/CityAssetOrganization.json` and the tools
index. Semantic root build/launch/deploy forwarders retain their parameters.

New test runs write subject names under `Saved/Automation`, `Artifacts/Preview`
or the explicitly selected fresh report directory. They do not move, rename or
overwrite an older numbered profile to migrate it. Existing current-version
player saves and catalog IDs are unchanged. The historical token-to-subject map
is `Tools/Common/CitySemanticNames.json`; exact command-line changes include:

| Purpose | Current interface |
| --- | --- |
| Desktop packaged smoke | `-UserDir` containing `DesktopSmoke-Automation`; `Tools/Test-DesktopSandbox.ps1` creates it |
| Low-density desktop fixture | Isolated profile under `LowDensity/Integration` |
| Reflection proof | Isolated profile under `LowDensity/DynamicReflections` |
| Immutable shopping allocation input/output | `-ShoppingAllocationSave`, `-ShoppingAllocationOutput` |
| Shopping departure regression | `-ShoppingSave`, `-ShoppingOutput` |
| Water refresh regression | `-WaterRefreshSave`, `-WaterRefreshOutput` |
| Outpatient native fixture directory | `-OutpatientFixtureRoot` and isolated `Outpatient` profile |
| Technology, condition, calendar and visual studies | Subject-named isolated profiles matching each test's explicit guard |
| Shared CI reservation | Standalone bold `Drone CI native/source/build window CLAIMED ...` in the primary issue queue |

The shared CI owner is recognized only at the beginning of a claim. Other local
owners, including the former numbered owner, block the job. Do not change an
active owner's header or run an older queued CI wrapper while using the new
shared marker. Finish/drain older work before adopting this revision. Local
native ownership and running-Editor/build guards remain enforced.

Tests, their script consumers, profile guards and completion markers are changed
together. Old manual command lines need updating; no old-profile fallback is
provided. Use a fresh output/profile and the executable built from this source.
An older maintained executable still expects its older automation interface.

The preserved shopping/outpatient captured city currently fails current-catalog
footprint validation before those behavioral checks. [Fixture refresh #285](https://git.shaun-husain.com/CitySimVR/citysim/issues/285)
retains the failure and original input/hash. Naming validation does not establish
that this older city or its dependent visual checks pass on the current catalog.

## Historical exceptions

Keep historical evidence, archives, changelog entries, original approval/source
manifests, frozen acceptance records and generated `issue-N/vN` exports unchanged.
Their identifiers and hashes establish what was actually approved or measured.
Old-to-new mapping manifests deliberately contain old names for traceability.
Serialized source-internal object/take names are also retained; renaming them
would reauthor approved source payloads.

Completed one-shot authoring/import recipes, evidence compositors/verifiers and
hash-pinned captured-city inputs may still reference their original task paths.
They are historical or explicitly pinned operations, not general-purpose setup
commands. Historical verifiers continue to read their original receipts/markers;
do not point them at a new run and claim the old evidence was regenerated.
Where a current test exports a renamed fixture, its direct capture consumers
follow that new output name. Read a recipe before reuse; never replay a completed
native import merely to validate organization.

No native asset or source payload is renamed/reimported by #283. The prior
Unreal-aware asset move and its accepted cook evidence remain authoritative.
