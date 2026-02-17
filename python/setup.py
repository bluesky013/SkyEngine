import argparse
import os.path
import sys

from PySide6.QtWidgets import QApplication
from widgets.engine_config_widget import EngineConfigWidget

current_file = os.path.abspath(__file__)
engine_path = os.path.dirname(os.path.dirname(current_file))

parser = argparse.ArgumentParser(description='示例脚本')
parser.add_argument('-e', '--engine', type=str, help='三方库json')
args = parser.parse_args()

def init_engine_path():
    if args.engine:
        global engine_path
        engine_path = args.engine

def app_main():
    init_engine_path()

    app = QApplication(sys.argv)
    main_window = EngineConfigWidget(engine_path)
    sys.exit(app.exec())


if __name__ == "__main__":
    app_main()