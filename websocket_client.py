import websocket
import threading
import json
import sys

from window import GUIWindow
from PyQt5.QtWidgets import QApplication

        
def on_message(ws, message):
    print('Message received')
    bandJson = json.loads(message)
    gui_window.update_bands(bandJson)
    print(bandJson)
    
def on_close(ws, close_status_code, close_msg):
    print("CLOSED")
 
# Init websocket
wapp = websocket.WebSocketApp("ws://10.0.1.75", on_message=on_message, on_close=on_close) 
# Init GUI
app = QApplication(sys.argv)
gui_window = GUIWindow(wapp)
gui_window.show()
        
if __name__ == "__main__":
    # Init thread with websocket connection
    wst = threading.Thread(target=wapp.run_forever)
    wst.daemon = True
    wst.start()
    # Exit handler
    sys.exit(app.exec_())
