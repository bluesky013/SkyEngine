#!/usr/bin/env python3
"""SkyEngine 工程工具：创建工程 / 生成 IDE 工程 / 打包。

设计（对齐主流引擎）：
- 工程布局：`native/` 是唯一原生代码库（游戏模块 + 平台 launcher 入口 + 独立 CMake 入口）；
  生成物统一在 `<Project>/build/<platform>/`。
- 模板 engine/templates/{game,launcher,ide}/ 是唯一来源；本脚本只做令牌替换。
- 打包由工程内的导出预设驱动：export_presets.json（入库）+ export_credentials.json（不入库）。
  凭据解析顺序：环境变量 > export_credentials.json。

子命令：
- create   从 templates/game 建工程，并把 templates/launcher/<platform> 铺到 native/<platform>
- generate 按预设生成平台工程：android → templates/ide/android 到 build/android；ios/macos → cmake --preset
- package  generate + 构建 + 收集产物（android 出 APK；ios/macos 走 cmake --preset/--build）
- apple-distribute 通过 Xcode 归档、验签并使用外部 export-options 本地导出 Apple 应用
"""

import argparse
import hashlib
import json
import os
import posixpath
import plistlib
import re
import shutil
import stat
import subprocess
import sys
import zipfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from shader_project import cook_project as cook_project_shaders, project_modules, stage_products as stage_shader_products
from module_resources import stage as stage_module_sources

REPO_ROOT = Path(__file__).resolve().parent.parent

TOKEN_KEYS = [
    '{{PROJECT_NAME}}',
    '{{PROJECT_DISPLAY_NAME}}',
    '{{APPLICATION_ID}}',
    '{{ANDROID_NAMESPACE}}',
    '{{ANDROID_NAMESPACE_PATH}}',
    '{{LAUNCHER_LIB_NAME}}',
    '{{VERSION_NAME}}',
    '{{VERSION_CODE}}',
    '{{COMPILE_SDK}}',
    '{{MIN_SDK}}',
    '{{TARGET_SDK}}',
    '{{NDK_VERSION}}',
    '{{ANDROID_ABI}}',
    '{{MODULE_PACKAGING}}',
    '{{ANDROID_CMAKE_ARGUMENTS}}',
    '{{ANDROID_ORIENTATION}}',
    '{{DEVELOP_MODE}}',
    '{{ANDROID_KEYSTORE_PATH}}',
    '{{ANDROID_KEYSTORE_ALIAS}}',
    '{{ANDROID_STORE_PASSWORD}}',
    '{{ANDROID_KEY_PASSWORD}}',
]

LAUNCHER_PLATFORMS = ['android', 'ios', 'macos', 'windows', 'linux', 'harmonyos']

ORIENTATIONS = ('landscape', 'portrait', 'auto_rotation', 'auto_rotation_landscape', 'auto_rotation_portrait')
ANDROID_ORIENTATIONS = dict(zip(ORIENTATIONS, ('landscape', 'portrait', 'fullSensor', 'sensorLandscape', 'sensorPortrait')))
IOS_ORIENTATIONS = {
    'landscape': ['UIInterfaceOrientationLandscapeLeft'],
    'portrait': ['UIInterfaceOrientationPortrait'],
    'auto_rotation': ['UIInterfaceOrientationPortrait', 'UIInterfaceOrientationPortraitUpsideDown',
                      'UIInterfaceOrientationLandscapeLeft', 'UIInterfaceOrientationLandscapeRight'],
    'auto_rotation_landscape': ['UIInterfaceOrientationLandscapeLeft', 'UIInterfaceOrientationLandscapeRight'],
    'auto_rotation_portrait': ['UIInterfaceOrientationPortrait', 'UIInterfaceOrientationPortraitUpsideDown'],
}


def preset_orientation(preset):
    value = preset.get('orientation', 'landscape')
    if value == 'default':
        value = 'landscape'
    if not isinstance(value, str) or value not in ORIENTATIONS:
        raise SystemExit('Invalid orientation: expected default, ' + ', '.join(ORIENTATIONS))
    if preset.get('platform') not in ('android', 'harmonyos', 'ios') and value != 'landscape':
        raise SystemExit('orientation is only supported on android, harmonyos and ios')
    return value

# 环境变量覆盖（凭据优先级最高）。
ENV_ANDROID = {
    'keystorePath': 'SKY_ANDROID_KEYSTORE',
    'storePassword': 'SKY_ANDROID_STORE_PASSWORD',
    'keyAlias': 'SKY_ANDROID_KEY_ALIAS',
    'keyPassword': 'SKY_ANDROID_KEY_PASSWORD',
}
ENV_APPLE = {
    'teamId': 'SKY_APPLE_TEAM_ID',
    'signingIdentity': 'SKY_APPLE_SIGNING_IDENTITY',
    'provisioningProfile': 'SKY_APPLE_PROVISIONING_PROFILE',
}


def sanitize_identifier(name):
    ident = ''.join(ch for ch in name if ch.isalnum() or ch == '_')
    if not ident or ident[0].isdigit():
        ident = 'Game' + ident
    return ident


def sanitize_package(application_id):
    parts = [p for p in application_id.split('.') if p]
    clean = []
    for part in parts:
        seg = ''.join(ch for ch in part if ch.isalnum() or ch == '_')
        if not seg or seg[0].isdigit():
            seg = 'p' + seg
        clean.append(seg)
    return '.'.join(clean) if clean else 'com.skyengine.game'


def substitute_text(text, tokens):
    for key in TOKEN_KEYS:
        text = text.replace(key, tokens[key])
    return text


def instantiate_template(template_dir, out_dir, tokens):
    if not template_dir.is_dir():
        raise SystemExit('模板目录不存在: {}'.format(template_dir))
    out_dir.mkdir(parents=True, exist_ok=True)
    for src in template_dir.rglob('*'):
        if not src.is_file():
            continue
        rel = src.relative_to(template_dir).as_posix()
        dst = out_dir / substitute_text(rel, tokens)
        dst.parent.mkdir(parents=True, exist_ok=True)
        # 二进制安全：模板可能包含 gradle-wrapper.jar。
        data = src.read_bytes()
        for key in TOKEN_KEYS:
            data = data.replace(key.encode('ascii'), tokens[key].encode('utf-8'))
        dst.write_bytes(data)
        if dst.name == 'gradlew':
            dst.chmod(dst.stat().st_mode | 0o111)
    print('生成: {}'.format(out_dir))


def load_json(path, default=None):
    path = Path(path)
    if not path.is_file():
        return default
    with path.open('r', encoding='utf-8') as handle:
        return json.load(handle)


def load_descriptor(project_arg):
    path = Path(project_arg).resolve()
    if path.is_dir():
        candidates = sorted(path.glob('*.skyproj'))
        if not candidates:
            raise SystemExit('未找到 .skyproj: {}'.format(path))
        path = candidates[0]
    if not path.is_file():
        raise SystemExit('工程描述符不存在: {}'.format(path))
    return path, load_json(path, {})


def find_preset(project_dir, name):
    presets = load_json(project_dir / 'export_presets.json', [])
    defaults = {}
    if isinstance(presets, dict):
        defaults = presets.get('defaults', {}) or {}
        presets = presets.get('presets', [])
    for preset in presets:
        if preset.get('name') == name:
            merged = dict(defaults)
            merged.update(preset)
            return merged
    raise SystemExit('未找到导出预设: {} (于 {})'.format(name, project_dir / 'export_presets.json'))


def env_or(value, env_name):
    return os.environ.get(env_name) or value or ''


def android_credentials(credentials, keystore_name):
    android = (credentials.get('android') or {})
    entry = android.get(keystore_name) or {}
    return {field: env_or(entry.get(field), env) for field, env in ENV_ANDROID.items()}


def apple_credentials(credentials):
    apple = credentials.get('apple') or {}
    return {field: env_or(apple.get(field), env) for field, env in ENV_APPLE.items()}


def base_tokens(descriptor):
    """身份令牌（与平台无关）。"""
    name = descriptor.get('name') or 'Game'
    identifier = sanitize_identifier(name)
    settings = descriptor.get('settings') or {}
    namespace = sanitize_package('com.skyengine.' + identifier.lower())
    return {
        '{{PROJECT_NAME}}': identifier,
        '{{PROJECT_DISPLAY_NAME}}': name,
        '{{ANDROID_NAMESPACE}}': namespace,
        '{{ANDROID_NAMESPACE_PATH}}': namespace.replace('.', '/'),
        '{{LAUNCHER_LIB_NAME}}': identifier,
        '{{VERSION_NAME}}': str(settings.get('versionName') or '1.0'),
        '{{VERSION_CODE}}': str(settings.get('versionCode') or 1),
        '{{COMPILE_SDK}}': '34',
        '{{MIN_SDK}}': '29',
        '{{TARGET_SDK}}': '33',
        '{{NDK_VERSION}}': '28.1.13356709',
        '{{ANDROID_ABI}}': 'arm64-v8a',
        '{{MODULE_PACKAGING}}': 'dynamic',
        '{{ANDROID_CMAKE_ARGUMENTS}}': "'-DSKY_MODULE_PACKAGING=dynamic'",
        '{{ANDROID_ORIENTATION}}': 'landscape',
        '{{DEVELOP_MODE}}': 'false',
        '{{APPLICATION_ID}}': namespace,
        '{{ANDROID_KEYSTORE_PATH}}': '',
        '{{ANDROID_KEYSTORE_ALIAS}}': '',
        '{{ANDROID_STORE_PASSWORD}}': '',
        '{{ANDROID_KEY_PASSWORD}}': '',
    }


def build_tokens(descriptor, preset, credentials):
    tokens = base_tokens(descriptor)
    tokens['{{ANDROID_ORIENTATION}}'] = ANDROID_ORIENTATIONS[preset_orientation(preset)]
    settings = descriptor.get('settings') or {}

    tokens['{{APPLICATION_ID}}'] = preset.get('bundleId') or ''
    tokens['{{VERSION_NAME}}'] = str(preset.get('versionName') or settings.get('versionName') or '1.0')
    tokens['{{VERSION_CODE}}'] = str(preset.get('versionCode') or settings.get('versionCode') or 1)
    tokens['{{COMPILE_SDK}}'] = str(preset.get('compileSdk', 34))
    tokens['{{MIN_SDK}}'] = str(preset.get('minSdk', 29))
    tokens['{{TARGET_SDK}}'] = str(preset.get('targetSdk', 33))
    tokens['{{NDK_VERSION}}'] = str(preset.get('ndkVersion', '28.1.13356709'))
    tokens['{{ANDROID_ABI}}'] = (preset.get('abis') or ['arm64-v8a'])[0]
    tokens['{{MODULE_PACKAGING}}'] = str(preset.get('packing', 'dynamic'))
    tokens['{{DEVELOP_MODE}}'] = str(development_preset(preset)).lower()

    if preset.get('platform') == 'android':
        android = android_credentials(credentials, preset.get('keystore', 'release'))
        tokens['{{ANDROID_KEYSTORE_PATH}}'] = android['keystorePath']
        tokens['{{ANDROID_KEYSTORE_ALIAS}}'] = android['keyAlias']
        tokens['{{ANDROID_STORE_PASSWORD}}'] = android['storePassword']
        tokens['{{ANDROID_KEY_PASSWORD}}'] = android['keyPassword']
    return tokens


def gradle_wrapper_name():
    return 'gradlew.bat' if os.name == 'nt' else 'gradlew'


def package_android(out_dir, project_dir, tokens, variant):
    wrapper = out_dir / gradle_wrapper_name()
    if not wrapper.is_file():
        raise SystemExit('缺少 gradle wrapper: {}'.format(wrapper))
    task = ':app:assemble' + ('Release' if variant == 'release' else 'Debug')
    cmd = [str(wrapper), '-p', str(out_dir), task]
    print('构建: {}'.format(' '.join(cmd)))
    result = subprocess.run(cmd)
    if result.returncode != 0:
        raise SystemExit('gradle 构建失败 (exit {})'.format(result.returncode))

    apk = out_dir / 'app' / 'build' / 'outputs' / 'apk' / variant / 'app-{}.apk'.format(variant)
    if not apk.is_file():
        raise SystemExit('未找到 APK: {}'.format(apk))
    dest = out_dir / '{}-{}.apk'.format(tokens['{{PROJECT_NAME}}'], variant)
    shutil.copy2(apk, dest)
    print('产物: {}'.format(dest))
    return dest


def preset_cmake_args(preset):
    args = []
    for key, value in (preset.get('cacheVariables') or {}).items():
        if isinstance(value, bool):
            value = 'ON' if value else 'OFF'
        args.append('-D{}={}'.format(key, value))
    for field, variable in (('compileDefinitions', 'SKY_EXTRA_CXX_DEFINITIONS'),
                            ('compileFlags', 'SKY_EXTRA_CXX_FLAGS')):
        values = preset.get(field) or []
        if values:
            args.append('-D{}={}'.format(variable, ';'.join(values)))
    return args + list(preset.get('extraConfigureArgs') or [])


def development_preset(preset):
    variables = dict(preset.get('cacheVariables') or {})
    for argument in preset.get('extraConfigureArgs') or []:
        if argument.startswith('-D') and '=' in argument:
            name, value = argument[2:].split('=', 1)
            variables[name.split(':', 1)[0]] = value
    return any(str(variables.get(key, '')).upper() in ('1', 'ON', 'TRUE', 'YES')
               for key in ('SKY_DEVELOP', 'SKY_BUILD_TOOL', 'SKY_BUILD_SANDBOX'))


def android_cmake_args(engine, preset):
    args = ['-DSKY_ENGINE_ROOT=' + str(engine),
            '-DSKY_MODULE_PACKAGING=' + str(preset.get('packing', 'dynamic')),
            '-DSKY_SHADER_DISTRIBUTION=' + str(preset.get('name') or preset.get('platform', 'android'))]
    args += preset_cmake_args(preset)
    args.append('-DSKY_DEPLOY_RUNTIME_RESOURCES=OFF')
    if os.environ.get('SKY_3RD_PATH'):
        args.append('-D3RD_PATH=' + os.environ['SKY_3RD_PATH'])
    # Groovy single-quoted strings preserve dollar signs in paths/definitions.
    return ', '.join("'" + arg.replace('\\', '\\\\').replace("'", "\\'") + "'" for arg in args)


def apple_cmake_args(preset, credentials, default_bundle_id):
    """Preset -> cmake cache args (bundle id, macOS .app, signing, extra options)."""
    args = []
    bundle_id = preset.get('bundleId') or default_bundle_id
    if bundle_id:
        args.append('-DSKY_BUNDLE_ID=' + str(bundle_id))
    if preset.get('platform') == 'macos' and preset.get('bundle'):
        args.append('-DSKY_MACOS_BUNDLE=ON')
    args.append('-DSKY_MODULE_PACKAGING=' + str(preset.get('packing', 'dynamic')))
    args.append('-DSKY_SHADER_DISTRIBUTION=' + str(preset.get('name') or preset.get('platform', 'macos')))

    apple = apple_credentials(credentials)
    team = preset.get('team') or apple['teamId']
    identity = preset.get('signingIdentity') or apple['signingIdentity']
    profile = preset.get('provisioningProfile') or apple['provisioningProfile']
    args += ['-DSKY_APPLE_TEAM_ID=' + team, '-DSKY_APPLE_SIGNING_IDENTITY=' + identity,
             '-DSKY_APPLE_PROVISIONING_PROFILE=' + profile]

    if preset.get('signing', 'auto') == 'off':
        args += ['-DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=NO', '-DCODE_SIGNING_REQUIRED=NO',
                 '-DCODE_SIGNING_ALLOWED=NO']

    return args + preset_cmake_args(preset)


def apple_build_configuration(preset):
    configurations = {'debug': 'Debug', 'release': 'Release'}
    variant = preset.get('variant', 'release')
    configuration = configurations.get(variant) if isinstance(variant, str) else None
    if configuration is None:
        print('Apple variant must be debug or release', file=sys.stderr)
        return None
    arguments = preset.get('extraBuildArgs') or []
    if not isinstance(arguments, list) or any(
            not isinstance(argument, str) or argument == '--config' or argument.startswith('--config=')
            or argument == '--preset' or argument.startswith('--preset=') for argument in arguments):
        print('Select the Apple build configuration with variant, not extraBuildArgs', file=sys.stderr)
        return None
    return configuration


def apple_export_versions(descriptor, preset):
    settings = descriptor.get('settings') or {}
    name = preset.get('versionName', settings.get('versionName', '1.0'))
    code = preset.get('versionCode', settings.get('versionCode', 1))
    if not isinstance(name, str) or re.fullmatch(r'[0-9]+(?:\.[0-9]+){0,2}', name) is None:
        print('Apple versionName must contain one to three numeric components', file=sys.stderr)
        return None
    if not isinstance(code, int) or isinstance(code, bool) or code <= 0:
        print('Apple versionCode must be a positive integer', file=sys.stderr)
        return None
    return name, str(code)


def prepare_macos_metadata(project_dir, tokens):
    source = project_dir / 'native/macos'
    launcher = source / 'CMakeLists.txt'
    if not launcher.is_file() or '${SKY_MACOS_INFO_PLIST}' not in launcher.read_text(encoding='utf-8'):
        print('Update native/macos/CMakeLists.txt to use ${SKY_MACOS_INFO_PLIST} for MACOSX_BUNDLE_INFO_PLIST',
              file=sys.stderr)
        return None
    manifest_path = source / 'Info.plist'
    if not manifest_path.is_file():
        print('Missing project-owned macOS Info.plist', file=sys.stderr)
        return None
    with manifest_path.open('rb') as handle:
        manifest = plistlib.load(handle)
    if not isinstance(manifest, dict):
        print('macOS Info.plist must contain a dictionary', file=sys.stderr)
        return None
    manifest['CFBundleShortVersionString'] = tokens.get('{{VERSION_NAME}}', '1.0')
    manifest['CFBundleVersion'] = tokens.get('{{VERSION_CODE}}', '1')
    output = project_dir / 'build/macos/export/Info.plist'
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(plistlib.dumps(manifest, sort_keys=False))
    return '-DSKY_MACOS_INFO_PLIST=' + output.resolve().as_posix()


def prepare_harmonyos_project(engine, project_dir, preset, tokens, credentials):
    out_dir = project_dir / 'build' / 'harmonyos'
    source = project_dir / 'native' / 'harmonyos' / 'entry'
    if not source.is_dir():
        raise SystemExit('Missing project-owned HarmonyOS launcher: ' + str(source))
    import json5
    manifest_path = source / 'src/main/module.json5'
    manifest, error, _ = json5.parse(manifest_path.read_text(encoding='utf-8'), allow_duplicate_keys=False)
    if error:
        raise SystemExit('Invalid HarmonyOS module.json5: ' + error)
    module = manifest.get('module') if isinstance(manifest, dict) else None
    abilities = module.get('abilities') if isinstance(module, dict) else None
    main = module.get('mainElement') if isinstance(module, dict) else None
    if not isinstance(main, str) or not isinstance(abilities, list):
        raise SystemExit('HarmonyOS module.json5 requires mainElement and abilities')
    entries = [entry for entry in abilities if isinstance(entry, dict) and entry.get('name') == main]
    if len(entries) != 1:
        raise SystemExit('HarmonyOS mainElement must identify exactly one ability')
    entries[0]['orientation'] = preset_orientation(preset)
    abis = preset.get('abis') or ['arm64-v8a']
    if abis != ['arm64-v8a']:
        raise SystemExit('HarmonyOS currently requires abis=[arm64-v8a]')
    compile_sdk = preset.get('compileSdk', 26)
    target_sdk = preset.get('targetSdk', 26)
    min_sdk = preset.get('minSdk', 26)
    if not all(isinstance(v, int) and v >= 26 for v in (compile_sdk, target_sdk, min_sdk)) or not compile_sdk >= target_sdk >= min_sdk:
        raise SystemExit('HarmonyOS requires compileSdk >= targetSdk >= minSdk >= 26')
    if preset.get('variant', 'debug') not in ('debug', 'release'):
        raise SystemExit('HarmonyOS variant must be debug or release')
    if preset.get('extraBuildArgs'):
        raise SystemExit('HarmonyOS extraBuildArgs are not supported by the Hvigor packaging entry')
    signing = preset.get('signing', 'off')
    if signing not in ('auto', 'off'):
        raise SystemExit('HarmonyOS signing must be auto or off')
    product = {'name': 'default', 'compileSdkVersion': '{}.0.0'.format(compile_sdk),
               'targetSdkVersion': '{}.0.0'.format(target_sdk), 'compatibleSdkVersion': '{}.0.0'.format(min_sdk),
               'runtimeOS': 'HarmonyOS',
               'buildOption': {'strictMode': {'caseSensitiveCheck': True, 'useNormalizedOHMUrl': True}}}
    configs = []
    if signing == 'auto':
        material = credentials.get('harmonyos') or {}
        fields = ('storeFile', 'storePassword', 'keyAlias', 'keyPassword', 'signAlg', 'profile', 'certpath')
        if any(not isinstance(material.get(key), str) or not material[key] for key in fields):
            raise SystemExit('HarmonyOS signing requires the full harmonyos material in export_credentials.json')
        material = {key: material[key] for key in fields}
        for key in ('storeFile', 'profile', 'certpath'):
            path = (project_dir / material[key]).resolve()
            if not path.is_file():
                raise SystemExit('HarmonyOS signing file is missing: ' + key)
            material[key] = path.as_posix()
        configs = [{'name': 'default', 'type': 'HarmonyOS', 'material': material}]
        product['signingConfig'] = 'default'
    # Refresh only generated copies; native/ remains the source of truth.
    for name in ('src', 'types'):
        generated = out_dir / 'entry' / name
        if generated.exists():
            shutil.rmtree(generated)
    shutil.copytree(source, out_dir / 'entry', dirs_exist_ok=True)
    instantiate_template(engine / 'engine/templates/ide/harmonyos', out_dir, tokens)
    (out_dir / 'entry/src/main/module.json5').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    root_profile = {'app': {'signingConfigs': configs, 'products': [product],
                            'buildModeSet': [{'name': 'debug'}, {'name': 'release'}]},
                    'modules': [{'name': 'entry', 'srcPath': './entry',
                                 'targets': [{'name': 'default', 'applyToProducts': ['default']}]}]}
    arguments = ['-DSKY_ENGINE_ROOT=' + engine.as_posix(), '-DOHOS_STL=c++_shared',
                 '-DSKY_MODULE_PACKAGING=' + str(preset.get('packing', 'dynamic')),
                 '-DSKY_SHADER_DISTRIBUTION=' + preset['name']]
    arguments += preset_cmake_args(preset)
    arguments.append('-DSKY_DEPLOY_RUNTIME_RESOURCES=OFF')
    if os.environ.get('SKY_3RD_PATH'):
        arguments.append('-D3RD_PATH=' + Path(os.environ['SKY_3RD_PATH']).as_posix())
    module_profile = {'apiType': 'stageMode', 'targets': [{'name': 'default'}],
                      'buildOption': {'externalNativeOptions': {
                          'path': './src/main/cpp/CMakeLists.txt',
                          'arguments': arguments, 'abiFilters': abis},
                          'nativeLib': {'collectAllLibs': True}}}
    for path, data in ((out_dir / 'build-profile.json5', root_profile),
                       (out_dir / 'entry/build-profile.json5', module_profile)):
        path.write_text(json.dumps(data, indent=2) + '\n', encoding='utf-8')
    return out_dir


def prepare_ios_orientation(project_dir, preset, tokens=None):
    source = project_dir / 'native/ios'
    launcher = source / 'CMakeLists.txt'
    if not launcher.is_file() or '${SKY_IOS_INFO_PLIST}' not in launcher.read_text(encoding='utf-8'):
        raise SystemExit('Update native/ios/CMakeLists.txt to use ${SKY_IOS_INFO_PLIST} for MACOSX_BUNDLE_INFO_PLIST')
    with (source / 'Info.plist').open('rb') as handle:
        manifest = plistlib.load(handle)
    if not isinstance(manifest, dict):
        raise SystemExit('iOS Info.plist must contain a dictionary')
    orientations = IOS_ORIENTATIONS[preset_orientation(preset)]
    # Device-qualified keys take precedence over the common orientation list.
    for key in list(manifest):
        if key.startswith('UISupportedInterfaceOrientations~') or key.startswith('UISupportedInterfaceOrientations-'):
            manifest[key] = list(orientations)
    manifest['UISupportedInterfaceOrientations'] = list(orientations)
    manifest['UISupportedInterfaceOrientations~ipad'] = list(orientations)
    if tokens is not None:
        manifest['CFBundleShortVersionString'] = tokens.get('{{VERSION_NAME}}', '1.0')
        manifest['CFBundleVersion'] = tokens.get('{{VERSION_CODE}}', '1')
    output = project_dir / 'build/ios/export/Info.plist'
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(plistlib.dumps(manifest, sort_keys=False))
    return '-DSKY_IOS_INFO_PLIST=' + output.resolve().as_posix()


def package_harmonyos(out_dir, preset, tokens):
    tools_root = os.environ.get('HARMONYOS_COMMAND_LINE_TOOLS')
    suffix = '.bat' if os.name == 'nt' else ''
    commands = []
    for name in ('ohpm', 'hvigorw'):
        command = Path(tools_root) / 'bin' / (name + suffix) if tools_root else shutil.which(name)
        if not command or not Path(command).is_file():
            raise SystemExit('Set HARMONYOS_COMMAND_LINE_TOOLS to the official command-line-tools directory')
        commands.append(str(command))
    mode = preset.get('variant', 'debug')
    subprocess.run([commands[0], 'install', '--all'], cwd=out_dir, check=True)
    subprocess.run([commands[1], '--mode', 'module', '-p', 'product=default',
                    '-p', 'module=entry@default', '-p', 'buildMode=' + mode,
                    'assembleHap', '--no-daemon'], cwd=out_dir, check=True)
    signed = preset.get('signing', 'off') != 'off'
    artifacts = list((out_dir / 'entry/build/default/outputs/default').glob(
        '*-signed.hap' if signed else '*-unsigned.hap'))
    if len(artifacts) != 1:
        raise SystemExit('Expected exactly one HarmonyOS HAP, found {}'.format(len(artifacts)))
    validate_harmonyos_hap(artifacts[0], tokens['{{LAUNCHER_LIB_NAME}}'], preset.get('abis') or ['arm64-v8a'])
    destination = out_dir / '{}-{}-{}.hap'.format(tokens['{{PROJECT_NAME}}'], mode, 'signed' if signed else 'unsigned')
    shutil.copy2(artifacts[0], destination)
    print('产物: {}'.format(destination))
    return destination


def validate_harmonyos_hap(artifact, launcher, abis):
    with zipfile.ZipFile(artifact) as archive:
        names = set(archive.namelist())
        for abi in abis:
            for library in ('lib' + launcher + '.so', 'libc++_shared.so'):
                name = 'libs/{}/{}'.format(abi, library)
                if name not in names:
                    raise SystemExit('HarmonyOS HAP is missing native library: ' + name)
                with archive.open(name) as stream:
                    header = stream.read(20)
                if len(header) != 20 or header[:6] != b'\x7fELF\x02\x01' or int.from_bytes(header[18:20], 'little') != 183:
                    raise SystemExit('HarmonyOS HAP requires ELF64 AArch64 libraries: ' + name)
        for resource in ('configs/modules_game.json', 'products/bundles.json'):
            if 'resources/rawfile/' + resource not in names:
                raise SystemExit('HarmonyOS HAP is missing runtime resource: ' + resource)


def find_apple_app(project_dir, platform, name, required=True, configuration=None):
    output = project_dir / 'build' / platform / 'bin'
    if configuration is None:
        candidates = sorted(output.glob('*/' + name + '.app')) + sorted(output.glob(name + '.app'))
    else:
        selected = output / configuration / (name + '.app')
        candidates = [selected] if selected.is_dir() else []
    if not candidates and not required:
        return None
    if len(candidates) != 1:
        raise SystemExit('需要唯一的 .app 产物，找到 {} 个: {}'.format(len(candidates), candidates))
    return candidates[0]


def zip_bundle(app, archive, prefix):
    """Preserve Mach-O executable bits and framework symlinks in Apple archives."""
    with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as output:
        for source in [app] + sorted(app.rglob('*')):
            relative = Path(prefix) / app.name / source.relative_to(app)
            info = zipfile.ZipInfo.from_file(source, str(relative)) if not source.is_symlink() else \
                zipfile.ZipInfo(str(relative))
            info.create_system = 3
            info.external_attr = source.lstat().st_mode << 16
            info.compress_type = zipfile.ZIP_DEFLATED
            if source.is_symlink():
                output.writestr(info, os.readlink(source))
            elif source.is_dir():
                output.writestr(info, b'')
            else:
                output.writestr(info, source.read_bytes())


def apple_resource_inventory(destination):
    files = {}
    for relative in ('configs', 'products', 'assets/shaders'):
        directory = destination / relative
        if directory.is_symlink():
            print('Apple runtime resource directory must not be a symlink: ' + relative, file=sys.stderr)
            return None
        for path in sorted(directory.rglob('*')) if directory.is_dir() else []:
            if path.is_symlink():
                print('Apple runtime resource must not be a symlink: ' + str(path), file=sys.stderr)
                return None
            if not path.is_file():
                continue
            digest = hashlib.sha256()
            with path.open('rb') as stream:
                for chunk in iter(lambda: stream.read(1024 * 1024), b''):
                    digest.update(chunk)
            files[path.relative_to(destination).as_posix()] = digest.hexdigest()
    return files


def apple_resource_directory(app, platform):
    return app / 'Contents/Resources' if platform == 'macos' else app


def apple_executable_directory(app, platform):
    return app / 'Contents/MacOS' if platform == 'macos' else app


def retire_legacy_macos_resources(destination):
    contents = destination.parent
    if destination.name != 'Resources' or contents.name != 'Contents' or contents.parent.suffix != '.app':
        return True
    bundle = contents.parent.resolve()
    executable_root = contents / 'MacOS'
    if destination.is_symlink() or not destination.resolve().is_relative_to(bundle) or not executable_root.resolve().is_relative_to(bundle):
        print('macOS application resource and executable directories must remain inside the bundle', file=sys.stderr)
        return False
    paths = [executable_root / relative for relative in ('configs', 'products', 'assets/shaders')]
    if any(not path.parent.resolve().is_relative_to(executable_root.resolve()) for path in paths):
        print('Refusing to retire legacy resources through an aliased executable directory', file=sys.stderr)
        return False
    for path in paths:
        if path.is_symlink() or path.is_file():
            path.unlink()
        elif path.is_dir():
            shutil.rmtree(path)
    assets = executable_root / 'assets'
    if assets.is_dir() and not assets.is_symlink() and not any(assets.iterdir()):
        assets.rmdir()
    return True


def apple_distribution_command(command, output, phase):
    result = subprocess.run(command, capture_output=True, text=True)
    log = output / (phase + '.log')
    log.write_text(result.stdout + result.stderr, encoding='utf-8')
    if result.returncode:
        print('Apple {} failed (exit {}); see {}'.format(phase, result.returncode, log), file=sys.stderr)
        return None
    return result


def verify_apple_distribution_app(app, preset, tokens, receipt, output, phase, team):
    platform = preset['platform']
    resources = apple_resource_directory(app, platform)
    plist = app / 'Contents/Info.plist' if platform == 'macos' else app / 'Info.plist'
    if not plist.is_file() or not receipt.is_file():
        print('Signed Apple application is missing its metadata or fresh resource receipt', file=sys.stderr)
        return False
    with plist.open('rb') as handle:
        metadata = plistlib.load(handle)
    if not isinstance(metadata, dict) or (
            metadata.get('CFBundleIdentifier'), metadata.get('CFBundleShortVersionString'), metadata.get('CFBundleVersion')) != (
                tokens['{{APPLICATION_ID}}'], tokens['{{VERSION_NAME}}'], tokens['{{VERSION_CODE}}']):
        print('Signed Apple application identity/version differs from the selected preset', file=sys.stderr)
        return False
    executable = metadata.get('CFBundleExecutable')
    if not isinstance(executable, str) or not executable or '/' in executable or executable in ('.', '..'):
        print('Signed Apple application has no valid bundle executable', file=sys.stderr)
        return False
    binary = apple_executable_directory(app, platform) / executable
    if not binary.is_file() or not binary.stat().st_mode & stat.S_IXUSR:
        print('Signed Apple application has no executable binary', file=sys.stderr)
        return False
    expected = load_json(receipt, {})
    files = apple_resource_inventory(resources)
    if (not isinstance(expected, dict) or expected.get('platform') != platform
            or expected.get('distribution') != preset['name']
            or expected.get('configuration') != apple_build_configuration(preset)
            or not isinstance(expected.get('files'), dict)
            or not {'configs/modules_game.json', 'products/bundles.json'} <= expected['files'].keys()
            or files is None or files != expected['files']):
        print('Signed Apple application resources do not match the pre-sign staging receipt', file=sys.stderr)
        return False
    if apple_distribution_command(['codesign', '--verify', '--deep', '--strict', str(app)], output,
                                  phase + '-codesign-verify') is None:
        return False
    identity = apple_distribution_command(['codesign', '--display', '--verbose=4', str(app)], output,
                                         phase + '-codesign-identity')
    if identity is None:
        return False
    signature = identity.stdout + identity.stderr
    if 'Signature=adhoc' in signature or 'Authority=' not in signature:
        print('Apple distribution requires a certificate signature, not an ad-hoc signature', file=sys.stderr)
        return False
    actual_team = re.search(r'^TeamIdentifier=(.+)$', signature, re.MULTILINE)
    if team and (actual_team is None or actual_team.group(1).strip() != team):
        print('Signed Apple application belongs to a different development team', file=sys.stderr)
        return False
    return True


def retire_apple_distribution(output):
    for name in ('Application.xcarchive', 'export', 'inspection', 'result.json'):
        path = output / name
        if path.is_symlink() or path.is_file():
            path.unlink()
        elif path.is_dir():
            shutil.rmtree(path)


def apple_distribution_failed(output, message):
    print(message, file=sys.stderr)
    retire_apple_distribution(output)
    return 1


def apple_archive_application(archive, tokens):
    manifest = archive / 'Info.plist'
    if not manifest.is_file():
        print('Apple archive is missing Info.plist', file=sys.stderr)
        return None
    with manifest.open('rb') as handle:
        metadata = plistlib.load(handle)
    properties = metadata.get('ApplicationProperties') if isinstance(metadata, dict) else None
    if not isinstance(properties, dict) or (
            properties.get('CFBundleIdentifier'), properties.get('CFBundleShortVersionString'), properties.get('CFBundleVersion')) != (
                tokens['{{APPLICATION_ID}}'], tokens['{{VERSION_NAME}}'], tokens['{{VERSION_CODE}}']):
        print('Expected an application archive with matching identity/version, not a generic archive', file=sys.stderr)
        return None
    relative = properties.get('ApplicationPath')
    if (not isinstance(relative, str) or not relative or Path(relative).is_absolute()
            or len(Path(relative).parts) != 2 or Path(relative).parts[0] != 'Applications'
            or '..' in Path(relative).parts or not relative.endswith('.app')):
        print('Apple archive has no valid ApplicationPath', file=sys.stderr)
        return None
    products = archive / 'Products'
    application = products / relative
    if not application.is_dir() or not application.resolve().is_relative_to(products.resolve()):
        print('Apple archive ApplicationPath does not identify its application product', file=sys.stderr)
        return None
    return application


def retire_apple_archive_products(project_dir, platform, configuration, name, output):
    generated = project_dir / 'build' / platform
    derived = output / 'DerivedData'
    archive_products = derived / 'ArchiveIntermediates/Launcher/InstallationBuildProductsLocation'
    if derived.is_symlink() or not derived.resolve().is_relative_to(output.resolve()) or not archive_products.resolve().is_relative_to(derived.resolve()):
        print('Refusing to retire Apple archive products through an aliased DerivedData directory', file=sys.stderr)
        return False
    configured = generated / 'bin' / configuration / (name + '.app')
    if not configured.parent.resolve().is_relative_to(generated.resolve()):
        print('Refusing to retire Apple app products outside the selected build directory', file=sys.stderr)
        return False
    for product in (configured, archive_products):
        if product.is_symlink():
            product.unlink()
        elif product.is_dir():
            shutil.rmtree(product)
    return True


def apple_export_options(path, output, team):
    if not path.is_file() or path.resolve().is_relative_to(output.resolve()):
        print('Supply an external export-options plist outside the generated distribution directory', file=sys.stderr)
        return None
    converted = subprocess.run(['plutil', '-convert', 'xml1', '-o', '-', str(path)], capture_output=True, text=True)
    if converted.returncode:
        print('Invalid Apple export-options plist: ' + str(path), file=sys.stderr)
        return None
    options = plistlib.loads(converted.stdout.encode('utf-8'))
    if not isinstance(options, dict) or options.get('destination', 'export') != 'export':
        print('Apple distribution only supports local export; uploading is not enabled', file=sys.stderr)
        return None
    if options.get('manageAppVersionAndBuildNumber') is True:
        print('Disable manageAppVersionAndBuildNumber to preserve the export preset version', file=sys.stderr)
        return None
    if team and options.get('teamID', team) != team:
        print('Export options select a different Apple development team', file=sys.stderr)
        return None
    return options


def cmd_apple_distribute(args):
    if sys.platform != 'darwin':
        print('Apple distribution requires macOS and Xcode', file=sys.stderr)
        return 1
    engine = Path(args.engine).resolve() if args.engine else REPO_ROOT
    descriptor, _ = load_descriptor(args.project)
    selected = find_preset(descriptor.parent, args.preset)
    platform = selected.get('platform')
    if platform not in ('ios', 'macos'):
        print('apple-distribute requires an ios/macos preset', file=sys.stderr)
        return 1
    output = descriptor.parent / 'build' / platform / 'distribution'
    if output.is_symlink() or not output.parent.resolve().is_relative_to(descriptor.parent.resolve()):
        print('Apple distribution output must remain inside the project build directory', file=sys.stderr)
        return 1
    output.mkdir(parents=True, exist_ok=True)
    retire_apple_distribution(output)
    resolved = _resolve(args.project, args.preset)
    if resolved is None:
        return 1
    project_dir, preset, tokens, credentials = resolved
    if preset['platform'] != platform or preset.get('signing') != 'auto':
        print('apple-distribute requires an ios/macos preset with signing=auto', file=sys.stderr)
        return 1
    if (args.device and (platform != 'ios' or args.archive_only)) or (args.launch and not args.device):
        print('Device installation/launch requires an iOS export and an explicit --device', file=sys.stderr)
        return 1
    if not args.archive_only and not args.export_options:
        print('Apple export requires an explicit --export-options plist', file=sys.stderr)
        return 1
    if args.archive_only and args.export_options:
        print('--archive-only does not consume export options', file=sys.stderr)
        return 1
    if any(shutil.which(tool) is None for tool in ('cmake', 'xcodebuild', 'codesign', 'plutil', 'ditto', 'xcrun')):
        print('Apple distribution requires CMake and the installed Xcode command-line tools', file=sys.stderr)
        return 1
    native = project_dir / 'native/CMakeLists.txt'
    launcher = project_dir / 'native' / platform / 'CMakeLists.txt'
    if (not native.is_file() or 'SKY_APPLE_ARCHIVE_RESOURCES' not in native.read_text(encoding='utf-8')
            or not launcher.is_file() or 'sky_stage_apple_archive(Launcher)' not in launcher.read_text(encoding='utf-8')):
        print('Update native/CMakeLists.txt with the Apple pre-sign archive staging contract', file=sys.stderr)
        return 1
    material = apple_credentials(credentials)
    team = preset.get('team') or material['teamId']
    identity = preset.get('signingIdentity') or material['signingIdentity']
    if identity == '-':
        print('Apple distribution requires a certificate signing identity', file=sys.stderr)
        return 1
    options_path = Path(args.export_options).resolve() if args.export_options else None
    if options_path is not None:
        options = apple_export_options(options_path, output, team)
        if options is None:
            return 1
        if not team:
            team = options.get('teamID', '')
            if not isinstance(team, str):
                print('Apple export teamID must be a string', file=sys.stderr)
                return 1
            preset = dict(preset, team=team)
    if platform == 'ios' and not team:
        print('iOS distribution requires a development team in the preset, credentials or export options', file=sys.stderr)
        return 1
    configuration = apple_build_configuration(preset)
    receipt = project_dir / 'build' / platform / 'apple-stage' / (configuration + '.json')
    if receipt.exists():
        receipt.unlink()
    if _prepare(engine, project_dir, preset, tokens, credentials, build=False, archive_resources=True):
        return apple_distribution_failed(output, 'Apple archive preparation failed')
    projects = sorted((project_dir / 'build' / platform).glob('*.xcodeproj'))
    if len(projects) != 1:
        return apple_distribution_failed(output, 'Expected exactly one configured Xcode project for Apple distribution')
    if not retire_apple_archive_products(project_dir, platform, configuration, tokens['{{PROJECT_NAME}}'], output):
        return apple_distribution_failed(output, 'Apple archive product cleanup failed')
    archive = output / 'Application.xcarchive'
    command = ['xcodebuild', '-project', str(projects[0]), '-scheme', 'Launcher', '-configuration', configuration,
               '-destination', 'generic/platform=' + ('iOS' if platform == 'ios' else 'macOS'),
               '-archivePath', str(archive), '-derivedDataPath', str(output / 'DerivedData'),
               'CODE_SIGNING_ALLOWED=YES', 'CODE_SIGNING_REQUIRED=YES']
    if team:
        command.append('DEVELOPMENT_TEAM=' + team)
    if identity:
        command.append('CODE_SIGN_IDENTITY=' + identity)
    command += ['CODE_SIGN_STYLE=' + ('Manual' if preset.get('provisioningProfile') or material['provisioningProfile']
                                      else 'Automatic'), 'archive']
    if apple_distribution_command(command, output, 'archive') is None:
        return apple_distribution_failed(output, 'No Apple distribution was published')
    archived_application = apple_archive_application(archive, tokens)
    if archived_application is None or not verify_apple_distribution_app(
            archived_application, preset, tokens, receipt, output, 'archive', team):
        return apple_distribution_failed(output, 'Apple archive validation failed')
    published = archive
    installed = False
    if not args.archive_only:
        exported = output / 'export'
        if apple_distribution_command(['xcodebuild', '-exportArchive', '-archivePath', str(archive),
                                       '-exportPath', str(exported), '-exportOptionsPlist', str(options_path)],
                                      output, 'export') is None:
            return apple_distribution_failed(output, 'Apple export failed; no deployable result was retained')
        if platform == 'ios':
            packages = sorted(exported.glob('*.ipa'))
            if len(packages) != 1:
                return apple_distribution_failed(output, 'Apple export did not produce exactly one IPA')
            with zipfile.ZipFile(packages[0]) as package:
                if any(name.startswith('/') or '..' in Path(name).parts for name in package.namelist()):
                    return apple_distribution_failed(output, 'Apple export contains an invalid archive entry')
            inspection = output / 'inspection'
            if apple_distribution_command(['ditto', '-x', '-k', str(packages[0]), str(inspection)],
                                          output, 'inspect-export') is None:
                return apple_distribution_failed(output, 'Cannot inspect the exported iOS application')
            applications = sorted((inspection / 'Payload').glob('*.app'))
            published = packages[0]
        else:
            applications = sorted(exported.glob('*.app'))
            published = applications[0] if len(applications) == 1 else exported
        if len(applications) != 1 or not verify_apple_distribution_app(
                applications[0], preset, tokens, receipt, output, 'export', team):
            return apple_distribution_failed(output, 'Exported Apple application validation failed')
        if args.device:
            if apple_distribution_command(['xcrun', 'devicectl', 'device', 'install', 'app', '--device',
                                           args.device, str(applications[0])], output, 'install') is None:
                return apple_distribution_failed(output, 'Apple device installation failed')
            installed = True
            if args.launch and apple_distribution_command(
                    ['xcrun', 'devicectl', 'device', 'process', 'launch', '--device', args.device,
                     tokens['{{APPLICATION_ID}}']], output, 'launch') is None:
                return apple_distribution_failed(output, 'Apple application launch failed')
    result = {'platform': platform, 'configuration': configuration, 'archive': str(archive),
              'artifact': str(published), 'archiveOnly': args.archive_only, 'installed': installed,
              'bundleId': tokens['{{APPLICATION_ID}}'], 'versionName': tokens['{{VERSION_NAME}}'],
              'versionCode': tokens['{{VERSION_CODE}}']}
    (output / 'result.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print('Verified Apple artifact: ' + str(published))
    return 0


def collect_apple_package(engine, project_dir, preset, name, versions=None):
    platform = preset['platform']
    configuration = apple_build_configuration(preset)
    if configuration is None:
        return None
    app = find_apple_app(project_dir, platform, name, configuration=configuration)
    resource_root = apple_resource_directory(app, platform)
    executable = apple_executable_directory(app, platform) / name
    plist = app / 'Contents' / 'Info.plist' if platform == 'macos' else app / 'Info.plist'
    if not plist.is_file() or not executable.is_file() or not executable.stat().st_mode & stat.S_IXUSR:
        raise SystemExit('不完整的 Apple 应用包: {}'.format(app))
    if versions is not None:
        with plist.open('rb') as handle:
            manifest = plistlib.load(handle)
        if not isinstance(manifest, dict) or (
                manifest.get('CFBundleShortVersionString'), manifest.get('CFBundleVersion')) != tuple(versions):
            print('Built Apple bundle version does not match the selected export preset', file=sys.stderr)
            return None
    if not stage_runtime_resources(engine, project_dir, preset, resource_root):
        return None
    suffix = '-unsigned.ipa' if platform == 'ios' else '-macos.zip'
    archive = project_dir / 'build' / platform / (name + suffix)
    zip_bundle(app, archive, 'Payload' if platform == 'ios' else '')
    print('应用: {}'.format(app))
    print('产物: {}'.format(archive))
    if platform == 'ios':
        print('  未签名 IPA，仅用于检查打包内容；真机安装需要有效签名及描述文件。')
    return archive


def find_asset_tool(engine):
    name = 'AssetTool.exe' if os.name == 'nt' else 'AssetTool'
    for sub in ('Develop', 'Debug', 'Release', 'RelWithDebInfo', ''):
        candidate = engine / 'output' / 'bin' / sub / name if sub else engine / 'output' / 'bin' / name
        if candidate.is_file():
            return candidate
    return None


def run_cook(engine, project_dir, platform):
    """Rebuild all project products; failed cooks cannot leave deployable output."""
    tool = find_asset_tool(engine)
    if tool is None:
        print('Build AssetTool before packaging; a full asset cook is required', file=sys.stderr)
        return False
    products = project_dir / 'products'
    if products.is_symlink():
        products.unlink()
    elif products.exists():
        shutil.rmtree(products)
    cmd = [str(tool), '--project', str(project_dir), '--engine', str(tool.parent), '--cook-all', '--platform', platform]
    print('cook: {}'.format(' '.join(cmd)))
    result = subprocess.run(cmd, cwd=str(tool.parent))
    if result.returncode != 0:
        if products.is_symlink():
            products.unlink()
        elif products.exists():
            shutil.rmtree(products)
        print('AssetTool cook failed (exit {})'.format(result.returncode), file=sys.stderr)
        return False
    return True


def strip_jsonc(text):
    """Remove // and /* */ comments so a .jsonc cook config parses as JSON."""
    import re
    text = re.sub(r'/\*.*?\*/', '', text, flags=re.S)
    text = re.sub(r'(^|[^:])//[^\n]*', r'\1', text)
    return text


def load_distribution_bundles(project_dir, distribution):
    """Bundles for `distribution` from the project's cook config (single source)."""
    for name in ('asset_cook.jsonc', 'asset_build_presets.json'):
        cfg = project_dir / 'configs' / name
        if not cfg.is_file():
            continue
        try:
            data = json.loads(strip_jsonc(cfg.read_text(encoding='utf-8')))
        except ValueError:
            continue
        dists = data.get('distributions') or {}
        if distribution in dists:
            return [str(b) for b in dists[distribution]]
    return None


def select_bundles(project_dir, preset):
    """Bundles to package: preset `bundles`, else the cook config distribution, else [common]."""
    if preset.get('bundles'):
        return list(preset['bundles'])
    from_dist = load_distribution_bundles(project_dir, preset.get('name', ''))
    if from_dist:
        return from_dist
    return ['common']


def check_bundle_conflicts(bundles):
    seen = set()
    for name in bundles:
        if name in seen:
            raise SystemExit('bundle 列表重复: {}'.format(name))
        seen.add(name)


def canonical_world_reference(value):
    if not isinstance(value, str) or '\0' in value or ':' in value:
        return None
    if not value:
        return ''
    path = posixpath.normpath(value.replace('\\', '/'))
    return None if path in ('.', '..') or path.startswith(('/', '../')) else path


def collect_world_documents(project_dir, preset):
    assets = (project_dir / 'assets').resolve()
    selected = sorted(assets.rglob('*.world'))
    if preset.get('worlds'):
        if not isinstance(preset['worlds'], list):
            return None, None, 'World selection must be an array of logical asset paths'
        paths = [canonical_world_reference(path) for path in preset['worlds']]
        if any(not path for path in paths):
            return None, None, 'World selection must contain relative project asset paths'
        selected = [assets / path for path in paths]
    worlds = []
    roots = []
    for world in selected:
        world = Path(os.path.abspath(world))
        if not world.is_relative_to(assets) or not world.resolve().is_relative_to(assets) or not world.is_file():
            return None, None, 'World must be a project asset: {}'.format(world)
        if world in worlds:
            continue
        document = load_json(world)
        if not isinstance(document, dict) or not isinstance(document.get('assetRoots'), list):
            return None, None, 'Re-save World in the editor to collect explicit asset roots: {}'.format(world)
        for reference in document['assetRoots']:
            if (not isinstance(reference, dict) or not isinstance(reference.get('uuid'), str)
                    or not reference['uuid'] or not isinstance(reference.get('type'), str) or not reference['type']):
                return None, None, 'Malformed asset root in {}'.format(world)
            roots.append(dict(reference, world=world.relative_to(assets).as_posix()))
        worlds.append(world)
    return worlds, roots, None


def collect_world_asset_roots(project_dir, preset):
    _, roots, error = collect_world_documents(project_dir, preset)
    return roots, error


def stage_bundles(project_dir, preset, dest_products_dir, engine=None):
    """Copy the selected cooked bundles and write products/bundles.json.

    Cooked bundles are read from <Project>/products/<bundle> (cook output);
    bundles without cooked output are still listed (the runtime falls back).
    """
    bundles = select_bundles(project_dir, preset)
    check_bundle_conflicts(bundles)
    if any(not isinstance(name, str) or not name or name in ('.', '..') or any(c in name for c in '/\\:') for name in bundles):
        print('Invalid package bundle name', file=sys.stderr)
        return None
    destination = dest_products_dir.resolve()
    project = project_dir.resolve()
    if destination == project or destination in project.parents or destination.is_relative_to(project / 'products'):
        print('Package destination must not replace project files or cooked products', file=sys.stderr)
        return None
    worlds, _, error = collect_world_documents(project_dir, preset)
    if error:
        print(error, file=sys.stderr)
        return None
    assets = (project_dir / 'assets').resolve()
    roots = [{'path': world.relative_to(assets).as_posix(), 'type': 'World'} for world in worlds]
    descriptor = optional_descriptor(project_dir)
    default_scene = canonical_world_reference(descriptor.get('defaultScene', ''))
    if default_scene is None:
        print('Default World must be a relative project asset path', file=sys.stderr)
        return None
    if default_scene and default_scene not in {root['path'] for root in roots}:
        print('Default World must be selected for packaging: ' + default_scene, file=sys.stderr)
        return None
    tool = find_asset_tool(engine or REPO_ROOT) if roots else None
    if roots and tool is None:
        print('Build AssetTool to verify World product dependencies before packaging', file=sys.stderr)
        return None
    if dest_products_dir.exists():
        shutil.rmtree(dest_products_dir)
    dest_products_dir.mkdir(parents=True, exist_ok=True)
    for name in bundles:
        src = project_dir / 'products' / name
        if src.is_dir():
            shutil.copytree(src, dest_products_dir / name, dirs_exist_ok=True,
                            ignore=shutil.ignore_patterns('product.lock', '*.tmp'))
        else:
            print('  (bundle "{}" not cooked; skipped copy)'.format(name))
    distribution = preset.get('name') or preset.get('platform', 'default')
    if (project_dir / 'products/shaders' / distribution / 'distribution.json').is_file():
        error = stage_shader_products(project_dir, distribution, dest_products_dir, preset.get('platform'))
        if error:
            print(error, file=sys.stderr)
            return None
    manifest = {
        'platform': preset.get('platform', ''),
        'distribution': preset.get('name', ''),
        'bundles': bundles,
    }
    (dest_products_dir / 'bundles.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    roots_path = dest_products_dir / 'asset-roots.json'
    roots_path.write_text(json.dumps({'bundles': bundles, 'roots': roots}, indent=2) + '\n', encoding='utf-8')
    if roots:
        result = subprocess.run([str(tool.resolve()), '--audit-products', str(roots_path.resolve())], cwd=tool.parent)
        if result.returncode != 0:
            print('World product dependency verification failed', file=sys.stderr)
            return None
    print('bundles.json: {}'.format(dest_products_dir / 'bundles.json'))
    return bundles


def optional_descriptor(project_dir):
    return load_descriptor(project_dir)[1] if any(project_dir.glob('*.skyproj')) else {}


def stage_configs(engine, project_dir, dest_configs_dir, development=True):
    """Ship startup configuration in the read-only package; project files win."""
    if dest_configs_dir.exists():
        shutil.rmtree(dest_configs_dir)
    dest_configs_dir.mkdir(parents=True, exist_ok=True)
    for source in (engine / 'configs', project_dir / 'configs'):
        if source.is_dir():
            shutil.copytree(source, dest_configs_dir, dirs_exist_ok=True)
    jolt_license = engine / 'plugins/jolt/LICENSE.txt'
    if jolt_license.is_file():
        notices = dest_configs_dir / 'licenses'
        notices.mkdir(parents=True, exist_ok=True)
        shutil.copy2(jolt_license, notices / 'Jolt.txt')
    startup = dest_configs_dir / 'modules_game.json'
    if startup.is_file():
        data = load_json(startup, {})
        if not isinstance(data, dict) or not isinstance(data.get('game', {}), dict):
            print('Game configuration must contain object-shaped startup settings', file=sys.stderr)
            return False
        descriptor = optional_descriptor(project_dir)
        if descriptor:
            default_scene = canonical_world_reference(descriptor.get('defaultScene', ''))
            if default_scene is None:
                print('Default World must be a relative project asset path', file=sys.stderr)
                return False
            data.setdefault('game', {})['defaultScene'] = default_scene
        if not development:
            excluded = {'Aurora.ShaderCompiler', 'Aurora.Cook'}
            data['modules'] = [module for module in data.get('modules', []) if module.get('name') not in excluded]
            for module in data['modules']:
                module['dependencies'] = [name for name in module.get('dependencies', []) if name not in excluded]
        startup.write_text(json.dumps(data, indent=2) + '\n', encoding='utf-8')
    return True


def stage_runtime_resources(engine, project_dir, preset, destination, development=None):
    development = development_preset(preset) if development is None else development
    runtime_directory = None
    if (preset.get('platform') == 'macos' and destination.name == 'Resources' and destination.parent.name == 'Contents'
            and destination.parent.parent.suffix == '.app'):
        if not retire_legacy_macos_resources(destination):
            return False
        runtime_directory = destination.parent / 'MacOS'
    if development:
        configs, error = project_modules(engine, project_dir, optional_descriptor(project_dir))
        if not error:
            error = stage_module_sources(configs, destination / 'assets/shaders')
        if error:
            print(error, file=sys.stderr)
            return False
    else:
        strip_development_resources(destination, runtime_directory)
    if not stage_configs(engine, project_dir, destination / 'configs', development=development):
        return False
    return stage_bundles(project_dir, preset, destination / 'products', engine) is not None


def strip_development_resources(destination, runtime_directory=None):
    """Retire generated development files when reusing a shipping app/assets directory."""
    sources = destination / 'assets/shaders'
    if sources.exists() or sources.is_symlink():
        if not sources.parent.resolve().is_relative_to(destination.resolve()):
            raise SystemExit('Refusing to retire shader sources through an aliased assets directory: ' + str(sources))
        if sources.is_symlink():
            sources.unlink()
        else:
            shutil.rmtree(sources)
    directories = [(destination, destination), (destination / 'Frameworks', destination)]
    if runtime_directory is not None:
        directories += [(runtime_directory, runtime_directory), (runtime_directory / 'Frameworks', runtime_directory)]
    for directory, owner in directories:
        if not directory.is_dir():
            continue
        if not directory.resolve().is_relative_to(owner.resolve()):
            raise SystemExit('Refusing to retire compiler files through an aliased runtime directory: ' + str(directory))
        for library in directory.iterdir():
            name = library.name.lower().removeprefix('lib')
            if name.startswith(('slang', 'aurora.shadercompiler.')) and any(
                    suffix in name for suffix in ('.dylib', '.so', '.dll', '.framework')):
                if library.is_dir() and not library.is_symlink():
                    shutil.rmtree(library)
                else:
                    library.unlink()


def run_cmake_preset(engine, project_dir, platform, build, extra, extra_build, configuration=None):
    native_dir = project_dir / 'native'
    env = os.environ.copy()
    env['SKY_ENGINE_ROOT'] = str(engine)
    args = list(extra)
    if env.get('SKY_3RD_PATH'):
        args.append('-D3RD_PATH=' + env['SKY_3RD_PATH'])
    if configuration is not None:
        args.append('-DCMAKE_BUILD_TYPE=' + configuration)
    configure = ['cmake', '--preset', platform] + args
    print('配置: {} (cwd={})'.format(' '.join(configure), native_dir))
    result = subprocess.run(configure, cwd=str(native_dir), env=env)
    if result.returncode != 0:
        raise SystemExit('cmake 配置失败 (exit {})'.format(result.returncode))
    if build:
        build_cmd = ['cmake', '--build', '--preset', platform]
        if configuration is not None:
            build_cmd += ['--config', configuration]
        build_cmd += list(extra_build)
        print('构建: {} (cwd={})'.format(' '.join(build_cmd), native_dir))
        result = subprocess.run(build_cmd, cwd=str(native_dir), env=env)
        if result.returncode != 0:
            raise SystemExit('cmake 构建失败 (exit {})'.format(result.returncode))


def cmd_create(args):
    def progress(stage):
        if getattr(args, 'progress', False):
            print('[sky-create] ' + stage, flush=True)

    engine = Path(args.engine).resolve() if args.engine else REPO_ROOT
    project_dir = Path(args.dir).resolve() / args.name
    if project_dir.exists() and any(project_dir.iterdir()):
        if not args.force:
            raise SystemExit('目标已存在且非空: {} (用 --force 覆盖)'.format(project_dir))

    descriptor = {'name': args.name}
    tokens = base_tokens(descriptor)

    # game skeleton (native/ CMake entry + source module)
    progress('templates')
    instantiate_template(engine / 'engine' / 'templates' / 'game', project_dir, tokens)
    # platform launcher entries -> native/<platform>/
    progress('launchers')
    launcher_root = engine / 'engine' / 'templates' / 'launcher'
    for platform in LAUNCHER_PLATFORMS:
        platform_dir = launcher_root / platform
        if platform_dir.is_dir():
            instantiate_template(platform_dir, project_dir / 'native' / platform, tokens)

    progress('directories')
    for sub in ('assets', 'configs', 'cache'):
        (project_dir / sub).mkdir(parents=True, exist_ok=True)

    identifier = tokens['{{PROJECT_NAME}}']
    skyproj = {
        'id': identifier.lower(),
        'name': args.name,
        'engineVersion': '0.1.0',
        'defaultScene': '',
        'settings': {
            'rhi': 'vulkan',
            'versionName': '1.0',
            'versionCode': 1,
        },
        'modules': [identifier + 'Module'],
        'plugins': [],
    }
    progress('descriptor')
    with (project_dir / (args.name + '.skyproj')).open('w', encoding='utf-8') as handle:
        json.dump(skyproj, handle, indent=2)
    print('工程: {}'.format(project_dir))
    return 0


def valid_bundle_id(platform, value):
    if not isinstance(value, str) or not value:
        return False
    if platform in ('ios', 'macos'):
        return re.fullmatch(r'[A-Za-z0-9.-]+', value) is not None
    if platform == 'harmonyos':
        return 7 <= len(value) <= 128 and re.fullmatch(
            r'[A-Za-z](?:[A-Za-z0-9_]*[A-Za-z0-9])?(?:\.[A-Za-z0-9](?:[A-Za-z0-9_]*[A-Za-z0-9])?){2,}', value) is not None
    return re.fullmatch(r'[A-Za-z][A-Za-z0-9_]*(?:\.[A-Za-z][A-Za-z0-9_]*)+', value) is not None


def _resolve(project_arg, preset_name):
    skyproj, descriptor = load_descriptor(project_arg)
    project_dir = skyproj.parent
    preset = find_preset(project_dir, preset_name)
    if preset['platform'] in ('android', 'harmonyos', 'ios', 'macos'):
        bundle_id = preset.get('bundleId')
        if not valid_bundle_id(preset['platform'], bundle_id):
            print('Export preset requires a valid {} bundleId; move the install identity from .skyproj to each platform preset.'.format(
                preset['platform']), file=sys.stderr)
            return None
    credentials = load_json(project_dir / 'export_credentials.json', {})
    tokens = build_tokens(descriptor, preset, credentials)
    if preset['platform'] in ('ios', 'macos'):
        versions = apple_export_versions(descriptor, preset)
        if apple_build_configuration(preset) is None or versions is None:
            return None
        tokens['{{VERSION_NAME}}'], tokens['{{VERSION_CODE}}'] = versions
    if preset['platform'] == 'android':
        launchers = list((project_dir / 'native/android/java').rglob('SkyEngineActivity.java'))
        if len(launchers) != 1:
            print('Android export requires exactly one project-owned SkyEngineActivity.java.', file=sys.stderr)
            return None
        declaration = re.search(r'^\s*package\s+([A-Za-z][A-Za-z0-9_]*(?:\.[A-Za-z][A-Za-z0-9_]*)+)\s*;',
                                launchers[0].read_text(encoding='utf-8'), re.MULTILINE)
        if declaration is None:
            print('Android launcher has no valid Java package declaration.', file=sys.stderr)
            return None
        tokens['{{ANDROID_NAMESPACE}}'] = declaration.group(1)
        tokens['{{ANDROID_NAMESPACE_PATH}}'] = declaration.group(1).replace('.', '/')
    return project_dir, preset, tokens, credentials


def retire_package_artifacts(project_dir, platform):
    output = project_dir / 'build' / platform
    patterns = {
        'android': ('*.apk', 'app/build/outputs/apk/**/*.apk'),
        'harmonyos': ('*.hap', 'entry/build/default/outputs/default/*.hap'),
        'ios': ('*.ipa',),
        'macos': ('*-macos.zip',),
    }
    for pattern in patterns[platform]:
        for artifact in output.glob(pattern):
            artifact.unlink()


def _prepare(engine, project_dir, preset, tokens, credentials, build, archive_resources=False):
    """Generate the platform project, cook, and stage the selected bundles.

    With `build`, also run the platform build (gradle assemble / cmake build) and
    collect the artifact.
    """
    platform = preset['platform']
    orientation = preset_orientation(preset)
    if platform in ('android', 'harmonyos'):
        if platform == 'android':
            out_dir = project_dir / 'build/android'
            tokens = dict(tokens)
            tokens['{{ANDROID_ORIENTATION}}'] = ANDROID_ORIENTATIONS[orientation]
            tokens['{{ANDROID_CMAKE_ARGUMENTS}}'] = android_cmake_args(engine, preset)
            instantiate_template(engine / 'engine/templates/ide/android', out_dir, tokens)
            resources = out_dir / 'app/src/main/assets'
        else:
            out_dir = prepare_harmonyos_project(engine, project_dir, preset, tokens, credentials)
            resources = out_dir / 'entry/src/main/resources/rawfile'
        retire_package_artifacts(project_dir, platform)
        if not run_cook(engine, project_dir, platform) or not prepare_shader_products(engine, project_dir, preset):
            return 1
        if not stage_runtime_resources(engine, project_dir, preset, resources):
            return 1
        if build:
            if platform == 'android':
                package_android(out_dir, project_dir, tokens, preset.get('variant', 'debug'))
            else:
                package_harmonyos(out_dir, preset, tokens)
    elif platform in ('ios', 'macos'):
        if build and preset.get('signing', 'auto') != 'off':
            raise SystemExit('Apple package 要求 signing=off；签名分发请使用 apple-distribute。')
        configuration = apple_build_configuration(preset)
        if configuration is None:
            return 1
        metadata_args = []
        if platform == 'ios':
            metadata_args.append(prepare_ios_orientation(project_dir, preset, tokens))
        elif preset.get('bundle'):
            metadata = prepare_macos_metadata(project_dir, tokens)
            if metadata is None:
                return 1
            metadata_args.append(metadata)
        if not archive_resources:
            retire_package_artifacts(project_dir, platform)
        if build:
            old_app = find_apple_app(project_dir, platform, tokens['{{PROJECT_NAME}}'], required=False,
                                     configuration=configuration)
            if old_app is not None:
                # Reuse compiled objects, but never ship libraries/resources left
                # by a previous preset or disabled plugin.
                shutil.rmtree(old_app)
        if not run_cook(engine, project_dir, platform):
            return 1
        if not prepare_shader_products(engine, project_dir, preset):
            return 1
        # IDE builds deploy runtime resources; unsigned package collection owns staging after the build.
        deployment_args = ['-DSKY_APPLE_ARCHIVE_RESOURCES=' + ('ON' if archive_resources else 'OFF')]
        if archive_resources:
            deployment_args += ['-DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=YES',
                                '-DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED=YES',
                                '-DCODE_SIGNING_ALLOWED=YES', '-DCODE_SIGNING_REQUIRED=YES']
        deployment_args.append('-DSKY_DEPLOY_RUNTIME_RESOURCES=' + ('OFF' if build or archive_resources else 'ON'))
        run_cmake_preset(engine, project_dir, platform, build=build,
                         extra=apple_cmake_args(preset, credentials, tokens['{{APPLICATION_ID}}']) + metadata_args + deployment_args,
                         extra_build=preset.get('extraBuildArgs') or [], configuration=configuration)
        if build:
            versions = (tokens.get('{{VERSION_NAME}}', '1.0'), tokens.get('{{VERSION_CODE}}', '1'))
            if collect_apple_package(engine, project_dir, preset, tokens['{{PROJECT_NAME}}'], versions) is None:
                return 1
    else:
        raise SystemExit('不支持的平台: {}'.format(platform))


def cmd_generate(args):
    engine = Path(args.engine).resolve() if args.engine else REPO_ROOT
    resolved = _resolve(args.project, args.preset)
    if resolved is None:
        return 1
    project_dir, preset, tokens, credentials = resolved
    return _prepare(engine, project_dir, preset, tokens, credentials, build=False) or 0


def cmd_package(args):
    engine = Path(args.engine).resolve() if args.engine else REPO_ROOT
    resolved = _resolve(args.project, args.preset)
    if resolved is None:
        return 1
    project_dir, preset, tokens, credentials = resolved
    return _prepare(engine, project_dir, preset, tokens, credentials, build=True) or 0


def shader_cook_tool(engine):
    name = 'ShaderCookTool.exe' if os.name == 'nt' else 'ShaderCookTool'
    for configuration in ('Develop', 'Debug', 'Release', 'RelWithDebInfo', ''):
        candidate = engine / 'output/bin' / configuration / name
        if candidate.is_file():
            return candidate
    return engine / 'output/bin/Develop' / name


def prepare_shader_products(engine, project_dir, preset):
    _, descriptor = load_descriptor(project_dir)
    platform = preset['platform']
    targets = preset.get('shaderTargets') or (['metal'] if platform in ('ios', 'macos') else ['vulkan'])
    error = cook_project_shaders(engine, project_dir, descriptor, preset.get('name') or platform,
                                 targets, select_bundles(project_dir, preset), shader_cook_tool(engine), platform, preset.get('shaderEnvironments'))
    if error:
        print(error, file=sys.stderr)
    return not error


def cmd_cook_shaders(args):
    engine = Path(args.engine).resolve() if args.engine else REPO_ROOT
    project, descriptor = load_descriptor(args.project)
    error = cook_project_shaders(engine, project.parent, descriptor, args.distribution, args.target,
                                 args.bundle or ['common'], Path(args.tool).resolve() if args.tool else shader_cook_tool(engine), args.platform)
    if error:
        print(error, file=sys.stderr)
        return 1
    return 0


def cmd_stage_shaders(args):
    engine = Path(args.engine).resolve() if args.engine else REPO_ROOT
    project, descriptor = load_descriptor(args.project)
    configs, error = project_modules(engine, project.parent, descriptor)
    if not error:
        error = stage_module_sources(configs, Path(args.destination))
    if error:
        print(error, file=sys.stderr)
        return 1
    return 0


def cmd_stage_runtime(args):
    engine = Path(args.engine).resolve() if args.engine else REPO_ROOT
    project, _ = load_descriptor(args.project)
    project_dir = project.parent
    preset = find_preset(project_dir, args.distribution)
    if getattr(args, 'xcode_environment', False):
        target = os.environ.get('TARGET_BUILD_DIR', '')
        folder = os.environ.get('UNLOCALIZED_RESOURCES_FOLDER_PATH', '')
        relative = Path(folder)
        if (preset.get('platform') not in ('ios', 'macos') or not target or not Path(target).is_absolute()
                or not folder or relative.is_absolute() or '..' in relative.parts
                or os.environ.get('CONFIGURATION') != apple_build_configuration(preset)):
            print('Apple archive staging requires the selected configuration and Xcode product environment', file=sys.stderr)
            return 1
        destination = Path(target) / relative
        if not destination.resolve().is_relative_to(Path(target).resolve()) or not any(
                path.suffix == '.app' for path in [destination] + list(destination.parents)
                if path.is_relative_to(Path(target))):
            print('Xcode runtime resource destination must belong to its application bundle', file=sys.stderr)
            return 1
        if preset['platform'] == 'macos' and (destination.name != 'Resources' or destination.parent.name != 'Contents'
                                             or destination.parent.parent.suffix != '.app'):
            print('macOS Xcode resources must use Contents/Resources', file=sys.stderr)
            return 1
    else:
        destination = Path(args.destination)
        if (preset.get('platform') == 'macos' and destination.name == 'MacOS' and destination.parent.name == 'Contents'
                and destination.parent.parent.suffix == '.app'):
            destination = destination.parent / 'Resources'
    receipt = Path(args.receipt).resolve() if getattr(args, 'receipt', None) else None
    if receipt is not None:
        if receipt.is_relative_to(destination.resolve()):
            print('Apple resource receipts must remain outside the signed application', file=sys.stderr)
            return 1
        if receipt.exists():
            receipt.unlink()
    manifest = project_dir / 'products/shaders' / args.distribution / 'distribution.json'
    if not args.development and not manifest.is_file():
        print('Shader distribution has not been cooked: ' + args.distribution, file=sys.stderr)
        return 1
    if not stage_runtime_resources(engine, project_dir, preset, destination, args.development):
        return 1
    if receipt is not None:
        files = apple_resource_inventory(destination)
        if files is None:
            return 1
        receipt.parent.mkdir(parents=True, exist_ok=True)
        receipt.write_text(json.dumps({'platform': preset['platform'], 'distribution': args.distribution,
                                       'configuration': apple_build_configuration(preset), 'files': files},
                                      indent=2) + '\n', encoding='utf-8')
    return 0


def main():
    parser = argparse.ArgumentParser(description='SkyEngine 工程工具')
    sub = parser.add_subparsers(dest='command', required=True)

    p_create = sub.add_parser('create', help='从模板创建工程')
    p_create.add_argument('--dir', required=True, help='父目录')
    p_create.add_argument('--name', required=True, help='工程名')
    p_create.add_argument('--progress', action='store_true', help='输出工程创建阶段，供编辑器展示进度')
    p_create.add_argument('--engine', type=str, help='引擎目录（默认仓库根）')
    p_create.add_argument('--force', action='store_true', default=False, help='目标非空时覆盖')
    p_create.set_defaults(func=cmd_create)

    p_shaders = sub.add_parser('cook-shaders', help='Cook one project shader distribution from registered module resources')
    p_shaders.add_argument('-p', '--project', required=True)
    p_shaders.add_argument('--distribution', default='default')
    p_shaders.add_argument('--platform', choices=['windows', 'macos', 'ios', 'android', 'linux', 'harmonyos'], required=True)
    p_shaders.add_argument('--target', choices=['vulkan', 'metal', 'd3d12'], action='append', required=True)
    p_shaders.add_argument('--bundle', action='append')
    p_shaders.add_argument('--engine')
    p_shaders.add_argument('--tool')
    p_shaders.set_defaults(func=cmd_cook_shaders)

    p_sources = sub.add_parser('stage-shaders', help='Stage the selected project shader source inventory')
    p_sources.add_argument('-p', '--project', required=True)
    p_sources.add_argument('--engine')
    p_sources.add_argument('--destination', required=True)
    p_sources.set_defaults(func=cmd_stage_shaders)

    p_runtime = sub.add_parser('stage-runtime', help='Deploy selected runtime products/configs during native builds')
    p_runtime.add_argument('-p', '--project', required=True)
    p_runtime.add_argument('--engine')
    p_runtime.add_argument('--distribution', required=True)
    destination = p_runtime.add_mutually_exclusive_group(required=True)
    destination.add_argument('--destination')
    destination.add_argument('--xcode-environment', action='store_true', help='Use the current Xcode archive product directory')
    p_runtime.add_argument('--receipt', help='Write a resource inventory outside the application bundle')
    p_runtime.add_argument('--development', action='store_true')
    p_runtime.set_defaults(func=cmd_stage_runtime)

    p_apple = sub.add_parser('apple-distribute', help='Archive, verify and locally export a signed Apple application')
    p_apple.add_argument('-p', '--project', required=True)
    p_apple.add_argument('--preset', required=True)
    p_apple.add_argument('--engine')
    p_apple.add_argument('--export-options', help='External Xcode export-options plist; destination must be export')
    p_apple.add_argument('--archive-only', action='store_true', help='Verify a local signed archive without claiming export')
    p_apple.add_argument('--device', help='Explicit iOS device UDID to install the verified exported application')
    p_apple.add_argument('--launch', action='store_true', help='Launch after explicit iOS device installation')
    p_apple.set_defaults(func=cmd_apple_distribute)

    for name, helptext in (('generate', '按预设生成平台工程'), ('package', '按预设打包')):
        p = sub.add_parser(name, help=helptext)
        p.add_argument('-p', '--project', required=True, help='.skyproj 路径（或工程目录）')
        p.add_argument('--preset', required=True, help='export_presets.json 中的预设名')
        p.add_argument('--engine', type=str, help='引擎目录（默认仓库根）')
        if name == 'generate':
            p.set_defaults(func=cmd_generate)
        else:
            p.set_defaults(func=cmd_package)

    args = parser.parse_args()
    return args.func(args)


if __name__ == '__main__':
    sys.exit(main())
