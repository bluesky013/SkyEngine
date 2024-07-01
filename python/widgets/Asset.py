import os.path

from PySide6.QtCore import Slot, QDir
from PySide6.QtWidgets import QWidget, QVBoxLayout, QTreeView, QFileSystemModel, QComboBox, QListView, QHBoxLayout, \
    QSplitter


class AssetTreeViewWidget(QWidget):
    def __init__(self, parent, engine, workspace):
        QWidget.__init__(self, parent)

        self.bundle = ["Engine", "Project"]

        self.modelDict = dict()
        self.update_path(engine, workspace)

        self.model = QFileSystemModel(self)

        self.combo = QComboBox(self)
        self.combo.addItems(self.bundle)
        self.combo.activated.connect(self.combo_activated)

        self.model = QFileSystemModel()

        self.treeView = QTreeView(self)
        self.treeView.setModel(self.model)
        self.treeView.setRootIndex(self.model.setRootPath(self.modelDict[self.combo.currentText()]))

        self.setLayout(QVBoxLayout(self))
        self.layout().addWidget(self.combo)
        self.layout().addWidget(self.treeView)

    def update_path(self, engine, workspace):
        self.modelDict["Engine"] = os.path.join(engine, "assets")
        self.modelDict["Project"] = os.path.join(workspace, "assets")

    @Slot()
    def combo_activated(self, idx):
        model = self.modelDict[self.combo.itemText(idx)]
        self.treeView.setRootIndex(self.model.setRootPath(model))


class AssetListViewWidget(QWidget):
    def __init__(self, parent):
        QWidget.__init__(self, parent)

        self.listView = QListView(self)

        self.setLayout(QVBoxLayout(self))
        self.layout().addWidget(self.listView)


class AssetBrowserWidget(QWidget):
    def __init__(self, parent, engine, workspace):
        QWidget.__init__(self, parent)
        print("AssetBrowser %s, %s" % (engine, workspace))

        self.tree = AssetTreeViewWidget(self, engine, workspace)
        self.list = AssetListViewWidget(self)

        self.splitter = QSplitter(self)
        self.splitter.setMinimumWidth(10)
        self.splitter.addWidget(self.tree)
        self.splitter.addWidget(self.list)

        self.setLayout(QHBoxLayout(self))
        self.layout().addWidget(self.splitter)

    def update_path(self, engine, workspace):
        self.tree.update_path(engine, workspace)