"""Project shader cooking through the same module resource graph as development."""

import json
from pathlib import Path
import shutil
import subprocess
import tempfile

from module_resources import collect, stage, write_changed
from shader_headers import logical_path


TARGETS = {'vulkan': 0, 'metal': 1, 'd3d12': 2}
STAGES = {'vertex': 1, 'fragment': 2, 'compute': 4}


def project_modules(engine, project, descriptor):
    configs = [engine / 'engine/aurora/resources.json']
    for name in descriptor.get('plugins', []):
        if isinstance(name, dict):
            name = name.get('id', '')
        if not logical_path(name) or '/' in name:
            return None, f'Invalid project plugin name: {name}'
        candidates = [base / 'plugins' / name / 'plugin.json' for base in (project, engine)]
        existing = [path for path in candidates if path.is_file()]
        if len(existing) != 1:
            return None, f'Missing or ambiguous project plugin description: {name}'
        data = json.loads(existing[0].read_text(encoding='utf-8'))
        if 'resources' in data:
            configs.append(existing[0])
    for name in descriptor.get('resourceModules', []):
        if not logical_path(name):
            return None, f'Invalid project resource descriptor: {name}'
        path = (project / name).resolve()
        if not path.is_relative_to(project.resolve()) or not path.is_file():
            return None, f'Missing project resource descriptor: {name}'
        configs.append(path)
    owners = {}
    for config in configs:
        resources = json.loads(config.read_text(encoding='utf-8')).get('resources', {})
        owner = resources.get('owner', '')
        if owner in owners and owners[owner] != config:
            return None, f'Duplicate project resource owner: {owner}'
        owners[owner] = config
    index = 0
    while index < len(configs):
        config = configs[index]
        index += 1
        resources = json.loads(config.read_text(encoding='utf-8')).get('resources', {})
        for root in resources.get('roots', []):
            for dependency in root.get('dependencies', []):
                if not isinstance(dependency, str) or not logical_path(dependency) or '/' in dependency:
                    return None, f'Invalid resource dependency in {config}'
                if dependency in owners:
                    continue
                candidates = [base / 'plugins' / dependency / 'plugin.json' for base in (project, engine)]
                existing = [path for path in candidates if path.is_file()]
                if len(existing) != 1:
                    return None, f'Missing or ambiguous resource dependency: {dependency}; register its descriptor in resourceModules'
                owner = json.loads(existing[0].read_text(encoding='utf-8')).get('resources', {}).get('owner')
                if owner != dependency:
                    return None, f'Resource dependency owner mismatch: {dependency}'
                owners[dependency] = existing[0]
                configs.append(existing[0])
    return configs, ''


def append_program(program, targets, available, entries):
    if not isinstance(program, dict):
        return 'Shader program requirement must be an object'
    path = program.get('path', '')
    stages = program.get('entries', [])
    variants = program.get('variants', [{}])
    if not isinstance(variants, list) or not variants or any(not isinstance(variant, dict) for variant in variants):
        return f'Invalid shader variant requirements: {path}'
    for variant in variants:
        if any(not isinstance(name, str) or not name.isidentifier() or not isinstance(value, int) or isinstance(value, bool)
               or value < 0 or value > 0xffffffff for name, value in variant.items()):
            return f'Invalid shader variant value: {path}'
    if path not in available or not path.endswith('.slang') or not isinstance(stages, list) or not stages:
        return f'Missing shader program or entries: {path}'
    for entry in stages:
        if not isinstance(entry, dict) or entry.get('stage') not in STAGES or not isinstance(entry.get('name'), str) or not entry['name']:
            return f'Invalid shader entry: {path}'
        for target in targets:
            for variant in variants:
                request = {'path': path, 'entry': entry['name'], 'stage': STAGES[entry['stage']],
                           'target': TARGETS[target], 'variant': variant}
                key = (path, entry['name'], request['stage'], request['target'], tuple(sorted(variant.items())))
                entries[key] = request
    return ''


def project_requirements(project, configs, targets, bundles, available):
    entries = {}
    for config in configs:
        resources = json.loads(config.read_text(encoding='utf-8'))['resources']
        for root in resources['roots']:
            if root.get('kind') != 'shader':
                continue
            for program in root.get('programs', []):
                if not isinstance(program, dict):
                    return None, f'Invalid shader program requirement: {config}'
                if program.get('bundle', root.get('bundle', 'common')) not in bundles:
                    continue
                error = append_program(program, targets, available, entries)
                if error:
                    return None, error
    requirements = project / 'configs/shader_requirements.json'
    if requirements.is_file():
        data = json.loads(requirements.read_text(encoding='utf-8'))
        for program in data.get('programs', []):
            if program.get('bundle', 'common') in bundles:
                error = append_program(program, targets, available, entries)
                if error:
                    return None, error
    # Material sources are authored JSON; their technique requests do not require loading runtime modules.
    for path in sorted((project / 'assets').rglob('*.mat')):
        material = json.loads(path.read_text(encoding='utf-8'))
        if material.get('bundle', 'common') not in bundles:
            continue
        for technique in material.get('techniques', []):
            program = technique.get('program', {})
            if not isinstance(program, dict) or 'vertex' not in program or 'fragment' not in program or 'compute' in program:
                return None, f'Invalid material technique; explicit vertex/fragment program required: {path}'
            for stage in ('vertex', 'fragment'):
                shader = program[stage]
                if not isinstance(shader, dict):
                    return None, f'Invalid material shader reference: {path}'
                request = {'path': shader.get('relativePath', ''), 'entries': [
                    {'stage': stage, 'name': shader.get('entry', '')}], 'variants': [program.get('variant', {})]}
                error = append_program(request, targets, available, entries)
                if error:
                    return None, f'{path}: {error}'
    return {'entries': [entries[key] for key in sorted(entries)]}, ''


def cook_project(engine, project, descriptor, distribution, targets, bundles, tool):
    if not logical_path(distribution) or '/' in distribution or not targets or any(target not in TARGETS for target in targets):
        return 'Invalid shader distribution or target list'
    configs, error = project_modules(engine, project, descriptor)
    if error:
        return error
    _, available, error = collect(configs)
    if error:
        return error
    requirements, error = project_requirements(project, configs, targets, bundles, available)
    if error:
        return error
    if not tool.is_file():
        return f'ShaderCookTool is not built: {tool}'
    build = project / 'build/shader-cook'
    build.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=build) as temporary:
        temporary = Path(temporary)
        sources, products = temporary / 'sources', temporary / 'products'
        error = stage(configs, sources)
        if error:
            return error
        usage = temporary / 'requirements.json'
        write_changed(usage, (json.dumps(requirements, indent=2) + '\n').encode('utf-8'))
        result = subprocess.run([str(tool), str(usage), str(sources), str(products)],
                                cwd=tool.parent, capture_output=True, text=True)
        write_changed(build / (distribution + '.log'), (result.stdout + result.stderr).encode('utf-8'))
        if result.returncode:
            return f'Project shader cook failed ({result.returncode}); see {build / (distribution + ".log")}'
        write_changed(products / 'requirements.json', usage.read_bytes())
        write_changed(products / 'distribution.json', (json.dumps({'distribution': distribution, 'targets': targets,
                      'bundles': bundles}, indent=2) + '\n').encode('utf-8'))
        destination = (project / 'products/shaders' / distribution).resolve()
        if not destination.is_relative_to((project / 'products/shaders').resolve()):
            return 'Shader product destination escapes project products'
        destination.parent.mkdir(parents=True, exist_ok=True)
        if destination.exists():
            shutil.rmtree(destination)
        shutil.copytree(products, destination)
    return ''


def stage_products(project, distribution, destination):
    source = project / 'products/shaders' / distribution
    if not logical_path(distribution) or '/' in distribution or not (source / 'distribution.json').is_file():
        return f'Shader distribution has not been cooked: {distribution}'
    shutil.copytree(source, destination / 'shaders', dirs_exist_ok=True)
    return ''
