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

PLATFORMS = {'windows': 1, 'macos': 2, 'ios': 3, 'android': 4, 'linux': 5}
PLATFORM_TARGETS = {'windows': {'vulkan', 'd3d12'}, 'macos': {'metal', 'vulkan'},
                    'ios': {'metal'}, 'android': {'vulkan'}, 'linux': {'vulkan'}}
USAGE_FORMAT = 3
PRODUCT_FORMAT = 2


def target_environments(platform, targets, overrides=None):
    if platform not in PLATFORMS or not targets or any(target not in PLATFORM_TARGETS[platform] for target in targets):
        return None, 'Explicit shader platform and compatible targets are required'
    if overrides is None:
        overrides = {}
    if not isinstance(overrides, dict) or any(target not in targets for target in overrides):
        return None, 'Shader environment overrides must name selected targets'
    spirv_profile, spirv_version = ('spirv_1_3', 10300) if platform == 'android' else ('spirv_1_5', 10500)
    profiles = {'vulkan': spirv_profile, 'metal': 'metallib_2_3', 'd3d12': 'sm_6_5'}
    versions = {'vulkan': spirv_version, 'metal': 20300, 'd3d12': 60500}
    environments = {}
    for target in targets:
        environment = {'target': TARGETS[target], 'platform': PLATFORMS[platform],
                       'codeFormat': {'vulkan': 1, 'metal': 2, 'd3d12': 3}[target],
                       'profile': profiles[target],
                       'minimumVersion': versions[target], 'requiredCapabilities': 0, 'matrixLayout': 0, 'options': []}
        custom = overrides.get(target, {})
        allowed = {'codeFormat', 'profile', 'minimumVersion', 'requiredCapabilities', 'matrixLayout', 'options'}
        if not isinstance(custom, dict) or set(custom) - allowed:
            return None, 'Invalid shader target environment override: ' + target
        environment.update(custom)
        for name, maximum in [('codeFormat', 4), ('minimumVersion', 0xffffffff),
                              ('requiredCapabilities', 0xffffffffffffffff), ('matrixLayout', 1)]:
            value = environment[name]
            if not isinstance(value, int) or isinstance(value, bool) or not 0 <= value <= maximum:
                return None, 'Invalid shader environment field: ' + name
        if environment['minimumVersion'] == 0:
            environment['minimumVersion'] = versions[target]
        if environment['codeFormat'] not in {'vulkan': {1}, 'metal': {2, 4}, 'd3d12': {3}}[target]:
            return None, 'Shader code format disagrees with target: ' + target
        if not isinstance(environment['profile'], str) or not environment['profile'] or '\x00' in environment['profile']:
            return None, 'Shader profile must be explicit'
        options = environment['options']
        if not isinstance(options, list) or any(not isinstance(option, str) or not option or '\x00' in option for option in options):
            return None, 'Shader target options must be nonempty strings'
        environment['options'] = sorted(set(options))
        environments[target] = environment
    return environments, ''


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


def append_program(program, targets, available, entries, environments):
    if not isinstance(program, dict):
        return 'Shader program requirement must be an object'
    path = program.get('path', '')
    stages = program.get('entries', [])
    variants = program.get('variants', [{}])
    if not isinstance(variants, list) or not variants or any(not isinstance(variant, dict) for variant in variants):
        return f'Invalid shader variant requirements: {path}'
    normalized = []
    for variant in variants:
        values = {}
        for name, value in variant.items():
            if not isinstance(name, str) or not name.isidentifier():
                return f'Invalid shader variant key: {path}'
            if isinstance(value, bool):
                value = {'type': 2, 'value': int(value)}
            elif isinstance(value, int) and -0x80000000 <= value <= 0xffffffff:
                value = {'type': 1 if value < 0 else 0, 'value': value & 0xffffffff}
            if (not isinstance(value, dict) or set(value) != {'type', 'value'} or
                    type(value['type']) is not int or value['type'] not in (0, 1, 2) or
                    type(value['value']) is not int or not 0 <= value['value'] <= 0xffffffff or
                    (value['type'] == 2 and value['value'] > 1)):
                return f'Invalid shader variant value: {path}'
            values[name] = value
        normalized.append(values)
    variants = normalized
    if path not in available or not path.endswith('.slang') or not isinstance(stages, list) or not stages:
        return f'Missing shader program or entries: {path}'
    for entry in stages:
        if not isinstance(entry, dict) or entry.get('stage') not in STAGES or not isinstance(entry.get('name'), str) or not entry['name']:
            return f'Invalid shader entry: {path}'
        for target in targets:
            for variant in variants:
                request = {'path': path, 'entry': entry['name'], 'stage': STAGES[entry['stage']],
                           'target': TARGETS[target], 'variant': variant, 'environment': environments[target]}
                key = (path, entry['name'], request['stage'], request['target'], json.dumps(environments[target], sort_keys=True), json.dumps(variant, sort_keys=True))
                entries[key] = request
    return ''


def project_requirements(project, configs, targets, bundles, available, environments):
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
                error = append_program(program, targets, available, entries, environments)
                if error:
                    return None, error
    requirements = project / 'configs/shader_requirements.json'
    if requirements.is_file():
        data = json.loads(requirements.read_text(encoding='utf-8'))
        for program in data.get('programs', []):
            if program.get('bundle', 'common') in bundles:
                error = append_program(program, targets, available, entries, environments)
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
            variant_keys = set()
            complete_schema = True
            for stage in ('vertex', 'fragment'):
                shader = program[stage]
                if not isinstance(shader, dict):
                    return None, f'Invalid material shader reference: {path}'
                raw = available.get(shader.get('relativePath', '') + '.json')
                complete_schema = complete_schema and bool(raw)
                if raw:
                    metadata = json.loads(raw)
                    vertex_keys = {entry['name'] for entry in metadata.get('vertex', [])}
                    variant_keys.update(entry['key'] for entry in metadata.get('variants', [])
                                        if entry.get('domain', 'shader') == 'shader' and entry['key'] not in vertex_keys)
            if complete_schema and set(program.get('variant', {})) - variant_keys:
                return None, f'{path}: Material program contains an unknown or non-Shader option'
            for stage in ('vertex', 'fragment'):
                shader = program[stage]
                if not isinstance(shader, dict):
                    return None, f'Invalid material shader reference: {path}'
                variant = dict(program.get('variant', {}))
                companion_bytes = available.get(shader.get('relativePath', '') + '.json')
                if companion_bytes:
                    companion = json.loads(companion_bytes)
                    vertex_keys = {entry['name'] for entry in companion.get('vertex', [])}
                    declarations = {entry['key']: entry for entry in companion.get('variants', [])
                                    if entry.get('domain', 'shader') == 'shader' and entry['key'] not in vertex_keys}
                    variant = {name: value for name, value in variant.items() if name in declarations}
                    for prop in material.get('properties', []):
                        name = prop.get('name')
                        if name not in declarations:
                            continue
                        if name in variant:
                            return None, f'{path}: duplicate Material option {name}'
                        value = prop.get('value', {})
                        scalar_type = {'bool': 2, 'int32': 1, 'uint32': 0}.get(declarations[name].get('type', 'uint32'))
                        expected_index = {0: 6, 1: 5, 2: 7}.get(scalar_type)
                        if not isinstance(value, dict) or value.get('index') != expected_index or 'value' not in value:
                            return None, f'{path}: Material option has an incompatible value type: {name}'
                        raw = value['value']
                        if ((scalar_type == 2 and type(raw) is not bool) or
                                (scalar_type != 2 and (type(raw) is not int or
                                 not (-0x80000000 if scalar_type == 1 else 0) <= raw <= (0x7fffffff if scalar_type == 1 else 0xffffffff)))):
                            return None, f'{path}: Material option is out of range: {name}'
                        variant[name] = {'type': scalar_type, 'value': int(raw) & 0xffffffff}
                request = {'path': shader.get('relativePath', ''), 'entries': [
                    {'stage': stage, 'name': shader.get('entry', '')}], 'variants': [variant]}
                error = append_program(request, targets, available, entries, environments)
                if error:
                    return None, f'{path}: {error}'
    return {'formatVersion': USAGE_FORMAT, 'entries': [entries[key] for key in sorted(entries)]}, ''


def cook_project(engine, project, descriptor, distribution, targets, bundles, tool, platform, environment_overrides=None):
    if not logical_path(distribution) or '/' in distribution or not targets or any(target not in TARGETS for target in targets):
        return 'Invalid shader distribution or target list'
    environments, error = target_environments(platform, targets, environment_overrides)
    if error:
        return error
    configs, error = project_modules(engine, project, descriptor)
    if error:
        return error
    _, available, error = collect(configs)
    if error:
        return error
    requirements, error = project_requirements(project, configs, targets, bundles, available, environments)
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
                      'bundles': bundles, 'formatVersion': PRODUCT_FORMAT, 'platform': platform, 'environments': environments}, indent=2) + '\n').encode('utf-8'))
        destination = (project / 'products/shaders' / distribution).resolve()
        if not destination.is_relative_to((project / 'products/shaders').resolve()):
            return 'Shader product destination escapes project products'
        destination.parent.mkdir(parents=True, exist_ok=True)
        if destination.exists():
            shutil.rmtree(destination)
        shutil.copytree(products, destination)
    return ''


def stage_products(project, distribution, destination, platform):
    source = project / 'products/shaders' / distribution
    if not logical_path(distribution) or '/' in distribution or not (source / 'distribution.json').is_file():
        return f'Shader distribution has not been cooked: {distribution}'
    manifest = json.loads((source / 'distribution.json').read_text(encoding='utf-8'))
    targets = manifest.get('targets', [])
    if manifest.get('formatVersion') != PRODUCT_FORMAT or manifest.get('platform') != platform:
        return 'Shader products have an incompatible platform or protocol; recook: ' + distribution
    environments, error = target_environments(platform, targets)
    if error:
        return error
    declared = manifest.get('environments')
    if not isinstance(declared, dict) or set(declared) != set(environments):
        return 'Shader products have no complete target environments; recook: ' + distribution
    for target, environment in declared.items():
        if not isinstance(environment, dict) or set(environment) != set(environments[target]):
            return 'Incomplete shader product environment; recook: ' + distribution
        overrides = {key: value for key, value in environment.items() if key not in ('target', 'platform')}
        normalized, error = target_environments(platform, [target], {target: overrides})
        if error or normalized[target] != environment:
            return 'Shader product target environment is invalid; recook: ' + distribution
        if not (source / target / 'index.bin').is_file():
            return 'Shader product index is missing; recook: ' + target
    shutil.copytree(source, destination / 'shaders', dirs_exist_ok=True)
    return ''
