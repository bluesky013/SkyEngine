"""Configure real module targets without engine third-party libraries."""

from pathlib import Path
import subprocess
import json
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]


class ModuleConfigurationTest(unittest.TestCase):
    def configure(self, body):
        temporary = tempfile.TemporaryDirectory(prefix="sky-module-config-")
        self.addCleanup(temporary.cleanup)
        source = Path(temporary.name)
        (source / "owner.cpp").write_text("int value() { return 0; }\n", encoding="utf-8")
        (source / "main.cpp").write_text("int main() { return 0; }\n", encoding="utf-8")
        (source / "CMakeLists.txt").write_text(
            f'''cmake_minimum_required(VERSION 3.19)
project(ModuleConfiguration LANGUAGES CXX)
enable_testing()
set(ENGINE_ROOT "{ROOT.as_posix()}")
include("{ROOT.as_posix()}/cmake/functions.cmake")
{body}
''', encoding="utf-8")
        build = source / "build"
        result = subprocess.run(["cmake", "-S", str(source), "-B", str(build)],
                                text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=120)
        return result, build

    def test_particle_builder_follows_development_and_test_needs(self):
        for develop, tests in ((False, False), (False, True), (True, False)):
            with self.subTest(develop=develop, tests=tests):
                result, build = self.configure(f'''
set(SKY_BUILD_PARTICLE ON)
set(SKY_DEVELOP {"ON" if develop else "OFF"})
set(SKY_BUILD_TEST {"ON" if tests else "OFF"})
set(SKY_MODULE_PACKAGING static)
foreach(dependency Core Framework Aurora.Core.Static Aurora.Adaptor.Static Content.Module Content.Metadata)
    add_library(${{dependency}} INTERFACE)
endforeach()
add_library(3rdParty::googletest INTERFACE IMPORTED)
add_subdirectory("{ROOT.as_posix()}/engine/particle" particle)
if(TARGET Particle.Builder)
    file(WRITE "${{CMAKE_BINARY_DIR}}/builder-module" "present")
endif()
if(TARGET Particle.Builder.Static)
    file(WRITE "${{CMAKE_BINARY_DIR}}/builder-library" "present")
endif()
get_property(launcher_dependencies GLOBAL PROPERTY Launcher_DEP)
file(WRITE "${{CMAKE_BINARY_DIR}}/launcher-dependencies" "${{launcher_dependencies}}")
sky_generate_static_modules("${{CMAKE_BINARY_DIR}}/RegisterStaticModules.cpp")
''')
                self.assertEqual(result.returncode, 0, result.stdout)
                self.assertEqual((build / "builder-module").exists(), develop)
                self.assertEqual((build / "builder-library").exists(), develop or tests)
                self.assertNotIn("Particle.Builder", (build / "launcher-dependencies").read_text())
                generated = (build / "RegisterStaticModules.cpp").read_text()
                self.assertEqual('"Particle.Builder"' in generated, develop)
                self.assertIn('[] { return std::unique_ptr<sky::IModule>', generated)
                self.assertIn('extern "C" bool SkyEngineRegisterStaticModules', generated)
                self.assertIn('if (!manager.AddStaticModule(', generated)
                self.assertIn('return false;', generated)
                self.assertIn('return true;', generated)

    def test_shared_runtime_and_configure_manifest_cases(self):
        cases = json.loads((ROOT / "engine/framework/test/ModuleManifestCases.json").read_text())
        for case in cases:
            with self.subTest(case=case["name"]):
                manifest = json.dumps(case["manifest"])
                result, build = self.configure(f'''
set_property(GLOBAL PROPERTY SKY_RUNTIME_BUILD_OPTIONS Enabled Disabled)
set(Enabled ON)
set(Disabled OFF)
foreach(module Consumer Provider Metadata OFF)
    add_library(${{module}} INTERFACE)
endforeach()
file(WRITE "${{CMAKE_CURRENT_SOURCE_DIR}}/manifest.json" [==[{manifest}]==])
sky_resolve_module_manifest(selected "${{CMAKE_CURRENT_SOURCE_DIR}}/manifest.json")
file(WRITE "${{CMAKE_BINARY_DIR}}/selected" "${{selected}}")
get_target_property(consumer_dependencies Consumer MANUALLY_ADDED_DEPENDENCIES)
file(WRITE "${{CMAKE_BINARY_DIR}}/consumer-order" "${{consumer_dependencies}}")
''')
                if "expected" in case:
                    self.assertEqual(result.returncode, 0, result.stdout)
                    self.assertEqual(set(filter(None, (build / "selected").read_text().split(";"))), set(case["expected"]))
                    if case["name"] == "implicit metadata":
                        self.assertIn("Metadata", (build / "consumer-order").read_text())
                    if case["name"] == "optional disabled":
                        self.assertNotIn("Provider", (build / "consumer-order").read_text())
                else:
                    self.assertNotEqual(result.returncode, 0, result.stdout)

    def test_enabled_missing_target_is_an_error(self):
        result, _ = self.configure('''
file(WRITE "${CMAKE_CURRENT_SOURCE_DIR}/manifest.json" [[{"modules":[{"name":"Missing"}]}]])
sky_resolve_module_manifest(selected "${CMAKE_CURRENT_SOURCE_DIR}/manifest.json")
''')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Enabled module target is unavailable", result.stdout)

    def test_asset_tool_static_inventory_excludes_legacy_dynamic_modules(self):
        result, build = self.configure(f'''
set(SKY_DEVELOP ON)
set(SKY_MODULE_PACKAGING static)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${{CMAKE_BINARY_DIR}}/bin")
set_property(GLOBAL PROPERTY SKY_RUNTIME_BUILD_OPTIONS SKY_BUILD_AUDIO SKY_BUILD_JOLT SKY_BUILD_RECAST SKY_BUILD_TERRAIN SKY_BUILD_ANIMATION SKY_BUILD_PARTICLE)
foreach(option SKY_BUILD_AUDIO SKY_BUILD_JOLT SKY_BUILD_RECAST SKY_BUILD_TERRAIN SKY_BUILD_ANIMATION SKY_BUILD_PARTICLE)
    set(${{option}} OFF)
endforeach()
foreach(module Framework Aurora.Cook Aurora.Metadata Content.Builder.Module Content.Metadata Curve.Builder Curve.Assets)
    add_library(${{module}} INTERFACE)
endforeach()
set_property(GLOBAL PROPERTY SKY_STATIC_MODULE_TARGETS Curve.Builder Curve.Assets)
function(sky_add_exe)
    file(WRITE "${{CMAKE_CURRENT_BINARY_DIR}}/main.cpp" "int main() {{ return 0; }}\n")
    set_source_files_properties("${{CMAKE_CURRENT_BINARY_DIR}}/main.cpp" PROPERTIES GENERATED TRUE)
    add_executable(AssetTool "${{CMAKE_CURRENT_BINARY_DIR}}/main.cpp")
endfunction()
add_subdirectory("{ROOT.as_posix()}/tools/asset_tool" tool)
''')
        self.assertEqual(result.returncode, 0, result.stdout)
        factories = (build / "tool/RegisterStaticModules.cpp").read_text()
        self.assertIn("sky_module_create_Curve_Builder", factories)
        self.assertNotIn("sky_module_create_Aurora_Cook", factories)
        self.assertNotIn("sky_module_create_Content_Builder_Module", factories)
        for name in ("modules_asset_tool.json", "modules_asset_audit.json"):
            self.assertEqual((build / "bin/configs" / name).read_bytes(), (ROOT / "configs" / name).read_bytes())

    def test_private_link_requirements_do_not_leak_to_consumers(self):
        result, build = self.configure('''
add_library(PublicDep INTERFACE)
target_compile_definitions(PublicDep INTERFACE PUBLIC_REQUIREMENT)
add_library(PrivateDep INTERFACE)
target_compile_definitions(PrivateDep INTERFACE PRIVATE_REQUIREMENT)
sky_add_library(TARGET Subject STATIC SOURCES owner.cpp
    PUBLIC_LINK_LIBS PublicDep PRIVATE_LINK_LIBS PrivateDep)
add_executable(Consumer main.cpp)
target_link_libraries(Consumer PRIVATE Subject)
file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/subject-definitions"
    CONTENT "$<TARGET_PROPERTY:Subject,COMPILE_DEFINITIONS>")
file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/consumer-definitions"
    CONTENT "$<TARGET_PROPERTY:Consumer,COMPILE_DEFINITIONS>")
''')
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertIn("PRIVATE_REQUIREMENT", (build / "subject-definitions").read_text())
        consumer = (build / "consumer-definitions").read_text()
        self.assertIn("PUBLIC_REQUIREMENT", consumer)
        self.assertNotIn("PRIVATE_REQUIREMENT", consumer)


if __name__ == "__main__":
    unittest.main()
