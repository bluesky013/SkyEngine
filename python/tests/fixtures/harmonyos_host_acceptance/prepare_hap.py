"""Prepare an isolated ignored HAP bridge without changing the game project."""

import argparse
import json
from pathlib import Path
import shutil


FIXTURE = Path(__file__).resolve().parent
ROOT = FIXTURE.parents[3]


def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--template-hap', type=Path, required=True)
    parser.add_argument('--native-library', type=Path, required=True)
    parser.add_argument('--native-sdk', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    template = args.template_hap.resolve()
    output = args.output.resolve()
    ignored_build = (ROOT / 'build').resolve()
    if not output.is_relative_to(ignored_build) or output == ignored_build:
        parser.error('--output must be a new directory below the engine build directory')
    if output.exists():
        parser.error('--output already exists; choose a fresh isolated directory')
    library = args.native_library.resolve()
    runtime = args.native_sdk.resolve() / 'llvm/lib/aarch64-linux-ohos/libc++_shared.so'
    required_files = ['build-profile.json5', 'oh-package.json5', 'hvigorfile.ts',
                      'entry/hvigorfile.ts', 'entry/src/main/module.json5']
    for path in [library, runtime, *(template / name for name in required_files)]:
        if not path.is_file():
            parser.error('required input missing: ' + str(path))
    for name in ('AppScope', 'hvigor', 'entry/src/main/resources'):
        if not (template / name).is_dir():
            parser.error('required template directory missing: ' + name)
    output.mkdir(parents=True)
    for name in required_files:
        destination = output / name
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(template / name, destination)
    for name in ('AppScope', 'hvigor', 'entry/src/main/resources'):
        shutil.copytree(template / name, output / name)
    package = json.loads((output / 'oh-package.json5').read_text(encoding='utf-8'))
    package['name'] = 'OHOSHostAcceptance'
    package['dependencies'] = {}
    write_json(output / 'oh-package.json5', package)
    write_json(output / 'entry/build-profile.json5', {
        'apiType': 'stageMode', 'targets': [{'name': 'default'}],
        'buildOption': {'nativeLib': {'collectAllLibs': True}}})
    write_json(output / 'entry/oh-package.json5', {
        'name': 'entry', 'version': '1.0.0',
        'dependencies': {'libOHOSHostAcceptance.so': 'file:./types/libOHOSHostAcceptance'}})
    types = output / 'entry/types/libOHOSHostAcceptance'
    write_json(types / 'oh-package.json5', {
        'name': 'libOHOSHostAcceptance.so', 'version': '1.0.0', 'types': './index.d.ts'})
    for source, destination in (
            ('index.d.ts', types / 'index.d.ts'),
            ('Index.ets', output / 'entry/src/main/ets/pages/Index.ets'),
            ('EntryAbility.ets', output / 'entry/src/main/ets/entryability/EntryAbility.ets')):
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(FIXTURE / source, destination)
    libraries = output / 'entry/libs/arm64-v8a'
    libraries.mkdir(parents=True)
    shutil.copy2(library, libraries / 'libOHOSHostAcceptance.so')
    shutil.copy2(runtime, libraries / runtime.name)
    print('Prepared isolated Host acceptance HAP: ' + str(output))


if __name__ == '__main__':
    main()
