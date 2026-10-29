#!/usr/bin/env python3
"""Exercise batch cooking of images, binary World products and unsupported sources.

Requires the real AssetTool and its deployed builder modules. Builds/downloads
nothing. Fixtures and process logs are kept in --work-dir (a temporary directory
by default). Engine assets are isolated from the project fixtures.
"""

import argparse
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import uuid
import zlib

if __package__:
    from .asset_product_test_helpers import product_body
else:
    from asset_product_test_helpers import product_body

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


def write_world(path, actors=None):
    path.write_text(json.dumps({'actors': actors or [], 'subSystems': [], 'assetRoots': []}), encoding='utf-8')


def check_world_product(project, diagnostic):
    products = []
    for product in (project / 'products/common').glob('*/*.bin'):
        data = product.read_bytes()
        try:
            body = product_body(data, product.stem)
        except ValueError as error:
            raise RuntimeError(f'Invalid flat product {product}: {error}; {diagnostic}') from error
        if len(body) < 4:
            raise RuntimeError(f'Truncated product type: {product}; {diagnostic}')
        length = struct.unpack_from('<I', body)[0]
        if length > len(body) - 4:
            raise RuntimeError(f'Invalid product type length: {product}; {diagnostic}')
        if body[4:4 + length] == b'World':
            products.append((product, body))
    if len(products) != 1 or not (project / 'products/common/product.index').is_file():
        raise RuntimeError(f'World was not published as an indexed binary product: {diagnostic}')
    if b'"actors"' in products[0][1] or b'"subSystems"' in products[0][1]:
        raise RuntimeError(f'World product retained authoring JSON instead of a binary payload: {diagnostic}')
    return products[0][0]


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
    write_world(project / 'assets/lighting.world')
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
        if result.returncode != 0 or "cook-all: 2 product(s), 0 failed for platform 'android'" not in log:
            raise RuntimeError(f'Valid source did not produce a successful batch: {diagnostic}')
        if not list((project / 'products/common').glob('*/*.bin')):
            raise RuntimeError(f'Successful batch did not write a product: {diagnostic}')
    elif (result.returncode != 1
          or "cook-all: 1 product(s), 1 failed for platform 'android'" not in log
          or "cook-all: failed 'pixel.png' target 'common'" not in log):
        raise RuntimeError(f'Rejected source did not report a failed batch and nonzero exit code: {diagnostic}')
    check_world_product(project, diagnostic)
    return {'name': name, 'exitCode': result.returncode, 'command': command, 'log': str(path)}


def run_document_case(tool, root, engine, logs, missing_builder):
    name = 'missing-builder' if missing_builder else 'world-only'
    project = root / name
    (project / 'assets').mkdir(parents=True)
    (project / 'configs').mkdir()
    (project / 'configs/asset_cook.jsonc').write_text(json.dumps({
        'bundles': ['common'], 'presets': {'ios': ['common']},
    }), encoding='utf-8')
    write_world(project / 'assets/lighting.world')
    if missing_builder:
        (project / 'assets/unsupported.source').write_bytes(b'unsupported asset')
        # Seed a known source that has no registered builder; source scans normally filter unknown extensions.
        (project / 'assets/assets.db').write_text(json.dumps({'assets': [{
            'uuid': '11111111-2222-4333-8444-555555555555', 'markedName': '',
            'ext': '.source', 'path': 'unsupported.source', 'dependencies': [],
        }]}), encoding='utf-8')
    command = [str(tool), '--project', str(project), '--engine', str(engine),
               '--cook-all', '--platform', 'ios']
    result = subprocess.run(command, cwd=tool.parent, capture_output=True, text=True, timeout=120)
    log = result.stdout + result.stderr
    path = logs / f'asset-tool-batch-{name}.log'
    path.write_text(log, encoding='utf-8')
    diagnostic = f'exit {result.returncode}; {path}'
    if missing_builder:
        if (result.returncode != 1
                or "no builder for extension '.source'" not in log
                or "cook-all: 1 product(s), 1 failed for platform 'ios'" not in log):
            raise RuntimeError(f'Unsupported source was not rejected: {diagnostic}')
    elif result.returncode != 0 or "cook-all: 1 product(s), 0 failed for platform 'ios'" not in log:
        raise RuntimeError(f'World-only project did not produce a cooked World: {diagnostic}')
    check_world_product(project, diagnostic)
    return {'name': name, 'exitCode': result.returncode, 'command': command, 'log': str(path)}


def run_invalid_world_case(tool, root, engine, logs, unknown_component):
    name = 'unknown-world-component' if unknown_component else 'invalid-world-document'
    project = root / name
    (project / 'assets').mkdir(parents=True)
    (project / 'configs').mkdir()
    (project / 'configs/asset_cook.jsonc').write_text(json.dumps({
        'bundles': ['common'], 'presets': {'ios': ['common']},
    }), encoding='utf-8')
    world = project / 'assets/rejected.world'
    if unknown_component:
        write_world(world, [{
            'uuid': '11111111-2222-4333-8444-555555555555', 'name': 'Rejected',
            'components': [{'type': '22222222-3333-4444-8555-666666666666', 'data': {}}],
        }])
    else:
        world.write_text('{"actors": "invalid", "subSystems": [], "assetRoots": []}', encoding='utf-8')
    command = [str(tool), '--project', str(project), '--engine', str(engine), '--cook-all', '--platform', 'ios']
    result = subprocess.run(command, cwd=tool.parent, capture_output=True, text=True, timeout=120)
    log = result.stdout + result.stderr
    path = logs / f'asset-tool-batch-{name}.log'
    path.write_text(log, encoding='utf-8')
    diagnostic = f'exit {result.returncode}; {path}'
    if (result.returncode != 1 or "cook-all: 0 product(s), 1 failed for platform 'ios'" not in log
            or "cook-all: failed 'rejected.world' target 'common'" not in log):
        raise RuntimeError(f'Invalid World was silently accepted: {diagnostic}')
    if list((project / 'products/common').glob('*/*.bin')):
        raise RuntimeError(f'Invalid World published a product: {diagnostic}')
    return {'name': name, 'exitCode': result.returncode, 'command': command, 'log': str(path)}


def run_physics_world_case(tool, root, engine, logs, platform, rejection=None):
    name = f'physics-world-{platform}' + (f'-{rejection}' if rejection else '')
    project = root / name
    (project / 'assets').mkdir(parents=True)
    (project / 'configs').mkdir()
    (project / 'configs/asset_cook.jsonc').write_text(json.dumps({
        'bundles': ['common'], 'presets': {platform: ['common']},
    }), encoding='utf-8')
    world = json.loads((REPOSITORY / 'python/tests/fixtures/jolt_physics.world').read_text(encoding='utf-8'))
    physics = world['actors'][0]['components'][1]
    if rejection == 'invalid-config':
        world['subSystems'][0]['config']['elements']['deterministic'] = True
    elif rejection == 'missing-field':
        del physics['data']['elements']['shapeSource']
    (project / 'assets/physics.world').write_text(json.dumps(world), encoding='utf-8')
    command = [str(tool), '--project', str(project), '--engine', str(engine), '--cook-all', '--platform', platform]
    result = subprocess.run(command, cwd=tool.parent, capture_output=True, text=True, timeout=120)
    text = result.stdout + result.stderr
    log = logs / f'asset-tool-batch-{name}.log'
    log.write_text(text, encoding='utf-8')
    diagnostic = f'exit {result.returncode}; {log}'
    if rejection:
        reason = ('Deterministic physics is not supported' if rejection == 'invalid-config'
                  else 'World object fields disagree with its schema')
        if result.returncode != 1 or reason not in text or f"cook-all: 0 product(s), 1 failed for platform '{platform}'" not in text:
            raise RuntimeError(f'Invalid physics World did not fail through its registered schema: {diagnostic}')
        if list((project / 'products/common').glob('*/*.bin')):
            raise RuntimeError(f'Invalid physics World published a product: {diagnostic}')
    else:
        if (result.returncode != 0 or f"cook-all: 1 product(s), 0 failed for platform '{platform}'" not in text
                or 'JoltPhysicsModule' not in text):
            raise RuntimeError(f'Physics World did not cook through its existing metadata owner: {diagnostic}')
        product = check_world_product(project, diagnostic)
        body = product_body(product.read_bytes(), product.stem)
        offset = 4 + struct.unpack_from('<I', body)[0]
        dependencies = struct.unpack_from('<I', body, offset)[0]
        offset += 4 + dependencies * 16
        codec_bytes = struct.unpack_from('<I', body, offset)[0]
        offset += 4
        if codec_bytes:
            raise RuntimeError(f'Default uncompressed World fixture unexpectedly used a codec: {diagnostic}')
        payload_bytes = struct.unpack_from('<I', body, offset)[0]
        offset += 4
        payload = body[offset:offset + payload_bytes]
        if len(payload) != payload_bytes or payload[:4] != b'WRLD':
            raise RuntimeError(f'Physics World was not encoded as a complete WRLD payload: {diagnostic}')
        for identity in (physics['type'], physics['data']['classId'], world['subSystems'][0]['config']['classId']):
            if uuid.UUID(identity).bytes not in payload:
                raise RuntimeError(f'Cooked World omitted a physics component/config schema: {diagnostic}')
    return {'name': name, 'exitCode': result.returncode, 'command': command, 'log': str(log)}


def run_symlink_world_case(tool, root, engine, logs):
    project = root / 'internal-world-symlink'
    (project / 'assets/shared').mkdir(parents=True)
    (project / 'assets/scenes').mkdir()
    (project / 'configs').mkdir()
    (project / 'configs/asset_cook.jsonc').write_text(json.dumps({
        'bundles': ['common'], 'presets': {'ios': ['common']},
    }), encoding='utf-8')
    write_world(project / 'assets/shared/template.source')
    logical_path = 'scenes/start.world'
    try:
        (project / 'assets' / logical_path).symlink_to('../shared/template.source')
    except OSError as error:
        raise RuntimeError(f'Cannot create the internal World symlink fixture: {error}') from error
    command = [str(tool), '--project', str(project), '--engine', str(engine), '--cook-all', '--platform', 'ios']
    result = subprocess.run(command, cwd=tool.parent, capture_output=True, text=True, timeout=120)
    log = logs / 'asset-tool-batch-internal-world-symlink.log'
    output = result.stdout + result.stderr
    log.write_text(output, encoding='utf-8')
    diagnostic = f'exit {result.returncode}; {log}'
    if result.returncode != 0 or "cook-all: 1 product(s), 0 failed for platform 'ios'" not in output:
        raise RuntimeError(f'Internal World symlink did not cook successfully: {diagnostic}')
    check_world_product(project, diagnostic)
    index = project / 'products/common/product.index'
    entries = [json.loads(line) for line in index.read_text(encoding='utf-8').splitlines() if line.strip()]
    if [entry.get('path') for entry in entries] != [logical_path]:
        raise RuntimeError(f'World product index lost the logical symlink identity: {diagnostic}; {entries}')
    cases = [{'name': 'internal-world-symlink-cook', 'exitCode': result.returncode, 'command': command, 'log': str(log)}]
    manifest = project / 'products/asset-roots.json'
    manifest.write_text(json.dumps({'bundles': ['common'], 'roots': [{'path': logical_path, 'type': 'World'}]}), encoding='utf-8')
    command = [str(tool), '--audit-products', str(manifest)]
    result = subprocess.run(command, cwd=tool.parent, capture_output=True, text=True, timeout=120)
    log = logs / 'asset-tool-audit-internal-world-symlink.log'
    log.write_text(result.stdout + result.stderr, encoding='utf-8')
    if result.returncode != 0:
        raise RuntimeError(f'Cooked World could not be audited by its logical symlink path: exit {result.returncode}; {log}')
    cases.append({'name': 'internal-world-symlink-audit', 'exitCode': result.returncode, 'command': command, 'log': str(log)})
    return cases


def run_world_audit_cases(tool, project, logs):
    products = project / 'products'
    manifest = products / 'asset-roots.json'
    cases = []
    roots = (
        ('world-path', {'path': 'lighting.world', 'type': 'World'}, True),
        ('missing-world-path', {'path': 'missing.world', 'type': 'World'}, False),
        ('world-type-mismatch', {'path': 'lighting.world', 'type': 'AuroraMesh'}, False),
        ('ambiguous-world-root', {'path': 'lighting.world', 'uuid': '11111111-2222-4333-8444-555555555555', 'type': 'World'}, False),
        ('unsafe-world-path', {'path': '../lighting.world', 'type': 'World'}, False),
    )
    world_product = check_world_product(project, 'before package audit')
    original = world_product.read_bytes()
    malformed = (
        ('unsupported-product-version', original[:4] + struct.pack('<I', 2) + original[8:]),
        ('bad-product-checksum', original[:32] + bytes([original[32] ^ 1]) + original[33:]),
        ('bad-product-body', original[:-1] + bytes([original[-1] ^ 1])),
    )
    for name, root, valid in (*roots,
                              *((name, {'path': 'lighting.world', 'type': 'World'}, False) for name, _ in malformed),
                              ('missing-world-product', {'path': 'lighting.world', 'type': 'World'}, False)):
        world_product.write_bytes(dict(malformed).get(name, original))
        if name == 'missing-world-product':
            world_product.unlink()
        manifest.write_text(json.dumps({'bundles': ['common'], 'roots': [root]}), encoding='utf-8')
        command = [str(tool), '--audit-products', str(manifest)]
        result = subprocess.run(command, cwd=tool.parent, capture_output=True, text=True, timeout=120)
        log = logs / f'asset-tool-audit-{name}.log'
        log.write_text(result.stdout + result.stderr, encoding='utf-8')
        if result.returncode != (0 if valid else 1):
            raise RuntimeError(f'World product audit {name} returned {result.returncode}; {log}')
        if valid:
            report = json.loads((products / 'asset-closure.json').read_text(encoding='utf-8'))
            if ('groups' in report or len(report['products']) != 1
                    or report['products'][0]['formatVersion'] != 1
                    or report['products'][0]['checksum'] != struct.unpack_from('<Q', original, 32)[0]):
                raise RuntimeError(f'World product audit omitted the verified envelope identity: {log}')
        cases.append({'name': name, 'exitCode': result.returncode, 'command': command, 'log': str(log)})
    return cases


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--asset-tool', type=Path, help='Built AssetTool executable; default: repository output/bin')
    parser.add_argument('--work-dir', type=Path, help='Keep generated fixtures and process logs here')
    parser.add_argument('--with-jolt', action='store_true', help='Also require real physics World cooking through deployed Jolt modules')
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
        for missing_builder in (False, True):
            report['cases'].append(run_document_case(tool, root, engine, logs, missing_builder))
        report['cases'].extend(run_world_audit_cases(tool, root / 'world-only', logs))
        report['cases'].extend(run_symlink_world_case(tool, root, engine, logs))
        for unknown_component in (False, True):
            report['cases'].append(run_invalid_world_case(tool, root, engine, logs, unknown_component))
        if args.with_jolt:
            for platform in ('ios', 'android'):
                report['cases'].append(run_physics_world_case(tool, root, engine, logs, platform))
            for rejection in ('invalid-config', 'missing-field'):
                report['cases'].append(run_physics_world_case(tool, root, engine, logs, 'android', rejection))
    except (RuntimeError, subprocess.SubprocessError) as error:
        parser.exit(1, str(error) + '\n')
    (logs / 'asset-tool-batch-integration.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report, indent=2))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
