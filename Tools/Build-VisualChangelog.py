"""Build a durable gallery from existing captures; never edits or removes sources.

Run with the bundled Python/Pillow documented in ENVIRONMENT.md.
"""

from pathlib import Path
import hashlib
import html
import json
import textwrap

from PIL import Image, ImageDraw, ImageFont, ImageOps

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "docs/visual-changelog"
FONT = Path("C:/Windows/Fonts/segoeui.ttf")


def card(task, title, status, description, *images):
    return dict(task=task, title=title, status=status, description=description,
                images=[dict(source=source, caption=caption) for source, caption in images])


CARDS = [
    card("D38", "The first city art kit", "Accepted foundation; further art deferred",
         "Authored building silhouettes, density families, utilities and construction established the tabletop identity.",
         ("Artifacts/ArtPreview/D38/contact-sheet.png", "Blender starter-kit contact sheet")),
    card("D40", "Roads over and under terrain", "Construction flow accepted; D23 residual checks",
         "Terrain-conforming roads, elevated spans and independent underpass levels became buildable and inspectable.",
         ("Artifacts/Preview/D40-GroundRoads.png", "Ground roads"),
         ("Artifacts/Preview/D40-ElevatedRoads.png", "Elevated spans"),
         ("Artifacts/Preview/D40-UnderpassLowView.png", "Underpass clearance")),
    card("D42", "A readable city history", "Appearance accepted; persistence/performance checks remain",
         "Monthly and annual Reports replace a text-only history with selectable charts and exact values. These captures use a synthetic layout fixture.",
         ("Artifacts/Preview/D42-ReportsMonthly.png", "Monthly Reports"),
         ("Artifacts/Preview/D42-ReportsAnnual.png", "Annual Reports")),
    card("D45", "Fire response and visible damage", "Gameplay accepted; shared D23 gate remains",
         "Authored fire vehicles, response beacons and damaged building roofs make emergencies visible on the board.",
         ("Artifacts/Preview/D45-DamagedRoofs-Front.png", "Damaged roof families"),
         ("Artifacts/Preview/D45-BeaconPair-CrossA.png", "Active response lights"),
         ("Artifacts/Preview/D45-BeaconPair-Parked.png", "Parked lights")),
    card("D46 / D53", "Schools and qualifications", "Menus/staff summary accepted; learning checks pending",
         "Distinct school silhouettes support primary, secondary and higher education. Qualification, capacity and wage progression still have targeted checks.",
         ("Artifacts/Preview/D46-Schools-Front.png", "School family: front"),
         ("Artifacts/Preview/D46-Schools-Back.png", "School family: back")),
    card("D47", "Parks with activities", "Overall appearance accepted; D39/D23 checks remain",
         "Parks gained sports facilities, bounded visitor activity and terrain clearance.",
         ("Artifacts/Preview/D47-Parks-Front.png", "Park family"),
         ("Artifacts/Preview/D47-ParkTerrainActivity-FieldsDay.png", "Sports on terrain"),
         ("Artifacts/Preview/D47-ParkTerrainActivity-Night.png", "Night activity")),
    card("D48", "Clearer civic stations", "Accepted",
         "Refined police/fire stations and Inspect presentation make expansion and operation easier to read.",
         ("Artifacts/Preview/D48-Stations-Front.png", "Station silhouettes"),
         ("Artifacts/Preview/D48-StationInspection-FireDay.png", "Fire station Inspect")),
    card("D49 / D50", "Entering and organizing cities", "Entry/library flow accepted",
         "A menu environment, terrain previews and recoverable city archive/restore support creation and save browsing.",
         ("Artifacts/Preview/D49-EntryMenu-MainDay.png", "Earlier entry menu"),
         ("Artifacts/Preview/D49-EntryScene-Main.png", "Refined entry scene"),
         ("Artifacts/Preview/D50-NewTerrain.png", "Chosen terrain preview"),
         ("Artifacts/Preview/D50-LoadPreview.png", "Saved city preview")),
    card("D51", "Disasters and custody", "Arson/jail accepted; targeted scenarios pending",
         "Earthquake/tornado effects, per-city disaster policy and jail occupancy add visible consequences and recovery scenarios.",
         ("Artifacts/Preview/D51-OnsetQuake.png", "Earthquake onset"),
         ("Artifacts/Preview/D51-OnsetTornado.png", "Tornado onset"),
         ("Artifacts/Preview/D51-JailInspection-PoliceDay.png", "Custody Inspect")),
    card("D52 / D55", "Condition and selection cues", "Accepted",
         "Condition indicators and inspect/demolition highlighting improve target readability in daytime and darkness.",
         ("Artifacts/Preview/D52-Indicators-Day.png", "Condition indicators"),
         ("Artifacts/Preview/D55-SelectionDay.png", "Selection highlights"),
         ("Artifacts/Preview/D55-DemolishNight.png", "Night demolition preview")),
    card("D56", "Utilities with distinctive silhouettes", "Presentation accepted; damage/save checks pending",
         "Alternative generation and water facilities gained authored geometry, damaged states and operational inspection.",
         ("Artifacts/Preview/D56-Facilities-Blender.png", "Utility family"),
         ("Artifacts/Preview/D56-Facilities-Damaged.png", "Damaged variants"),
         ("Artifacts/Preview/D56-UtilityInspection-PowerDay.png", "Output and expansion Inspect")),
    card("D59", "HUD clarity and objectives", "Closed; residual gameplay checks in D39/D23",
         "Successive HUD rounds improved objectives, placement costs and insufficient-funds feedback. Earlier intermediate captures are consolidated here.",
         ("Artifacts/Preview/D59-CityDay.png", "Earlier HUD"),
         ("Artifacts/Preview/D59-Readability-OverviewDay-PIE.png", "Readability round"),
         ("Artifacts/Preview/D59-ObjectiveIcon-OverviewDay-PIE.png", "Accepted objectives round"),
         ("Artifacts/Preview/D59-ObjectiveIcon-BuildingNoFunds-PIE.png", "Unaffordable placement")),
    card("D60", "Health care and crowded markers", "Basic gameplay/markers accepted; clinical checks pending",
         "Hospitals, ambulance lights and bounded illness markers expose treatment and recovery without one Actor per citizen.",
         ("Artifacts/Preview/D60-Hospitals-Front.png", "Hospital silhouettes"),
         ("Artifacts/Preview/D60-Hospital-Inspect-Responding.png", "Ambulance response"),
         ("Artifacts/Preview/D60-Clusters-Dense.png", "Crowded illness markers")),
    card("D61", "Waste collection", "Development accepted; residual D39/D23 checks",
         "Collection/recycling facilities, truck colors and service inspection make the waste loop visible.",
         ("Artifacts/Preview/D61-Waste-Front.png", "Waste facility family"),
         ("Artifacts/Preview/D61-TruckColors-Day.png", "Collection vehicle colors"),
         ("Artifacts/Preview/D61-Waste-Inspect-Recycling.png", "Recycling Inspect")),
    card("D62", "Varied maps and weather", "Rain/snow appearance accepted; transitions pending",
         "Dry, river, lake and mountain terrain plus rain/snow establish climate variety. These native captures predate the accepted faster rain tuning.",
         ("Artifacts/Preview/D62-Dry-PIE.png", "Dry terrain"),
         ("Artifacts/Preview/D62-Mountains-PIE.png", "Mountain terrain"),
         ("Artifacts/Preview/D62-Rain-PIE.png", "Rain presentation"),
         ("Artifacts/Preview/D62-Snow-PIE.png", "Snow presentation")),
    card("D63", "Water supply you can diagnose", "Closed; residual D39/D23 checks",
         "Water rules distinguish source capacity and pressure, with placement guidance and operational overlays.",
         ("Artifacts/Preview/D63-Water-Full-PIE.png", "Supplied city"),
         ("Artifacts/Preview/D63-Water-LowPressure-PIE.png", "Pressure loss"),
         ("Artifacts/Preview/D63-Placement-Day-PIE.png", "Source placement guidance")),
    card("D64 / D65", "Scenery and spatial coverage", "Overall scenery/snow accepted; targeted checks pending",
         "Climate-appropriate palms/pines/rocks and progressive wet/snow coverage vary across the board.",
         ("Artifacts/Preview/D64-WarmCoast-PIE.png", "Warm coast"),
         ("Artifacts/Preview/D64-CoolCoast-PIE.png", "Cool coast"),
         ("Artifacts/Preview/D65-SnowPartial-Day-PIE.png", "Partial snow coverage"),
         ("Artifacts/Preview/D65-SnowFull-Day-PIE.png", "Full snow coverage")),
    card("D66", "Seasons and grounded lots", "Appearance accepted; lifecycle transitions pending",
         "Seasonal trees, park/porch decorations and terrain-clearing lawn/concrete foundations enrich the city calendar.",
         ("Artifacts/Preview/D66-Refinement-Autumn-PIE.png", "Autumn trees"),
         ("Artifacts/Preview/D66-Refinement-FinalTreeParks-PIE.png", "December park dressing"),
         ("Artifacts/ArtPreview/D66-ResidentialGround-Home-Blender.png", "Home foundation study")),
    card("D68", "Desktop controls and city panels", "Integrated; physical/laptop/flow checks pending",
         "Shared actions and information gained keyboard/mouse presentation and resolution-aware desktop panels. Captures show native layout QA.",
         ("Artifacts/D68-ColorPerf-Desktop-1920-1080-Final/Overview.png", "Desktop overview"),
         ("Artifacts/D68-UiReview-Captures/1920-1080/Reports.png", "Desktop Reports"),
         ("Artifacts/D68-ColorPerf-Desktop-1920-1080-Final/Build.png", "Build controls")),
    card("D71", "More detailed PC citizens", "Three static PC variants integrated; device checks pending",
         "Casual, office and worker palettes retain the established height/feet/facing contracts. Quest keeps the baseline; gait and fleet detail remain future work.",
         ("Artifacts/ArtPreview/D71/Citizen-Comparison-Labeled.png", "Labeled baseline/refined citizen comparison")),
    card("D75 / D76 / D77 / D78", "Private development and resumed growth", "Integrated; physical pacing/balance checks pending",
         "Private-funded zoning, faster construction, density-specific demand and saved-city growth continuation are in maintained packages. These UI fixtures show presentation, not a real-budget playtest.",
         ("Artifacts/D75-Desktop/D68-Automation-3840-2160/Saved/Automation/Desktop/Build.png", "Native packaged build controls"),
         ("Artifacts/D78-Desktop-4K-Final/D68-Automation-3840-2160/Saved/Automation/Desktop/CityInfo.png", "Refined City Info charts")),
    card("D79", "Buildings, roofs and shop identity", "Integrated; physical/GPU checks pending",
         "Refined geometry, window shades, surface textures and storefront variants build on the accepted art pipeline. Rooftop metal/reflection repair is integrated.",
         ("Artifacts/ArtPreview/D79/Building-Comparison.png", "Building geometry comparison"),
         ("Artifacts/ArtPreview/D79/Surfaces/Building-Surfaces-Preview.png", "Authored surfaces"),
         ("Artifacts/ArtPreview/D79/Surfaces/Roof-Textures-Preview.png", "Roof texture families"),
         ("Artifacts/ArtPreview/D79/Surfaces/Store-Signs-Preview.png", "Storefront variation")),
    card("D83", "Park and animation prototypes", "Active Blender work; runtime integration pending",
         "Smoother utilities/parks and rigged sports/skate clips are isolated prototypes. This is a dated source-preview snapshot; current originals stay protected.",
         ("Artifacts/ArtPreview/D83/Water-Comparison.png", "Water facility prototype"),
         ("Artifacts/ArtPreview/D83/Tree-Design-Labeled.png", "Flat tree crowns"),
         ("Artifacts/ArtPreview/D83/Skateboard-Size-Comparison.png", "Smaller skateboard"),
         ("Artifacts/ArtPreview/D83/Skate-Tricks-Keyframes.png", "Trick keyframes: authoring only")),
]


def sha(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def build():
    for item in CARDS:
        for image in item["images"]:
            if not (ROOT / image["source"]).is_file():
                raise FileNotFoundError(image["source"])
    OUT.mkdir(parents=True, exist_ok=True)
    images_out = OUT / "images"
    images_out.mkdir(exist_ok=True)
    sheets_out = OUT / "sheets"
    sheets_out.mkdir(exist_ok=True)
    font = ImageFont.truetype(str(FONT), 22)
    title_font = ImageFont.truetype(str(FONT), 32)
    records, sections = [], []
    for index, item in enumerate(CARDS):
        figure_markup = []
        canvas = Image.new("RGB", (1600, 160 + 485 * ((len(item["images"]) + 1) // 2)), "#111b29")
        draw = ImageDraw.Draw(canvas)
        draw.text((30, 20), f'{item["task"]}  |  {item["title"]}', font=title_font, fill="#f1f5f9")
        for line_index, line in enumerate(textwrap.wrap(item["status"], 105)):
            draw.text((30, 70 + line_index * 28), line, font=font, fill="#b4c8dd")
        for j, image in enumerate(item["images"]):
            source = ROOT / image["source"]
            source_hash = sha(source)
            with Image.open(source) as opened:
                original_size = opened.size
                rgb = opened.convert("RGB")
                rgb.thumbnail((1500, 1000), Image.Resampling.LANCZOS)
                name = f'{index + 1:02d}-{j + 1}-{source.stem}.jpg'
                target = images_out / name
                rgb.save(target, quality=89, optimize=True)
                thumb = ImageOps.contain(rgb, (750, 410), Image.Resampling.LANCZOS)
                x, y = 30 + (j % 2) * 780, 155 + (j // 2) * 485
                canvas.paste(thumb, (x + (750 - thumb.width) // 2, y))
                draw.text((x, y + 421), image["caption"], font=font, fill="#f1f5f9")
            assert sha(source) == source_hash, f"Concurrent image edit: {source}"
            record = dict(**image, source_sha256=source_hash, source_bytes=source.stat().st_size,
                          source_dimensions=original_size, output=f"images/{name}",
                          output_sha256=sha(target))
            records.append(record)
            figure_markup.append(f'<figure><a href="images/{name}"><img src="images/{name}" '
                                 f'loading="lazy" alt="{html.escape(image["caption"], quote=True)}"></a>'
                                 f'<figcaption>{html.escape(image["caption"])}</figcaption></figure>')
        sheet_name = f'{index + 1:02d}-{item["task"].replace(" / ", "-")}.jpg'
        canvas.save(sheets_out / sheet_name, quality=90, optimize=True)
        sections.append(f'<section id="milestone-{index + 1}" data-task="{html.escape(item["task"])}">'
                        f'<h2><span>{html.escape(item["task"])}</span> {html.escape(item["title"])}</h2>'
                        f'<p class="status">{html.escape(item["status"])}</p>'
                        f'<p>{html.escape(item["description"])}</p>'
                        f'<div class="figures">{"".join(figure_markup)}</div></section>')
    page = '''<!doctype html><html lang="en"><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1"><title>City Sim VR — visual changelog</title>
<style>
:root{color-scheme:dark light;font-family:Segoe UI,system-ui,sans-serif;background:#101927;color:#edf3fa}
*{box-sizing:border-box}body{max-width:1320px;margin:0 auto;padding:36px 24px}h1{font-size:38px;margin:0 0 12px}
h2{font-size:26px;font-weight:600;margin:0 0 12px}h2 span{color:#95b7d8;font-size:20px}
p{line-height:1.65;max-width:100ch;color:#c6d3e1}.intro{margin-bottom:28px}a{color:#a7ccef}
nav{position:sticky;top:0;background:#101927ee;padding:12px 0;z-index:2;display:flex;gap:12px;flex-wrap:wrap}
button,input{font:inherit;background:#203149;color:#edf3fa;border:1px solid #63758c;border-radius:6px;padding:9px 12px}
input{min-width:220px}button{cursor:pointer}section{padding:30px 0;border-top:1px solid #304054;scroll-margin-top:80px}
.status{margin:0;color:#afcbae}.figures{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:20px}
figure{margin:0;background:#182435;border-radius:8px;overflow:hidden}img{display:block;width:100%;height:320px;object-fit:contain;background:#0b1320}
figcaption{padding:12px 16px;font-size:15px}footer{padding:24px 0;color:#bbc9d9}
@media(max-width:650px){body{padding:20px 16px}h1{font-size:30px}.figures{grid-template-columns:1fr}img{height:auto;max-height:420px}}
@media print{nav{display:none}body{color:#111;background:white}section{break-inside:avoid}p{color:#222}}
</style><body><header><h1>City Sim VR</h1><p class="intro">Visual changelog · consolidated 11 October 2026<br>
Selected milestones from the existing art and native preview captures. Task order shows development milestones, not capture dates.
Desktop/PIE and Blender images do not establish headset appearance, timing or comfort.
<a href="../CHANGELOG.md">Full completed evidence</a> · <a href="../DEVELOPMENT_TASKS.md">Active work and pending checks</a> ·
<a href="cleanup-2026-10-11.json">Cleanup manifest</a></p></header>
<nav aria-label="Gallery controls"><input id="search" type="search" placeholder="Find task or feature" aria-label="Find task or feature"><button id="all" type="button">Show all milestones</button><span id="count" aria-live="polite"></span></nav>
<main>''' + "".join(sections) + '''</main><footer>Original capture paths and SHA256 hashes are recorded in <a href="gallery.json">gallery.json</a>.
The active development plan remains authoritative for status. Active source previews were copied, never retired.</footer>
<script>const input=document.getElementById('search');const sections=[...document.querySelectorAll('main section')];
function filter(){const q=input.value.toLowerCase().trim();let n=0;for(const s of sections){s.hidden=!s.textContent.toLowerCase().includes(q);if(!s.hidden)n++;}document.getElementById('count').textContent=n+' / '+sections.length+' milestones';}
input.addEventListener('input',filter);document.getElementById('all').addEventListener('click',()=>{input.value='';filter();});filter();</script></body></html>'''
    (OUT / "index.html").write_text(page, encoding="utf-8")
    (OUT / "gallery.json").write_text(json.dumps(dict(date="2026-10-11", milestones=CARDS, images=records), indent=2) + "\n", encoding="utf-8")
    # Compact all-milestone overview; individual sheets retain readable labels.
    overview = Image.new("RGB", (1800, 180 + 360 * ((len(CARDS) + 2) // 3)), "#111b29")
    draw = ImageDraw.Draw(overview)
    draw.text((24, 24), "CITY SIM VR  /  Visual changelog", font=title_font, fill="#f1f5f9")
    draw.text((24, 76), "11 October 2026  ·  Existing Blender / native previews  ·  See gallery for status", font=font, fill="#b4c8dd")
    for index, item in enumerate(CARDS):
        first = records[sum(len(c["images"]) for c in CARDS[:index])]
        with Image.open(OUT / first["output"]) as opened:
            thumb = ImageOps.contain(opened, (560, 245), Image.Resampling.LANCZOS)
            x, y = 20 + index % 3 * 600, 165 + index // 3 * 360
            overview.paste(thumb, (x + (560 - thumb.width) // 2, y))
        for line_index, line in enumerate(textwrap.wrap(f'{item["task"]} | {item["title"]}', 44)):
            draw.text((x, y + 255 + line_index * 28), line, font=font, fill="#f1f5f9")
    overview.save(OUT / "overview.jpg", quality=90, optimize=True)
    print(json.dumps(dict(milestones=len(CARDS), captures=len(records), gallery=str(OUT / "index.html"))))


if __name__ == "__main__":
    build()
