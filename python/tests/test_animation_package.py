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

if __package__:
    from .asset_product_test_helpers import checksum, product_body, product_bytes
else:
    from asset_product_test_helpers import checksum, product_body, product_bytes

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
    path.write_bytes(product_bytes(value, envelope(kind, dependencies)))
    return path


class AnimationPackageTest(unittest.TestCase):
    def test_flat_fixture_has_a_versioned_identity_and_checked_body(self):
        self.assertEqual(checksum(b'hello'), 0xA430D84680AABD0B)
        value = identity()
        body = envelope('AuroraImage')
        encoded = product_bytes(value, body)
        self.assertEqual(len(encoded) - len(body), 40)
        self.assertEqual(encoded[:8], b'SPRD\x01\x00\x00\x00')
        self.assertEqual(encoded[8:24], binary_uuid(value))
        self.assertEqual(struct.unpack_from('<QQ', encoded, 24), (len(body), checksum(body)))
        self.assertEqual(product_body(encoded, value), body)
        for offset in (4, 8, 24, 32, len(encoded) - 1):
            corrupt = bytearray(encoded)
            corrupt[offset] ^= 1
            with self.subTest(offset=offset), self.assertRaises(ValueError):
                product_body(corrupt, value)

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
            mesh, world = identity(), identity()
            (project / 'assets/collider.world').write_text(json.dumps({
                'actors': [], 'assetRoots': [{'uuid': mesh, 'type': 'AuroraMesh'}]
            }), encoding='utf-8')
            bundle = project / 'products/common'
            product = write_product(bundle, mesh, 'AuroraMesh')
            write_product(bundle, world, 'World', [mesh])
            (bundle / 'product.index').write_text(json.dumps({'path': 'collider.world', 'id': world}) + '\n', encoding='utf-8')
            self.assertEqual(package.stage_bundles(project, {}, project / 'package/products'), ['common'])
            product.unlink()
            self.assertIsNone(package.stage_bundles(project, {}, project / 'package/products'))

    def test_real_tool_resolves_flat_dependency_closure_without_sources(self):
        tool = Path(os.environ['SKY_ASSET_TOOL']) if os.environ.get('SKY_ASSET_TOOL') else package.find_asset_tool(package.REPO_ROOT)
        if tool is None:
            self.skipTest('Build AssetTool for product-only audit')
        with tempfile.TemporaryDirectory() as temp:
            products = Path(temp) / 'products'
            bundle = products / 'common'
            mesh, skin, rig, graph, clip, material, texture = [identity() for _ in range(7)]
            write_product(bundle, mesh, 'AuroraMesh', [skin, material])
            skin_path = write_product(bundle, skin, 'AuroraSkin', [rig])
            write_product(bundle, rig, 'AuroraSkeleton')
            write_product(bundle, graph, 'AnimationGraph', [rig, clip])
            write_product(bundle, clip, 'AnimationClip')
            write_product(bundle, material, 'AuroraMaterial', [texture])
            texture_path = write_product(bundle, texture, 'AuroraImage')
            unrelated = write_product(bundle, identity(), 'AuroraImage')
            unrelated.write_bytes(b'not part of the selected dependency closure')
            roots = [{'uuid': value, 'type': kind} for value, kind in ((mesh, 'AuroraMesh'), (skin, 'AuroraSkin'), (rig, 'AuroraSkeleton'), (graph, 'AnimationGraph'))]
            manifest = products / 'asset-roots.json'
            manifest.write_text(json.dumps({'bundles': ['common'], 'roots': roots}), encoding='utf-8')

            def run():
                return subprocess.run([str(tool.resolve()), '--audit-products', str(manifest)], cwd=tool.parent, capture_output=True, text=True, timeout=30)

            result = run()
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            report = json.loads((products / 'asset-closure.json').read_text())
            self.assertEqual({value['uuid'] for value in report['products']}, {mesh, skin, rig, graph, clip, material, texture})
            self.assertNotIn('groups', report)
            for item in report['products']:
                path = bundle / item['uuid'][:2] / (item['uuid'] + '.bin')
                self.assertEqual(item['formatVersion'], 1)
                self.assertEqual(item['checksum'], checksum(product_body(path.read_bytes(), item['uuid'])))
                self.assertEqual(item['bulkSize'], 0)
            # One literal-only LZ4 block exercises the actual deployed codec module.
            compressed = struct.pack('<II', 4, 5) + b'\x40\x01\x00\x00\x00'
            texture_path.write_bytes(product_bytes(texture, string('AuroraImage') + struct.pack('<I', 0) + string('lz4')
                                                  + struct.pack('<I', len(compressed)) + compressed + struct.pack('<Q', 0)))
            result = run()
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            valid_texture = texture_path.read_bytes()
            for name, data in (
                    ('unsupported-version', valid_texture[:4] + struct.pack('<I', 2) + valid_texture[8:]),
                    ('bad-checksum', valid_texture[:32] + bytes([valid_texture[32] ^ 1]) + valid_texture[33:]),
                    ('bad-body', valid_texture[:-1] + bytes([valid_texture[-1] ^ 1])),
                    ('truncated', valid_texture[:-1])):
                with self.subTest(name=name):
                    texture_path.write_bytes(data)
                    self.assertNotEqual(run().returncode, 0)
            texture_path.write_bytes(valid_texture)
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
            skin_path.write_bytes(b'corrupt')
            result = run()
            self.assertNotEqual(result.returncode, 0)
            self.assertIn(skin, result.stderr)
            self.assertFalse((Path(temp) / 'assets').exists())


if __name__ == '__main__':
    unittest.main()
