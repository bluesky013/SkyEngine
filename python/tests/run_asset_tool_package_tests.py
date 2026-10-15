#!/usr/bin/env python3
"""Exercise the built AssetTool's batch cook with valid and rejected source images.

Requires the real AssetTool and its deployed builder modules. Builds/downloads
nothing. Fixtures and process logs are kept in --work-dir (a temporary directory
by default). Engine assets are isolated so each case cooks exactly one source.
"""

import argparse
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import zlib

REPOSITORY = Path(__file__).resolve().parents[2]


def png_chunk(kind, data):
    return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))


def find_asset_tool():
    name = 'AssetTool.exe' if os.name == 'nt' else 'AssetTool'
    for configuration in ('Debug', 'Release', ''):
        candidate = REPOSITORY / 'output/bin' / configuration / name
        if candidate.is_file():
            return candidate
    return None


def run_case(tool, root, engine, logs, valid):
    name = 'positive' if valid else 'negative'
    project = root / name
    (project / 'assets').mkdir(parents=True)
    (project / 'configs').mkdir()
    (project / 'configs/asset_cook.jsonc').write_text(json.dumps({
        'bundles': ['common'], 'presets': {'android': ['common']},
    }), encoding='utf-8')
    # Project image settings must override deployed engine defaults.
    # A missing project mesh preset must still resolve through engine defaults.
    (project / 'configs/image_build_presets.json').write_text(json.dumps({
        'defaultBundle': 'common',
        'bundles': {'common': {'encode': 'NONE', 'srgb': False,
                               'maxSize': 1, 'generateMip': False}},
    }), encoding='utf-8')
    image = (b'\x89PNG\r\n\x1a\n'
             + png_chunk(b'IHDR', struct.pack('>IIBBBBB', 1, 1, 8, 6, 0, 0, 0))
             + png_chunk(b'IDAT', zlib.compress(b'\x00\xff\x00\x00\xff'))
             + png_chunk(b'IEND', b''))
    (project / 'assets/pixel.png').write_bytes(image if valid else b'not a PNG image')
    command = [str(tool), '--project', str(project), '--engine', str(engine),
               '--cook-all', '--platform', 'android']
    result = subprocess.run(command, cwd=tool.parent, capture_output=True, text=True, timeout=120)
    log = result.stdout + result.stderr
    path = logs / f'asset-tool-batch-{name}.log'
    path.write_text(log, encoding='utf-8')
    diagnostic = f'exit {result.returncode}; {path}'
    if ("loaded 1 image build bundles (default 'common')" not in log
            or 'config: encode=NONE srgb=0' not in log
            or 'maxSize=1 generateMip=0' not in log):
        raise RuntimeError(f'Project builder configuration was not loaded after module registration: {diagnostic}')
    if 'mesh build bundles' not in log:
        raise RuntimeError(f'Engine builder configuration fallback was not loaded: {diagnostic}')
    if valid:
        if result.returncode != 0 or "cook-all: 1 product(s), 0 failed for platform 'android'" not in log:
            raise RuntimeError(f'Valid source did not produce a successful batch: {diagnostic}')
        if not list((project / 'products/common').glob('*/*.bin')):
            raise RuntimeError(f'Successful batch did not write a product: {diagnostic}')
    elif (result.returncode != 1
          or "cook-all: 0 product(s), 1 failed for platform 'android'" not in log
          or "cook-all: failed 'pixel.png' target 'common'" not in log):
        raise RuntimeError(f'Rejected source did not report a failed batch and nonzero exit code: {diagnostic}')
    return {'name': name, 'exitCode': result.returncode, 'command': command, 'log': str(path)}


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--asset-tool', type=Path, help='Built AssetTool executable; default: repository output/bin')
    parser.add_argument('--work-dir', type=Path, help='Keep generated fixtures and process logs here')
    args = parser.parse_args()
    tool = args.asset_tool or find_asset_tool()
    if tool is None or not tool.is_file():
        parser.error('Build AssetTool with its builder modules first, or pass --asset-tool')
    tool = tool.resolve()
    logs = (args.work_dir or Path(tempfile.mkdtemp(prefix='skyengine-assettool-integration-'))).resolve()
    logs.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix='fixtures-', dir=logs))
    engine = root / 'engine'
    shutil.copytree(REPOSITORY / 'configs', engine / 'configs')
    (engine / 'assets').mkdir()
    print(f'Artifacts: {logs}', flush=True)
    report = {'tool': str(tool), 'fixtures': str(root), 'cases': []}
    try:
        for valid in (True, False):
            report['cases'].append(run_case(tool, root, engine, logs, valid))
    except (RuntimeError, subprocess.SubprocessError) as error:
        parser.exit(1, str(error) + '\n')
    (logs / 'asset-tool-batch-integration.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report, indent=2))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
