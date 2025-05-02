def convert_file_size(file_size):
    if file_size < 1024:
        size_str = f"{file_size} B"
    elif file_size < 1024 * 1024:
        size_str = f"{file_size / 1024:.1f} KB"
    else:
        size_str = f"{file_size / (1024 * 1024):.1f} MB"

    return size_str

class BrowserVisitor:
    def __init__(self):
        self.base_dir = ''

    def listdir(self, path: str):
        return []

    def push(self, src: str, dst: str):
        pass

    def pull(self, src: str, dst: str):
        pass

    def rm(self, path: str):
        pass

class DeviceBase:
    def __init__(self):
        self.serial = 'Unknown'
        self.name = 'Unknown'
        self.arch = 'Unknown'
        self.type = 'Unknown'
        self.version = 'Unknown'
        self.chip = 'Unknown'

    def connect(self):
        pass

    def refresh_app_list(self):
        pass

    def device_info(self):
        info_text = f"""
设备名称: {self.name}
设备版本: {self.version}
设备类型: {self.type}
CPU信息: {self.arch}
芯片型号: {self.chip}
设备序列号: {self.serial}
"""
        return info_text

    def create_visitor(self, app) -> BrowserVisitor:
        pass

class DeviceFactory:
    def __init__(self):
        self.devices = []

    def list_devices(self):
        return self.devices