import os

from PySide6.QtCore import Qt, QPoint
from PySide6.QtWidgets import QWidget, QComboBox, QVBoxLayout, QLabel, QHBoxLayout, QPushButton, QTextEdit, QSplitter, \
    QListWidget, QListWidgetItem, QMessageBox, QStyledItemDelegate, QTableWidget, QHeaderView, QMenu, QFileDialog, \
    QTableWidgetItem

from device import DeviceBase
from ios_device import IOSDeviceFactory
from android_device import AndroidDeviceFactory

class ReadOnlyDelegate(QStyledItemDelegate):
    def createEditor(self, parent, option, index):
        # 返回None表示不允许编辑
        return None

class AppFileBrowser(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)

        self.current_dir = None
        layout = QVBoxLayout(self)
        self.setLayout(layout)

        # path
        self.path = QTextEdit(self)
        self.path.setFixedHeight(32)
        layout.addWidget(self.path)

        # op
        op_layout = QHBoxLayout()
        self.back_button = QPushButton('返回')
        self.refresh_button = QPushButton('刷新')
        self.delete_button = QPushButton('删除')
        self.upload_dir_button = QPushButton('上传文件夹')
        self.upload_file_button = QPushButton('上传文件')
        op_layout.addWidget(self.back_button)
        op_layout.addWidget(self.refresh_button)
        op_layout.addWidget(self.delete_button)
        op_layout.addWidget(self.upload_dir_button)
        op_layout.addWidget(self.upload_file_button)
        layout.addLayout(op_layout)

        self.back_button.clicked.connect(self.op_back_dir)
        self.refresh_button.clicked.connect(self.op_refresh_dir)
        self.delete_button.clicked.connect(self.op_delete)
        self.upload_dir_button.clicked.connect(self.op_upload_dir)
        self.upload_file_button.clicked.connect(self.op_upload_file)

        # table
        self.file_table = QTableWidget(self)
        self.file_table.setColumnCount(4)
        self.file_table.setHorizontalHeaderLabels(["名称", "类型", "大小", "修改日期"])
        header = self.file_table.horizontalHeader()
        header.setSectionResizeMode(0, QHeaderView.ResizeMode.ResizeToContents)
        header.setSectionResizeMode(1, QHeaderView.ResizeMode.ResizeToContents)
        header.setSectionResizeMode(2, QHeaderView.ResizeMode.ResizeToContents)
        header.setSectionResizeMode(3, QHeaderView.ResizeMode.ResizeToContents)
        self.file_table.itemDoubleClicked.connect(self.on_double_click)
        self.file_table.setItemDelegate(ReadOnlyDelegate())
        self.file_table.setContextMenuPolicy(Qt.ContextMenuPolicy.CustomContextMenu)
        self.file_table.customContextMenuRequested.connect(self.show_context_menu)

        layout.addWidget(self.file_table)
        layout.addStretch()

        self.visitor = None

    def show_context_menu(self, point: QPoint):
        item = self.file_table.itemAt(point)
        if not item:
            return

        name = self.file_table.item(item.row(), 0).text()
        type = self.file_table.item(item.row(), 1).text()
        file_path = os.path.join(self.current_dir, name).replace('\\', '/')

        menu = QMenu(self)
        save_action = menu.addAction("保存")

        if type == '文件夹':
            save_action.triggered.connect(lambda: self.op_save_dir(name, file_path))
        else:
            save_action.triggered.connect(lambda: self.op_save(name, file_path))

        menu.exec(self.file_table.viewport().mapToGlobal(point))

    def op_save(self, file: str, src: str):
        file_names, _ = QFileDialog.getSaveFileName(self, "选择保存路径", file)
        if file_names:
            self.visitor.pull(src, file_names)

    def op_save_dir(self, file: str, src: str):
        dialog = QFileDialog(self, caption="选择保存路径", directory=file)
        dialog.setFileMode(QFileDialog.FileMode.Directory)
        dialog.setOption(QFileDialog.Option.DontUseNativeDialog, True)
        dialog.setOption(QFileDialog.Option.ShowDirsOnly, True)
        dialog.exec()
        dst_list = dialog.selectedFiles()
        if len(dst_list) > 0:
            self.visitor.pull(src, dst_list[0])

    def upload_file(self, src: str):
        self.visitor.push(src, self.current_dir)
        self.op_refresh_dir()

    def op_delete(self):
        items = self.file_table.selectedItems()
        for item in items:
            name = self.file_table.item(item.row(), 0).text()
            file_path = os.path.join(self.current_dir, name).replace('\\', '/')
            self.visitor.rm(file_path)
        self.op_refresh_dir()

    def op_upload_dir(self):
        folder_path = QFileDialog.getExistingDirectory(self, "选择要上传的文件夹")
        if len(folder_path) != 0:
            self.upload_file(folder_path)

    def op_upload_file(self):
        file_names, _ = QFileDialog.getOpenFileNames(self, "选择要上传的文件")
        for file in file_names:
            self.upload_file(file)

    def op_back_dir(self):
        if self.current_dir != self.visitor.base_dir:
            self.enter_dir(os.path.dirname(self.current_dir))

    def op_refresh_dir(self):
        self.enter_dir(self.current_dir)

    def set_context(self, device: DeviceBase, app: str):
        self.visitor = device.create_visitor(app)
        self.enter_dir(self.visitor.base_dir)

    def on_double_click(self, index):
        file_name = self.file_table.item(index.row(), 0)
        file_type = self.file_table.item(index.row(), 1)

        if file_type.text() == '文件夹':
            dir_path = file_name.text()
            path = os.path.join(self.current_dir, dir_path)
            path = path.replace('\\', '/')
            self.enter_dir(path)

    def enter_dir(self, path: str):
        flist = self.visitor.listdir(path)
        self.current_dir = path
        self.file_table.setRowCount(len(flist))
        self.path.setText(path)
        for row, info in enumerate(flist):
            file_name = info.get('name')
            file_type = info.get('type')
            file_size = info.get('size')
            file_time = info.get('time')

            name_item = QTableWidgetItem(file_name)
            type_item = QTableWidgetItem(file_type)
            size_item = QTableWidgetItem(file_size)
            time_item = QTableWidgetItem(file_time)

            self.file_table.setItem(row, 0, name_item)
            self.file_table.setItem(row, 1, type_item)
            self.file_table.setItem(row, 2, size_item)
            self.file_table.setItem(row, 3, time_item)

class DeviceBrowser(QWidget):
    def __init__(self, parent = None):
        super().__init__(parent)
        self.devices = []
        self.factories = [IOSDeviceFactory(), AndroidDeviceFactory()]
        self.active_device = None

        layout = QVBoxLayout(self)
        layout.setAlignment(Qt.AlignmentFlag.AlignTop)
        self.setLayout(layout)

        # device list
        self.device_combo = QComboBox()
        layout.addWidget(QLabel('设备列表'))
        layout.addWidget(self.device_combo)

        # connection button
        btn_layout = QHBoxLayout()
        refresh_btn = QPushButton('刷新设备', self)
        connect_btn = QPushButton('连接设备', self)
        btn_layout.addWidget(refresh_btn)
        btn_layout.addWidget(connect_btn)
        btn_layout.addStretch()
        layout.addLayout(btn_layout)

        refresh_btn.clicked.connect(self.refresh_device)
        connect_btn.clicked.connect(self.connect_device)

        # device info
        self.device_info_text = QTextEdit(self)
        self.device_info_text.setReadOnly(True)
        self.device_info_text.setFixedHeight(120)
        layout.addWidget(QLabel('设备信息'))
        layout.addWidget(self.device_info_text)

        # app info
        app_layout = QHBoxLayout()
        splitter = QSplitter(self)
        app_layout.addWidget(splitter)
        self.app_list = QListWidget(self)
        self.app_file_table = AppFileBrowser(self)
        splitter.addWidget(self.app_list)
        splitter.addWidget(self.app_file_table)
        layout.addLayout(app_layout)
        layout.addStretch()

        self.app_list.itemDoubleClicked.connect(self.app_double_clicked)

    def app_double_clicked(self, item: QListWidgetItem):
        self.app_file_table.set_context(self.active_device, str(item.data(Qt.ItemDataRole.UserRole)))

    def connect_device(self):
        index = self.device_combo.currentIndex()
        if index >= 0:
            device = self.device_combo.itemData(index)
            self.device_info_text.setText(device.device_info())
            self.refresh_app_list(device)
            self.active_device = device
            QMessageBox.information(
                self,
                "Info",
                "Device Connected."
            )

    def refresh_app_list(self, device: DeviceBase):
        app_list = device.refresh_app_list()
        self.app_list.clear()
        for key, data in app_list.items():
            item_name = key
            if data != '':
                item_name = data

            item = QListWidgetItem(item_name)
            item.setData(Qt.ItemDataRole.UserRole, key)
            self.app_list.addItem(item)

    def refresh_device(self):
        self.device_combo.clear()

        dev_list : list[DeviceBase] = []
        for factory in self.factories:
            dev_list += factory.list_devices()

        for dev in dev_list:
            self.devices.append(dev)
            self.device_combo.addItem(f'[{dev.serial}]{dev.name}', dev)