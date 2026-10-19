"""Compose paginated, labeled sheets from Blender or Unreal preview manifests."""
from pathlib import Path
import argparse
import hashlib
import json
import math
import shutil
from PIL import Image, ImageDraw, ImageFont


def font(size):
    path = Path('C:/Windows/Fonts/segoeui.ttf')
    return ImageFont.truetype(str(path), size) if path.exists() else ImageFont.load_default(size=size)


def fitted_text(draw, text, width, size):
    for value in range(size, 10, -1):
        face = font(value)
        if draw.textlength(text, font=face) <= width:
            return text, face
    face = font(11)
    while text and draw.textlength(text + '...', font=face) > width:
        text = text[:-1]
    return text + '...', face


def compose(manifest, output, columns=3, rows=4, tile_size=384, title=None):
    manifest = manifest.absolute()
    doc = json.loads(manifest.read_text(encoding='utf-8'))
    if doc.get('schema_version') != 1 or not doc.get('source_unchanged') or not doc.get('views'):
        raise ValueError('Expected a successful version-1 preview manifest')
    if not (1 <= columns <= 8 and 1 <= rows <= 8 and 64 <= tile_size <= 1024):
        raise ValueError('Columns/rows must be 1..8; tile size must be 64..1024')
    # Validate the entire render set before producing any sheets.
    for entry in doc['views']:
        if Path(entry['image']).name != entry['image']:
            raise ValueError('Preview image names must be plain filenames')
        path = (manifest.parent/entry['image']).absolute()
        if path.parent != manifest.parent:
            raise ValueError('Preview images must be beside their manifest')
        if hashlib.sha256(path.read_bytes()).hexdigest() != entry['sha256']:
            raise ValueError('Preview image changed: ' + str(path))
        with Image.open(path) as image:
            if image.size != (doc['resolution'], doc['resolution']):
                raise ValueError('Unexpected preview image dimensions: ' + str(path))
    output.mkdir(parents=True, exist_ok=True)
    referenced_renders = output/'ReferencedRenders'
    referenced_renders.mkdir(exist_ok=True)
    columns = min(columns, len(doc['views']))
    capacity = columns * rows
    pages = []
    references = {}
    width = columns * (tile_size + 24) + 48
    for start in range(0, len(doc['views']), capacity):
        entries = doc['views'][start:start + capacity]
        page_rows = math.ceil(len(entries)/columns)
        page_number = len(pages) + 1
        page_file = f'PreviewSheet-{page_number:02}.png'
        page_references = []
        height = 124 + page_rows * (tile_size + 88) + 36
        sheet = Image.new('RGB', (width, height), (24, 30, 40))
        draw = ImageDraw.Draw(sheet)
        heading, face = fitted_text(draw, title or Path(doc['source']).name, width - 48, 26)
        draw.text((24, 14), heading, font=face, fill=(237, 242, 248))
        subtitle = f"Scene: {doc['scene']} | frame {doc['frame']} | {doc['engine']}" + (f" / {doc['color']}" if doc['engine'] == 'workbench' else ' / '+doc.get('render_note', 'studio materials'))
        subtitle, face = fitted_text(draw, subtitle, width - 48, 17)
        draw.text((24, 51), subtitle, font=face, fill=(174, 190, 208))
        for column in range(columns):
            draw.text((48 + column * (tile_size + 24) + tile_size//2, 94), chr(65 + column),
                      anchor='mt', font=font(18), fill=(174, 190, 208))
        for index, entry in enumerate(entries):
            column = chr(65 + index % columns)
            row = index // columns + 1
            cell = f'{column}{row}'
            reference = f'P{page_number:02}-{cell}'
            source_image = manifest.parent/entry['image']
            render_file = referenced_renders/f'P{page_number:02}_{cell}_{entry["image"]}'
            # Keep the original render bytes/resolution and the input manifest valid.
            shutil.copyfile(source_image, render_file)
            references[reference] = dict(cell=cell, page=page_number, column=column, row=row,
                                        sheet=page_file, image=render_file.relative_to(output).as_posix(),
                                        source_image=str(source_image), object=entry['object'],
                                        view=entry['view'], sha256=entry['sha256'])
            page_references.append(reference)
            x = 48 + (index % columns) * (tile_size + 24)
            y = 120 + (index // columns) * (tile_size + 88)
            if index % columns == 0:
                draw.text((24, y + tile_size//2), str(row), anchor='mm', font=font(18), fill=(174, 190, 208))
            with Image.open(render_file) as original:
                image = original.convert('RGB')
                image.thumbnail((tile_size, tile_size), Image.Resampling.LANCZOS)
                sheet.paste(image, (x + (tile_size - image.width)//2, y + (tile_size - image.height)//2))
            draw.rounded_rectangle((x + 6, y + 6, x + 50, y + 36), radius=5, fill=(36, 86, 119))
            draw.text((x + 28, y + 20), cell, anchor='mm', font=font(19), fill=(255, 255, 255))
            label, face = fitted_text(draw, entry['object'], tile_size, 18)
            draw.text((x, y + tile_size + 9), label, font=face, fill=(237, 242, 248))
            dim = ' x '.join(f'{value:.3g}' for value in entry['dimensions_m'])
            line = f"{entry['view']} | {dim} m | {entry['triangles']:,} tris"
            line, face = fitted_text(draw, line, tile_size, 15)
            draw.text((x, y + tile_size + 36), line, font=face, fill=(174, 190, 208))
        draw.text((24, height - 32), f"{doc.get('renderer_label', 'Blender')} mesh previews | source unchanged | page {page_number}", font=font(15), fill=(174, 190, 208))
        destination = output/page_file
        sheet.save(destination)
        pages.append(dict(file=destination.name, dimensions=list(sheet.size), tiles=len(entries),
                          sha256=hashlib.sha256(destination.read_bytes()).hexdigest(), references=page_references))
    report = dict(schema_version=1, manifest=str(manifest), source_sha256=doc['source_sha256'],
                  render_count=len(doc['views']), columns=columns, rows_per_page=rows,
                  pages=pages, references=references)
    (output/'PreviewSheets.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--columns', type=int, default=3)
    parser.add_argument('--rows', type=int, default=4, help='Maximum rows per page')
    parser.add_argument('--tile-size', type=int, default=384)
    parser.add_argument('--title')
    args = parser.parse_args()
    report = compose(args.manifest, (args.output or args.manifest.parent).absolute(), args.columns, args.rows, args.tile_size, args.title)
    print('CITYSIM_PREVIEW_SHEETS_PASS', len(report['pages']), 'pages;', report['render_count'], 'renders')


if __name__ == '__main__':
    main()
