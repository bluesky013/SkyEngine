import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from shader_project import append_program, project_requirements, project_modules


class ShaderProjectTest(unittest.TestCase):
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
        self.assertEqual(append_program(program, ['vulkan', 'metal'], {'toon/Main.slang': b''}, entries), '')
        self.assertEqual(len(entries), 4)
        self.assertEqual(append_program(program, ['vulkan'], {'toon/Main.slang': b''}, entries), '')
        self.assertEqual(len(entries), 4)
        self.assertEqual({entry['variant']['MODE'] for entry in entries.values()}, {0, 1})

    def test_invalid_program_and_variant_fail_without_a_default_shader(self):
        entries = {}
        self.assertIn('Missing shader', append_program({'path': ''}, ['vulkan'], {}, entries))
        program = {'path': 'toon/Main.slang', 'entries': [{'stage': 'compute', 'name': 'Main'}], 'variants': [{'MODE': -1}]}
        self.assertIn('Invalid shader variant', append_program(program, ['vulkan'], {'toon/Main.slang': b''}, entries))
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
            usage, error = project_requirements(project, [config], ['vulkan'], ['common'], {'toon/Custom.slang': b''})
            self.assertEqual(error, '')
            self.assertEqual({entry['entry'] for entry in usage['entries']}, {'CustomVertex', 'CustomFragment'})
            self.assertEqual(len(usage['entries']), 2)


if __name__ == '__main__':
    unittest.main()
