import argparse
import json
import os
import shutil
import subprocess

from pathlib import Path
from git import Repo

parser = argparse.ArgumentParser(description='示例脚本')
parser.add_argument('-i', '--intermediate', type=str, help='中间文件')
parser.add_argument('-o', '--output', type=str, help='输出路径')
parser.add_argument('-e', '--engine', type=str, help='引擎目录')
parser.add_argument('-t', '--target', type=str, help='编译单个包')
parser.add_argument('-p', '--platform', type=str, choices=["Win32", "MacOS-x86", "MacOS-arm", "Android", "IOS", "Linux"], help='编译平台')
parser.add_argument('-c', '--clean', action='store_true', default=False, help='清理工程')
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

def run_cmake(build_dir: str, source_dir: str, build_type, options: dict = None, cache: str = None, components = None):
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

    try:
        # 执行CMake配置
        process = subprocess.run(cmake_cmd, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        print(process)

        # 执行 Build
        build_cmd = ["cmake", "--build", build_dir, "--config", build_type]
        process = subprocess.run(build_cmd, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        print(process)

        # 执行 Install
        build_cmd = ["cmake", "--install", build_dir, "--config", build_type]

        if components:
            for component in components:
                build_cmd.extend(["--component", component])

        process = subprocess.run(build_cmd, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        print(process)

        print("CMake执行成功!")
    except subprocess.CalledProcessError as e:
        print(f"CMake执行失败: {e.stderr}")
        raise

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

    if src_inc.exists():
        dst_inc.mkdir(parents=True, exist_ok=True)
        if dst_inc.exists():
            shutil.rmtree(dst_inc)
        shutil.copytree(src_inc, dst_inc)

    if src_src.exists():
        dst_src.mkdir(parents=True, exist_ok=True)
        if dst_src.exists():
            shutil.rmtree(dst_src)
        shutil.copytree(src_src, dst_src)

    if src_bin.exists():
        dst_bin.mkdir(parents=True, exist_ok=True)
        if dst_bin.exists():
            shutil.rmtree(dst_bin)
        shutil.copytree(src_bin, dst_bin)

    if src_lib.exists():
        dst_lib.mkdir(parents=True, exist_ok=True)
        if dst_lib.exists():
            shutil.rmtree(dst_lib)
        shutil.copytree(src_lib, dst_lib)


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

def build_package_type(name, source_dir, build_type, options, cache, components):
    build_dir = os.path.join(source_dir, f"build_{args.platform}_{build_type}")
    install_dir = os.path.join(build_dir, 'install')

    # common options
    cmake_modules_path = os.path.join(args.engine, 'cmake', 'thirdparty').replace('\\', '/')
    print(f'cmake module find path: {cmake_modules_path}')
    options['3RD_PATH'] = args.output
    options['3RD_FIND_PATH'] = cmake_modules_path
    options['BUILD_TESTING'] = False
    options['CMAKE_INSTALL_PREFIX'] = install_dir
    options['CMAKE_BUILD_TYPE'] = build_type

    if args.platform == 'Android':
        fill_android_config(options)
    elif args.platform == 'IOS':
        fill_ios_config(options)

    run_cmake(build_dir, source_dir, build_type, options, cache, components)
    copy_package(name, install_dir, build_type)

def build_package(name, source_dir, options, cache, components):
    print("build package ...", name)
    build_package_type(name, source_dir, 'Debug', options, cache, components)
    build_package_type(name, source_dir, 'Release', options, cache, components)
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
    if len(name) == 0:
        return

    print("build package ...", name)

    if platforms and args.platform not in platforms:
        return

    clone_dir = os.path.join(args.intermediate, name)
    if not os.path.exists(clone_dir):
        repo = Repo.clone_from(url, str(clone_dir))
    else:
        repo = Repo(str(clone_dir))

    if cache:
        cache = os.path.join(str(clone_dir), cache)
        cache = str(os.path.normpath(cache))

    if args.clean:
        print(f"清理 '{name}'")
        repo.git.clean('-xdf')
        return

    if tag:
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
            subprocess.run(["python", custom, '-p', args.platform], cwd=str(clone_dir))
        else:
            subprocess.run(["python", custom], cwd=str(clone_dir))

    # init submodule
    if submodule is True:
        print(repo.submodules)

        for submodule in repo.submodules:
            submodule.update(init=True, recursive=True, force=True)

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

    if is_tool is not True:
        build_package(name, source_dir, options, cache, components)

def app_main():
    json_file = os.path.join(args.engine, 'cmake', 'thirdparty.json')
    with open(json_file, 'r', encoding='utf-8') as file:
        data = json.load(file)

    packages = data.get('packages', [])

    filtered = list(filter(lambda pkg: pkg['name'] == args.target, packages))

    if len(filtered) > 0:
        process_package(filtered[0])
    else:
        for package in packages:
            process_package(package)

if __name__ == "__main__":
    app_main()
