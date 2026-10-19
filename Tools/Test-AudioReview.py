"""Focused isolated checks for audio-feedback persistence, versioning and seeking."""
import copy
import importlib.util
import json
from pathlib import Path
import shutil
import threading
import unittest
import urllib.error
import urllib.request
import uuid
from functools import partial
from http.server import ThreadingHTTPServer
from AudioReview import write_review

ROOT = Path(__file__).resolve().parents[1]
QA = ROOT / "Artifacts/D82/FeedbackReviewQA"
spec = importlib.util.spec_from_file_location("review_server", ROOT / "Tools/Serve-CitySoundtrackReview.py")
server_module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(server_module)


class FeedbackTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        QA.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(ROOT / "Artifacts/D82/Soundtrack1/MP3/01-Afterglow.mp3", QA / "Sample.mp3")
        cls.catalog = write_review(QA, {"title": "Feedback QA (test items)", "assets": [
            {"id": f"qa-{i}", "title": f"Test sample {i}", "audio": "Sample.mp3", "seconds": 210.731708,
             "description": "QA only; this is not a user decision about the soundtrack."} for i in range(1, 4)]})
        handler = partial(server_module.ReviewHandler, directory=str(QA), feedback_dir=QA / "BackendFeedback")
        cls.server = ThreadingHTTPServer(("127.0.0.1", 0), handler)
        cls.base = "http://127.0.0.1:" + str(cls.server.server_port)
        cls.thread = threading.Thread(target=cls.server.serve_forever, daemon=True)
        cls.thread.start()

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()
        cls.server.server_close()
        cls.thread.join()

    def payload(self):
        return {"schema": 1, "reviewId": self.catalog["reviewId"], "submissionId": str(uuid.uuid4()),
                "overall": "QA overall", "entries": [{"id": i["id"], "sha256": i["sha256"],
                "decision": "unreviewed", "notes": ""} for i in self.catalog["items"]]}

    def request(self, path="/api/feedback", payload=None, origin=None, headers=None):
        data = json.dumps(payload).encode() if payload is not None else None
        request_headers = {"Content-Type": "application/json", "Origin": origin or self.base}
        request_headers.update(headers or {})
        request = urllib.request.Request(self.base + path, data=data, headers=request_headers)
        try:
            response = urllib.request.urlopen(request)
        except urllib.error.HTTPError as error:
            response = error
        with response:
            return response.status, response.read(), dict(response.headers)

    def test_persist_partial_idempotent_and_history(self):
        payload = self.payload()
        payload["entries"][0].update(decision="approve", notes="QA: approved")
        payload["entries"][1].update(decision="changes", notes="[0:58] Shorter intro; <script>literal note</script> 🎵")
        status, body, _ = self.request(payload=payload)
        self.assertEqual(status, 200)
        receipt = json.loads(body)
        self.assertEqual(receipt["reviewed"], 2)
        stored = json.loads(Path(receipt["receipt"]).read_text())
        self.assertEqual(stored["entries"][1]["notes"], payload["entries"][1]["notes"])
        self.assertEqual(stored["entries"][2]["decision"], "unreviewed")
        original = Path(receipt["receipt"]).read_bytes()
        self.assertEqual(self.request(payload=payload)[1], body)
        changed = copy.deepcopy(payload)
        changed["overall"] = "QA updated"
        self.assertEqual(self.request(payload=changed)[0], 409)
        changed["submissionId"] = str(uuid.uuid4())
        updated = self.request(payload=changed)
        self.assertEqual(updated[0], 200)
        self.assertEqual(Path(receipt["receipt"]).read_bytes(), original)
        latest = json.loads(self.request("/api/feedback/latest")[1])["feedback"]
        self.assertEqual(latest["submissionId"], changed["submissionId"])

    def test_invalid_feedback_does_not_save(self):
        before = set((QA / "BackendFeedback").rglob("*.json"))
        invalid = []
        request = self.payload(); request["entries"][0]["decision"] = "changes"; invalid.append(request)
        request = self.payload(); request["reviewId"] = "stale-version"; invalid.append(request)
        request = self.payload(); request["entries"][0]["sha256"] = "stale-audio"; invalid.append(request)
        request = self.payload(); request["entries"][1]["id"] = request["entries"][0]["id"]; invalid.append(request)
        request = self.payload(); request["entries"][0]["decision"] = "maybe"; invalid.append(request)
        request = self.payload(); request["entries"][0]["notes"] = "x" * 8001; invalid.append(request)
        request = self.payload(); request["submissionId"] = "../../elsewhere"; invalid.append(request)
        request = self.payload(); request["overall"] = ""; invalid.append(request)
        request = self.payload(); request["entries"].pop(); invalid.append(request)
        for request in invalid:
            self.assertEqual(self.request(payload=request)[0], 400)
        self.assertEqual(self.request(payload=self.payload(), origin="https://other.example")[0], 403)
        self.assertEqual(self.request(payload=self.payload(), headers={"Host": "other.example"})[0], 403)
        self.assertEqual(self.request(payload=self.payload(), headers={"Content-Type": "text/plain"})[0], 415)
        self.assertEqual(self.request("/elsewhere", payload=self.payload())[0], 404)
        # A small body with an oversized declared length checks rejection before reading.
        self.assertEqual(self.request(payload=self.payload(), headers={"Content-Length": "270000"})[0], 413)
        self.assertEqual(before, set((QA / "BackendFeedback").rglob("*.json")))

    def test_audio_ranges_and_reusable_sample_template(self):
        status, body, headers = self.request("/Sample.mp3", headers={"Range": "bytes=1024-2047"})
        self.assertEqual(status, 206)
        self.assertEqual(body, (QA / "Sample.mp3").read_bytes()[1024:2048])
        self.assertEqual(headers["Accept-Ranges"], "bytes")
        self.assertEqual(self.request("/Sample.mp3", headers={"Range": "bytes=999999999-"})[0], 416)
        self.assertNotIn("bpm", self.catalog["items"][0])
        self.assertEqual(len(self.catalog["items"]), 3)
        self.assertIn("Request changes", (QA / "Review.html").read_text())


if __name__ == "__main__":
    unittest.main(verbosity=2)
