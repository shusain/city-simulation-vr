"""Render selected meshes from a saved blend into a preview manifest and PNGs.

blender --background --factory-startup --disable-autoexec --python-exit-code 1
    --python Tools/Render-CityBlendPreviews.py -- --blend file.blend --output folder
"""
from pathlib import Path
import argparse
import fnmatch
import json
import re
import sys
import bpy

from pathlib import Path
import sys
sys.path.insert(0, str(next(p for p in Path(__file__).resolve().parents if (p / "CityToolPaths.py").is_file())))
from Assets.Blender.CityAssetPreview import VIEWS, digest, preview_scene, render_asset


def visible_meshes(scene):
    renderable = set()
    def visit(collection):
        if collection.hide_render:
            return
        renderable.update(collection.objects)
        for child in collection.children:
            visit(child)
    visit(scene.collection)
    return {obj for obj in scene.objects if obj.type == 'MESH' and obj in renderable
            and not obj.hide_render and obj.visible_get()}


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--blend', type=Path, required=True)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--list', action='store_true', help='List source scenes/mesh names without rendering')
    parser.add_argument('--objects', action='append', default=[], help='Repeatable case-sensitive mesh-name wildcard; default: all meshes')
    parser.add_argument('--scene', help='Source scene name; default: saved active scene')
    parser.add_argument('--frame', type=int, help='Pose frame; default: saved scene frame')
    parser.add_argument('--views', nargs='+', choices=VIEWS, default=['iso'])
    parser.add_argument('--resolution', type=int, default=512)
    parser.add_argument('--engine', choices=['workbench', 'eevee'], default='workbench')
    parser.add_argument('--color', choices=['material', 'vertex', 'object'], default='material', help='Workbench color source; Eevee uses actual material nodes')
    parser.add_argument('--include-hidden', action='store_true', help='Include hidden meshes/LODs in the selected source scene')
    args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else [])
    if args.resolution < 64 or args.resolution > 4096:
        parser.error('--resolution must be between 64 and 4096')
    if not args.list and args.output is None:
        parser.error('--output is required unless --list is used')
    return args


def main():
    args = parse_args()
    source = args.blend.absolute()
    if source.suffix.lower() != '.blend':
        raise ValueError('--blend must name a .blend file')
    before = digest(source)
    bpy.ops.wm.open_mainfile(filepath=str(source))
    scene = bpy.data.scenes.get(args.scene) if args.scene else bpy.context.scene
    if scene is None:
        raise ValueError('Source scene not found: ' + str(args.scene))
    bpy.context.window.scene = scene
    visible = visible_meshes(scene)
    if args.list:
        print('Scenes:', ', '.join(s.name for s in bpy.data.scenes))
        print('Selected scene:', scene.name, '| saved frame:', scene.frame_current)
        for obj in sorted((o for o in scene.objects if o.type == 'MESH'), key=lambda o: o.name):
            print('MESH', obj.name, '| visible' if obj in visible else '| hidden')
        assert digest(source) == before, 'Source blend changed during listing'
        print('CITYSIM_BLEND_OBJECT_LIST_PASS')
        return
    scene.frame_set(args.frame if args.frame is not None else scene.frame_current)
    objects = sorted([o for o in scene.objects if o.type == 'MESH'
                      and (args.include_hidden or o in visible)
                      and (not args.objects or any(fnmatch.fnmatchcase(o.name, pattern) for pattern in args.objects))], key=lambda o: o.name)
    if not objects:
        raise ValueError('No meshes match the scene/visibility/name filters')
    output = args.output.absolute()
    output.mkdir(parents=True, exist_ok=True)
    render_scene = preview_scene(args.resolution, args.engine, args.color)
    views = list(dict.fromkeys(args.views))
    rows = []
    for index, obj in enumerate(objects, 1):
        slug = re.sub(r'[^A-Za-z0-9_-]', '_', obj.name)[:100] or 'Mesh'
        filename = lambda view: output / f'{index:03}_{slug}_{view}.png'
        for row in render_asset(obj, scene, render_scene, filename, views, args.engine):
            rows.append(dict(object=obj.name, **row))
    assert digest(source) == before, 'Source blend changed during previews'
    report = dict(schema_version=1, source=str(source), source_sha256=before, source_unchanged=True,
                  scene=scene.name, frame=scene.frame_current, engine=args.engine, color=args.color,
                  resolution=args.resolution, object_patterns=args.objects, include_hidden=args.include_hidden, object_count=len(objects), views=rows,
                  limitations='One evaluated mesh per tile; Eevee studio lighting, Workbench ignores material textures; source is never saved')
    (output/'PreviewManifest.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('CITYSIM_BLEND_PREVIEWS_PASS', len(objects), 'meshes;', len(rows), 'renders;', output/'PreviewManifest.json')


if __name__ == '__main__':
    main()
