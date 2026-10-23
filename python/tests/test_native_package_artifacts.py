"""Package boundary regressions without installed SDKs or signing credentials."""

import importlib.util
import json
import os
from pathlib import Path
import stat
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
from types import SimpleNamespace
import zipfile

SCRIPT = Path(__file__).resolve().parents[1] / 'package_project.py'
SPEC = importlib.util.spec_from_file_location('package_project', SCRIPT)
PACKAGE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PACKAGE)
from shader_project import target_environments


class NativePackageArtifactTest(unittest.TestCase):
    def test_failed_cook_stops_packaging_while_missing_tool_can_use_precooked_assets(self):
        with mock.patch.object(PACKAGE, 'find_asset_tool', return_value=Path('/engine/AssetTool')), \
                mock.patch.object(PACKAGE.subprocess, 'run', return_value=SimpleNamespace(returncode=7)):
            with self.assertRaises(SystemExit):
                PACKAGE.run_cook(Path('/engine'), Path('/project'), 'ios')
        with mock.patch.object(PACKAGE, 'find_asset_tool', return_value=None):
            self.assertFalse(PACKAGE.run_cook(Path('/engine'), Path('/project'), 'ios'))

    def test_repackage_removes_deleted_configs_and_unselected_products(self):
        with tempfile.TemporaryDirectory() as temp:
            engine, project, app, resources = self.make_app(Path(temp), 'ios')
            (project / 'configs/removed.json').write_text('{}')
            PACKAGE.collect_apple_package(engine, project, {'platform': 'ios'}, 'Smoke')
            (project / 'configs/removed.json').unlink()
            (resources / 'products/old_bundle').mkdir()
            archive = PACKAGE.collect_apple_package(engine, project, {'platform': 'ios'}, 'Smoke')
            with zipfile.ZipFile(archive) as output:
                self.assertNotIn('Payload/Smoke.app/configs/removed.json', output.namelist())
                self.assertFalse(any('/old_bundle' in name for name in output.namelist()))

    def test_shipping_retires_development_sources_and_compiler_files_from_a_reused_app(self):
        with tempfile.TemporaryDirectory() as temp:
            engine, project, app, resources = self.make_app(Path(temp), 'macos')
            (resources / 'assets/shaders').mkdir(parents=True)
            (resources / 'assets/shaders/Main.slang').write_text('source')
            for name in ('libslang-compiler.dylib', 'libAurora.ShaderCompiler.dylib', 'libKeep.dylib'):
                (resources / name).write_bytes(b'library')
            archive = PACKAGE.collect_apple_package(engine, project, {'platform': 'macos'}, 'Smoke')
            with zipfile.ZipFile(archive) as output:
                self.assertFalse(any(name.endswith('.slang') or 'slang-compiler' in name or 'Aurora.ShaderCompiler' in name
                                     for name in output.namelist()))
                self.assertIn('Smoke.app/Contents/MacOS/libKeep.dylib', output.namelist())

    def test_first_cmake_configure_preserves_explicit_plugin_switches(self):
        for override, expected in (([], 'ON'), (['-DSKY_BUILD_SMOKE=OFF'], 'OFF')):
            with self.subTest(expected=expected), tempfile.TemporaryDirectory() as temp:
                root = Path(temp)
                (root / 'plugins').mkdir()
                (root / 'plugins/plugins.json').write_text(json.dumps({'plugins': [
                    {'name': 'Smoke', 'cmake_var': 'SKY_BUILD_SMOKE', 'enabled': True}
                ]}), encoding='utf-8')
                helper = PACKAGE.REPO_ROOT / 'cmake/plugin_switches.cmake'
                (root / 'CMakeLists.txt').write_text(
                    'cmake_minimum_required(VERSION 3.20)\nproject(Smoke NONE)\n'
                    'set(ENGINE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")\n'
                    'include("' + helper.as_posix() + '")\n'
                    'file(WRITE "${CMAKE_BINARY_DIR}/switch.txt" "${SKY_BUILD_SMOKE}")\n', encoding='utf-8')
                subprocess.run(['cmake', '-S', str(root), '-B', str(root / 'build')] + override,
                               check=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                self.assertEqual((root / 'build/switch.txt').read_text(), expected)

    def test_android_forwards_engine_and_preset_without_groovy_interpolation(self):
        preset = {'packing': 'static', 'cacheVariables': {'SKY_BUILD_TEST': False},
                  'compileDefinitions': ['NAME=$value'], 'compileFlags': ['-Wall'],
                  'extraConfigureArgs': ['-DEXTRA=with space']}
        with mock.patch.dict(os.environ, {'SKY_3RD_PATH': '/deps with space'}):
            result = PACKAGE.android_cmake_args(Path("/engine's path\\root"), preset)
        self.assertIn("'-DSKY_ENGINE_ROOT=/engine\\'s path\\\\root'", result)
        self.assertIn("'-DSKY_MODULE_PACKAGING=static'", result)
        self.assertIn("'-DSKY_BUILD_TEST=OFF'", result)
        self.assertIn("'-DSKY_EXTRA_CXX_DEFINITIONS=NAME=$value'", result)
        self.assertIn("'-DSKY_EXTRA_CXX_FLAGS=-Wall'", result)
        self.assertIn("'-DEXTRA=with space'", result)
        self.assertIn("'-D3RD_PATH=/deps with space'", result)

    def make_app(self, root, platform):
        project, engine = root / 'project', root / 'engine'
        app = project / 'build' / platform / 'bin' / 'Release' / 'Smoke.app'
        resources = app / 'Contents' / 'MacOS' if platform == 'macos' else app
        resources.mkdir(parents=True)
        executable = resources / 'Smoke'
        executable.write_bytes(b'test executable')
        executable.chmod(0o755)
        plist = app / 'Contents/Info.plist' if platform == 'macos' else app / 'Info.plist'
        plist.write_text('test plist', encoding='utf-8')
        (engine / 'configs').mkdir(parents=True)
        (engine / 'configs/modules_game.json').write_text('{"modules": []}', encoding='utf-8')
        (project / 'configs').mkdir(parents=True)
        (project / 'configs/modules_game.json').write_text('{"modules": [{"name": "Smoke", "dependencies": []}]}', encoding='utf-8')
        (project / 'products/common').mkdir(parents=True)
        (project / 'products/common/asset.bin').write_bytes(b'cooked')
        return engine, project, app, resources

    def test_ios_archive_contains_payload_resources_and_executable_mode(self):
        with tempfile.TemporaryDirectory() as temp:
            engine, project, app, resources = self.make_app(Path(temp), 'ios')
            archive = PACKAGE.collect_apple_package(engine, project, {'platform': 'ios'}, 'Smoke')
            self.assertTrue(archive.name.endswith('-unsigned.ipa'))
            with zipfile.ZipFile(archive) as output:
                self.assertEqual(output.read('Payload/Smoke.app/products/common/asset.bin'), b'cooked')
                self.assertEqual(json.loads(output.read('Payload/Smoke.app/configs/modules_game.json')),
                                 {'modules': [{'name': 'Smoke', 'dependencies': []}]})
                self.assertTrue(output.getinfo('Payload/Smoke.app/Smoke').external_attr >> 16 & stat.S_IXUSR)
            self.assertFalse((project / 'build/ios/bin/products').exists())

    def test_macos_archive_preserves_framework_links_and_bundle_layout(self):
        with tempfile.TemporaryDirectory() as temp:
            engine, project, app, resources = self.make_app(Path(temp), 'macos')
            (resources / 'library.1.dylib').write_bytes(b'library')
            (resources / 'library.dylib').symlink_to('library.1.dylib')
            archive = PACKAGE.collect_apple_package(engine, project, {'platform': 'macos'}, 'Smoke')
            with zipfile.ZipFile(archive) as output:
                self.assertIn('Smoke.app/Contents/Info.plist', output.namelist())
                self.assertIn('Smoke.app/Contents/MacOS/products/bundles.json', output.namelist())
                link = output.getinfo('Smoke.app/Contents/MacOS/library.dylib')
                self.assertTrue(stat.S_ISLNK(link.external_attr >> 16))
                self.assertEqual(output.read(link), b'library.1.dylib')

    def test_rejects_missing_or_ambiguous_app_instead_of_collecting_stale_build(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            with self.assertRaises(SystemExit):
                PACKAGE.find_apple_app(root, 'ios', 'Smoke')
            for config in ('Debug', 'Release'):
                (root / 'build/ios/bin' / config / 'Smoke.app').mkdir(parents=True)
            with self.assertRaises(SystemExit):
                PACKAGE.find_apple_app(root, 'ios', 'Smoke')

    def test_signed_collection_fails_before_mutating_or_building(self):
        with mock.patch.object(PACKAGE, 'run_cmake_preset') as configure:
            with self.assertRaises(SystemExit):
                PACKAGE._prepare(Path('/engine'), Path('/project'),
                                 {'platform': 'ios', 'signing': 'auto'}, {}, {}, True)
            configure.assert_not_called()

    def test_android_generation_supports_unicode_engine_path(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            source, destination = root / 'template', root / 'generated'
            source.mkdir()
            (source / 'build.gradle').write_text('arguments {{ANDROID_CMAKE_ARGUMENTS}}', encoding='utf-8')
            tokens = PACKAGE.base_tokens({'name': 'Smoke'})
            tokens['{{ANDROID_CMAKE_ARGUMENTS}}'] = PACKAGE.android_cmake_args(Path('/引擎'), {})
            PACKAGE.instantiate_template(source, destination, tokens)
            self.assertIn('/引擎', (destination / 'build.gradle').read_text(encoding='utf-8'))

    def test_native_ide_deploy_stages_shader_products_without_loading_the_compiler(self):
        with tempfile.TemporaryDirectory() as temp:
            engine, project, app, resources = self.make_app(Path(temp), 'ios')
            (project / 'Smoke.skyproj').write_text('{"name":"Smoke"}', encoding='utf-8')
            (project / 'export_presets.json').write_text('[{"name":"Smoke-ios","platform":"ios"}]', encoding='utf-8')
            startup = project / 'configs/modules_game.json'
            startup.write_text(json.dumps({'modules': [
                {'name': 'Aurora.ShaderCompiler', 'dependencies': []},
                {'name': 'Smoke', 'dependencies': ['Aurora.ShaderCompiler']}
            ]}), encoding='utf-8')
            distribution = project / 'products/shaders/Smoke-ios'
            (distribution / 'metal').mkdir(parents=True)
            (distribution / 'distribution.json').write_text(json.dumps({'formatVersion': 2, 'platform': 'ios', 'targets': ['metal'],
                'environments': target_environments('ios', ['metal'])[0]}), encoding='utf-8')
            (distribution / 'metal/index.bin').write_bytes(b'cooked shader index')
            args = SimpleNamespace(engine=str(engine), project=str(project), distribution='Smoke-ios',
                                   destination=str(resources), development=False)
            self.assertEqual(PACKAGE.cmd_stage_runtime(args), 0)
            self.assertEqual((resources / 'products/shaders/metal/index.bin').read_bytes(), b'cooked shader index')
            self.assertEqual(json.loads((resources / 'configs/modules_game.json').read_text())['modules'],
                             [{'name': 'Smoke', 'dependencies': []}])
            self.assertEqual(len(json.loads(startup.read_text())['modules']), 2)

    def test_apple_products_are_cooked_before_the_ide_build_deploy_runs(self):
        events = []
        with mock.patch.object(PACKAGE, 'run_cook', side_effect=lambda *args: events.append('assets')), \
                mock.patch.object(PACKAGE, 'prepare_shader_products', side_effect=lambda *args: events.append('shaders') or True), \
                mock.patch.object(PACKAGE, 'run_cmake_preset', side_effect=lambda *args, **kwargs: events.append('native')), \
                mock.patch.object(PACKAGE, 'collect_apple_package', side_effect=lambda *args: events.append('archive')), \
                mock.patch.object(PACKAGE, 'find_apple_app', return_value=None):
            PACKAGE._prepare(Path('/engine'), Path('/project'), {'platform': 'ios', 'signing': 'off'},
                             {'{{ANDROID_NAMESPACE}}': 'test.app', '{{PROJECT_NAME}}': 'Smoke'}, {}, True)
        self.assertEqual(events, ['assets', 'shaders', 'native', 'archive'])

    def test_android_shipping_excludes_the_compiler_but_development_preserves_it(self):
        source = PACKAGE.REPO_ROOT / 'engine/templates/ide/android/app/build.gradle'
        for development in (False, True):
            tokens = PACKAGE.build_tokens({'name': 'Smoke'}, {'platform': 'android',
                'cacheVariables': {'SKY_DEVELOP': development}}, {})
            text = PACKAGE.substitute_text(source.read_text(), tokens)
            self.assertIn('if (!' + str(development).lower() + ')', text)
            self.assertIn('**/libAurora.ShaderCompiler.so', text)
            self.assertIn('**/libslang*.so', text)
            self.assertIn('merge${variantTaskName}Assets', text)

    def test_static_shipping_registration_omits_the_compiler_factory(self):
        template = (PACKAGE.REPO_ROOT / 'engine/templates/game/native/CMakeLists.txt').read_text()
        registration = template[template.index('if (SKY_MODULE_PACKAGING STREQUAL "static")'):
                                template.index('# The same deployment runs')]
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / 'main.cpp').write_text('int main() {}\n')
            (root / 'CMakeLists.txt').write_text(
                'cmake_minimum_required(VERSION 3.20)\nproject(StaticShipping CXX)\n'
                'include("' + (PACKAGE.REPO_ROOT / 'cmake/functions.cmake').as_posix() + '")\n'
                'set(SKY_MODULE_PACKAGING static)\n'
                'add_executable(Launcher main.cpp)\n'
                'add_library(Aurora.Render INTERFACE)\nadd_library(Aurora.ShaderCompiler INTERFACE)\n'
                'set_property(GLOBAL PROPERTY SKY_STATIC_MODULE_TARGETS Aurora.Render Aurora.ShaderCompiler)\n'
                + registration, encoding='utf-8')
            for development in ('OFF', 'ON'):
                build = root / development
                subprocess.run(['cmake', '-S', str(root), '-B', str(build), '-DSKY_DEVELOP=' + development],
                               check=True, capture_output=True, text=True)
                registration_text = (build / 'RegisterStaticModules.cpp').read_text()
                self.assertIn('sky_module_create_Aurora_Render', registration_text)
                self.assertEqual('sky_module_create_Aurora_ShaderCompiler' in registration_text, development == 'ON')

    @unittest.skipUnless(sys.platform == 'darwin', 'requires native Apple bundle generation')
    def test_building_only_the_launcher_deploys_products_inside_the_new_bundle(self):
        template = (PACKAGE.REPO_ROOT / 'engine/templates/game/native/CMakeLists.txt').read_text()
        deployment = template[template.index('# The same deployment runs'):
                              template.index('function(_sky_collect_imported_runtimes')]
        with tempfile.TemporaryDirectory() as temp:
            engine, project, app, resources = self.make_app(Path(temp), 'macos')
            shutil.rmtree(app)
            (project / 'Smoke.skyproj').write_text('{"name":"Smoke"}', encoding='utf-8')
            (project / 'export_presets.json').write_text('[{"name":"Smoke-macos","platform":"macos"}]', encoding='utf-8')
            distribution = project / 'products/shaders/Smoke-macos'
            (distribution / 'metal').mkdir(parents=True)
            (distribution / 'distribution.json').write_text(json.dumps({'formatVersion': 2, 'platform': 'macos', 'targets': ['metal'],
                'environments': target_environments('macos', ['metal'])[0]}), encoding='utf-8')
            (distribution / 'metal/index.bin').write_bytes(b'shader index')
            native = project / 'native'
            native.mkdir()
            (native / 'main.c').write_text('int main(void) { return 0; }\n')
            (native / 'CMakeLists.txt').write_text(
                'cmake_minimum_required(VERSION 3.20)\nproject(Smoke C)\n'
                'set(ENGINE_ROOT "' + str(PACKAGE.REPO_ROOT) + '")\n'
                'set(SKY_PLATFORM macos)\n'
                'add_executable(Launcher MACOSX_BUNDLE main.c)\n'
                'set_target_properties(Launcher PROPERTIES OUTPUT_NAME Smoke RUNTIME_OUTPUT_DIRECTORY "'
                + str(project / 'build/macos/bin/Release') + '")\n'
                + deployment.replace('{{PROJECT_NAME}}', 'Smoke'), encoding='utf-8')
            subprocess.run(['cmake', '-S', str(native), '-B', str(project / 'cmake')],
                           check=True, capture_output=True, text=True)
            subprocess.run(['cmake', '--build', str(project / 'cmake'), '--target', 'Launcher'],
                           check=True, capture_output=True, text=True)
            self.assertTrue((resources / 'Smoke').is_file())
            self.assertEqual((resources / 'products/shaders/metal/index.bin').read_bytes(), b'shader index')


if __name__ == '__main__':
    unittest.main()
