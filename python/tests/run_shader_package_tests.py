"""Cook a relocated distribution and run existing consumers without source/compiler files."""

import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

REPOSITORY = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPOSITORY / 'python'))
from package_project import stage_bundles


def add_native_programs(project):
    root = project / 'plugins/first'
    sources = root / 'assets/shaders/first/pipeline'
    attributes = ('[__AttributeUsage(_AttributeTargets.Var)]\n'
                  'struct AuroraResourceAttribute { int logicalSet; int logicalBinding; string owner; };\n')
    programs = [
        ('PackageCompute.slang', [{'stage': 'compute', 'name': 'FillTarget'}],
         attributes + '[AuroraResource(1, 0, "first/pipeline/PackageCompute.slang")]\n'
         '[[vk::binding(0, 1)]] RWTexture2D<float4> image\n#if !AURORA_TARGET_MSL\n: register(u0, space1)\n#endif\n;\n'
         '[numthreads(1, 1, 1)] void FillTarget(uint3 id : SV_DispatchThreadID) { image[id.xy] = float4(0, 0, 1, 1); }\n'),
        ('PackageFullscreen.slang', [{'stage': 'vertex', 'name': 'DrawQuad'}, {'stage': 'fragment', 'name': 'SampleTarget'}],
         attributes + '[AuroraResource(1, 0, "first/pipeline/PackageFullscreen.slang")]\n'
         '[[vk::binding(0, 1)]] Texture2D<float4> image\n#if !AURORA_TARGET_MSL\n: register(t0, space1)\n#endif\n;\n'
         '[AuroraResource(1, 1, "first/pipeline/PackageFullscreen.slang")]\n'
         '[[vk::binding(1, 1)]] SamplerState linearSampler\n#if !AURORA_TARGET_MSL\n: register(s1, space1)\n#endif\n;\n'
         'struct Varying { float4 position : SV_Position; float2 uv : TEXCOORD0; };\n'
         'Varying DrawQuad(uint id : SV_VertexID) { Varying result; '
         'result.uv = float2((id << 1) & 2, id & 2); result.position = float4(result.uv * 2 - 1, 0, 1); return result; }\n'
         'float4 SampleTarget(Varying input) : SV_Target { return image.Sample(linearSampler, input.uv); }\n'),
    ]
    programs.append(('PackageMaterial.slang', [{'stage': 'vertex', 'name': 'VSMain'},
                                               {'stage': 'fragment', 'name': 'FSMain'}], r"""
        #include <aurora/pipeline/SceneBatch.slangh>
        struct MaterialValues {
            [AuroraField("tint")] float4 tint;
            [AuroraField("gain")] float4 gain;
        };
        [AuroraResource(2, 2, "fixture/material")] ConstantBuffer<MaterialValues> shaderValues;
        [AuroraResource(2, 3, "fixture/material")] Texture2D<float4> materialColor;
        [AuroraResource(2, 4, "fixture/material")] SamplerState materialSampler;
        struct Vertex { float4 position : SV_Position; float2 uv : TEXCOORD0; };
        Vertex VSMain(float3 position : POSITION) {
            Vertex v;
            v.position = mul(gSceneBatch.world, float4(position.xy * 1.8, 0.5, 1));
            v.uv = position.xy + 0.5;
            return v;
        }
        float4 FSMain(Vertex v) : SV_Target {
            return materialColor.Sample(materialSampler, v.uv) * shaderValues.tint * shaderValues.gain;
        }
    """))
    variant_declaration = ('#ifndef PACKAGE_VALUE\n#define PACKAGE_VALUE 0\n#endif\n'
                           '#if AURORA_TARGET_DXIL\nstatic const uint kPackageValue = PACKAGE_VALUE;\n'
                           '#else\n[SpecializationConstant] [[vk::constant_id(5)]] const uint kPackageValue = PACKAGE_VALUE;\n#endif\n')
    programs = [(name, entries, variant_declaration + source.replace('float4(0, 0, 1, 1)', 'float4(0, 0, float(kPackageValue), 1)')
                 .replace('return image.Sample(linearSampler, input.uv);', 'return image.Sample(linearSampler, input.uv) * float(kPackageValue);'))
                if name != 'PackageMaterial.slang' else (name, entries, source) for name, entries, source in programs]
    config = root / 'plugin.json'
    description = json.loads(config.read_text(encoding='utf-8'))
    description['resources']['roots'][0]['dependencies'].append('aurora')
    inventory = description['resources']['roots'][0]['programs']
    for filename, entries, source in programs:
        (sources / filename).write_text(source, encoding='utf-8')
        variant = filename != 'PackageMaterial.slang'
        (sources / (filename + '.json')).write_text(json.dumps({'entries': [entry['name'] for entry in entries],
            'sources': [{'name': 'shader', 'width': 1}] if variant else [],
            'variants': [{'key': 'PACKAGE_VALUE', 'source': 'shader', 'width': 1, 'default': 0, 'symbol': 'kPackageValue'}] if variant else []}), encoding='utf-8')
        inventory.append({'path': 'first/pipeline/' + filename, 'entries': entries,
                          'variants': [{'PACKAGE_VALUE': 1}] if variant else [{}]})
    config.write_text(json.dumps(description, indent=2), encoding='utf-8')


def copy_consumer(binary, destination, backend=None):
    destination.mkdir(parents=True, exist_ok=True)
    shutil.copy2(binary, destination / binary.name)
    modules = ['Aurora.Metadata', 'Aurora.Render', 'Aurora.' + {'metal': 'Metal', 'vulkan': 'Vulkan', 'd3d12': 'DX12'}[backend]] if backend else []
    extension = '.dll' if os.name == 'nt' else '.dylib' if sys.platform == 'darwin' else '.so'
    for module in modules:
        library = binary.parent / (('' if os.name == 'nt' else 'lib') + module + extension)
        if not library.is_file():
            raise SystemExit('Required runtime module is not built: ' + str(library))
        shutil.copy2(library, destination / library.name)
    if backend == 'vulkan':
        for pattern in ('*vulkan*' + extension + '*', '*MoltenVK*' + extension + '*'):
            for library in binary.parent.glob(pattern):
                if library.is_file():
                    shutil.copy2(library, destination / library.name)
    for library in destination.iterdir():
        if 'slang' in library.name.lower() or 'shadercompiler' in library.name.lower():
            raise SystemExit('Compiler unexpectedly selected for shipping: ' + str(library))
        if sys.platform == 'darwin' and library.is_file():
            linked = subprocess.run(['otool', '-L', str(library)], check=True, capture_output=True, text=True).stdout
            if 'slang' in linked.lower() or 'shadercompiler' in linked.lower():
                raise SystemExit('Shipping consumer links compiler runtime: ' + str(library))
    return destination / binary.name


def run_consumer(binary, filters, root, backend=None):
    environment = os.environ.copy()
    environment['SKY_SHADER_PACKAGE_ROOT'] = str(root)
    if backend:
        environment['SKY_SHADER_PACKAGE_BACKEND'] = backend
    environment.pop('DYLD_LIBRARY_PATH', None)
    environment.pop('DYLD_FALLBACK_LIBRARY_PATH', None)
    environment.pop('LD_LIBRARY_PATH', None)
    if sys.platform == 'darwin':
        environment['DYLD_PRINT_LIBRARIES'] = '1'
    result = subprocess.run([str(binary), '--gtest_filter=' + filters, '--gtest_color=no'],
                            cwd=binary.parent, env=environment, capture_output=True, text=True)
    print(result.stdout, end='')
    print(result.stderr, end='', file=sys.stderr)
    if '[  SKIPPED ]' in result.stdout or 'Running 0 tests' in result.stdout or 'slang' in result.stderr.lower():
        return 1
    return result.returncode


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, default=REPOSITORY / 'cmake-build-debug')
    parser.add_argument('--config', default='Debug')
    parser.add_argument('--bin-dir', type=Path)
    parser.add_argument('--tool', type=Path)
    parser.add_argument('--target', choices=['vulkan', 'metal', 'd3d12'], action='append')
    parser.add_argument('--skip-native', action='store_true', help='Only validate the compiler-free Core consumer')
    args = parser.parse_args()
    platform = 'macos' if sys.platform == 'darwin' else 'windows' if os.name == 'nt' else 'linux'
    targets = args.target or (['metal'] if sys.platform == 'darwin' else ['vulkan', 'd3d12'] if os.name == 'nt' else ['vulkan'])
    bins = (args.bin_dir or REPOSITORY / 'output/bin' / args.config).resolve()
    suffix = '.exe' if os.name == 'nt' else ''
    tool = args.tool or bins / ('ShaderCookTool' + suffix)
    core = bins / ('Aurora.CoreTest' + suffix)
    native = bins / ('Aurora.Renderer.ModuleTest' + suffix)
    for binary in [tool, core] + ([] if args.skip_native else [native]):
        if not binary.is_file():
            parser.error('Build the existing consumer/tool first or pass --bin-dir/--tool: ' + str(binary))
    work = args.build_dir.resolve() / 'shader-package-test'
    work.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=work) as temporary:
        project = Path(temporary)
        for owner in ('first', 'second'):
            shutil.copytree(REPOSITORY / 'engine/aurora/core/test/plugins' / owner, project / 'plugins' / owner)
        add_native_programs(project)
        (project / 'ShaderPackage.skyproj').write_text(json.dumps({'name': 'ShaderPackage', 'plugins': ['first', 'second']}), encoding='utf-8')
        command = [sys.executable, str(REPOSITORY / 'python/package_project.py'), 'cook-shaders',
                   '--project', str(project), '--distribution', 'shipping', '--tool', str(tool.resolve()), '--platform', platform]
        for target in targets:
            command += ['--target', target]
        result = subprocess.run(command, cwd=REPOSITORY)
        if result.returncode:
            log = project / 'build/shader-cook/shipping.log'
            if log.is_file():
                print(log.read_text(encoding='utf-8'), file=sys.stderr)
            return result.returncode
        destination = work / 'relocated'
        if not stage_bundles(project, {'name': 'shipping', 'platform': platform, 'bundles': ['common']}, destination):
            return 1
        if any(destination.rglob('*.slang')) or any(destination.rglob('*.slangh')):
            parser.error('Shipping package unexpectedly includes sources')
        for target in targets:
            if len(list(destination.rglob(target + '/index.bin'))) != 1:
                parser.error('Expected exactly one distribution index for ' + target)
    shipping = work / 'stripped-host'
    if shipping.exists():
        shutil.rmtree(shipping)
    consumer = copy_consumer(core, shipping / 'core')
    products = destination / 'shaders'
    result = run_consumer(consumer, 'ShaderDistribution.*', products)
    if result or args.skip_native:
        return result
    for target in targets:
        host = copy_consumer(native, shipping / target, target)
        result = run_consumer(host, 'RendererModule.ShippedProducts*WithoutCompiler', products, target)
        if result:
            return result
    return 0


if __name__ == '__main__':
    sys.exit(main())
