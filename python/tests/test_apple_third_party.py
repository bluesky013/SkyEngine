"""Apple dependency target identity, native artifacts and tool-source contracts."""

import contextlib
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[2]


def load_script(name, arguments=()):
    source = ROOT / "python" / (name + ".py")
    spec = importlib.util.spec_from_file_location("apple_test_" + name, source)
    module = importlib.util.module_from_spec(spec)
    with mock.patch.object(sys, "argv", [str(source), *arguments]):
        spec.loader.exec_module(module)
    return module


THIRD_PARTY = load_script("third_party")
OPENSSL = load_script("build_openssl", ["-p", "MacOS-x86", "-o", "unused-output"])
CPYTHON = load_script("build_cpython", ["-p", "MacOS-x86", "-o", "unused-output"])


class AppleThirdPartyTargetTest(unittest.TestCase):
    def test_macos_platform_selects_architecture_independently_of_host(self):
        for platform, architecture in (("MacOS-x86", "x86_64"), ("MacOS-arm", "arm64")):
            with self.subTest(platform=platform), mock.patch.multiple(
                    THIRD_PARTY.args, platform=platform, output="output", intermediate="intermediate"):
                options = {}
                THIRD_PARTY.fill_common_options(options, "Release", "install")
                self.assertEqual(options["CMAKE_OSX_ARCHITECTURES"], architecture)
                self.assertEqual(options["CMAKE_OSX_SYSROOT"], "macosx")

    def test_win32_preserves_existing_options_artifact_and_key_contracts(self):
        with mock.patch.multiple(THIRD_PARTY.args, platform="Win32", output="output", intermediate="intermediate"):
            with self.subTest(contract="options"):
                options = {}
                THIRD_PARTY.fill_common_options(options, "Release", "install")
                self.assertFalse({"CMAKE_OSX_ARCHITECTURES", "CMAKE_OSX_SYSROOT", "CMAKE_TOOLCHAIN_FILE",
                                  "CMAKE_MACOSX_BUNDLE", "PLATFORM"}.intersection(options))
            with self.subTest(contract="artifacts"), \
                    mock.patch.object(THIRD_PARTY.shutil, "which") as locate, \
                    mock.patch.object(THIRD_PARTY.subprocess, "run") as inspect:
                self.assertIsNone(THIRD_PARTY.apple_artifact_error("not-an-apple-package", "Win32"))
                locate.assert_not_called()
                inspect.assert_not_called()
            with self.subTest(contract="key"):
                package = {"name": "sfmt", "tag": "1.5.4", "header_only": False}
                legacy_payload = '{"header_only": false, "tag": "1.5.4"}'
                legacy_key = hashlib.sha256(legacy_payload.encode()).hexdigest()[:16]
                self.assertEqual(THIRD_PARTY.compute_package_key(package), legacy_key)
                self.assertEqual(THIRD_PARTY.compute_package_key(package, "Win32"), legacy_key)

    def test_ios_sdk_architecture_and_cache_identity_are_separate(self):
        keys = set()
        for selection, sdk, architecture in (
                ("OS64", "iphoneos", "arm64"),
                ("SIMULATOR64", "iphonesimulator", "x86_64"),
                ("SIMULATORARM64", "iphonesimulator", "arm64")):
            with self.subTest(selection=selection), mock.patch.multiple(
                    THIRD_PARTY.args, platform="IOS", ios_platform=selection,
                    intermediate="intermediate", output="output"):
                options = {}
                THIRD_PARTY.fill_common_options(options, "Debug", "install")
                self.assertEqual(options["PLATFORM"], selection)
                self.assertEqual(options["CMAKE_OSX_SYSROOT"], sdk)
                self.assertEqual(options["CMAKE_OSX_ARCHITECTURES"], architecture)
                keys.add(THIRD_PARTY.compute_package_key({"name": "sfmt", "tag": "1.5.4"}))
        self.assertEqual(len(keys), 3)
        package = {"name": "sfmt", "tag": "1.5.4"}
        self.assertNotEqual(THIRD_PARTY.compute_package_key(package, "MacOS-x86"),
                            THIRD_PARTY.compute_package_key(package, "MacOS-arm"))

    def test_default_ios_selection_remains_device_arm64(self):
        with mock.patch.object(THIRD_PARTY.args, "ios_platform", None):
            self.assertEqual(THIRD_PARTY.ios_build_identity(),
                             {"platform": "OS64", "sdk": "iphoneos", "architecture": "arm64"})

    def test_ios_selection_is_rejected_for_other_platforms(self):
        result = subprocess.run([sys.executable, str(ROOT / "python/third_party.py"),
                                 "-p", "MacOS-x86", "--ios-platform", "SIMULATOR64", "--list"],
                                capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 2)
        self.assertIn("--ios-platform is only valid", result.stderr)

    def test_conflicting_package_and_extra_cmake_options_fail_before_build(self):
        with mock.patch.multiple(THIRD_PARTY.args, platform="IOS", ios_platform="SIMULATORARM64"):
            for options, extra in (({"CMAKE_OSX_ARCHITECTURES": "x86_64"}, ""),
                                   ({}, "-DCMAKE_OSX_SYSROOT=iphoneos"),
                                   ({}, "-D PLATFORM=OS64"),
                                   ({}, "-DCMAKE_OSX_ARCHITECTURES:STRING=arm64;x86_64")):
                with self.subTest(options=options, extra=extra), \
                        mock.patch.dict(os.environ, {"SKY_3RD_CMAKE_ARGS": extra}), \
                        mock.patch.object(THIRD_PARTY, "run_cmake") as build, \
                        mock.patch.object(THIRD_PARTY, "copy_package") as publish:
                    self.assertFalse(THIRD_PARTY.build_package_type("sfmt", "source", "Debug", options, None, None))
                    build.assert_not_called()
                    publish.assert_not_called()

    def test_matching_absolute_sdk_path_is_accepted(self):
        with mock.patch.multiple(THIRD_PARTY.args, platform="IOS", ios_platform="SIMULATOR64"), \
                mock.patch.dict(os.environ, {"SKY_3RD_CMAKE_ARGS": ""}):
            self.assertIsNone(THIRD_PARTY.apple_option_error(
                {"CMAKE_OSX_SYSROOT": "/Installed/iPhoneSimulator18.5.sdk"}, "IOS"))

    def test_simulator_and_device_build_trees_do_not_overlap(self):
        directories = []
        with tempfile.TemporaryDirectory() as temp, \
                mock.patch.multiple(THIRD_PARTY.args, platform="IOS", intermediate=temp, output=temp), \
                mock.patch.dict(os.environ, {"SKY_3RD_CMAKE_ARGS": ""}), \
                mock.patch.object(THIRD_PARTY, "run_cmake") as build, \
                mock.patch.object(THIRD_PARTY, "apple_artifact_error", return_value=None), \
                mock.patch.object(THIRD_PARTY, "copy_package"):
            for selection in THIRD_PARTY.IOS_BUILD_TARGETS:
                with mock.patch.object(THIRD_PARTY.args, "ios_platform", selection):
                    self.assertTrue(THIRD_PARTY.build_package_type("sfmt", temp, "Debug", {}, None, None))
                    directories.append(build.call_args.args[0])
        self.assertEqual(len(set(directories)), 3)

    def test_invalid_artifact_is_not_published(self):
        with tempfile.TemporaryDirectory() as temp, \
                mock.patch.multiple(THIRD_PARTY.args, platform="MacOS-arm", intermediate=temp, output=temp), \
                mock.patch.dict(os.environ, {"SKY_3RD_CMAKE_ARGS": ""}), \
                mock.patch.object(THIRD_PARTY, "run_cmake"), \
                mock.patch.object(THIRD_PARTY, "apple_artifact_error", return_value="wrong architecture"), \
                mock.patch.object(THIRD_PARTY, "copy_package") as publish:
            self.assertFalse(THIRD_PARTY.build_package_type("sfmt", temp, "Release", {}, None, None))
            publish.assert_not_called()

    def test_custom_output_failure_does_not_mark_metadata(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / "cmake").mkdir()
            (root / "cmake/thirdparty.json").write_text(json.dumps({
                "packages": [{"name": "custom", "tag": "1", "custom_only": True}]}))
            arguments = SimpleNamespace(engine=temp, output=temp, intermediate=temp, platform="MacOS-arm",
                                        target=None, force=False, clean=False, check=False, list=False, archive=None)
            with mock.patch.object(THIRD_PARTY, "args", arguments), \
                    mock.patch.object(THIRD_PARTY, "load_build_metadata", return_value={}), \
                    mock.patch.object(THIRD_PARTY, "process_package"), \
                    mock.patch.object(THIRD_PARTY, "package_output_error", return_value="wrong architecture"), \
                    mock.patch.object(THIRD_PARTY, "mark_package_built") as mark:
                with self.assertRaises(SystemExit) as result:
                    THIRD_PARTY.app_main()
                self.assertEqual(result.exception.code, 1)
                mark.assert_not_called()

    def test_invalid_cached_output_cannot_skip_rebuild(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / "cmake").mkdir()
            package = {"name": "sfmt", "tag": "1.5.4"}
            (root / "cmake/thirdparty.json").write_text(json.dumps({"packages": [package]}))
            arguments = SimpleNamespace(engine=temp, output=temp, intermediate=temp, platform="MacOS-arm",
                                        target=None, force=False, clean=False, check=False, list=False, archive=None)
            with mock.patch.object(THIRD_PARTY, "args", arguments), \
                    mock.patch.object(THIRD_PARTY, "load_build_metadata", return_value={}), \
                    mock.patch.object(THIRD_PARTY, "is_package_up_to_date", return_value=True), \
                    mock.patch.object(THIRD_PARTY, "package_output_error", return_value="wrong architecture"), \
                    mock.patch.object(THIRD_PARTY, "process_package", return_value=False) as build, \
                    mock.patch.object(THIRD_PARTY, "mark_package_built") as mark:
                with self.assertRaises(SystemExit):
                    THIRD_PARTY.app_main()
                build.assert_called_once_with(package)
                mark.assert_not_called()


class AppleToolSourceTest(unittest.TestCase):
    def prepare(self, root):
        package = {"name": "ios-cmake", "tag": "4.5.0", "is_tool": True, "platforms": ["IOS"]}
        (root / "IOS").mkdir()
        key = THIRD_PARTY.compute_package_key(package, "IOS")
        (root / "IOS/build_metadata.json").write_text(json.dumps({"ios-cmake": {"key": key, "platform": "IOS"}}))
        source = root / "intermediate/ios-cmake"
        source.mkdir(parents=True)
        return package, source

    def test_pinned_tool_file_passes_without_install_directory(self):
        with tempfile.TemporaryDirectory() as temp, \
                mock.patch.multiple(THIRD_PARTY.args, output=temp, intermediate=None, ios_platform=None):
            root = Path(temp)
            package, source = self.prepare(root)
            (source / "ios.toolchain.cmake").write_text("# pinned toolchain\n")
            with mock.patch.object(THIRD_PARTY.subprocess, "run", side_effect=[
                    SimpleNamespace(returncode=0, stdout="revision\n"),
                    SimpleNamespace(returncode=0, stdout="revision\n"),
                    SimpleNamespace(returncode=0)]):
                self.assertEqual(THIRD_PARTY.check_platform([package], "IOS"), (0, 0))
            self.assertFalse((root / "IOS/ios-cmake").exists())

    def test_missing_tool_is_not_hidden_by_an_empty_install_directory(self):
        with tempfile.TemporaryDirectory() as temp, \
                mock.patch.multiple(THIRD_PARTY.args, output=temp, intermediate=None, ios_platform=None):
            root = Path(temp)
            package, source = self.prepare(root)
            (root / "IOS/ios-cmake").mkdir()
            self.assertEqual(THIRD_PARTY.check_platform([package], "IOS"), (1, 0))

    def test_changed_tool_file_or_revision_fails_strict_inspection(self):
        for revision, modified in (("other", 0), ("revision", 1)):
            with self.subTest(revision=revision, modified=modified), tempfile.TemporaryDirectory() as temp, \
                    mock.patch.multiple(THIRD_PARTY.args, output=temp, intermediate=None, ios_platform=None):
                root = Path(temp)
                package, source = self.prepare(root)
                (source / "ios.toolchain.cmake").write_text("# toolchain\n")
                with mock.patch.object(THIRD_PARTY.subprocess, "run", side_effect=[
                        SimpleNamespace(returncode=0, stdout=revision),
                        SimpleNamespace(returncode=0, stdout="revision"),
                        SimpleNamespace(returncode=modified)]):
                    self.assertEqual(THIRD_PARTY.check_platform([package], "IOS"), (1, 0))


class AppleCustomTargetTest(unittest.TestCase):
    def test_openssl_receives_target_architecture_in_cmake(self):
        for platform, architecture in (("MacOS-x86", "x86_64"), ("MacOS-arm", "arm64")):
            with self.subTest(platform=platform), mock.patch.object(OPENSSL.args, "platform", platform), \
                    mock.patch.object(OPENSSL, "run") as run:
                OPENSSL.configure_cmake("darwin", Path("generated"))
                arguments = run.call_args_list[0].args[0]
                self.assertIn("-DCMAKE_OSX_ARCHITECTURES=" + architecture, arguments)
                self.assertIn("-DCMAKE_OSX_SYSROOT=macosx", arguments)

    def test_native_cpython_arch_flags_preserve_existing_options(self):
        for platform, architecture in (("MacOS-x86", "x86_64"), ("MacOS-arm", "arm64")):
            with self.subTest(platform=platform), mock.patch.object(CPYTHON.args, "platform", platform), \
                    mock.patch.object(CPYTHON.sys, "platform", "darwin"), \
                    mock.patch.object(CPYTHON.os, "uname", create=True, return_value=SimpleNamespace(machine=architecture)), \
                    mock.patch.dict(os.environ, {"CFLAGS": "-O2", "CXXFLAGS": "", "LDFLAGS": "-Lexisting"}):
                environment, error = CPYTHON.apple_unix_environment(3, 13)
                self.assertIsNone(error)
                self.assertIn("-O2", environment["CFLAGS"])
                self.assertIn("-Lexisting", environment["LDFLAGS"])
                for name in ("CFLAGS", "CXXFLAGS", "LDFLAGS"):
                    self.assertIn("-arch " + architecture, environment[name])

    def test_cpython_cross_host_and_foreign_arch_flags_are_explicit_failures(self):
        with mock.patch.object(CPYTHON.args, "platform", "MacOS-arm"), \
                mock.patch.object(CPYTHON.sys, "platform", "darwin"), \
                mock.patch.object(CPYTHON.os, "uname", create=True, return_value=SimpleNamespace(machine="x86_64")):
            environment, error = CPYTHON.apple_unix_environment(3, 13)
            self.assertIsNone(environment)
            self.assertIn("Python 3.13 build interpreter", error)
        with mock.patch.object(CPYTHON.args, "platform", "MacOS-arm"), \
                mock.patch.object(CPYTHON.sys, "platform", "darwin"), \
                mock.patch.object(CPYTHON.os, "uname", create=True, return_value=SimpleNamespace(machine="arm64")), \
                mock.patch.dict(os.environ, {"CFLAGS": "-arch x86_64"}):
            environment, error = CPYTHON.apple_unix_environment(3, 13)
            self.assertIsNone(environment)
            self.assertIn("requires CFLAGS", error)


class AppleSlangHostGeneratorTest(unittest.TestCase):
    def test_cross_host_macos_uses_host_tools_and_failure_cannot_continue_target_build(self):
        for directory in ("host-generators", None):
            with self.subTest(directory=directory), \
                    mock.patch.object(THIRD_PARTY.args, "platform", "MacOS-arm"), \
                    mock.patch.object(THIRD_PARTY.host_platform, "machine", return_value="x86_64"), \
                    mock.patch.object(THIRD_PARTY.subprocess, "check_output", return_value="v2026.19"), \
                    mock.patch.object(THIRD_PARTY, "build_slang_host_generators", return_value=directory) as host, \
                    mock.patch.object(THIRD_PARTY, "build_package_type", return_value=True) as target:
                success = THIRD_PARTY.build_package("slang", "source", {}, None, None)
                host.assert_called_once()
                self.assertEqual(success, directory is not None)
                if directory is None:
                    target.assert_not_called()
                else:
                    for call in target.call_args_list:
                        self.assertEqual(call.args[3]["SLANG_GENERATORS_PATH"], directory)
        with self.subTest(platform="Win32"), \
                mock.patch.object(THIRD_PARTY.args, "platform", "Win32"), \
                mock.patch.object(THIRD_PARTY.subprocess, "check_output", return_value="v2026.19"), \
                mock.patch.object(THIRD_PARTY, "build_slang_host_generators") as host, \
                mock.patch.object(THIRD_PARTY, "build_package_type", return_value=True):
            self.assertTrue(THIRD_PARTY.build_package("slang", "source", {}, None, None))
            host.assert_not_called()

    def test_host_target_sdk_flags_are_isolated_and_architecture_invalidates_cached_tools(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            commands = []

            def run(arguments):
                commands.append(arguments)
                if "--build" in arguments:
                    bootstrap = root / "build_host_generators/generators/Release/bin" / (
                        "slang-bootstrap.exe" if os.name == "nt" else "slang-bootstrap")
                    bootstrap.parent.mkdir(parents=True, exist_ok=True)
                    bootstrap.write_text("tool fixture")
                    bootstrap.chmod(0o755)

            options = {"CMAKE_OSX_ARCHITECTURES": "arm64", "CMAKE_OSX_SYSROOT": "iphoneos",
                       "CMAKE_CXX_FLAGS": "--target=arm64-apple-ios", "PLATFORM": "OS64",
                       "SLANG_EMBED_CORE_MODULE": "ON", "SLANG_GENERATORS_PATH": "target-tools"}
            with mock.patch.object(THIRD_PARTY.subprocess, "check_output", return_value="source-revision"), \
                    mock.patch.object(THIRD_PARTY.host_platform, "system", return_value="Darwin"), \
                    mock.patch.object(THIRD_PARTY.host_platform, "machine", return_value="x86_64"), \
                    mock.patch.object(THIRD_PARTY, "run_stream", side_effect=run), \
                    mock.patch.object(THIRD_PARTY, "host_generator_error", return_value=None):
                first = THIRD_PARTY.build_slang_host_generators(temp, options)
                self.assertTrue(first)
                configure = commands[0]
                self.assertIn("-DCMAKE_OSX_ARCHITECTURES=x86_64", configure)
                self.assertIn("-DCMAKE_OSX_SYSROOT=macosx", configure)
                self.assertFalse(any("iphoneos" in value or "--target=" in value or
                                     value.startswith(("-DPLATFORM=", "-DSLANG_GENERATORS_PATH=")) for value in configure))
                identity_path = root / "build_host_generators/source-revision.txt"
                identity = json.loads(identity_path.read_text())
                self.assertEqual(identity["architecture"], "x86_64")
                self.assertEqual(identity["system"], "Darwin")
                self.assertEqual(THIRD_PARTY.build_slang_host_generators(temp, options), first)
                self.assertEqual(len(commands), 2)
                with mock.patch.object(THIRD_PARTY.host_platform, "machine", return_value="arm64"):
                    self.assertTrue(THIRD_PARTY.build_slang_host_generators(temp, options))
                self.assertEqual(len(commands), 4)
                self.assertIn("-DCMAKE_OSX_ARCHITECTURES=arm64", commands[2])

    def test_cached_generator_must_pass_native_inspection_again(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            commands = []

            def run(arguments):
                commands.append(arguments)
                if "--build" in arguments:
                    bootstrap = root / "build_host_generators/generators/Release/bin" / (
                        "slang-bootstrap.exe" if os.name == "nt" else "slang-bootstrap")
                    bootstrap.parent.mkdir(parents=True, exist_ok=True)
                    bootstrap.write_text("fixture")
                    bootstrap.chmod(0o755)

            with mock.patch.object(THIRD_PARTY.subprocess, "check_output", return_value="source-revision"), \
                    mock.patch.object(THIRD_PARTY.host_platform, "system", return_value="Darwin"), \
                    mock.patch.object(THIRD_PARTY.host_platform, "machine", return_value="x86_64"), \
                    mock.patch.object(THIRD_PARTY, "run_stream", side_effect=run), \
                    mock.patch.object(THIRD_PARTY, "host_generator_error", return_value=None):
                THIRD_PARTY.build_slang_host_generators(temp, {})
                with mock.patch.object(THIRD_PARTY, "host_generator_error", side_effect=["wrong host CPU", None]):
                    self.assertTrue(THIRD_PARTY.build_slang_host_generators(temp, {}))
                self.assertEqual(len(commands), 4)


@unittest.skipUnless(sys.platform == "darwin" and shutil.which("xcrun"), "requires Apple SDK tools")
class AppleArtifactIntegrationTest(unittest.TestCase):
    def build_archive(self, root, target, sdk):
        sdk_result = subprocess.run(["xcrun", "--sdk", sdk, "--show-sdk-path"], capture_output=True, text=True)
        if sdk_result.returncode:
            self.skipTest("Selected Apple SDK is not installed: " + sdk)
        source = root / "probe.c"
        source.write_text("int sky_probe(void) { return 42; }\n")
        library = root / "lib/Release/libprobe.a"
        library.parent.mkdir(parents=True)
        obj = root / "probe.o"
        result = subprocess.run(["xcrun", "clang", "-target", target, "-isysroot", sdk_result.stdout.strip(),
                                 "-c", str(source), "-o", str(obj)], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        result = subprocess.run(["xcrun", "ar", "rcs", str(library), str(obj)], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_actual_macos_library_rejects_the_other_target_architecture(self):
        for architecture, platform, wrong in (("x86_64", "MacOS-x86", "MacOS-arm"),
                                             ("arm64", "MacOS-arm", "MacOS-x86")):
            with self.subTest(architecture=architecture), tempfile.TemporaryDirectory() as temp:
                root = Path(temp)
                self.build_archive(root, architecture + "-apple-macos11.0", "macosx")
                self.assertIsNone(THIRD_PARTY.apple_artifact_error(root, platform))
                self.assertIn("expected only", THIRD_PARTY.apple_artifact_error(root, wrong))

    def test_arm64_device_and_simulator_libraries_are_not_interchangeable(self):
        for selection, target, sdk, wrong in (
                ("OS64", "arm64-apple-ios13.0", "iphoneos", "SIMULATORARM64"),
                ("SIMULATORARM64", "arm64-apple-ios13.0-simulator", "iphonesimulator", "OS64")):
            with self.subTest(selection=selection), tempfile.TemporaryDirectory() as temp:
                root = Path(temp)
                self.build_archive(root, target, sdk)
                with mock.patch.object(THIRD_PARTY.args, "ios_platform", selection):
                    self.assertIsNone(THIRD_PARTY.apple_artifact_error(root, "IOS"))
                with mock.patch.object(THIRD_PARTY.args, "ios_platform", wrong):
                    self.assertIn("expected", THIRD_PARTY.apple_artifact_error(root, "IOS"))

    def test_extensionless_macho_executable_is_checked_but_scripts_are_not(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            self.build_archive(root, "x86_64-apple-macos11.0", "macosx")
            (root / "bin").mkdir()
            script = root / "bin/helper"
            script.write_text("#!/bin/sh\necho helper\n")
            self.assertIsNone(THIRD_PARTY.apple_artifact_error(root, "MacOS-x86"))
            sdk = subprocess.run(["xcrun", "--sdk", "macosx", "--show-sdk-path"],
                                 capture_output=True, text=True)
            source = root / "main.c"
            source.write_text("int main(void) { return 0; }\n")
            executable = root / "bin/compiler"
            result = subprocess.run(["xcrun", "clang", "-target", "arm64-apple-macos11.0",
                                     "-isysroot", sdk.stdout.strip(), str(source), "-o", str(executable)],
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            error = THIRD_PARTY.apple_artifact_error(root, "MacOS-x86")
            self.assertIn(str(executable.resolve()), error)
            self.assertIn("expected only x86_64", error)
            bootstrap = executable.with_name("slang-bootstrap")
            executable.rename(bootstrap)
            with mock.patch.object(THIRD_PARTY.host_platform, "system", return_value="Darwin"), \
                    mock.patch.object(THIRD_PARTY.host_platform, "machine", return_value="x86_64"):
                self.assertIn("expected only x86_64", THIRD_PARTY.host_generator_error(root / "bin"))
                bootstrap.write_text("#!/bin/sh\nexit 0\n")
                self.assertIn("not a native Mach-O", THIRD_PARTY.host_generator_error(root / "bin"))

    def test_broken_and_external_native_runtime_symlinks_do_not_hide_behind_valid_libraries(self):
        for mode in ("broken", "external-file", "external-directory"):
            with self.subTest(mode=mode), tempfile.TemporaryDirectory() as temp:
                container = Path(temp)
                root = container / "package"
                root.mkdir()
                self.build_archive(root, "x86_64-apple-macos11.0", "macosx")
                alias = root / "lib/Release/libalias.dylib"
                if mode == "broken":
                    alias.symlink_to("missing.dylib")
                    diagnostic = "broken runtime symlink"
                else:
                    external = container / "external"
                    external.mkdir()
                    shutil.copy2(root / "lib/Release/libprobe.a", external / "libprobe.a")
                    alias.symlink_to(external / "libprobe.a" if mode == "external-file" else external,
                                     target_is_directory=mode == "external-directory")
                    diagnostic = "runtime symlink escapes package"
                self.assertIn(diagnostic, THIRD_PARTY.apple_artifact_error(root, "MacOS-x86"))


if __name__ == "__main__":
    unittest.main()
