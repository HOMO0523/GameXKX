import json
from pathlib import Path
import tempfile
import threading
import time
import unittest

from gamexxk_dev_client import Client, request_payload, submit_file


class DevFileProtocolTests(unittest.TestCase):
    def test_execution_errors_do_not_pass_a_batch(self):
        class FakeClient(Client):
            def call(self, command, arguments=None, request_id=None):
                if command == "simulate.start":
                    return {"ok": True, "data": {"id": "one"}}
                return {"ok": True, "data": {"id": "one", "done": 1, "total": 1,
                        "running": False, "report": {"errors": 1}}}
        result = FakeClient("files", Path("unused"), 1).simulate({}, 1)
        self.assertFalse(result["ok"])

    def test_no_partial_json_and_same_id_collects_one_result(self):
        with tempfile.TemporaryDirectory() as name:
            root = Path(name)
            (root / "inbox").mkdir()
            (root / "outbox").mkdir()
            seen = []

            def worker():
                deadline = time.monotonic() + 3
                while time.monotonic() < deadline:
                    requests = list((root / "inbox").glob("*.json"))
                    if requests:
                        body = json.loads(requests[0].read_text(encoding="utf-8"))
                        seen.append(body)
                        (root / "outbox" / requests[0].name).write_text(json.dumps({"ok": True, "request_id": body["request_id"]}))
                        return
                    time.sleep(0.01)

            thread = threading.Thread(target=worker)
            thread.start()
            request = request_payload("item.give", {"id": "Currency.Gold", "quantity": 1}, "retry_one")
            first = submit_file(root, request, timeout=3)
            second = submit_file(root, request, timeout=3)
            thread.join(timeout=3)
            self.assertEqual(first, second)
            self.assertEqual(len(seen), 1)

    def test_unsafe_or_conflicting_request_ids_are_rejected(self):
        with self.assertRaises(ValueError):
            request_payload("inspect", {}, "../player")
        with tempfile.TemporaryDirectory() as name:
            root = Path(name)
            (root / "inbox").mkdir()
            (root / "outbox").mkdir()
            old = request_payload("inspect", {}, "same")
            (root / "inbox/same.json").write_text(json.dumps(old))
            with self.assertRaises(ValueError):
                submit_file(root, request_payload("heal", {}, "same"), timeout=0.1)

    def test_timeout_keeps_request_for_later_collection(self):
        with tempfile.TemporaryDirectory() as name:
            root = Path(name)
            (root / "inbox").mkdir()
            (root / "outbox").mkdir()
            with self.assertRaises(TimeoutError):
                submit_file(root, request_payload("inspect", {}, "pending"), timeout=0.01)
            self.assertTrue((root / "inbox/pending.json").is_file())


if __name__ == "__main__":
    unittest.main()
