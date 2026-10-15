"""Validate native test completion reporting without Xcode or a simulator."""

import importlib.util
from pathlib import Path
import unittest


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


if __name__ == "__main__":
    unittest.main()
