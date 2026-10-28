#!/usr/bin/env python3
"""Create one lean, real template project for APK/unsigned IPA/macOS package smoke tests.

This only creates source/configuration. Build it with the printed public commands,
setting SKY_3RD_PATH to the corresponding bootstrap output for each platform.
"""

import argparse
import importlib.util
import json
from pathlib import Path
import platform
import shlex
import shutil
from types import SimpleNamespace

SCRIPT = Path(__file__).resolve().parents[1] / 'package_project.py'
SPEC = importlib.util.spec_from_file_location('package_project', SCRIPT)
PACKAGE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PACKAGE)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--dir', required=True, type=Path, help='Parent directory; the new project must not already exist')
    parser.add_argument('--name', default='PlatformSmoke')
    parser.add_argument('--scene', choices=['cube-plane'], help='Include a World consumed by the standard game startup')
    args = parser.parse_args(argv)
    PACKAGE.cmd_create(SimpleNamespace(engine=None, dir=args.dir, name=args.name, force=False))
    project = args.dir.resolve() / args.name
    identifier = PACKAGE.sanitize_identifier(args.name)
    descriptor_path = project / (args.name + '.skyproj')
    descriptor = json.loads(descriptor_path.read_text(encoding='utf-8'))
    (project / 'configs/modules_game.json').write_text(json.dumps({
        'modules': [
            {'name': 'Aurora.Render', 'dependencies': ['Aurora.Metadata']},
            {'name': 'Content.Module', 'dependencies': ['Content.Metadata']},
            {'name': identifier + 'Module', 'dependencies': ['Aurora.Render', 'Content.Module']},
        ]
    }, indent=2) + '\n', encoding='utf-8')
    (project / 'configs/package_smoke.json').write_text(json.dumps({
        'project': args.name, 'purpose': 'package-smoke', 'schema': 1
    }, indent=2) + '\n', encoding='utf-8')
    presets_path = project / 'export_presets.json'
    presets = json.loads(presets_path.read_text(encoding='utf-8'))
    world_path = None
    if args.scene == 'cube-plane':
        world_path = 'scenes/cube_plane.world'
        destination = project / 'assets' / world_path
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(Path(__file__).with_name('fixtures') / 'cube_plane.world', destination)
        descriptor['defaultScene'] = world_path
        descriptor_path.write_text(json.dumps(descriptor, indent=2) + '\n', encoding='utf-8')
    disabled = ('JOLT', 'RECAST', 'TERRAIN', 'VEGETATION', 'AUDIO', 'NETWORK_ENET',
                'PVS', 'COMPRESSION', 'ANIMATION', 'CONSOLE', 'SANDBOX', 'TEST', 'TOOL')
    for preset in presets:
        preset['cacheVariables'] = {'SKY_BUILD_' + name: 'OFF' for name in disabled}
        preset['extraBuildArgs'] = ['--target', 'Launcher', '--parallel', '2']
        if world_path:
            preset['worlds'] = [world_path]
        if preset['platform'] == 'android':
            preset['cacheVariables'].update({
                'CMAKE_JOB_POOLS': 'sky_package=2',
                'CMAKE_JOB_POOL_COMPILE': 'sky_package',
                'CMAKE_JOB_POOL_LINK': 'sky_package',
            })
        if preset['platform'] == 'ios':
            preset['packing'] = 'static'
            preset['cacheVariables']['CMAKE_OSX_DEPLOYMENT_TARGET'] = '13.0'
        if preset['platform'] == 'macos':
            preset['cacheVariables']['CMAKE_OSX_ARCHITECTURES'] = platform.machine()
    presets_path.write_text(json.dumps(presets, indent=2) + '\n', encoding='utf-8')
    ios = next(preset for preset in presets if preset['platform'] == 'ios')
    simulator_variables = dict(ios['cacheVariables'],
                               SKY_MODULE_PACKAGING=ios['packing'],
                               SKY_SHADER_DISTRIBUTION=ios['name'],
                               SKY_BUNDLE_ID=descriptor['settings']['applicationId'],
                               CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED='NO',
                               CODE_SIGNING_ALLOWED='NO', CODE_SIGNING_REQUIRED='NO')
    simulator = {
        'version': 3,
        'configurePresets': [{
            'name': 'ios-simulator-smoke', 'inherits': 'ios-simulator',
            'cacheVariables': simulator_variables,
        }],
        'buildPresets': [{
            'name': 'ios-simulator-smoke', 'configurePreset': 'ios-simulator-smoke',
            'configuration': 'Release',
        }],
    }
    (project / 'native/CMakeUserPresets.json').write_text(json.dumps(simulator, indent=2) + '\n', encoding='utf-8')
    for preset in presets:
        command = ['python3', str(SCRIPT), 'package', '-p', str(project), '--preset', preset['name']]
        print(shlex.join(command))
    print_simulator_commands(project)
    return 0


def print_simulator_commands(project):
    print('# iOS Simulator: use a separate dependency output and intermediate tree.')
    print('export SKY_IOS_SIMULATOR_ARCH="${SKY_IOS_SIMULATOR_ARCH:-$(uname -m)}"')
    print('case "$SKY_IOS_SIMULATOR_ARCH" in')
    print('  arm64) sky_smoke_simulator_platform=SIMULATORARM64 ;;')
    print('  x86_64) sky_smoke_simulator_platform=SIMULATOR64 ;;')
    print('  *) echo "Unsupported simulator architecture" >&2; exit 1 ;;')
    print('esac')
    print('sky_smoke_simulator_deps=' + shlex.quote(str(project / 'build/simulator-thirdparty')) + '/"$SKY_IOS_SIMULATOR_ARCH"')
    print('sky_smoke_simulator_intermediate=' + shlex.quote(str(project / 'build/simulator-thirdparty-intermediate'))
          + '/"$SKY_IOS_SIMULATOR_ARCH"')
    print('export SKY_3RD_CMAKE_ARGS="-DPLATFORM=$sky_smoke_simulator_platform '
          '-DARCHS=$SKY_IOS_SIMULATOR_ARCH -DCMAKE_OSX_ARCHITECTURES=$SKY_IOS_SIMULATOR_ARCH '
          '-DCMAKE_OSX_SYSROOT=iphonesimulator -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=NO"')
    command = ['python3', str(SCRIPT.with_name('third_party.py')), '-p', 'IOS']
    print(shlex.join(command) + ' -o "$sky_smoke_simulator_deps" -i "$sky_smoke_simulator_intermediate"')
    print('unset SKY_3RD_CMAKE_ARGS')
    print('export SKY_ENGINE_ROOT=' + shlex.quote(str(PACKAGE.REPO_ROOT)))
    print('export SKY_3RD_PATH="$sky_smoke_simulator_deps/IOS"')
    print('cd ' + shlex.quote(str(project / 'native')))
    print('cmake --preset ios-simulator-smoke -D3RD_PATH="$SKY_3RD_PATH"')
    print('cmake --build --preset ios-simulator-smoke --target Launcher --parallel 2')


if __name__ == '__main__':
    raise SystemExit(main())
