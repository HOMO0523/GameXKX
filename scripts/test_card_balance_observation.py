import csv
import io
import json
import tempfile
import unittest
from pathlib import Path


from scripts.run_card_balance_observation import (
    aggregate_case_rows,
    audit_card_catalog,
    get_observation_matrix_config,
    load_existing_runs,
    parse_metric_map,
    read_case_rows,
    validate_automation_report,
    write_final_summary,
)


CASE_FIELDS = (
    "schema_version",
    "cohort",
    "quest_npc",
    "equipment_set",
    "equipment_quality",
    "enhancement",
    "chapter",
    "node",
    "seed_ordinal",
    "seed",
    "outcome",
    "rounds",
    "remaining_party_health",
    "first_round_deaths",
    "companion_template",
    "companion_role",
    "companion_primary_archetype",
    "companion_birth_cards",
    "companion_selected_cards",
    "terrain",
    "active_cards",
    "automatic_resolutions",
    "energy_spent",
    "energy_gained",
    "mana_spent",
    "mana_gained",
    "energy_unspent_at_phase_end",
    "mana_unspent_at_phase_end",
    "healing_generated",
    "armor_generated",
    "overkill_damage",
    "overhealing",
    "stranded_target_failures",
    "maximum_queue_depth",
    "maximum_hand_size",
    "damage_by_source",
    "damage_by_origin",
    "healing_by_source",
    "armor_by_source",
    "status_produced",
    "status_consumed",
    "cards_seen_by_id",
    "cards_played_by_id",
    "damage_by_card_id",
    "healing_by_card_id",
    "armor_by_card_id",
    "error",
)


def make_case_csv(rows, fields=CASE_FIELDS):
    output = io.StringIO()
    writer = csv.DictWriter(output, fieldnames=fields, lineterminator="\n")
    writer.writeheader()
    for row in rows:
        complete = {field: "" for field in fields}
        complete.update(row)
        writer.writerow(complete)
    return output.getvalue()


CASE_ROWS = [
    {
        "schema_version": 3,
        "cohort": "NakedBaseline",
        "quest_npc": "Npc.TusiChief",
        "equipment_set": "None",
        "equipment_quality": "Common",
        "enhancement": 0,
        "chapter": 1,
        "node": "Battle",
        "seed_ordinal": 0,
        "seed": 900000,
        "outcome": "Victory",
        "rounds": 4,
        "remaining_party_health": 120,
        "first_round_deaths": 0,
        "companion_template": "Companion.Blade",
        "companion_role": "Blade",
        "companion_primary_archetype": "Archetype.Blade.BloodBlade",
        "companion_birth_cards": "Card.A;Card.B",
        "companion_selected_cards": "Card.A;Card.B",
        "terrain": "Plain",
        "active_cards": 8,
        "automatic_resolutions": 2,
        "energy_spent": 7,
        "energy_gained": 3,
        "mana_spent": 9,
        "mana_gained": 4,
        "energy_unspent_at_phase_end": 1,
        "mana_unspent_at_phase_end": 5,
        "healing_generated": 12,
        "armor_generated": 6,
        "overkill_damage": 9,
        "overhealing": 3,
        "stranded_target_failures": 0,
        "maximum_queue_depth": 4,
        "maximum_hand_size": 9,
        "damage_by_source": "Player=100",
        "damage_by_origin": "Direct=80;Reaction=20",
        "healing_by_source": "Healer=12",
        "armor_by_source": "Guard=6",
        "status_produced": "Burn=2",
        "status_consumed": "Burn=1",
        "cards_seen_by_id": "Card.A=2;Card.B=1",
        "cards_played_by_id": "Card.A=1",
        "damage_by_card_id": "Card.A=80",
        "healing_by_card_id": "Card.B=12",
        "armor_by_card_id": "Card.B=6",
    },
    {
        "schema_version": 3,
        "cohort": "NakedBaseline",
        "quest_npc": "Npc.TusiChief",
        "equipment_set": "None",
        "equipment_quality": "Common",
        "enhancement": 0,
        "chapter": 1,
        "node": "Battle",
        "seed_ordinal": 1,
        "seed": 900001,
        "outcome": "Defeat",
        "rounds": 6,
        "remaining_party_health": 0,
        "first_round_deaths": 0,
        "companion_template": "Companion.Guard",
        "companion_role": "Guard",
        "companion_primary_archetype": "Archetype.Guard.ArmorGrowth",
        "companion_birth_cards": "Card.A;Card.C",
        "companion_selected_cards": "Card.A;Card.C",
        "terrain": "Plain",
        "active_cards": 10,
        "automatic_resolutions": 1,
        "energy_spent": 9,
        "energy_gained": 2,
        "mana_spent": 12,
        "mana_gained": 1,
        "energy_unspent_at_phase_end": 2,
        "mana_unspent_at_phase_end": 4,
        "healing_generated": 4,
        "armor_generated": 3,
        "overkill_damage": 1,
        "overhealing": 0,
        "stranded_target_failures": 1,
        "maximum_queue_depth": 5,
        "maximum_hand_size": 10,
        "damage_by_source": "Player=50",
        "damage_by_origin": "Direct=50",
        "cards_seen_by_id": "Card.A=1;Card.C=3",
        "cards_played_by_id": "Card.A=1;Card.C=2",
        "damage_by_card_id": "Card.A=50;Card.C=50",
    },
    {
        "schema_version": 3,
        "cohort": "NakedBaseline",
        "quest_npc": "Npc.TusiChief",
        "equipment_set": "None",
        "equipment_quality": "Common",
        "enhancement": 0,
        "chapter": 1,
        "node": "Elite",
        "seed_ordinal": 2,
        "seed": 901002,
        "outcome": "Stalemate",
        "rounds": 100,
        "remaining_party_health": 80,
        "first_round_deaths": 0,
        "companion_template": "Companion.Healer",
        "companion_role": "Healer",
        "companion_primary_archetype": "Archetype.Healer.Medicine",
        "companion_birth_cards": "Card.C;Card.D",
        "companion_selected_cards": "Card.C;Card.D",
        "terrain": "Forest",
        "active_cards": 6,
        "automatic_resolutions": 0,
        "energy_spent": 6,
        "energy_gained": 0,
        "mana_spent": 8,
        "mana_gained": 0,
        "energy_unspent_at_phase_end": 0,
        "mana_unspent_at_phase_end": 0,
        "healing_generated": 2,
        "armor_generated": 2,
        "overkill_damage": 0,
        "overhealing": 0,
        "stranded_target_failures": 0,
        "maximum_queue_depth": 3,
        "maximum_hand_size": 8,
        "cards_seen_by_id": "Card.C=1",
        "error": "Simulation.MaxRounds",
    },
]

CASE_CSV = make_case_csv(CASE_ROWS)

LEGACY_CASE_CSV = """cohort,quest_npc,equipment_set,equipment_quality,enhancement,chapter,node,seed_ordinal,seed,outcome,rounds,remaining_party_health,first_round_deaths,damage_by_source,healing_by_source,armor_by_source,status_produced,status_consumed,error
NakedBaseline,Npc.TusiChief,None,Common,0,1,Battle,0,900000,Victory,4,120,0,Player=100,,,Burn=2,Burn=1,
"""

PROJECT_ROOT = Path(__file__).resolve().parents[1]


class CardBalanceObservationParserTests(unittest.TestCase):
    def test_matrix_configuration_keeps_locked_and_orthogonal_contracts_separate(self):
        locked = get_observation_matrix_config("locked")
        orthogonal = get_observation_matrix_config("orthogonal")

        self.assertEqual(locked.expected_case_count, 2400)
        self.assertEqual(locked.automation_test, "GameXXK.Diagnostics.CardBalanceObservation")
        self.assertEqual(orthogonal.expected_case_count, 2520)
        self.assertEqual(
            orthogonal.automation_test,
            "GameXXK.Diagnostics.OrthogonalBalanceObservation",
        )
        with self.assertRaisesRegex(ValueError, "unknown observation matrix"):
            get_observation_matrix_config("mixed")

    def test_metric_map_rejects_malformed_items(self):
        with self.assertRaisesRegex(ValueError, "invalid metric item"):
            parse_metric_map("Burn=2;missing-value")

    def test_metric_map_rejects_duplicate_keys(self):
        with self.assertRaisesRegex(ValueError, "invalid metric item"):
            parse_metric_map("Card.A=1;Card.A=2")

    def test_case_reader_rejects_unknown_schema(self):
        rows = [dict(CASE_ROWS[0], schema_version=99)]

        with self.assertRaisesRegex(ValueError, "case CSV row 2 is malformed"):
            read_case_rows(io.StringIO(make_case_csv(rows)))

    def test_case_reader_rejects_invalid_numeric_value(self):
        rows = [dict(CASE_ROWS[0], overkill_damage="not-an-integer")]

        with self.assertRaisesRegex(ValueError, "case CSV row 2 is malformed"):
            read_case_rows(io.StringIO(make_case_csv(rows)))

    def test_case_reader_rejects_missing_identity_and_per_card_columns(self):
        required_fields = {
            "companion_template",
            "companion_role",
            "companion_primary_archetype",
            "companion_birth_cards",
            "companion_selected_cards",
            "terrain",
            "cards_seen_by_id",
            "cards_played_by_id",
            "damage_by_card_id",
            "healing_by_card_id",
            "armor_by_card_id",
        }
        for missing_field in sorted(required_fields):
            fields = [field for field in CASE_FIELDS if field != missing_field]
            output = io.StringIO()
            writer = csv.DictWriter(output, fieldnames=fields, lineterminator="\n")
            writer.writeheader()
            writer.writerow({field: CASE_ROWS[0].get(field, "") for field in fields})
            with self.subTest(missing_field=missing_field):
                with self.assertRaisesRegex(ValueError, "case CSV row 2 is malformed"):
                    read_case_rows(io.StringIO(output.getvalue()))

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
                "energy_unspent_at_phase_end": 3,
                "mana_unspent_at_phase_end": 9,
                "healing_generated": 18,
                "armor_generated": 11,
                "overkill_damage": 10,
                "overhealing": 3,
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
        self.assertEqual(
            summary["card_usage"],
            {
                "Card.A": {
                    "seen": 3,
                    "played": 2,
                    "play_rate": 0.666667,
                    "damage": 130,
                    "healing": 0,
                    "armor": 0,
                },
                "Card.B": {
                    "seen": 1,
                    "played": 0,
                    "play_rate": 0.0,
                    "damage": 0,
                    "healing": 12,
                    "armor": 6,
                },
                "Card.C": {
                    "seen": 4,
                    "played": 2,
                    "play_rate": 0.5,
                    "damage": 50,
                    "healing": 0,
                    "armor": 0,
                },
            },
        )
        self.assertEqual(summary["companion_roles"]["Blade"]["case_count"], 1)
        self.assertEqual(summary["terrains"]["Plain"]["case_count"], 2)

    def test_aggregate_rejects_duplicate_case_identity(self):
        rows = read_case_rows(io.StringIO(CASE_CSV))
        rows.append(dict(rows[0]))

        with self.assertRaisesRegex(ValueError, "case identities are not unique"):
            aggregate_case_rows(rows, expected_case_count=4)

    def test_orthogonal_variants_share_control_seed_without_duplicate_identity(self):
        fields = ("schema_version", "dimension", "variant") + CASE_FIELDS[1:]
        rows = [
            dict(CASE_ROWS[0], dimension="Profession", variant="Blade"),
            dict(CASE_ROWS[0], dimension="Profession", variant="Guard"),
        ]

        parsed = read_case_rows(io.StringIO(make_case_csv(rows, fields)))
        summary = aggregate_case_rows(parsed, expected_case_count=2)

        self.assertEqual(summary["dimensions"]["Profession"]["Blade"]["case_count"], 1)
        self.assertEqual(summary["dimensions"]["Profession"]["Guard"]["case_count"], 1)

    def test_orthogonal_identity_requires_dimension_and_variant_together(self):
        fields = ("schema_version", "dimension", "variant") + CASE_FIELDS[1:]
        row = dict(CASE_ROWS[0], dimension="Profession", variant="")

        with self.assertRaisesRegex(ValueError, "case CSV row 2 is malformed"):
            read_case_rows(io.StringIO(make_case_csv([row], fields)))

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
            {"0": 45, "1": 95, "2": 48, "3": 10},
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
        self.assertEqual(
            formation_setup_cards,
            [
                "Profession.FormationMaster.BaMenLunZhuan",
                "Profession.FormationMaster.DingZhen",
                "Profession.FormationMaster.GuanShi",
                "Profession.FormationMaster.JieShanWeiZhang",
                "Profession.FormationMaster.KunZhen",
                "Profession.FormationMaster.LinFengFuZhen",
                "Profession.FormationMaster.LinYingMiZong",
                "Profession.FormationMaster.ShanMenFengSuo",
                "Profession.FormationMaster.YiWeiZhen",
                "Profession.FormationMaster.YinShuiHuiYuan",
                "Profession.FormationMaster.ZhenQiGuWu",
            ],
        )
        self.assertIn(
            "Npc.JinGui.ShiJingErMu",
            audit["zero_cost_draw_cards"],
        )
        self.assertEqual(
            audit["exact_duplicate_effect_groups"],
            [
                [
                    "Profession.Blade.LianXiGuiQiao",
                    "Profession.Sorcerer.JuLing",
                ],
                [
                    "Profession.Sorcerer.FenMaiFu",
                    "Profession.Sorcerer.LingYanLianDan",
                ],
            ],
        )
        self.assertEqual(audit["strict_energy_dominance_groups"], [])


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

    def test_orthogonal_summary_uses_separate_artifact_names(self):
        records = [
            {
                "run_id": "orthogonal_a",
                "csv_sha256": "orthogonal-hash",
                "duration_seconds": 2.0,
                "summary": {"recurring_stalemates": []},
            }
        ]
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)

            write_final_summary(records, {}, root, artifact_prefix="orthogonal_")

            self.assertTrue((root / "orthogonal_observation_runs.jsonl").is_file())
            self.assertTrue((root / "orthogonal_final_summary.json").is_file())
            self.assertTrue((root / "orthogonal_final_summary.md").is_file())
            self.assertFalse((root / "final_summary.json").exists())


if __name__ == "__main__":
    unittest.main()
