## 1. OpenSSL third-party

- [x] 1.1 Add an `openssl` package (version `3.0.18`, matching CPython 3.13.9) to `cmake/thirdparty.json` with a custom build step
- [x] 1.2 One-time generate + vendor the Perl-produced headers/sources and source lists (`python/gen_openssl.py` -> `cmake/thirdparty/openssl/generated/`, `sources-<variant>.cmake`)
- [x] 1.3 Compile the static libraries with CMake (`cmake/thirdparty/openssl/CMakeLists.txt`: `libcrypto`, `libssl`, `libdefault`, `libcommon`, `liblegacy`) and install into `3RD_PATH/<platform>/openssl` (`python/build_openssl.py`)
- [x] 1.4 Android: build with the NDK CMake toolchain (verified on a Windows host -> AArch64 static libraries)
- [ ] 1.5 Windows/Linux/macOS: generate the `win64` variant and build on the remaining hosts

## 2. Wire into the static interpreter

- [x] 2.1 Android/Unix: configure the CPython build with `--with-openssl=<prefix>` and add `_ssl`/`_hashlib` to `Modules/Setup.local` (`*static*`)
- [x] 2.2 Build the module sources into `libpython3.XX.a` (OpenSSL includes in `cmake/thirdparty/cpython_android/CMakeLists.txt`)
- [x] 2.3 Windows: enable the `_ssl`/`_hashlib` static builds against the static OpenSSL and link them into the static core
- [x] 2.4 `cmake/thirdparty/Findopenssl.cmake` + `Findcpython.cmake`: link the static OpenSSL libraries when `SKY_PYTHON_SSL` is on

## 3. CA bundle and configuration

- [x] 3.1 Bundle a CA certificate bundle (`cacert.pem`) and ship it with the output (`<prefix>/ssl/cert.pem`)
- [x] 3.2 Point `ssl` at the bundle (`SSL_CERT_FILE` / `SKY_PYTHON_SSL_CERT`) in `PythonEngine`

## 4. Build option

- [x] 4.1 Gate the OpenSSL/SSL integration behind `SKY_PYTHON_SSL` (default off)
- [x] 4.2 Fail configuration clearly when SSL is requested but the OpenSSL package is missing

## 5. Tests and verification

- [x] 5.1 Verify `import ssl, _hashlib` (Windows: `PythonRuntimeTest.TlsBuiltins` passes; Android pending)
- [ ] 5.2 Verify an HTTPS request succeeds with the bundled CA bundle
- [x] 5.3 Verify no shared OpenSSL library is loaded at runtime (Windows: `PythonModule.dll` imports no `libssl`/`libcrypto`)

## 6. Deferred (do together with the Android adaptation)

- [ ] 6.1 Android end-to-end: build the Android CPython core with `_ssl`/`_hashlib` (unix variant + `Setup.local`), link into `PythonModule`, and confirm the builtins import on device/emulator
- [ ] 6.2 5.2 HTTPS request test once Android is wired (uses the bundled `ssl/cert.pem`)
- [ ] 6.3 macOS/Linux: build the `darwin`/`unix` variants and run the same verification
- [ ] 6.4 Confirm `_ssl`/`_hashlib` registration is a no-op on Android (modules already in the core inittab via `Setup.local`)
