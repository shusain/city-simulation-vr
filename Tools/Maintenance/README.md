# Maintenance tools

Native asset organization uses [the explicit Unreal migration](../Assets/Unreal/Organize-CityAssets.py) and its pinned relocation manifest; run it through the coordinated CI preparation window described in [asset organization](../../docs/ASSET_ORGANIZATION.md). It does not authorize replaying completed imports or deleting evidence.

The scoped [soundscape literal repair](../Audio/Soundscape/Repair-CitySoundscapeReferences.py) and [citizen blend repair](../Assets/Unreal/Repair-CityCitizenBlend.py) are completed, input-hash-pinned maintenance recipes. Their shared Editor helpers preserve existing graph/sample settings. They are not routine import or startup commands; fresh native tests remain the authority for their outputs.

`../Plan-CityArtifactCleanup.py` is a completed, version-pinned retirement recipe. Preserve its historical inputs and guards; it is not a general cleanup command. Completed retirement manifests live in [the visual changelog](../../docs/visual-changelog/README.md). New maintenance work should identify its exact inputs and preservation rules before extracting/adopting an operation. See [the supported tool index](../README.md) for reusable path, process, report, manifest and source-verification APIs.
