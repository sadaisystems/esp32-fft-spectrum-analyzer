from PyQt5.QtWidgets import QMainWindow
from PyQt5 import QtWidgets

class GUIWindow(QMainWindow):
    def __init__(self, wapp):
        super(GUIWindow, self).__init__() 
        self.setGeometry(200, 200, 300, 300)
        self.setWindowTitle("ESP32")
        
        self.wapp = wapp
        
        self.initUI()
    
    def initUI(self):
        self.label = QtWidgets.QLabel(self)
        self.label.setText("ESP32 Spectrum Analyser Data Stream")
        self.label.adjustSize()
        self.label.move(50, 50)
        
        self.b1 = QtWidgets.QPushButton(self)
        self.b1.setText("Close connection")
        self.b1.clicked.connect(self.button_close_clicked)
        self.b1.adjustSize()
        self.b1.move(50, 150)
        
        self.label_bands = QtWidgets.QLabel(self)
        self.label_bands.setText("Bands: ")
        self.label_bands.adjustSize()
        self.label_bands.move(50, 100)
        
    def button_close_clicked(self):
        self.wapp.close()
        
    def update_bands(self, bands):
        self.label_bands.setText("Bands: " + str(bands))
        self.label_bands.adjustSize()

