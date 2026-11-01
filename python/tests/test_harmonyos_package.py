"""Exercise generated HarmonyOS projects and reject incomplete deliverables."""

import importlib.util
import json
import os
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock
import zipfile

SPEC = importlib.util.spec_from_file_location('harmonyos_package', Path(__file__).parents[1] / 'package_project.py')
PACKAGE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PACKAGE)


class HarmonyOSPackageTest(unittest.TestCase):
    def create(self, root):
        PACKAGE.cmd_create(SimpleNamespace(engine=None, dir=root, name='HapProbe', force=False))
        return Path(root) / 'HapProbe'

    def test_public_generate_keeps_owned_sources_and_builds_native_entry(self):
        with tempfile.TemporaryDirectory() as temp, mock.patch('builtins.print'):
            project = self.create(temp)
            page = project / 'native/harmonyos/entry/src/main/ets/pages/Index.ets'
            page.write_text(page.read_text() + '\n// Project-owned change.\n')
            launcher = project / 'native/harmonyos/cpp/ohos_main.cpp'
            launcher.write_text(launcher.read_text() + '\n// Project-owned application factory.\n')
            owned_launcher = launcher.read_bytes()
            with mock.patch.object(PACKAGE, 'run_cook', return_value=True), \
                    mock.patch.object(PACKAGE, 'prepare_shader_products', return_value=True), \
                    mock.patch.object(PACKAGE, 'stage_bundles', return_value=['common']), \
                    mock.patch.dict(os.environ, {'SKY_3RD_PATH': str(Path(temp) / 'ohos libs')}):
                self.assertEqual(PACKAGE.cmd_generate(SimpleNamespace(
                    engine=None, project=str(project), preset='HapProbe-harmonyos')), 0)
            output = project / 'build/harmonyos'
            self.assertEqual(page.read_bytes(), (output / 'entry/src/main/ets/pages/Index.ets').read_bytes())
            self.assertEqual(launcher.read_bytes(), owned_launcher)
            profile = json.loads((output / 'entry/build-profile.json5').read_text())
            native = profile['buildOption']['externalNativeOptions']
            self.assertTrue((output / 'entry' / native['path']).is_file())
            bridge = (output / 'entry' / native['path']).parent
            self.assertEqual((bridge / '../../../../../../native').resolve(), (project / 'native').resolve())
            args = native['arguments']
            self.assertIn('-D3RD_PATH=' + (Path(temp) / 'ohos libs').as_posix(), args)
            product = json.loads((output / 'build-profile.json5').read_text())['app']['products'][0]
            self.assertEqual(product['compatibleSdkVersion'], '26.0.0')
            self.assertNotIn('signingConfig', product)
            for file in output.rglob('*'):
                if file.is_file():
                    self.assertNotIn(b'{{', file.read_bytes(), str(file))

    def test_created_launcher_bridges_framework_host_without_owning_its_frame_loop(self):
        with tempfile.TemporaryDirectory() as temp, mock.patch('builtins.print'):
            project = self.create(temp)
            source = (project / 'native/harmonyos/cpp/ohos_main.cpp').read_text(encoding='utf-8')
            cmake = (project / 'native/harmonyos/cpp/CMakeLists.txt').read_text(encoding='utf-8')
            self.assertIn('<framework/platform/OHOSHost.h>', source)
            self.assertIn('sky::OHOSHost::Get()', source)
            self.assertIn('sky::GameApplication', source)
            for mechanism in ('std::thread', 'std::deque', 'std::condition_variable', 'OH_NativeVSync_'):
                with self.subTest(mechanism=mechanism):
                    self.assertNotIn(mechanism, source)
            self.assertNotIn('native_vsync/', source)
            self.assertNotIn('native_vsync', cmake)
            self.assertNotIn('OHOSHost.cpp', cmake)
            self.assertRegex(cmake, r'LINK_LIBS\s+Framework\b')

    def test_failed_cook_retires_old_hap_and_does_not_build(self):
        with tempfile.TemporaryDirectory() as temp, mock.patch('builtins.print'):
            project = self.create(temp)
            output = project / 'build/harmonyos'
            old = output / 'entry/build/default/outputs/default/entry-default-unsigned.hap'
            old.parent.mkdir(parents=True)
            old.write_bytes(b'old artifact')
            copy = output / 'HapProbe-debug-unsigned.hap'
            copy.write_bytes(b'old artifact')
            with mock.patch.object(PACKAGE, 'run_cook', return_value=False), \
                    mock.patch.object(PACKAGE, 'package_harmonyos') as build:
                result = PACKAGE.cmd_package(SimpleNamespace(engine=None, project=str(project), preset='HapProbe-harmonyos'))
            self.assertEqual(result, 1)
            build.assert_not_called()
            self.assertFalse(old.exists())
            self.assertFalse(copy.exists())

    def test_signing_without_credentials_fails_before_overwriting_output(self):
        with tempfile.TemporaryDirectory() as temp, mock.patch('builtins.print'):
            project = self.create(temp)
            _, preset, tokens, _ = PACKAGE._resolve(project, 'HapProbe-harmonyos')
            preset['signing'] = 'auto'
            with self.assertRaisesRegex(SystemExit, 'signing requires'):
                PACKAGE.prepare_harmonyos_project(PACKAGE.REPO_ROOT, project, preset, tokens, {})
            self.assertFalse((project / 'build/harmonyos').exists())

    def test_arkts_only_hap_and_wrong_elf_are_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            artifact = Path(temp) / 'app.hap'
            with zipfile.ZipFile(artifact, 'w') as archive:
                archive.writestr('ets/modules.abc', b'arkts')
            with self.assertRaisesRegex(SystemExit, 'missing native library'):
                PACKAGE.validate_harmonyos_hap(artifact, 'HapProbe', ['arm64-v8a'])
            with zipfile.ZipFile(artifact, 'a') as archive:
                archive.writestr('libs/arm64-v8a/libHapProbe.so', b'wrong architecture')
            with self.assertRaisesRegex(SystemExit, 'ELF64 AArch64'):
                PACKAGE.validate_harmonyos_hap(artifact, 'HapProbe', ['arm64-v8a'])


if __name__ == '__main__':
    unittest.main()
