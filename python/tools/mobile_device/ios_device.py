from pymobiledevice3.lockdown import create_using_usbmux, LockdownClient
from pymobiledevice3.usbmux import list_devices, MuxDevice
from pymobiledevice3.services.installation_proxy import InstallationProxyService
from pymobiledevice3.services.house_arrest import HouseArrestService

import os

from tools.mobile_device.device import DeviceBase, DeviceFactory, BrowserVisitor, convert_file_size

platform_codes = {
    't8020': 'A12 Bionic',
    't8027': 'A12X Bionic',
    't8030': 'A13 Bionic',
    't8101': 'A14 Bionic',
    't8110': 'A15 Bionic',
    't8120': 'A16 Bionic',
    't8130': 'A17 Pro',
    't8140a': 'A18',
    't8140': 'A18 Pro',

    't8103': 'M1',
    't6000': 'M1 Pro',
    't6001': 'M1 Max',
    't6002': 'M1 Ultra',

    't8112': 'M2',
    't6020': 'M2 Pro',
    't6021': 'M2 Max',
    't6022': 'M2 Ultra',
}

def convert_type(ft: str):
    if ft == 'S_IFDIR':
        return '文件夹'
    elif ft == 'S_IFLNK':
        return '文件链接'
    elif ft == 'S_IFREG':
        return '文件'
    return '其他'

class IOSBrowserVisitor(BrowserVisitor):
    def __init__(self, client: LockdownClient, app: str):
        super().__init__()
        self.base_dir = '/Documents'
        self.afc = HouseArrestService(client, app, True)

    def listdir(self, path: str):
        flist = self.afc.listdir(path)

        files = []
        for file in flist:
            info = self.afc.stat(os.path.join(path, file).replace('\\', '/'))

            file_type = convert_type(info.get('st_ifmt'))
            file_size = info.get('st_size')
            file_time = info.get('st_birthtime').strftime("%Y-%m-%d %H:%M:%S")

            file_info = {
                'name': file,
                'type': file_type,
                'size': convert_file_size(file_size),
                'time': file_time
            }
            files.append(file_info)

        return files

    def push(self, src: str, dst: str):
        self.afc.push(src, dst)

    def pull(self, src: str, dst: str):
        self.afc.pull(src, dst)

    def rm(self, path: str):
        self.afc.rm(path)

class IOSDevice(DeviceBase):
    def __init__(self, device: MuxDevice):
        super().__init__()

        self.device = device
        self.client = create_using_usbmux(device.serial)
        self.serial = device.serial
        self.name = self.client.all_values.get('DeviceName')
        self.arch = self.client.all_values.get('CPUArchitecture')
        self.type = self.client.all_values.get('DeviceClass')
        self.version = self.client.all_values.get('HumanReadableProductVersionString')

        platform = self.client.all_values.get('HardwarePlatform')
        self.chip = platform_codes.get(platform, 'Unknown')

        self.afc = None
        self.base_dir = '/Documents'
        self.current_dir = ''


    def connect(self):
        pass

    def refresh_app_list(self):
        result = {}

        with InstallationProxyService(lockdown=self.client) as installation_proxy:
            app_list = installation_proxy.get_apps('User')

            for key, data in app_list.items():
                result[key] = data.get('CFBundleName')

        return result

    def create_visitor(self, app:str):
        return IOSBrowserVisitor(self.client, app)

class IOSDeviceFactory(DeviceFactory):
    def __init__(self):
        super().__init__()

    def list_devices(self):
        results = []

        devices = list_devices()
        for dev in devices:
            results.append(IOSDevice(dev))

        return results