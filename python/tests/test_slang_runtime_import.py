"""Check the static Slang import model used by the engine on every platform."""

import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


FINDER = Path(__file__).resolve().parents[2] / "cmake/thirdparty/FindSlang.cmake"
ARCHIVES = ("slang-compiler", "compiler-core", "slang-cpp-parser", "core", "cmark-gfm", "lz4", "miniz", "unexpected")


@unittest.skipUnless(shutil.which("cmake"), "CMake is required to inspect imported targets")
class SlangStaticImportTest(unittest.TestCase):
    def make_package(self, root, suffix, lib_prefix):
        (root / "slang/include").mkdir(parents=True)
        for config in ("Debug", "Release"):
            directory = root / "slang/lib" / config
            directory.mkdir(parents=True)
            for name in ARCHIVES:
                (directory / (lib_prefix + name + suffix)).write_text(config, encoding="utf-8")
            # Stale shared libraries from an older package must not be linked.
            (directory / (lib_prefix + "slang-compiler.so")).write_text(config, encoding="utf-8")
            (directory / (lib_prefix + "slang-compiler.dylib")).write_text(config, encoding="utf-8")

    def configure(self, root, suffix, lib_prefix, windows, config):
        source = root / "source"
        source.mkdir(exist_ok=True)
        (source / "CMakeLists.txt").write_text(
            'cmake_minimum_required(VERSION 3.20)\n'
            'project(SlangStaticCheck LANGUAGES CXX)\n'
            f'set(WIN32 {"TRUE" if windows else "FALSE"})\n'
            f'set(CMAKE_STATIC_LIBRARY_SUFFIX "{suffix}")\n'
            f'set(slang_PATH "{(root / "slang").as_posix()}")\n'
            f'include("{FINDER.as_posix()}")\n'
            'get_target_property(type 3rdParty::slang TYPE)\n'
            'get_target_property(dyn 3rdParty::slang INTERFACE_DYN_LIBS)\n'
            'file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/link.json" CONTENT\n'
            ' "{\\"type\\":\\"${type}\\",\\"dyn\\":\\"${dyn}\\",'
            '\\"libs\\":\\"$<JOIN:$<TARGET_GENEX_EVAL:3rdParty::slang,'
            '$<TARGET_PROPERTY:3rdParty::slang,INTERFACE_LINK_LIBRARIES>>,|>\\"}")\n',
            encoding="utf-8")
        build = root / ("build-" + config)
        run = subprocess.run(["cmake", "-S", str(source), "-B", str(build),
                              "-DCMAKE_BUILD_TYPE=" + config], capture_output=True, text=True)
        return run, build / "link.json"

    def check(self, suffix, lib_prefix, windows):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp).resolve()
            self.make_package(root, suffix, lib_prefix)
            expected = [lib_prefix + name + suffix for name in ARCHIVES]
            for config in ("Debug", "Release", "Develop", "RelWithDebInfo", "MinSizeRel"):
                with self.subTest(config=config):
                    run, output = self.configure(root, suffix, lib_prefix, windows, config)
                    self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
                    model = json.loads(output.read_text(encoding="utf-8"))
                    self.assertEqual(model["type"], "INTERFACE_LIBRARY")
                    self.assertEqual(model["dyn"], "")
                    libs = [Path(path) for path in model["libs"].split("|") if path]
                    runtime_config = "Debug" if config == "Debug" else "Release"
                    config_dir = root / "slang/lib" / runtime_config
                    archive_paths = [path for path in libs if path.suffix == suffix]
                    self.assertEqual([path.name for path in archive_paths], expected)
                    self.assertTrue(all(path.parent == config_dir for path in archive_paths))

    def test_unix_like_packages_link_ordered_static_archives(self):
        self.check(".a", "lib", windows=False)

    def test_windows_packages_link_ordered_static_archives(self):
        self.check(".lib", "", windows=True)


if __name__ == "__main__":
    unittest.main()
