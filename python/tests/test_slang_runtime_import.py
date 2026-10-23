"""Check the CMake runtime model used by Android and Apple packagers."""

import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


FINDER = Path(__file__).resolve().parents[2] / "cmake/thirdparty/FindSlang.cmake"


@unittest.skipUnless(shutil.which("cmake"), "CMake is required to inspect imported runtime targets")
class SlangRuntimeImportTest(unittest.TestCase):
    def configure(self, root, suffix, config):
        source = root / "source"
        source.mkdir(exist_ok=True)
        (source / "CMakeLists.txt").write_text(
            'cmake_minimum_required(VERSION 3.20)\n'
            'project(SlangRuntimeCheck LANGUAGES NONE)\n'
            'set(WIN32 FALSE)\n' +
            ('set(ANDROID TRUE)\n' if suffix == '.so' else '') +
            f'set(CMAKE_SHARED_LIBRARY_SUFFIX "{suffix}")\n'
            f'set(slang_PATH "{(root / "slang").as_posix()}")\n'
            f'include("{FINDER.as_posix()}")\n'
            'get_target_property(runtime_files 3rdParty::slang INTERFACE_DYN_LIBS)\n'
            'file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/runtime.json" CONTENT\n'
            ' "{\\"type\\":\\"$<TARGET_PROPERTY:3rdParty::slang,TYPE>\\",'
            '\\"compiler\\":\\"$<TARGET_FILE:3rdParty::slang>\\",'
            '\\"files\\":\\"${runtime_files}\\"}")\n', encoding="utf-8")
        build = root / ("build-" + config)
        run = subprocess.run(["cmake", "-S", str(source), "-B", str(build),
                              "-DCMAKE_BUILD_TYPE=" + config], capture_output=True, text=True)
        return run, build / "runtime.json"

    def make_package(self, root, suffix):
        (root / "slang/include").mkdir(parents=True)
        for config in ("Debug", "Release"):
            directory = root / "slang/lib" / config
            directory.mkdir(parents=True)
            (directory / ("libslang-compiler" + suffix)).write_text(config, encoding="utf-8")
            (directory / ("libslang" + suffix)).write_text(config, encoding="utf-8")
            (directory / ("libslang-glsl-module-test" + suffix)).write_text(config, encoding="utf-8")

    def test_android_model_uses_soname_and_matching_runtime_configuration(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp).resolve()
            self.make_package(root, ".so")
            for config, runtime_config in (("Debug", "Debug"), ("Release", "Release"),
                                           ("Develop", "Release"), ("RelWithDebInfo", "Release"), ("MinSizeRel", "Release")):
                with self.subTest(config=config):
                    run, output = self.configure(root, ".so", config)
                    self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
                    model = json.loads(output.read_text(encoding="utf-8"))
                    self.assertEqual(model["type"], "SHARED_LIBRARY")
                    self.assertEqual(Path(model["compiler"]),
                                     root / "slang/lib" / runtime_config / "libslang-compiler.so")
                    files = [Path(path) for path in model["files"].split(";")]
                    self.assertIn(root / "slang/lib" / runtime_config / "libslang-glsl-module-test.so", files)
                    self.assertNotIn(root / "slang/lib" / runtime_config / "libslang.so", files)
                    self.assertTrue(all(path.parent.name == runtime_config for path in files))

    @unittest.skipIf(os.name == "nt", "Creating symlinks requires extra Windows privileges")
    def test_apple_model_resolves_versioned_compiler_symlink(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp).resolve()
            self.make_package(root, ".dylib")
            directory = root / "slang/lib/Release"
            compiler = directory / "libslang-compiler.dylib"
            versioned = directory / "libslang-compiler.0.test.dylib"
            compiler.rename(versioned)
            compiler.symlink_to(versioned.name)
            run, output = self.configure(root, ".dylib", "Release")
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            model = json.loads(output.read_text(encoding="utf-8"))
            self.assertEqual(Path(model["compiler"]), versioned)

    def test_compatibility_alias_alone_cannot_hide_missing_compiler_runtime(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp).resolve()
            self.make_package(root, ".so")
            (root / "slang/lib/Release/libslang-compiler.so").unlink()
            run, _ = self.configure(root, ".so", "Release")
            self.assertNotEqual(run.returncode, 0)
            self.assertIn("Slang compiler runtime is missing:", run.stdout + run.stderr)


if __name__ == "__main__":
    unittest.main()
