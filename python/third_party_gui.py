import argparse
import os
import sys

from PySide6.QtWidgets import QApplication

from widgets.third_party_build_widget import ThirdPartyBuildWidget

current_file = os.path.abspath(__file__)
engine_path = os.path.dirname(os.path.dirname(current_file))

parser = argparse.ArgumentParser(description='SkyEngine 三方库构建界面')
parser.add_argument('-e', '--engine', type=str, help='引擎目录')
args = parser.parse_args()


def init_engine_path():
    if args.engine:
        global engine_path
        engine_path = args.engine


def app_main():
    init_engine_path()

    app = QApplication(sys.argv)
    main_window = ThirdPartyBuildWidget(engine_path)
    main_window.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    app_main()
