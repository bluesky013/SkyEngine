"""Package boundary regressions with an optional real Android SDK asset-packaging check."""

import importlib.util
import json
import os
from pathlib import Path
import re
import stat
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
from types import SimpleNamespace
import zipfile

if __package__:
    from .asset_product_test_helpers import product_bytes
else:
    from asset_product_test_helpers import product_bytes

SCRIPT = Path(__file__).resolve().parents[1] / 'package_project.py'
SPEC = importlib.util.spec_from_file_location('package_project', SCRIPT)
PACKAGE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PACKAGE)
from shader_project import target_environments


class NativePackageArtifactTest(unittest.TestCase):
    def test_framework_globs_regenerate_after_source_and_header_changes_without_reconfigure(self):
        if shutil.which('cmake') is None or shutil.which('ninja') is None:
            self.skipTest('CMake and Ninja are required for the incremental source inventory regression')
        framework = (PACKAGE.REPO_ROOT / 'engine/framework/CMakeLists.txt').read_text(encoding='utf-8')
        expressions = []
        for name in ('APP_SRC', 'APP_INC'):
            match = re.search(r'file\s*\(\s*GLOB_RECURSE\s+' + name + r'\b[^)]*\)', framework, re.DOTALL)
            self.assertIsNotNone(match, name)
            expressions.append(match.group())
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source, headers, build = root / 'src', root / 'include', root / 'build'
            source.mkdir()
            headers.mkdir()
            old_source, old_header = source / 'OldProduct.cpp', headers / 'OldProduct.h'
            old_source.write_text('int product_value() { return 1; }\n', encoding='utf-8')
            old_header.write_text('#pragma once\n', encoding='utf-8')
            (root / 'CMakeLists.txt').write_text(
                'cmake_minimum_required(VERSION 3.19)\nproject(FrameworkSourceInventory CXX)\n'
                + '\n'.join(expressions) + '\n'
                + 'add_library(Inventory STATIC ${APP_SRC} ${APP_INC})\n'
                + 'file(WRITE "${CMAKE_BINARY_DIR}/sources.txt" "${APP_SRC};${APP_INC}")\n', encoding='utf-8')

            def run(command):
                result = subprocess.run(command, capture_output=True, text=True, timeout=120)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

            def inventory():
                return {Path(value).name for value in (build / 'sources.txt').read_text().split(';') if value}

            run(['cmake', '-S', str(root), '-B', str(build), '-G', 'Ninja'])
            run(['cmake', '--build', str(build)])
            self.assertEqual(inventory(), {'OldProduct.cpp', 'OldProduct.h'})
            old_source.unlink()
            (source / 'AssetProduct.cpp').write_text('int product_value() { return 2; }\n', encoding='utf-8')
            run(['cmake', '--build', str(build)])
            self.assertEqual(inventory(), {'AssetProduct.cpp', 'OldProduct.h'})
            old_header.rename(headers / 'AssetProduct.h')
            run(['cmake', '--build', str(build)])
            self.assertEqual(inventory(), {'AssetProduct.cpp', 'AssetProduct.h'})

    def test_full_cook_replaces_all_old_products_before_the_tool_runs(self):
        with tempfile.TemporaryDirectory() as temporary:
            project = Path(temporary)
            products = project / 'products'
            for relative in ('common/groups/old/current.bin', 'common/11/old.bin', 'shaders/old/index.bin'):
                path = products / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(b'stale')
            source = project / 'assets/source.world'
            source.parent.mkdir()
            source.write_bytes(b'authoring source')
            cooked = products / 'common/22/new.bin'

            def cook(command, **kwargs):
                self.assertFalse(products.exists())
                self.assertIn('--cook-all', command)
                self.assertEqual(command[-2:], ['--platform', 'ios'])
                cooked.parent.mkdir(parents=True)
                cooked.write_bytes(b'new product')
                return SimpleNamespace(returncode=0)

            with mock.patch.object(PACKAGE, 'find_asset_tool', return_value=Path('/engine/AssetTool')), \
                    mock.patch.object(PACKAGE.subprocess, 'run', side_effect=cook):
                self.assertTrue(PACKAGE.run_cook(Path('/engine'), project, 'ios'))
            self.assertEqual([path for path in products.rglob('*') if path.is_file()], [cooked])
            self.assertEqual(source.read_bytes(), b'authoring source')

    def test_failed_cook_discards_partial_products_and_missing_tool_never_claims_success(self):
        with tempfile.TemporaryDirectory() as temporary:
            project = Path(temporary)
            marker = project / 'products/common/11/old.bin'
            marker.parent.mkdir(parents=True)
            marker.write_bytes(b'old product')
            with mock.patch.object(PACKAGE, 'find_asset_tool', return_value=None), \
                    mock.patch.object(PACKAGE.subprocess, 'run') as run:
                self.assertFalse(PACKAGE.run_cook(Path('/engine'), project, 'ios'))
                run.assert_not_called()
            self.assertEqual(marker.read_bytes(), b'old product')

            def fail(*args, **kwargs):
                self.assertFalse(marker.exists())
                marker.parent.mkdir(parents=True)
                marker.write_bytes(b'partial product')
                return SimpleNamespace(returncode=7)

            with mock.patch.object(PACKAGE, 'find_asset_tool', return_value=Path('/engine/AssetTool')), \
                    mock.patch.object(PACKAGE.subprocess, 'run', side_effect=fail):
                self.assertFalse(PACKAGE.run_cook(Path('/engine'), project, 'ios'))
            self.assertFalse((project / 'products').exists())

    def test_every_platform_stops_before_shader_staging_and_native_build_after_cook_failure(self):
        with tempfile.TemporaryDirectory() as temporary:
            project = Path(temporary)
            for platform in ('android', 'ios', 'macos'):
                with self.subTest(platform=platform), \
                        mock.patch.object(PACKAGE, 'instantiate_template'), \
                        mock.patch.object(PACKAGE, 'stage_configs', return_value=True), \
                        mock.patch.object(PACKAGE, 'find_apple_app', return_value=None), \
                        mock.patch.object(PACKAGE, 'run_cook', return_value=False), \
                        mock.patch.object(PACKAGE, 'prepare_shader_products') as shaders, \
                        mock.patch.object(PACKAGE, 'stage_bundles') as bundles, \
                        mock.patch.object(PACKAGE, 'run_cmake_preset') as native, \
                        mock.patch.object(PACKAGE, 'package_android') as android, \
                        mock.patch.object(PACKAGE, 'collect_apple_package') as apple:
                    result = PACKAGE._prepare(Path('/engine'), project, {'platform': platform, 'signing': 'off'},
                                              {'{{PROJECT_NAME}}': 'Smoke'}, {}, True)
                    self.assertEqual(result, 1)
                    for step in (shaders, bundles, native, android, apple):
                        step.assert_not_called()

    @unittest.skipIf(os.name == 'nt', 'requires unprivileged symlink creation')
    def test_full_cook_retires_a_products_alias_without_deleting_its_target(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            project, external = root / 'project', root / 'external'
            project.mkdir()
            external.mkdir()
            marker = external / 'keep.bin'
            marker.write_bytes(b'external product')
            (project / 'products').symlink_to(external, target_is_directory=True)
            with mock.patch.object(PACKAGE, 'find_asset_tool', return_value=Path('/engine/AssetTool')), \
                    mock.patch.object(PACKAGE.subprocess, 'run', return_value=SimpleNamespace(returncode=0)):
                self.assertTrue(PACKAGE.run_cook(Path('/engine'), project, 'ios'))
            self.assertFalse((project / 'products').exists())
            self.assertEqual(marker.read_bytes(), b'external product')

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

    def test_android_sdk_default_filter_keeps_flat_products_and_excludes_hidden_assets(self):
        template = PACKAGE.REPO_ROOT / 'engine/templates/ide/android/app/build.gradle'
        self.assertNotRegex(template.read_text(encoding='utf-8'), r'\bignoreAssetsPatterns?\b')
        sdk_roots = [Path(value) for key in ('ANDROID_HOME', 'ANDROID_SDK_ROOT') if (value := os.environ.get(key))]
        sdk_roots += [Path.home() / 'Library/Android/sdk', Path.home() / 'Android/Sdk']
        aapt_name = 'aapt.exe' if os.name == 'nt' else 'aapt'
        for sdk in sdk_roots:
            tools = sorted(sdk.glob('build-tools/*/' + aapt_name),
                           key=lambda path: tuple(int(part) for part in re.findall(r'\d+', path.parent.name)))
            platforms = sorted(sdk.glob('platforms/android-*/android.jar'),
                               key=lambda path: tuple(int(part) for part in re.findall(r'\d+', path.parent.name)))
            if tools and platforms:
                aapt, android_jar = tools[-1], platforms[-1]
                break
        else:
            self.skipTest('Android SDK aapt and android.jar are required for this optional asset-packaging regression')
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            assets = root / 'assets'
            records = {
                'products/common/product.index': b'{"path":"main.world","id":"11111111-2222-4333-8444-555555555555"}\n',
                'products/common/11/11111111-2222-4333-8444-555555555555.bin': product_bytes(
                    '11111111-2222-4333-8444-555555555555', b'World\0binary payload'),
            }
            ignored = {'.git/config': b'ignored', '.hidden.bin': b'ignored', '.hidden/nested.bin': b'ignored',
                       'products/common/11/.hidden.bin': b'ignored', 'backup.bin~': b'ignored'}
            for relative, data in {**records, **ignored}.items():
                target = assets / relative
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(data)
            manifest = root / 'AndroidManifest.xml'
            manifest.write_text(
                '<manifest xmlns:android="http://schemas.android.com/apk/res/android" package="org.skyengine.flatproducts">'
                '<application android:label="FlatProducts"/></manifest>', encoding='utf-8')
            apk = root / 'flat-products.apk'
            result = subprocess.run([str(aapt), 'package', '-f', '-F', str(apk), '-M', str(manifest),
                                     '-I', str(android_jar), '-A', str(assets),
                                     '--min-sdk-version', '29', '--target-sdk-version', '33'],
                                    capture_output=True, text=True, timeout=60)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            with zipfile.ZipFile(apk) as archive:
                output = {path: archive.read(path) for path in archive.namelist() if path.startswith('assets/')}
            for relative, data in records.items():
                self.assertEqual(output.get('assets/' + relative), data)
            for relative in ignored:
                self.assertNotIn('assets/' + relative, output)

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
            world = project / 'assets/scenes/lighting.world'
            world.parent.mkdir(parents=True)
            content = b'{"actors": [{"name": "Cube"}], "assetRoots": []}\n'
            world.write_bytes(content)
            with mock.patch.object(PACKAGE, 'find_asset_tool', return_value=Path('/engine/AssetTool')), \
                    mock.patch.object(PACKAGE.subprocess, 'run', return_value=SimpleNamespace(returncode=0)):
                archive = PACKAGE.collect_apple_package(engine, project, {'platform': 'ios'}, 'Smoke')
            self.assertTrue(archive.name.endswith('-unsigned.ipa'))
            with zipfile.ZipFile(archive) as output:
                self.assertEqual(output.read('Payload/Smoke.app/products/common/asset.bin'), b'cooked')
                self.assertFalse(any(name.endswith('.world') for name in output.namelist()))
                roots = json.loads(output.read('Payload/Smoke.app/products/asset-roots.json'))
                self.assertEqual(roots['roots'], [{'path': 'scenes/lighting.world', 'type': 'World'}])
                self.assertEqual(json.loads(output.read('Payload/Smoke.app/configs/modules_game.json')),
                                 {'modules': [{'name': 'Smoke', 'dependencies': []}]})
                self.assertTrue(output.getinfo('Payload/Smoke.app/Smoke').external_attr >> 16 & stat.S_IXUSR)
            self.assertFalse((project / 'build/ios/bin/products').exists())

    def test_package_keeps_flat_products_and_index_without_publication_scratch_files(self):
        with tempfile.TemporaryDirectory() as temporary:
            engine, project, _, _ = self.make_app(Path(temporary), 'ios')
            bundle = project / 'products/common'
            identity = '11111111-2222-4333-8444-555555555555'
            records = {
                'product.index': ('{"path":"main.world","id":"' + identity + '"}\n').encode('utf-8'),
                '11/' + identity + '.bin': product_bytes(identity, b'cooked binary payload'),
            }
            scratch = {
                'product.lock': b'lock',
                'product.index.abandoned.tmp': b'partial index',
                '11/' + identity + '.bin.abandoned.tmp': b'partial product',
            }
            for relative, data in {**records, **scratch}.items():
                path = bundle / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(data)
            archive = PACKAGE.collect_apple_package(engine, project, {'platform': 'ios'}, 'Smoke')
            with zipfile.ZipFile(archive) as output:
                prefix = 'Payload/Smoke.app/products/common/'
                for relative, data in records.items():
                    self.assertEqual(output.read(prefix + relative), data)
                for relative in scratch:
                    self.assertNotIn(prefix + relative, output.namelist())
            for relative, data in scratch.items():
                self.assertEqual((bundle / relative).read_bytes(), data)

    def test_worlds_use_cooked_products_and_audit_logical_source_paths(self):
        with tempfile.TemporaryDirectory() as temp:
            project = Path(temp) / 'project'
            assets = project / 'assets'
            (assets / 'scenes').mkdir(parents=True)
            documents = {
                'main.world': b'{"actors": [], "assetRoots": []}\n',
                'scenes/lighting.world': b'{\n  "actors": [{"name": "Cube"}], "assetRoots": []\n}\n',
            }
            for path, content in documents.items():
                (assets / path).write_bytes(content)
            (assets / 'notes.json').write_text('not a World document', encoding='utf-8')
            destination = Path(temp) / 'staged/products'
            with mock.patch.object(PACKAGE, 'find_asset_tool', return_value=Path('/engine/AssetTool')), \
                    mock.patch.object(PACKAGE.subprocess, 'run', return_value=SimpleNamespace(returncode=0)) as audit:
                self.assertEqual(PACKAGE.stage_bundles(project, {}, destination), ['common'])
            audit.assert_called_once()
            self.assertFalse(list(destination.rglob('*.world')))
            roots = json.loads((destination / 'asset-roots.json').read_text())
            self.assertEqual(roots['roots'], [{'path': path, 'type': 'World'} for path in sorted(documents)])

    def test_explicit_world_selection_drives_staging_and_dependency_audit_together(self):
        with tempfile.TemporaryDirectory() as temp:
            project = Path(temp) / 'project'
            assets = project / 'assets'
            (assets / 'scenes').mkdir(parents=True)
            reference = {'uuid': '11111111-2222-4333-8444-555555555555', 'type': 'AuroraMaterial'}
            selected = assets / 'scenes/selected.world'
            content = json.dumps({'actors': [], 'assetRoots': [reference]}).encode('utf-8')
            selected.write_bytes(content)
            (assets / 'unselected.world').write_text('{"actors": []}', encoding='utf-8')
            destination = Path(temp) / 'staged/products'
            (destination / 'worlds').mkdir(parents=True)
            (destination / 'worlds/removed.world').write_text('old package', encoding='utf-8')
            preset = {'worlds': ['scenes/selected.world', 'scenes/selected.world']}
            with mock.patch.object(PACKAGE, 'find_asset_tool', return_value=Path('/engine/AssetTool')), \
                    mock.patch.object(PACKAGE.subprocess, 'run', return_value=SimpleNamespace(returncode=0)) as audit:
                self.assertEqual(PACKAGE.stage_bundles(project, preset, destination), ['common'])
            audit.assert_called_once()
            self.assertFalse(list(destination.rglob('*.world')))
            manifest = json.loads((destination / 'asset-roots.json').read_text(encoding='utf-8'))
            self.assertEqual(manifest['roots'], [{'path': 'scenes/selected.world', 'type': 'World'}])

    @unittest.skipIf(os.name == 'nt', 'requires unprivileged symlink creation')
    def test_internal_world_symlink_keeps_its_selected_logical_identity(self):
        with tempfile.TemporaryDirectory() as temp:
            project = Path(temp) / 'project'
            assets = project / 'assets'
            (assets / 'scenes').mkdir(parents=True)
            (assets / 'shared').mkdir()
            (assets / 'shared/main.world').write_text('{"actors": [], "assetRoots": []}')
            (assets / 'scenes/start.world').symlink_to(assets / 'shared/main.world')
            destination = Path(temp) / 'staged/products'
            with mock.patch.object(PACKAGE, 'find_asset_tool', return_value=Path('/engine/AssetTool')), \
                    mock.patch.object(PACKAGE.subprocess, 'run', return_value=SimpleNamespace(returncode=0)):
                self.assertEqual(PACKAGE.stage_bundles(project, {'worlds': ['scenes\\start.world']}, destination), ['common'])
            roots = json.loads((destination / 'asset-roots.json').read_text())
            self.assertEqual(roots['roots'], [{'path': 'scenes/start.world', 'type': 'World'}])

    def test_missing_cooked_world_rejects_package_instead_of_shipping_authoring_json(self):
        with tempfile.TemporaryDirectory() as temp:
            engine, project, app, _ = self.make_app(Path(temp), 'ios')
            (project / 'assets').mkdir()
            (project / 'assets/main.world').write_text('{"actors": [], "assetRoots": []}')
            with mock.patch.object(PACKAGE, 'find_asset_tool', return_value=Path('/engine/AssetTool')), \
                    mock.patch.object(PACKAGE.subprocess, 'run', return_value=SimpleNamespace(returncode=1)):
                self.assertIsNone(PACKAGE.collect_apple_package(engine, project, {'platform': 'ios'}, 'Smoke'))
            self.assertFalse(list((project / 'build/ios').glob('*.ipa')))
            self.assertFalse(list(app.rglob('*.world')))

    def test_default_world_must_be_selected_before_replacing_package_products(self):
        with tempfile.TemporaryDirectory() as temp:
            project = Path(temp) / 'project'
            (project / 'assets').mkdir(parents=True)
            for name in ('default', 'other'):
                (project / ('assets/' + name + '.world')).write_text('{"actors": [], "assetRoots": []}')
            (project / 'Game.skyproj').write_text('{"defaultScene": "default.world"}')
            destination = Path(temp) / 'staged/products'
            destination.mkdir(parents=True)
            marker = destination / 'previous.bin'
            marker.write_bytes(b'previous package')
            self.assertIsNone(PACKAGE.stage_bundles(project, {'worlds': ['other.world']}, destination))
            self.assertEqual(marker.read_bytes(), b'previous package')

    def test_world_selection_rejects_escape_missing_and_invalid_documents_before_staging(self):
        with tempfile.TemporaryDirectory() as temp:
            project = Path(temp) / 'project'
            assets = project / 'assets'
            assets.mkdir(parents=True)
            outside = project / 'outside.world'
            outside.write_text('{"actors": [], "assetRoots": []}', encoding='utf-8')
            (assets / 'legacy.world').write_text('{"actors": []}', encoding='utf-8')
            (assets / 'invalid.world').write_text('{"assetRoots": [{"uuid": 3, "type": "Mesh"}]}', encoding='utf-8')
            destination = Path(temp) / 'staged/products'
            destination.mkdir(parents=True)
            marker = destination / 'keep.bin'
            marker.write_bytes(b'previous package')
            for path in ('../outside.world', str(outside), 'missing.world', 'legacy.world', 'invalid.world'):
                with self.subTest(path=path):
                    self.assertIsNone(PACKAGE.stage_bundles(project, {'worlds': [path]}, destination))
                    self.assertEqual(marker.read_bytes(), b'previous package')

    @unittest.skipIf(os.name == 'nt', 'requires unprivileged symlink creation')
    def test_world_symlink_cannot_escape_the_project_assets(self):
        with tempfile.TemporaryDirectory() as temp:
            project = Path(temp) / 'project'
            assets = project / 'assets'
            assets.mkdir(parents=True)
            outside = Path(temp) / 'outside.world'
            outside.write_text('{"assetRoots": []}', encoding='utf-8')
            (assets / 'linked.world').symlink_to(outside)
            destination = Path(temp) / 'staged/products'
            self.assertIsNone(PACKAGE.stage_bundles(project, {}, destination))
            self.assertFalse(destination.exists())

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
        with mock.patch.object(PACKAGE, 'run_cook', side_effect=lambda *args: events.append('assets') or True), \
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
