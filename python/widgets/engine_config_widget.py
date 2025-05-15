import json
import locale
import os
import subprocess
import sys

from PySide6.QtWidgets import QWidget, QMainWindow, QFormLayout, QHBoxLayout, QTabWidget, QTableWidget, \
    QTableWidgetItem, QVBoxLayout, QPushButton, QMessageBox, QComboBox

from widgets.common.edit_with_select_wnd import EditWithSelect

platform_filter_table = {
    'Win32': ['win32'],
    'MacOS-x86': ['darwin'],
    'MacOS-arm': ['darwin'],
    'MacOS-ios': ['darwin'],
    'Android': ['win32', 'darwin'],
}

class EngineThirdPartyTable(QWidget):
    def __init__(self, engine_path: str, parent=None):
        super(EngineThirdPartyTable, self).__init__(parent)

        self.json_file = None
        self.third_path = None
        self.engine_path = None

        layout = QVBoxLayout(self)
        self.setLayout(layout)

        form_layout = QFormLayout(self)
        self.third_path_edit = EditWithSelect(parent=self, is_dir=True)
        self.third_path_edit.on_selected.connect(self.third_path_selected)
        form_layout.addRow("3rd Path", self.third_path_edit)

        self.table = QTableWidget(self)

        b_layout = QHBoxLayout(self)
        self.build_packages_btn = QPushButton("Build packages")
        self.build_packages_btn.clicked.connect(self.build_packages)
        self.platform = QComboBox(self)
        self.platform.addItems(["Win32", "MacOS-x86", "MacOS-arm", "Android"])
        b_layout.addWidget(self.platform)
        b_layout.addWidget(self.build_packages_btn)

        layout.addLayout(form_layout)
        layout.addWidget(self.table)
        layout.addLayout(b_layout)

        self.update_data(engine_path)

    def third_path_selected(self, path: str):
        self.third_path = path
        print(f'3rd path {self.third_path} selected.')

    def build_packages(self):
        if self.third_path is None:
            QMessageBox.warning(self, "Error", "Please select third-party path")
            return

        platform = self.platform.currentText()
        current_platform = sys.platform
        available_platforms = platform_filter_table[platform]

        if current_platform not in available_platforms:
            QMessageBox.warning(self, "Error", "Current platform not supported")
            return

        intermediate = os.path.join(self.third_path, "packages")
        output = os.path.join(self.third_path, f'{platform}_out')

        py = os.path.join(self.engine_path, "python", "third_party.py")

        def safe_decode(byte_data):
            if not byte_data:
                return ""

            # 尝试常见编码
            encodings = ['utf-8', locale.getpreferredencoding(), 'gbk', 'latin1']
            for enc in encodings:
                try:
                    return byte_data.decode(enc)
                except UnicodeDecodeError:
                    continue

            # 所有编码失败，使用替换模式
            return byte_data.decode('utf-8', errors='replace')

        process = subprocess.Popen(
            [sys.executable, py, '-i', intermediate, '-o', output, '-e', self.engine_path, '-p', platform],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            universal_newlines=True,
            encoding=locale.getpreferredencoding()
        )

        stdout_bytes, stderr_bytes = process.communicate()
        print(stdout_bytes)
        print(stderr_bytes)


    def update_data(self, engine_path: str):
        self.engine_path = engine_path
        json_file = os.path.join(self.engine_path, 'cmake', 'thirdparty.json')

        with open(json_file, 'r', encoding='utf-8') as file:
            data = json.load(file)
        self.json_file = json_file
        data = data.get('packages', [])

        headers = list(data[0].keys()) if data else []
        self.table.setRowCount(len(data))
        self.table.setColumnCount(len(headers))
        self.table.setHorizontalHeaderLabels(headers)
        for row_idx, row_data in enumerate(data):
            for col_idx, header in enumerate(headers):
                value = row_data.get(header, "")
                item = QTableWidgetItem(str(value))
                self.table.setItem(row_idx, col_idx, item)


class EngineConfigWidget(QMainWindow):
    def __init__(self, engine_path: str, parent=None):
        super(EngineConfigWidget, self).__init__(parent)

        self.trd_table = None
        self.engine_path_edit = None

        self.third_parth = ''
        self.engine_path = engine_path
        self.setup_ui()

    def setup_ui(self):
        layout = QVBoxLayout(self)

        # form
        form_layout = QFormLayout(self)
        self.engine_path_edit = EditWithSelect(parent=self, dft_path=self.engine_path, is_dir=True)
        self.engine_path_edit.on_selected.connect(self.engine_path_selected)
        form_layout.addRow("Engine Root Path", self.engine_path_edit)

        # tab
        tab_widget = QTabWidget(self)
        self.trd_table = EngineThirdPartyTable(self.engine_path, self)
        tab_widget.addTab(self.trd_table, 'Third Party')

        layout.addLayout(form_layout)
        layout.addWidget(tab_widget)

        main = QWidget(self)
        main.setLayout(layout)

        self.setWindowTitle('EngineSetup')
        self.setMinimumWidth(512)
        self.setMinimumHeight(256)
        self.setCentralWidget(main)
        self.show()

    def engine_path_selected(self, path: str):
        self.engine_path = path
        self.trd_table.update_data(path)
        print(f'engine path {self.engine_path} selected.')