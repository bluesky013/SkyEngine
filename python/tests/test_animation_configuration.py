"""Configure the real animation targets against lightweight engine dependencies."""

import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
MODULES = ('Animation.GraphAssets', 'Animation.World', 'Animation.Builder', 'Animation.Editor')


@unittest.skipUnless(shutil.which('cmake'), 'CMake is required')
class AnimationConfigurationTest(unittest.TestCase):
    def run_cmake(self, root, *options):
        result = subprocess.run(
            ['cmake', '-S', str(root), '-B', str(root / 'build'), *options],
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=120)
        self.assertEqual(result.returncode, 0, result.stdout)

    def write(self, root, path, text):
        target = root / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(text, encoding='utf-8')

    def test_engine_option_is_known_and_survives_cache_and_manifest_changes(self):
        with tempfile.TemporaryDirectory(prefix='sky-animation-options-') as temporary:
            root = Path(temporary)
            self.write(root, 'plugins/plugins.json', json.dumps({'plugins': []}))
            self.write(root, 'CMakeLists.txt', '''cmake_minimum_required(VERSION 3.20)
project(AnimationOptions NONE)
set(ENGINE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(3RD_PATH "${CMAKE_CURRENT_SOURCE_DIR}/unused")
include("@ROOT@/cmake/options.cmake")
include("@ROOT@/cmake/plugin_switches.cmake")
'''.replace('@ROOT@', ROOT.as_posix()))
            header = root / 'build/generated/framework/BuildOptions.h'
            self.run_cmake(root)
            self.assertIn('{"SKY_BUILD_ANIMATION", true}', header.read_text())
            self.run_cmake(root, '-DSKY_BUILD_ANIMATION=OFF')
            self.assertIn('{"SKY_BUILD_ANIMATION", false}', header.read_text())
            self.write(root, 'plugins/plugins.json', json.dumps({'plugins': [
                {'name': 'Smoke', 'cmake_var': 'SKY_BUILD_SMOKE', 'enabled': True}
            ]}))
            self.run_cmake(root)
            self.assertIn('{"SKY_BUILD_ANIMATION", false}', header.read_text())
            self.assertIn('{"SKY_BUILD_SMOKE", true}', header.read_text())
            self.assertEqual(header.read_text().count('"SKY_BUILD_ANIMATION"'), 1)
            self.run_cmake(root, '-DSKY_BUILD_ANIMATION=ON')
            self.assertIn('{"SKY_BUILD_ANIMATION", true}', header.read_text())

    def fixture(self, root):
        self.write(root, 'engine/CMakeLists.txt', (ROOT / 'engine/CMakeLists.txt').read_text())
        shutil.copytree(ROOT / 'engine/animation', root / 'engine/animation')
        stubs = {
            'core': 'add_library(Core INTERFACE)\n',
            'framework': 'add_library(Framework INTERFACE)\n',
            'content': 'add_custom_target(Content.Module)\nadd_custom_target(Content.Metadata)\n'
                       'add_custom_target(Content.Builder.Module)\n',
            'content/editor': 'add_custom_target(Content.Views)\n',
            'aurora': 'add_library(Aurora.Renderer.Interface INTERFACE)\n'
                      'add_custom_target(Aurora.Render)\nadd_custom_target(Aurora.Metadata)\n'
                      'add_custom_target(Aurora.Cook)\n',
            'aurora/editor': 'add_custom_target(Aurora.Render.Editor)\n',
            'sandbox': 'add_library(EditorCore INTERFACE)\nadd_library(EditorUi INTERFACE)\n'
                       'add_library(EditorWidgets INTERFACE)\nadd_custom_target(SandboxEditor)\n',
        }
        for directory in ('audio', 'ui', 'physics', 'network', 'navigation', 'terrain', 'vegetation'):
            stubs[directory] = ''
        for directory, body in stubs.items():
            self.write(root, 'engine/' + directory + '/CMakeLists.txt', body)
        self.write(root, 'CMakeLists.txt', '''cmake_minimum_required(VERSION 3.20)
project(AnimationConfiguration LANGUAGES CXX)
set(ENGINE_ROOT "@ROOT@")
set(3RD_PATH "${CMAKE_CURRENT_SOURCE_DIR}/unused")
include("${ENGINE_ROOT}/cmake/options.cmake")
include("${ENGINE_ROOT}/cmake/functions.cmake")
add_library(3rdParty::acl INTERFACE IMPORTED)
add_library(3rdParty::googletest INTERFACE IMPORTED)
add_subdirectory(engine)
file(WRITE "${CMAKE_BINARY_DIR}/animation-targets.txt" "")
foreach(target Animation AnimationTest Animation.Graph.Static Animation.GraphTest
        Animation.GraphAssets.Static Animation.GraphAssets Animation.World.Static
        Animation.World Animation.WorldTest Animation.ModuleTest Animation.Builder.Static
        Animation.Builder Animation.BuilderTest Animation.Editor.Static Animation.Editor
        Animation.EditorTest)
    if(TARGET ${target})
        get_target_property(kind ${target} TYPE)
        get_target_property(source ${target} SOURCE_DIR)
        get_target_property(links ${target} LINK_LIBRARIES)
        get_target_property(definitions ${target} COMPILE_DEFINITIONS)
        file(APPEND "${CMAKE_BINARY_DIR}/animation-targets.txt"
            "${target}|${kind}|${source}|${links}|${definitions}\\n")
    endif()
endforeach()
get_property(modules GLOBAL PROPERTY SKY_STATIC_MODULE_TARGETS)
file(WRITE "${CMAKE_BINARY_DIR}/static-modules.txt" "${modules}")
'''.replace('@ROOT@', ROOT.as_posix()))

    def test_target_boundaries_and_packaging_matrix(self):
        cases = (
            ('shipping', (), True, False, False, False, False),
            ('tool', ('-DSKY_BUILD_TOOL=ON',), True, True, False, False, False),
            ('tests', ('-DSKY_BUILD_TEST=ON',), True, False, False, True, False),
            ('editor', ('-DSKY_BUILD_SANDBOX=ON', '-DSKY_BUILD_TEST=ON'), True, True, True, True, False),
            ('static', ('-DSKY_BUILD_SANDBOX=ON', '-DSKY_MODULE_PACKAGING=static'), True, True, True, False, True),
            ('disabled', ('-DSKY_BUILD_SANDBOX=ON', '-DSKY_BUILD_TEST=ON', '-DSKY_BUILD_ANIMATION=OFF'),
             False, False, False, True, False),
        )
        for name, options, enabled, builder, editor, tests, static in cases:
            with self.subTest(case=name), tempfile.TemporaryDirectory(prefix='sky-animation-config-') as temporary:
                root = Path(temporary)
                self.fixture(root)
                self.run_cmake(root, *options)
                targets = dict((row[0], row[1:]) for row in (
                    line.split('|') for line in (root / 'build/animation-targets.txt').read_text().splitlines()))
                self.assertIn('Animation', targets)
                self.assertEqual(targets['Animation'][2].split(';'), ['Core', '3rdParty::acl'])
                self.assertTrue(targets['Animation'][1].endswith('/engine/animation/core'))
                self.assertNotIn('Animation.Graph.Static', targets)
                self.assertNotIn('Animation.GraphTest', targets)
                self.assertEqual('AnimationTest' in targets, tests)
                if enabled:
                    self.assertEqual(targets['Animation.GraphAssets.Static'][2].split(';'),
                                     ['Animation', 'Framework'])
                    self.assertTrue(targets['Animation.GraphAssets.Static'][1].endswith('/engine/animation/assets'))
                    self.assertEqual(targets['Animation.World.Static'][2].split(';'),
                                     ['Animation.GraphAssets.Static', 'Framework', 'Aurora.Renderer.Interface'])
                if builder:
                    self.assertEqual(targets['Animation.Builder.Static'][2].split(';'),
                                     ['Animation.GraphAssets.Static', 'Framework'])
                if editor:
                    self.assertEqual(targets['Animation.Editor.Static'][2].split(';'),
                                     ['Animation.GraphAssets.Static', 'EditorCore', 'EditorUi',
                                      'EditorWidgets', 'Aurora.Renderer.Interface'])
                for module, present in zip(MODULES, (enabled, enabled, builder, editor)):
                    self.assertEqual(module in targets, present)
                    self.assertEqual(module + '.Static' in targets, present)
                    if present:
                        self.assertEqual(targets[module][0], 'STATIC_LIBRARY' if static else 'SHARED_LIBRARY')
                        self.assertIn('/engine/animation/', targets[module][1])
                        if static:
                            self.assertIn('SKY_MODULE_TAG=' + module.replace('.', '_'), targets[module][3])
                for test, present in (('WorldTest', enabled and tests),
                                      ('ModuleTest', enabled and tests), ('BuilderTest', builder and tests),
                                      ('EditorTest', editor and tests)):
                    self.assertEqual('Animation.' + test in targets, present)
                modules = (root / 'build/static-modules.txt').read_text().split(';')
                self.assertEqual(set(modules) - {''}, set(MODULES) if static else set())
                ownership = (root / 'build/source-ownership.txt').read_text()
                self.assertNotIn('/plugins/animation/', ownership)
                for source in ('GraphDefinition.cpp', 'GraphInstance.cpp'):
                    owners = [line for line in ownership.splitlines() if '/src/graph/' + source + ' | ' in line]
                    self.assertEqual(len(owners), 1)
                    self.assertTrue(owners[0].endswith(' | Animation'))


if __name__ == '__main__':
    unittest.main()
