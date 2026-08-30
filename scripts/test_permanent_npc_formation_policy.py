from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
PRIVATE = ROOT / "Source/GameXXK/Private"
PUBLIC = ROOT / "Source/GameXXK/Public"


def production_sources():
    for base in (PRIVATE, PUBLIC):
        for pattern in ("*.cpp", "*.h"):
            for path in base.rglob(pattern):
                if "Tests" not in path.relative_to(ROOT).parts:
                    yield path


class PermanentNpcFormationPolicyTests(unittest.TestCase):
    def test_temporary_field_is_tombstone_only_in_production(self) -> None:
        allowed = {
            ROOT / "Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp",
            ROOT / "Source/GameXXK/Private/GameXXKPartyFormationRules.cpp",
            ROOT / "Source/GameXXK/Public/GameXXKCardRunTypes.h",
        }
        offenders = [
            path.relative_to(ROOT).as_posix()
            for path in production_sources()
            if path not in allowed
            and "ActiveTemporaryQuestNpcId"
            in path.read_text(encoding="utf-8", errors="ignore")
        ]
        self.assertEqual(offenders, [])

    def test_removed_support_copy_and_empty_npc_state_are_unreachable(self) -> None:
        production = "\n".join(
            path.read_text(encoding="utf-8", errors="ignore")
            for path in production_sources()
        )
        for forbidden in (
            "邀请月白同行",
            "邀请{0}支援",
            "已有任务支援",
            "临时 NPC",
            "NPC · 未编入",
            "本次路线临时加入",
            "临时路线支援",
            "不可招募",
        ):
            self.assertNotIn(forbidden, production)


if __name__ == "__main__":
    unittest.main()
