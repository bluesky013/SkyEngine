import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from shader_project import append_program, project_requirements, project_modules, target_environments, stage_products


class ShaderProjectTest(unittest.TestCase):
    def setUp(self):
        self.environments, error = target_environments('macos', ['vulkan', 'metal'])
        self.assertEqual(error, '')

    def test_selected_plugin_collects_resource_dependencies(self):
        with tempfile.TemporaryDirectory() as temporary:
            engine, project = Path(temporary) / 'engine', Path(temporary) / 'project'
            config = engine / 'engine/aurora/resources.json'
            config.parent.mkdir(parents=True)
            config.write_text(json.dumps({'resources': {'owner': 'aurora', 'roots': []}}), encoding='utf-8')
            for owner, dependencies in [('toon', ['shared']), ('shared', ['aurora'])]:
                config = project / 'plugins' / owner / 'plugin.json'
                config.parent.mkdir(parents=True)
                config.write_text(json.dumps({'resources': {'owner': owner, 'roots': [
                    {'kind': 'shader', 'dependencies': dependencies}]}}), encoding='utf-8')
            configs, error = project_modules(engine, project, {'plugins': [{'id': 'toon'}]})
            self.assertEqual(error, '')
            self.assertEqual(len(configs), 3)
            self.assertEqual(configs[-1].parent.name, 'shared')

    def test_variants_are_explicit_requests_and_identical_requirements_merge(self):
        entries = {}
        program = {'path': 'toon/Main.slang', 'entries': [{'stage': 'compute', 'name': 'DispatchCustom'}],
                   'variants': [{'MODE': 0}, {'MODE': 1}]}
        self.assertEqual(append_program(program, ['vulkan', 'metal'], {'toon/Main.slang': b''}, entries, self.environments), '')
        self.assertEqual(len(entries), 4)
        self.assertEqual(append_program(program, ['vulkan'], {'toon/Main.slang': b''}, entries, self.environments), '')
        self.assertEqual(len(entries), 4)
        self.assertEqual({entry['variant']['MODE']['value'] for entry in entries.values()}, {0, 1})

    def test_invalid_program_and_variant_fail_without_a_default_shader(self):
        entries = {}
        self.assertIn('Missing shader', append_program({'path': ''}, ['vulkan'], {}, entries, self.environments))
        program = {'path': 'toon/Main.slang', 'entries': [{'stage': 'compute', 'name': 'Main'}], 'variants': [{'MODE': -0x80000001}]}
        self.assertIn('Invalid shader variant', append_program(program, ['vulkan'], {'toon/Main.slang': b''}, entries, self.environments))
        self.assertEqual(entries, {})

    def test_material_requests_and_selected_distribution_are_collected(self):
        with tempfile.TemporaryDirectory() as temporary:
            project = Path(temporary)
            (project / 'assets').mkdir()
            (project / 'assets/custom.mat').write_text(json.dumps({'techniques': [
                {'tag': 'opaque', 'program': {'vertex': {'relativePath': 'toon/Custom.slang', 'entry': 'CustomVertex'},
                 'fragment': {'relativePath': 'toon/Custom.slang', 'entry': 'CustomFragment'}}}]}), encoding='utf-8')
            config = project / 'resources.json'
            config.write_text(json.dumps({'resources': {'owner': 'toon', 'roots': [{'kind': 'shader',
                'bundle': 'editor', 'programs': [{'path': 'toon/Editor.slang', 'entries': [{'stage': 'compute', 'name': 'Main'}]}]}]}}), encoding='utf-8')
            usage, error = project_requirements(project, [config], ['vulkan'], ['common'], {'toon/Custom.slang': b''}, self.environments)
            self.assertEqual(error, '')
            self.assertEqual({entry['entry'] for entry in usage['entries']}, {'CustomVertex', 'CustomFragment'})
            self.assertEqual(len(usage['entries']), 2)

    def test_typed_options_preserve_signedness_and_boolean_encoding(self):
        entries = {}
        program = {'path': 'toon/Main.slang', 'entries': [{'stage': 'compute', 'name': 'Main'}],
                   'variants': [{'SIGNED': -3, 'ON': True, 'UINT': 7}]}
        self.assertEqual(append_program(program, ['vulkan'], {'toon/Main.slang': b''}, entries, self.environments), '')
        values = next(iter(entries.values()))['variant']
        self.assertEqual(values, {'SIGNED': {'type': 1, 'value': 0xfffffffd}, 'ON': {'type': 2, 'value': 1},
                                  'UINT': {'type': 0, 'value': 7}})
        program['variants'] = [{'ON': {'type': 2, 'value': 2}}]
        self.assertIn('Invalid shader variant', append_program(program, ['vulkan'], {'toon/Main.slang': b''}, {}, self.environments))

    def test_material_options_are_projected_by_stage_and_duplicates_fail(self):
        with tempfile.TemporaryDirectory() as temporary:
            project = Path(temporary)
            (project / 'assets').mkdir()
            material = {'techniques': [{'program': {'vertex': {'relativePath': 'toon/V.slang', 'entry': 'Main'},
                         'fragment': {'relativePath': 'toon/F.slang', 'entry': 'Main'}}}],
                        'properties': [{'name': 'TEXTURED', 'value': {'index': 7, 'value': True}}]}
            path = project / 'assets/custom.mat'
            path.write_text(json.dumps(material), encoding='utf-8')
            available = {'toon/V.slang': b'', 'toon/F.slang': b'', 'toon/V.slang.json': b'{"variants":[]}',
                         'toon/F.slang.json': b'{"variants":[{"key":"TEXTURED","type":"bool"}]}'}
            usage, error = project_requirements(project, [], ['vulkan'], ['common'], available, self.environments)
            self.assertEqual(error, '')
            self.assertEqual(usage['formatVersion'], 3)
            requests = {entry['stage']: entry['variant'] for entry in usage['entries']}
            self.assertEqual(requests[1], {})
            self.assertEqual(requests[2], {'TEXTURED': {'type': 2, 'value': 1}})
            material['techniques'][0]['program']['variant'] = {'TEXTURED': False}
            path.write_text(json.dumps(material), encoding='utf-8')
            self.assertIn('duplicate', project_requirements(project, [], ['vulkan'], ['common'], available, self.environments)[1])
            material['techniques'][0]['program']['variant'] = {'TYPO': True}
            path.write_text(json.dumps(material), encoding='utf-8')
            self.assertIn('unknown', project_requirements(project, [], ['vulkan'], ['common'], available, self.environments)[1])

    def test_platform_is_required_and_target_combinations_are_checked(self):
        for platform, targets in [(None, ['metal']), ('ios', ['vulkan']), ('android', ['metal']), ('linux', ['d3d12'])]:
            environments, error = target_environments(platform, targets)
            self.assertIsNone(environments)
            self.assertTrue(error)
        macos, error = target_environments('macos', ['metal'])
        self.assertEqual(error, '')
        ios, error = target_environments('ios', ['metal'])
        self.assertEqual(error, '')
        self.assertNotEqual(macos['metal'], ios['metal'])
        self.assertEqual(set(ios['metal']), {'target', 'platform', 'codeFormat', 'profile', 'minimumVersion',
                                          'requiredCapabilities', 'matrixLayout', 'options'})

    def test_target_options_and_version_are_explicit_and_canonical(self):
        environments, error = target_environments('ios', ['metal'], {'metal': {
            'minimumVersion': 20300, 'requiredCapabilities': 9, 'matrixLayout': 1, 'options': ['b', 'a', 'a']}})
        self.assertEqual(error, '')
        self.assertEqual(environments['metal']['options'], ['a', 'b'])
        self.assertEqual(environments['metal']['minimumVersion'], 20300)
        self.assertEqual(environments['metal']['requiredCapabilities'], 9)
        for overrides in [{'metal': {'platform': 2}}, {'metal': {'matrixLayout': 2}}, {'metal': {'codeFormat': 1}}]:
            self.assertTrue(target_environments('ios', ['metal'], overrides)[1])

    def test_same_metal_program_on_two_platforms_keeps_separate_requests(self):
        program = {'path': 'toon/Main.slang', 'entries': [{'stage': 'compute', 'name': 'Main'}]}
        entries = {}
        for platform in ['macos', 'ios']:
            environments, error = target_environments(platform, ['metal'])
            self.assertEqual(error, '')
            self.assertEqual(append_program(program, ['metal'], {'toon/Main.slang': b''}, entries, environments), '')
        self.assertEqual(len(entries), 2)
        self.assertEqual({entry['environment']['platform'] for entry in entries.values()}, {2, 3})

    def test_staging_rejects_previous_or_other_platform_products(self):
        with tempfile.TemporaryDirectory() as temporary:
            project = Path(temporary)
            source = project / 'products/shaders/shipping'
            (source / 'metal').mkdir(parents=True)
            (source / 'metal/index.bin').write_bytes(b'index')
            manifest = {'targets': ['metal']}
            (source / 'distribution.json').write_text(json.dumps(manifest), encoding='utf-8')
            self.assertIn('recook', stage_products(project, 'shipping', project / 'stage', 'ios'))
            environments, error = target_environments('ios', ['metal'])
            self.assertEqual(error, '')
            manifest.update(formatVersion=2, platform='ios', environments=environments)
            (source / 'distribution.json').write_text(json.dumps(manifest), encoding='utf-8')
            self.assertIn('platform', stage_products(project, 'shipping', project / 'stage', 'macos'))
            self.assertFalse((project / 'stage').exists())
            self.assertEqual(stage_products(project, 'shipping', project / 'stage', 'ios'), '')
            self.assertEqual((project / 'stage/shaders/metal/index.bin').read_bytes(), b'index')


if __name__ == '__main__':
    unittest.main()
