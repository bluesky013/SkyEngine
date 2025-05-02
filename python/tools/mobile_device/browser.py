import sys

from PySide6.QtWidgets import QApplication, QMainWindow, QTabWidget
# from tools.mobile_device.device_widget import DeviceBrowser
from tools.mobile_device.device_browser import DeviceBrowser

class Browser(QMainWindow):
    def __init__(self):
        super().__init__()

        self.setMinimumWidth(800)
        self.setMinimumHeight(600)

        self.tabWidget = QTabWidget(self)
        self.setCentralWidget(self.tabWidget)

        self.device = DeviceBrowser()
        self.tabWidget.addTab(self.device, 'Device')


if __name__ == "__main__":
    app = QApplication(sys.argv)

    browser = Browser()
    browser.show()

    sys.exit(app.exec())