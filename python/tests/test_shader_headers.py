import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import unittest


REPO = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location('shader_headers', REPO / 'python/shader_headers.py')
HEADERS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(HEADERS)
SOURCE = '''#pragma once
#include <aurora/shader/SharedShader.h>
#define FIELDS(F, A, T) F(T, MAT4, transform) A(T, FLOAT4, colors, 2)
#define RESOURCES(R) R(UNIFORM, Params, gPass, 1, 0, FIELDS) R(TEXTURE, TEXTURE2D, FLOAT4, gImage, 1, 1)
AURORA_SHADER_PASS(test::toon, ToonPass, RESOURCES)
#undef RESOURCES
#undef FIELDS
'''


def module(root, source=SOURCE):
    (root / 'Pass.h').write_text(source, encoding='utf-8')
    config = root / 'plugin.json'
    config.write_text(json.dumps({'resources': {'owner': 'toon', 'roots': [
        {'kind': 'shader', 'path': 'assets/shaders', 'exports': [
            {'source': 'Pass.h', 'include': 'toon/pipeline/Pass.h'}]}]}}), encoding='utf-8')
    return config, root / 'assets/shaders/toon/pipeline/Pass.slangh'


class ShaderHeaderExportTest(unittest.TestCase):
    def test_real_aurora_declarations_export_without_cpp_or_slang(self):
        exports, error = HEADERS.prepare_exports(REPO / 'engine/aurora/resources.json')
        self.assertEqual(error, '')
        self.assertEqual(len(exports), 11)
        lighting = next(text for _, name, text in exports if name.endswith('/SceneLightingPass.h'))
        self.assertIn('float4 positions[8];', lighting)
        self.assertIn('[[vk::binding(2, 1)]] Texture2D<float> gShadowImage', lighting)
        self.assertNotIn('AURORA_SHADER_PASS', lighting)
        self.assertNotIn('#include', lighting)

    def test_persistent_update_check_and_noop_timestamp(self):
        with tempfile.TemporaryDirectory() as directory:
            config, output = module(Path(directory))
            self.assertIn('Missing or stale', HEADERS.export_headers([config], True))
            self.assertFalse(output.exists())
            self.assertEqual(HEADERS.export_headers([config], False), '')
            first = output.read_bytes()
            timestamp = output.stat().st_mtime_ns
            self.assertEqual(HEADERS.export_headers([config], True), '')
            self.assertEqual(HEADERS.export_headers([config], False), '')
            self.assertEqual(output.stat().st_mtime_ns, timestamp)
            self.assertEqual(output.read_bytes(), first)
            self.assertNotIn(directory, output.read_text())
            (config.parent / 'Pass.h').write_text(SOURCE.replace('colors, 2', 'colors, 3'), encoding='utf-8')
            self.assertIn('stale', HEADERS.export_headers([config], True))
            self.assertEqual(output.read_bytes(), first)
            self.assertEqual(HEADERS.export_headers([config], False), '')
            self.assertIn('colors[3]', output.read_text())

    def test_handwritten_collision_does_not_overwrite(self):
        with tempfile.TemporaryDirectory() as directory:
            config, output = module(Path(directory))
            output.parent.mkdir(parents=True)
            output.write_text('// Handwritten shader helper\n')
            self.assertIn('collides', HEADERS.export_headers([config], False))
            self.assertEqual(output.read_text(), '// Handwritten shader helper\n')

    def test_manually_changed_generated_content_fails_check(self):
        with tempfile.TemporaryDirectory() as directory:
            config, output = module(Path(directory))
            self.assertEqual(HEADERS.export_headers([config], False), '')
            output.write_text(output.read_text().replace('colors[2]', 'colors[99]'))
            self.assertIn('stale', HEADERS.export_headers([config], True))

    def test_case_collision_is_rejected_before_writing(self):
        with tempfile.TemporaryDirectory() as directory:
            config, output = module(Path(directory))
            data = json.loads(config.read_text())
            data['resources']['roots'][0]['exports'].append(
                {'source': 'Pass.h', 'include': 'toon/pipeline/pass.h'})
            config.write_text(json.dumps(data))
            self.assertIn('case-colliding', HEADERS.export_headers([config], False))
            self.assertFalse(output.exists())

    def test_unsupported_syntax_and_binding_errors_are_diagnostic(self):
        cases = [SOURCE + 'struct Unexpected {};\n',
                 SOURCE.replace('MAT4, transform', 'BOOL, transform'),
                 SOURCE.replace('FLOAT4, colors, 2', 'FLOAT2, colors, 2'),
                 SOURCE.replace('gImage, 1, 1', 'gImage, 1, 0'),
                 SOURCE.replace('#pragma once', '#if SOMETHING\n#pragma once')]
        for source in cases:
            with self.subTest(source=source):
                text, error = HEADERS.render_declaration(source, 'toon/pipeline/Pass.h')
                self.assertIsNone(text)
                self.assertTrue(error)

    def test_output_and_input_cannot_escape_module(self):
        with tempfile.TemporaryDirectory() as directory:
            config, _ = module(Path(directory))
            data = json.loads(config.read_text())
            data['resources']['roots'][0]['exports'][0]['include'] = 'toon/../escape.h'
            config.write_text(json.dumps(data))
            self.assertIn('Invalid export mapping', HEADERS.export_headers([config], False))

    def test_cmake_update_check_and_clean_preserve_source_asset(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            config, output = module(root)
            helper = (REPO / 'cmake/module_resources.cmake').as_posix()
            (root / 'CMakeLists.txt').write_text(
                'cmake_minimum_required(VERSION 3.20)\nproject(HeaderExport NONE)\n'
                f'include("{helper}")\n'
                'sky_register_module_resources(CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json")\n')
            build = root / 'build'
            subprocess.run(['cmake', '-S', str(root), '-B', str(build)], check=True, capture_output=True)
            for target in ('ShaderHeaders.Update', 'ShaderHeaders.Check', 'clean'):
                subprocess.run(['cmake', '--build', str(build), '--target', target], check=True, capture_output=True)
            self.assertTrue(output.is_file())
            self.assertEqual(HEADERS.export_headers([config], True), '')


if __name__ == '__main__':
    unittest.main()
