"""World root collection and real AssetTool product-only dependency audit."""

import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
import uuid

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import package_project as package


def identity():
    return str(uuid.uuid4()).upper()


def binary_uuid(value):
    return uuid.UUID(value).bytes


def string(value):
    data = value.encode('utf-8')
    return struct.pack('<I', len(data)) + data


def envelope(kind, dependencies=()):
    return (string(kind) + struct.pack('<I', len(dependencies))
            + b''.join(binary_uuid(value) for value in dependencies)
            + string('') + struct.pack('<IIQ', 4, 1, 0))


def write_product(bundle, value, kind, dependencies=()):
    path = bundle / value[:2] / (value + '.bin')
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(envelope(kind, dependencies))
    return path


def write_group(bundle, owner, entries):
    generation = identity()
    directory = bundle / '.groups' / owner / generation
    directory.mkdir(parents=True)
    owners = bundle / '.groups/owners'
    owners.mkdir(exist_ok=True)
    manifest = struct.pack('<II', 0x47505244, 1) + binary_uuid(owner) + binary_uuid(generation) + struct.pack('<QI', 1, len(entries))
    for value, kind, dependencies in entries:
        data = envelope(kind, dependencies)
        (directory / (value + '.bin')).write_bytes(data)
        (owners / (value + '.bin')).write_bytes(binary_uuid(owner) + struct.pack('<I', 0))
        checksum = 14695981039346656037
        for byte in data:
            checksum = ((checksum ^ byte) * 1099511628211) & ((1 << 64) - 1)
        manifest += binary_uuid(value) + struct.pack('<QQ', len(data), checksum)
    (directory.parent / 'current.bin').write_bytes(manifest)
    return generation


class AnimationPackageTest(unittest.TestCase):
    def test_build_option_manifest_records_explicit_disabled_feature(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / 'plugins').mkdir()
            (root / 'plugins/plugins.json').write_text(json.dumps({'plugins': [
                {'name': 'Smoke', 'cmake_var': 'SKY_BUILD_SMOKE', 'enabled': True}
            ]}), encoding='utf-8')
            helper = package.REPO_ROOT / 'cmake/plugin_switches.cmake'
            (root / 'CMakeLists.txt').write_text(
                'cmake_minimum_required(VERSION 3.20)\nproject(Smoke NONE)\n'
                'set(ENGINE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")\n'
                'include("' + helper.as_posix() + '")\n', encoding='utf-8')
            result = subprocess.run(['cmake', '-S', str(root), '-B', str(root / 'build'), '-DSKY_BUILD_SMOKE=OFF'], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            header = (root / 'build/generated/framework/BuildPluginOptions.h').read_text()
            self.assertIn('{"SKY_BUILD_SMOKE", false}', header)

    def test_explicit_skin_roots_and_legacy_world_resave_diagnostic(self):
        with tempfile.TemporaryDirectory() as temp:
            project = Path(temp)
            (project / 'assets').mkdir()
            references = [{'uuid': identity(), 'type': kind} for kind in ('AuroraMesh', 'AuroraSkin', 'AuroraSkeleton', 'AnimationGraph')]
            world = project / 'assets/character.world'
            world.write_text(json.dumps({'actors': [], 'assetRoots': references}), encoding='utf-8')
            roots, error = package.collect_world_asset_roots(project, {})
            self.assertIsNone(error)
            self.assertEqual([value['uuid'] for value in roots], [value['uuid'] for value in references])
            self.assertTrue(all(value['world'] == 'character.world' for value in roots))
            world.write_text('{"actors": []}', encoding='utf-8')
            self.assertIn('Re-save World', package.collect_world_asset_roots(project, {})[1])
            self.assertIsNotNone(package.collect_world_asset_roots(project, {'worlds': ['../outside.world']})[1])

    def test_collision_only_mesh_is_checked_by_real_package_audit(self):
        tool = package.find_asset_tool(package.REPO_ROOT)
        if tool is None:
            self.skipTest('Build AssetTool for product-only audit')
        with tempfile.TemporaryDirectory() as temp:
            project = Path(temp)
            (project / 'assets').mkdir()
            mesh = identity()
            (project / 'assets/collider.world').write_text(json.dumps({
                'actors': [], 'assetRoots': [{'uuid': mesh, 'type': 'AuroraMesh'}]
            }), encoding='utf-8')
            product = write_product(project / 'products/common', mesh, 'AuroraMesh')
            self.assertEqual(package.stage_bundles(project, {}, project / 'package/products'), ['common'])
            product.unlink()
            self.assertIsNone(package.stage_bundles(project, {}, project / 'package/products'))

    def test_real_tool_resolves_graph_material_and_skin_group_without_sources(self):
        tool = Path(os.environ['SKY_ASSET_TOOL']) if os.environ.get('SKY_ASSET_TOOL') else package.find_asset_tool(package.REPO_ROOT)
        if tool is None:
            self.skipTest('Build AssetTool for product-only audit')
        with tempfile.TemporaryDirectory() as temp:
            products = Path(temp) / 'products'
            bundle = products / 'common'
            mesh, skin, rig, graph, clip, material, texture = [identity() for _ in range(7)]
            generation = write_group(bundle, mesh, [(mesh, 'AuroraMesh', [material]), (skin, 'AuroraSkin', []), (rig, 'AuroraSkeleton', [])])
            write_product(bundle, graph, 'AnimationGraph', [rig, clip])
            write_product(bundle, clip, 'AnimationClip')
            write_product(bundle, material, 'AuroraMaterial', [texture])
            texture_path = write_product(bundle, texture, 'AuroraImage')
            roots = [{'uuid': value, 'type': kind} for value, kind in ((mesh, 'AuroraMesh'), (skin, 'AuroraSkin'), (rig, 'AuroraSkeleton'), (graph, 'AnimationGraph'))]
            manifest = products / 'asset-roots.json'
            manifest.write_text(json.dumps({'bundles': ['common'], 'roots': roots}), encoding='utf-8')

            def run():
                return subprocess.run([str(tool.resolve()), '--audit-products', str(manifest)], cwd=tool.parent, capture_output=True, text=True, timeout=30)

            result = run()
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            report = json.loads((products / 'asset-closure.json').read_text())
            self.assertEqual({value['uuid'] for value in report['products']}, {mesh, skin, rig, graph, clip, material, texture})
            self.assertEqual(report['groups'], [{'bundle': 'common', 'owner': mesh, 'generation': generation, 'revision': 1}])
            # One literal-only LZ4 block exercises the actual deployed codec module.
            compressed = struct.pack('<II', 4, 5) + b'\x40\x01\x00\x00\x00'
            texture_path.write_bytes(string('AuroraImage') + struct.pack('<I', 0) + string('lz4')
                                     + struct.pack('<I', len(compressed)) + compressed + struct.pack('<Q', 0))
            result = run()
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            texture_path.write_bytes(texture_path.read_bytes()[:-1])
            self.assertNotEqual(run().returncode, 0)
            texture_path.unlink()
            result = run()
            self.assertNotEqual(result.returncode, 0)
            self.assertIn(texture, result.stderr)
            write_product(bundle, texture, 'AuroraImage')
            roots[1]['type'] = 'AuroraSkeleton'
            manifest.write_text(json.dumps({'bundles': ['common'], 'roots': roots}), encoding='utf-8')
            result = run()
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('type mismatch', result.stderr)
            roots[1]['type'] = 'AuroraSkin'
            manifest.write_text(json.dumps({'bundles': ['common'], 'roots': roots}), encoding='utf-8')
            (bundle / '.groups' / mesh / generation / (skin + '.bin')).write_bytes(b'corrupt')
            result = run()
            self.assertNotEqual(result.returncode, 0)
            self.assertIn(skin, result.stderr)
            self.assertFalse((Path(temp) / 'assets').exists())


if __name__ == '__main__':
    unittest.main()
