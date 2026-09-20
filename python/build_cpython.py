#!/usr/bin/env python3
"""Build CPython from source and install it into <3RD_PATH>/cpython.

Invoked by python/third_party.py through a package "custom" step. The working
directory is the CPython source clone. The install layout is:

    <output>/<platform>/cpython/
        include/    Python.h and friends (plus pyconfig.h)
        libs/       import library and static core library (Windows)
        lib/        shared library (Unix)
        bin/        python3XX.dll (Windows) or libpython3.XX.so/dylib (Unix)
        Lib/        standard library (Windows)
        lib/python3.XX/  standard library (Unix)
        DLLs/       extension modules (Windows)

On Windows a static core library (python3XX_static.lib) is additionally built
from the pythoncore sources with Py_NO_ENABLE_SHARED, for statically linked
embedding. A static core has no sys.winver, so embedding must set
PyConfig.site_import = 0.
"""

import argparse
import os
import re
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path

parser = argparse.ArgumentParser(description="Build CPython for SkyEngine")
parser.add_argument("-p", "--platform", required=True, choices=["Win32", "MacOS-x86", "MacOS-arm", "Android", "IOS", "Linux"])
parser.add_argument("-o", "--output", default=None, help="third-party output root (defaults to <engine>/build_3rd)")
args = parser.parse_args()

SCRIPT_PATH = Path(__file__).resolve()
ENGINE_ROOT = SCRIPT_PATH.parent.parent
OUTPUT_ROOT = Path(args.output).resolve() if args.output else ENGINE_ROOT / "build_3rd"
INSTALL_DIR = OUTPUT_ROOT / args.platform / "cpython"
SOURCE_DIR = Path.cwd()


def run(cmd, cwd=None, env=None):
    printable = " ".join(str(c) for c in cmd)
    print(f"[cpython] {printable}")
    subprocess.run([str(c) for c in cmd], cwd=str(cwd or SOURCE_DIR), check=True, env=env)


def read_version():
    header = SOURCE_DIR / "Include" / "patchlevel.h"
    text = header.read_text(encoding="utf-8", errors="ignore")

    def number(name):
        match = re.search(r"#define\s+" + name + r"\s+(\d+)", text)
        if not match:
            raise RuntimeError(f"cannot find {name} in {header}")
        return int(match.group(1))

    return number("PY_MAJOR_VERSION"), number("PY_MINOR_VERSION"), number("PY_MICRO_VERSION")


def reset_dir(path):
    if path.exists():
        shutil.rmtree(path)
    path.mkdir(parents=True, exist_ok=True)


def copy_headers():
    target = INSTALL_DIR / "include"
    reset_dir(target)
    for item in (SOURCE_DIR / "Include").iterdir():
        dst = target / item.name
        if item.is_dir():
            shutil.copytree(item, dst)
        else:
            shutil.copy2(item, dst)
    pyconfig_candidates = [
        SOURCE_DIR / "PC" / "pyconfig.h",
        SOURCE_DIR / "PCbuild" / "amd64" / "pyconfig.h",
    ]
    for pyconfig in pyconfig_candidates:
        if pyconfig.exists():
            shutil.copy2(pyconfig, target / "pyconfig.h")
            break
    else:
        raise RuntimeError("pyconfig.h not found; build CPython before installing")


def find_visual_studio():
    program_files_x86 = os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")
    vswhere = Path(program_files_x86) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
    if not vswhere.exists():
        return None, None
    result = subprocess.run(
        [str(vswhere), "-latest", "-products", "*",
         "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
         "-property", "installationPath"],
        capture_output=True, text=True, check=True)
    install = result.stdout.strip().splitlines()[0].strip() if result.stdout.strip() else ""
    if not install:
        return None, None
    vcvars = Path(install) / "VC" / "Auxiliary" / "Build" / "vcvars64.bat"
    msbuild = Path(install) / "MSBuild" / "Current" / "Bin" / "MSBuild.exe"
    return vcvars, msbuild


def patch_static_project():
    source = SOURCE_DIR / "PCbuild" / "pythoncore.vcxproj"
    text = source.read_text(encoding="utf-8")
    text = text.replace("<ConfigurationType>DynamicLibrary</ConfigurationType>",
                        "<ConfigurationType>StaticLibrary</ConfigurationType>")
    text = text.replace("<PreprocessorDefinitions>", "<PreprocessorDefinitions>Py_NO_ENABLE_SHARED;")
    target = SOURCE_DIR / "PCbuild" / "pythoncore_static.vcxproj"
    target.write_text(text, encoding="utf-8")
    return target


TIER1_STATIC_MODULES = [
    "unicodedata", "_decimal", "_uuid", "_zoneinfo", "_elementtree", "pyexpat",
    "_bz2", "_lzma", "_socket", "select", "_overlapped", "_queue",
]
# Excluded on Windows static: _ctypes (needs a static libffi; the externals ship only an import
# library) and its ctypes module relies on sys.dllhandle, which is disabled by Py_NO_ENABLE_SHARED.
STATIC_SUPPORT_LIBS = ["liblzma.lib"]

# Built into the static core when a static OpenSSL package is available (SKY_PYTHON_SSL).
SSL_STATIC_MODULES = ["_ssl", "_hashlib"]
OPENSSL_DIR = OUTPUT_ROOT / args.platform / "openssl"


def static_ssl_available():
    include = OPENSSL_DIR / "include" / "openssl" / "ssl.h"
    return include.exists() and (OPENSSL_DIR / "lib").is_dir()


def patch_static_module_project(name):
    source = SOURCE_DIR / "PCbuild" / f"{name}.vcxproj"
    text = source.read_text(encoding="utf-8")
    text = text.replace("<ConfigurationType>DynamicLibrary</ConfigurationType>",
                        "<ConfigurationType>StaticLibrary</ConfigurationType>")
    text = text.replace("<TargetExt>$(PyStdlibPydExt)</TargetExt>", "<TargetExt>.lib</TargetExt>")
    anchor = '<Import Project="$(VCTargetsPath)\\Microsoft.Cpp.targets" />'
    inject = ("  <ItemDefinitionGroup>\n"
              "    <ClCompile>\n"
              "      <PreprocessorDefinitions>Py_NO_ENABLE_SHARED;%(PreprocessorDefinitions)</PreprocessorDefinitions>\n"
              "    </ClCompile>\n"
              "  </ItemDefinitionGroup>\n")
    if anchor in text and "Py_NO_ENABLE_SHARED" not in text:
        text = text.replace(anchor, inject + anchor)
    target = SOURCE_DIR / "PCbuild" / f"{name}_static.vcxproj"
    target.write_text(text, encoding="utf-8")
    return target


def build_static_modules_windows(major, minor):
    vcvars, msbuild = find_visual_studio()
    if vcvars is None or msbuild is None or not vcvars.exists() or not msbuild.exists():
        raise RuntimeError("Visual Studio C++ tools not found; cannot build the static CPython modules")

    modules = list(TIER1_STATIC_MODULES)
    use_ssl = static_ssl_available()
    if use_ssl:
        modules += SSL_STATIC_MODULES
        print(f"[cpython] static OpenSSL found at {OPENSSL_DIR}; building {', '.join(SSL_STATIC_MODULES)}")
    else:
        print("[cpython] static OpenSSL not found; skipping _ssl/_hashlib (build the openssl package for SKY_PYTHON_SSL)")

    arch_dir = SOURCE_DIR / "PCbuild" / "amd64"
    batch = SOURCE_DIR / "PCbuild" / "static_module_build.bat"
    result = {}
    for config, suffix in (("Release", ""), ("Debug", "_d")):
        libraries = []
        for name in modules:
            project = patch_static_module_project(name)
            ssl_props = ""
            if name in SSL_STATIC_MODULES:
                ssl_props = (f' /p:opensslIncludeDir="{OPENSSL_DIR / "include"}"'
                             f' /p:opensslOutDir="{OPENSSL_DIR / "lib"}"'
                             f' /p:SkipCopySSLDLL=1')
            batch.write_text(
                "@echo off\n"
                f'call "{vcvars}" >nul\n'
                f'"{msbuild}" "{project}" /nologo /m /v:minimal /p:Configuration={config} /p:Platform=x64{ssl_props}\n',
                encoding="utf-8")
            run(["cmd", "/c", str(batch)], cwd=SOURCE_DIR)
            library = arch_dir / f"{name}_static{suffix}.lib"
            if not library.exists():
                raise RuntimeError(f"expected static module library missing: {library}")
            libraries.append(library)
        result[config] = libraries
    return result


def build_static_core_windows(major, minor):
    vcvars, msbuild = find_visual_studio()
    if vcvars is None or msbuild is None or not vcvars.exists() or not msbuild.exists():
        raise RuntimeError("Visual Studio C++ tools not found; cannot build the static CPython core")

    project = patch_static_project()
    tag = f"{major}{minor}"
    arch_dir = SOURCE_DIR / "PCbuild" / "amd64"
    batch = SOURCE_DIR / "PCbuild" / "static_core_build.bat"
    outputs = {}
    for config, suffix in (("Release", ""), ("Debug", "_d")):
        target_name = f"python{tag}_static{suffix}"
        batch.write_text(
            "@echo off\n"
            f'call "{vcvars}" >nul\n'
            f'"{msbuild}" "{project}" /nologo /m /v:minimal /p:Configuration={config} '
            f'/p:Platform=x64 /p:TargetName={target_name}\n',
            encoding="utf-8")
        run(["cmd", "/c", str(batch)], cwd=SOURCE_DIR)
        library = arch_dir / f"{target_name}.lib"
        if not library.exists():
            raise RuntimeError(f"expected static core library missing: {library}")
        outputs[config] = library
    return outputs


def build_windows(major, minor):
    build_bat = SOURCE_DIR / "PCbuild" / "build.bat"
    if not build_bat.exists():
        raise RuntimeError(f"PCbuild/build.bat not found at {build_bat}")

    for config in ("Release", "Debug"):
        run(["cmd", "/c", str(build_bat), "-p", "x64", "-c", config], cwd=SOURCE_DIR)

    arch_dir = SOURCE_DIR / "PCbuild" / "amd64"
    tag = f"{major}{minor}"
    release_dll = arch_dir / f"python{tag}.dll"
    release_lib = arch_dir / f"python{tag}.lib"
    debug_dll = arch_dir / f"python{tag}_d.dll"
    debug_lib = arch_dir / f"python{tag}_d.lib"
    for required in (release_dll, release_lib, debug_dll, debug_lib):
        if not required.exists():
            raise RuntimeError(f"expected CPython artifact missing: {required}")

    copy_headers()

    (INSTALL_DIR / "libs" / "Release").mkdir(parents=True, exist_ok=True)
    (INSTALL_DIR / "libs" / "Debug").mkdir(parents=True, exist_ok=True)
    (INSTALL_DIR / "bin" / "Release").mkdir(parents=True, exist_ok=True)
    (INSTALL_DIR / "bin" / "Debug").mkdir(parents=True, exist_ok=True)
    shutil.copy2(release_lib, INSTALL_DIR / "libs" / "Release" / release_lib.name)
    shutil.copy2(debug_lib, INSTALL_DIR / "libs" / "Debug" / debug_lib.name)
    shutil.copy2(release_dll, INSTALL_DIR / "bin" / "Release" / release_dll.name)
    shutil.copy2(debug_dll, INSTALL_DIR / "bin" / "Debug" / debug_dll.name)
    shutil.copy2(release_dll, INSTALL_DIR / release_dll.name)

    static_libs = build_static_core_windows(major, minor)
    shutil.copy2(static_libs["Release"], INSTALL_DIR / "libs" / "Release" / static_libs["Release"].name)
    shutil.copy2(static_libs["Debug"], INSTALL_DIR / "libs" / "Debug" / static_libs["Debug"].name)

    module_libs = build_static_modules_windows(major, minor)
    for config, suffix in (("Release", ""), ("Debug", "_d")):
        module_dir = INSTALL_DIR / "libs" / config / "modules"
        reset_dir(module_dir)
        for library in module_libs[config]:
            shutil.copy2(library, module_dir / library.name)
        for name in STATIC_SUPPORT_LIBS:
            stem, ext = os.path.splitext(name)
            support = arch_dir / f"{stem}{suffix}{ext}"
            if support.exists():
                shutil.copy2(support, module_dir / support.name)

    stdlib = INSTALL_DIR / "Lib"
    reset_dir(stdlib)
    shutil.copytree(SOURCE_DIR / "Lib", stdlib, dirs_exist_ok=True, ignore=shutil.ignore_patterns("test", "__pycache__"))

    dlls = INSTALL_DIR / "DLLs"
    reset_dir(dlls)
    for pyd in arch_dir.glob("*.pyd"):
        shutil.copy2(pyd, dlls / pyd.name)
    if (SOURCE_DIR / "DLLs").exists():
        for item in (SOURCE_DIR / "DLLs").glob("*.pyd"):
            shutil.copy2(item, dlls / item.name)
    for dll in arch_dir.glob("*.dll"):
        if dll.name not in (release_dll.name, debug_dll.name):
            shutil.copy2(dll, dlls / dll.name)


ANDROID_HOST = "aarch64-linux-android"

# Tier 1 extension modules built into the static core for Android (no external/vendored headers needed).
ANDROID_BUILTIN_SETUP = [
    "*static*",
    "array arraymodule.c",
    "_csv _csv.c",
    "_json _json.c",
    "_random _randommodule.c",
    "_struct _struct.c",
    "binascii binascii.c",
    "cmath cmathmodule.c",
    "math mathmodule.c",
    "zlib zlibmodule.c",
    "select selectmodule.c",
    "socket socketmodule.c",
    "unicodedata unicodedata.c",
    "_zoneinfo _zoneinfo.c",
    "_decimal _decimal/_decimal.c",
]

# _ssl/_hashlib are appended to the builtin set when a static OpenSSL is available.
ANDROID_SSL_SETUP = ["_ssl _ssl.c", "_hashlib _hashlib.c"]


def resolve_android_env():
    sdk = None
    for name in ("ANDROID_HOME", "ANDROID_SDK_ROOT", "ANDROID_SDK"):
        value = os.environ.get(name)
        if value:
            candidate = Path(os.path.normpath(value))
            if candidate.exists():
                sdk = candidate
                break

    ndk = None
    for name in ("ANDROID_NDK_HOME", "ANDROID_NDK_ROOT"):
        value = os.environ.get(name)
        if value:
            candidate = Path(os.path.normpath(value))
            if candidate.exists():
                ndk = candidate
                break

    if ndk is None and sdk is not None:
        ndk_root = sdk / "ndk"
        version = os.environ.get("ANDROID_NDK_VERSION")
        if version and (ndk_root / version).exists():
            ndk = ndk_root / version
        elif ndk_root.is_dir():
            versions = sorted(ndk_root.iterdir(), key=lambda p: [int(x) for x in re.findall(r"\d+", p.name)])
            if versions:
                ndk = versions[-1]
    return sdk, ndk


def to_msys_path(path):
    text = str(path)
    if os.name == "nt" and len(text) >= 2 and text[1] == ":":
        return "/" + text[0].lower() + text[2:].replace("\\", "/")
    return text.replace("\\", "/")


def find_bash():
    for name in ("bash", "bash.exe"):
        found = shutil.which(name)
        if found:
            return found
    return None


def find_ninja():
    found = shutil.which("ninja")
    if found:
        return found
    candidate = Path(r"D:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe")
    return str(candidate) if candidate.exists() else None


def android_ndk_bin(ndk):
    prebuilt = ndk / "toolchains" / "llvm" / "prebuilt"
    for entry in sorted(prebuilt.iterdir()):
        if entry.is_dir():
            return entry / "bin"
    raise RuntimeError(f"NDK prebuilt toolchain not found under {prebuilt}")


def parse_makefile_sources(makefile):
    lines = makefile.read_text(encoding="utf-8", errors="ignore").splitlines()
    variables = {}
    i = 0
    while i < len(lines):
        match = re.match(r"^([A-Z0-9_]+)\s*=(.*)$", lines[i])
        if not match:
            i += 1
            continue
        name, value = match.group(1), match.group(2)
        while lines[i].rstrip().endswith("\\") and i + 1 < len(lines):
            value += " " + lines[i + 1]
            i += 1
        variables[name] = re.sub(r"#.*", "", value).split()
        i += 1

    def resolve(name, seen=None):
        seen = seen or set()
        result = []
        for token in variables.get(name, []):
            ref = re.fullmatch(r"\$\(([A-Z0-9_]+)\)", token)
            if ref and ref.group(1) not in seen:
                result += resolve(ref.group(1), seen | {ref.group(1)})
            elif not ref:
                result.append(token)
        return result

    objects = resolve("LIBRARY_OBJS") + resolve("MODOBJS") + resolve("LIBMPDEC_OBJS")
    return sorted({re.sub(r"\.o$", ".c", obj) for obj in objects if obj.endswith(".o")})


def find_frozen_root():
    for candidate in (SOURCE_DIR / "PCbuild" / "obj" / "313_frozen", SOURCE_DIR):
        if (candidate / "Python" / "frozen_modules").is_dir():
            return candidate
    raise RuntimeError("frozen module headers not found; build CPython on the host first or run Tools/build/freeze_modules.py")


def build_android(major, minor):
    sdk, ndk = resolve_android_env()
    if ndk is None:
        raise RuntimeError("Android NDK not found; set ANDROID_NDK_HOME or ANDROID_HOME")
    bash = find_bash()
    if bash is None:
        raise RuntimeError("a POSIX shell (bash) is required to run CPython's configure for the Android cross-build")
    ninja = find_ninja()
    if ninja is None:
        raise RuntimeError("ninja not found; install it or run from a Visual Studio environment")

    tag = f"{major}.{minor}"
    api = 31
    bin_dir = android_ndk_bin(ndk)
    suffix = ".cmd" if os.name == "nt" else ""
    exe = ".exe" if os.name == "nt" else ""
    tool_prefix = f"aarch64-linux-android{api}-clang"

    build_dir = SOURCE_DIR / "android-build"
    build_dir.mkdir(parents=True, exist_ok=True)
    build_triplet = "i686-pc-mingw32" if os.name == "nt" else "x86_64-pc-linux-gnu"

    use_ssl = static_ssl_available()
    openssl_prefix = to_msys_path(OPENSSL_DIR) if use_ssl else None

    # Builtin extension modules are read by makesetup during configure from the build directory.
    setup_lines = list(ANDROID_BUILTIN_SETUP)
    if use_ssl:
        setup_lines += [f"{entry} -I{to_msys_path(OPENSSL_DIR / 'include')}" for entry in ANDROID_SSL_SETUP]
        print(f"[cpython] static OpenSSL found at {OPENSSL_DIR}; enabling {', '.join(ANDROID_SSL_SETUP)}")
    setup_local = build_dir / "Modules" / "Setup.local"
    setup_local.parent.mkdir(parents=True, exist_ok=True)
    setup_local.write_text("\n".join(setup_lines) + "\n", encoding="utf-8")

    openssl_arg = f'--with-openssl="{openssl_prefix}" ' if openssl_prefix else ""
    configure = (
        f'cd "{to_msys_path(build_dir)}" && '
        f'CC="{to_msys_path(bin_dir / (tool_prefix + suffix))}" '
        f'CXX="{to_msys_path(bin_dir / (tool_prefix + "++" + suffix))}" '
        f'AR="{to_msys_path(bin_dir / ("llvm-ar" + exe))}" '
        f'RANLIB="{to_msys_path(bin_dir / ("llvm-ranlib" + exe))}" '
        f'STRIP="{to_msys_path(bin_dir / ("llvm-strip" + exe))}" '
        f'LD="{to_msys_path(bin_dir / ("ld.lld" + exe))}" '
        f'"{to_msys_path(SOURCE_DIR)}/configure" --host={ANDROID_HOST} --build={build_triplet} '
        f'--disable-shared --without-ensurepip --enable-ipv6 '
        f'{openssl_arg}'
        f'--with-build-python="{to_msys_path(sys.executable)}"'
    )
    print("[cpython] configuring for Android (bash)")
    subprocess.run([bash, "-lc", configure], check=True)

    makefile = build_dir / "Makefile"
    if not makefile.exists():
        raise RuntimeError(f"configure did not produce {makefile}")
    sources = parse_makefile_sources(makefile)
    (build_dir / "libsrc.txt").write_text("\n".join(sources), encoding="utf-8")
    print(f"[cpython] android core sources: {len(sources)}")

    frozen_root = find_frozen_root()
    cmake_build = build_dir / "cmake-build"
    cmake_source = ENGINE_ROOT / "cmake" / "thirdparty" / "cpython_android"
    run([
        "cmake", "-S", str(cmake_source), "-B", str(cmake_build), "-G", "Ninja",
        f"-DCMAKE_MAKE_PROGRAM={ninja}",
        f"-DCMAKE_TOOLCHAIN_FILE={ndk / 'build' / 'cmake' / 'android.toolchain.cmake'}",
        "-DANDROID_ABI=arm64-v8a", f"-DANDROID_PLATFORM=android-{api}", "-DANDROID_STL=c++_static",
        f"-DCPYTHON_SRC={SOURCE_DIR}", f"-DCPYTHON_BUILD={build_dir}",
        f"-DCPYTHON_FROZEN={frozen_root}", f"-DCPYTHON_VERSION={tag}",
    ] + ([f"-DOPENSSL_INCLUDE_DIR={OPENSSL_DIR / 'include'}"] if use_ssl else []))
    run(["cmake", "--build", str(cmake_build)])

    include_dir = INSTALL_DIR / "include"
    reset_dir(include_dir)
    for item in (SOURCE_DIR / "Include").iterdir():
        dst = include_dir / item.name
        if item.is_dir():
            shutil.copytree(item, dst, dirs_exist_ok=True)
        else:
            shutil.copy2(item, dst)
    shutil.copy2(build_dir / "pyconfig.h", include_dir / "pyconfig.h")

    lib_dir = INSTALL_DIR / "lib"
    reset_dir(lib_dir)
    built_lib = cmake_build / f"libpython{tag}.a"
    if not built_lib.exists():
        raise RuntimeError(f"static core library not found: {built_lib}")
    shutil.copy2(built_lib, lib_dir / built_lib.name)

    stdlib = SOURCE_DIR / "Lib"
    if stdlib.exists():
        shutil.copytree(stdlib, lib_dir / f"python{tag}", dirs_exist_ok=True,
                        ignore=shutil.ignore_patterns("test", "__pycache__"))

    package_android_stdlib_zip(tag)


ANDROID_STDLIB_EXCLUDE = {"test", "idlelib", "tkinter", "turtledemo", "lib2to3", "__pycache__", "ensurepip"}


def package_android_stdlib_zip(tag):
    stdlib = SOURCE_DIR / "Lib"
    if not stdlib.is_dir():
        return
    zip_path = INSTALL_DIR / "lib" / f"python{tag.replace('.', '')}.zip"
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as archive:
        for file in stdlib.rglob("*"):
            if not file.is_file() or file.suffix in (".pyc", ".pyo"):
                continue
            rel = file.relative_to(stdlib)
            if any(part in ANDROID_STDLIB_EXCLUDE for part in rel.parts):
                continue
            archive.write(file, rel.as_posix())
    print(f"[cpython] android stdlib zip: {zip_path}")


def build_unix(major, minor):
    prefix = INSTALL_DIR / "stage"
    reset_dir(prefix)
    tag = f"{major}.{minor}"

    configure_args = ["./configure", f"--prefix={prefix}", "--enable-shared", "--with-ensurepip=no"]
    if static_ssl_available():
        configure_args.append(f"--with-openssl={OPENSSL_DIR}")
        print(f"[cpython] static OpenSSL found at {OPENSSL_DIR}; enabling ssl/_hashlib")
    run(configure_args, cwd=SOURCE_DIR)
    jobs = str(os.cpu_count() or 4)
    run(["make", f"-j{jobs}"], cwd=SOURCE_DIR)
    run(["make", "install"], cwd=SOURCE_DIR)

    copy_headers()
    (INSTALL_DIR / "lib").mkdir(parents=True, exist_ok=True)
    (INSTALL_DIR / "bin").mkdir(parents=True, exist_ok=True)

    for pattern in (f"libpython{tag}.*",):
        for lib in (prefix / "lib").glob(pattern):
            shutil.copy2(lib, INSTALL_DIR / "lib" / lib.name)
            shutil.copy2(lib, INSTALL_DIR / "bin" / lib.name)
            shutil.copy2(lib, INSTALL_DIR / lib.name)

    python_lib_dir = prefix / "lib" / f"python{tag}"
    if python_lib_dir.exists():
        target = INSTALL_DIR / "lib" / f"python{tag}"
        if target.exists():
            shutil.rmtree(target)
        shutil.copytree(python_lib_dir, target)

    for exe in (prefix / "bin").glob(f"python{tag}*"):
        if exe.is_file():
            shutil.copy2(exe, INSTALL_DIR / "bin" / exe.name)


def main():
    major, minor, micro = read_version()
    print(f"[cpython] source {SOURCE_DIR}")
    print(f"[cpython] version {major}.{minor}.{micro} -> {INSTALL_DIR}")

    INSTALL_DIR.mkdir(parents=True, exist_ok=True)
    if args.platform == "Win32":
        build_windows(major, minor)
    elif args.platform == "Android":
        build_android(major, minor)
    else:
        build_unix(major, minor)

    print("[cpython] install complete")


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as error:
        print(f"[cpython] build failed: {error}", file=sys.stderr)
        sys.exit(error.returncode)
    except Exception as error:  # noqa: BLE001
        print(f"[cpython] build failed: {error}", file=sys.stderr)
        sys.exit(1)
