"""Verify public preset generation writes platform orientation without editing native sources."""

import importlib.util
import json
from pathlib import Path
import plistlib
import subprocess
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock
import xml.etree.ElementTree as ET

SPEC = importlib.util.spec_from_file_location('orientation_package', Path(__file__).parents[1] / 'package_project.py')
PACKAGE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PACKAGE)


class ExportOrientationTest(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        with mock.patch('builtins.print'):
            PACKAGE.cmd_create(SimpleNamespace(engine=None, dir=temp.name, name='Orientation', force=False))
        self.project = Path(temp.name) / 'Orientation'

    def generate(self, platform, **overrides):
        preset = {'name': 'test', 'platform': platform, 'bundleId': 'com.skyengine.orientation', 'signing': 'off', **overrides}
        (self.project / 'export_presets.json').write_text(json.dumps([preset]), encoding='utf-8')
        with mock.patch.object(PACKAGE, 'run_cook', return_value=True), \
                mock.patch.object(PACKAGE, 'prepare_shader_products', return_value=True), \
                mock.patch.object(PACKAGE, 'stage_bundles', return_value=[]), \
                mock.patch.object(PACKAGE, 'run_cmake_preset') as configure, mock.patch('builtins.print'):
            result = PACKAGE.cmd_generate(SimpleNamespace(engine=None, project=str(self.project), preset='test'))
        self.assertEqual(result, 0)
        return configure

    def test_defaults_and_each_policy_reach_all_mobile_outputs(self):
        policies = [({}, 'landscape'), ({'orientation': 'default'}, 'landscape')]
        policies += [({'orientation': value}, value) for value in PACKAGE.ORIENTATIONS]
        android = ['landscape', 'portrait', 'fullSensor', 'sensorLandscape', 'sensorPortrait']
        ios = [
            ['UIInterfaceOrientationLandscapeLeft'], ['UIInterfaceOrientationPortrait'],
            ['UIInterfaceOrientationPortrait', 'UIInterfaceOrientationPortraitUpsideDown',
             'UIInterfaceOrientationLandscapeLeft', 'UIInterfaceOrientationLandscapeRight'],
            ['UIInterfaceOrientationLandscapeLeft', 'UIInterfaceOrientationLandscapeRight'],
            ['UIInterfaceOrientationPortrait', 'UIInterfaceOrientationPortraitUpsideDown'],
        ]
        original = {p: p.read_bytes() for p in (self.project / 'native').rglob('*') if p.is_file()}
        for platform in ('android', 'harmonyos', 'ios'):
            for override, value in policies:
                with self.subTest(platform=platform, override=override):
                    configure = self.generate(platform, **override)
                    index = PACKAGE.ORIENTATIONS.index(value)
                    if platform == 'android':
                        manifest = ET.parse(self.project / 'build/android/app/src/main/AndroidManifest.xml')
                        activity = manifest.getroot().find('application/activity')
                        self.assertEqual(activity.get('{http://schemas.android.com/apk/res/android}screenOrientation'), android[index])
                    elif platform == 'harmonyos':
                        path = self.project / 'build/harmonyos/entry/src/main/module.json5'
                        self.assertEqual(json.loads(path.read_text())['module']['abilities'][0]['orientation'], value)
                    else:
                        path = self.project / 'build/ios/export/Info.plist'
                        manifest = plistlib.loads(path.read_bytes())
                        self.assertEqual(manifest['UISupportedInterfaceOrientations'], ios[index])
                        self.assertEqual(manifest['UISupportedInterfaceOrientations~ipad'], ios[index])
                        self.assertIn('UIApplicationSceneManifest', manifest)
                        self.assertIn('-DSKY_IOS_INFO_PLIST=' + path.resolve().as_posix(), configure.call_args.kwargs['extra'])
        for path, data in original.items():
            self.assertEqual(path.read_bytes(), data, str(path))

    def test_bad_values_fail_before_cook_or_generated_changes(self):
        marker = self.project / 'build/keep.txt'
        marker.parent.mkdir()
        marker.write_text('keep')
        for value in ('', 'sideways', None, 5, False, [], {}, 'landscape\0bad'):
            with self.subTest(value=value), mock.patch.object(PACKAGE, 'prepare_harmonyos_project') as prepare:
                with self.assertRaisesRegex(SystemExit, 'Invalid orientation'):
                    self.generate('harmonyos', orientation=value)
                prepare.assert_not_called()
                self.assertEqual(marker.read_text(), 'keep')
        for platform in ('windows', 'linux', 'macos'):
            with self.subTest(platform=platform):
                self.assertEqual(PACKAGE.preset_orientation({'platform': platform}), 'landscape')
                with self.assertRaisesRegex(SystemExit, 'only supported'):
                    self.generate(platform, orientation='portrait')

    def test_harmony_json5_preserves_other_abilities_and_rejects_bad_main(self):
        source = self.project / 'native/harmonyos/entry/src/main/module.json5'
        original = """{module: { // project-owned metadata
          name: 'entry', type: 'entry', mainElement: 'EntryAbility',
          abilities: [{name:'OtherAbility',orientation:'portrait'},
                      {name:'EntryAbility',orientation:'portrait',label:'https://example.test/a//b'},],
        }}"""
        source.write_text(original, encoding='utf-8')
        self.generate('harmonyos', orientation='auto_rotation_landscape')
        output = self.project / 'build/harmonyos/entry/src/main/module.json5'
        abilities = json.loads(output.read_text())['module']['abilities']
        self.assertEqual(abilities[0]['orientation'], 'portrait')
        self.assertEqual(abilities[1]['orientation'], 'auto_rotation_landscape')
        self.assertEqual(abilities[1]['label'], 'https://example.test/a//b')
        self.assertEqual(source.read_text(), original)
        previous = output.read_bytes()
        for invalid in ('{broken', original.replace("name:'OtherAbility'", "name:'EntryAbility'")):
            source.write_text(invalid, encoding='utf-8')
            with self.assertRaises(SystemExit):
                self.generate('harmonyos')
            self.assertEqual(output.read_bytes(), previous)

    def test_ios_qualified_metadata_and_launcher_property_use_export_copy(self):
        source = self.project / 'native/ios/Info.plist'
        manifest = plistlib.loads(source.read_bytes())
        manifest['UISupportedInterfaceOrientations~iphone'] = ['UIInterfaceOrientationPortrait']
        manifest['CustomProjectSetting'] = 'keep'
        source.write_bytes(plistlib.dumps(manifest))
        self.generate('ios')
        exported = self.project / 'build/ios/export/Info.plist'
        result = plistlib.loads(exported.read_bytes())
        self.assertEqual(result['UISupportedInterfaceOrientations~iphone'], ['UIInterfaceOrientationLandscapeLeft'])
        self.assertEqual(result['CustomProjectSetting'], 'keep')
        self.assertEqual(plistlib.loads(source.read_bytes()), manifest)

        fixture = self.project / 'build/cmake-orientation'
        fixture.mkdir()
        (fixture / 'CMakeLists.txt').write_text(
            'cmake_minimum_required(VERSION 3.20)\nproject(Orientation NONE)\n'
            'function(sky_add_exe)\nadd_custom_target(Launcher)\nendfunction()\n'
            'set(SKY_APPLE_TEAM_ID "")\nset(SKY_APPLE_SIGNING_IDENTITY "")\nset(SKY_APPLE_PROVISIONING_PROFILE "")\n'
            'include("' + (self.project / 'native/ios/CMakeLists.txt').as_posix() + '")\n'
            'get_target_property(selected Launcher MACOSX_BUNDLE_INFO_PLIST)\n'
            'file(WRITE "${CMAKE_BINARY_DIR}/selected.txt" "${selected}")\n', encoding='utf-8')
        subprocess.run(['cmake', '-S', str(fixture), '-B', str(fixture / 'out'),
                        '-DSKY_IOS_INFO_PLIST=' + exported.as_posix()], check=True,
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        self.assertEqual((fixture / 'out/selected.txt').read_text(), exported.as_posix())

    def test_legacy_ios_launcher_reports_required_contract_instead_of_ignoring_orientation(self):
        launcher = self.project / 'native/ios/CMakeLists.txt'
        launcher.write_text('set_target_properties(Launcher PROPERTIES MACOSX_BUNDLE_INFO_PLIST "Info.plist")')
        with self.assertRaisesRegex(SystemExit, 'SKY_IOS_INFO_PLIST'):
            self.generate('ios')


if __name__ == '__main__':
    unittest.main()
