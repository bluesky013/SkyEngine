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
"""

import argparse
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

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
    '{{ANDROID_KEYSTORE_PATH}}',
    '{{ANDROID_KEYSTORE_ALIAS}}',
    '{{ANDROID_STORE_PASSWORD}}',
    '{{ANDROID_KEY_PASSWORD}}',
]

LAUNCHER_PLATFORMS = ['android', 'ios', 'macos', 'windows']

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
            data = data.replace(key.encode('ascii'), tokens[key].encode('ascii'))
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
    path = Path(project_arg)
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
    namespace = sanitize_package(settings.get('applicationId') or ('com.skyengine.' + identifier.lower()))
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
        '{{APPLICATION_ID}}': namespace,
        '{{ANDROID_KEYSTORE_PATH}}': '',
        '{{ANDROID_KEYSTORE_ALIAS}}': '',
        '{{ANDROID_STORE_PASSWORD}}': '',
        '{{ANDROID_KEY_PASSWORD}}': '',
    }


def build_tokens(descriptor, preset, credentials):
    tokens = base_tokens(descriptor)
    settings = descriptor.get('settings') or {}

    # 安装用 applicationId 可被预设 bundleId 覆盖；namespace/Java 包名保持 create 时的值。
    tokens['{{APPLICATION_ID}}'] = preset.get('bundleId') or tokens['{{ANDROID_NAMESPACE}}']
    tokens['{{VERSION_NAME}}'] = str(preset.get('versionName') or settings.get('versionName') or '1.0')
    tokens['{{VERSION_CODE}}'] = str(preset.get('versionCode') or settings.get('versionCode') or 1)
    tokens['{{COMPILE_SDK}}'] = str(preset.get('compileSdk', 34))
    tokens['{{MIN_SDK}}'] = str(preset.get('minSdk', 29))
    tokens['{{TARGET_SDK}}'] = str(preset.get('targetSdk', 33))
    tokens['{{NDK_VERSION}}'] = str(preset.get('ndkVersion', '28.1.13356709'))
    tokens['{{ANDROID_ABI}}'] = (preset.get('abis') or ['arm64-v8a'])[0]

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


def apple_cmake_args(preset, credentials, default_bundle_id):
    """Preset -> cmake cache args (bundle id, macOS .app, signing, extra options)."""
    args = []
    bundle_id = preset.get('bundleId') or default_bundle_id
    if bundle_id:
        args.append('-DSKY_BUNDLE_ID=' + str(bundle_id))
    if preset.get('platform') == 'macos' and preset.get('bundle'):
        args.append('-DSKY_MACOS_BUNDLE=ON')

    apple = apple_credentials(credentials)
    team = preset.get('team') or apple['teamId']
    identity = preset.get('signingIdentity') or apple['signingIdentity']
    profile = preset.get('provisioningProfile') or apple['provisioningProfile']
    if team:
        args.append('-DSKY_APPLE_TEAM_ID=' + team)
    if identity:
        args.append('-DSKY_APPLE_SIGNING_IDENTITY=' + identity)
    if profile:
        args.append('-DSKY_APPLE_PROVISIONING_PROFILE=' + profile)

    if preset.get('signing', 'auto') == 'off':
        args += ['-DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=NO', '-DCODE_SIGNING_REQUIRED=NO',
                 '-DCODE_SIGNING_ALLOWED=NO']

    for key, value in (preset.get('cacheVariables') or {}).items():
        args.append('-D{}={}'.format(key, value))
    definitions = preset.get('compileDefinitions') or []
    if definitions:
        args.append('-DSKY_EXTRA_CXX_DEFINITIONS=' + ';'.join(definitions))
    flags = preset.get('compileFlags') or []
    if flags:
        args.append('-DSKY_EXTRA_CXX_FLAGS=' + ';'.join(flags))
    args += preset.get('extraConfigureArgs') or []
    return args


def find_asset_tool(engine):
    name = 'AssetTool.exe' if os.name == 'nt' else 'AssetTool'
    for sub in ('Debug', 'Release', ''):
        candidate = engine / 'output' / 'bin' / sub / name if sub else engine / 'output' / 'bin' / name
        if candidate.is_file():
            return candidate
    return None


def run_cook(engine, project_dir, platform):
    """Headless cook of the project's assets into <project>/products (AssetTool --cook-all)."""
    tool = find_asset_tool(engine)
    if tool is None:
        print('  (AssetTool not built; skipping cook - assets must be pre-cooked)')
        return False
    cmd = [str(tool), '--project', str(project_dir), '--engine', str(tool.parent), '--cook-all', '--platform', platform]
    print('cook: {}'.format(' '.join(cmd)))
    result = subprocess.run(cmd, cwd=str(tool.parent))
    return result.returncode == 0


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


def stage_bundles(project_dir, preset, dest_products_dir):
    """Copy the selected cooked bundles and write products/bundles.json.

    Cooked bundles are read from <Project>/products/<bundle> (cook output);
    bundles without cooked output are still listed (the runtime falls back).
    """
    bundles = select_bundles(project_dir, preset)
    check_bundle_conflicts(bundles)
    dest_products_dir.mkdir(parents=True, exist_ok=True)
    for name in bundles:
        src = project_dir / 'products' / name
        if src.is_dir():
            shutil.copytree(src, dest_products_dir / name, dirs_exist_ok=True)
        else:
            print('  (bundle "{}" not cooked; skipped copy)'.format(name))
    manifest = {
        'platform': preset.get('platform', ''),
        'distribution': preset.get('name', ''),
        'bundles': bundles,
    }
    (dest_products_dir / 'bundles.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    print('bundles.json: {}'.format(dest_products_dir / 'bundles.json'))
    return bundles


def run_cmake_preset(engine, project_dir, platform, build, extra, extra_build):
    native_dir = project_dir / 'native'
    env = os.environ.copy()
    env.setdefault('SKY_ENGINE_ROOT', str(engine))
    args = list(extra)
    if env.get('SKY_3RD_PATH'):
        args.append('-D3RD_PATH=' + env['SKY_3RD_PATH'])
    configure = ['cmake', '--preset', platform] + args
    print('配置: {} (cwd={})'.format(' '.join(configure), native_dir))
    result = subprocess.run(configure, cwd=str(native_dir), env=env)
    if result.returncode != 0:
        raise SystemExit('cmake 配置失败 (exit {})'.format(result.returncode))
    if build:
        build_cmd = ['cmake', '--build', '--preset', platform] + list(extra_build)
        print('构建: {} (cwd={})'.format(' '.join(build_cmd), native_dir))
        result = subprocess.run(build_cmd, cwd=str(native_dir), env=env)
        if result.returncode != 0:
            raise SystemExit('cmake 构建失败 (exit {})'.format(result.returncode))


def cmd_create(args):
    engine = Path(args.engine).resolve() if args.engine else REPO_ROOT
    project_dir = Path(args.dir).resolve() / args.name
    if project_dir.exists() and any(project_dir.iterdir()):
        if not args.force:
            raise SystemExit('目标已存在且非空: {} (用 --force 覆盖)'.format(project_dir))

    descriptor = {'name': args.name}
    tokens = base_tokens(descriptor)

    # game skeleton (native/ CMake entry + source module)
    instantiate_template(engine / 'engine' / 'templates' / 'game', project_dir, tokens)
    # platform launcher entries -> native/<platform>/
    launcher_root = engine / 'engine' / 'templates' / 'launcher'
    for platform in LAUNCHER_PLATFORMS:
        platform_dir = launcher_root / platform
        if platform_dir.is_dir():
            instantiate_template(platform_dir, project_dir / 'native' / platform, tokens)

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
            'applicationId': tokens['{{ANDROID_NAMESPACE}}'],
            'versionName': '1.0',
            'versionCode': 1,
        },
        'modules': [identifier + 'Module'],
        'plugins': [],
    }
    with (project_dir / (args.name + '.skyproj')).open('w', encoding='utf-8') as handle:
        json.dump(skyproj, handle, indent=2)
    print('工程: {}'.format(project_dir))
    return 0


def _resolve(project_arg, preset_name):
    skyproj, descriptor = load_descriptor(project_arg)
    project_dir = skyproj.parent
    preset = find_preset(project_dir, preset_name)
    credentials = load_json(project_dir / 'export_credentials.json', {})
    tokens = build_tokens(descriptor, preset, credentials)
    return project_dir, preset, tokens, credentials


def _generate(engine, project_dir, preset, tokens, credentials):
    platform = preset['platform']
    if platform == 'android':
        out_dir = project_dir / 'build' / 'android'
        instantiate_template(engine / 'engine' / 'templates' / 'ide' / 'android', out_dir, tokens)
        run_cook(engine, project_dir, 'android')
        stage_bundles(project_dir, preset, out_dir / 'app' / 'src' / 'main' / 'assets' / 'products')
    elif platform in ('ios', 'macos'):
        run_cmake_preset(engine, project_dir, platform, build=False,
                         extra=apple_cmake_args(preset, credentials, tokens['{{ANDROID_NAMESPACE}}']),
                         extra_build=preset.get('extraBuildArgs') or [])
    else:
        raise SystemExit('不支持的平台: {}'.format(platform))


def cmd_generate(args):
    engine = Path(args.engine).resolve() if args.engine else REPO_ROOT
    project_dir, preset, tokens, credentials = _resolve(args.project, args.preset)
    _generate(engine, project_dir, preset, tokens, credentials)
    return 0


def cmd_package(args):
    engine = Path(args.engine).resolve() if args.engine else REPO_ROOT
    project_dir, preset, tokens, credentials = _resolve(args.project, args.preset)
    platform = preset['platform']
    variant = preset.get('variant', 'debug')

    if platform == 'android':
        out_dir = project_dir / 'build' / 'android'
        instantiate_template(engine / 'engine' / 'templates' / 'ide' / 'android', out_dir, tokens)
        run_cook(engine, project_dir, 'android')
        stage_bundles(project_dir, preset, out_dir / 'app' / 'src' / 'main' / 'assets' / 'products')
        package_android(out_dir, project_dir, tokens, variant)
    elif platform in ('ios', 'macos'):
        run_cmake_preset(engine, project_dir, platform, build=True,
                         extra=apple_cmake_args(preset, credentials, tokens['{{ANDROID_NAMESPACE}}']),
                         extra_build=preset.get('extraBuildArgs') or [])
        run_cook(engine, project_dir, platform)
        stage_bundles(project_dir, preset, project_dir / 'build' / platform / 'bin' / 'products')
    else:
        raise SystemExit('不支持的平台: {}'.format(platform))
    return 0


def main():
    parser = argparse.ArgumentParser(description='SkyEngine 工程工具')
    sub = parser.add_subparsers(dest='command', required=True)

    p_create = sub.add_parser('create', help='从模板创建工程')
    p_create.add_argument('--dir', required=True, help='父目录')
    p_create.add_argument('--name', required=True, help='工程名')
    p_create.add_argument('--engine', type=str, help='引擎目录（默认仓库根）')
    p_create.add_argument('--force', action='store_true', default=False, help='目标非空时覆盖')
    p_create.set_defaults(func=cmd_create)

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
