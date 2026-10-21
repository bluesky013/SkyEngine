import json
from pathlib import Path
import sys
import subprocess
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from module_resources import MANIFEST, collect, stage


class ModuleResourcesTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)

    def module(self, owner, files, dependencies=None, includes=None):
        module = self.root / owner
        root = module / 'assets/shaders'
        for path, content in files.items():
            target = root / path
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text(content, encoding='utf-8', newline='\n')
        config = module / 'plugin.json'
        config.write_text(json.dumps({'resources': {'owner': owner, 'roots': [{
            'kind': 'shader', 'path': 'assets/shaders', 'dependencies': dependencies or [],
            'includeRoots': includes or [], 'exports': []}]}}), encoding='utf-8')
        return config

    def test_staged_paths_and_manifest_are_order_independent(self):
        a = self.module('first', {'first/pipeline/ToonPass.slang': 'first\r\n'})
        b = self.module('second', {'second/pipeline/ToonPass.slang': 'second'})
        destination = self.root / 'relocated'
        self.assertEqual(stage([b, a], destination), '')
        before = (destination / MANIFEST).read_bytes()
        stamp = (destination / MANIFEST).stat().st_mtime_ns
        self.assertEqual(stage([a, b], destination), '')
        self.assertEqual((destination / MANIFEST).read_bytes(), before)
        self.assertEqual((destination / MANIFEST).stat().st_mtime_ns, stamp)
        self.assertEqual((destination / 'first/pipeline/ToonPass.slang').read_bytes(), b'first\n')
        self.assertNotIn(str(self.root), before.decode())

    def test_validation_failure_does_not_publish_partial_stage(self):
        a = self.module('a', {'a/Main.slang': 'a'})
        b = self.module('b', {'b/Main.slang': 'b'}, ['missing'])
        destination = self.root / 'stage'
        self.assertEqual(stage([a], destination), '')
        before = (destination / MANIFEST).read_bytes()
        self.assertIn('dependency', stage([a, b], destination))
        self.assertEqual((destination / MANIFEST).read_bytes(), before)
        self.assertFalse((destination / 'b').exists())

    def test_retired_owner_does_not_leave_stale_assets(self):
        a = self.module('a', {'a/Main.slang': 'a'})
        b = self.module('b', {'b/Main.slang': 'b'})
        destination = self.root / 'stage'
        self.assertEqual(stage([a, b], destination), '')
        (destination / 'unrelated.txt').write_text('preserve')
        self.assertEqual(stage([a], destination), '')
        self.assertFalse((destination / 'b/Main.slang').exists())
        self.assertTrue((destination / 'unrelated.txt').is_file())

    def test_duplicate_owner_and_case_collision_are_rejected(self):
        a = self.module('a', {'a/Main.slang': 'a'})
        self.assertIn('Duplicate resource owner', collect([a, a])[2])
        b = self.module('b', {'B/Main.slang': 'b'})
        self.assertIn('out-of-owner', collect([b])[2])

    def test_dependency_scope_rejects_ambiguous_custom_includes(self):
        a = self.module('a', {'a/include/Value.slangh': 'a'}, includes=['a/include'])
        b = self.module('b', {'b/include/Value.slangh': 'b'}, includes=['b/include'])
        self.assertEqual(collect([a, b])[2], '')
        c = self.module('c', {'c/Main.slang': 'c'}, ['a', 'b'])
        self.assertIn('Ambiguous shader include', collect([a, b, c])[2])

    def test_cycles_and_out_of_owner_include_roots_are_rejected(self):
        a = self.module('a', {'a/Main.slang': 'a'}, ['b'])
        b = self.module('b', {'b/Main.slang': 'b'}, ['a'])
        self.assertIn('cyclic', collect([a, b])[2])
        c = self.module('c', {'c/Main.slang': 'c'}, includes=['a'])
        self.assertIn('Include root', collect([c])[2])

    def test_cpp_headers_are_not_shader_assets(self):
        a = self.module('a', {'a/Pass.h': 'cpp'})
        self.assertIn('C++ headers', collect([a])[2])

    def test_real_module_inventory_contains_persisted_exports_and_materials(self):
        repository = Path(__file__).resolve().parents[2]
        manifest, sources, error = collect([repository / 'engine/aurora/resources.json'])
        self.assertEqual(error, '')
        self.assertEqual(len(manifest['modules']), 1)
        self.assertIn('aurora/pipeline/OpaquePass.slangh', sources)
        self.assertIn('aurora/materials/Unlit.slang', sources)
        self.assertIn('aurora/materials/Unlit.slang.json', sources)

    def test_editor_asset_deploy_preserves_module_shader_inventory(self):
        config = self.module('toon', {'toon/main.slang': 'void main() {}'})
        manifest, sources, error = collect([config])
        self.assertEqual(error, '')
        destination = self.root / 'deployed/assets'
        self.assertEqual(stage([config], destination / 'shaders'), '')
        original = (destination / 'shaders' / MANIFEST).read_bytes()
        source = self.root / 'engine-assets'
        (source / 'shaders').mkdir(parents=True)
        (source / 'shaders/obsolete.slang').write_text('obsolete')
        (source / 'mesh.bin').write_bytes(b'mesh')
        (destination / 'stale.bin').write_bytes(b'stale')
        script = Path(__file__).resolve().parents[2] / 'engine/sandbox/app/deploy_resources.cmake'
        subprocess.run(['cmake', f'-DSRC_DIR={source}', f'-DDST_DIR={destination}',
                        '-DEXCLUDE_CHILDREN=shaders', '-P', str(script)], check=True,
                       capture_output=True, text=True)
        self.assertEqual((destination / 'shaders' / MANIFEST).read_bytes(), original)
        self.assertEqual((destination / 'mesh.bin').read_bytes(), b'mesh')
        self.assertFalse((destination / 'stale.bin').exists())
        self.assertFalse((destination / 'shaders/obsolete.slang').exists())


if __name__ == '__main__':
    unittest.main()
