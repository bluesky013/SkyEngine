## Why

Scripts increasingly need TLS/HTTPS (`urllib`, `http.client`, `requests`, `wss://`). Python's `ssl` and `_hashlib`
modules require OpenSSL, which is not currently a third-party dependency of the engine and is therefore gated out
of the statically embedded interpreter. CPython 3.13.9 pins OpenSSL **3.0.18** (via `PCbuild/get_externals.bat`) and
requires >= 1.1.1, so the embedded OpenSSL should match the Python version.

## What Changes

- Add **OpenSSL 3.0.18** as a third-party package, built **statically** per platform (Windows, Android, and the
  Unix desktop targets), matching the static-core model.
- Cross-compile OpenSSL for Android with the NDK and for the other platforms with their toolchains.
- Wire `_ssl`/`_hashlib` into the statically linked interpreter as builtins (`--with-openssl=<prefix>` plus
  `Modules/Setup.local` entries on Unix/Android; the corresponding static-core integration on Windows).
- Bundle a CA certificate bundle and point `ssl` at it (`SSL_CERT_FILE` / default verify path) so TLS verification
  works out of the box.
- Gate the whole thing behind a build option (e.g. `SKY_PYTHON_SSL`) so HTTP-only or offline builds stay lean.

**Non-goals**: alternative TLS libraries, platform certificate-store integration beyond a bundled CA file, and
replacing the engine's C++ networking/asset HTTP layer.

## Capabilities

### New Capabilities
- `python-ssl`: provisioning and statically linking OpenSSL so the embedded interpreter exposes `ssl`/`_hashlib`.

### Modified Capabilities
<!-- none -->

## Impact

- `cmake/thirdparty.json` (OpenSSL package), `python/gen_openssl.py` (one-time Perl generation, vendored),
  `cmake/thirdparty/openssl/` (generated files, `sources-<variant>.cmake`, and a CMakeLists that compiles the
  static libraries), `python/build_openssl.py` (CMake-only compile + install), `python/build_cpython.py` and
  `cmake/thirdparty/cpython_android/CMakeLists.txt` (link OpenSSL, builtin `_ssl`/`_hashlib`),
  `cmake/thirdparty/Findopenssl.cmake` + `Findcpython.cmake` (static OpenSSL link), `plugins/python`
  (CA bundle, build option).
- Depends on `add-static-python-embedding` (static core) and `add-android-python-runtime` (Android pipeline).
- Introduces an OpenSSL dependency that must track security updates.
