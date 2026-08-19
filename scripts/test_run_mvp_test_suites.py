import re
import unittest
from pathlib import Path


SCRIPT = Path(__file__).with_name("run_mvp_test_suites.ps1")


class RunMvpTestSuitesContractTests(unittest.TestCase):
    def test_failure_counter_accepts_unreal_fail_and_legacy_failed_tokens(self) -> None:
        text = SCRIPT.read_text(encoding="utf-8-sig")
        match = re.search(r'\$Failed\s*=\s*\(\[regex\]::Matches\(\$Text,\s*"([^"]+)"\)\)\.Count', text)
        self.assertIsNotNone(match, "run_mvp_test_suites.ps1 must expose a deterministic failure regex")
        assert match is not None

        failure_pattern = re.compile(match.group(1))
        sample = "Result={Fail}\nResult={Failed}\nResult={Success}\n"
        self.assertEqual(failure_pattern.findall(sample), ["Result={Fail}", "Result={Failed}"])

    def test_commandlet_children_skip_redundant_ubt_sdk_probe(self) -> None:
        text = SCRIPT.read_text(encoding="utf-8-sig")
        environment_assignment = re.search(
            r"\$env:UE_SKIP_UBT_SDK_SETUP\s*=\s*['\"]1['\"]",
            text,
        )
        self.assertIsNotNone(
            environment_assignment,
            "automation commandlets must not spawn the modal-prone redundant UBT SDK probe",
        )


if __name__ == "__main__":
    unittest.main()
