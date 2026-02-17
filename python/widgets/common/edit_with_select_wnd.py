from PySide6.QtCore import Signal
from PySide6.QtWidgets import QWidget, QHBoxLayout, QLineEdit, QPushButton, QFileDialog, QSizePolicy


class EditWithSelect(QWidget):
    on_selected = Signal(str)

    def __init__(self, is_dir: bool = False, dft_path: str = None, parent=None):
        super(EditWithSelect, self).__init__(parent)

        layout = QHBoxLayout(self)
        self.setLayout(layout)

        self.editor = QLineEdit(self)
        self.editor.setReadOnly(True)
        if dft_path:
            self.editor.setText(dft_path)

        layout.addWidget(self.editor)

        self.btn = QPushButton("...")
        self.btn.clicked.connect(self.btn_clicked)
        layout.addWidget(self.btn)

        self.is_dir = is_dir

    def btn_clicked(self):
        if self.is_dir:
            tmp_dir = QFileDialog.getExistingDirectory(parent=self)
            if tmp_dir is not None:
                self.editor.setText(tmp_dir)
                self.on_selected.emit(tmp_dir)
