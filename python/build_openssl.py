#!/usr/bin/env python3
"""Compile the static OpenSSL libraries with CMake and install them.

The Perl-generated headers/sources and the resolved source lists live in a zip
under cmake/patches/ (see python/gen_openssl.py). This step extracts that zip
into a temporary directory and compiles with CMake, so no Perl and no make are
required - only CMake and (for Android) the NDK toolchain, which run natively on
Windows.

Invoked by python/third_party.py through the "openssl" package custom step with
the OpenSSL source clone as the working directory. Install layout:

    <output>/<platform>/openssl/
        include/openssl/...   public headers
        lib/                  libcrypto, libssl, libdefault, libcommon, liblegacy (static)
        ssl/                  openssldir with the bundled CA bundle (cert.pem)
"""

import argparse
import os
import re
import shutil
import subprocess
import sys
import urllib.error
import urllib.request
import zipfile
from pathlib import Path

CA_BUNDLE_URL = "https://curl.se/ca/cacert.pem"

parser = argparse.ArgumentParser(description="Build static OpenSSL for SkyEngine")
parser.add_argument("-p", "--platform", required=True,
                    choices=["Win32", "MacOS-x86", "MacOS-arm", "Android", "IOS", "Linux"])
parser.add_argument("-o", "--output", default=None, help="third-party output root (defaults to <engine>/build_3rd)")
args = parser.parse_args()

SCRIPT_PATH = Path(__file__).resolve()
ENGINE_ROOT = SCRIPT_PATH.parent.parent
OUTPUT_ROOT = Path(args.output).resolve() if args.output else ENGINE_ROOT / "build_3rd"
INSTALL_DIR = OUTPUT_ROOT / args.platform / "openssl"
SOURCE_DIR = Path.cwd()
CMAKE_SOURCE = ENGINE_ROOT / "cmake" / "thirdparty" / "openssl"
PATCH_DIR = ENGINE_ROOT / "cmake" / "patches"
BUILD_DIR = OUTPUT_ROOT / "intermediate" / f"openssl-{args.platform}"
GENERATED_DIR = OUTPUT_ROOT / "intermediate" / "openssl-generated"

# Generated variant (generated/ + sources-<variant>.cmake) to compile.
VARIANTS = {
    "Win32": "win64",
    "MacOS-x86": "darwin",
    "MacOS-arm": "darwin",
    "Linux": "unix",
    "Android": "unix",
    "IOS": "darwin",
}

LIB_SUFFIX = ".lib" if args.platform == "Win32" else ".a"
STATIC_LIB_NAMES = (f"libcrypto{LIB_SUFFIX}", f"libssl{LIB_SUFFIX}")


def run(cmd, cwd=None):
    print("[openssl] " + " ".join(str(c) for c in cmd))
    subprocess.run([str(c) for c in cmd], cwd=str(cwd or SOURCE_DIR), check=True)


def source_version():
    version_file = SOURCE_DIR / "VERSION.dat"
    if not version_file.exists():
        return None
    values = {}
    for line in version_file.read_text(encoding="utf-8", errors="ignore").splitlines():
        if "=" in line:
            key, value = line.split("=", 1)
            values[key.strip()] = value.strip()
    if all(key in values for key in ("MAJOR", "MINOR", "PATCH")):
        return f"{values['MAJOR']}.{values['MINOR']}.{values['PATCH']}"
    return None


def find_generated_zip():
    version = source_version()
    if version:
        candidate = PATCH_DIR / f"openssl_generated_{version}.zip"
        if candidate.exists():
            return candidate
    matches = sorted(PATCH_DIR.glob("openssl_generated_*.zip"))
    if not matches:
        raise RuntimeError(
            f"no generated archive found in {PATCH_DIR}.\n"
            "Create one with: python python/gen_openssl.py --src <openssl-src> --variant <variant>")
    return matches[-1]


def extract_generated():
    archive = find_generated_zip()
    if GENERATED_DIR.exists():
        shutil.rmtree(GENERATED_DIR)
    GENERATED_DIR.mkdir(parents=True)
    with zipfile.ZipFile(archive) as zf:
        zf.extractall(GENERATED_DIR)
    print(f"[openssl] extracted {archive.name} -> {GENERATED_DIR}")
    return GENERATED_DIR


def find_ninja():
    override = os.environ.get("NINJA")
    if override and Path(override).exists():
        return override
    for name in ("ninja", "ninja.exe"):
        found = shutil.which(name)
        if found:
            return found
    # The VS-bundled Ninja is the usual source on Windows; locate VS through vswhere.
    program_files_x86 = os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")
    vswhere = Path(program_files_x86) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
    if vswhere.exists():
        result = subprocess.run([str(vswhere), "-latest", "-products", "*", "-property", "installationPath"],
                                capture_output=True, text=True)
        install = result.stdout.strip().splitlines()[0].strip() if result.stdout.strip() else ""
        if install:
            candidate = Path(install) / "Common7" / "IDE" / "CommonExtensions" / \
                "Microsoft" / "CMake" / "Ninja" / "ninja.exe"
            if candidate.exists():
                return str(candidate)
    return None


def resolve_android_ndk():
    for name in ("ANDROID_NDK_HOME", "ANDROID_NDK_ROOT"):
        value = os.environ.get(name)
        if value and os.path.exists(value):
            return Path(os.path.normpath(value))
    for sdk_name in ("ANDROID_HOME", "ANDROID_SDK_ROOT", "ANDROID_SDK"):
        sdk = os.environ.get(sdk_name)
        if not sdk or not os.path.isdir(sdk):
            continue
        ndk_root = Path(sdk) / "ndk"
        version = os.environ.get("ANDROID_NDK_VERSION")
        if version and (ndk_root / version).exists():
            return ndk_root / version
        if ndk_root.is_dir():
            versions = sorted(ndk_root.iterdir(), key=lambda p: [int(x) for x in re.findall(r"\d+", p.name)])
            if versions:
                return versions[-1]
    raise RuntimeError("Android NDK not found; set ANDROID_NDK_HOME or ANDROID_HOME")


def configure_cmake(variant, generated):
    cmake = ["cmake", "-S", str(CMAKE_SOURCE), "-B", str(BUILD_DIR)]
    if args.platform == "Android":
        ninja = find_ninja()
        if ninja is None:
            raise RuntimeError("ninja not found; required for the Android CMake build")
        ndk = resolve_android_ndk()
        cmake += ["-G", "Ninja", f"-DCMAKE_MAKE_PROGRAM={ninja}",
                  f"-DCMAKE_TOOLCHAIN_FILE={ndk / 'build' / 'cmake' / 'android.toolchain.cmake'}",
                  "-DANDROID_ABI=arm64-v8a", "-DANDROID_PLATFORM=android-31", "-DANDROID_STL=c++_static"]
    cmake += [
        f"-DOPENSSL_SRC={SOURCE_DIR}",
        f"-DOPENSSL_VARIANT={variant}",
        f"-DOPENSSL_GENERATED={generated / 'generated'}",
        f"-DOPENSSL_SOURCES={generated / f'sources-{variant}.cmake'}",
        f"-DOPENSSL_INSTALL_DIR={INSTALL_DIR}",
        f"-DCMAKE_INSTALL_PREFIX={INSTALL_DIR}",
    ]
    if args.platform != "Win32":
        cmake.append("-DCMAKE_BUILD_TYPE=Release")
    run(cmake)
    run(["cmake", "--build", str(BUILD_DIR), "--config", "Release", "--parallel"])
    run(["cmake", "--install", str(BUILD_DIR), "--config", "Release"])


def install_ca_bundle():
    ssl_dir = INSTALL_DIR / "ssl"
    ssl_dir.mkdir(parents=True, exist_ok=True)
    target = ssl_dir / "cert.pem"

    override = os.environ.get("OPENSSL_CA_BUNDLE")
    if override:
        source = Path(override)
        if not source.exists():
            raise RuntimeError(f"OPENSSL_CA_BUNDLE points to a missing file: {source}")
        shutil.copy2(source, target)
        print(f"[openssl] CA bundle installed from {source} -> {target}")
        return

    try:
        with urllib.request.urlopen(CA_BUNDLE_URL, timeout=30) as response:
            data = response.read()
        target.write_bytes(data)
        print(f"[openssl] CA bundle downloaded ({len(data)} bytes) -> {target}")
    except (urllib.error.URLError, OSError) as error:
        print(f"[openssl] warning: could not fetch CA bundle ({error}); "
              f"set OPENSSL_CA_BUNDLE to a local cacert.pem", file=sys.stderr)


def verify_install():
    include = INSTALL_DIR / "include" / "openssl" / "ssl.h"
    if not include.exists():
        raise RuntimeError(f"OpenSSL headers not installed at {include}")
    for name in STATIC_LIB_NAMES:
        library = INSTALL_DIR / "lib" / name
        if not library.exists():
            raise RuntimeError(f"expected static OpenSSL library missing: {library}")


def main():
    variant = VARIANTS.get(args.platform, "unix")
    print(f"[openssl] source {SOURCE_DIR}")
    print(f"[openssl] variant {variant} -> {INSTALL_DIR}")

    generated = extract_generated()
    if not (generated / f"sources-{variant}.cmake").exists():
        raise RuntimeError(
            f"variant '{variant}' is not present in the generated archive.\n"
            f"Regenerate it with: python python/gen_openssl.py --src <openssl-src> --variant {variant}")

    configure_cmake(variant, generated)
    verify_install()
    install_ca_bundle()
    print("[openssl] install complete")


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as error:
        print(f"[openssl] build failed: {error}", file=sys.stderr)
        sys.exit(error.returncode)
    except Exception as error:  # noqa: BLE001
        print(f"[openssl] build failed: {error}", file=sys.stderr)
        sys.exit(1)
