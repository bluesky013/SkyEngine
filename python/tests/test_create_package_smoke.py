"""Smoke fixture generation stays on the public project/startup path without SDKs."""

import contextlib
import importlib.util
import io
import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest


SCRIPT = Path(__file__).with_name('create_package_smoke.py')
SPEC = importlib.util.spec_from_file_location('create_package_smoke', SCRIPT)
SMOKE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(SMOKE)
ROOT = SCRIPT.resolve().parents[2]


class PackageSmokeFixtureTest(unittest.TestCase):
    def create(self, parent, scene=None):
        arguments = ['--dir', str(parent), '--name', 'SceneSmoke']
        if scene:
            arguments += ['--scene', scene]
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            self.assertEqual(SMOKE.main(arguments), 0)
        return parent / 'SceneSmoke', output.getvalue()

    def test_scene_uses_the_checked_in_world_and_standard_game_module(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            project, _ = self.create(root / 'fixture', 'cube-plane')
            with contextlib.redirect_stdout(io.StringIO()):
                SMOKE.PACKAGE.cmd_create(SimpleNamespace(
                    engine=str(ROOT), dir=root / 'reference', name='SceneSmoke', force=False))
            reference = root / 'reference/SceneSmoke/native/source'
            generated = project / 'native/source'
            for path in reference.rglob('*'):
                if path.is_file():
                    self.assertEqual((generated / path.relative_to(reference)).read_bytes(), path.read_bytes())
            document = project / 'assets/scenes/cube_plane.world'
            self.assertEqual(document.read_bytes(), (SCRIPT.parent / 'fixtures/cube_plane.world').read_bytes())
            descriptor = json.loads((project / 'SceneSmoke.skyproj').read_text())
            self.assertEqual(descriptor['defaultScene'], 'scenes/cube_plane.world')
            presets = json.loads((project / 'export_presets.json').read_text())
            self.assertTrue(all(preset['worlds'] == ['scenes/cube_plane.world'] for preset in presets))
            modules = json.loads((project / 'configs/modules_game.json').read_text())['modules']
            self.assertEqual(modules, [
                {'name': 'Aurora.Render', 'dependencies': ['Aurora.Metadata']},
                {'name': 'Content.Module', 'dependencies': ['Content.Metadata']},
                {'name': 'SceneSmokeModule', 'dependencies': ['Aurora.Render', 'Content.Module']},
            ])

    def test_empty_smoke_stays_empty_and_builds_the_launcher_with_lean_flags(self):
        with tempfile.TemporaryDirectory() as temporary:
            project, _ = self.create(Path(temporary))
            descriptor = json.loads((project / 'SceneSmoke.skyproj').read_text())
            self.assertEqual(descriptor['defaultScene'], '')
            self.assertFalse(list((project / 'assets').rglob('*.world')))
            presets = json.loads((project / 'export_presets.json').read_text())
            for preset in presets:
                with self.subTest(platform=preset['platform']):
                    self.assertEqual(preset['extraBuildArgs'], ['--target', 'Launcher', '--parallel', '2'])
                    self.assertEqual(preset['cacheVariables']['SKY_BUILD_ANIMATION'], 'OFF')
                    self.assertEqual(preset['cacheVariables']['SKY_BUILD_CONSOLE'], 'OFF')
                    self.assertNotIn('worlds', preset)

    def test_simulator_reuses_ios_distribution_with_separate_build_and_dependencies(self):
        with tempfile.TemporaryDirectory(prefix='sky smoke ') as temporary:
            project, commands = self.create(Path(temporary), 'cube-plane')
            user = json.loads((project / 'native/CMakeUserPresets.json').read_text())
            configure = user['configurePresets'][0]
            self.assertEqual(configure['inherits'], 'ios-simulator')
            cache = configure['cacheVariables']
            self.assertEqual(cache['SKY_SHADER_DISTRIBUTION'], 'SceneSmoke-ios')
            self.assertEqual(cache['SKY_MODULE_PACKAGING'], 'static')
            descriptor = json.loads((project / 'SceneSmoke.skyproj').read_text())
            self.assertEqual(cache['SKY_BUNDLE_ID'], descriptor['settings']['applicationId'])
            self.assertNotIn('3RD_PATH', cache)
            self.assertNotIn('CMAKE_OSX_ARCHITECTURES', cache)
            self.assertIn('SKY_3RD_CMAKE_ARGS=', commands)
            self.assertIn('third_party.py', commands)
            self.assertIn('-p IOS', commands)
            self.assertIn(str(project / 'build/simulator-thirdparty'), commands)
            self.assertIn(str(project / 'build/simulator-thirdparty-intermediate'), commands)
            self.assertIn('/"$SKY_IOS_SIMULATOR_ARCH"', commands)
            self.assertIn('-o "$sky_smoke_simulator_deps" -i "$sky_smoke_simulator_intermediate"', commands)
            self.assertIn('export SKY_3RD_PATH="$sky_smoke_simulator_deps/IOS"', commands)
            self.assertIn('SIMULATORARM64', commands)
            self.assertIn('SIMULATOR64', commands)
            self.assertIn('cmake --preset ios-simulator-smoke -D3RD_PATH="$SKY_3RD_PATH"', commands)
            self.assertIn('cmake --build --preset ios-simulator-smoke --target Launcher', commands)

    def test_public_simulator_preset_uses_environment_architecture_and_dependency_path(self):
        presets = json.loads((ROOT / 'engine/templates/game/native/CMakePresets.json').read_text())
        configure = {preset['name']: preset for preset in presets['configurePresets']}
        simulator = configure['ios-simulator']
        self.assertEqual(simulator['inherits'], 'ios')
        self.assertEqual(simulator['cacheVariables'], {
            'SKY_PLATFORM': 'ios-simulator',
            'CMAKE_OSX_SYSROOT': 'iphonesimulator',
            'CMAKE_OSX_ARCHITECTURES': '$env{SKY_IOS_SIMULATOR_ARCH}',
            '3RD_PATH': '$env{SKY_3RD_PATH}',
        })
        self.assertEqual(configure['ios']['cacheVariables']['SKY_PLATFORM'], 'ios')
        self.assertEqual(configure['base']['binaryDir'], '${sourceDir}/../build/${presetName}')
        build = next(preset for preset in presets['buildPresets'] if preset['name'] == 'ios-simulator')
        self.assertEqual(build['configurePreset'], 'ios-simulator')
        self.assertEqual(build['configuration'], 'Release')


if __name__ == '__main__':
    unittest.main()
