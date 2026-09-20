#!/usr/bin/env python3
"""One-time generator for the vendored OpenSSL sources used by the CMake build.

OpenSSL 3.0 has no CMake build and no generated headers in its source tree: its
Perl `Configure` writes `configuration.h` and its makefile rules expand the rest
with `util/dofile.pl`. This script performs that generation once and packs the
results into `cmake/patches/openssl_generated_<version>.zip`, so normal builds
(including Windows -> Android) need neither Perl nor make: python/build_openssl.py
extracts the zip into a temporary directory and compiles from there.

Run it on a machine with a working Perl (Linux/macOS, or Strawberry Perl on
Windows) once per variant after changing the OpenSSL version or options. Variants
are merged into the archive, so generate each one:

    python python/gen_openssl.py --src <openssl-src> --variant unix   --target linux-x86_64
    python python/gen_openssl.py --src <openssl-src> --variant win64  --target mingw64
    python python/gen_openssl.py --src <openssl-src> --variant darwin --target darwin64-arm64-cc
"""

import argparse
import os
import re
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path

parser = argparse.ArgumentParser(description="Generate the vendored OpenSSL archive")
parser.add_argument("--src", required=True, help="OpenSSL source tree (configured in-source)")
parser.add_argument("--variant", default="unix", help="variant name (unix / win64 / darwin)")
parser.add_argument("--target", default="linux-x86_64", help="OpenSSL Configure target used to generate")
parser.add_argument("--repo", default=None, help="SkyEngine root (defaults to the script's parent/parent)")
args = parser.parse_args()

SCRIPT_PATH = Path(__file__).resolve()
REPO_ROOT = Path(args.repo).resolve() if args.repo else SCRIPT_PATH.parent.parent
SRC = Path(args.src).resolve()
PATCH_DIR = REPO_ROOT / "cmake" / "patches"
STAGE = REPO_ROOT / "build_3rd" / "intermediate" / "openssl-gen-stage"
COMMON_DIR = STAGE / "generated" / "common"
VARIANT_DIR = STAGE / "generated" / args.variant
SOURCES_FRAGMENT = STAGE / f"sources-{args.variant}.cmake"

LIBRARY_VARS = {
    "libcrypto": "OPENSSL_CRYPTO_SOURCES",
    "libssl": "OPENSSL_SSL_SOURCES",
    "providers/libdefault.a": "OPENSSL_DEFAULT_SOURCES",
    "providers/libcommon.a": "OPENSSL_COMMON_SOURCES",
    "providers/liblegacy.a": "OPENSSL_LEGACY_SOURCES",
}

GENERATE_TARGETS = ("generate_crypto_bn", "generate_crypto_objects",
                    "generate_crypto_conf", "generate_crypto_asn1")
GENERATE_OUTPUTS = (
    "crypto/bn/bn_prime.h",
    "crypto/objects/obj_dat.h",
    "crypto/objects/obj_xref.h",
    "include/openssl/obj_mac.h",
    "crypto/conf/conf_def.h",
    "crypto/asn1/charmap.h",
)

CONFIG_HEADERS = (
    "include/openssl/configuration.h",
    "include/crypto/bn_conf.h",
    "include/crypto/dso_conf.h",
)

EXCLUDE_PREFIXES = ("doc/", "apps/", "test/", "fuzz/", "engines/", "tools/", "gost-engine/")


def perl_executable():
    return os.environ.get("PERL", "perl")


def source_version():
    version_file = SRC / "VERSION.dat"
    values = {}
    for line in version_file.read_text(encoding="utf-8", errors="ignore").splitlines():
        if "=" in line:
            key, value = line.split("=", 1)
            values[key.strip()] = value.strip()
    return f"{values['MAJOR']}.{values['MINOR']}.{values['PATCH']}"


def configure():
    subprocess.run([perl_executable(), "Configure", args.target, "no-shared", "no-tests", "no-asm"],
                   cwd=str(SRC), check=True)


def parse_makefile():
    lines = (SRC / "Makefile").read_text(encoding="utf-8", errors="ignore").splitlines()
    variables = {}
    i = 0
    while i < len(lines):
        line = lines[i]
        m = re.match(r"^([A-Za-z_][A-Za-z0-9_]*)\s*[:+]?=(.*)$", line)
        if m and not line.startswith("\t"):
            name, value = m.group(1), m.group(2)
            while value.rstrip().endswith("\\"):
                i += 1
                value = value.rstrip()[:-1] + " " + lines[i]
            variables[name] = value.strip()
        i += 1
    return lines, variables


def expand(text, variables, depth=0):
    if depth > 12:
        return text

    def repl(match):
        return expand(variables.get(match.group(1), ""), variables, depth + 1)

    return re.sub(r"\$\(([A-Za-z_][A-Za-z0-9_]*)\)", repl, text)


def split_cmdline(text):
    tokens, current, quoted = [], "", False
    for char in text:
        if char == '"':
            quoted = not quoted
            continue
        if char.isspace() and not quoted:
            if current:
                tokens.append(current)
                current = ""
            continue
        current += char
    if current:
        tokens.append(current)
    return tokens


def variant_defines(variables):
    """Extract the compile-time -D flags OpenSSL's Configure chose for this target."""
    raw = variables.get("LIB_CPPFLAGS") or variables.get("CNF_CPPFLAGS") or ""
    for _ in range(8):
        raw = re.sub(r"\$\(([A-Za-z_][A-Za-z0-9_]*)\)", lambda m: variables.get(m.group(1), ""), raw)
    defines = []
    for token in split_cmdline(raw):
        if token.startswith("-D") and len(token) > 2:
            define = token[2:]
            name = define.split("=", 1)[0]
            if name in ("OPENSSLDIR", "ENGINESDIR", "MODULESDIR", "__ANDROID_API__"):
                continue
            defines.append(define)
    return sorted(set(defines))


def write_defines(variables):
    defines = variant_defines(variables)
    lines = ["# Generated by python/gen_openssl.py - do not edit.", "set(OPENSSL_VARIANT_DEFINES"]
    for define in defines:
        lines.append(f'    "{define}"')
    lines.append(")")
    (STAGE / "generated" / f"defines-{args.variant}.cmake").write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"[gen] staged defines-{args.variant}.cmake ({len(defines)}: {' '.join(defines[:6])}{' ...' if len(defines) > 6 else ''})")


def collect_rules(lines):
    rules = []
    i = 0
    while i < len(lines):
        line = lines[i]
        if line and not line[0].isspace() and ":" in line and not line.startswith("#") and not line.endswith("\\"):
            targets = [t for t in line.split(":", 1)[0].split() if t]
            recipe = []
            j = i + 1
            while j < len(lines) and lines[j].startswith("\t"):
                recipe.append(lines[j][1:])
                j += 1
            for target in targets:
                rules.append((target, recipe))
            i = j if recipe else i + 1
        else:
            i += 1
    return rules


def run_generation(lines, variables):
    generated = []
    for target, recipe in collect_rules(lines):
        if not recipe or not target.endswith((".h", ".c")):
            continue
        if target.startswith(EXCLUDE_PREFIXES) or "buildtest" in target:
            continue
        generated.append(target)
        commands = [expand(raw.lstrip("@").replace("$@", target), variables) for raw in recipe]
        result = subprocess.run(["bash", "-c", "\n".join(commands)], cwd=str(SRC),
                                capture_output=True, text=True)
        if result.returncode != 0:
            raise RuntimeError(f"generation failed for {target}:\n{result.stderr[-1500:]}")

    rule_map = dict(collect_rules(lines))
    # generate_crypto_objects appends to obj_mac.h, so drop the outputs first for idempotency.
    for name in GENERATE_OUTPUTS:
        output = SRC / name
        if output.exists():
            output.unlink()
    for target in GENERATE_TARGETS:
        recipe = rule_map.get(target)
        if not recipe:
            continue
        commands = [expand(raw.lstrip("@").replace("$@", target), variables) for raw in recipe]
        result = subprocess.run(["bash", "-c", "\n".join(commands)], cwd=str(SRC),
                                capture_output=True, text=True)
        if result.returncode != 0:
            raise RuntimeError(f"generation failed for {target}:\n{result.stderr[-1500:]}")

    generated += [name for name in GENERATE_OUTPUTS if (SRC / name).exists()]
    return sorted(set(generated))


def extract_sources():
    script = (
        'require "./configdata.pm";'
        'my $u=\\%configdata::unified_info;'
        'my %libs = map { $_ => 1 } @{$u->{libraries}};'
        'for my $lib (@ARGV) {'
        '  my @src;'
        '  for my $o (@{$u->{sources}->{$lib}||[]}) {'
        '    next if $libs{$o};'
        '    push @src, @{$u->{sources}->{$o}||[]};'
        '  }'
        '  print "###$lib\\n", join("\\n", @src), "\\n";'
        '}'
    )
    result = subprocess.run([perl_executable(), "-e", script, *LIBRARY_VARS.keys()],
                            cwd=str(SRC), capture_output=True, text=True, check=True)
    lists, current = {}, None
    for line in result.stdout.splitlines():
        if line.startswith("###"):
            current = line[3:].strip()
            lists[current] = []
        elif line.strip() and current is not None:
            lists[current].append(line.strip())
    return lists


def vendor_generated(generated_targets):
    for directory in (COMMON_DIR, VARIANT_DIR):
        if directory.exists():
            shutil.rmtree(directory)
    COMMON_DIR.mkdir(parents=True)
    VARIANT_DIR.mkdir(parents=True)
    generated_set = set()
    # configuration.h is written by Configure itself (not by a build rule).
    for target in sorted(set(generated_targets) | set(CONFIG_HEADERS)):
        source = SRC / target
        if not source.exists():
            continue
        generated_set.add(target)
        destination = (VARIANT_DIR / target) if target in CONFIG_HEADERS else (COMMON_DIR / target)
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)
    return generated_set


def write_sources_fragment(sources, generated_set):
    def entries(lib):
        out = []
        for source in sources.get(lib, []):
            rel = source.replace("\\", "/")
            # Config headers never appear in source lists, so generated sources all live in common/.
            if rel in generated_set:
                out.append("${OPENSSL_GENERATED}/common/" + rel)
            else:
                out.append("${OPENSSL_SRC}/" + rel)
        return out

    lines = ["# Generated by python/gen_openssl.py - do not edit."]
    for lib, var in LIBRARY_VARS.items():
        lines.append(f"set({var}")
        for entry in entries(lib):
            lines.append(f'    "{entry}"')
        lines.append(")")
    SOURCES_FRAGMENT.write_text("\n".join(lines) + "\n", encoding="utf-8")
    counts = {var: len(entries(lib)) for lib, var in LIBRARY_VARS.items()}
    print(f"[gen] staged sources-{args.variant}.cmake {counts}")


def update_archive():
    version = source_version()
    PATCH_DIR.mkdir(parents=True, exist_ok=True)
    archive = PATCH_DIR / f"openssl_generated_{version}.zip"

    entries = {}
    if archive.exists():
        with zipfile.ZipFile(archive) as zf:
            entries = {name: zf.read(name) for name in zf.namelist()}

    for base, _, files in os.walk(STAGE):
        for name in files:
            full = Path(base) / name
            rel = full.relative_to(STAGE).as_posix()
            entries[rel] = full.read_bytes()

    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as zf:
        for name in sorted(entries):
            zf.writestr(name, entries[name])
    print(f"[gen] updated {archive} ({len(entries)} entries)")


def main():
    if STAGE.exists():
        shutil.rmtree(STAGE)
    STAGE.mkdir(parents=True)
    configure()
    lines, variables = parse_makefile()
    generated_targets = run_generation(lines, variables)
    generated_set = vendor_generated(generated_targets)
    sources = extract_sources()
    write_sources_fragment(sources, generated_set)
    write_defines(variables)
    update_archive()
    print(f"[gen] variant '{args.variant}': {len(generated_set)} generated files vendored")


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as error:
        print(f"[gen] failed: {error}", file=sys.stderr)
        sys.exit(error.returncode)
    except Exception as error:  # noqa: BLE001
        print(f"[gen] failed: {error}", file=sys.stderr)
        sys.exit(1)
