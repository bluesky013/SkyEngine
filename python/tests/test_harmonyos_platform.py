"""Contracts for HarmonyOS dependency selection, cache isolation and toolchain setup."""

import contextlib
import importlib.util
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("harmonyos_third_party", ROOT / "python/third_party.py")
THIRD_PARTY = importlib.util.module_from_spec(SPEC)
with mock.patch.object(sys, "argv", [str(SPEC.origin)]):
    SPEC.loader.exec_module(THIRD_PARTY)


class HarmonyOSPlatformTest(unittest.TestCase):
    def test_dependency_set_and_versions_match_android(self):
        packages = json.loads((ROOT / "cmake/thirdparty.json").read_text(encoding="utf-8"))["packages"]

        def selected(platform):
            return {p["name"]: (p.get("url"), p.get("tag"), p.get("commit")) for p in packages
                    if not p.get("platforms") or platform in p["platforms"]}

        self.assertEqual(selected("HarmonyOS"), selected("Android"))
        self.assertIn("slang", selected("HarmonyOS"))
        self.assertIn("cpython", selected("HarmonyOS"))

    def test_android_options_are_inherited_before_harmonyos_overrides(self):
        package = {"options": {"a": "base", "b": "base"},
                   "options_Android": {"a": "android", "b": "android"},
                   "options_HarmonyOS": {"b": "native"}}
        self.assertEqual(THIRD_PARTY.package_options(package, "HarmonyOS"), {"a": "android", "b": "native"})
        self.assertEqual(THIRD_PARTY.package_options(package, "Android"), {"a": "android", "b": "android"})
        self.assertEqual(package["options"], {"a": "base", "b": "base"})

    def test_missing_sdk_fails_before_build(self):
        with mock.patch.object(THIRD_PARTY.args, "ohos_sdk", None), \
                mock.patch.dict(os.environ, {"OHOS_SDK_NATIVE": ""}), \
                contextlib.redirect_stderr(io.StringIO()) as diagnostic:
            with self.assertRaises(SystemExit) as result:
                THIRD_PARTY.resolve_harmonyos_sdk()
        self.assertEqual(result.exception.code, 2)
        self.assertIn("HarmonyOS requires", diagnostic.getvalue())

    def test_sdk_fingerprint_invalidates_only_harmonyos_cache(self):
        with tempfile.TemporaryDirectory() as temp:
            native = Path(temp) / "native sdk"
            toolchain = native / "build/cmake/ohos.toolchain.cmake"
            toolchain.parent.mkdir(parents=True)
            toolchain.write_text("# SDK toolchain\n", encoding="utf-8")
            metadata = native / "oh-uni-package.json"
            metadata.write_text('{"version":"6.0.0.47"}', encoding="utf-8")
            (native / "sysroot/usr/include").mkdir(parents=True)
            with mock.patch.object(THIRD_PARTY.args, "ohos_sdk", str(native)):
                package = {"name": "sfmt", "tag": "1.5.4"}
                old_native = THIRD_PARTY.compute_package_key(package, "HarmonyOS")
                old_android = THIRD_PARTY.compute_package_key(package, "Android")
                metadata.write_text('{"version":"6.0.0.48"}', encoding="utf-8")
                self.assertNotEqual(old_native, THIRD_PARTY.compute_package_key(package, "HarmonyOS"))
                self.assertEqual(old_android, THIRD_PARTY.compute_package_key(package, "Android"))
                options = {}
                THIRD_PARTY.fill_harmonyos_config(options)
                self.assertEqual(Path(options["CMAKE_TOOLCHAIN_FILE"]).resolve(), toolchain.resolve())
                self.assertEqual(options["OHOS_ARCH"], "arm64-v8a")
                self.assertEqual(options["OHOS_STL"], "c++_shared")
                self.assertNotIn("ANDROID_ABI", options)
                self.assertNotIn("ANDROID_STL", options)

    def test_incomplete_sdk_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp, \
                mock.patch.object(THIRD_PARTY.args, "ohos_sdk", temp), \
                contextlib.redirect_stderr(io.StringIO()) as diagnostic:
            with self.assertRaises(SystemExit):
                THIRD_PARTY.resolve_harmonyos_sdk()
        self.assertIn("ohos.toolchain.cmake", diagnostic.getvalue())

    def test_cmake_rejects_foreign_abi_stl_and_dependency_output(self):
        with tempfile.TemporaryDirectory() as temp:
            source = Path(temp)
            (source / "CMakeLists.txt").write_text(
                'cmake_minimum_required(VERSION 3.20)\nproject(Contract LANGUAGES NONE)\n'
                'set(CMAKE_SYSTEM_NAME OHOS)\n'
                f'include("{ROOT.as_posix()}/cmake/configuration.cmake")\n', encoding="utf-8")
            for name, options, diagnostic in (
                    ("abi", ["-DOHOS_ARCH=x86_64"], "OHOS_ARCH=arm64-v8a"),
                    ("stl", ["-DOHOS_STL=c++_static"], "OHOS_STL=c++_shared"),
                    ("cache", ["-DSKY_THIRD_PARTY_PLATFORM=Android"], "own third-party output")):
                with self.subTest(name=name):
                    result = subprocess.run(["cmake", "-S", str(source), "-B", str(source / name),
                                             "-DOHOS_ARCH=arm64-v8a", "-DOHOS_STL=c++_shared", *options],
                                            capture_output=True, text=True, timeout=60)
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn(diagnostic, result.stdout + result.stderr)
            (source / "build_metadata.json").write_text(
                '{"sfmt":{"platform":"Android","key":"old"}}', encoding="utf-8")
            result = subprocess.run(["cmake", "-S", str(source), "-B", str(source / "metadata"),
                                     "-DOHOS_ARCH=arm64-v8a", "-DOHOS_STL=c++_shared", f"-D3RD_PATH={source}"],
                                    capture_output=True, text=True, timeout=60)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("was built for Android", result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
