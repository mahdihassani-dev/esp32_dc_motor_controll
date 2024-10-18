import tkinter as tk
import requests

# ESP32 IP Address
esp_ip = 'http://192.168.1.105'  # Replace with your ESP32's local IP address

def update_motor():
    # Only send commands if the motor is running
    if motor_status_var.get():
        speed = speed_var.get()
        direction = direction_var.get()
        requests.get(f"{esp_ip}/control?speed={speed}&direction={direction}")
    else:
        requests.get(f"{esp_ip}/control?speed=0")  # Ensure motor stays off

def start_stop_motor():
    # Check motor status and send appropriate command
    if motor_status_var.get():
        # Motor is starting
        requests.get(f"{esp_ip}/control?speed={speed_var.get()}&direction={direction_var.get()}")
    else:
        # Motor is stopping
        requests.get(f"{esp_ip}/control?speed=0")  # Stop motor

# GUI setup
root = tk.Tk()
root.title("ESP32 Motor Control")

# Motor Speed
speed_var = tk.IntVar(value=0)
tk.Label(root, text="Speed (0-255)", font=("Arial", 14)).pack(pady=10)
speed_slider = tk.Scale(root, from_=0, to=255, orient="horizontal", variable=speed_var, command=lambda x: update_motor(), length=400)
speed_slider.pack(pady=10)

# Motor Direction
direction_var = tk.IntVar(value=1)
tk.Label(root, text="Direction", font=("Arial", 14)).pack(pady=10)
tk.Radiobutton(root, text="Forward", variable=direction_var, value=1, font=("Arial", 12), command=update_motor).pack(anchor=tk.W, padx=50)
tk.Radiobutton(root, text="Backward", variable=direction_var, value=0, font=("Arial", 12), command=update_motor).pack(anchor=tk.W, padx=50)

# Motor Start/Stop
motor_status_var = tk.IntVar(value=1)  # Motor running by default
start_stop_checkbox = tk.Checkbutton(root, text="Start/Stop Motor", font=("Arial", 14), variable=motor_status_var, command=start_stop_motor)
start_stop_checkbox.pack(pady=20)

# Aesthetic adjustments
root.geometry("500x300")
root.configure(bg="#f0f0f0")

# Run the GUI
root.mainloop()
