"""Exercise the production Apple loader with real shared libraries outside cwd."""

from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
CLANG = shutil.which("clang++")
PROBE = "CoreDynamicModuleProbe"
LIBRARY_SOURCE = """\
extern "C" __attribute__((visibility("default"))) int SkyDynamicModuleProbe()
{
    return 42;
}
"""
HOST_SOURCE = """\
#include <core/util/DynamicModule.h>
#include <cstdio>

int main()
{
    sky::DynamicModule module("CoreDynamicModuleProbe");
    if (!module.Load()) {
        std::fprintf(stderr, "%s\\n", module.GetLastError().c_str());
        return 1;
    }
    const auto probe = module.GetAddress<int (*)()>("SkyDynamicModuleProbe");
    if (probe == nullptr || probe() != 42) {
        return 2;
    }
    module.Unload();
    if (module.IsLoaded()) {
        return 3;
    }
    sky::DynamicModule missing("SkyMissingModuleLoaderRegression");
    if (missing.Load() || missing.GetLastError().empty()) {
        return 4;
    }
    return 0;
}
"""


@unittest.skipUnless(sys.platform == "darwin" and CLANG, "requires the macOS compiler and dynamic loader")
class AppleDynamicModuleLoaderTest(unittest.TestCase):
    def run_loader_fixture(self, ios_layout):
        with tempfile.TemporaryDirectory(prefix="sky-module-loader-") as temp:
            root = Path(temp)
            bundle = root / "Probe.app"
            executable_dir = bundle if ios_layout else bundle / "Contents/MacOS"
            executable_dir.mkdir(parents=True)
            unrelated_dir = root / "unrelated-cwd"
            unrelated_dir.mkdir()
            library_source = root / "probe.cpp"
            host_source = root / "host.cpp"
            library_source.write_text(LIBRARY_SOURCE, encoding="ascii")
            host_source.write_text(HOST_SOURCE, encoding="ascii")

            if ios_layout:
                library_dir = bundle / "Frameworks" / f"{PROBE}.framework"
                library_dir.mkdir(parents=True)
                library = library_dir / PROBE
            else:
                library = executable_dir / f"lib{PROBE}.dylib"
            subprocess.run([CLANG, "-dynamiclib", str(library_source), "-o", str(library)], check=True, capture_output=True)

            command = [CLANG, "-std=c++20", "-I", str(ROOT / "engine/core/include")]
            if ios_layout:
                # Select the iOS path policy while using a host-runnable Mach-O.
                # The real iOS source is compiled separately by the device build.
                conditions = root / "conditions"
                conditions.mkdir()
                (conditions / "TargetConditionals.h").write_text("#pragma once\n#define TARGET_OS_IPHONE 1\n", encoding="ascii")
                command.extend(["-I", str(conditions)])
            executable = executable_dir / "Probe"
            command.extend([str(host_source), str(ROOT / "engine/core/src/util/DynamicModule.cpp"), "-o", str(executable)])
            subprocess.run(command, check=True, capture_output=True)
            result = subprocess.run([str(executable)], cwd=unrelated_dir, text=True, capture_output=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_macos_bundle_library_loads_outside_working_directory(self):
        self.run_loader_fixture(ios_layout=False)

    def test_ios_framework_path_policy_loads_from_frameworks_directory(self):
        self.run_loader_fixture(ios_layout=True)


if __name__ == "__main__":
    unittest.main()
