"""Explicit, pinned Unreal asset move. Run through the shared CI native window.

Uses Unreal rename/fixup operations; never filesystem-renames a native package.
Retains dependency, source identity and changed-file receipts for review/commit.
"""
from pathlib import Path
import hashlib
import json
import sys

sys.path.insert(0, str(next(p for p in Path(__file__).resolve().parents if (p / 'CityToolPaths.py').is_file())))
from CityToolPaths import repository_root


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    import unreal
    root = repository_root(__file__)
    content = root / 'CitySimVR/Content'
    output = root / 'Artifacts/CIAssetPreparation'
    if output.exists() and any(output.iterdir()):
        raise RuntimeError('Preserve the existing asset preparation receipts')
    output.mkdir(parents=True, exist_ok=True)
    plan = json.loads((root / 'Tools/Assets/Manifests/CityAssetOrganization.json').read_text())
    moves = plan['native']
    old_names = {e['old'] for e in moves}
    new_names = {e['new'] for e in moves}
    assert len(old_names) == len(new_names) == len(moves)
    native_file = lambda name: content / (name.removeprefix('/Game/') + '.uasset')
    lib = unreal.EditorAssetLibrary
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    # All project referencers, including the preserved VR template. Scanning
    # every Engine/plugin mount is unnecessary for project-owned packages.
    unreal.log('CITYSIM_ORGANIZATION scanning /Game dependencies before writes')
    registry.scan_paths_synchronous(['/Game'], True)
    unreal.log('CITYSIM_ORGANIZATION project registry scan complete')
    before = {p.relative_to(root).as_posix(): digest(p) for p in content.rglob('*.uasset')}
    (output / 'Before.json').write_text(json.dumps(before, indent=2))
    references = {}
    objects = {}
    for entry in moves:
        assert digest(native_file(entry['old'])) == entry['sha256'], entry['old']
        assert not native_file(entry['new']).exists(), entry['new']
        obj = unreal.load_asset(entry['old'])
        assert obj, entry['old']
        objects[entry['old']] = obj
        refs = [str(p) for p in lib.find_package_referencers_for_asset(entry['old'], True)]
        assert all(p.startswith('/Game/CitySim/') for p in refs), refs
        references[entry['old']] = refs
    unreal.log('CITYSIM_ORGANIZATION pinned packages and referencers verified')
    # Preload direct referencers before the batch rename; Unreal updates loaded
    # hard references, soft references and material/MetaSound/animation graphs.
    referencers = {name: unreal.load_asset(name) for refs in references.values() for name in refs}
    assert all(referencers.values())
    classes = {old: obj.get_class().get_path_name() for old, obj in objects.items()}
    # Native defaults hold these sound/animation assets or their dependencies.
    #216 proved all objects moved, but AssetTools' automatic no-redirector path
    # left122 unchanged originals on disk. Preserve redirectors explicitly for
    # this live set, then use the normal reference-checked redirector fixup.
    # Their C++ replacements must accompany the prepared bytes before delivery.
    redirected = {old for old, obj in objects.items()
                  if isinstance(obj, (unreal.SoundBase, unreal.AnimSequence, unreal.BlendSpace))}
    assert len(redirected) == 124
    rename_data = [unreal.AssetRenameData(objects[e['old']], e['new'].rsplit('/', 1)[0], e['new'].rsplit('/', 1)[1]) for e in moves if e['old'] not in redirected]
    assert unreal.AssetToolsHelpers.get_asset_tools().rename_assets(rename_data), 'Unreal asset rename failed'
    for entry in moves:
        if entry['old'] in redirected:
            assert unreal.CityEditorAssetLibrary.rename_asset_preserving_redirector(
                objects[entry['old']], entry['new'], list(objects.values()) + list(referencers.values())), entry['old']
    unreal.log('CITYSIM_ORGANIZATION Unreal batch rename complete')
    # A hard reference keeps the same UObject pointer during rename, so its
    # owning package can remain clean. Save every referencer after the complete
    # batch, as AssetTools does, before deleting redirectors.
    assert lib.save_loaded_assets(list(objects.values()) + list(referencers.values()), False)
    # AssetTools can already remove unneeded old packages. Fix only actual
    # remaining redirector packages, then require every old path to be gone.
    remaining_redirectors = sorted(name for name in old_names if native_file(name).exists())
    for entry in moves:
        expected = entry['new'] + '.' + entry['new'].rsplit('/', 1)[1]
        actual = objects[entry['old']].get_path_name()
        assert actual == expected, ('Rename did not move the reviewed object', entry['old'], actual, expected)
    for name in remaining_redirectors:
        assert digest(native_file(name)) != next(e['sha256'] for e in moves if e['old'] == name), ('Unchanged original remains', name)
        unreal.log('CITYSIM_ORGANIZATION remaining old file ' + name + ' sha256=' + digest(native_file(name)))
    if remaining_redirectors:
        assert unreal.CityEditorAssetLibrary.remove_unreferenced_redirectors(remaining_redirectors), 'Referenced or unresolved redirectors; inspect native log'
    # Repoint moved source files without reimporting or altering the approved data.
    repointed = []
    registry.scan_paths_synchronous(['/Game/CitySim'], True)
    for data in registry.get_assets_by_path('/Game/CitySim', recursive=True):
        name = str(data.package_name)
        obj = data.get_asset()
        if not obj:
            raise RuntimeError('Cannot load native package: ' + name)
        for index, filename in enumerate(unreal.CityEditorAssetLibrary.get_imported_source_files(obj)):
            normalized = filename.replace('\\', '/')
            for source in plan['sources']:
                if normalized.endswith('/' + source['old']) or normalized == source['old']:
                    target = root / source['new']
                    # Config/manifests can change references; binary/import payloads cannot.
                    assert digest(target) == source['sha256'], source['new']
                    assert unreal.CityEditorAssetLibrary.repoint_imported_source(obj, str(target), index)
                    obj.modify()
                    assert lib.save_loaded_asset(obj, False)
                    updated = unreal.CityEditorAssetLibrary.get_imported_source_files(obj)[index]
                    assert Path(updated).resolve() == target.resolve(), ('Import filename differs', name, updated)
                    assert digest(Path(updated)) == source['sha256'], ('Import payload differs', name, updated)
                    repointed.append(dict(package=name, source=source['new'], sha256=source['sha256']))
                    break
    registry.scan_paths_synchronous(['/Game/CitySim'], True)
    dependency_options = unreal.AssetRegistryDependencyOptions(include_soft_package_references=True, include_hard_package_references=True)
    for entry in moves:
        assert not native_file(entry['old']).exists(), entry['old']
        obj = unreal.load_asset(entry['new'])
        assert obj and obj.get_class().get_path_name() == classes[entry['old']], entry['new']
    stale = {}
    dependency_packages = set()
    for data in registry.get_assets_by_path('/Game/CitySim', recursive=True):
        name = str(data.package_name)
        dependency_packages.add(name)
        # The Python binding returns None when a package has no dependencies.
        deps = {str(p) for p in (registry.get_dependencies(name, dependency_options) or [])}
        if deps & old_names:
            stale[name] = sorted(deps & old_names)
    assert not stale, stale
    assert new_names <= dependency_packages, ('Destination missing from dependency audit', sorted(new_names - dependency_packages))
    after = {p.relative_to(root).as_posix(): digest(p) for p in content.rglob('*.uasset')}
    changed = sorted(p for p, sha in after.items() if before.get(p) != sha)
    removed = sorted(set(before) - set(after))
    assert set(removed) == {native_file(e['old']).relative_to(root).as_posix() for e in moves}
    assert all(p.startswith('CitySimVR/Content/CitySim/') for p in changed)
    workspace_marker = next((part for part in root.parts if part.startswith('drone-')), None)
    if workspace_marker:
        markers = (workspace_marker.encode(), workspace_marker.encode('utf-16-le'))
        for name in changed:
            data = (root / name).read_bytes()
            assert not any(marker in data for marker in markers), ('Ephemeral workspace in native output', name)
    receipt = dict(status='passed', renamed=len(moves), references=references, classes=classes,
                   redirectors_removed=len(remaining_redirectors), dependency_packages_checked=len(dependency_packages),
                   portable_import_filenames=True, ephemeral_workspace_strings=False,
                   repointed=repointed, changed={p: after[p] for p in changed}, removed=removed,
                   stale_dependencies=stale, method='Unreal AssetTools/ObjectTools rename + soft-path/dependent saves + verified unreferenced redirector deletion; no reimport')
    (output / 'Organization.json').write_text(json.dumps(receipt, indent=2) + '\n')
    (output / 'Files.txt').write_text('\n'.join(changed) + '\n')
    print('CITYSIM_ASSET_ORGANIZATION_PASS', len(moves), 'renames;', len(changed), 'saved packages;', len(repointed), 'source references')


if __name__ == '__main__':
    main()
