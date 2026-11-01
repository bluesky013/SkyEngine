# OHOS Host lifecycle acceptance fixture

This fixture links the real `Framework` and `Core` targets with the official API 26
SDK. It does not compile production sources in a second owner or add production
test interfaces. Run it in its own HAP process, never alongside a game launcher.

The final test library wraps `OH_NativeVSync_Create`, `RequestFrame` and `Destroy`.
Successful create/destroy calls reach the SDK; request callbacks are held by the
fixture and explicitly delivered. Failed calls use a null handle or the SDK's
`NATIVE_ERROR_BINDER_ERROR` result. NativeWindow references are real SDK calls;
the two windows come from real XComponents and remain retained during the run.

The cases cover create failure and retry after Stop, request/factory failure,
foreign component rejection, zero extent, background/foreground, real surface
replacement with stable WindowID, callback delivery during Destroy, a delayed
callback after Stop and across Configure, and release of an old shared surface
lease after the next session. Explicit attempt counters prevent unrelated setup
failures from satisfying failure-injection checks. RAII stops the worker and
releases held leases even if a prerequisite check fails.

Positive transitions wait for worker-owned observations. Negative tick checks
observe for 50 ms; these are bounded acceptance observations, not a proof against
all scheduler interleavings or a replacement for concurrency stress testing.
The 30 assertions drive public Host background/surface events. After they finish,
the `Run 65s surface stress` button starts a separate session using real SDK VSync
callbacks. ArkUI removes and recreates its XComponent every 600 ms; real surface
callbacks attach/detach the Host. EntryAbility foreground/background callbacks
also reach the Host, so Home and returning to the Ability exercise real lifecycle
transitions during this run. The final JSON includes surface, application,
NativeWindow retain/release and VSync create/destroy counts. Sample process RSS,
thread and file-descriptor counts externally; a short run does not establish a
long-term leak bound.

## Build

From the engine root in PowerShell, with existing HarmonyOS third-party output:

```powershell
$nativeSdk = 'D:/Program Files/Huawei/sdk/default/openharmony/native'
$sdkCmake = "$nativeSdk/build-tools/cmake/bin/cmake.exe"
& $sdkCmake -S python/tests/fixtures/harmonyos_host_acceptance -B build/harmonyos-host-acceptance -G Ninja `
  "-DCMAKE_MAKE_PROGRAM=$nativeSdk/build-tools/cmake/bin/ninja.exe" `
  "-DCMAKE_TOOLCHAIN_FILE=$nativeSdk/build/cmake/ohos.toolchain.cmake" `
  -DOHOS_ARCH=arm64-v8a -DOHOS_STL=c++_shared -DCMAKE_BUILD_TYPE=Debug `
  '-D3RD_PATH=D:/Code/Engine/SkyEngine2/build/harmonyos-thirdparty/HarmonyOS' `
  -DSKY_BUILD_TEST=OFF -DSKY_BUILD_SANDBOX=OFF -DSKY_BUILD_TOOL=OFF -DSKY_USE_TRACY=OFF
& $sdkCmake --build build/harmonyos-host-acceptance --parallel 8
```

## Isolated HAP bridge

`prepare_hap.py` copies only the generated HAP template configuration/resources,
the already built test library, and the SDK C++ runtime into a **new** ignored
directory under `build`. It leaves HarmonyPhysics and its generated project
unchanged. The template's local signing configuration and bundle identity are
preserved without printing signing material; installing this HAP therefore
replaces the app that uses that same bundle. Coordinate the device run and
restore the formal game HAP afterwards.

```powershell
python python/tests/fixtures/harmonyos_host_acceptance/prepare_hap.py `
  --template-hap examples/HarmonyPhysics/build/harmonyos `
  --native-library build/harmonyos-host-acceptance/libOHOSHostAcceptance.so `
  --native-sdk $nativeSdk --output build/harmonyos-host-acceptance-hap
$env:DEVECO_SDK_HOME = 'D:/Program Files/Huawei/sdk'
$env:JAVA_HOME = 'D:/Program Files/Huawei/DevEco Studio/jbr'
$cltBin = 'D:/Program Files/Huawei/command-line-tools/bin'
Push-Location build/harmonyos-host-acceptance-hap
& "$cltBin/ohpm.bat" install --all
& "$cltBin/hvigorw.bat" --mode module -p product=default -p module=entry@default -p buildMode=debug assembleHap --no-daemon
Pop-Location
```

After installing and starting the signed HAP through the normal device workflow,
the page creates two real SDK surfaces, runs once, and displays JSON with
`passed`/`failed` counts. `OHOSHostAcceptance` hilog lines name every assertion.
Treat prerequisite timeouts and any nonzero `failed` count as failures. Building
the fixture or HAP alone does not complete device acceptance.
