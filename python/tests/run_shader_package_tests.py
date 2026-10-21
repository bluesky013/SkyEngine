"""Exercise project shader cook and a relocated, compiler-free CoreTest consumer."""

import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

REPOSITORY = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPOSITORY / 'python'))
from package_project import stage_bundles


def main():
    work = REPOSITORY / 'cmake-build-debug/shader-package-test'
    work.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=work) as temporary:
        project = Path(temporary)
        for owner in ('first', 'second'):
            shutil.copytree(REPOSITORY / 'engine/aurora/core/test/plugins' / owner, project / 'plugins' / owner)
        (project / 'ShaderPackage.skyproj').write_text(json.dumps({'name': 'ShaderPackage', 'plugins': ['first', 'second']}), encoding='utf-8')
        command = [sys.executable, str(REPOSITORY / 'python/package_project.py'), 'cook-shaders',
                   '--project', str(project), '--distribution', 'shipping',
                   '--target', 'vulkan', '--target', 'metal', '--target', 'd3d12']
        result = subprocess.run(command, cwd=REPOSITORY)
        if result.returncode:
            return result.returncode
        destination = work / 'relocated'
        if not destination.resolve().is_relative_to(work.resolve()):
            return 1
        if not stage_bundles(project, {'name': 'shipping', 'platform': 'windows', 'bundles': ['common']}, destination):
            return 1
        if any(destination.rglob('*.slang')) or any(destination.rglob('*.slangh')):
            print('Shipping package unexpectedly includes sources', file=sys.stderr)
            return 1
        for target in ('vulkan', 'metal', 'd3d12'):
            if len(list(destination.rglob(target + '/index.bin'))) != 1:
                print('Expected exactly one distribution index for ' + target, file=sys.stderr)
                return 1
    # The project sources and intermediate staging are gone before the Core-only consumer starts.
    consumer = REPOSITORY / 'output/bin/Debug/Aurora.CoreTest.exe'
    result = subprocess.run([str(consumer), '--gtest_filter=ShaderDistribution.*', '--gtest_color=no'], cwd=consumer.parent)
    if result.returncode:
        return result.returncode
    native = consumer.parent / 'Aurora.Renderer.GpuSubmissionTest.exe'
    return subprocess.run([str(native), '--gtest_filter=*RelocatedProductsRenderSceneWithoutSourcesOrCompiler*',
                           '--gtest_color=no'], cwd=native.parent).returncode


if __name__ == '__main__':
    sys.exit(main())
