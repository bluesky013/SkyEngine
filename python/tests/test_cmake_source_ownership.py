"""Exercise the real configure-time guard without engine or third-party dependencies."""

from pathlib import Path
import os
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]


class SourceOwnershipTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix='sky-source-ownership-')
        cls.addClassCleanup(cls.temp.cleanup)
        cls.root = Path(cls.temp.name)
        cls.source = cls.root / 'source'
        for name in ('owner/impl.cpp', 'owner/impl.extra.cpp', 'owner/impl.inc', 'consumer/main.cpp',
                     'owner/test/shared.cpp', 'consumer/test/main.cpp', 'consumer/module_test/main.cpp'):
            path = cls.source / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text('int main() { return 0; }\n', encoding='utf-8')

    def configure(self, consumer, owner='add_library(Owner STATIC impl.cpp)', root='', minimum='3.19'):
        (self.source / 'owner/CMakeLists.txt').write_text(owner, encoding='utf-8')
        (self.source / 'consumer/CMakeLists.txt').write_text(consumer, encoding='utf-8')
        (self.source / 'CMakeLists.txt').write_text('''cmake_minimum_required(VERSION @MINIMUM@)
project(SourceOwnership LANGUAGES CXX)
enable_testing()
include("@ROOT@/cmake/functions.cmake")
add_subdirectory(owner)
add_subdirectory(consumer)
@BODY@
'''.replace('@ROOT@', ROOT.as_posix()).replace('@BODY@', root).replace('@MINIMUM@', minimum), encoding='utf-8')
        return subprocess.run(['cmake', '-S', str(self.source), '-B', str(self.root / 'build')],
                              stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=120)

    def accepts(self, *args, **kwargs):
        result = self.configure(*args, **kwargs)
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertNotIn('Source ownership:', result.stdout)

    def rejects(self, diagnostic, *args, **kwargs):
        result = self.configure(*args, **kwargs)
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn('Source ownership:', result.stdout)
        self.assertIn(diagnostic, ' '.join(result.stdout.split()))

    def test_linking_implementation_preserves_single_owner(self):
        self.accepts('sky_add_test(TARGET Consumer SOURCES test/main.cpp LIBS Owner)')
        lines = (self.root / 'build/source-ownership.txt').read_text().splitlines()
        self.assertEqual(len([line for line in lines if line.endswith('/owner/impl.cpp | Owner')]), 1)

    def test_object_reuse_does_not_recompile(self):
        self.accepts('''sky_add_test(TARGET A SOURCES test/main.cpp $<TARGET_OBJECTS:Owner>)
sky_add_test(TARGET B SOURCES test/main.cpp LIBS Owner)''',
                     owner='add_library(Owner OBJECT impl.cpp)')

    def test_shared_test_main_and_dll_probes_are_allowed(self):
        self.accepts('''sky_add_test(TARGET A SOURCES test/main.cpp ../owner/test/shared.cpp)
sky_add_test(TARGET B SOURCES module_test/main.cpp ../owner/test/shared.cpp)
add_library(ProbeA SHARED ../owner/test/shared.cpp)
add_library(ProbeB SHARED ../owner/test/shared.cpp)''')

    def test_duplicate_inside_owner_directory_is_rejected(self):
        self.rejects('compiled by multiple targets', '', owner='''add_library(Owner STATIC impl.cpp)
add_library(Copy STATIC ./impl.cpp)''')

    def test_glob_does_not_hide_duplicate_sources(self):
        self.rejects('compiled by multiple targets', '', owner='''add_library(Owner STATIC impl.cpp)
file(GLOB copies *.cpp)
add_library(Copy STATIC ${copies})''')

    def test_cross_directory_glob_is_rejected(self):
        self.rejects('outside its directory', '''file(GLOB copies ../owner/*.cpp)
add_library(Copy STATIC ${copies})''', owner='')

    def test_late_target_sources_is_checked(self):
        self.rejects('outside its directory', 'add_library(Copy STATIC main.cpp)',
                     root='target_sources(Copy PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/owner/impl.cpp")')

    def test_deferred_target_sources_is_checked(self):
        self.rejects('outside its directory', 'add_library(Copy STATIC main.cpp)',
                     root='cmake_language(DEFER CALL target_sources Copy PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/owner/impl.cpp")')

    def test_test_cannot_compile_production_even_when_owner_is_disabled(self):
        self.rejects('compiles production source',
                     'sky_add_test(TARGET Consumer SOURCES test/main.cpp ../owner/impl.cpp)', owner='')

    def test_test_in_owning_directory_still_cannot_compile_implementation(self):
        self.rejects('compiles production source', '',
                     owner='sky_add_test(TARGET Consumer SOURCES test/shared.cpp impl.cpp)')

    def test_dotted_source_name_is_checked(self):
        self.rejects('compiles production source',
                     'sky_add_test(TARGET Consumer SOURCES ../owner/impl.extra.cpp)', owner='')

    def test_explicit_source_language_cannot_hide_compilation(self):
        self.rejects('compiles production source', '''set_source_files_properties(../owner/impl.inc PROPERTIES LANGUAGE CXX)
sky_add_test(TARGET Consumer SOURCES ../owner/impl.inc)''', owner='')

    def test_normalized_paths_share_one_identity(self):
        self.rejects('compiled by multiple targets', '', owner='''add_library(Owner STATIC impl.cpp)
add_library(Copy STATIC "${CMAKE_CURRENT_SOURCE_DIR}/test/../impl.cpp")''')

    @unittest.skipUnless(os.name == 'nt', 'Windows path identity')
    def test_windows_path_case_cannot_hide_duplicates(self):
        self.rejects('compiled by multiple targets', '', owner='''add_library(Owner STATIC impl.cpp)
string(TOUPPER "${CMAKE_CURRENT_SOURCE_DIR}/impl.cpp" copy)
add_library(Copy STATIC "${copy}")''')

    def test_interface_source_propagation_is_rejected(self):
        self.rejects('exports a translation unit', '', owner='''add_library(Owner INTERFACE)
target_sources(Owner INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/impl.cpp")''')
        self.rejects('exports a translation unit', '', owner='''add_library(Owner INTERFACE)
target_sources(Owner INTERFACE "$<$<CONFIG:Debug>:${CMAKE_CURRENT_SOURCE_DIR}/impl.cpp>")''')
        self.rejects('exports a translation unit', '', owner='''add_library(Owner INTERFACE)
set_source_files_properties(impl.inc PROPERTIES LANGUAGE CXX)
target_sources(Owner INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/impl.inc")''')

    def test_unknown_source_expression_fails_closed(self):
        self.rejects('unresolved source expression',
                     'add_library(Copy STATIC "$<$<CONFIG:Debug>:${CMAKE_CURRENT_SOURCE_DIR}/../owner/impl.cpp>")')

    def test_generated_sources_are_owned_not_blanket_exempt(self):
        generated = '''file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/generated.cpp" "int main(){return 0;}")
set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/generated.cpp" PROPERTIES GENERATED TRUE)
sky_add_test(TARGET A SOURCES "${CMAKE_CURRENT_BINARY_DIR}/generated.cpp")'''
        self.accepts(generated)
        self.rejects('compiled by multiple targets', generated + '''
sky_add_test(TARGET B SOURCES "${CMAKE_CURRENT_BINARY_DIR}/generated.cpp")''')

    def test_header_only_source_is_not_compiled(self):
        self.accepts('''set_source_files_properties(../owner/impl.cpp PROPERTIES HEADER_FILE_ONLY TRUE)
sky_add_test(TARGET Consumer SOURCES test/main.cpp ../owner/impl.cpp LIBS Owner)''')

    def test_generated_launcher_registration_from_parent_binary_directory(self):
        self.accepts('add_executable(Launcher main.cpp)', root='''
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/registration.cpp" "int registration(){return 0;}")
set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/registration.cpp" TARGET_DIRECTORY Launcher PROPERTIES GENERATED TRUE)
target_sources(Launcher PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/registration.cpp")''')

    def test_static_generator_marks_registration_for_existing_game_templates(self):
        self.accepts('add_executable(Launcher main.cpp)', minimum='3.20', root='''set(SKY_MODULE_PACKAGING static)
sky_generate_static_modules("${CMAKE_CURRENT_BINARY_DIR}/registration.cpp")
target_sources(Launcher PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/registration.cpp")''')

    def variant(self, definitions='STATIC_FACTORY=1', declaration='STATIC_FACTORY=1', owner='Owner', source='../owner/impl.cpp'):
        return f'''sky_add_test(TARGET Variant SOURCES test/main.cpp ../owner/impl.cpp)
target_compile_definitions(Variant PRIVATE {definitions})
sky_source_compile_variant(TARGET Variant OWNER {owner} SOURCE {source}
    DEFINITIONS {declaration} REASON "Verify a distinct static factory contract")'''

    def test_explicit_compile_variant_is_allowed(self):
        self.accepts(self.variant())

    def test_variant_cannot_lie_about_compile_definitions(self):
        self.rejects('requires distinct compile definition', self.variant(definitions='OTHER=1'))
        self.rejects('requires distinct compile definition', self.variant(), owner='''add_library(Owner STATIC impl.cpp)
target_compile_definitions(Owner PRIVATE STATIC_FACTORY=1)''')

    def test_variant_cannot_name_wrong_owner_or_source(self):
        self.rejects('must both compile the declared source', self.variant(),
                     owner='add_library(Owner STATIC impl.extra.cpp)')
        self.rejects('must both compile the declared source', self.variant(source='../owner/impl.extra.cpp'))

    def test_variant_cannot_exempt_another_consumer(self):
        self.rejects('outside its directory', self.variant() + '\nadd_library(Copy STATIC ../owner/impl.cpp)')

    def test_variants_cannot_replace_the_primary_owner(self):
        self.rejects('no primary compilation owner', '''sky_add_test(TARGET A SOURCES ../owner/impl.cpp)
sky_add_test(TARGET B SOURCES ../owner/impl.cpp)
target_compile_definitions(A PRIVATE A_VARIANT)
target_compile_definitions(B PRIVATE B_VARIANT)
sky_source_compile_variant(TARGET A OWNER B SOURCE ../owner/impl.cpp DEFINITIONS A_VARIANT REASON "A variant")
sky_source_compile_variant(TARGET B OWNER A SOURCE ../owner/impl.cpp DEFINITIONS B_VARIANT REASON "B variant")''', owner='')

    def test_variant_requires_reason_and_test_target(self):
        self.rejects('variant requires', self.variant().replace('REASON "Verify a distinct static factory contract"', ''))
        self.rejects('restricted to test targets', self.variant().replace(
            'sky_add_test(TARGET Variant SOURCES', 'add_executable(Variant'))


if __name__ == '__main__':
    unittest.main()
