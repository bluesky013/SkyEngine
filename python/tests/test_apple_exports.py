"""Exercise Apple export settings through generated metadata and collected artifacts."""

import importlib.util
import json
import os
from pathlib import Path
import plistlib
import re
import shutil
import subprocess
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock
import zipfile


SPEC = importlib.util.spec_from_file_location('apple_exports_package', Path(__file__).parents[1] / 'package_project.py')
PACKAGE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PACKAGE)


class AppleProjectFixture:
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        with mock.patch('builtins.print'):
            PACKAGE.cmd_create(SimpleNamespace(engine=None, dir=temporary.name, name='AppleExport', force=False))
        self.project = (Path(temporary.name) / 'AppleExport').resolve()
        self.descriptor = self.project / 'AppleExport.skyproj'
        self.commands = []

    def preset(self, platform, **settings):
        preset = {'name': 'Selected', 'platform': platform, 'bundleId': 'com.skyengine.apple-export',
                  'bundle': True, 'signing': 'off', **settings}
        (self.project / 'export_presets.json').write_text(json.dumps([preset]), encoding='utf-8')
        return preset

    def arguments(self):
        return SimpleNamespace(engine=None, project=str(self.project), preset='Selected')

    def make_app(self, platform, configuration, *, version=None):
        app = self.project / 'build' / platform / 'bin' / configuration / 'AppleExport.app'
        resources = app / 'Contents/Resources' if platform == 'macos' else app
        resources.mkdir(parents=True, exist_ok=True)
        executable_directory = app / 'Contents/MacOS' if platform == 'macos' else app
        executable_directory.mkdir(parents=True, exist_ok=True)
        executable = executable_directory / 'AppleExport'
        executable.write_bytes(configuration.encode('ascii'))
        executable.chmod(0o755)
        manifest_path = app / 'Contents/Info.plist' if platform == 'macos' else app / 'Info.plist'
        exported = self.project / 'build' / platform / 'export/Info.plist'
        manifest = plistlib.loads(exported.read_bytes()) if exported.is_file() else {}
        if version is not None:
            manifest['CFBundleVersion'] = version
        manifest_path.write_bytes(plistlib.dumps(manifest))
        return app

class AppleExportTest(AppleProjectFixture, unittest.TestCase):
    def test_variant_drives_configure_build_and_exact_collection_when_both_configs_exist(self):
        for platform in ('macos', 'ios'):
            for variant, configuration in (('debug', 'Debug'), ('release', 'Release')):
                with self.subTest(platform=platform, variant=variant):
                    self.preset(platform, variant=variant, versionName='9.8.7', versionCode=42)
                    other = 'Release' if configuration == 'Debug' else 'Debug'
                    other_app = self.make_app(platform, other)
                    original_other = {p.relative_to(other_app): p.read_bytes()
                                      for p in other_app.rglob('*') if p.is_file()}
                    commands = []

                    def native(command, **kwargs):
                        commands.append(command)
                        if '--build' in command:
                            self.assertEqual(command[command.index('--config') + 1], configuration)
                            self.make_app(platform, configuration)
                        return SimpleNamespace(returncode=0)

                    with mock.patch.object(PACKAGE, 'run_cook', return_value=True), \
                            mock.patch.object(PACKAGE, 'prepare_shader_products', return_value=True), \
                            mock.patch.object(PACKAGE.subprocess, 'run', side_effect=native), \
                            mock.patch('builtins.print'):
                        self.assertEqual(PACKAGE.cmd_package(self.arguments()), 0)
                    self.assertIn('-DCMAKE_BUILD_TYPE=' + configuration, commands[0])
                    self.assertEqual(commands[1][:4], ['cmake', '--build', '--preset', platform])
                    archive = self.project / 'build' / platform / (
                        'AppleExport-unsigned.ipa' if platform == 'ios' else 'AppleExport-macos.zip')
                    with zipfile.ZipFile(archive) as package:
                        prefix = 'Payload/AppleExport.app/' if platform == 'ios' else 'AppleExport.app/Contents/'
                        executable = prefix + ('AppleExport' if platform == 'ios' else 'MacOS/AppleExport')
                        self.assertEqual(package.read(executable), configuration.encode('ascii'))
                        metadata = plistlib.loads(package.read(prefix + 'Info.plist'))
                        self.assertEqual(metadata['CFBundleShortVersionString'], '9.8.7')
                        self.assertEqual(metadata['CFBundleVersion'], '42')
                    for relative, data in original_other.items():
                        self.assertEqual((other_app / relative).read_bytes(), data)

    def test_metadata_inherits_descriptor_and_overrides_without_mutating_project_sources(self):
        descriptor = json.loads(self.descriptor.read_text())
        descriptor['settings'].update(versionName='3.4.5', versionCode=67)
        self.descriptor.write_text(json.dumps(descriptor))
        for platform in ('macos', 'ios'):
            with self.subTest(platform=platform):
                source = self.project / 'native' / platform / 'Info.plist'
                original = plistlib.loads(source.read_bytes())
                original['ProjectOwnedKey'] = 'preserve'
                source.write_bytes(plistlib.dumps(original))
                source_bytes = source.read_bytes()
                for overrides, name, code in (({}, '3.4.5', '67'),
                                              ({'versionName': '7.8.9', 'versionCode': 321}, '7.8.9', '321')):
                    self.preset(platform, **overrides)
                    with mock.patch.object(PACKAGE, 'run_cook', return_value=True), \
                            mock.patch.object(PACKAGE, 'prepare_shader_products', return_value=True), \
                            mock.patch.object(PACKAGE, 'run_cmake_preset') as native, \
                            mock.patch('builtins.print'):
                        self.assertEqual(PACKAGE.cmd_generate(self.arguments()), 0)
                    exported = self.project / 'build' / platform / 'export/Info.plist'
                    result = plistlib.loads(exported.read_bytes())
                    self.assertEqual(result['CFBundleShortVersionString'], name)
                    self.assertEqual(result['CFBundleVersion'], code)
                    self.assertEqual(result['ProjectOwnedKey'], 'preserve')
                    self.assertEqual(source.read_bytes(), source_bytes)
                    variable = 'SKY_IOS_INFO_PLIST' if platform == 'ios' else 'SKY_MACOS_INFO_PLIST'
                    self.assertIn('-D' + variable + '=' + exported.as_posix(), native.call_args.kwargs['extra'])

    def test_invalid_settings_preserve_existing_export_and_stop_before_cook(self):
        marker = self.project / 'build/macos/export/Info.plist'
        marker.parent.mkdir(parents=True)
        marker.write_bytes(b'previous export')
        invalid = [{'variant': value} for value in ('', 'profile', None, True, [], {})]
        invalid += [{'versionName': value} for value in ('', '1.2.3.4', 'release', None, True, 1)]
        invalid += [{'versionCode': value} for value in (0, -1, None, True, '42', 1.5)]
        invalid += [{'extraBuildArgs': value} for value in (
            ['--config', 'Debug'], ['--config=Release'], ['--preset', 'other'], ['--preset=ios'], [None], True)]
        for settings in invalid:
            with self.subTest(settings=settings):
                self.preset('macos', **settings)
                with mock.patch.object(PACKAGE, 'run_cook') as cook, \
                        mock.patch.object(PACKAGE, 'run_cmake_preset') as native:
                    self.assertEqual(PACKAGE.cmd_generate(self.arguments()), 1)
                cook.assert_not_called()
                native.assert_not_called()
                self.assertEqual(marker.read_bytes(), b'previous export')

    def test_invalid_descriptor_version_cannot_silently_use_default(self):
        descriptor = json.loads(self.descriptor.read_text())
        descriptor['settings']['versionCode'] = 0
        self.descriptor.write_text(json.dumps(descriptor))
        self.preset('ios')
        with mock.patch.object(PACKAGE, 'run_cook') as cook:
            self.assertEqual(PACKAGE.cmd_generate(self.arguments()), 1)
        cook.assert_not_called()

    def test_missing_selected_app_never_uses_another_configuration(self):
        self.make_app('ios', 'Release')
        self.assertIsNone(PACKAGE.find_apple_app(self.project, 'ios', 'AppleExport', required=False,
                                               configuration='Debug'))
        self.assertEqual(PACKAGE.find_apple_app(self.project, 'ios', 'AppleExport', configuration='Release'),
                         self.project / 'build/ios/bin/Release/AppleExport.app')

    def test_built_version_mismatch_cannot_publish_archive_or_stage_resources(self):
        for platform in ('macos', 'ios'):
            with self.subTest(platform=platform):
                self.preset(platform, versionName='2.3.4', versionCode=25)

                def native(command, **kwargs):
                    if '--build' in command:
                        self.make_app(platform, 'Release', version='old')
                    return SimpleNamespace(returncode=0)

                with mock.patch.object(PACKAGE, 'run_cook', return_value=True), \
                        mock.patch.object(PACKAGE, 'prepare_shader_products', return_value=True), \
                        mock.patch.object(PACKAGE, 'stage_runtime_resources') as stage, \
                        mock.patch.object(PACKAGE.subprocess, 'run', side_effect=native), \
                        mock.patch('builtins.print'):
                    self.assertEqual(PACKAGE.cmd_package(self.arguments()), 1)
                stage.assert_not_called()
                self.assertFalse(list((self.project / 'build' / platform).glob('*.ipa')))
                self.assertFalse(list((self.project / 'build' / platform).glob('*-macos.zip')))

    def test_macos_cmake_consumes_the_export_copy_and_default_variant_stays_release(self):
        if not shutil.which('cmake'):
            self.skipTest('CMake is required to inspect the launcher metadata contract')
        self.preset('macos', versionName='2.0.3', versionCode=8)
        with mock.patch.object(PACKAGE, 'run_cook', return_value=True), \
                mock.patch.object(PACKAGE, 'prepare_shader_products', return_value=True), \
                mock.patch.object(PACKAGE, 'run_cmake_preset') as native, mock.patch('builtins.print'):
            self.assertEqual(PACKAGE.cmd_generate(self.arguments()), 0)
        self.assertEqual(native.call_args.kwargs['configuration'], 'Release')
        exported = self.project / 'build/macos/export/Info.plist'
        fixture = self.project / 'build/metadata-contract'
        fixture.mkdir()
        (fixture / 'CMakeLists.txt').write_text(
            'cmake_minimum_required(VERSION 3.20)\nproject(Metadata NONE)\n'
            'function(sky_add_exe)\nadd_custom_target(Launcher)\nendfunction()\n'
            'set(SKY_MACOS_BUNDLE ON)\nset(SKY_APPLE_TEAM_ID "")\n'
            'set(SKY_APPLE_SIGNING_IDENTITY "")\nset(SKY_APPLE_PROVISIONING_PROFILE "")\n'
            'include("' + (self.project / 'native/macos/CMakeLists.txt').as_posix() + '")\n'
            'get_target_property(selected Launcher MACOSX_BUNDLE_INFO_PLIST)\n'
            'file(WRITE "${CMAKE_BINARY_DIR}/selected.txt" "${selected}")\n', encoding='utf-8')
        result = subprocess.run(['cmake', '-S', str(fixture), '-B', str(fixture / 'out'),
                                 '-DSKY_MACOS_INFO_PLIST=' + exported.as_posix()], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual((fixture / 'out/selected.txt').read_text(), exported.as_posix())


class AppleDistributionTest(AppleProjectFixture, unittest.TestCase):
    def distribution_args(self, platform='ios', **overrides):
        self.preset(platform, variant='debug', versionName='2.3.4', versionCode=25,
                    signing='auto', team='TESTTEAM')
        options = self.project.parent / 'external-export-options.plist'
        options.write_bytes(plistlib.dumps({'method': 'debugging', 'destination': 'export',
                                           'teamID': 'TESTTEAM', 'manageAppVersionAndBuildNumber': False}))
        args = {'engine': None, 'project': str(self.project), 'preset': 'Selected',
                'export_options': str(options), 'archive_only': False, 'device': None, 'launch': False}
        args.update(overrides)
        return SimpleNamespace(**args)

    def fake_distribution(self, platform, *, fail=None, adhoc=False):
        self.events = []
        self.signed = {}
        output = self.project / 'build' / platform / 'distribution'
        receipt = self.project / 'build' / platform / 'apple-stage/Debug.json'

        def stage(engine, project, preset, destination, development):
            self.events.append('stage')
            for relative, data in {'configs/modules_game.json': b'{"modules":[]}',
                                   'products/bundles.json': b'{"bundles":[]}'}.items():
                path = destination / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(data)
            return True

        def prepare(engine, project, preset, tokens, credentials, build, archive_resources):
            self.assertFalse(build)
            self.assertTrue(archive_resources)
            (self.project / 'build' / platform / 'AppleExport.xcodeproj').mkdir(parents=True, exist_ok=True)
            shader = self.project / 'products/shaders/Selected/distribution.json'
            shader.parent.mkdir(parents=True, exist_ok=True)
            shader.write_text('{}')

        def native(command, **kwargs):
            self.commands.append(command)
            if command[0] == 'plutil':
                options = plistlib.loads(Path(command[-1]).read_bytes())
                return SimpleNamespace(returncode=0, stdout=plistlib.dumps(options).decode('utf-8'), stderr='')
            if command[0] == 'xcodebuild' and command[-1] == 'archive':
                self.events.append('archive')
                for path in getattr(self, 'expected_retired_products', []):
                    self.assertFalse(path.exists(), str(path))
                self.assertEqual(command[command.index('-configuration') + 1], 'Debug')
                archive = Path(command[command.index('-archivePath') + 1])
                archive.mkdir(parents=True)
                archive_info = {'ArchiveVersion': 2, 'Name': 'AppleExport', 'SchemeName': 'Launcher'}
                if fail != 'generic-archive':
                    archive_info['ApplicationProperties'] = {
                        'ApplicationPath': 'Applications/AppleExport.app',
                        'CFBundleIdentifier': 'wrong.identity' if fail == 'wrong-properties' else 'com.skyengine.apple-export',
                        'CFBundleShortVersionString': '2.3.4', 'CFBundleVersion': '25'}
                (archive / 'Info.plist').write_bytes(plistlib.dumps(archive_info))
                if fail == 'archive':
                    return SimpleNamespace(returncode=1, stdout='partial archive', stderr='failure')
                if fail == 'missing-app':
                    return SimpleNamespace(returncode=0, stdout='', stderr='')
                app = archive / 'Products/Applications/AppleExport.app'
                resources = app / 'Contents/Resources' if platform == 'macos' else app
                resources.mkdir(parents=True)
                executable_directory = app / 'Contents/MacOS' if platform == 'macos' else app
                executable_directory.mkdir(parents=True, exist_ok=True)
                executable = executable_directory / 'AppleExport'
                executable.write_bytes(b'signed executable')
                executable.chmod(0o755)
                manifest = app / 'Contents/Info.plist' if platform == 'macos' else app / 'Info.plist'
                manifest.write_bytes(plistlib.dumps({'CFBundleIdentifier': 'com.skyengine.apple-export',
                                                     'CFBundleExecutable': 'AppleExport',
                                                     'CFBundleShortVersionString': '2.3.4', 'CFBundleVersion': '25'}))
                folder = 'AppleExport.app/Contents/Resources' if platform == 'macos' else 'AppleExport.app'
                stage_args = SimpleNamespace(engine=None, project=str(self.project), distribution='Selected',
                                             destination=None, xcode_environment=True, receipt=str(receipt), development=False)
                with mock.patch.dict(os.environ, {'TARGET_BUILD_DIR': str(app.parent),
                                                   'UNLOCALIZED_RESOURCES_FOLDER_PATH': folder,
                                                   'EXECUTABLE_FOLDER_PATH': 'AppleExport.app/Contents/MacOS',
                                                   'CONFIGURATION': 'Debug'}):
                    self.assertEqual(PACKAGE.cmd_stage_runtime(stage_args), 0)
                self.events.append('sign')
                self.signed[str(app)] = PACKAGE.apple_resource_inventory(resources)
                return SimpleNamespace(returncode=0, stdout='archive completed', stderr='')
            if command[0] == 'codesign':
                app = Path(command[-1])
                resources = app / 'Contents/Resources' if platform == 'macos' else app
                if '--verify' in command:
                    self.events.append('verify')
                    self.assertEqual(PACKAGE.apple_resource_inventory(resources),
                                     self.signed.get(str(app), self.signed[str(output / 'Application.xcarchive/Products/Applications/AppleExport.app')]))
                    return SimpleNamespace(returncode=1 if fail == 'codesign' else 0, stdout='', stderr='')
                identity = 'Signature=adhoc\n' if adhoc else 'Authority=Apple Development: Fixture\nTeamIdentifier=TESTTEAM\n'
                return SimpleNamespace(returncode=0, stdout='', stderr=identity)
            if command[0] == 'xcodebuild' and '-exportArchive' in command:
                self.events.append('export')
                exported = Path(command[command.index('-exportPath') + 1])
                exported.mkdir()
                if fail == 'export':
                    (exported / 'stale.ipa').write_bytes(b'partial export')
                    return SimpleNamespace(returncode=1, stdout='', stderr='export failed')
                app = output / 'Application.xcarchive/Products/Applications/AppleExport.app'
                if platform == 'ios':
                    PACKAGE.zip_bundle(app, exported / 'AppleExport.ipa', 'Payload')
                else:
                    shutil.copytree(app, exported / app.name)
                return SimpleNamespace(returncode=0, stdout='export completed', stderr='')
            if command[0] == 'ditto':
                self.events.append('inspect')
                with zipfile.ZipFile(command[-2]) as archive:
                    archive.extractall(command[-1])
                    for info in archive.infolist():
                        path = Path(command[-1]) / info.filename
                        path.chmod(info.external_attr >> 16 & 0o777)
                return SimpleNamespace(returncode=0, stdout='', stderr='')
            if command[:5] == ['xcrun', 'devicectl', 'device', 'install', 'app']:
                self.events.append('install')
                return SimpleNamespace(returncode=1 if fail == 'install' else 0, stdout='', stderr='')
            if command[:5] == ['xcrun', 'devicectl', 'device', 'process', 'launch']:
                self.events.append('launch')
                return SimpleNamespace(returncode=0, stdout='', stderr='')
            self.fail('Unexpected native command: ' + repr(command))

        return output, prepare, stage, native

    def run_distribution(self, args, *, fail=None, adhoc=False):
        platform = json.loads((self.project / 'export_presets.json').read_text())[0]['platform']
        output, prepare, stage, native = self.fake_distribution(platform, fail=fail, adhoc=adhoc)
        with mock.patch.object(PACKAGE.sys, 'platform', 'darwin'), \
                mock.patch.object(PACKAGE.shutil, 'which', side_effect=lambda name: '/installed/' + name), \
                mock.patch.object(PACKAGE, '_prepare', side_effect=prepare), \
                mock.patch.object(PACKAGE, 'stage_runtime_resources', side_effect=stage), \
                mock.patch.object(PACKAGE.subprocess, 'run', side_effect=native), \
                mock.patch('builtins.print'):
            result = PACKAGE.cmd_apple_distribute(args)
        return result, output

    def test_archive_export_preserve_pre_sign_resources_and_install_only_explicit_device(self):
        for platform in ('ios', 'macos'):
            with self.subTest(platform=platform):
                args = self.distribution_args(platform)
                self.commands.clear()
                result, output = self.run_distribution(args)
                self.assertEqual(result, 0)
                self.assertEqual(self.events.count('stage'), 1)
                self.assertLess(self.events.index('stage'), self.events.index('sign'))
                self.assertLess(self.events.index('verify'), self.events.index('export'))
                self.assertEqual(self.events.count('verify'), 2)
                self.assertNotIn('install', self.events)
                summary = json.loads((output / 'result.json').read_text())
                self.assertEqual(summary['configuration'], 'Debug')
                self.assertEqual(summary['versionCode'], '25')
                self.assertTrue(Path(summary['artifact']).exists())
                self.assertFalse(any('-allowProvisioningUpdates' in command for command in self.commands))
                self.assertFalse(any('-allowProvisioningDeviceRegistration' in command for command in self.commands))
        args = self.distribution_args('ios', device='explicit-device-udid', launch=True)
        self.commands.clear()
        result, _ = self.run_distribution(args)
        self.assertEqual(result, 0)
        self.assertEqual(self.events[-2:], ['install', 'launch'])
        install = next(command for command in self.commands if command[:5] == ['xcrun', 'devicectl', 'device', 'install', 'app'])
        self.assertEqual(install[install.index('--device') + 1], 'explicit-device-udid')

    def test_archive_only_reports_its_limit_and_never_exports_or_installs(self):
        args = self.distribution_args('macos', archive_only=True, export_options=None)
        result, output = self.run_distribution(args)
        self.assertEqual(result, 0)
        self.assertNotIn('export', self.events)
        self.assertNotIn('install', self.events)
        self.assertTrue(json.loads((output / 'result.json').read_text())['archiveOnly'])

    def test_reusing_archive_cache_removes_old_app_data_and_plugins_but_preserves_objects(self):
        args = self.distribution_args('macos', archive_only=True, export_options=None)
        output = self.project / 'build/macos/distribution'
        derived = output / 'DerivedData/ArchiveIntermediates/Launcher'
        archived = derived / 'InstallationBuildProductsLocation/Applications/AppleExport.app'
        old_library = derived / 'InstallationBuildProductsLocation/@rpath/libOldDependency.dylib'
        old_library.parent.mkdir(parents=True)
        old_library.write_bytes(b'old standalone install product')
        configured = self.project / 'build/macos/bin/Debug/AppleExport.app'
        for app in (archived, configured):
            for relative in ('Contents/MacOS/products/shaders/requirements.json', 'Contents/MacOS/libOldPlugin.dylib'):
                path = app / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(b'old app product')
        objects = derived / 'IntermediateBuildFilesPath/keep.o'
        objects.parent.mkdir(parents=True)
        objects.write_bytes(b'compiled object')
        other = self.project / 'build/macos/bin/Release/AppleExport.app/keep'
        other.parent.mkdir(parents=True)
        other.write_bytes(b'other configuration')
        self.expected_retired_products = [archived, configured, old_library]
        result, output = self.run_distribution(args)
        self.assertEqual(result, 0)
        self.assertEqual(objects.read_bytes(), b'compiled object')
        self.assertEqual(other.read_bytes(), b'other configuration')
        actual = output / 'Application.xcarchive/Products/Applications/AppleExport.app'
        self.assertTrue((actual / 'Contents/Resources/products/bundles.json').is_file())
        self.assertFalse((actual / 'Contents/MacOS/products').exists())

    def test_failures_and_adhoc_signatures_retire_old_and_partial_deliverables(self):
        for failure, adhoc in (('archive', False), ('missing-app', False), ('generic-archive', False),
                               ('wrong-properties', False), ('codesign', False),
                               ('export', False), ('install', False), (None, True)):
            with self.subTest(failure=failure, adhoc=adhoc):
                args = self.distribution_args('ios', device='explicit-device-udid' if failure == 'install' else None)
                output = self.project / 'build/ios/distribution'
                old = output / 'export/old.ipa'
                old.parent.mkdir(parents=True, exist_ok=True)
                old.write_bytes(b'old success')
                (output / 'result.json').write_text('{"old":true}')
                result, output = self.run_distribution(args, fail=failure, adhoc=adhoc)
                self.assertEqual(result, 1)
                self.assertFalse((output / 'Application.xcarchive').exists())
                self.assertFalse((output / 'export').exists())
                self.assertFalse((output / 'result.json').exists())

    def test_upload_options_and_device_guessing_are_rejected_before_archive(self):
        args = self.distribution_args('ios')
        options = Path(args.export_options)
        options.write_bytes(plistlib.dumps({'destination': 'upload'}))
        result, output = self.run_distribution(args)
        self.assertEqual(result, 1)
        self.assertFalse((output / 'result.json').exists())
        self.assertEqual(self.events, [])
        args = self.distribution_args('ios', launch=True)
        result, _ = self.run_distribution(args)
        self.assertEqual(result, 1)
        self.assertEqual(self.events, [])

    def test_archive_preparation_uses_post_build_staging_and_clears_unsigned_cache(self):
        args = self.distribution_args('ios')
        resolved = PACKAGE._resolve(args.project, args.preset)
        old = self.project / 'build/ios/keep-unsigned.ipa'
        old.parent.mkdir(parents=True)
        old.write_bytes(b'other workflow')
        with mock.patch.object(PACKAGE, 'run_cook', return_value=True), \
                mock.patch.object(PACKAGE, 'prepare_shader_products', return_value=True), \
                mock.patch.object(PACKAGE, 'run_cmake_preset') as native:
            self.assertIsNone(PACKAGE._prepare(PACKAGE.REPO_ROOT, *resolved, build=False, archive_resources=True))
        arguments = native.call_args.kwargs['extra']
        self.assertIn('-DSKY_APPLE_ARCHIVE_RESOURCES=ON', arguments)
        self.assertIn('-DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=YES', arguments)
        self.assertEqual(arguments[-1], '-DSKY_DEPLOY_RUNTIME_RESOURCES=OFF')
        self.assertEqual(old.read_bytes(), b'other workflow')

    def test_archive_staging_rejects_foreign_destinations_and_in_bundle_receipts(self):
        self.preset('ios', variant='debug')
        target = self.project / 'archive-products'
        app = target / 'AppleExport.app'
        app.mkdir(parents=True)
        args = SimpleNamespace(engine=None, project=str(self.project), distribution='Selected', destination=None,
                               xcode_environment=True, receipt=str(app / 'receipt.json'), development=False)
        environment = {'TARGET_BUILD_DIR': str(target), 'UNLOCALIZED_RESOURCES_FOLDER_PATH': 'AppleExport.app', 'CONFIGURATION': 'Debug'}
        for override in ({}, {'CONFIGURATION': 'Release'}, {'UNLOCALIZED_RESOURCES_FOLDER_PATH': '../Outside.app'},
                         {'UNLOCALIZED_RESOURCES_FOLDER_PATH': 'unowned-directory'}):
            with self.subTest(environment=override), mock.patch.dict(os.environ, environment | override), \
                    mock.patch.object(PACKAGE, 'stage_runtime_resources') as stage:
                self.assertEqual(PACKAGE.cmd_stage_runtime(args), 1)
                stage.assert_not_called()

    def test_archive_post_build_command_is_attached_by_the_launcher_directory(self):
        if not shutil.which('cmake') or not shutil.which('ninja'):
            self.skipTest('CMake and Ninja are required for the target-directory contract')
        native = (self.project / 'native/CMakeLists.txt').read_text()
        function = re.search(r'function\(sky_stage_apple_archive target\).*?endfunction\(\)', native, re.DOTALL)
        self.assertIsNotNone(function)
        fixture = self.project / 'build/archive-target-owner'
        fixture.mkdir(parents=True)
        dummy = fixture / 'main.c'
        dummy.write_text('int main(void) { return 0; }\n')
        (fixture / 'CMakeLists.txt').write_text(
            'cmake_minimum_required(VERSION 3.20)\nproject(ArchiveOwner C)\n'
            'set(APPLE TRUE)\nset(SKY_MACOS_BUNDLE ON)\nset(SKY_APPLE_ARCHIVE_RESOURCES ON)\n'
            'set(SKY_DEPLOY_RUNTIME_RESOURCES OFF)\nset(SKY_SHADER_DISTRIBUTION Selected)\n'
            'set(ENGINE_ROOT "' + PACKAGE.REPO_ROOT.as_posix() + '")\n'
            'set(_sky_project_native_directory "' + (self.project / 'native').as_posix() + '")\n'
            'set(SKY_APPLE_TEAM_ID "")\nset(SKY_APPLE_SIGNING_IDENTITY "")\n'
            'set(SKY_APPLE_PROVISIONING_PROFILE "")\n'
            'function(sky_add_exe)\nadd_executable(Launcher "' + dummy.as_posix() + '")\nendfunction()\n'
            + function.group() + '\n'
            'add_subdirectory("' + (self.project / 'native/macos').as_posix() + '" launcher)\n')
        result = subprocess.run(['cmake', '-S', str(fixture), '-B', str(fixture / 'out'), '-G', 'Ninja'],
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn('--xcode-environment', (fixture / 'out/build.ninja').read_text())


if __name__ == '__main__':
    unittest.main()
