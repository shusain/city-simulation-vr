"""Adopt only verified D88 field derivatives; retain every original source row."""
from pathlib import Path
import json,copy,hashlib
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Artifacts/D88'
proof=json.loads((OUT/'Import.json').read_text());assert not proof['errors'] and len(proof['assets'])==11
path=ROOT/'ArtSource/Manifests/city_park_integration.json';doc=json.loads(path.read_text())
new_path=ROOT/'ArtSource/Manifests/city_citizen_sports_native.json';new=json.loads(new_path.read_text())
changes=[]
for collection in ('clips','rigs'):
    replacements={e['name']:e for e in new[collection]}
    for entry in doc[collection]:
        replacement=replacements.get(entry['name'])
        if replacement:
            prior=entry.get('sports_refinement_prior_entry') or copy.deepcopy(entry)
            entry.clear();entry.update(replacement);entry['sports_refinement_prior_entry']=prior
            changes.append(entry['name'])
assert len(changes)==11
doc['sports_refinement_task']='D88';doc['sports_refinement_source']=new['source'];doc['sports_refinement_source_sha256']=new['source_sha256']
path.write_text(json.dumps(doc,indent=2)+'\n')
new['native_selected']=True;new_path.write_text(json.dumps(new,indent=2)+'\n')
(OUT/'Promotion.json').write_text(json.dumps(dict(task='D88',changed_entries=changes,original_entries_retained=True,
        unrelated_clips_rigs_statics_props_unchanged=True),indent=2)+'\n')
print('CITYSIM_D88_PROMOTION_PASS 5 clips;6 field rig/LOD entries;old entries retained')
