import io
import json
import tempfile
import unittest
from pathlib import Path


from scripts.run_card_balance_observation import (
    aggregate_case_rows,
    audit_card_catalog,
    load_existing_runs,
    parse_metric_map,
    read_case_rows,
    validate_automation_report,
    write_final_summary,
)


CASE_CSV = """schema_version,cohort,quest_npc,equipment_set,equipment_quality,enhancement,chapter,node,seed_ordinal,seed,outcome,rounds,remaining_party_health,first_round_deaths,active_cards,automatic_resolutions,energy_spent,energy_gained,mana_spent,mana_gained,healing_generated,armor_generated,stranded_target_failures,maximum_queue_depth,maximum_hand_size,damage_by_source,damage_by_origin,healing_by_source,armor_by_source,status_produced,status_consumed,error
2,NakedBaseline,Npc.TusiChief,None,Common,0,1,Battle,0,900000,Victory,4,120,0,8,2,7,3,9,4,12,6,0,4,9,Player=100,Direct=80;Reaction=20,Healer=12,Guard=6,Burn=2,Burn=1,
2,NakedBaseline,Npc.TusiChief,None,Common,0,1,Battle,1,900001,Defeat,6,0,0,10,1,9,2,12,1,4,3,1,5,10,Player=50,Direct=50,,,,,
2,NakedBaseline,Npc.TusiChief,None,Common,0,1,Elite,2,901002,Stalemate,100,80,0,6,0,6,0,8,0,2,2,0,3,8,,,,,,,Simulation.MaxRounds
"""

LEGACY_CASE_CSV = """cohort,quest_npc,equipment_set,equipment_quality,enhancement,chapter,node,seed_ordinal,seed,outcome,rounds,remaining_party_health,first_round_deaths,damage_by_source,healing_by_source,armor_by_source,status_produced,status_consumed,error
NakedBaseline,Npc.TusiChief,None,Common,0,1,Battle,0,900000,Victory,4,120,0,Player=100,,,Burn=2,Burn=1,
"""

PROJECT_ROOT = Path(__file__).resolve().parents[1]


class CardBalanceObservationParserTests(unittest.TestCase):
    def test_metric_map_rejects_malformed_items(self):
        with self.assertRaisesRegex(ValueError, "invalid metric item"):
            parse_metric_map("Burn=2;missing-value")

    def test_aggregate_preserves_outcomes_and_status_utilization(self):
        rows = read_case_rows(io.StringIO(CASE_CSV))

        summary = aggregate_case_rows(rows, expected_case_count=3)

        self.assertEqual(
            summary["outcomes"],
            {"Victory": 1, "Defeat": 1, "Stalemate": 1},
        )
        self.assertEqual(
            summary["status_utilization"]["Burn"],
            {"produced": 2, "consumed": 1, "ratio": 0.5},
        )
        self.assertEqual(summary["resolved_rounds"], {"median": 5.0, "p90": 6})
        self.assertEqual(summary["cohorts"]["NakedBaseline"]["case_count"], 3)
        self.assertEqual(summary["chapter_nodes"]["1|Battle"]["case_count"], 2)
        self.assertEqual(
            summary["runtime_totals"],
            {
                "active_cards": 24,
                "automatic_resolutions": 3,
                "energy_spent": 22,
                "energy_gained": 5,
                "mana_spent": 29,
                "mana_gained": 5,
                "healing_generated": 18,
                "armor_generated": 11,
                "stranded_target_failures": 1,
            },
        )
        self.assertEqual(
            summary["runtime_maxima"],
            {"maximum_queue_depth": 5, "maximum_hand_size": 10},
        )
        self.assertEqual(
            summary["stranded_target_cases"],
            ["NakedBaseline|1|Battle|900001"],
        )
        self.assertEqual(
            summary["top_sources"]["damage_by_origin"][:2],
            [
                {"source": "Direct", "value": 130},
                {"source": "Reaction", "value": 20},
            ],
        )
        self.assertEqual(
            summary["recurring_stalemates"],
            ["NakedBaseline|1|Elite|901002"],
        )

    def test_aggregate_rejects_duplicate_case_identity(self):
        rows = read_case_rows(io.StringIO(CASE_CSV))
        rows.append(dict(rows[0]))

        with self.assertRaisesRegex(ValueError, "case identities are not unique"):
            aggregate_case_rows(rows, expected_case_count=4)

    def test_aggregate_rejects_wrong_case_count(self):
        rows = read_case_rows(io.StringIO(CASE_CSV))

        with self.assertRaisesRegex(ValueError, "expected 2400 cases, found 3"):
            aggregate_case_rows(rows)


class CardBalanceObservationCatalogTests(unittest.TestCase):
    def test_catalog_audit_finds_quality_energy_and_zero_cost_draw_risk(self):
        audit = audit_card_catalog(PROJECT_ROOT)

        self.assertEqual(audit["card_count"], 198)
        self.assertEqual(
            audit["quality_counts"],
            {"Common": 122, "Rare": 47, "Epic": 29},
        )
        self.assertEqual(
            audit["energy_counts"],
            {"0": 33, "1": 89, "2": 55, "3": 21},
        )
        self.assertEqual(
            audit["owner_counts"],
            {"Hero": 36, "Profession": 108, "QuestNpc": 24, "Route": 30},
        )
        formation_setup_cards = [
            card_id
            for card_id in audit["setup_only_for_greedy_policy_cards"]
            if card_id.startswith("Profession.FormationMaster.")
        ]
        self.assertEqual(len(formation_setup_cards), 9)
        self.assertIn("Profession.FormationMaster.GuanShi", formation_setup_cards)
        self.assertIn("Profession.FormationMaster.ZhenQiGuWu", formation_setup_cards)
        self.assertIn(
            "Npc.JinGui.ShiJingErMu",
            audit["zero_cost_draw_cards"],
        )
        self.assertIn(
            [
                "Profession.Guard.BuDongRuShan",
                "Profession.Guard.YiFuDangGuan",
            ],
            audit["exact_duplicate_effect_groups"],
        )
        formation_dominance = next(
            group
            for group in audit["strict_energy_dominance_groups"]
            if {
                card["id"] for card in group["cards"]
            }
            == {
                "Profession.FormationMaster.LinFengFuZhen",
                "Profession.FormationMaster.LinYingMiZong",
            }
        )
        self.assertEqual(
            formation_dominance["cards"],
            [
                {
                    "id": "Profession.FormationMaster.LinFengFuZhen",
                    "energy": 0,
                },
                {
                    "id": "Profession.FormationMaster.LinYingMiZong",
                    "energy": 1,
                },
            ],
        )


class CardBalanceObservationReportTests(unittest.TestCase):
    def test_report_validation_accepts_one_success_with_warnings(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "index.json"
            path.write_text(
                json.dumps(
                    {
                        "succeeded": 0,
                        "succeededWithWarnings": 1,
                        "failed": 0,
                        "notRun": 0,
                        "tests": [{"state": "SuccessWithWarnings"}],
                    }
                ),
                encoding="utf-8",
            )

            report = validate_automation_report(path)

            self.assertEqual(report["succeededWithWarnings"], 1)

    def test_report_validation_rejects_failed_automation(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "index.json"
            path.write_text(
                json.dumps(
                    {
                        "succeeded": 0,
                        "succeededWithWarnings": 0,
                        "failed": 1,
                        "notRun": 0,
                        "tests": [{"state": "Fail"}],
                    }
                ),
                encoding="utf-8",
            )

            with self.assertRaisesRegex(RuntimeError, "diagnostic automation failed"):
                validate_automation_report(path)


class CardBalanceObservationSummaryTests(unittest.TestCase):
    def test_existing_runs_are_reaggregated_from_their_csv(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            run_root = root / "run_a"
            run_root.mkdir()
            csv_path = run_root / "cases.csv"
            csv_path.write_text(CASE_CSV, encoding="utf-8")
            (run_root / "run_summary.json").write_text(
                json.dumps(
                    {
                        "run_id": "run_a",
                        "csv_sha256": "stale-hash",
                        "csv_path": str(csv_path),
                        "duration_seconds": 1.25,
                        "summary": {"case_count": 3},
                    }
                ),
                encoding="utf-8",
            )

            records = load_existing_runs(root, expected_case_count=3)

            self.assertEqual(len(records), 1)
            self.assertEqual(
                records[0]["summary"]["cohorts"]["NakedBaseline"]["case_count"],
                3,
            )
            self.assertNotEqual(records[0]["csv_sha256"], "stale-hash")

    def test_existing_runs_skip_legacy_csv_schema(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            run_root = root / "legacy_run"
            run_root.mkdir()
            csv_path = run_root / "cases.csv"
            csv_path.write_text(LEGACY_CASE_CSV, encoding="utf-8")
            (run_root / "run_summary.json").write_text(
                json.dumps(
                    {
                        "run_id": "legacy_run",
                        "csv_path": str(csv_path),
                        "duration_seconds": 1.0,
                    }
                ),
                encoding="utf-8",
            )

            self.assertEqual(
                load_existing_runs(root, expected_case_count=1),
                [],
            )

    def test_summary_writes_machine_readable_run_ledger(self):
        records = [
            {
                "run_id": "run_a",
                "csv_sha256": "abc123",
                "duration_seconds": 1.25,
                "summary": {"recurring_stalemates": []},
            }
        ]
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)

            write_final_summary(records, {}, root)

            ledger = root / "observation_runs.jsonl"
            self.assertTrue(ledger.is_file())
            self.assertEqual(
                json.loads(ledger.read_text(encoding="utf-8").strip())["run_id"],
                "run_a",
            )


if __name__ == "__main__":
    unittest.main()
