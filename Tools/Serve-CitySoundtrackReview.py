"""Serve seekable audio auditions and capture versioned local feedback (loopback only)."""
import argparse
from datetime import datetime, timezone
import functools
import hashlib
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import re
import threading
from urllib.parse import urlsplit
import uuid

WRITE_LOCK = threading.Lock()


class ReviewHandler(SimpleHTTPRequestHandler):
    def __init__(self, *args, feedback_dir=None, **kwargs):
        self.feedback_dir = Path(feedback_dir)
        super().__init__(*args, **kwargs)

    def catalog(self):
        return json.loads((Path(self.directory) / "ReviewCatalog.json").read_text(encoding="utf-8"))

    def json_response(self, status, value):
        body = json.dumps(value, ensure_ascii=True).encode()
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(body)

    def allowed_host(self):
        port = self.server.server_port
        return self.headers.get("Host") in (f"127.0.0.1:{port}", f"localhost:{port}")

    def do_GET(self):
        if not self.allowed_host():
            return self.json_response(403, {"error": "Use the local review address"})
        if urlsplit(self.path).path == "/api/feedback/latest":
            try:
                collection = self.feedback_dir / self.catalog()["reviewId"]
                records = [json.loads(p.read_text(encoding="utf-8")) for p in collection.glob("*.json")]
                latest = max(records, key=lambda r: r["submittedUTC"]) if records else None
                return self.json_response(200, {"feedback": latest})
            except (OSError, ValueError, KeyError):
                return self.json_response(500, {"error": "Could not read saved feedback"})
        super().do_GET()

    def do_POST(self):
        origin = self.headers.get("Origin")
        if not self.allowed_host() or origin != "http://" + self.headers.get("Host", ""):
            return self.json_response(403, {"error": "Submit from this local review page"})
        if urlsplit(self.path).path != "/api/feedback":
            return self.json_response(404, {"error": "Unknown feedback endpoint"})
        if self.headers.get("Content-Type", "").split(";")[0].strip() != "application/json":
            return self.json_response(415, {"error": "Expected JSON feedback"})
        try:
            size = int(self.headers.get("Content-Length", "0"))
            if not 0 < size <= 262144:
                return self.json_response(413, {"error": "Feedback is too large or empty"})
            request = json.loads(self.rfile.read(size))
            catalog = self.catalog()
            content, submission_id = validate_feedback(catalog, request)
            canonical = json.dumps(content, ensure_ascii=True, sort_keys=True).encode()
            content_hash = hashlib.sha256(canonical).hexdigest()
            collection = self.feedback_dir / catalog["reviewId"]
            record_path = collection / (submission_id + ".json")
            with WRITE_LOCK:
                if record_path.exists():
                    record = json.loads(record_path.read_text(encoding="utf-8"))
                    if record["contentSha256"] != content_hash:
                        return self.json_response(409, {"error": "Submission ID already used for different feedback"})
                else:
                    collection.mkdir(parents=True, exist_ok=True)
                    record = dict(content, title=catalog["title"], submissionId=submission_id,
                                  submittedUTC=datetime.now(timezone.utc).isoformat(), contentSha256=content_hash)
                    temporary = record_path.with_suffix(".tmp")
                    with temporary.open("x", encoding="utf-8") as stream:
                        json.dump(record, stream, indent=2, ensure_ascii=True)
                    temporary.rename(record_path)
            self.json_response(200, {"receipt": str(record_path), "submittedUTC": record["submittedUTC"],
                                     "reviewed": sum(e["decision"] != "unreviewed" for e in record["entries"])})
        except (ValueError, KeyError, TypeError, AttributeError) as error:
            self.json_response(400, {"error": str(error) or "Invalid feedback"})
        except OSError:
            self.json_response(500, {"error": "Could not save feedback; draft can be retried or exported"})

    def send_head(self):
        self.remaining = None
        path = Path(self.translate_path(self.path))
        requested = self.headers.get("Range")
        if not requested or not path.is_file():
            return super().send_head()
        size = path.stat().st_size
        match = re.fullmatch(r"bytes=(\d*)-(\d*)", requested.strip())
        try:
            if not match or not any(match.groups()) or not size:
                raise ValueError()
            first, last = match.groups()
            if first:
                start = int(first)
                end = min(int(last), size - 1) if last else size - 1
            else:
                suffix = int(last)
                if suffix <= 0:
                    raise ValueError()
                start, end = max(0, size - suffix), size - 1
            if start >= size or end < start:
                raise ValueError()
        except ValueError:
            self.send_response(416)
            self.send_header("Content-Range", f"bytes */{size}")
            self.send_header("Content-Length", "0")
            self.end_headers()
            return None
        stream = path.open("rb")
        stream.seek(start)
        self.remaining = end - start + 1
        self.send_response(206)
        self.send_header("Content-Type", self.guess_type(str(path)))
        self.send_header("Content-Range", f"bytes {start}-{end}/{size}")
        self.send_header("Content-Length", str(self.remaining))
        self.send_header("Last-Modified", self.date_time_string(path.stat().st_mtime))
        self.end_headers()
        return stream

    def end_headers(self):
        self.send_header("Accept-Ranges", "bytes")
        self.send_header("Cache-Control", "no-cache")
        super().end_headers()

    def copyfile(self, source, outputfile):
        try:
            if self.remaining is None:
                return super().copyfile(source, outputfile)
            while self.remaining:
                data = source.read(min(65536, self.remaining))
                if not data:
                    break
                outputfile.write(data)
                self.remaining -= len(data)
        except (BrokenPipeError, ConnectionResetError):
            pass  # Audio players cancel obsolete requests when switching songs.


def validate_feedback(catalog, request):
    if not isinstance(request, dict) or request.get("schema") != 1 or request.get("reviewId") != catalog["reviewId"]:
        raise ValueError("Audio collection changed; refresh before submitting")
    submission_id = str(uuid.UUID(request.get("submissionId", "")))
    overall = request.get("overall", "")
    if not isinstance(overall, str) or len(overall) > 8000:
        raise ValueError("Overall notes must be at most 8000 characters")
    entries = request.get("entries")
    if not isinstance(entries, list) or len(entries) != len(catalog["items"]):
        raise ValueError("Feedback must identify every item; unreviewed items are allowed")
    by_id = {}
    for entry in entries:
        if not isinstance(entry, dict) or not isinstance(entry.get("id"), str) or entry["id"] in by_id:
            raise ValueError("Invalid or duplicate audio item")
        by_id[entry["id"]] = entry
    normalized = []
    for item in catalog["items"]:
        entry = by_id.get(item["id"], {})
        if entry.get("sha256") != item["sha256"]:
            raise ValueError("Audio version changed; refresh before submitting")
        decision, notes = entry.get("decision"), entry.get("notes", "")
        if decision not in ("approve", "reject", "changes", "unreviewed"):
            raise ValueError("Unknown decision")
        if not isinstance(notes, str) or len(notes) > 8000:
            raise ValueError("Item notes must be at most 8000 characters")
        if decision == "changes" and not notes.strip():
            raise ValueError("Describe requested changes for " + item["title"])
        normalized.append({"id": item["id"], "title": item["title"], "audio": item["audio"],
                           "sha256": item["sha256"], "decision": decision, "notes": notes})
    if not overall.strip() and not any(e["decision"] != "unreviewed" or e["notes"].strip() for e in normalized):
        raise ValueError("Record a decision or note before submitting")
    return {"schema": 1, "reviewId": catalog["reviewId"], "overall": overall, "entries": normalized}, submission_id


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--port", type=int, default=8762)
    parser.add_argument("--feedback-dir", type=Path, help="Defaults to ROOT/Feedback; use a separate folder for QA")
    args = parser.parse_args()
    root = args.root.resolve(strict=True)
    if not root.is_dir():
        parser.error("Root must be a directory")
    feedback = (args.feedback_dir or root / "Feedback").resolve()
    handler = functools.partial(ReviewHandler, directory=str(root), feedback_dir=feedback)
    with ThreadingHTTPServer(("127.0.0.1", args.port), handler) as server:
        print(f"http://127.0.0.1:{args.port}/Review.html", flush=True)
        server.serve_forever()


if __name__ == "__main__":
    main()
