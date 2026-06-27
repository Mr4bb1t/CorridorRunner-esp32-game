import serial
import tkinter as tk
from PIL import Image, ImageTk
import sys

SERIAL_PORT = 'COM6'
BAUD_RATE = 2000000

WIDTH = 128
HEIGHT = 160
SCALE = 3
WINDOW_WIDTH = WIDTH * SCALE
WINDOW_HEIGHT = HEIGHT * SCALE

class ScreenMirror:
    def __init__(self, master):
        self.master = master
        self.master.title("Corridor Runner - Mirror")
        self.master.geometry(f"{WINDOW_WIDTH}x{WINDOW_HEIGHT}")
        self.master.configure(bg='black')

        self.canvas = tk.Canvas(master, width=WINDOW_WIDTH, height=WINDOW_HEIGHT, bg='black', highlightthickness=0)
        self.canvas.pack()
        self.photo = None
        self.image_on_canvas = None
        
        print(f"Tentando conectar na porta {SERIAL_PORT} a {BAUD_RATE} bps...")
        try:
            self.ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=2.0)
            print("Conectado com sucesso!")
        except Exception as e:
            print(f"Erro ao conectar: {e}")
            sys.exit(1)

        self.frames_received = 0
        # Inicia rapido
        self.master.after(5, self.update_frame)

    def update_frame(self):
        # Enviar pedido de frame ('F')
        self.ser.write(b'F')
        
        # Ler o cabeçalho (4 bytes)
        header = self.ser.read(4)
        if len(header) == 4 and header[0] == 0xAA and header[1] == 0x55:
            frame_w = header[2]
            frame_h = header[3]
            frame_size = frame_w * frame_h * 2
            
            data = bytearray()
            while len(data) < frame_size:
                chunk = self.ser.read(frame_size - len(data))
                if not chunk:
                    print("Timeout lendo o frame")
                    break 
                data.extend(chunk)
                
            if len(data) == frame_size:
                self.frames_received += 1
                if self.frames_received % 20 == 0:
                    print(f"Frames recebidos com sucesso: {self.frames_received}")
                
                # A imagem agora tem dimensões frame_w x frame_h
                image = Image.frombuffer('RGB', (frame_w, frame_h), bytes(data), 'raw', 'BGR;16', 0, 1)
                
                # Tenta trocar os canais se a cor ainda estiver bizarra (Blue e Red trocados)
                r, g, b = image.split()
                image = Image.merge('RGB', (b, g, r))
                
                image = image.resize((WINDOW_WIDTH, WINDOW_HEIGHT), Image.Resampling.NEAREST)
                
                self.photo = ImageTk.PhotoImage(image)
                if self.image_on_canvas is None:
                    self.image_on_canvas = self.canvas.create_image(0, 0, anchor=tk.NW, image=self.photo)
                else:
                    self.canvas.itemconfig(self.image_on_canvas, image=self.photo)
        else:
            print(f"Header invalido ou vazio. Bytes recebidos: {header}")
            self.ser.reset_input_buffer()
                    
        # Agendar próxima atualização muito mais rápido
        self.master.after(5, self.update_frame)
        
    def on_closing(self):
        if self.ser.is_open:
            self.ser.close()
        self.master.destroy()

if __name__ == "__main__":
    root = tk.Tk()
    app = ScreenMirror(root)
    root.protocol("WM_DELETE_WINDOW", app.on_closing)
    root.mainloop()
