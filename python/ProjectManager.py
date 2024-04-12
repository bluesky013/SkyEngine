import sys, os
import configparser

from PySide6.QtCore import Qt, QSize, Slot
from PySide6.QtWidgets import QApplication, QSplitter, QWidget, QSizePolicy, QHBoxLayout, QFormLayout, QLabel, \
    QFileDialog, QLineEdit, QPushButton, QMessageBox

from build.Project import ProjectBuilder


class ProjectWidget(QWidget):
    def __init__(self, *args):
        QWidget.__init__(self, *args)

        self.vLayout = QFormLayout()
        self.setLayout(self.vLayout)

        dft_engine_path = os.path.join(os.curdir, os.pardir)
        self.enginePath = QLineEdit(os.path.abspath(dft_engine_path), self)
        self.projectPath = QLineEdit(self)
        self.projectName = QLineEdit(self)
        self.filesBtn = QPushButton("Files...")
        self.filesBtn.clicked.connect(self.on_files_clicked)
        self.createBtn = QPushButton("Create")
        self.createBtn.clicked.connect(self.on_create_clicked)

        self.vLayout.addRow(QLabel("Engine  Dir"), self.enginePath)
        self.vLayout.addRow(QLabel("Project Dir"), self.projectPath)
        self.vLayout.addRow(QLabel("Project Name"), self.projectName)

        self.vLayout.addWidget(self.filesBtn)
        self.vLayout.addWidget(self.createBtn)

        self.configFile = 'project_manager.ini'
        self.load_config()

    def __del__(self):
        self.save_config()

    def create_new_project(self, path):
        os.mkdir(path, 0o777)
        builder = ProjectBuilder(path, self.enginePath.text())
        builder.config_project()

    @Slot()
    def on_create_clicked(self):
        project_name = self.projectName.text()
        if len(project_name) == 0:
            QMessageBox(QMessageBox.Icon.Critical, 'Error', 'Project name must not be empty').exec()
            return

        project_path = os.path.abspath(self.projectPath.text())
        project_path = os.path.join(project_path, self.projectName.text())

        if os.path.exists(project_path):
            QMessageBox(QMessageBox.Icon.Critical, 'Error', 'Project %s is already exist' % project_path).exec()
            return

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


if __name__ == "__main__":
    app = QApplication(sys.argv)

    mainWindow = QWidget()
    mainWindow.setWindowTitle("ProjectManager")
    mainWindow.setMinimumSize(QSize(512, 256))

    project = ProjectWidget(mainWindow)
    project.setSizePolicy(QSizePolicy(QSizePolicy.Policy.Minimum, QSizePolicy.Policy.Minimum))

    mainWindow.setLayout(QHBoxLayout())
    mainWindow.layout().setContentsMargins(0, 0, 0, 0)
    mainWindow.layout().addWidget(project)
    mainWindow.show()

    app.aboutToQuit.connect(lambda: project.save_config())
    sys.exit(app.exec())
