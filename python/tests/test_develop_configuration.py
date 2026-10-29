"""Build-policy and tool-selection regressions without a platform SDK."""

import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location('package_project', ROOT / 'python/package_project.py')
PACKAGE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PACKAGE)


class DevelopToolDiscoveryTest(unittest.TestCase):
    def test_prefers_develop_and_preserves_legacy_fallbacks(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            for name, finder in (('AssetTool', PACKAGE.find_asset_tool), ('ShaderCookTool', PACKAGE.shader_cook_tool)):
                suffix = '.exe' if PACKAGE.os.name == 'nt' else ''
                for configuration in ('', 'RelWithDebInfo', 'Release', 'Debug', 'Develop'):
                    tool = root / 'output/bin' / configuration / (name + suffix)
                    tool.parent.mkdir(parents=True, exist_ok=True)
                    tool.touch()
                    self.assertEqual(finder(root), tool)
                (root / 'output/bin/Develop' / (name + suffix)).unlink()
                self.assertEqual(finder(root), root / 'output/bin/Debug' / (name + suffix))


@unittest.skipUnless(shutil.which('ninja'), 'Ninja is required for SDK-free configuration tests')
class DevelopConfigurationTest(unittest.TestCase):
    def configure(self, root, generator, *args):
        source = root / 'source'
        source.mkdir(exist_ok=True)
        (source / 'CMakeLists.txt').write_text('''cmake_minimum_required(VERSION 3.21)
project(Policy NONE)
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG")
include("@ROOT@/cmake/build_configurations.cmake")
include("@ROOT@/cmake/thirdparty_helpers.cmake")
set(example_PATH "${CMAKE_CURRENT_SOURCE_DIR}/example")
set(CMAKE_STATIC_LIBRARY_PREFIX "lib")
set(CMAKE_STATIC_LIBRARY_SUFFIX ".a")
sky_3rd_static(example LIBS first second DEBUG_SUFFIX d)
add_library(Engine STATIC IMPORTED)
set_target_properties(Engine PROPERTIES IMPORTED_CONFIGURATIONS "Develop;Release"
    IMPORTED_LOCATION_DEVELOP "develop.a" IMPORTED_LOCATION_RELEASE "release.a")
add_library(Prebuilt STATIC IMPORTED)
set_target_properties(Prebuilt PROPERTIES IMPORTED_CONFIGURATIONS Release IMPORTED_LOCATION_RELEASE "prebuilt.a")
get_directory_property(definitions COMPILE_DEFINITIONS)
file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/$<CONFIG>.txt" CONTENT
"${definitions}\\n$<GENEX_EVAL:$<TARGET_PROPERTY:3rdParty::example,INTERFACE_LINK_LIBRARIES>>\\n$<TARGET_FILE:Engine>\\n$<TARGET_FILE:Prebuilt>\\n")
file(WRITE "${CMAKE_BINARY_DIR}/flags.txt" "${CMAKE_CXX_FLAGS_DEVELOP}")
'''.replace('@ROOT@', ROOT.as_posix()), encoding='utf-8')
        result = subprocess.run(['cmake', '-S', str(source), '-B', str(root / 'build'), '-G', generator, *args],
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        return result

    def test_multi_configuration_defaults_and_dependencies(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            result = self.configure(root, 'Ninja Multi-Config', '-DSKY_DEVELOP=ON')
            self.assertEqual(result.returncode, 0, result.stdout)
            for configuration, assertions, validation in (('Debug', 1, 1), ('Develop', 1, 0),
                                                         ('Release', 0, 0), ('RelWithDebInfo', 0, 0)):
                output = (root / 'build' / (configuration + '.txt')).read_text()
                self.assertIn('SKY_ENABLE_ASSERT=' + str(assertions), output)
                self.assertIn('SKY_ENABLE_GPU_VALIDATION=' + str(validation), output)
                self.assertIn('SKY_ENABLE_RESOURCE_NAME=1', output)
                for library in ('first', 'second'):
                    expected = '/Debug/lib' + library + 'd.a' if configuration == 'Debug' else '/Release/lib' + library + '.a'
                    self.assertIn(expected, output)
                if configuration == 'Develop':
                    self.assertIn('develop.a', output)
                    self.assertIn('prebuilt.a', output)

    def test_single_configuration_and_explicit_overrides(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            result = self.configure(root, 'Ninja', '-DCMAKE_BUILD_TYPE=Develop', '-DSKY_ASSERTIONS=OFF',
                                    '-DSKY_GPU_VALIDATION=ON', '-DSKY_RESOURCE_NAMES=OFF')
            self.assertEqual(result.returncode, 0, result.stdout)
            output = (root / 'build/Develop.txt').read_text()
            for definition in ('SKY_BUILD_DEVELOP=1', 'SKY_ENABLE_ASSERT=0',
                               'SKY_ENABLE_GPU_VALIDATION=1', 'SKY_ENABLE_RESOURCE_NAME=0'):
                self.assertIn(definition, output)
            self.assertEqual((root / 'build/flags.txt').read_text(), '-O2 -g -DNDEBUG')

    @unittest.skipUnless(any(shutil.which(name) for name in ('cc', 'clang', 'gcc', 'cl')) and
                         any(shutil.which(name) for name in ('c++', 'clang++', 'g++', 'cl')),
                         'C and C++ compilers are required')
    def test_enabled_languages_initialize_develop_flags_and_preserve_overrides(self):
        overrides = {'C': '-DSKY_TEST_C_OVERRIDE=1', 'CXX': '-DSKY_TEST_CXX_OVERRIDE=1'}
        for explicit in (False, True):
            with self.subTest(explicit=explicit), tempfile.TemporaryDirectory() as temp:
                root = Path(temp)
                source = root / 'source'
                source.mkdir()
                (source / 'CMakeLists.txt').write_text('''cmake_minimum_required(VERSION 3.21)
project(LanguagePolicy LANGUAGES C CXX)
include("@ROOT@/cmake/build_configurations.cmake")
foreach(language C CXX)
    file(WRITE "${CMAKE_BINARY_DIR}/${language}.txt" "${CMAKE_${language}_FLAGS_DEVELOP}")
endforeach()
file(WRITE "${CMAKE_BINARY_DIR}/compiler.txt" "${CMAKE_CXX_COMPILER_ID}")
'''.replace('@ROOT@', ROOT.as_posix()), encoding='utf-8')
                command = ['cmake', '-S', str(source), '-B', str(root / 'build'), '-G', 'Ninja',
                           '-DCMAKE_BUILD_TYPE=Develop']
                if explicit:
                    command.extend('-DCMAKE_' + language + '_FLAGS_DEVELOP=' + flags
                                   for language, flags in overrides.items())
                result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
                self.assertEqual(result.returncode, 0, result.stdout)
                compiler = (root / 'build/compiler.txt').read_text()
                for language, override in overrides.items():
                    flags = (root / 'build' / (language + '.txt')).read_text()
                    if explicit:
                        self.assertEqual(flags, override)
                    else:
                        for expected in (('/O2', '/Zi', '/DNDEBUG') if compiler == 'MSVC'
                                         else ('-O2', '-g', '-DNDEBUG')):
                            self.assertIn(expected, flags)

    def test_invalid_diagnostic_is_rejected(self):
        for option in ('SKY_ASSERTIONS', 'SKY_GPU_VALIDATION', 'SKY_RESOURCE_NAMES'):
            with self.subTest(option=option), tempfile.TemporaryDirectory() as temp:
                result = self.configure(Path(temp), 'Ninja', '-D' + option + '=invalid')
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(option + ' must be AUTO, ON or OFF', result.stdout)

    def test_game_presets_keep_publishing_defaults(self):
        presets = json.loads((ROOT / 'engine/templates/game/native/CMakePresets.json').read_text())
        configure = {p['name']: p for p in presets['configurePresets']}
        build = {p['name']: p for p in presets['buildPresets']}
        for platform in ('windows', 'linux', 'macos', 'ios', 'android'):
            develop = platform + '-develop'
            self.assertEqual(configure[develop]['cacheVariables']['SKY_DEVELOP'], 'ON')
            self.assertEqual(build[develop].get('configuration', 'Develop'), 'Develop')
            self.assertEqual(build[platform].get('configuration', 'Release'), 'Release')


@unittest.skipUnless(shutil.which('cmake'), 'CMake is required for module configuration tests')
class JoltBuilderConfigurationTest(unittest.TestCase):
    def configure(self, root, option=None):
        plugin = root / 'jolt'
        plugin.mkdir()
        for name in ('CMakeLists.txt', 'plugin.json'):
            shutil.copyfile(ROOT / 'plugins/jolt' / name, plugin / name)
        for directory, target in (('shared', 'JoltPhysics.AssetCodec.Static'),
                                  ('adaptor', 'JoltPhysicsModule'), ('render', 'JoltPhysicsRenderModule')):
            child = plugin / directory
            child.mkdir()
            (child / 'stub.cpp').write_text('int stub_' + directory + ';\n', encoding='utf-8')
            declaration = ('sky_add_library(TARGET ' + target + ' STATIC' if directory == 'shared'
                           else 'sky_add_module(TARGET ' + target)
            (child / 'CMakeLists.txt').write_text(declaration + ' SOURCES stub.cpp)\n', encoding='utf-8')
        (plugin / 'test').mkdir()
        (plugin / 'test/CMakeLists.txt').write_text('add_custom_target(JoltFixtureTests)\n', encoding='utf-8')
        (plugin / 'builder/src').mkdir(parents=True)
        (plugin / 'builder/include').mkdir()
        shutil.copyfile(ROOT / 'plugins/jolt/builder/CMakeLists.txt', plugin / 'builder/CMakeLists.txt')
        shutil.copyfile(ROOT / 'plugins/jolt/builder/PhysicsBuilderModule.cpp', plugin / 'builder/PhysicsBuilderModule.cpp')
        (plugin / 'builder/src/PhysicsAssetBuilder.cpp').write_text('int builder_stub;\n', encoding='utf-8')
        (root / 'main.cpp').write_text('int main() { return 0; }\n', encoding='utf-8')
        template = (ROOT / 'engine/templates/game/native/CMakeLists.txt').read_text(encoding='utf-8')
        registration = template[template.index('if (SKY_MODULE_PACKAGING STREQUAL "static")'):
                                template.index('# The same deployment runs')]
        (root / 'CMakeLists.txt').write_text('''cmake_minimum_required(VERSION 3.21)
project(JoltModulePolicy LANGUAGES CXX)
set(ENGINE_ROOT "@ROOT@")
set(3RD_PATH "${CMAKE_CURRENT_SOURCE_DIR}/unused")
set(SKY_MODULE_PACKAGING static)
include("${ENGINE_ROOT}/cmake/options.cmake")
include("${ENGINE_ROOT}/cmake/functions.cmake")
function(sky_find_3rd)
endfunction()
add_library(Physics INTERFACE)
add_library(Framework INTERFACE)
add_executable(Launcher main.cpp)
add_subdirectory(jolt)
sky_set_dependency(TARGET Launcher)
@REGISTRATION@
get_target_property(links Launcher LINK_LIBRARIES)
file(WRITE "${CMAKE_BINARY_DIR}/links.txt" "${links}")
file(WRITE "${CMAKE_BINARY_DIR}/targets.txt" "")
foreach(target JoltPhysics.Builder JoltPhysics.Builder.Static JoltFixtureTests)
    if(TARGET ${target})
        file(APPEND "${CMAKE_BINARY_DIR}/targets.txt" "${target}\n")
    endif()
endforeach()
if(TARGET JoltPhysics.Builder)
    get_target_property(definitions JoltPhysics.Builder COMPILE_DEFINITIONS)
    file(WRITE "${CMAKE_BINARY_DIR}/builder-definitions.txt" "${definitions}")
endif()
'''.replace('@ROOT@', ROOT.as_posix()).replace('@REGISTRATION@', registration), encoding='utf-8')
        command = ['cmake', '-S', str(root), '-B', str(root / 'build')]
        if option:
            command.append('-D' + option + '=ON')
        result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=120)
        self.assertEqual(result.returncode, 0, result.stdout)
        return root / 'build'

    def test_shipping_omits_builder_and_developer_consumers_keep_it(self):
        for option in (None, 'SKY_DEVELOP', 'SKY_BUILD_TOOL', 'SKY_BUILD_SANDBOX', 'SKY_BUILD_TEST'):
            with self.subTest(option=option), tempfile.TemporaryDirectory() as temporary:
                build = self.configure(Path(temporary), option)
                targets = (build / 'targets.txt').read_text().splitlines()
                registration = (build / 'RegisterStaticModules.cpp').read_text()
                links = (build / 'links.txt').read_text().split(';')
                enabled = option is not None
                for target in ('JoltPhysics.Builder', 'JoltPhysics.Builder.Static'):
                    self.assertEqual(target in targets, enabled)
                self.assertEqual('sky_module_create_JoltPhysics_Builder' in registration, enabled)
                self.assertEqual('JoltPhysics.Builder' in links, enabled)
                self.assertEqual('JoltFixtureTests' in targets, option == 'SKY_BUILD_TEST')
                for target in ('JoltPhysicsModule', 'JoltPhysicsRenderModule'):
                    self.assertIn(target, links)
                if enabled:
                    definitions = (build / 'builder-definitions.txt').read_text().split(';')
                    self.assertIn('SKY_MODULE_STATIC', definitions)
                    self.assertIn('SKY_MODULE_TAG=JoltPhysics_Builder', definitions)

    def test_builder_entry_emits_the_factory_requested_by_static_registration(self):
        compiler = next((shutil.which(name) for name in ('clang++', 'g++', 'c++') if shutil.which(name)), None)
        if compiler is None:
            self.skipTest('A GCC-compatible preprocessor is required')
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            headers = {
                'builder/physics/PhysicsAssetBuilder.h': '',
                'framework/asset/AssetBuilderManager.h': '',
            }
            module = (ROOT / 'engine/framework/include/framework/interface/IModule.h').read_text(encoding='utf-8')
            headers['framework/interface/IModule.h'] = '#define SKY_EXPORT\n' + module[module.index('#define REGISTER_MODULE(name)'):]
            for name, content in headers.items():
                path = root / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(content, encoding='utf-8')
            source = ROOT / 'plugins/jolt/builder/PhysicsBuilderModule.cpp'
            for static in (False, True):
                with self.subTest(static=static):
                    command = [compiler, '-E', '-P', '-I', str(root), str(source)]
                    if static:
                        command.extend(['-DSKY_MODULE_STATIC', '-DSKY_MODULE_TAG=JoltPhysics_Builder'])
                    result = subprocess.run(command, capture_output=True, text=True, timeout=120)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    self.assertEqual('sky_module_create_JoltPhysics_Builder' in result.stdout, static)
                    self.assertEqual('StartModule(' in result.stdout, not static)


if __name__ == '__main__':
    unittest.main()
