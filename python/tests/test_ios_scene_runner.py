"""Validate native test completion reporting without Xcode or a simulator."""

import importlib.util
from pathlib import Path
import plistlib
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch


SCRIPT = Path(__file__).with_name("run_ios_platform_tests.py")
SPEC = importlib.util.spec_from_file_location("ios_scene_test_runner", SCRIPT)
RUNNER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(RUNNER)


class IOSSceneRunnerResultTest(unittest.TestCase):
    def test_twenty_one_completed_tests_are_accepted(self):
        log = "[  PASSED  ] 21 tests.\r\nSKY_IOS_TEST_COUNT=21\r\nSKY_IOS_TEST_EXIT=0\r\n"
        self.assertEqual(RUNNER.validate_test_result(0, log), 21)

    def test_empty_filter_is_rejected_even_with_success_exit_marker(self):
        log = "Running 0 tests\nSKY_IOS_TEST_COUNT=0\nSKY_IOS_TEST_EXIT=0\n"
        with self.assertRaisesRegex(RuntimeError, "No native tests executed"):
            RUNNER.validate_test_result(0, log)

    def test_entirely_skipped_run_does_not_verify_the_contract(self):
        log = "[  SKIPPED ] 21 tests\nSKY_IOS_TEST_COUNT=0\nSKY_IOS_TEST_EXIT=0\n"
        with self.assertRaises(RuntimeError):
            RUNNER.validate_test_result(0, log)

    def test_both_completion_markers_are_required(self):
        for log in ("", "SKY_IOS_TEST_COUNT=21\n", "SKY_IOS_TEST_EXIT=0\n"):
            with self.subTest(log=log):
                with self.assertRaisesRegex(RuntimeError, "both completion markers"):
                    RUNNER.validate_test_result(0, log)

    def test_launcher_failure_or_crash_overrides_successful_host_output(self):
        log = "SKY_IOS_TEST_COUNT=21\nSKY_IOS_TEST_EXIT=0\n"
        for status in (1, -11):
            with self.subTest(status=status):
                with self.assertRaisesRegex(RuntimeError, "failed or crashed"):
                    RUNNER.validate_test_result(status, log)

    def test_failed_native_checks_are_rejected(self):
        log = "SKY_IOS_TEST_COUNT=21\nSKY_IOS_TEST_EXIT=1\n"
        with self.assertRaisesRegex(RuntimeError, "Native tests failed"):
            RUNNER.validate_test_result(0, log)

    def test_stale_positive_output_cannot_hide_a_later_zero_test_run(self):
        log = ("SKY_IOS_TEST_COUNT=21\nSKY_IOS_TEST_EXIT=0\n"
               "SKY_IOS_TEST_COUNT=0\nSKY_IOS_TEST_EXIT=0\n")
        with self.assertRaisesRegex(RuntimeError, "No native tests executed"):
            RUNNER.validate_test_result(0, log)


class IOSSceneRunnerBuildTest(unittest.TestCase):
    def make_args(self, directory):
        root = Path(directory)
        gtest = root / "Google Test"
        dependencies = root / "Simulator Dependencies"
        files = [
            gtest / "CMakeLists.txt", gtest / "src/gtest-all.cc",
            dependencies / "sfmt/include/SFMT.h",
            dependencies / "sfmt/lib/Debug/libSFMT.a",
            dependencies / "rapidjson/include/rapidjson/document.h",
        ]
        for path in files:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("fixture")
        return SimpleNamespace(
            sdk="iphonesimulator", arch="arm64", minimum_ios="13.0", config="Debug",
            third_party_root=dependencies, gtest_root=gtest, jobs=3, timeout=300,
        )

    def test_commands_build_one_test_target_and_preserve_sdk_dependency_paths(self):
        with tempfile.TemporaryDirectory() as temp:
            args = self.make_args(temp)
            directory = Path(temp) / "Work Directory"
            sdk = "/Applications/Test Xcode.app/SDKs/iPhoneSimulator.sdk"
            configure, build = RUNNER.cmake_build_commands(args, directory, sdk)
            self.assertEqual(configure[:3], ["cmake", "-S", str(RUNNER.TEST_FIXTURE)])
            self.assertIn("-DCMAKE_OSX_SYSROOT=" + sdk, configure)
            self.assertIn("-D3RD_PATH=" + str(args.third_party_root.resolve()), configure)
            self.assertIn("-DSKY_IOS_GTEST_ROOT=" + str(args.gtest_root.resolve()), configure)
            self.assertEqual(build, [
                "cmake", "--build", str(directory / "cmake-build"), "--config", "Debug",
                "--target", "IOSPlatformTest", "--parallel", "3",
            ])
            self.assertFalse(any(argument.endswith((".cpp", ".mm", ".cc")) for argument in configure + build))
            self.assertFalse(hasattr(RUNNER, "SOURCES"))

    def test_failed_configure_or_build_cannot_sign_or_return_a_stale_bundle(self):
        for failed_phase in ("configure", "build"):
            with self.subTest(phase=failed_phase), tempfile.TemporaryDirectory() as temp:
                directory = Path(temp)
                args = self.make_args(temp)
                app = directory / "cmake-build/bundle/Debug/SkyIOSPlatformTests.app"
                app.mkdir(parents=True)
                binary = app / "SkyIOSPlatformTests"
                binary.write_text("stale binary")
                (directory / "cmake-build/test-app-Debug.txt").write_text(str(app))
                calls = []

                def command(arguments, **kwargs):
                    calls.append(arguments)
                    if arguments[0] == "xcrun":
                        output = "/Installed/Simulator.sdk" if arguments[-1] == "--show-sdk-path" else "18.6"
                        return SimpleNamespace(returncode=0, stdout=output, stderr="")
                    failed = arguments[0] == "cmake" and (("--build" in arguments) == (failed_phase == "build"))
                    return SimpleNamespace(returncode=1 if failed else 0, stdout="", stderr="deliberate failure" if failed else "")

                with patch.object(RUNNER, "command", side_effect=command):
                    with self.assertRaisesRegex(RuntimeError, "CMake .* failed"):
                        RUNNER.build_test_app(args, directory)
                self.assertFalse(any(arguments[0] == "codesign" for arguments in calls))
                self.assertEqual(binary.read_text(), "stale binary")

    def test_success_uses_the_bundle_reported_by_cmake_and_shared_scene_metadata(self):
        with tempfile.TemporaryDirectory() as temp:
            directory = Path(temp)
            args = self.make_args(temp)
            app = directory / "cmake-build/bundle/Debug/SkyIOSPlatformTests.app"
            app.mkdir(parents=True)
            (app / "SkyIOSPlatformTests").write_text("fresh test binary")
            (directory / "cmake-build/test-app-Debug.txt").write_text(str(app))
            calls = []

            def command(arguments, **kwargs):
                calls.append(arguments)
                output = "/Installed/Simulator.sdk" if arguments[-1] == "--show-sdk-path" else "18.6"
                return SimpleNamespace(returncode=0, stdout=output if arguments[0] == "xcrun" else "", stderr="")

            with patch.object(RUNNER, "command", side_effect=command):
                actual, identifier = RUNNER.build_test_app(args, directory)
            self.assertEqual(actual, app)
            self.assertEqual(calls[-1], ["codesign", "--force", "--sign", "-", str(app)])
            manifest = plistlib.loads((app / "Info.plist").read_bytes())
            source = plistlib.loads(RUNNER.TEST_HOST_PLIST.read_bytes())
            self.assertEqual(manifest["UIApplicationSceneManifest"], source["UIApplicationSceneManifest"])
            self.assertEqual(manifest["CFBundleIdentifier"], identifier)
            self.assertEqual(manifest["CFBundleExecutable"], "SkyIOSPlatformTests")


if __name__ == "__main__":
    unittest.main()
