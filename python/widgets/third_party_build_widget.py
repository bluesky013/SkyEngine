import configparser
import hashlib
import json
import os
import platform as osp
import subprocess
import sys

from PySide6.QtCore import QProcess, QProcessEnvironment
from PySide6.QtGui import QFontDatabase, QTextCursor
from PySide6.QtWidgets import (
    QAbstractItemView,
    QCheckBox,
    QComboBox,
    QFormLayout,
    QHBoxLayout,
    QHeaderView,
    QLabel,
    QMainWindow,
    QMessageBox,
    QPlainTextEdit,
    QProgressBar,
    QPushButton,
    QSpinBox,
    QTableWidget,
    QTableWidgetItem,
    QVBoxLayout,
    QWidget,
)

from widgets.common.edit_with_select_wnd import EditWithSelect

PLATFORMS = ['Win32', 'MacOS-x86', 'MacOS-arm', 'Linux']
CONFIG_FILE = 'third_party_build.ini'
METADATA_FILE = 'build_metadata.json'


class ThirdPartyBuildWidget(QMainWindow):
    def __init__(self, engine_path: str, parent=None):
        super(ThirdPartyBuildWidget, self).__init__(parent)

        self.engine_path = engine_path
        self.process = None

        self.setup_ui()
        self.load_config()
        self.refresh_packages()

    def setup_ui(self):
        self.setWindowTitle('SkyEngine 三方库构建')
        self.setMinimumWidth(760)
        self.setMinimumHeight(560)

        central = QWidget(self)
        self.setCentralWidget(central)
        layout = QVBoxLayout(central)

        form = QFormLayout()
        self.engine_edit = EditWithSelect(is_dir=True, dft_path=self.engine_path, parent=self)
        self.intermediate_edit = EditWithSelect(
            is_dir=True,
            dft_path=os.path.join(self.engine_path, 'build_3rd', 'intermediate'),
            parent=self,
        )
        self.output_edit = EditWithSelect(
            is_dir=True,
            dft_path=os.path.join(self.engine_path, 'build_3rd'),
            parent=self,
        )
        self.engine_edit.on_selected.connect(self.on_path_selected)
        self.intermediate_edit.on_selected.connect(self.on_path_selected)
        self.output_edit.on_selected.connect(self.on_path_selected)
        form.addRow('Engine Root', self.engine_edit)
        form.addRow('Intermediate', self.intermediate_edit)
        form.addRow('Output', self.output_edit)
        layout.addLayout(form)

        opts = QHBoxLayout()
        self.platform = QComboBox()
        self.platform.addItems(PLATFORMS)
        self.platform.setCurrentIndex(self.default_platform_index())
        self.platform.currentTextChanged.connect(self.on_platform_changed)

        self.jobs = QSpinBox()
        self.jobs.setRange(0, 64)
        self.jobs.setValue(0)
        self.jobs.setToolTip('0 = 自动')

        self.force = QCheckBox('Force')
        self.clean = QCheckBox('Clean')

        opts.addWidget(QLabel('Platform'))
        opts.addWidget(self.platform)
        opts.addWidget(QLabel('Jobs'))
        opts.addWidget(self.jobs)
        opts.addWidget(self.force)
        opts.addWidget(self.clean)
        opts.addStretch()
        layout.addLayout(opts)

        self.table = QTableWidget()
        self.table.setColumnCount(4)
        self.table.setHorizontalHeaderLabels(['Name', 'Tag', 'Type', 'Status'])
        self.table.setSelectionBehavior(QAbstractItemView.SelectionBehavior.SelectRows)
        self.table.setSelectionMode(QAbstractItemView.SelectionMode.SingleSelection)
        self.table.setEditTriggers(QAbstractItemView.EditTrigger.NoEditTriggers)
        self.table.horizontalHeader().setSectionResizeMode(QHeaderView.ResizeMode.Stretch)
        layout.addWidget(self.table)

        btns = QHBoxLayout()
        self.build_selected_btn = QPushButton('构建选中')
        self.build_all_btn = QPushButton('构建全部')
        self.cancel_btn = QPushButton('取消')
        self.cancel_btn.setEnabled(False)
        self.build_selected_btn.clicked.connect(self.on_build_selected)
        self.build_all_btn.clicked.connect(self.on_build_all)
        self.cancel_btn.clicked.connect(self.on_cancel)
        btns.addWidget(self.build_selected_btn)
        btns.addWidget(self.build_all_btn)
        btns.addWidget(self.cancel_btn)
        btns.addStretch()
        layout.addLayout(btns)

        self.log = QPlainTextEdit()
        self.log.setReadOnly(True)
        self.log.setMaximumBlockCount(10000)
        self.log.setFont(QFontDatabase.systemFont(QFontDatabase.SystemFont.FixedFont))
        layout.addWidget(self.log)

        self.status_label = QLabel('空闲')
        self.statusBar().addWidget(self.status_label)

        self.progress = QProgressBar()
        self.progress.setRange(0, 0)
        self.progress.setVisible(False)
        self.progress.setFixedWidth(120)
        self.statusBar().addPermanentWidget(self.progress)

    def detect_platform(self):
        if sys.platform == 'win32':
            return 'Win32'
        if sys.platform == 'darwin':
            return 'MacOS-arm' if osp.machine() == 'arm64' else 'MacOS-x86'
        if sys.platform.startswith('linux'):
            return 'Linux'
        return 'Win32'

    def default_platform_index(self):
        platform = self.detect_platform()
        return PLATFORMS.index(platform) if platform in PLATFORMS else 0

    def engine_root(self):
        return self.engine_edit.editor.text().strip()

    def intermediate_path(self):
        return self.intermediate_edit.editor.text().strip()

    def output_root(self):
        return self.output_edit.editor.text().strip()

    def platform_name(self):
        return self.platform.currentText()

    def load_packages(self):
        json_file = os.path.join(self.engine_root(), 'cmake', 'thirdparty.json')
        try:
            with open(json_file, 'r', encoding='utf-8') as file:
                data = json.load(file)
        except (OSError, ValueError):
            QMessageBox.warning(self, '提示', f'无法读取 {json_file}')
            return []
        return data.get('packages', [])

    def filter_packages(self, packages):
        platform = self.platform_name()
        result = []
        for pkg in packages:
            platforms = pkg.get('platforms')
            if not platforms or platform in platforms:
                result.append(pkg)
        return result

    def compute_package_key(self, package):
        relevant = {k: v for k, v in package.items() if k != 'name'}
        content = json.dumps(relevant, sort_keys=True)
        return hashlib.sha256(content.encode()).hexdigest()[:16]

    def load_metadata(self):
        meta_path = os.path.join(self.output_root(), self.platform_name(), METADATA_FILE)
        if os.path.exists(meta_path):
            try:
                with open(meta_path, 'r', encoding='utf-8') as file:
                    return json.load(file)
            except (OSError, ValueError):
                return {}
        return {}

    def is_up_to_date(self, metadata, package):
        name = package.get('name')
        entry = metadata.get(name)
        if not entry:
            return False
        return (
            entry.get('key') == self.compute_package_key(package)
            and entry.get('platform') == self.platform_name()
        )

    def refresh_packages(self):
        packages = self.filter_packages(self.load_packages())
        metadata = self.load_metadata()
        self.table.setRowCount(len(packages))
        for row, pkg in enumerate(packages):
            name = pkg.get('name', '')
            tag = pkg.get('tag') or pkg.get('commit') or '(none)'
            ptype = 'header-only' if pkg.get('header_only') else ('tool' if pkg.get('is_tool') else 'static')
            status = '✓' if self.is_up_to_date(metadata, pkg) else ''
            for col, text in enumerate([name, tag, ptype, status]):
                self.table.setItem(row, col, QTableWidgetItem(str(text)))

    def on_platform_changed(self, text):
        self.refresh_packages()

    def on_path_selected(self, path):
        self.refresh_packages()

    def third_party_script(self):
        return os.path.join(self.engine_root(), 'python', 'third_party.py')

    def build_args(self, target=None):
        args = [
            sys.executable,
            self.third_party_script(),
            '-e', self.engine_root(),
            '-i', self.intermediate_path(),
            '-o', self.output_root(),
            '-p', self.platform_name(),
            '-j', str(self.jobs.value()),
        ]
        if self.force.isChecked():
            args.append('-f')
        if self.clean.isChecked():
            args.append('-c')
        if target:
            args.extend(['-t', target])
        return args

    def git_config_proxy(self):
        try:
            result = subprocess.run(
                ['git', 'config', '--global', '--get', 'http.proxy'],
                capture_output=True,
                text=True,
            )
            value = result.stdout.strip()
            if result.returncode == 0 and value:
                return value
        except (OSError, subprocess.SubprocessError):
            pass
        return None

    def detect_proxy(self):
        proxy = self.git_config_proxy()
        if proxy:
            return proxy
        for key in ('https_proxy', 'HTTPS_PROXY', 'http_proxy', 'HTTP_PROXY'):
            value = os.environ.get(key)
            if value:
                return value
        return None

    def inject_proxy(self, env):
        proxy = self.detect_proxy()
        if not proxy:
            return
        for name in ('http_proxy', 'https_proxy', 'HTTP_PROXY', 'HTTPS_PROXY'):
            env.insert(name, proxy)

    def start_build(self, target=None):
        if self.process is not None and self.process.state() != QProcess.ProcessState.NotRunning:
            return

        args = self.build_args(target)
        self.process = QProcess(self)
        self.process.setWorkingDirectory(self.engine_root())

        env = QProcessEnvironment.systemEnvironment()
        env.insert('PYTHONUTF8', '1')
        env.insert('PYTHONIOENCODING', 'utf-8')
        env.insert('PYTHONUNBUFFERED', '1')
        self.inject_proxy(env)
        self.process.setProcessEnvironment(env)

        self.process.readyReadStandardOutput.connect(self.on_stdout)
        self.process.readyReadStandardError.connect(self.on_stderr)
        self.process.finished.connect(self.on_finished)

        self.log.clear()
        self.set_busy(True)
        self.process.start(args[0], args[1:])

    def on_build_selected(self):
        row = self.table.currentRow()
        if row < 0:
            QMessageBox.warning(self, '提示', '请先选中一个包')
            return
        item = self.table.item(row, 0)
        if item is None:
            return
        self.start_build(target=item.text())

    def on_build_all(self):
        self.start_build(target=None)

    def on_stdout(self):
        self.append_log(self.process.readAllStandardOutput().data())

    def on_stderr(self):
        self.append_log(self.process.readAllStandardError().data())

    def append_log(self, data):
        text = data.decode('utf-8', errors='replace')
        self.log.moveCursor(QTextCursor.MoveOperation.End)
        self.log.insertPlainText(text)
        self.log.moveCursor(QTextCursor.MoveOperation.End)
        self.log.ensureCursorVisible()

    def on_finished(self, exit_code, exit_status):
        self.set_busy(False)
        if exit_status == QProcess.ExitStatus.NormalExit and exit_code == 0:
            self.status_label.setText('构建成功')
        else:
            self.status_label.setText(f'构建失败 (exit {exit_code})')
        self.refresh_packages()

    def on_cancel(self):
        if self.process is None:
            return
        pid = self.process.processId()
        if pid:
            self.kill_process_tree(pid)
        self.process.kill()
        self.process.waitForFinished(3000)

    def kill_process_tree(self, pid):
        try:
            if sys.platform == 'win32':
                subprocess.run(['taskkill', '/T', '/F', '/PID', str(pid)], capture_output=True)
            else:
                subprocess.run(['pkill', '-P', str(pid)], capture_output=True)
        except OSError:
            pass

    def set_busy(self, busy):
        self.build_selected_btn.setEnabled(not busy)
        self.build_all_btn.setEnabled(not busy)
        self.cancel_btn.setEnabled(busy)
        self.progress.setVisible(busy)
        if busy:
            self.status_label.setText('构建中...')

    def closeEvent(self, event):
        self.save_config()
        super(ThirdPartyBuildWidget, self).closeEvent(event)

    def load_config(self):
        config = configparser.ConfigParser()
        config.read(CONFIG_FILE)
        if not config.has_section('SavedConfig'):
            return

        engine = config.get('SavedConfig', 'engine', fallback='')
        if engine:
            self.engine_edit.editor.setText(engine)

        intermediate = config.get('SavedConfig', 'intermediate', fallback='')
        if intermediate:
            self.intermediate_edit.editor.setText(intermediate)

        output = config.get('SavedConfig', 'output', fallback='')
        if output:
            self.output_edit.editor.setText(output)

        platform = config.get('SavedConfig', 'platform', fallback='')
        if platform in PLATFORMS:
            self.platform.setCurrentText(platform)

        jobs = config.get('SavedConfig', 'jobs', fallback='')
        if jobs.isdigit():
            self.jobs.setValue(int(jobs))

    def save_config(self):
        config = configparser.ConfigParser()
        config['SavedConfig'] = {
            'engine': self.engine_root(),
            'intermediate': self.intermediate_path(),
            'output': self.output_root(),
            'platform': self.platform_name(),
            'jobs': str(self.jobs.value()),
        }
        with open(CONFIG_FILE, 'w') as file:
            config.write(file)
