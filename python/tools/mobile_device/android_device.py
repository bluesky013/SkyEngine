from datetime import datetime

from ppadb.client import Client as AdbClient
from ppadb.device import Device as AdbDevice

from tools.mobile_device.device import DeviceBase, DeviceFactory, BrowserVisitor, convert_file_size

import subprocess
import re
import os

def get_file_details(device, path):
    """获取文件详细信息"""
    try:
        cmd = f"ls -la {path}"

        output = device.shell(cmd)
        files = []

        # 解析 ls -la 输出
        for line in output.splitlines():
            if line.startswith('total'):
                continue

            match = re.match(
                r'^([-dlbcps])[rwsStTx-]{9}\s+\d+\s+\S+\s+\S+\s+(\d+)\s+(\d{4}-\d{2}-\d{2}\s+\d{2}:\d{2})\s+(.+)$',
                line.strip()
            )

            if match:
                file_type = '文件夹' if match.group(1) == 'd' else '文件'
                size = int(match.group(2))
                mod_time = match.group(3)
                file_name = match.group(4)
                # 尝试解析时间 (格式可能是 "Oct 1 10:00" 或 "2023-10-01 10:00")
                try:
                    if ':' in mod_time:
                        mod_date = datetime.strptime(mod_time, "%b %d %H:%M")
                        mod_date = mod_date.replace(year=datetime.now().year)
                    else:
                        mod_date = datetime.strptime(mod_time, "%Y-%m-%d %H:%M")
                except:
                    mod_date = mod_time  # 保持原始字符串如果解析失败

                file_info = {
                    'name': file_name,
                    'type': file_type,
                    'size': convert_file_size(size),
                    'time': mod_date if isinstance(mod_date, datetime) else mod_time
                }
                files.append(file_info)
        return files
    except Exception as e:
        print(f"获取文件详情失败: {e}")
        return []

def adb_push_file(local_file, device_path):
    try:
        subprocess.run(["adb", "push", local_file, device_path], check=True)
        print(f"文件 {local_file} 上传成功到 {device_path}")
    except subprocess.CalledProcessError as e:
        print(f"上传失败: {e}")


def adb_push_folder(local_folder, device_path):
    """上传整个文件夹到设备"""
    if not os.path.isdir(local_folder):
        print(f"错误: {local_folder} 不是有效文件夹")
        return

    try:
        subprocess.run(["adb", "push", local_folder, device_path], check=True)
        print(f"文件夹 {local_folder} 上传成功到 {device_path}")
    except subprocess.CalledProcessError as e:
        print(f"上传失败: {e}")

def adb_pull(device_path, local_path):
    """使用adb pull下载文件或文件夹"""
    try:
        result = subprocess.run(
            ["adb", "pull", device_path, local_path],
            capture_output=True,
            text=True,
            check=True
        )
        print(result.stdout)
        return True
    except subprocess.CalledProcessError as e:
        print(f"下载失败: {e.stderr}")
        return False

class AndroidBrowserVisitor(BrowserVisitor):
    def __init__(self, dev: AdbDevice, app: str):
        super().__init__()
        self.device = dev
        self.base_dir = f"/storage/emulated/0/Android/data/{app}/files"

    def listdir(self, path: str):
        return get_file_details(self.device, path)

    def push(self, src: str, dst: str):
        if os.path.isdir(src):
            basename = os.path.basename(src)
            for root, dirs, files in os.walk(src):
                root_dir_path = os.path.join(basename, root.replace(src, ""))
                self.device.shell("mkdir -p {}/{}".format(dst, root_dir_path))

            adb_push_folder(src, dst)
        else:
            adb_push_file(src, dst)

    def pull(self, src: str, dst: str):
        print(src, dst)
        adb_pull(src, dst)

    def rm(self, path: str):
        self.device.shell("rm -r " + path)


class AndroidDevice(DeviceBase):
    def __init__(self, dev: AdbDevice):
        super().__init__()
        self.device = dev
        self.serial = dev.serial
        self.name = dev.shell("getprop ro.product.name").strip()
        self.arch = dev.shell("getprop ro.product.cpu.abi").strip()
        self.chip = dev.shell("getprop ro.board.chiptype").strip()
        self.type = self.name
        self.version = 'Android-' + dev.shell("getprop ro.build.version.sdk").strip()

    def connect(self):
        self.device.create_connection()

    def _extract_from_dump(self, dump, start, end):
        """从dumpsys输出中提取信息"""
        try:
            return dump.split(start)[1].split(end)[0].strip()
        except:
            return "Uknown"

    def _app_list(self, filter_type = 'user'):
        if filter_type == "user":
            cmd = "pm list packages -3"
        elif filter_type == "system":
            cmd = "pm list packages -s"
        else:
            cmd = "pm list packages"

        return self.device.shell(cmd)

    def refresh_app_list(self):
        tmp_list = self._app_list()
        app_list = [line.split(":")[1] for line in tmp_list.splitlines() if line]

        result = {}

        for app in app_list:
            result[app] = ''

        return result

    def create_visitor(self, app:str):
        return AndroidBrowserVisitor(self.device, app)

def start_adb_server():
    try:
        # 启动ADB服务器
        subprocess.run(['adb', 'start-server'], check=True)
        print("ADB服务器已启动")
    except subprocess.CalledProcessError as e:
        print(f"启动ADB服务器失败: {e}")
    except FileNotFoundError:
        print("ADB未安装或不在PATH中")

class AndroidDeviceFactory(DeviceFactory):
    def __init__(self):
        super().__init__()

    def list_devices(self):
        results = []

        start_adb_server()

        client = AdbClient(host="127.0.0.1", port=5037)
        client.create_connection()
        devices = client.devices()

        for dev in devices:
            results.append(AndroidDevice(dev))

        return results