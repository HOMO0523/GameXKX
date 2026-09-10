"""Compile the actual F10 build policy in all supported build configurations."""
from pathlib import Path
import os
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
COMPILER = Path(os.environ.get('GAMEXXK_CL', r'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\cl.exe'))


class DevBuildPolicyTest(unittest.TestCase):
    def check_policy(self, shipping: int, opt_in: bool, expected: int) -> None:
        with tempfile.TemporaryDirectory(dir=ROOT / 'Saved') as directory:
            source = Path(directory) / 'policy.cpp'
            source.write_text('#include "Dev/GameXXKDevBuildPolicy.h"\n'
                              f'#if GAMEXXK_WITH_DEV_TOOLS != {expected}\n'
                              '#error F10 availability does not match the requested build variant\n'
                              '#endif\n', encoding='utf-8')
            command = [str(COMPILER), '/nologo', '/EP', f'/DUE_BUILD_SHIPPING={shipping}',
                       '/I' + str(ROOT / 'Source/GameXXK/Public'), str(source)]
            if opt_in:
                command.append('/DGAMEXXK_SHIPPING_WITH_DEV_TOOLS=1')
            result = subprocess.run(command, capture_output=True, text=True, errors='replace')
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_development_keeps_complete_dev_tools(self):
        self.check_policy(0, False, 1)

    def test_normal_shipping_does_not_enable_dev_tools(self):
        self.check_policy(1, False, 0)

    def test_explicit_shipping_f10_variant_enables_dev_tools(self):
        self.check_policy(1, True, 1)


if __name__ == '__main__':
    unittest.main()
