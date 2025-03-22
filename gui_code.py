import tkinter as tk
from tkinter import ttk
import requests

# ESP32 IP Address
esp_ip = 'http://192.168.184.133'  # Replace with your ESP32's local IP address

# Functions
def update_motor():
    if motor_status_var.get():
        speed = speed_var.get()
        direction = direction_var.get()
        requests.get(f"{esp_ip}/control?speed={speed}&direction={direction}")
    else:
        requests.get(f"{esp_ip}/control?speed=0")

def start_stop_motor():
    if motor_status_var.get():
        requests.get(f"{esp_ip}/control?speed={speed_var.get()}&direction={direction_var.get()}")
    else:
        requests.get(f"{esp_ip}/control?speed=0")

def set_pin15():
    status = pin15_status_var.get()
    requests.get(f"{esp_ip}/pin15?status={status}")

def play_sound():
    song = song_var.get()
    requests.get(f"{esp_ip}/play?song={song}")

def pause_sound():
    requests.get(f"{esp_ip}/pause")

def stop_sound():
    requests.get(f"{esp_ip}/stop")

def set_volume():
    volume = volume_var.get()
    requests.get(f"{esp_ip}/volume?volume={volume}")

# GUI Setup
root = tk.Tk()
root.title("ESP32 Motor and Sound Control")
root.geometry("600x500")
root.configure(bg="#f0f0f0")

# Style
style = ttk.Style()
style.configure("TFrame", background="#f0f0f0")
style.configure("TLabel", background="#f0f0f0", font=("Arial", 12))
style.configure("TButton", font=("Arial", 12))
style.configure("TRadiobutton", background="#f0f0f0", font=("Arial", 12))
style.configure("TCheckbutton", background="#f0f0f0", font=("Arial", 12))

# Motor Control Frame
motor_frame = ttk.Frame(root)
motor_frame.pack(pady=10, padx=10, fill="x")

ttk.Label(motor_frame, text="Motor Control").pack()

# Speed Control
speed_var = tk.IntVar(value=0)
ttk.Label(motor_frame, text="Speed (0-255)").pack()
speed_slider = ttk.Scale(motor_frame, from_=0, to=255, orient="horizontal", variable=speed_var, command=lambda x: update_motor())
speed_slider.pack(fill="x", pady=5)

# Direction Control
direction_var = tk.IntVar(value=1)
ttk.Label(motor_frame, text="Direction").pack()
ttk.Radiobutton(motor_frame, text="Forward", variable=direction_var, value=1, command=update_motor).pack(anchor="w")
ttk.Radiobutton(motor_frame, text="Backward", variable=direction_var, value=0, command=update_motor).pack(anchor="w")

# Start/Stop Motor
motor_status_var = tk.IntVar(value=1)
ttk.Checkbutton(motor_frame, text="Start/Stop Motor", variable=motor_status_var, command=start_stop_motor).pack(pady=10)

# Pin 15 Control
pin15_status_var = tk.IntVar(value=0)
ttk.Label(motor_frame, text="Pin 15 Control").pack()
ttk.Radiobutton(motor_frame, text="ON", variable=pin15_status_var, value=1, command=set_pin15).pack(anchor="w")
ttk.Radiobutton(motor_frame, text="OFF", variable=pin15_status_var, value=0, command=set_pin15).pack(anchor="w")

# Sound Control Frame
sound_frame = ttk.Frame(root)
sound_frame.pack(pady=10, padx=10, fill="x")

ttk.Label(sound_frame, text="Sound Control").pack()

# Song Number
song_var = tk.IntVar(value=1)
ttk.Label(sound_frame, text="Song Number").pack()
song_entry = ttk.Entry(sound_frame, textvariable=song_var)
song_entry.pack(fill="x", pady=5)

# Play/Pause/Stop Buttons
button_frame = ttk.Frame(sound_frame)
button_frame.pack(fill="x", pady=5)
ttk.Button(button_frame, text="Play", command=play_sound).pack(side="left", padx=5)
ttk.Button(button_frame, text="Pause", command=pause_sound).pack(side="left", padx=5)
ttk.Button(button_frame, text="Stop", command=stop_sound).pack(side="left", padx=5)

# Volume Control
volume_var = tk.IntVar(value=15)
ttk.Label(sound_frame, text="Volume (0-30)").pack()
volume_slider = ttk.Scale(sound_frame, from_=0, to=30, orient="horizontal", variable=volume_var, command=lambda x: set_volume())
volume_slider.pack(fill="x", pady=5)

# Run the GUI
root.mainloop()