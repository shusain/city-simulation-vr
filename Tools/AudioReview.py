"""Build a reusable, portable audio audition page from tracks or sample assets."""
import argparse
import hashlib
import json
from pathlib import Path

TEMPLATE = Path(__file__).with_name("AudioReview.html")


def write_review(output, manifest):
    output = Path(output)
    items = []
    for source in manifest.get("tracks", manifest.get("assets", [])):
        audio = source.get("mp3", source.get("audio"))
        path = (output / audio).resolve()
        if not path.is_relative_to(output.resolve()) or not path.is_file():
            raise ValueError("Audio must exist within the review folder")
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        if source.get("mp3Sha256", source.get("sha256", digest)) != digest:
            raise ValueError("Audio identity changed")
        seconds = source.get("seconds", 0)
        rounded = round(seconds)
        item = {"id": source["id"], "title": source.get("title", source["id"]),
                "audio": Path(audio).as_posix(), "sha256": digest,
                "duration": source.get("duration", f"{rounded // 60}:{rounded % 60:02d}"),
                "seconds": seconds, "description": source.get("intent", source.get("description", "")),
                "sections": source.get("sections", [])}
        if "bpm" in source:
            item["bpm"] = source["bpm"]
        items.append(item)
    if not items or len(items) > 1000 or len({i["id"] for i in items}) != len(items):
        raise ValueError("Expected unique audio items")
    identity = json.dumps([(i["id"], i["sha256"]) for i in items], separators=(",", ":"))
    catalog = {"schema": 1, "reviewId": hashlib.sha256(identity.encode()).hexdigest(),
               "title": manifest.get("title", "Audio review"), "task": manifest.get("task", ""),
               "items": items, "downloads": manifest.get("reviewDownloads", [])}
    if not catalog["downloads"] and (output / "CitySimVR-Initial-Soundtrack.m3u").exists():
        catalog["downloads"] = [
            {"label": "Download MP3 review set", "path": "CitySimVR-Initial-Soundtrack-Feedback-MP3.zip", "serverOnly": True},
            {"label": "Download playlist", "path": "CitySimVR-Initial-Soundtrack.m3u"}]
    data = json.dumps(catalog, ensure_ascii=True).replace("<", "\\u003c")
    page = TEMPLATE.read_text(encoding="utf-8").replace("__CATALOG__", data)
    (output / "ReviewCatalog.json").write_text(json.dumps(catalog, indent=2), encoding="utf-8")
    (output / "Review.html").write_text(page, encoding="utf-8")
    return catalog


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    catalog = write_review(args.output, json.loads(args.manifest.read_text(encoding="utf-8")))
    print(json.dumps({"reviewId": catalog["reviewId"], "items": len(catalog["items"])}))


if __name__ == "__main__":
    main()
