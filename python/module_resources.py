"""Collect module shader inventories for development, SDK staging and project cook."""

import argparse
import json
from pathlib import Path
import sys

from shader_headers import export_headers, logical_path, prepare_exports


MANIFEST = 'shader-sources.json'


def strings(value):
    return isinstance(value, list) and all(isinstance(item, str) for item in value)


def collect(configs):
    error = export_headers(configs, check=True)
    if error:
        return None, None, error
    modules, sources, owners = [], {}, set()
    for config in configs:
        data = json.loads(config.read_text(encoding='utf-8'))['resources']
        owner = data['owner']
        if owner.casefold() in owners:
            return None, None, f'Duplicate resource owner: {owner}'
        owners.add(owner.casefold())
        shader_roots = [root for root in data['roots'] if root.get('kind') == 'shader']
        if len(shader_roots) != 1:
            return None, None, f'Expected one shader root for {owner}'
        description = shader_roots[0]
        exports, error = prepare_exports(config)
        if error:
            return None, None, error
        generated = {path.resolve() for path, _, _ in exports}
        root = (config.resolve().parent / description['path']).resolve()
        if not root.is_dir():
            return None, None, f'Missing shader root: {root}'
        dependencies = description.get('dependencies', [])
        includes = description.get('includeRoots', [])
        if not strings(dependencies) or not strings(includes):
            return None, None, f'Invalid dependencies/includeRoots: {owner}'
        if len(set(dependencies)) != len(dependencies) or len(set(includes)) != len(includes):
            return None, None, f'Repeated dependencies/includeRoots: {owner}'
        files = []
        for path in sorted(root.rglob('*')):
            if not path.is_file():
                continue
            logical = path.relative_to(root).as_posix()
            if not path.resolve().is_relative_to(root) or not logical_path(logical) or not logical.startswith(owner + '/'):
                return None, None, f'Invalid or out-of-owner asset: {path}'
            if path.suffix not in ('.slang', '.slangh', '.json'):
                return None, None, f'Unsupported shader asset (C++ headers belong outside the shader root): {path}'
            key = logical.casefold()
            if key in sources:
                return None, None, f'Duplicate/case-colliding shader asset: {logical}'
            content = path.read_bytes().replace(b'\r\n', b'\n')
            if content.startswith(b'// Generated from ') and path.resolve() not in generated:
                return None, None, f'Orphaned generated export; remove or update it with its declaration mapping: {path}'
            sources[key] = (logical, content)
            files.append(logical)
        if not files:
            return None, None, f'Empty shader inventory: {owner}'
        for include in includes:
            if not logical_path(include) or not include.startswith(owner + '/') or not any(file.startswith(include + '/') for file in files):
                return None, None, f'Include root must name a nonempty owner directory: {include}'
        modules.append({'owner': owner, 'dependencies': sorted(dependencies), 'includeRoots': sorted(includes), 'files': files})
    by_owner = {module['owner']: module for module in modules}
    resolved = set()
    while len(resolved) != len(modules):
        ready = {owner for owner, module in by_owner.items() if set(module['dependencies']) <= resolved}
        if ready <= resolved:
            return None, None, 'Missing or cyclic module shader dependency'
        resolved |= ready
    # Check each dependency scope; independent plugins may use identical local helper names.
    for module in modules:
        scope, pending = set(), [module['owner']]
        while pending:
            owner = pending.pop()
            if owner not in scope:
                scope.add(owner)
                pending.extend(by_owner[owner]['dependencies'])
        aliases = {}
        roots = [''] + [root for owner in scope for root in by_owner[owner]['includeRoots']]
        for owner in scope:
            for path in by_owner[owner]['files']:
                for root in roots:
                    prefix = root + '/' if root else ''
                    if not path.startswith(prefix):
                        continue
                    alias = path[len(prefix):].casefold()
                    if alias in aliases and aliases[alias] != path:
                        return None, None, f'Ambiguous shader include in {module["owner"]}: {alias}'
                    aliases[alias] = path
    manifest = {'version': 1, 'modules': sorted(modules, key=lambda module: module['owner'])}
    return manifest, dict(sources.values()), ''


def write_changed(path, data):
    if path.is_file() and path.read_bytes() == data:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + '.tmp')
    temporary.write_bytes(data)
    temporary.replace(path)


def stage(configs, destination):
    manifest, sources, error = collect(configs)
    if error:
        return error
    if (destination.parent.resolve() != destination.parent.parent.resolve() / destination.parent.name or
            destination.resolve() != destination.parent.resolve() / destination.name):
        return f'Stage shader root and its asset container must be real directories: {destination}'
    destination = destination.resolve()
    # Only retire paths from our previous inventory, never unrelated destination files.
    manifest_path = destination / MANIFEST
    old_files = []
    if manifest_path.is_file():
        previous = json.loads(manifest_path.read_text(encoding='utf-8'))
        for module in previous.get('modules', []):
            old_files.extend(module.get('files', []))
    for name in old_files:
        target = (destination / name).resolve()
        if not logical_path(name) or not target.is_relative_to(destination):
            return f'Unsafe previous staged inventory: {name}'
    for name in sources:
        target = (destination / name).resolve()
        if not target.is_relative_to(destination):
            return f'Stage path escapes destination: {name}'
    for name, data in sources.items():
        write_changed(destination / name, data)
    for name in old_files:
        if name not in sources:
            target = destination / name
            if target.is_file():
                target.unlink()
    write_changed(manifest_path, (json.dumps(manifest, indent=2) + '\n').encode('utf-8'))
    return ''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config', type=Path, action='append', required=True)
    parser.add_argument('--stage', type=Path)
    args = parser.parse_args()
    error = stage(args.config, args.stage) if args.stage else collect(args.config)[2]
    if error:
        print(error, file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
