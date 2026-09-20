## Context

CPython 3.13.9 pins OpenSSL **3.0.18** (`PCbuild/get_externals.bat`) and requires OpenSSL >= 1.1.1
(`configure.ac`). The embedded interpreter is statically linked (`add-static-python-embedding`), so `_ssl`/`_hashlib`
must be built into the image and OpenSSL must be linked statically. The engine currently has no OpenSSL
third-party package.

## Goals / Non-Goals

**Goals**
- Provision OpenSSL 3.0.18 statically for Windows, Android, and the Unix desktop targets.
- Make `ssl` and `_hashlib` importable from the statically linked interpreter on those platforms.
- Provide a CA bundle so TLS verification works out of the box.
- Keep the feature behind a build option.

**Non-Goals**
- Alternative TLS backends and platform certificate-store integration.
- Replacing the engine's C++ networking/HTTP/asset layer.

## Decisions

### D1. Match the Python-pinned OpenSSL version
Use OpenSSL **3.0.18** to match CPython 3.13.9. This keeps ABI/feature expectations aligned with the interpreter
and simplifies future Python bumps (the version tracks `get_externals.bat`).

### D2. Static OpenSSL build per platform
OpenSSL 3.0 has no CMake build and ships no generated headers: its Perl `Configure` writes `configuration.h`
and its makefile targets expand the remaining generated headers/sources. Only `configuration.h`,
`crypto/bn_conf.h`, and `crypto/dso_conf.h` depend on the target; the other generated headers depend only on
the OpenSSL version (no `no-*` features). The build therefore separates generation (one-time) from compilation:
- **Generate once** (`python/gen_openssl.py`, needs Perl): run `Configure <target> no-shared no-tests no-asm`,
  execute the generated dofile rules directly (no `make` required), and pack the results plus the resolved
  `libcrypto`/`libssl`/provider source lists into `cmake/patches/openssl_generated_<version>.zip`
  (`generated/common/` platform-independent, `generated/<variant>/` the 3 config headers,
  `generated/defines-<variant>.cmake` the Configure-chosen `-D` flags, `sources-<variant>.cmake`).
  Variants are merged into the archive, so each target family is generated once. `no-asm` avoids assembler
  generation. Three variants are provided: `unix` (Linux/Android), `win64` (Windows), `darwin` (macOS/iOS).
- **Compile** (`cmake/thirdparty/openssl/CMakeLists.txt` + `python/build_openssl.py`): extract the zip into a
  temporary directory (`3RD_PATH/intermediate/openssl-generated`) and build the static `libcrypto`/`libssl` plus
  the `libdefault`/`libcommon`/`liblegacy` provider archives with CMake, installing `include/openssl` +
  libraries into `3RD_PATH/<platform>/openssl`. This needs no Perl and no make, so it runs natively on Windows,
  including the **Windows host -> Android** cross-build via the NDK CMake toolchain.
- Android is a Linux target (`OPENSSL_SYS_LINUX`, LP64, `DSO_DLFCN`), so it shares the `unix` variant; extra
  macros (`__ANDROID_API__`) come from the NDK. The build is a `custom` step in `cmake/thirdparty.json`,
  mirroring the CPython package.

### D3. Wire `_ssl`/`_hashlib` into the static core
Configure CPython with `--with-openssl=<prefix>` and add `_ssl`/`_hashlib` to the builtin set:
- Android/Unix: add them to `Modules/Setup.local` (`*static*`) and link `-lssl -lcrypto`; the CMake core build
  also adds the OpenSSL include dir and static libs.
- Windows: enable the `_ssl`/`_hashlib` static-module builds against the static OpenSSL libraries and link them into
  the static core (the CPython Windows externals ship only a DLL + import library, so the static OpenSSL from D2 is
  required). Because the Windows core does not put these modules in its builtin table, `PythonEngine` must register
  them with `PyImport_AppendInittab("_ssl", PyInit__ssl)` / `"_hashlib"` (guarded by `SKY_PYTHON_SSL`); otherwise
  `import _ssl` searches for a `_ssl.pyd` and fails with "DLL load failed".

### D4. CA bundle
Ship a CA bundle (e.g. `cacert.pem`) and point `ssl` at it (`SSL_CERT_FILE` and/or the default verify path in the
plugin's configuration) so `https://` verification works without a system store. On Android the bundle is packaged
with the app; on desktop it sits next to the executable.

### D5. Build option
Gate the whole feature behind `SKY_PYTHON_SSL` (default off, like the other optional capabilities) so offline/HTTP
-only builds stay lean and do not carry an OpenSSL dependency.

## Risks / Trade-offs

- [OpenSSL build toolchain] -> Windows needs Perl + NASM; Android needs a POSIX shell + `make`. Document and fail
  clearly when prerequisites are missing.
- [Security maintenance] -> OpenSSL versions must track CVEs; pinning to the CPython version simplifies updates.
- [Binary size] -> static OpenSSL adds several MB; acceptable for scripting builds, gated by the option.
- [CA bundle freshness] -> bundle a known-good `cacert.pem` and document how to refresh it.

## Open Questions

- Build OpenSSL with `Configure`/`make` or with its CMake build (experimental in 3.0) on Windows/Android.
- Where the CA bundle is sourced from and how it is refreshed.
- Whether desktop non-Windows targets are in scope now or follow.
