"""Validate native test completion reporting without Xcode or a simulator."""

import importlib.util
import json
from pathlib import Path
import plistlib
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock
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
        for status in (0, 1):
            with self.subTest(status=status), self.assertRaisesRegex(RuntimeError, "Native tests failed"):
                RUNNER.validate_test_result(status, log)

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
            dependencies / "lz4/include/lz4.h",
            dependencies / "lz4/lib/Debug/liblz4.a",
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


class IOSPhysicalDeviceRunnerTest(unittest.TestCase):
    def make_args(self, directory):
        args = IOSSceneRunnerBuildTest().make_args(directory)
        args.sdk = "iphoneos"
        args.arch = "arm64"
        args.device = "paired-ipad"
        args.team_id = "EXISTINGTEAM"
        args.bundle_id = "com.skyengine.platform-tests"
        args.allow_provisioning_updates = False
        args.udid = None
        args.runtime = None
        args.gtest_filter = None
        return args

    def make_app(self, directory, identifier):
        app = directory / "cmake-build/bundle/Debug/SkyIOSPlatformTests.app"
        app.mkdir(parents=True)
        (app / "SkyIOSPlatformTests").write_text("Xcode signed binary")
        (directory / "cmake-build/test-app-Debug.txt").write_text(str(app))
        manifest = plistlib.loads(RUNNER.TEST_HOST_PLIST.read_bytes())
        manifest["CFBundleIdentifier"] = identifier
        manifest["CFBundleExecutable"] = "SkyIOSPlatformTests"
        (app / "Info.plist").write_bytes(plistlib.dumps(manifest))
        return app

    def test_device_configuration_keeps_existing_signing_material_and_updates_opt_in(self):
        with tempfile.TemporaryDirectory() as temp:
            args = self.make_args(temp)
            configure, build = RUNNER.cmake_build_commands(args, Path(temp), "Installed/iPhoneOS.sdk")
            self.assertIn("-DSKY_IOS_TEST_TEAM_ID=EXISTINGTEAM", configure)
            self.assertIn("-DSKY_IOS_TEST_BUNDLE_ID=" + args.bundle_id, configure)
            self.assertIn("-DSKY_IOS_TEST_DEVICE=ON", configure)
            self.assertIn("-DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=NO", configure)
            self.assertIn("-DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED=NO", configure)
            self.assertNotIn("-allowProvisioningUpdates", build)
            args.allow_provisioning_updates = True
            _, build = RUNNER.cmake_build_commands(args, Path(temp), "Installed/iPhoneOS.sdk")
            self.assertEqual(build[-2:], ["--", "-allowProvisioningUpdates"])

    def test_xcode_signed_bundle_is_only_verified_never_rewritten_or_adhoc_signed(self):
        with tempfile.TemporaryDirectory() as temp:
            directory = Path(temp)
            args = self.make_args(temp)
            app = self.make_app(directory, args.bundle_id)
            before = (app / "Info.plist").read_bytes()
            calls = []

            def command(arguments, **kwargs):
                calls.append(arguments)
                output = "/Installed/iPhoneOS.sdk" if arguments[-1] == "--show-sdk-path" else "18.5"
                return SimpleNamespace(returncode=0, stdout=output if arguments[0] == "xcrun" else "", stderr="")

            with patch.object(RUNNER, "command", side_effect=command):
                result = RUNNER.build_test_app(args, directory)
            self.assertEqual(result, (app, args.bundle_id))
            self.assertEqual((app / "Info.plist").read_bytes(), before)
            self.assertEqual(calls[-1], ["codesign", "--verify", "--deep", "--strict", str(app)])
            self.assertFalse(any("--sign" in arguments or "--force" in arguments for arguments in calls))

    def test_signature_failure_and_wrong_bundle_id_cannot_proceed_to_install(self):
        for failure in ("signature", "identifier", "executable", "scene"):
            with self.subTest(failure=failure), tempfile.TemporaryDirectory() as temp:
                directory = Path(temp)
                args = self.make_args(temp)
                app = self.make_app(directory, "com.other.identifier" if failure == "identifier" else args.bundle_id)
                if failure in ("executable", "scene"):
                    manifest = plistlib.loads((app / "Info.plist").read_bytes())
                    if failure == "executable":
                        manifest["CFBundleExecutable"] = "WrongExecutable"
                    else:
                        manifest.pop("UIApplicationSceneManifest")
                    (app / "Info.plist").write_bytes(plistlib.dumps(manifest))
                before = (app / "Info.plist").read_bytes()
                calls = []

                def command(arguments, **kwargs):
                    calls.append(arguments)
                    output = "/Installed/iPhoneOS.sdk" if arguments[-1] == "--show-sdk-path" else "18.5"
                    failed = arguments[0] == "codesign" and failure == "signature"
                    return SimpleNamespace(returncode=1 if failed else 0,
                                           stdout=output if arguments[0] == "xcrun" else "", stderr="")

                with patch.object(RUNNER, "command", side_effect=command):
                    built, error = RUNNER.build_test_app(args, directory)
                self.assertIsNone(built)
                self.assertTrue(error)
                self.assertEqual((app / "Info.plist").read_bytes(), before)
                self.assertFalse(any("devicectl" in arguments for arguments in calls))

    def test_install_failure_is_reported_without_launching(self):
        with tempfile.TemporaryDirectory() as temp:
            directory = Path(temp)
            args = self.make_args(temp)
            installed_apps = {args.bundle_id: "previous manual app kept on device"}

            def device_control(*arguments, **kwargs):
                if arguments[:3] == ("device", "uninstall", "app"):
                    installed_apps.pop(args.bundle_id, None)
                return SimpleNamespace(returncode=1, stdout="", stderr="device locked")

            with patch.object(RUNNER, "devicectl", side_effect=device_control) as control:
                count, error = RUNNER.run_device_tests(args, Path("signed.app"), args.bundle_id, directory)
            self.assertEqual(count, 0)
            self.assertIn("installation failed", error)
            self.assertEqual(control.call_count, 1)
            self.assertEqual(control.call_args.args[:3], ("device", "install", "app"))
            self.assertEqual(installed_apps, {args.bundle_id: "previous manual app kept on device"})
            self.assertIn("Cleanup skipped", (directory / "cleanup.log").read_text())
            self.assertTrue((directory / "install.log").is_file())

    def test_failed_or_empty_device_run_is_cleaned_up_and_cannot_pass(self):
        for returncode, log, expected in (
                (1, "SKY_IOS_TEST_COUNT=21\nSKY_IOS_TEST_EXIT=0\n", "launcher failed"),
                (0, "SKY_IOS_TEST_COUNT=0\nSKY_IOS_TEST_EXIT=0\n", "No native tests"),
                (0, "SKY_IOS_TEST_COUNT=21\nSKY_IOS_TEST_EXIT=1\n", "Native tests failed"),
                (1, "SKY_IOS_TEST_COUNT=21\nSKY_IOS_TEST_EXIT=1\n", "Native tests failed"),
                (0, "no completion markers", "both completion markers")):
            with self.subTest(expected=expected), tempfile.TemporaryDirectory() as temp:
                directory = Path(temp)
                args = self.make_args(temp)
                responses = [SimpleNamespace(returncode=0, stdout="installed", stderr=""),
                             SimpleNamespace(returncode=returncode, stdout=log, stderr=""),
                             SimpleNamespace(returncode=0, stdout="uninstalled", stderr="")]
                with patch.object(RUNNER, "devicectl", side_effect=responses) as control, \
                        patch.object(RUNNER, "simctl") as simulator:
                    count, error = RUNNER.run_device_tests(args, Path("signed.app"), args.bundle_id, directory)
                self.assertEqual(count, 0)
                self.assertIn(expected, error)
                self.assertEqual(control.call_args_list[-1].args[:3], ("device", "uninstall", "app"))
                simulator.assert_not_called()
                self.assertEqual((directory / "test.log").read_text(), log)

    def test_success_uses_console_termination_filter_and_cleanup(self):
        with tempfile.TemporaryDirectory() as temp:
            directory = Path(temp)
            args = self.make_args(temp)
            args.gtest_filter = "IOSNativePlatformTest.*"
            responses = [SimpleNamespace(returncode=0, stdout="installed", stderr=""),
                         SimpleNamespace(returncode=0, stdout="SKY_IOS_TEST_COUNT=24\nSKY_IOS_TEST_EXIT=0\n", stderr=""),
                         SimpleNamespace(returncode=0, stdout="uninstalled", stderr="")]
            with patch.object(RUNNER, "devicectl", side_effect=responses) as control:
                count, error = RUNNER.run_device_tests(args, Path("signed.app"), args.bundle_id, directory)
            self.assertEqual(count, 24)
            self.assertIsNone(error)
            launch = control.call_args_list[1].args
            self.assertEqual(launch[:5], ("device", "process", "launch", "--device", "paired-ipad"))
            self.assertIn("--console", launch)
            self.assertIn("--terminate-existing", launch)
            self.assertEqual(launch[-2:], ("--", "--gtest_filter=IOSNativePlatformTest.*"))
            self.assertTrue((directory / "cleanup.log").is_file())

    def test_native_launch_timeout_preserves_console_and_still_uninstalls(self):
        with tempfile.TemporaryDirectory() as temp:
            directory = Path(temp)
            args = self.make_args(temp)
            responses = [SimpleNamespace(returncode=0, stdout="installed", stderr=""),
                         SimpleNamespace(returncode=1, stdout="partial console", stderr="native command timed out"),
                         SimpleNamespace(returncode=0, stdout="uninstalled", stderr="")]
            with patch.object(RUNNER, "devicectl", side_effect=responses) as control:
                count, error = RUNNER.run_device_tests(args, Path("signed.app"), args.bundle_id, directory)
            self.assertEqual(count, 0)
            self.assertIn("launcher failed", error)
            self.assertEqual(control.call_args_list[-1].args[:3], ("device", "uninstall", "app"))
            self.assertIn("partial console", (directory / "test.log").read_text())

    def test_device_control_uses_native_timeout_without_python_exception_translation(self):
        with patch.object(RUNNER, "command", return_value=SimpleNamespace(returncode=1)) as command:
            result = RUNNER.devicectl("device", "process", "launch", timeout=25)
        self.assertEqual(result.returncode, 1)
        self.assertEqual(command.call_args.args[0][:4], ["xcrun", "devicectl", "--timeout", "25"])
        self.assertIsNone(command.call_args.kwargs["timeout"])
        self.assertIs(command.call_args.kwargs["check"], False)

    def test_cleanup_failure_prevents_a_success_result(self):
        with tempfile.TemporaryDirectory() as temp:
            directory = Path(temp)
            args = self.make_args(temp)
            responses = [SimpleNamespace(returncode=0, stdout="", stderr=""),
                         SimpleNamespace(returncode=0, stdout="SKY_IOS_TEST_COUNT=21\nSKY_IOS_TEST_EXIT=0\n", stderr=""),
                         SimpleNamespace(returncode=1, stdout="", stderr="uninstall failed")]
            with patch.object(RUNNER, "devicectl", side_effect=responses):
                count, error = RUNNER.run_device_tests(args, Path("signed.app"), args.bundle_id, directory)
            self.assertEqual(count, 21)
            self.assertIn("cleanup failed", error)

    def test_manual_mode_launches_without_console_or_test_markers_and_keeps_the_app(self):
        with tempfile.TemporaryDirectory() as temp:
            directory = Path(temp)
            args = self.make_args(temp)
            args.manual_input = True
            responses = [SimpleNamespace(returncode=0, stdout="installed", stderr=""),
                         SimpleNamespace(returncode=0, stdout="Application launched", stderr="")]
            with patch.object(RUNNER, "devicectl", side_effect=responses) as control, \
                    patch.object(RUNNER, "device_test_result") as validate:
                count, error = RUNNER.run_device_tests(args, Path("signed.app"), args.bundle_id, directory)
            self.assertEqual(count, 0)
            self.assertIsNone(error)
            self.assertEqual(control.call_count, 2)
            launch = control.call_args_list[1].args
            self.assertNotIn("--console", launch)
            self.assertEqual(launch[-2:], ("--", "--sky-manual-input"))
            validate.assert_not_called()
            result = json.loads((directory / "device-result.json").read_text())
            self.assertEqual(result["mode"], "manual-input")
            self.assertTrue(result["kept_app"])
            self.assertFalse(result["automated_tests_run"])
            self.assertIsNone(result["test_count"])

    def test_keep_app_preserves_a_completed_automated_run(self):
        with tempfile.TemporaryDirectory() as temp:
            directory = Path(temp)
            args = self.make_args(temp)
            args.keep_app = True
            responses = [SimpleNamespace(returncode=0, stdout="installed", stderr=""),
                         SimpleNamespace(returncode=0, stdout="SKY_IOS_TEST_COUNT=21\nSKY_IOS_TEST_EXIT=0\n", stderr="")]
            with patch.object(RUNNER, "devicectl", side_effect=responses) as control:
                count, error = RUNNER.run_device_tests(args, Path("signed.app"), args.bundle_id, directory)
            self.assertEqual(count, 21)
            self.assertIsNone(error)
            self.assertEqual(control.call_count, 2)
            result = json.loads((directory / "device-result.json").read_text())
            self.assertTrue(result["kept_app"])
            self.assertTrue(result["automated_tests_run"])

    def test_manual_install_or_launch_failure_cannot_report_a_running_app(self):
        for failed_phase in ("install", "launch"):
            with self.subTest(phase=failed_phase), tempfile.TemporaryDirectory() as temp:
                directory = Path(temp)
                args = self.make_args(temp)
                args.manual_input = True
                responses = [SimpleNamespace(returncode=int(failed_phase == "install"), stdout="", stderr="")]
                if failed_phase == "launch":
                    responses.append(SimpleNamespace(returncode=1, stdout="", stderr="device locked"))
                with patch.object(RUNNER, "devicectl", side_effect=responses) as control:
                    count, error = RUNNER.run_device_tests(args, Path("signed.app"), args.bundle_id, directory)
                self.assertEqual(count, 0)
                self.assertTrue(error)
                self.assertEqual(control.call_count, len(responses))
                result = json.loads((directory / "device-result.json").read_text())
                self.assertFalse(result["launched"])
                self.assertEqual(result["kept_app"], failed_phase == "launch")
                self.assertFalse(result["automated_tests_run"])
                self.assertIsNone(result["test_count"])

    def test_manual_build_default_is_reset_for_the_next_automated_invocation(self):
        with tempfile.TemporaryDirectory() as temp:
            args = self.make_args(temp)
            args.manual_input = True
            configure, _ = RUNNER.cmake_build_commands(args, Path(temp), "Installed/iPhoneOS.sdk")
            self.assertIn("-DSKY_IOS_TEST_MANUAL_INPUT=ON", configure)
            args.manual_input = False
            configure, _ = RUNNER.cmake_build_commands(args, Path(temp), "Installed/iPhoneOS.sdk")
            self.assertIn("-DSKY_IOS_TEST_MANUAL_INPUT=OFF", configure)
            args.device = None
            configure, _ = RUNNER.cmake_build_commands(args, Path(temp), "Installed/iPhoneSimulator.sdk")
            self.assertIn("-DSKY_IOS_TEST_DEVICE=OFF", configure)
            self.assertIn("-DSKY_IOS_TEST_MANUAL_INPUT=OFF", configure)

    def test_cli_rejects_device_signing_on_a_simulator_and_wrong_device_arch(self):
        for arguments, expected in ((["--udid", "sim", "--team-id", "TEAM"], "Signing options require --device"),
                                    (["--device", "ipad", "--arch", "x86_64", "--team-id", "TEAM"], "require --arch arm64"),
                                    (["--device", "ipad", "--team-id", "TEAM", "--sdk", "iphonesimulator"], "requires the iphoneos SDK"),
                                    (["--udid", "sim", "--sdk", "iphoneos"], "requires the iphonesimulator SDK"),
                                    (["--device", "ipad", "--team-id", "TEAM", "--manual-input", "--gtest-filter", "IOSNativePlatformTest.*"],
                                     "cannot use --gtest-filter"),
                                    (["--device", "ipad"], "requires an existing --team-id"),
                                    (["--manual-input"], "require --device"),
                                    (["--keep-app"], "require --device")):
            with self.subTest(arguments=arguments), \
                    patch.object(RUNNER.platform, "system", return_value="Darwin"), \
                    patch.object(RUNNER, "build_test_app") as build, \
                    patch.object(RUNNER, "run_tests") as execute, \
                    patch("sys.argv", [str(SCRIPT), "--third-party-root", "unused", *arguments]):
                with self.assertRaises(SystemExit) as result, mock.patch("sys.stderr") as diagnostic:
                    RUNNER.main()
                self.assertEqual(result.exception.code, 2)
                self.assertIn(expected, "".join(call.args[0] for call in diagnostic.write.call_args_list))
                build.assert_not_called()
                execute.assert_not_called()


if __name__ == "__main__":
    unittest.main()
