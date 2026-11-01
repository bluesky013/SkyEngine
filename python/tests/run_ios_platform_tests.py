#!/usr/bin/env python3
r"""Build and run the native iOS platform regressions without the renderer.

Requires Xcode, CMake, an installed iOS Simulator runtime, local GoogleTest sources,
and SFMT/RapidJSON installed for that simulator SDK and architecture. Core and
Framework are built through their production CMake targets. Downloads nothing.
By default a temporary simulator is created and deleted. With --udid, the existing
device's boot state is restored.
Build artifacts, compiler output, and test output remain in --work-dir.

Example:
    python3 python/tests/run_ios_platform_tests.py --runtime iOS-18-6 \
        --third-party-root /path/to/ios-simulator-dependencies
    python3 python/tests/run_ios_platform_tests.py --udid DEVICE_ID \
        --third-party-root /path/to/ios-simulator-dependencies \
        --gtest-filter 'IOSPlatformStateTest.*' --work-dir /tmp/sky-ios-tests
"""

import argparse
import json
import os
import pathlib
import platform
import plistlib
import re
import subprocess
import tempfile
import uuid


REPOSITORY = pathlib.Path(__file__).resolve().parents[2]
TEST_HOST_PLIST = REPOSITORY / "engine/framework/test/platform/ios/Info.plist"
TEST_FIXTURE = REPOSITORY / "python/tests/fixtures/ios_platform"


def command(arguments, *, timeout=300, check=True, env=None):
    return subprocess.run(
        arguments, capture_output=True, text=True, timeout=timeout, check=check, env=env
    )


def simctl(*arguments, timeout=300, check=True, env=None):
    return command(["xcrun", "simctl", *arguments], timeout=timeout, check=check, env=env)


def validate_test_result(returncode, log):
    """Require both a successful host exit and at least one completed check.

    The count excludes skipped tests in the UIKit host, so an empty filter or
    an entirely skipped run cannot produce a false-positive validation result.
    Return the executed count for callers that report the result.
    """
    counts = re.findall(r"SKY_IOS_TEST_COUNT=(\d+)", log)
    exit_codes = re.findall(r"SKY_IOS_TEST_EXIT=(\d+)", log)
    if returncode:
        raise RuntimeError(f"Simulator launcher failed or crashed (exit {returncode})")
    if not counts or not exit_codes:
        raise RuntimeError("Native test host did not report both completion markers")
    if int(exit_codes[-1]) != 0:
        raise RuntimeError(f"Native tests failed (host exit {exit_codes[-1]})")
    count = int(counts[-1])
    if count <= 0:
        raise RuntimeError("No native tests executed; check the gtest filter")
    return count


def select_runtime(requested):
    runtimes = json.loads(simctl("list", "runtimes", "--json").stdout)["runtimes"]
    available = [
        runtime
        for runtime in runtimes
        if runtime.get("isAvailable") and ".iOS-" in runtime["identifier"]
    ]
    if requested:
        available = [
            runtime
            for runtime in available
            if requested in (runtime["identifier"], runtime["name"])
            or runtime["identifier"].endswith("." + requested)
        ]
    if not available:
        raise RuntimeError("No matching installed iOS Simulator runtime; downloads are disabled")
    return max(
        available,
        key=lambda runtime: tuple(int(part) for part in runtime["version"].split(".")),
    )["identifier"]


def cmake_build_commands(args, directory, sdk):
    build = directory / "cmake-build"
    configure = [
        "cmake", "-S", str(TEST_FIXTURE), "-B", str(build), "-G", "Xcode",
        "-DCMAKE_SYSTEM_NAME=iOS", "-DCMAKE_OSX_SYSROOT=" + str(sdk),
        "-DCMAKE_OSX_ARCHITECTURES=" + args.arch,
        "-DCMAKE_OSX_DEPLOYMENT_TARGET=" + args.minimum_ios,
        "-DSKY_IOS_TEST_CONFIG=" + args.config,
        "-D3RD_PATH=" + str(args.third_party_root.resolve()),
        "-DSKY_IOS_GTEST_ROOT=" + str(args.gtest_root.resolve()),
        "-DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=NO",
        "-DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED=NO",
    ]
    compile_target = [
        "cmake", "--build", str(build), "--config", args.config,
        "--target", "IOSPlatformTest", "--parallel", str(args.jobs),
    ]
    return configure, compile_target


def build_test_app(args, directory):
    sdk = command(["xcrun", "--sdk", args.sdk, "--show-sdk-path"]).stdout.strip()
    sdk_version = command(["xcrun", "--sdk", args.sdk, "--show-sdk-version"]).stdout.strip()
    configure, compile_target = cmake_build_commands(args, directory, sdk)
    (directory / "build-info.json").write_text(json.dumps({
        "sdk": args.sdk, "sdk_path": sdk, "sdk_version": sdk_version,
        "arch": args.arch, "minimum_ios": args.minimum_ios,
        "test_host_plist": str(TEST_HOST_PLIST),
        "configuration": args.config, "fixture": str(TEST_FIXTURE),
        "third_party_root": str(args.third_party_root.resolve()),
        "commands": [configure, compile_target],
    }, indent=2))
    gtest = args.gtest_root.resolve()
    if not (gtest / "src/gtest-all.cc").is_file() or not (gtest / "CMakeLists.txt").is_file():
        raise RuntimeError("GoogleTest sources not found; pass --gtest-root (downloads are disabled)")
    dependencies = args.third_party_root.resolve()
    required = [
        dependencies / "sfmt/include/SFMT.h",
        dependencies / "rapidjson/include/rapidjson/document.h",
        dependencies / "sfmt/lib" / args.config / "libSFMT.a",
    ]
    if any(not path.is_file() for path in required):
        raise RuntimeError("Missing simulator SFMT/RapidJSON; pass --third-party-root for the selected SDK/architecture/configuration")
    print(f"Building production Core/Framework and IOSPlatformTest with {args.sdk} ({args.arch})", flush=True)
    for arguments, log_name, label in (
        (configure, "configure.log", "CMake configuration"),
        (compile_target, "compile.log", "CMake build"),
    ):
        result = command(arguments, timeout=max(args.timeout, 300), check=False)
        (directory / log_name).write_text(result.stdout + result.stderr)
        if result.returncode:
            raise RuntimeError(f"{label} failed; see {directory / log_name}")
    app_record = directory / "cmake-build" / f"test-app-{args.config}.txt"
    if not app_record.is_file():
        raise RuntimeError(f"CMake did not report its test bundle: {app_record}")
    app = pathlib.Path(app_record.read_text().strip())
    if not app.is_dir() or not (app / "SkyIOSPlatformTests").is_file():
        raise RuntimeError(f"CMake test bundle is incomplete: {app}")
    identifier = "com.skyengine.platform-tests." + uuid.uuid4().hex
    # Share the exact scene manifest with the CMake FrameworkTest app target.
    manifest = plistlib.loads(TEST_HOST_PLIST.read_bytes())
    scene_manifest = manifest.get("UIApplicationSceneManifest", {})
    configurations = scene_manifest.get("UISceneConfigurations", {}).get("UIWindowSceneSessionRoleApplication", [])
    if (scene_manifest.get("UIApplicationSupportsMultipleScenes") is not False
            or len(configurations) != 1
            or configurations[0].get("UISceneDelegateClassName") != "SkyIOSPlatformTestSceneDelegate"):
        raise RuntimeError(f"Invalid single-scene test host manifest: {TEST_HOST_PLIST}")
    manifest.update({
        "CFBundleExecutable": "SkyIOSPlatformTests",
        "CFBundleIdentifier": identifier,
        "CFBundleName": "SkyIOSPlatformTests",
        "CFBundleDevelopmentRegion": "en",
        "CFBundleVersion": "1",
        "CFBundleShortVersionString": "1.0",
        "CFBundlePackageType": "APPL",
        "LSRequiresIPhoneOS": True,
        "MinimumOSVersion": args.minimum_ios,
        "UIDeviceFamily": [1, 2],
        "UILaunchScreen": {},
    })
    (app / "Info.plist").write_bytes(plistlib.dumps(manifest))
    command(["codesign", "--force", "--sign", "-", str(app)])
    return app, identifier


def run_tests(args, app, identifier, directory):
    udid = args.udid
    created = False
    booted_by_runner = False
    installed = False
    try:
        if udid is None:
            runtime = select_runtime(args.runtime)
            udid = simctl("create", "SkyEngine Platform Tests", args.device_type, runtime).stdout.strip()
            created = True
        devices = json.loads(simctl("list", "devices", "available", "--json").stdout)["devices"]
        selected = next(
            ((runtime, device) for runtime, group in devices.items() for device in group if device["udid"] == udid),
            None,
        )
        device = selected[1] if selected else None
        if device is None:
            raise RuntimeError(f"Simulator {udid} is not installed/available")
        (directory / "simulator-info.json").write_text(json.dumps({
            "udid": udid, "device": device["name"], "runtime": selected[0],
            "initial_state": device["state"], "temporary": created,
        }, indent=2))
        print(f"Running on {device['name']} ({udid}), runtime {selected[0]}", flush=True)
        if device["state"] != "Booted":
            simctl("boot", udid)
            booted_by_runner = True
        simctl("bootstatus", udid, "-b", timeout=args.timeout)
        simctl("install", udid, str(app))
        installed = True
        arguments = ["launch", "--console-pty", udid, identifier]
        if args.gtest_filter:
            arguments.append("--gtest_filter=" + args.gtest_filter)
        environment = os.environ.copy()
        if "GTEST_FILTER" in environment:
            environment["SIMCTL_CHILD_GTEST_FILTER"] = environment["GTEST_FILTER"]
        try:
            result = simctl(*arguments, timeout=args.timeout, check=False, env=environment)
        except subprocess.TimeoutExpired as error:
            def decode(value):
                return value.decode(errors="replace") if isinstance(value, bytes) else (value or "")
            (directory / "test.log").write_text(decode(error.stdout) + decode(error.stderr))
            raise RuntimeError(f"Simulator test launch timed out; partial output: {directory / 'test.log'}") from error
        log = result.stdout + result.stderr
        (directory / "test.log").write_text(log)
        print(log, end="", flush=True)
        try:
            validate_test_result(result.returncode, log)
        except RuntimeError as error:
            raise RuntimeError(f"{error}; see {directory / 'test.log'}") from error
    finally:
        if udid:
            operations = []
            if installed:
                operations.append(("uninstall", udid, identifier))
            if booted_by_runner:
                operations.append(("shutdown", udid))
            if created:
                operations.append(("delete", udid))
            cleanup_log = []
            for operation in operations:
                try:
                    result = simctl(*operation, timeout=60, check=False)
                    cleanup_log.append(f"{operation}: exit={result.returncode}\n{result.stdout}{result.stderr}")
                except (OSError, subprocess.SubprocessError) as error:
                    cleanup_log.append(f"{operation}: {error}\n")
            (directory / "cleanup.log").write_text("\n".join(cleanup_log))


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    simulator = parser.add_mutually_exclusive_group()
    simulator.add_argument("--udid", help="Use an existing simulator and restore its boot state")
    simulator.add_argument("--runtime", help="Installed runtime identifier/name; default: latest iOS")
    parser.add_argument("--device-type", default="com.apple.CoreSimulator.SimDeviceType.iPhone-16-Pro")
    parser.add_argument("--sdk", default="iphonesimulator", help="Installed simulator SDK name or path")
    parser.add_argument("--arch", choices=("arm64", "x86_64"),
                        default="arm64" if platform.machine().lower() in ("arm64", "aarch64") else "x86_64")
    parser.add_argument("--minimum-ios", default="13.0")
    parser.add_argument("--config", choices=("Debug", "Release"), default="Debug")
    parser.add_argument("--third-party-root", type=pathlib.Path, required=True,
                        help="SFMT/RapidJSON install root for the selected simulator SDK, architecture and configuration")
    parser.add_argument("--gtest-root", type=pathlib.Path,
                        default=REPOSITORY / "build_3rd/intermediate/googletest/googletest")
    parser.add_argument("--work-dir", type=pathlib.Path)
    parser.add_argument("--gtest-filter", help="Override the checked-in UIKit main's platform-only default")
    parser.add_argument("--jobs", type=int, default=min(os.cpu_count() or 1, 4))
    parser.add_argument("--timeout", type=int, default=300, help="Simulator operation timeout in seconds")
    args = parser.parse_args()
    if platform.system() != "Darwin":
        parser.error("Native iOS platform builds require macOS, Xcode and an installed iOS Simulator SDK")
    if args.jobs < 1 or args.timeout < 1:
        parser.error("--jobs and --timeout must be positive")
    try:
        minimum_version = tuple(int(part) for part in args.minimum_ios.split("."))
    except ValueError:
        parser.error("--minimum-ios must be a numeric iOS version")
    if minimum_version < (13,):
        parser.error("The UIScene test host requires --minimum-ios 13.0 or newer")
    directory = args.work_dir or pathlib.Path(tempfile.mkdtemp(prefix="skyengine-ios-platform-"))
    directory = directory.resolve()
    directory.mkdir(parents=True, exist_ok=True)
    print(f"Artifacts: {directory}", flush=True)
    try:
        app, identifier = build_test_app(args, directory)
        run_tests(args, app, identifier, directory)
    except (RuntimeError, subprocess.SubprocessError) as error:
        parser.exit(1, str(error) + "\n")
    print(f"UIKit platform tests passed. Logs: {directory}", flush=True)


if __name__ == "__main__":
    main()
