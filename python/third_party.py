import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import zipfile

from pathlib import Path
from git import Repo

parser = argparse.ArgumentParser(description='SkyEngine 三方库编译工具')
parser.add_argument('-i', '--intermediate', type=str, help='中间文件')
parser.add_argument('-o', '--output', type=str, help='输出路径')
parser.add_argument('-e', '--engine', type=str, help='引擎目录')
parser.add_argument('-t', '--target', type=str, help='编译单个包')
parser.add_argument('-p', '--platform', type=str, choices=["Win32", "MacOS-x86", "MacOS-arm", "Android", "IOS", "Linux"], help='编译平台')
parser.add_argument('-c', '--clean', action='store_true', default=False, help='清理工程')
parser.add_argument('-j', '--jobs', type=int, default=0, help='并行编译线程数 (0=自动)')
parser.add_argument('-l', '--list', action='store_true', default=False, help='列出所有包信息')
parser.add_argument('-f', '--force', action='store_true', default=False, help='强制重新构建（忽略增量缓存）')
args = parser.parse_args()

tool_chain = {
    'Win32': 'Visual Studio 17 2022',
    'MacOS-x86': 'Xcode',
    'MacOS-arm': 'Xcode',
    'IOS': 'Xcode',
    'Android': 'Ninja',
    'Linux': 'Ninja'
}

NDK_VERSION = '27.0.12077973'
METADATA_FILE = 'build_metadata.json'

def load_build_metadata():
    meta_path = os.path.join(args.output, METADATA_FILE)
    if os.path.exists(meta_path):
        with open(meta_path, 'r', encoding='utf-8') as f:
            return json.load(f)
    return {}

def save_build_metadata(metadata):
    meta_path = os.path.join(args.output, METADATA_FILE)
    Path(args.output).mkdir(parents=True, exist_ok=True)
    with open(meta_path, 'w', encoding='utf-8') as f:
        json.dump(metadata, f, indent=2, ensure_ascii=False)

def compute_package_key(package):
    """Compute a hash from package config to detect changes."""
    relevant = {k: v for k, v in package.items() if k not in ('name',)}
    content = json.dumps(relevant, sort_keys=True)
    return hashlib.sha256(content.encode()).hexdigest()[:16]

def is_package_up_to_date(metadata, name, package_key):
    entry = metadata.get(name)
    if not entry:
        return False
    return entry.get('key') == package_key and entry.get('platform') == args.platform

def mark_package_built(metadata, name, package_key):
    metadata[name] = {
        'key': package_key,
        'platform': args.platform
    }

def get_log_dir():
    log_dir = os.path.join(args.intermediate, '_logs')
    Path(log_dir).mkdir(parents=True, exist_ok=True)
    return log_dir

def run_cmake(build_dir: str, source_dir: str, build_type, options: dict = None, cache: str = None, components = None, log_name: str = None):
    # 确保构建目录存在
    Path(build_dir).mkdir(parents=True, exist_ok=True)

    # 准备CMake命令
    if cache:
        cmake_cmd = ["cmake", "-S", source_dir, "-C", cache, "-B", build_dir, "-G", tool_chain[args.platform]]
    else:
        cmake_cmd = ["cmake", "-S", source_dir, "-B", build_dir, "-G", tool_chain[args.platform]]

    # 添加选项
    if options:
        for key, value in options.items():
            cmake_cmd.extend([f"-D{key}={value}"])

    log_file = None
    if log_name:
        log_path = os.path.join(get_log_dir(), f"{log_name}_{build_type}.log")
        log_file = open(log_path, 'w', encoding='utf-8')

    try:
        # 执行CMake配置
        print(f"  [configure] {source_dir}")
        process = subprocess.run(cmake_cmd, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        if log_file:
            log_file.write(f"=== configure ===\n{process.stdout.decode('utf-8', errors='replace')}\n")

        # 执行 Build
        build_cmd = ["cmake", "--build", build_dir, "--config", build_type]
        if args.jobs > 0:
            build_cmd.extend(["--parallel", str(args.jobs)])
        else:
            build_cmd.append("--parallel")
        print(f"  [build] {build_type}")
        process = subprocess.run(build_cmd, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        if log_file:
            log_file.write(f"=== build ===\n{process.stdout.decode('utf-8', errors='replace')}\n")

        # 执行 Install
        install_cmd = ["cmake", "--install", build_dir, "--config", build_type]

        if components:
            for component in components:
                install_cmd.extend(["--component", component])

        print(f"  [install] {build_type}")
        process = subprocess.run(install_cmd, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        if log_file:
            log_file.write(f"=== install ===\n{process.stdout.decode('utf-8', errors='replace')}\n")

        print(f"  [done] {build_type} 成功")
    except subprocess.CalledProcessError as e:
        stderr_text = e.stderr.decode('utf-8', errors='replace') if isinstance(e.stderr, bytes) else str(e.stderr)
        if log_file:
            log_file.write(f"=== ERROR ===\n{stderr_text}\n")
        print(f"CMake执行失败:\n{stderr_text}")
        raise
    finally:
        if log_file:
            log_file.close()

def copy_package(name, install_dir, build_type):
    src_inc_path = os.path.join(install_dir, "include")
    src_src_path = os.path.join(install_dir, "src")
    src_lib_path = os.path.join(install_dir, "lib")
    src_bin_path = os.path.join(install_dir, "bin")

    dst_inc_path = os.path.join(args.output, name, "include")
    dst_src_path = os.path.join(args.output, name, "src")
    dst_lib_path = os.path.join(args.output, name, "lib", build_type)
    dst_bin_path = os.path.join(args.output, name, "bin")

    src_inc = Path(str(src_inc_path))
    src_src = Path(str(src_src_path))
    src_lib = Path(str(src_lib_path))
    src_bin = Path(str(src_bin_path))

    dst_inc = Path(str(dst_inc_path))
    dst_src = Path(str(dst_src_path))
    dst_lib = Path(str(dst_lib_path))
    dst_bin = Path(str(dst_bin_path))

    for src, dst in [(src_inc, dst_inc), (src_src, dst_src), (src_bin, dst_bin), (src_lib, dst_lib)]:
        if src.exists():
            if dst.exists():
                shutil.rmtree(dst)
            shutil.copytree(src, dst)


def get_android_sdk_path():
    possible_names = ['ANDROID_HOME', 'ANDROID_SDK_ROOT', 'ANDROID_SDK']

    for name in possible_names:
        path = os.environ.get(name)
        if path and os.path.exists(path):
            print(f"find Android SDK: {path} from {name}")
            return path
    raise Exception("Android SDK path not found.")

def fill_ios_config(options):
    # options['CMAKE_SYSTEM_NAME'] = 'iOS'
    # options['CMAKE_OSX_DEPLOYMENT_TARGET'] = '13.0'
    # options['CMAKE_OSX_ARCHITECTURES'] = 'arm64'
    # options['CMAKE_IOS_DEVELOPER_ROOT'] = '/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer'
    # options['CMAKE_IOS_SDK_ROOT'] = '/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS.sdk'
    options['CMAKE_TOOLCHAIN_FILE'] = os.path.join(args.intermediate, 'ios-cmake', 'ios.toolchain.cmake')
    options['PLATFORM'] = 'OS64'

def fill_android_config(options):
    ndk = os.path.join(get_android_sdk_path(), "ndk", NDK_VERSION)
    toolchain = os.path.join(ndk, "build", "cmake", "android.toolchain.cmake")

    options['ANDROID_ABI'] = 'arm64-v8a'
    options['ANDROID_STL'] = 'c++_static'
    options['ANDROID_PLATFORM'] = 'android-31'
    options['CMAKE_TOOLCHAIN_FILE'] = toolchain

def fill_common_options(options, build_type, install_dir):
    cmake_modules_path = os.path.join(args.engine, 'cmake', 'thirdparty').replace('\\', '/')
    options['3RD_PATH'] = args.output
    options['3RD_FIND_PATH'] = cmake_modules_path
    options['BUILD_TESTING'] = 'OFF'
    options['CMAKE_INSTALL_PREFIX'] = install_dir
    options['CMAKE_BUILD_TYPE'] = build_type

    if args.platform == 'Android':
        fill_android_config(options)
    elif args.platform == 'IOS':
        fill_ios_config(options)

def build_package_type(name, source_dir, build_type, options, cache, components, header_only=False):
    build_dir = os.path.join(source_dir, f"build_{args.platform}_{build_type}")
    install_dir = os.path.join(build_dir, 'install')

    fill_common_options(options, build_type, install_dir)

    if header_only:
        # header-only: configure + install only, skip build
        Path(build_dir).mkdir(parents=True, exist_ok=True)

        cmake_cmd = ["cmake", "-S", source_dir, "-B", build_dir, "-G", tool_chain[args.platform]]
        if cache:
            cmake_cmd = ["cmake", "-S", source_dir, "-C", cache, "-B", build_dir, "-G", tool_chain[args.platform]]
        if options:
            for key, value in options.items():
                cmake_cmd.extend([f"-D{key}={value}"])

        print(f"  [configure] {source_dir}")
        subprocess.run(cmake_cmd, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

        install_cmd = ["cmake", "--install", build_dir, "--config", build_type]
        print(f"  [install] {build_type}")
        subprocess.run(install_cmd, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    else:
        run_cmake(build_dir, source_dir, build_type, options, cache, components, log_name=name)

    copy_package(name, install_dir, build_type)

def build_package(name, source_dir, options, cache, components, header_only=False):
    print("build package ...", name)
    if header_only:
        build_package_type(name, source_dir, 'Release', dict(options), cache, components, header_only=True)
    else:
        build_package_type(name, source_dir, 'Debug', dict(options), cache, components)
        build_package_type(name, source_dir, 'Release', dict(options), cache, components)
    print("build package done")

def process_package(package):
    name = package.get('name')
    url = package.get('url')
    tag = package.get('tag')
    custom = package.get('custom')
    custom_need_platform = package.get('custom_need_platform')
    cache = package.get('cache')
    platforms = package.get('platforms')
    submodule = package.get('submodule')
    submodules = package.get('submodules')
    components = package.get('components')
    source = package.get('source')
    is_tool = package.get('is_tool')
    options = package.get('options', {})
    header_only = package.get('header_only', False)
    if len(name) == 0:
        return

    print("build package ...", name)

    if platforms and args.platform not in platforms:
        return

    clone_dir = os.path.join(args.intermediate, name)
    need_full_history = submodule or submodules
    if not os.path.exists(clone_dir):
        clone_kwargs = {}
        if not need_full_history and tag:
            clone_kwargs['depth'] = 1
            clone_kwargs['branch'] = tag
        repo = Repo.clone_from(url, str(clone_dir), **clone_kwargs)
    else:
        repo = Repo(str(clone_dir))

    if cache:
        cache = os.path.join(str(clone_dir), cache)
        cache = str(os.path.normpath(cache))

    if args.clean:
        print(f"清理 '{name}'")
        repo.git.clean('-xdf')
        return

    if tag and need_full_history:
        repo.git.fetch('--tags')
        if tag not in repo.tags:
            raise ValueError(f"Tag '{tag}' 不存在于仓库中")

        branch_name = f"Branch_{tag}"

        if branch_name not in repo.branches:
            repo.git.checkout(tag, b=branch_name)
        else:
            repo.git.checkout(branch_name)

    # use custom step
    if custom:
        if custom_need_platform is True:
            subprocess.run([sys.executable, custom, '-p', args.platform], cwd=str(clone_dir), check=True)
        else:
            subprocess.run([sys.executable, custom], cwd=str(clone_dir), check=True)

    # init submodule
    if submodule is True:
        print(repo.submodules)

        for sm in repo.submodules:
            sm.update(init=True, recursive=True, force=True)

    if submodules:
        for subName in submodules:
            update_cmd = ["git", "submodule", "update", "--init", "--recursive", subName]
            subprocess.run(update_cmd, cwd=clone_dir, capture_output=True, text=True)

    # try to apply patch
    patch_path = os.path.join(args.engine, 'cmake', 'patches', name + '.patch')
    if os.path.exists(patch_path):
        repo.git.reset('--hard')
        repo.git.clean('-f')
        repo.git.apply(patch_path)
        print('Apply Patch 成功')

    source_dir = clone_dir
    if source:
        source_dir = os.path.join(str(clone_dir), source)

    if is_tool is True:
        return

    build_package(name, source_dir, options, cache, components, header_only=header_only)

def archive_output(json_file, data):
    """Zip the entire output directory and record MD5 into thirdparty.json."""
    archive_dir = os.path.join(args.engine, 'build_3rd', 'archives')
    Path(archive_dir).mkdir(parents=True, exist_ok=True)

    # create zip
    zip_name = f"thirdparty_{args.platform}.zip"
    zip_path = os.path.join(archive_dir, zip_name)
    print(f"[archive] creating {zip_path} ...")

    output_root = Path(args.output)
    with zipfile.ZipFile(zip_path, 'w', zipfile.ZIP_DEFLATED) as zf:
        for file in sorted(output_root.rglob('*')):
            if file.is_file() and file.name != METADATA_FILE:
                arcname = file.relative_to(output_root)
                zf.write(file, arcname)

    # compute md5
    md5 = hashlib.md5()
    with open(zip_path, 'rb') as f:
        for chunk in iter(lambda: f.read(8192), b''):
            md5.update(chunk)
    md5_hex = md5.hexdigest()

    # rename with short hash
    final_name = f"thirdparty_{args.platform}_{md5_hex[:12]}.zip"
    final_path = os.path.join(archive_dir, final_name)
    if os.path.exists(final_path):
        os.remove(final_path)
    os.rename(zip_path, final_path)

    # update thirdparty.json
    if 'archives' not in data:
        data['archives'] = {}
    data['archives'][args.platform] = {
        'file': final_name,
        'md5': md5_hex
    }
    with open(json_file, 'w', encoding='utf-8') as f:
        json.dump(data, f, indent='\t', ensure_ascii=False)

    size_mb = os.path.getsize(final_path) / (1024 * 1024)
    print(f"[archive] {final_name} ({size_mb:.1f} MB, md5: {md5_hex})")


def list_packages(packages):
    print(f"{'Name':<20} {'Tag':<25} {'Type':<12} {'Platforms'}")
    print("-" * 80)
    for pkg in packages:
        name = pkg.get('name', '')
        tag = pkg.get('tag', '(no tag)')
        ptype = 'header-only' if pkg.get('header_only') else 'tool' if pkg.get('is_tool') else 'static'
        platforms = ', '.join(pkg.get('platforms', ['all']))
        print(f"{name:<20} {tag:<25} {ptype:<12} {platforms}")

def app_main():
    json_file = os.path.join(args.engine, 'cmake', 'thirdparty.json')
    with open(json_file, 'r', encoding='utf-8') as file:
        data = json.load(file)

    packages = data.get('packages', [])

    if args.list:
        list_packages(packages)
        return

    metadata = load_build_metadata()

    if args.target:
        filtered = list(filter(lambda pkg: pkg['name'] == args.target, packages))
    else:
        filtered = packages

    built_any = False
    for package in filtered:
        name = package.get('name', '')
        if not name:
            continue
        package_key = compute_package_key(package)

        if not args.force and not args.clean and is_package_up_to_date(metadata, name, package_key):
            print(f"[skip] {name} (已是最新, 使用 -f 强制重建)")
            continue

        process_package(package)

        if not args.clean:
            mark_package_built(metadata, name, package_key)
            save_build_metadata(metadata)
            built_any = True

    # archive after build
    if built_any and not args.clean:
        archive_output(json_file, data)

if __name__ == "__main__":
    app_main()
