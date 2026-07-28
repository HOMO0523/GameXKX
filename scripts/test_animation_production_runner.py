#!/usr/bin/env python3
"""Unit tests for the single-attempt production submission guard."""

from __future__ import annotations

import unittest

from animation_production_runner import (
    PreflightError,
    _remaining_reserved_credit,
    _query_submit_ids,
    _select_entries,
    apply_query_result_state,
    apply_reuse_state,
    preflight_entry,
)


class AnimationProductionRunnerTests(unittest.TestCase):
    def setUp(self) -> None:
        self.entry = {
            "asset_id": "character_00_hero_attack",
            "expected_credits": 70,
            "max_submissions": 1,
            "automatic_retry": False,
            "status": "not_submitted",
        }
        self.state = {
            "status": "not_submitted",
            "submission_count": 0,
            "expected_credits": 70,
        }

    def test_accepts_first_attempt_with_sufficient_credit(self) -> None:
        preflight_entry(self.entry, self.state, live_credit=100)

    def test_rejects_any_second_attempt(self) -> None:
        self.state["submission_count"] = 1
        with self.assertRaisesRegex(PreflightError, "already attempted"):
            preflight_entry(self.entry, self.state, live_credit=100)

    def test_rejects_insufficient_credit(self) -> None:
        with self.assertRaisesRegex(PreflightError, "insufficient credit"):
            preflight_entry(self.entry, self.state, live_credit=69)

    def test_rejects_automatic_retry_configuration(self) -> None:
        self.entry["automatic_retry"] = True
        with self.assertRaisesRegex(PreflightError, "automatic retry"):
            preflight_entry(self.entry, self.state, live_credit=100)

    def test_existing_video_reuse_completes_without_spending_or_submission(self) -> None:
        manifest = {"entries": [self.entry]}
        ledger = {
            "assets": {self.entry["asset_id"]: self.state},
            "successful_count": 0,
            "reused_count": 0,
            "spent_credit_total": 0,
        }

        apply_reuse_state(
            ledger,
            self.entry["asset_id"],
            "SourceAssets/existing.mp4",
            "approved model comparison sample",
        )

        self.assertEqual(self.state["status"], "reused_existing")
        self.assertEqual(self.state["submission_count"], 0)
        self.assertEqual(ledger["reused_count"], 1)
        self.assertEqual(ledger["successful_count"], 1)
        self.assertEqual(ledger["spent_credit_total"], 0)
        self.assertEqual(_remaining_reserved_credit(manifest, ledger), 0)

    def test_success_query_moves_pending_asset_to_downloaded_success(self) -> None:
        self.state.update({"status": "pending", "submission_count": 1})
        ledger = {
            "assets": {self.entry["asset_id"]: self.state},
            "pending_count": 1,
            "successful_count": 0,
            "failed_count": 0,
        }
        result = {
            "gen_status": "success",
            "result_json": {
                "videos": [
                    {
                        "video_url": "https://example.invalid/video.mp4",
                        "fps": 24,
                        "width": 960,
                        "height": 960,
                        "format": "mp4",
                        "duration": 5.042,
                    }
                ]
            },
        }

        apply_query_result_state(
            ledger,
            self.entry["asset_id"],
            result,
            "SourceAssets/raw/video_1.mp4",
        )

        self.assertEqual(self.state["status"], "success")
        self.assertEqual(self.state["downloaded_video"], "SourceAssets/raw/video_1.mp4")
        self.assertEqual(self.state["video_metadata"]["width"], 960)
        self.assertEqual(ledger["pending_count"], 0)
        self.assertEqual(ledger["successful_count"], 1)

    def test_failed_query_is_terminal_and_never_retried(self) -> None:
        self.state.update({"status": "pending", "submission_count": 1})
        ledger = {
            "assets": {self.entry["asset_id"]: self.state},
            "pending_count": 1,
            "successful_count": 0,
            "failed_count": 0,
        }

        apply_query_result_state(
            ledger,
            self.entry["asset_id"],
            {"gen_status": "failed", "message": "backend rejected"},
            None,
        )

        self.assertEqual(self.state["status"], "failed_no_retry")
        self.assertEqual(ledger["pending_count"], 0)
        self.assertEqual(ledger["failed_count"], 1)

    def test_cli_fail_status_is_terminal_and_never_retried(self) -> None:
        self.state.update({"status": "pending", "submission_count": 1})
        ledger = {
            "assets": {self.entry["asset_id"]: self.state},
            "pending_count": 1,
            "successful_count": 0,
            "failed_count": 0,
        }

        apply_query_result_state(
            ledger,
            self.entry["asset_id"],
            {"gen_status": "fail", "fail_reason": "bad gateway"},
            None,
        )

        self.assertEqual(self.state["status"], "failed_no_retry")
        self.assertEqual(self.state["failure"], "bad gateway")
        self.assertEqual(ledger["pending_count"], 0)
        self.assertEqual(ledger["failed_count"], 1)

    def test_batch_selection_only_returns_unsubmitted_entries_up_to_limit(self) -> None:
        entries = [
            {"asset_id": "a"},
            {"asset_id": "b"},
            {"asset_id": "c"},
            {"asset_id": "d"},
        ]
        manifest = {"entries": entries}
        ledger = {
            "assets": {
                "a": {"status": "reused_existing", "submission_count": 0},
                "b": {"status": "not_submitted", "submission_count": 0},
                "c": {"status": "pending", "submission_count": 1},
                "d": {"status": "not_submitted", "submission_count": 0},
            }
        }

        selected = _select_entries(manifest, ledger, limit=1)

        self.assertEqual([entry["asset_id"] for entry in selected], ["b"])

    def test_parallel_query_helper_preserves_asset_mapping(self) -> None:
        pending = [("asset_a", "submit_a"), ("asset_b", "submit_b")]

        results = _query_submit_ids(
            pending,
            lambda arguments: {"submit_id": arguments[1].split("=", 1)[1]},
            max_workers=2,
        )

        self.assertEqual(results[0][0], "asset_a")
        self.assertEqual(results[0][1]["submit_id"], "submit_a")
        self.assertEqual(results[1][0], "asset_b")
        self.assertEqual(results[1][1]["submit_id"], "submit_b")


if __name__ == "__main__":
    unittest.main()
