#!/usr/bin/env python3
"""Build and run the native iOS platform regressions without the renderer.

Requires Xcode, an installed iOS Simulator runtime, and the repository's existing
GoogleTest sources. Downloads nothing. By default a temporary simulator is
created and deleted. With --udid, the existing device's boot state is restored.
Build artifacts, compiler output, and test output remain in --work-dir.

Example:
    python3 python/tests/run_ios_platform_tests.py --runtime iOS-18-6
    python3 python/tests/run_ios_platform_tests.py --udid DEVICE_ID \
        --gtest-filter 'IOSPlatformStateTest.*' --work-dir /tmp/sky-ios-tests
"""

import argparse
import concurrent.futures
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
SOURCES = (
    "engine/framework/platform/ios/IOSPlatform.mm",
    "engine/framework/platform/ios/IOSWindow.mm",
    "engine/framework/src/platform/PlatformBase.cpp",
    "engine/framework/src/window/NativeWindow.cpp",
    "engine/framework/src/window/NativeWindowManager.cpp",
    "engine/core/src/environment/Environment.cpp",
    "engine/core/src/file/FileSystem.cpp",
    "engine/core/src/file/FileIO.cpp",
    "engine/core/src/archive/StreamArchive.cpp",
    "engine/core/src/archive/FileArchive.cpp",
    "engine/core/src/archive/MemoryAchive.cpp",
    "engine/core/src/archive/BinaryData.cpp",
    "engine/core/src/memory/Allocator.cpp",
    "engine/core/src/util/String.cpp",
    "engine/framework/test/platform/ios/IOSPlatformStateTest.cpp",
    "engine/framework/test/platform/ios/IOSNativePlatformTest.mm",
    "engine/framework/test/platform/ios/main.mm",
)


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


def build_test_app(args, directory):
    compiler = command(["xcrun", "--sdk", args.sdk, "--find", "clang++"]).stdout.strip()
    sdk = command(["xcrun", "--sdk", args.sdk, "--show-sdk-path"]).stdout.strip()
    sdk_version = command(["xcrun", "--sdk", args.sdk, "--show-sdk-version"]).stdout.strip()
    (directory / "build-info.json").write_text(json.dumps({
        "sdk": args.sdk, "sdk_path": sdk, "sdk_version": sdk_version,
        "arch": args.arch, "minimum_ios": args.minimum_ios,
        "test_host_plist": str(TEST_HOST_PLIST),
    }, indent=2))
    gtest = args.gtest_root.resolve()
    if not (gtest / "src/gtest-all.cc").is_file():
        raise RuntimeError("GoogleTest sources not found; pass --gtest-root (downloads are disabled)")
    flags = [
        "-target", f"{args.arch}-apple-ios{args.minimum_ios}-simulator",
        "-isysroot", sdk, "-std=c++20",
        "-Wno-unused-value", "-Wno-deprecated-declarations",
        "-I" + str(REPOSITORY / "engine/core/include"),
        "-I" + str(REPOSITORY / "engine/framework/include"),
        "-I" + str(gtest / "include"), "-I" + str(gtest),
    ]
    sources = [REPOSITORY / source for source in SOURCES] + [gtest / "src/gtest-all.cc"]

    def compile_source(item):
        index, source = item
        output = directory / f"{index}.o"
        options = ["-fobjc-arc"] if source.suffix == ".mm" else []
        result = command(
            [compiler, *flags, *options, "-c", str(source), "-o", str(output)], check=False
        )
        return output, result, source

    print(f"Building {len(sources)} platform/test sources with {args.sdk} ({args.arch})", flush=True)
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
        compiled = list(pool.map(compile_source, enumerate(sources)))
    (directory / "compile.log").write_text(
        "\n".join(f"{source}\n{result.stdout}{result.stderr}" for _, result, source in compiled)
    )
    if any(result.returncode for _, result, _ in compiled):
        raise RuntimeError(f"Compilation failed; see {directory / 'compile.log'}")

    app = directory / "SkyIOSPlatformTests.app"
    app.mkdir(exist_ok=True)
    result = command(
        [compiler, *flags, *(str(output) for output, _, _ in compiled),
         "-framework", "UIKit", "-framework", "QuartzCore", "-framework", "Foundation",
         "-o", str(app / "SkyIOSPlatformTests")], check=False
    )
    (directory / "link.log").write_text(result.stdout + result.stderr)
    if result.returncode:
        raise RuntimeError(f"Link failed; see {directory / 'link.log'}")
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
    parser.add_argument("--arch", choices=("arm64", "x86_64"), default=platform.machine())
    parser.add_argument("--minimum-ios", default="13.0")
    parser.add_argument("--gtest-root", type=pathlib.Path,
                        default=REPOSITORY / "build_3rd/intermediate/googletest/googletest")
    parser.add_argument("--work-dir", type=pathlib.Path)
    parser.add_argument("--gtest-filter", help="Override the checked-in UIKit main's platform-only default")
    parser.add_argument("--jobs", type=int, default=min(os.cpu_count() or 1, 4))
    parser.add_argument("--timeout", type=int, default=300, help="Simulator operation timeout in seconds")
    args = parser.parse_args()
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
