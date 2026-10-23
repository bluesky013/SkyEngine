"""Exercise native runtime manifest generation without an Android SDK."""

import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
CMAKE = shutil.which("cmake")


@unittest.skipUnless(CMAKE, "requires CMake")
class AndroidRuntimeManifestTest(unittest.TestCase):
    def test_manifest_selects_config_and_abi_and_ignores_invisible_imports(self):
        template = (ROOT / "engine/templates/game/native/CMakeLists.txt").read_text()
        start = template.index("function(_sky_collect_imported_runtimes directory result)")
        helper = template[start:template.index("endfunction()", start) + len("endfunction()")]
        manifest = template[template.index("# AGP discovers linked imported libraries itself."):]
        with tempfile.TemporaryDirectory(prefix="sky-runtime-manifest-") as temp:
            root = Path(temp)
            source = root / "native"
            child = source / "local-import"
            child.mkdir(parents=True)
            (child / "CMakeLists.txt").write_text("add_library(LocalHeaders INTERFACE IMPORTED)\n")
            (source / "CMakeLists.txt").write_text(
                "cmake_minimum_required(VERSION 3.20)\n"
                "project(RuntimeManifest NONE)\n"
                "set(ANDROID TRUE)\n"
                "set(CMAKE_ANDROID_ARCH_ABI arm64-v8a)\n"
                "add_library(3rdParty::slang SHARED IMPORTED GLOBAL)\n"
                "set_target_properties(3rdParty::slang PROPERTIES IMPORTED_LOCATION /runtime/libslang-compiler.so)\n"
                "add_library(RuntimeCompiler SHARED IMPORTED GLOBAL)\n"
                "set_target_properties(RuntimeCompiler PROPERTIES\n"
                "  IMPORTED_CONFIGURATIONS \"DEBUG;RELEASE\"\n"
                "  IMPORTED_LOCATION_DEBUG /runtime/Debug/libcompiler.so\n"
                "  IMPORTED_LOCATION_RELEASE /runtime/Release/libcompiler.so\n"
                "  INTERFACE_DYN_LIBS \"$<IF:$<CONFIG:Debug>,/runtime/Debug/libcompiler.so;/runtime/Debug/libcompanion.so,"
                "/runtime/Release/libcompiler.so;/runtime/Release/libcompanion.so>\")\n"
                "add_subdirectory(local-import)\n" + helper + "\n" + manifest
            )
            for config in ("Debug", "Release"):
                subprocess.run(
                    [CMAKE, "-S", str(source), "-B", str(root / f"cmake-{config}"), f"-DCMAKE_BUILD_TYPE={config}"],
                    check=True, capture_output=True, text=True,
                )
                path = root / f"build/android/runtime-libraries/{config}/arm64-v8a/manifest.json"
                metadata = json.loads(path.read_text())
                self.assertEqual(metadata["linkedLibraries"], [f"/runtime/{config}/libcompiler.so"])
                self.assertEqual(set(metadata["libraries"]), {
                    f"/runtime/{config}/libcompiler.so", f"/runtime/{config}/libcompanion.so",
                })


if __name__ == "__main__":
    unittest.main()
