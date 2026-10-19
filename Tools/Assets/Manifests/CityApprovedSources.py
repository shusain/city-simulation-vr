"""Verify pinned generated exports without changing approved source material."""
from pathlib import Path
import hashlib
import json
import subprocess


def restore_approved_json_checkout(directory, approval_sha256):
    """Recover only exact Git JSON bytes after a Windows merge adds -text.

    A target checkout can already contain CRLF before the integration branch's
    attributes arrive. Unchanged blobs are not necessarily checked out again.
    This never normalizes an approval or accepts a new hash: the raw approval
    must match the independent pin, and every repair must be exactly LF->CRLF.
    """
    directory = Path(directory).resolve()
    root = Path(subprocess.check_output(['git','-C',str(directory),'rev-parse','--show-toplevel'],text=True).strip())

    def blob(path):
        return subprocess.check_output(['git','-C',str(root),'show','HEAD:'+path.relative_to(root).as_posix()])

    raw_approval = blob(directory/'approval.json')
    if hashlib.sha256(raw_approval).hexdigest() != approval_sha256:
        raise ValueError('Git approval differs from independent pin')
    approval = json.loads(raw_approval)
    selected = [('approval.json',approval_sha256)]
    selected += [(entry['name'],entry['sha256']) for entry in approval['artifacts'] if entry['name'].endswith('.json')]
    repairs = []
    for name, expected in selected:
        path = (directory/name).resolve()
        if not path.is_relative_to(directory) or ':' in name or '..' in Path(name).parts:
            raise ValueError('JSON repair escapes export directory')
        current = path.read_bytes()
        if hashlib.sha256(current).hexdigest() == expected:
            continue
        raw = blob(path)
        if hashlib.sha256(raw).hexdigest() != expected or b'\r' in raw or current != raw.replace(b'\n',b'\r\n'):
            raise ValueError('Approved JSON has a non-checkout change: '+name)
        repairs.append((path,raw))
    # Validate the complete plan before writing anything.
    for path,raw in repairs:
        path.write_bytes(raw)
    return [path.relative_to(root).as_posix() for path,_ in repairs]


def verified_export(directory, issue, version, approval_sha256, kind='comfy'):
    directory = Path(directory)
    approval_path = directory / 'approval.json'
    digest = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
    if digest(approval_path) != approval_sha256:
        raise ValueError('Pinned approval hash differs')
    approval = json.loads(approval_path.read_text(encoding='utf-8-sig'))
    handoff = json.loads((directory / 'unreal-handoff.json').read_text(encoding='utf-8-sig'))
    expected = ('CitySimVR/citysim', issue, version, kind)
    if (approval['repository'], approval['issue'], approval['version'], approval['kind']) != expected:
        raise ValueError('Approval identity differs')
    if (handoff['repository'], handoff['asset_issue'], handoff['approved_version'], handoff['kind']) != expected or handoff['approval_sha256'] != approval_sha256:
        raise ValueError('Handoff identity or approval hash differs')
    for manifest in (approval, handoff):
        for entry in manifest['artifacts']:
            name = 'source-bundle.zip' if entry['name'] == 'archive' else entry['name']
            relative = Path(name)
            path = (directory / relative).resolve()
            if relative.is_absolute() or ':' in name or '..' in relative.parts or not path.is_relative_to(directory.resolve()):
                raise ValueError('Artifact escapes export directory')
            if path.stat().st_size != entry['bytes'] or digest(path) != entry['sha256']:
                raise ValueError('Approved artifact differs: ' + name)
    return handoff
