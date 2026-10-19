"""Semantic switches for art recipes/imports; task labels are provenance only.

Unmarked primitive entries have no capabilities. Historical snapshots may retain
task fields, but those fields never grant a capability. Unknown or malformed
capabilities fail closed before authoring/import selection.
"""

CAPABILITIES = frozenset({
    'building_refinement',  # Authored building geometry and its larger budget.
    'building_surfaces',    # Authored surface materials and exact slot counts.
    'paired_park_lods',     # Authored paired park LOD exports.
    'reviewed_building',    # Preserve reviewed geometry, collision and pivot.
})


def capabilities(entry):
    values = entry.get('capabilities', [])
    if (not isinstance(values, list) or
            any(not isinstance(value, str) for value in values) or
            len(values) != len(set(values)) or not set(values) <= CAPABILITIES):
        raise ValueError(f"{entry.get('asset_id', '<unnamed>')}: invalid asset capabilities: {values!r}")
    return frozenset(values)


def has_capability(entry, capability):
    if capability not in CAPABILITIES:
        raise ValueError('Unknown asset capability: ' + capability)
    return capability in capabilities(entry)


def set_capability(entry, capability, enabled=True):
    """Update a recipe's output explicitly, preserving all other capabilities."""
    if capability not in CAPABILITIES:
        raise ValueError('Unknown asset capability: ' + capability)
    values = set(capabilities(entry))
    (values.add if enabled else values.discard)(capability)
    entry['capabilities'] = sorted(values)
