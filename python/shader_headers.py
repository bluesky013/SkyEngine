"""Export the shared declaration grammar into persistent shader assets."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import sys


IDENTIFIER = r'[A-Za-z_][A-Za-z_0-9]*'
TYPES = {'FLOAT': 'float', 'FLOAT2': 'float2', 'FLOAT4': 'float4',
         'MAT4': 'column_major float4x4', 'INT': 'int', 'UINT': 'uint'}
TEXTURES = {'TEXTURE2D': ('Texture2D', 't'), 'TEXTURE2D_ARRAY': ('Texture2DArray', 't'),
            'TEXTURECUBE': ('TextureCube', 't'), 'TEXTURECUBE_ARRAY': ('TextureCubeArray', 't'),
            'RW_TEXTURE2D': ('RWTexture2D', 'u')}
SAMPLERS = {'REGULAR': 'SamplerState', 'COMPARISON': 'SamplerComparisonState'}


def logical_path(value):
    return (isinstance(value, str) and re.fullmatch(r'[A-Za-z_0-9./-]+', value) is not None
            and all(part not in ('', '.', '..') for part in value.split('/')))


def uint(value, minimum=0):
    return re.fullmatch(r'[0-9]+', value) is not None and minimum <= int(value) <= 0xffffffff


def calls(body):
    result = []
    position = 0
    while position < len(body):
        match = re.match(r'\s*(' + IDENTIFIER + r')\s*\(([^()]*)\)\s*', body[position:])
        if not match:
            return None
        result.append((match[1], [arg.strip() for arg in match[2].split(',')]))
        position += match.end()
    return result


def parse_declaration(source):
    source = re.sub(r'/\*.*?\*/|//[^\n]*', '', source, flags=re.S)
    source = re.sub(r'\\\n', '', source)
    macros = {}
    active = set()
    declarations = []
    declared_names = set()
    for number, line in enumerate(source.splitlines(), 1):
        line = line.strip()
        if not line or line == '#pragma once' or re.fullmatch(r'#include\s+<aurora/shader/SharedShader.h>', line):
            continue
        define = re.fullmatch(r'#define\s+(' + IDENTIFIER + r')\(([^()]*)\)\s+(.*)', line)
        undef = re.fullmatch(r'#undef\s+(' + IDENTIFIER + ')', line)
        declared = re.fullmatch(r'AURORA_SHADER_PASS\(([^()]*)\)', line)
        if define and define[1] not in macros:
            macros[define[1]] = ([arg.strip() for arg in define[2].split(',')], define[3])
            active.add(define[1])
        elif undef and declarations and undef[1] in active:
            active.remove(undef[1])
        elif declared:
            declaration = [arg.strip() for arg in declared[1].split(',')]
            if len(declaration) != 3:
                return None, 'Expected AURORA_SHADER_PASS(namespace, name, resourceList)'
            namespace, name, resource_list = declaration
            if not re.fullmatch(IDENTIFIER + '(::' + IDENTIFIER + ')*', namespace) or not re.fullmatch(IDENTIFIER, name):
                return None, 'Invalid declaration namespace or name'
            if (namespace, name) in declared_names or resource_list not in active or len(macros[resource_list][0]) != 1:
                return None, 'Duplicate pass or missing resource list'
            declared_names.add((namespace, name))
            declarations.append(resource_list)
        else:
            return None, f'Unsupported declaration near logical line {number}: {line}'
    if not declarations or active:
        return None, 'Expected AURORA_SHADER_PASS and balanced declaration macros'
    return (macros, declarations), ''


def uniform_fields(macros, name):
    if name not in macros or len(macros[name][0]) != 3:
        return None, f'Missing field list: {name}'
    (field_op, array_op, owner), body = macros[name]
    fields = calls(body)
    if not fields:
        return None, f'Invalid or empty field list: {name}'
    lines, names = [], set()
    for op, args in fields:
        array = op == array_op
        if op not in (field_op, array_op) or len(args) != (4 if array else 3) or args[0] != owner:
            return None, f'Unsupported field in {name}'
        kind, field = args[1:3]
        if kind not in TYPES or not re.fullmatch(IDENTIFIER, field) or field in names:
            return None, f'Invalid, unsupported or duplicate field: {name}.{field}'
        if array and (kind != 'FLOAT4' or not uint(args[3], 1) or int(args[3]) > 4096):
            return None, f'Only bounded FLOAT4 arrays are supported: {name}.{field}'
        names.add(field)
        suffix = '[' + args[3] + ']' if array else ''
        lines.append(f'    [AuroraField("{field}")] {TYPES[kind]} {field}{suffix};')
    return lines, ''


def render_declaration(source, logical, helper_bytes=b''):
    parsed, error = parse_declaration(source)
    if error:
        return None, error
    macros, resource_lists = parsed
    resources = []
    for resource_list in resource_lists:
        entries = calls(macros[resource_list][1])
        if not entries or any(op != macros[resource_list][0][0] or not args for op, args in entries):
            return None, 'Invalid or empty resource list'
        resources.extend(entries)
    lines, slots, names, structures = [], set(), set(), set()
    used = set(resource_lists)
    for op, args in resources:
        kind = args[0]
        if kind == 'UNIFORM' and len(args) == 6:
            structure, variable, space, binding, fields = args[1:]
            if not re.fullmatch(IDENTIFIER, structure) or structure in structures:
                return None, f'Invalid or duplicate structure: {structure}'
            members, error = uniform_fields(macros, fields)
            if error:
                return None, error
            structures.add(structure)
            used.add(fields)
            lines.extend([f'struct {structure}', '{', *members, '};'])
            resource_type, register = f'ConstantBuffer<{structure}>', 'b'
        elif kind == 'TEXTURE' and len(args) == 6:
            shape, value, variable, space, binding = args[1:]
            if shape not in TEXTURES or value not in TYPES or value == 'MAT4':
                return None, f'Unsupported texture: {shape}<{value}>'
            texture, register = TEXTURES[shape]
            resource_type = f'{texture}<{TYPES[value]}>'
        elif kind == 'STRUCTURED' and len(args) == 5:
            value, variable, space, binding = args[1:]
            if value not in TYPES:
                return None, f'Unsupported structured buffer element: {value}'
            resource_type, register = f'StructuredBuffer<{TYPES[value]}>', 't'
        elif kind == 'SAMPLER' and len(args) == 5:
            sampler, variable, space, binding = args[1:]
            if sampler not in SAMPLERS:
                return None, f'Unsupported sampler: {sampler}'
            resource_type, register = SAMPLERS[sampler], 's'
        else:
            return None, f'Unsupported resource kind or arguments: {kind}'
        if not uint(space) or int(space) > 3 or not uint(binding):
            return None, 'Invalid Global/Pass/Batch binding slot'
        if not re.fullmatch(IDENTIFIER, variable) or variable in names or (int(space), int(binding)) in slots:
            return None, f'Invalid or duplicate resource binding: {variable}'
        names.add(variable)
        slots.add((int(space), int(binding)))
        lines.extend([f'[AuroraResource({space}, {binding}, "{logical}")]',
                      f'[[vk::binding({binding}, {space})]] {resource_type} {variable}',
                      '#if !AURORA_TARGET_MSL',
                      f'    : register({register}{binding}, space{space})',
                      '#endif', ';', ''])
    if used != set(macros):
        return None, 'Unused or unsupported declaration macros'
    fingerprint = hashlib.sha256((logical + '\n' + '\n'.join(lines)).encode('utf-8')).hexdigest()
    exporter = hashlib.sha256(Path(__file__).read_bytes().replace(b'\r\n', b'\n')).hexdigest()
    header = [f'// Generated from {logical}; do not edit.',
              f'// Inputs: {fingerprint}; exporter: {exporter}', '#pragma once', '',
              '#ifndef AURORA_DECLARATION_ATTRIBUTES', '#define AURORA_DECLARATION_ATTRIBUTES',
              '[__AttributeUsage(_AttributeTargets.Var)]',
              'struct AuroraResourceAttribute { int logicalSet; int logicalBinding; string owner; };',
              '[__AttributeUsage(_AttributeTargets.Var)]',
              'struct AuroraFieldAttribute { string path; };', '#endif', '']
    return '\n'.join(header + lines), ''


def prepare_exports(config):
    if not config.is_file():
        return None, f'Module description is missing: {config}'
    module_root = config.resolve().parent
    description = json.loads(config.read_text(encoding='utf-8'))
    resources = description.get('resources', {})
    if not isinstance(resources, dict):
        return None, f'Invalid resources description: {config}'
    owner = resources.get('owner', '')
    roots = resources.get('roots', [])
    if not logical_path(owner) or '/' in owner or not isinstance(roots, list):
        return None, f'Invalid resource owner or roots: {config}'
    prepared = []
    for root in roots:
        if not isinstance(root, dict):
            return None, f'Invalid resource root: {config}'
        if root.get('kind') != 'shader':
            continue
        root_path = root.get('path', '')
        if not logical_path(root_path):
            return None, f'Invalid shader root: {root_path}'
        destination = (module_root / root_path).resolve()
        if not destination.is_relative_to(module_root):
            return None, f'Shader root escapes its module: {root_path}'
        helpers = b''
        for helper in root.get('declarationHelpers', []):
            if not logical_path(helper):
                return None, f'Invalid declaration helper: {helper}'
            path = (module_root / helper).resolve()
            if not path.is_relative_to(module_root) or not path.is_file():
                return None, f'Missing declaration helper: {helper}'
            helpers += helper.encode('utf-8') + b'\0' + path.read_bytes().replace(b'\r\n', b'\n') + b'\0'
        for entry in root.get('exports', []):
            if not isinstance(entry, dict):
                return None, f'Invalid export: {config}'
            source, logical = entry.get('source', ''), entry.get('include', '')
            if not logical_path(source) or not logical_path(logical) or not logical.startswith(owner + '/') or not logical.endswith('.h'):
                return None, f'Invalid export mapping: {source} -> {logical}'
            source_path = (module_root / source).resolve()
            output = (destination / (logical[:-2] + '.slangh')).resolve()
            if not source_path.is_relative_to(module_root) or not output.is_relative_to(destination) or not source_path.is_file():
                return None, f'Missing or out-of-root declaration: {source}'
            text, error = render_declaration(source_path.read_text(encoding='utf-8'), logical, helpers)
            if error:
                return None, f'{source}: {error}'
            prepared.append((output, logical, text))
    return prepared, ''


def export_headers(configs, check):
    prepared, paths, logicals = [], set(), set()
    for config in configs:
        exports, error = prepare_exports(config)
        if error:
            return error
        for output, logical, content in exports:
            key = str(output).casefold()
            if key in paths or logical.casefold() in logicals:
                return f'Duplicate or case-colliding export: {logical}'
            paths.add(key)
            logicals.add(logical.casefold())
            old = output.read_text(encoding='utf-8') if output.is_file() else None
            marker = f'// Generated from {logical}; do not edit.\n'
            if old is not None and not old.startswith(marker):
                return f'Export collides with an unowned shader asset: {output}'
            if old != content:
                if check:
                    return f'Missing or stale export: {output}; run ShaderHeaders.Update'
                prepared.append((output, content))
    for output, content in prepared:
        output.parent.mkdir(parents=True, exist_ok=True)
        temporary = output.with_suffix(output.suffix + '.tmp')
        temporary.write_text(content, encoding='utf-8', newline='\n')
        temporary.replace(output)
    return ''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config', type=Path, action='append', required=True)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument('--check', action='store_true')
    mode.add_argument('--update', action='store_true')
    args = parser.parse_args()
    error = export_headers(args.config, args.check)
    if error:
        print(error, file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
