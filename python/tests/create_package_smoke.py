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
from types import SimpleNamespace

SCRIPT = Path(__file__).resolve().parents[1] / 'package_project.py'
SPEC = importlib.util.spec_from_file_location('package_project', SCRIPT)
PACKAGE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PACKAGE)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--dir', required=True, type=Path, help='Parent of the new project (must not already exist)')
    parser.add_argument('--name', default='PlatformSmoke')
    args = parser.parse_args()
    PACKAGE.cmd_create(SimpleNamespace(engine=None, dir=args.dir, name=args.name, force=False))
    project = args.dir.resolve() / args.name
    identifier = PACKAGE.sanitize_identifier(args.name)
    (project / 'configs/modules_game.json').write_text(json.dumps({
        'modules': [{'name': identifier + 'Module', 'dependencies': []}]
    }, indent=2) + '\n', encoding='utf-8')
    (project / 'configs/package_smoke.json').write_text(json.dumps({
        'project': args.name, 'purpose': 'package-smoke', 'schema': 1
    }, indent=2) + '\n', encoding='utf-8')
    presets_path = project / 'export_presets.json'
    presets = json.loads(presets_path.read_text(encoding='utf-8'))
    disabled = ('JOLT', 'RECAST', 'TERRAIN', 'VEGETATION', 'AUDIO', 'NETWORK_ENET',
                'PVS', 'COMPRESSION', 'SANDBOX', 'TEST', 'TOOL')
    for preset in presets:
        preset['cacheVariables'] = {'SKY_BUILD_' + name: 'OFF' for name in disabled}
        preset['extraBuildArgs'] = ['--target', identifier + 'Deploy', '--parallel', '2']
        if preset['platform'] == 'android':
            preset['cacheVariables'].update({
                'CMAKE_JOB_POOLS': 'sky_package=2',
                'CMAKE_JOB_POOL_COMPILE': 'sky_package',
                'CMAKE_JOB_POOL_LINK': 'sky_package',
            })
        if preset['platform'] == 'ios':
            preset['packing'] = 'static'
        if preset['platform'] == 'macos':
            preset['cacheVariables']['CMAKE_OSX_ARCHITECTURES'] = platform.machine()
    presets_path.write_text(json.dumps(presets, indent=2) + '\n', encoding='utf-8')
    for preset in presets:
        command = ['python3', str(SCRIPT), 'package', '-p', str(project), '--preset', preset['name']]
        print(shlex.join(command))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
