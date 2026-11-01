"""Verify resource ownership across CLI packaging and standalone native builds."""

import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock

SPEC = importlib.util.spec_from_file_location('resource_package', Path(__file__).parents[1] / 'package_project.py')
PACKAGE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PACKAGE)


class RuntimeResourceStagingTest(unittest.TestCase):
    def test_native_build_has_one_resource_owner(self):
        template = (PACKAGE.REPO_ROOT / 'engine/templates/game/native/CMakeLists.txt').read_text()
        defaults = template[template.index('set(_sky_deploy_resources_default ON)'):
                            template.index('set(_sky_project_native_directory')]
        deployment = template[template.index('# Mobile assets are prepared'):
                              template.index('function(_sky_collect_imported_runtimes')]
        deployment = (defaults + deployment).replace('{{PROJECT_NAME}}', 'Smoke')
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            script = root / 'python/package_project.py'
            script.parent.mkdir()
            script.write_text('from pathlib import Path\nimport sys\n'
                              'Path(sys.argv[sys.argv.index("--destination") + 1], "calls.txt").open("a").write("stage\\n")\n')
            for platform, managed, expected in [('Android', None, 0), ('OHOS', None, 0),
                                                 ('Darwin', None, 1), ('iOS', 'OFF', 0),
                                                 ('Darwin', 'OFF', 0), ('Windows', None, 1)]:
                with self.subTest(platform=platform, managed=managed):
                    source = root / (platform + str(managed))
                    output = source / 'bin'
                    output.mkdir(parents=True)
                    (output / 'keep.bin').write_bytes(b'unchanged')
                    (source / 'CMakeLists.txt').write_text(
                        'cmake_minimum_required(VERSION 3.20)\nproject(Smoke NONE)\n'
                        'set(CMAKE_SYSTEM_NAME "' + platform + '")\n'
                        + ('set(ANDROID TRUE)\n' if platform == 'Android' else '')
                        + 'set(ENGINE_ROOT "' + root.as_posix() + '")\n'
                        'add_executable(Launcher IMPORTED GLOBAL)\n'
                        'set_target_properties(Launcher PROPERTIES IMPORTED_LOCATION "' + (output / 'Launcher').as_posix() + '")\n'
                        + deployment, encoding='utf-8')
                    command = ['cmake', '-S', str(source), '-B', str(source / 'build'),
                               '-DPython3_EXECUTABLE=' + sys.executable]
                    if managed:
                        command += ['-DSKY_DEPLOY_RUNTIME_RESOURCES=' + managed]
                    for command in (command, ['cmake', '--build', str(source / 'build'), '--target', 'SmokeDeploy', '--config', 'Debug']):
                        result = subprocess.run(command, capture_output=True)
                        self.assertEqual(result.returncode, 0, (result.stdout + result.stderr).decode('utf-8', errors='replace'))
                    calls = output / 'calls.txt'
                    self.assertEqual(calls.read_text().count('stage') if calls.exists() else 0, expected)
                    self.assertEqual((output / 'keep.bin').read_bytes(), b'unchanged')

    def test_mobile_generation_stages_identical_runtime_content_before_native_build(self):
        with tempfile.TemporaryDirectory() as temp, mock.patch('builtins.print'):
            PACKAGE.cmd_create(SimpleNamespace(engine=None, dir=temp, name='Smoke', force=False))
            project = Path(temp) / 'Smoke'
            (project / 'Smoke.skyproj').write_text('{"name":"Smoke"}', encoding='utf-8')
            (project / 'export_presets.json').write_text(json.dumps([
                {'name': platform, 'platform': platform, 'bundleId': 'com.example.smoke', 'cacheVariables': {'SKY_DEVELOP': True}}
                for platform in ('android', 'harmonyos')]), encoding='utf-8')
            marker = project / 'products/common/probe.bin'
            marker.parent.mkdir(parents=True, exist_ok=True)
            marker.write_bytes(b'cooked')
            for platform, relative, builder in [('android', 'app/src/main/assets', 'package_android'),
                                                 ('harmonyos', 'entry/src/main/resources/rawfile', 'package_harmonyos')]:
                resources = project / 'build' / platform / relative

                def build(*args, **kwargs):
                    self.assertEqual((resources / 'products/common/probe.bin').read_bytes(), b'cooked')
                    self.assertTrue((resources / 'configs/modules_game.json').is_file())
                    self.assertTrue((resources / 'assets/shaders/shader-sources.json').is_file())
                    self.assertTrue(list((resources / 'assets/shaders').rglob('*.slang')))

                with self.subTest(platform=platform), mock.patch.object(PACKAGE, 'run_cook', return_value=True), \
                        mock.patch.object(PACKAGE, 'prepare_shader_products', return_value=True), \
                        mock.patch.object(PACKAGE, builder, side_effect=build) as native, \
                        mock.patch.object(PACKAGE, 'stage_runtime_resources', wraps=PACKAGE.stage_runtime_resources) as stage:
                    result = PACKAGE.cmd_package(SimpleNamespace(engine=None, project=str(project), preset=platform))
                    self.assertEqual(result, 0)
                    self.assertEqual(stage.call_count, 1)
                    native.assert_called_once()

    def test_apple_package_disables_native_staging_and_generate_restores_it(self):
        with tempfile.TemporaryDirectory() as temp:
            project = Path(temp)
            for platform in ('ios', 'macos'):
                for build in (False, True):
                    with self.subTest(platform=platform, build=build), \
                            mock.patch.object(PACKAGE, 'prepare_ios_orientation', return_value='-DSKY_IOS_INFO_PLIST=unused'), \
                            mock.patch.object(PACKAGE, 'find_apple_app', return_value=None), \
                            mock.patch.object(PACKAGE, 'run_cook', return_value=True), \
                            mock.patch.object(PACKAGE, 'prepare_shader_products', return_value=True), \
                            mock.patch.object(PACKAGE, 'run_cmake_preset') as native, \
                            mock.patch.object(PACKAGE, 'collect_apple_package', return_value=project / 'app.zip') as collect:
                        PACKAGE._prepare(PACKAGE.REPO_ROOT, project, {'platform': platform, 'signing': 'off'},
                                         {'{{PROJECT_NAME}}': 'Smoke', '{{APPLICATION_ID}}': 'com.example.smoke'}, {}, build)
                        self.assertEqual(native.call_args.kwargs['extra'][-1],
                                         '-DSKY_DEPLOY_RUNTIME_RESOURCES=' + ('OFF' if build else 'ON'))
                        self.assertEqual(collect.call_count, int(build))

    def test_failed_cook_retires_deliverables_on_every_packaged_platform(self):
        with tempfile.TemporaryDirectory() as temp, mock.patch('builtins.print'):
            PACKAGE.cmd_create(SimpleNamespace(engine=None, dir=temp, name='Smoke', force=False))
            project = Path(temp) / 'Smoke'
            layouts = {
                'android': ['Smoke-debug.apk', 'app/build/outputs/apk/debug/app-debug.apk'],
                'harmonyos': ['Smoke-debug-signed.hap', 'entry/build/default/outputs/default/entry-default-signed.hap'],
                'ios': ['Smoke-unsigned.ipa'],
                'macos': ['Smoke-macos.zip'],
            }
            for platform, names in layouts.items():
                with self.subTest(platform=platform):
                    output = project / 'build' / platform
                    old = [output / name for name in names]
                    for path in old:
                        path.parent.mkdir(parents=True, exist_ok=True)
                        path.write_bytes(b'old package')
                    marker = output / 'keep-cache.bin'
                    marker.write_bytes(b'keep')
                    with mock.patch.object(PACKAGE, 'run_cook', return_value=False), \
                            mock.patch.object(PACKAGE, 'stage_runtime_resources') as stage, \
                            mock.patch.object(PACKAGE, 'run_cmake_preset') as native, \
                            mock.patch.object(PACKAGE, 'package_android') as android, \
                            mock.patch.object(PACKAGE, 'package_harmonyos') as harmony:
                        result = PACKAGE.cmd_package(SimpleNamespace(engine=None, project=str(project), preset='Smoke-' + platform))
                        self.assertEqual(result, 1)
                        for action in (stage, native, android, harmony):
                            action.assert_not_called()
                    self.assertTrue(all(not path.exists() for path in old))
                    self.assertEqual(marker.read_bytes(), b'keep')


if __name__ == '__main__':
    unittest.main()
