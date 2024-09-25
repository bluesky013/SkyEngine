import configparser
import os
import sys
import ctypes
import platform as OSPlatform
from functools import partial

from PySide6.QtGui import QAction, QCursor
from PySide6.QtCore import Slot
from PySide6.QtWidgets import QApplication, QWidget, QSizePolicy, QHBoxLayout, QVBoxLayout, QFormLayout, QLabel, \
    QFileDialog, QLineEdit, QPushButton, QMessageBox, QGridLayout, QDialog, QGroupBox, QTableWidget, QMenu, \
    QComboBox, QTableWidgetItem, QHeaderView

from build.Project import ProjectBuilder, ProjectConfig
from build.Presets import PLATFORM_AVAILABLE_LIST, BUILD_TYPE_LIST, GENERATOR_LIST, PLATFORM_ARCH_LIST, \
    WindowsConfigPreset, AndroidConfigPreset

from widgets import Asset


def is_admin():
    try:
        return ctypes.windll.shell32.IsUserAnAdmin()
    except:
        return True


class BuildConfigWidget(QWidget):
    def __init__(self, platform):
        QWidget.__init__(self)
        self.platform = platform

        self.fLayout = QFormLayout()
        self.setLayout(self.fLayout)
        self.setMinimumWidth(512)

        self.buildType = QComboBox()
        self.buildType.addItems(BUILD_TYPE_LIST)
        self.buildName = QLineEdit("%s" % platform)
        self.generator = QComboBox()
        self.generator.addItems(GENERATOR_LIST[platform])
        self.arch = QComboBox()
        self.arch.addItems(PLATFORM_ARCH_LIST[platform])
        self.external = QLineEdit()

        self.fLayout.addRow("Name", self.buildName)
        self.fLayout.addRow("Build Type", self.buildType)
        self.fLayout.addRow("Generator", self.generator)
        self.fLayout.addRow("Arch", self.arch)
        self.fLayout.addRow("ThirdParty", self.external)
        self.platform_config()

        self.preset = None

    def platform_config(self):
        pass

    def save_config(self):
        self.preset.set_name(self.buildName.text())
        self.preset.set_generator(self.generator.currentText())
        self.preset.set_description('%s Build (%s)' % (self.platform, self.buildType.currentText()))
        self.preset.add_cache_variable('CMAKE_BUILD_TYPE', self.buildType.currentText())
        self.preset.add_cache_variable('3RD_PATH', self.external.text())


class WindowsConfigWidget(BuildConfigWidget):
    def __init__(self):
        BuildConfigWidget.__init__(self, 'Windows')
        self.preset = WindowsConfigPreset()

    def platform_config(self):
        pass

    def save_config(self):
        BuildConfigWidget.save_config(self)
        self.preset.set_arch(self.arch.currentText())


class AndroidConfigWidget(BuildConfigWidget):
    def __init__(self):
        BuildConfigWidget.__init__(self, 'Android')
        self.preset = AndroidConfigPreset()
        self.identifier = None
        self.ndkPath = None
        self.sdkPath = None

    def platform_config(self):
        self.sdkPath = QLineEdit(os.environ.get('ANDROID_HOME'))
        self.ndkPath = QLineEdit()

        self.identifier = QLineEdit("com.%s.MyProject" % os.getlogin())
        self.fLayout.addRow('SDK PATH', self.sdkPath)
        self.fLayout.addRow('NDK PATH', self.ndkPath)
        self.fLayout.addRow('Identifier', self.identifier)

    def save_config(self):
        BuildConfigWidget.save_config(self)
        self.preset.add_cache_variable('CMAKE_SYSTEM_NAME', 'Android')
        self.preset.add_cache_variable('CMAKE_ANDROID_STL_TYPE', 'c++_static')
        self.preset.add_cache_variable('CMAKE_ANDROID_ARCH_ABI', self.arch.currentText())


PLATFORM_CONFIG_TYPE = {
    'Windows': WindowsConfigWidget,
    'Android': AndroidConfigWidget
}


def check_widget_en(widget: QWidget, enable):
    widget.setEnabled(enable)
    if not enable:
        widget.setStyleSheet("color:grey")


class BuildConfigDialog(QDialog):
    def __init__(self, platform):
        QDialog.__init__(self)
        self.setWindowTitle('%s Build Config' % platform)

        self.setLayout(QVBoxLayout())
        self.configWidget = PLATFORM_CONFIG_TYPE[platform]()
        self.layout().addWidget(self.configWidget)
        applyBtn = QPushButton("Save")
        applyBtn.clicked.connect(self.apply_config)
        cancelBtn = QPushButton("Cancel")
        cancelBtn.clicked.connect(self.cancel)
        self.layout().addWidget(applyBtn)
        self.layout().addWidget(cancelBtn)

    @Slot()
    def apply_config(self):
        self.configWidget.save_config()
        self.accept()

    @Slot()
    def cancel(self):
        self.close()


class ProjectConfigDialog(QDialog):
    def __init__(self, parent, config: ProjectConfig):
        QDialog.__init__(self, parent)
        self.setWindowTitle("Project Config")
        self.setMinimumWidth(512)

        self.vLayout = QGridLayout()
        self.setLayout(self.vLayout)
        self.config = config

        self.buildGroup = QGroupBox('builds')
        self.buildGroup.setLayout(QVBoxLayout())
        self.table = QTableWidget()
        self.table.setColumnCount(3)
        self.table.horizontalHeader().hide()
        self.table.horizontalHeader().setSectionResizeMode(QHeaderView.ResizeMode.ResizeToContents)
        self.addBuildBtn = QPushButton('Add')
        self.addBuildBtn.clicked.connect(self.btn_add_clicked)

        self.buildGroup.layout().addWidget(self.addBuildBtn)
        self.buildGroup.layout().addWidget(self.table)

        applyBtn = QPushButton("Save")
        applyBtn.clicked.connect(self.apply_config)
        cancelBtn = QPushButton("Cancel")
        cancelBtn.clicked.connect(self.cancel)
        self.vLayout.addWidget(self.buildGroup)
        self.vLayout.addWidget(applyBtn)
        self.vLayout.addWidget(cancelBtn)

        self.update_table_content()

    @Slot()
    def apply_config(self):
        self.config.save_config()
        self.accept()

    @Slot()
    def cancel(self):
        self.close()

    @Slot()
    def btn_add_clicked(self):
        menu = QMenu('build list', self)
        for platform in PLATFORM_AVAILABLE_LIST[OSPlatform.system()]:
            action = QAction(platform, menu)
            action.triggered.connect(partial(self.act_add_build_triggered, platform))
            menu.addAction(action)
        menu.exec(QCursor.pos())

    @Slot()
    def act_add_build_triggered(self, platform):
        dialog = BuildConfigDialog(platform)
        if dialog.exec():
            if self.config.cmakePreset.add_preset(dialog.configWidget.preset):
                self.update_table_content()
            else:
                QMessageBox(QMessageBox.Icon.Critical,
                            'Failed',
                            'Preset name %s exists' % dialog.configWidget.preset.data['name']).exec()

    def update_table_content(self):
        self.table.clear()
        self.table.setRowCount(len(self.config.cmakePreset.data['configurePresets']) - 1)
        index = 0
        for preset in self.config.cmakePreset.data['configurePresets']:
            if preset['hidden']:
                continue

            nameItem = QTableWidgetItem('%s' % preset['name'])
            generatorItem = QTableWidgetItem('%s' % preset['generator'])
            buildType = QTableWidgetItem(preset['displayName'])
            self.table.setItem(index, 0, nameItem)
            self.table.setItem(index, 1, buildType)
            self.table.setItem(index, 2, generatorItem)
            index = index + 1


class ProjectWidget(QWidget):
    def __init__(self, *args):
        QWidget.__init__(self, *args)

        self.vLayout = QFormLayout()
        self.setLayout(self.vLayout)

        dft_engine_path = os.path.join(os.curdir, os.pardir)
        self.enginePath = QLineEdit(os.path.abspath(dft_engine_path), self)
        self.projectPath = QLineEdit(self)
        self.projectName = QLineEdit(self)
        self.enginePath.textChanged.connect(self.edit_line_changed)
        self.projectPath.textChanged.connect(self.edit_line_changed)
        self.projectName.textChanged.connect(self.edit_line_changed)

        self.filesBtn = QPushButton("Files...")
        self.filesBtn.clicked.connect(self.on_files_clicked)
        self.createBtn = QPushButton("Create")
        self.createBtn.clicked.connect(self.on_create_clicked)
        self.projectCfgBtn = QPushButton("Config")
        self.projectCfgBtn.clicked.connect(self.on_config_clicked)
        self.assetCfgBtn = QPushButton("Assets")
        self.assetCfgBtn.clicked.connect(self.on_assets_clicked)

        self.vLayout.addRow(QLabel("Engine Dir"), self.enginePath)
        self.vLayout.addRow(QLabel("Project Dir"), self.projectPath)
        self.vLayout.addRow(QLabel("Project Name"), self.projectName)

        self.vLayout.addWidget(self.filesBtn)
        self.vLayout.addWidget(self.createBtn)
        self.vLayout.addWidget(self.projectCfgBtn)
        self.vLayout.addWidget(self.assetCfgBtn)

        self.assetBrowser = None

        self.configFile = 'project_manager.ini'
        self.load_config()

    def edit_line_changed(self, str):
        if self.assetBrowser:
            self.assetBrowser.update_path(self.enginePath.text(),
                                          os.path.join(self.projectPath.text(), self.projectName.text()))

    def closeEvent(self, event):
        self.save_config()

    def create_new_project(self, path):
        os.mkdir(path, 0o777)
        builder = ProjectBuilder(path, self.enginePath.text())
        builder.init_project()

    def update_project(self, path):
        builder = ProjectBuilder(path, self.enginePath.text())
        builder.update_project()
        pass

    def get_project_full_path(self):
        project_path = os.path.abspath(self.projectPath.text())
        project_path = os.path.join(project_path, self.projectName.text())
        return project_path

    @Slot()
    def on_config_clicked(self):
        builder = ProjectBuilder(self.get_project_full_path(), self.enginePath.text())
        ProjectConfigDialog(self, builder.config).exec()

    @Slot()
    def on_assets_clicked(self):
        if not self.assetBrowser:
            self.assetBrowser = Asset.AssetBrowserWidget(None, self.enginePath.text(),
                                                         os.path.join(self.projectPath.text(), self.projectName.text()))
        self.assetBrowser.show()

    @Slot()
    def on_create_clicked(self):
        project_name = self.projectName.text()
        if len(project_name) == 0:
            QMessageBox(QMessageBox.Icon.Critical, 'Error', 'Project name must not be empty').exec()
            return

        project_path = self.get_project_full_path()
        if os.path.exists(project_path):
            self.update_project(project_path)
        else:
            self.create_new_project(project_path)

        QMessageBox(QMessageBox.Icon.Information, 'Success', 'Project %s created successful' % project_path).exec()

    @Slot()
    def on_files_clicked(self):
        dialog = QFileDialog()
        dialog.setFileMode(QFileDialog.FileMode.Directory)
        dialog.setViewMode(QFileDialog.ViewMode.Detail)

        if dialog.exec():
            file_names = dialog.selectedFiles()
            if len(file_names) > 0:
                self.projectPath.setText(os.path.abspath(file_names[0]))

    def load_config(self):
        config = configparser.ConfigParser()
        config.read(self.configFile)

        project_path = config.get('SavedConfig', 'directory', fallback='')
        project_name = config.get('SavedConfig', 'name', fallback='')
        self.projectPath.setText(project_path)
        self.projectName.setText(project_name)

    def save_config(self):
        config = configparser.ConfigParser()
        config['SavedConfig'] = {
            'directory': self.projectPath.text(),
            'name': self.projectName.text()
        }
        with open(self.configFile, 'w') as configfile:
            config.write(configfile)
        print("project saved")


def app_main():
    app = QApplication(sys.argv)

    mainWindow = QWidget()
    mainWindow.setWindowTitle("ProjectManager")
    mainWindow.setMinimumWidth(512)

    project = ProjectWidget(mainWindow)
    project.setSizePolicy(QSizePolicy(QSizePolicy.Policy.Minimum, QSizePolicy.Policy.Minimum))

    mainWindow.setLayout(QHBoxLayout())
    mainWindow.layout().setContentsMargins(0, 0, 0, 0)
    mainWindow.layout().addWidget(project)
    mainWindow.show()

    sys.exit(app.exec())


if __name__ == "__main__":
    if is_admin():
        app_main()
    elif OSPlatform.system() == "Windows":
        # Re-run the program with admin rights
        ctypes.windll.shell32.ShellExecuteW(None, "runas", sys.executable, " ".join(sys.argv), None, 1)
